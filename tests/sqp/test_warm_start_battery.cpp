// tests/test_warm_start_battery.cpp — PHASE-4 TASK 13: THE WARM-START
// BENCHMARK BATTERY, the evidence generator the whole phase exists to feed.
// Everything Tasks 0-12 built is composed here into one sweep grid and
// MEASURED; the numbers this file pins are the ones
// docs/notes/2026-07-30-warm-start-battery-results.md reports and the ones
// Phase 6 has to beat the IPM engine with.
//
// =====================================================================
// THE GRID
//
//   ARMS (5)             what varies is the START, nothing else
//     cold-each-step     every parameter value solved FROM SCRATCH: a FRESH
//                        SqpDriver (hence a fresh QpEngine, hence no retained
//                        K0 at all) and the model's own generic start_point(),
//                        through the 2-arg solve() that has no warm object to
//                        resolve. Run on the WARM arm's parameter grid. This is
//                        the baseline a user without any of Phase 4 pays.
//     warm               run_continuation with SqpOptions::start_level = kWarm
//                        and ContinuationOptions::use_predictor = false.
//     warm+predictor     the same with use_predictor = true (Task 9).
//     hot                the same with start_level = kHot (Task 4).
//     cold@pred          FIX ROUND 1. The cold-each-step arm again, on the
//                        PREDICTOR's own parameter grid -- which is not always
//                        the warm arm's. See the ArmIdx note below for the
//                        framing defect this exists to repair; it is the
//                        comparator every PER-SOLVE predictor figure uses.
//
//   x FULL-STEP (2)      SqpOptions::warm_full_step on/off -- Task 5's
//                        Kungurtsev-Diehl rule, the OPEN QUESTION this battery
//                        was built to answer (see FULL-STEP DECISION INPUT).
//
//   x FAMILIES (6)       tests/support/parametric_families.h
//     F1        n = 2   box QP, p: 0 -> 1, crosses both bound activations.
//     F2        n = 2   disk NLP, p: 0 -> 1, crosses p* = 0.786151377757423.
//     F2far     n = 2   THE SAME F2 with one thing changed -- start_point()
//                       moved from (0.5, 0.5) to (4, -4). It is a CONTROL, not
//                       a new family; see WHY THE COLD BASELINE NEEDS A
//                       CONTROL below.
//     F3n50     n = 50  spring chain, p: 0.25 -> 0.75, crosses p_act = 0.5.
//     F3n1000   n = 1000  the same at scale.
//     F3stress  n = 200 the same with the step controller pinned at dp = 0.5
//                       over p: 0.1 -> 2.0 -- LONG steps, so a warm start
//                       lands far enough from x*(p) for globalization to have
//                       something to decide. Built for the full-step question.
//
// 58 of the 60 cells are run; the two omitted (a predictor cell and the matched
// baseline that would have had no grid to run on without it) are named and
// justified under THE RUNTIME BUDGET below.
//
// =====================================================================
// EVERY COUNT IN THIS FILE COMES OFF A LEDGER, NOT OFF AN AD-HOC COUNTER.
// That is the brief's explicit requirement and it is what makes these numbers
// the same numbers a Phase-6 comparison will read. Concretely, `collect()`
// below reads ONLY:
//
//   - Ledger::sqp_records() -- one SqpSolveRecord per driver solve()
//     (ledger.h): counters.major_iters / qp_minor_iters / factorizations, and
//     the flat Task-7 fields start_level_used, full_step_majors,
//     watchdog_restores, factorizations_saved, soc_steps, soc_applied,
//     border_refine_steps, eqp_refine_steps;
//   - Ledger::records() -- one QP-level SolveRecord per subproblem: its
//     counters.factorizations and counters.k0_reused.
//
// ContinuationResult contributes only what no ledger can know: which parameter
// values were visited, whether the sweep reached p1, and the predictor's own
// outcome reports. The one place the two sources are deliberately crossed is
// check_ledger_agrees_with_the_driver(), which pins that the ledger's summed
// major_iters equals ContinuationResult::total_majors -- if those ever
// disagree the ledger has stopped being a faithful record and every other
// number here is void.
//
// MUTATION-VERIFIED (each mutation run against the whole battery, then
// reverted; the file was green before and after; all three RE-RUN after fix
// round 1 added the grid section and the fifth arm, with identical results):
//   M1  collect() sums `r.counters.qp_minor_iters` into s.majors where it sums
//       `r.counters.major_iters` -- i.e. the aggregation reads the WRONG
//       LEDGER FIELD. Six of the corpus sections below fail: the ledger/driver
//       cross-check, warm-vs-cold, predictor-vs-warm, the majors/minors trade,
//       the hot walk, and the pinned totals.
//   M2  the chain section reads `sqp_records()[1]` instead of
//       `sqp_records().front()` as the sweep's cold solve -- i.e. the WRONG
//       LEDGER RECORD. It fails on every family (record 1 is the first warm
//       step).
//   M3  the hot walk takes `records()[qp_cursor - 1]`, the LAST subproblem of
//       a hot-resolved solve, instead of `records()[qp_cursor]`, its FIRST.
//       The hot section fails -- later subproblems in the same solve build
//       fresh linearizations and pay real factorizations, which is exactly why
//       the hot claim is about the first one and why the walk has to partition
//       the QP records correctly to make it.
//   M4  PHASE-5 TASK 0, and the one mutation of the DRIVER rather than of this
//       file's own aggregation: make_warm_start's zero-major probe is skipped
//       and the old `structure_hash = 0` restored. Four tests fail across the
//       suite -- this file's chain section and its ZeroMajorStep pin,
//       test_warm_start.cpp's unit-level pin on the same path, and
//       test_continuation.cpp's zero-budget sweep -- i.e. the repair is pinned
//       at the corpus level, at the unit level and at the sweep level, none of
//       them redundantly.
//   M5  the same task's FIX ROUND 1, in the opposite direction: the probe is
//       re-enabled on the UNEVALUABLE start-point exit (which it must not
//       cover -- see sqp_driver.h's THE UNEVALUABLE EXIT). Exactly one test
//       fails, test_sqp_driver.cpp's
//       UnevaluableStartPointEmitsAColdHandOffSoARetryIsHonoured, and nothing
//       in THIS file moves -- that exit is unreachable in this corpus, which
//       is why the scope error survived the battery and had to be caught by
//       reading the ingest path instead.
//
// =====================================================================
// WHY THE COLD BASELINE NEEDS A CONTROL, AND WHAT THE BRIEF GOT WRONG.
//
// The task brief asserted "warm total majors < cold total majors on EVERY
// family". THAT IS FALSE AS STATED and this file does not pin it. Measured
// (majors, cold : warm):
//
//     F1        5 : 5     TIE
//     F2        8 : 8     TIE
//     F2far    19 : 10
//     F3n50    19 : 12
//     F3n1000  53 : 30
//     F3stress 33 : 15
//
// The two ties have two different causes and only one of them is about
// warm-starting at all:
//
//   F1'S TIE IS STRUCTURAL, AND NO START THAT STILL HAS WORK TO DO CAN BREAK
//   IT. F1 is a QP with linear constraints, so ONE subproblem is the whole
//   problem: the first QP solved from ANY starting point returns x*(p)
//   exactly, the next convergence test fires, and major_iters == 1. The only
//   way below 1 is 0 -- by the start point ALREADY satisfying the tolerance,
//   so that no subproblem is built at all.
//
//   THAT CASE IS REACHABLE AND THIS FILE MEASURES IT (fix round 1 correction:
//   an earlier version of this note said "no fixture can move it", which is
//   false as written). F1/warm+pred spends 1 major over 5 solves, because the
//   predictor is EXACT on F1's affine middle branch and hands each step a
//   point that is already optimal. What cannot move is the tie between COLD
//   and PLAIN WARM: a warm hand-off is x*(p_prev), and x*(p) moves with p, so
//   the warm start is never already optimal and always costs its one major --
//   exactly as the cold start does. major_iters is in {0, 1}, and both of
//   those arms sit at 1.
//
//   What warm starting buys on F1 is therefore visible one level down, in
//   FACTORIZATIONS: 5 cold against 1 warm, because the sweep's single engine
//   keeps its K0 across steps and the fresh-driver baseline cannot. A
//   benchmark that only counted majors would report "warm-starting does
//   nothing on F1", which is wrong.
//
//   F2 IS A FIXTURE ARTIFACT, and F2far is the control that proves it. F2's
//   start_point() is (0.5, 0.5) -- chosen in Task 8 to be strictly feasible
//   and off the path, NOT to be a realistic generic start -- and it is within
//   one Newton step of x*(p) for every p the sweep visits, so the cold arm is
//   already nearly as cheap as it can be. F2far changes THAT ONE FUNCTION and
//   nothing else (same objective, same constraint, same sweep, same options)
//   and the tie becomes 19 : 10 in majors and 24 : 11 in factorizations. The
//   honest statement is therefore not "warm always wins" but "warm's win is
//   bounded below by how good the model's own generic start point is", and
//   F2/F2far is that statement's evidence.
//
// So what this file pins is: warm <= cold in BOTH majors and factorizations on
// every family, STRICTLY in majors on the four families where a cold start has
// real work to do, and the two ties pinned individually with the reason
// attached.
//
// =====================================================================
// FULL-STEP DECISION INPUT (Task 5's parked question, in one line here and at
// length in the results note). Across the warm/hot cells the lever changes NO
// observed count: majors, QP minors, factorizations, factorizations_saved, the
// border-refinement count and the resolved-level histogram are identical with
// warm_full_step on and off. The mode genuinely ENGAGED (full_step_majors is
// 4..22 on every warm and hot cell with the lever on, and 0 with it off) and
// its watchdog NEVER fired (watchdog_restores == 0 in every cell). This corpus
// therefore cannot discriminate -- it neither supports nor undermines
// default-true -- and check_full_step_is_neutral() pins exactly that, so the
// day a change makes the lever matter, this test fails and says so.
//
// =====================================================================
// THE RUNTIME BUDGET, and the ONE cell it cost.
//
// The whole grid is run ONCE, inside a single TEST, because CMake's
// gtest_discover_tests registers one CTest entry per TEST and therefore one
// PROCESS per TEST: with the assertions split across a dozen TESTs the grid
// would be recomputed a dozen times. Each former test is now a check_*()
// section function called in order from TEST(WarmStartBattery, Corpus); a
// gtest ASSERT_ inside one of those returns from that section only, so a
// section that cannot proceed does not silence the rest.
//
// THE OMITTED CELLS: F3n1000 / warm+predictor / full_step = OFF, and the
// matched baseline (cold@pred) that would have been run on its grid. That
// single predictor sweep USED TO COST ~15.7 s in a Debug build -- more than
// every other cell put together -- because the predicted seed's inherited
// working set at n = 1000 made the threshold-crossing QP take ~195 minor
// iterations, each of them a border update on a 1001-row system with Eigen's
// Debug asserts armed. Running it landed the file at roughly 35 s in Debug,
// OVER the brief's budget, so the skip was required rather than merely
// convenient (independently measured by the Task-13 review on its own machine).
//
// THAT COST IS GONE AS OF PHASE-5 TASK 6 (the ratio-tested predictor path,
// predictor.h). The surviving F3n1000 / warm+predictor cell now reports 27
// minors where it reported 220, and its Release wall time fell 0.306 s ->
// 0.021 s. THE OMISSION IS THEREFORE NO LONGER FORCED -- which is exactly what
// the results note's open item O-5 said would happen when O-2 was fixed -- but
// it has NOT been reversed here: enabling the two cells adds two pinned rows
// and is a battery-scope decision, not a consequence of the predictor fix.
// Task 6 records the removal of the blocker and leaves the call to the phase.
//
// TO REPRODUCE THE OMITTED CELL: flip the `run_predictor_off_axis` argument of
// the "F3n1000" run_family() call in battery() from false to true. It reports
// 4 steps / 10 majors / 27 minors / 9 factorizations -- IDENTICAL to its
// full_step = ON twin in every column but `fullstep` (0), exactly as every
// other cell pair in the corpus is. That handle is named here rather than left
// as a development anecdote because everything else in this file is
// reproducible by running it.
//
// The cells are SKIPPED rather than deleted -- CellStats::measured is false for
// them, the corpus table prints them as "not run", and every section that would
// read them skips them explicitly instead of silently passing on a zeroed
// struct. The full_step = ON copies ARE run and ARE pinned, so the n = 1000
// predictor result itself is not weakened; what is given up is one instance of
// the full-step neutrality comparison, on the one cell where that comparison
// costs more than the rest of the file.
//
// WHERE THAT LEAVES THE FILE. BEFORE Phase-5 Task 6: ~1.3 s in Release and
// ~16.4 s in Debug on the author's machine; the Task-13 review measured 19.8 s
// in Debug on its own, for the file as it stood before fix round 1's fifth arm
// (which costs ~0.5 s). That was variance on ONE dominant cell, not a
// measurement disagreement -- the F3n1000 / warm+predictor cell alone was ~15 s
// of either figure and the other 57 cells shared the rest.
//
// AFTER Task 6 that cell is no longer dominant at all: 0.64 s Release / 3.0 s
// Debug for the WHOLE file (re-measured on the author's machine, clang++, MKL).
// The prediction that "anything that makes that one cell cheaper makes this
// whole file cheap" is what happened, and the file now has an order of
// magnitude of Debug headroom rather than ~34 %.
//
// =====================================================================
// ACCELERATE STANDING RULE (docs/notes/2026-07-28-accelerate-audit-
// checklist.md). THE PINS IN THIS FILE ARE THE MOST BACKEND-SENSITIVE IN THE
// REPOSITORY. Every count was MEASURED ON MKL PARDISO, the only backend
// available in this session, and the chain from backend to number is short:
// the factorization decides which trial steps are accepted, hence how many
// majors a step costs, hence -- through ContinuationOptions::target_majors --
// the dp schedule, hence WHICH PARAMETER VALUES ARE VISITED AT ALL. A first
// Accelerate run that lands on different numbers is a RE-MEASUREMENT, not
// automatically a defect.
//
// WHAT IS BACKEND-INDEPENDENT, and must hold on any backend:
//   - every cell reaches p1 with zero status failures;
//   - warm <= cold and warm+predictor <= warm in majors, on every family --
//     the INEQUALITIES and the RATIOS, not the integers they are computed
//     from;
//   - the first solve of a sweep is cold, and a later step that resolves kCold
//     is a broken hand-off;
//   - a kHot-resolved solve skipped its first factorization;
//   - zero predictor degradations.
// WHAT IS PINNED-COUNT, i.e. re-measure on a backend change:
//   check_pinned_corpus_totals() in its entirety, plus the per-cell numbers
//   quoted in the chain, majors/minors-trade and full-step sections.

#include <chrono>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <iostream>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/sqp/continuation.h>
#include <hven/detail/sqp/ledger.h>
#include <hven/detail/sqp/sqp_driver.h>
#include <hven/detail/sqp/sqp_types.h>
#include <hven/detail/sqp/warm_start.h>

#include "support/parametric_families.h"
#include "support/scale_problems.h"

namespace hven::solvers {
namespace {

using test_support::F1BoxQp;
using test_support::F2CircleNlp;
using test_support::F3SpringChain;
using test_support::F7CollocationChain;

Vec p_vec(double p) { return Vec::Constant(1, p); }

// F2 with a GENERIC start point instead of the near-optimal one Task 8 gave
// it. The only override; see this file's WHY THE COLD BASELINE NEEDS A CONTROL
// note for what it is measuring and why it is a control rather than a family.
class FarStartF2 : public F2CircleNlp {
  public:
    explicit FarStartF2(double p0) : F2CircleNlp(p0) {}
    Vec start_point() const override { return (Vec(2) << 4.0, -4.0).finished(); }
};

// ---------------------------------------------------------------------
// One cell of the grid.
// ---------------------------------------------------------------------

// FIVE arms, not four. kArmColdPred is FIX-ROUND-1's addition and it exists
// for one reason: the predictor arm does not always sweep the same parameter
// grid as the warm arm, because a predicted seed lets the adaptive-dp
// controller GROW the step (continuation.h's target_majors rule). On F3n1000
// the predictor crosses p: 0.25 -> 0.75 in FOUR solves where cold and warm
// take SIX. Comparing the predictor's total against `kArmCold` -- which is run
// on the WARM arm's grid -- therefore mixes two different savings:
//
//   (a) PER-SOLVE: is a predicted solve cheaper than a cold one at the SAME p?
//   (b) PER-TRAVERSAL: does the sweep need fewer p values at all?
//
// Both are real and (b) is a genuine benefit of the predictor, but they are
// DIFFERENT CLAIMS and the first release of this battery reported their
// product as if it were (a). kArmColdPred is the cold baseline re-run on the
// PREDICTOR's own grid, which is what makes (a) measurable; the note reports
// (a), (b) and the end-to-end product separately.
//
// On the five configurations where the two grids coincide it is a duplicate of
// kArmCold, and check_grids_are_what_the_ratios_assume() asserts exactly that,
// so the arm doubles as a self-check on the grid bookkeeping.
enum ArmIdx {
    kArmCold = 0,
    kArmWarm = 1,
    kArmPred = 2,
    kArmHot = 3,
    kArmColdPred = 4,
    kArmCount = 5
};

const char *arm_name(int a) {
    switch (a) {
    case kArmCold:
        return "cold";
    case kArmWarm:
        return "warm";
    case kArmPred:
        return "warm+pred";
    case kArmHot:
        return "hot";
    default:
        return "cold@pred";
    }
}

// Everything one cell reports. The counters are LEDGER SUMS (see this file's
// EVERY COUNT COMES OFF A LEDGER note); the fields below the divider are the
// only ones a ledger cannot know.
struct CellStats {
    // FALSE ONLY for the one cell THE RUNTIME BUDGET note names. Every section
    // below skips an unmeasured cell explicitly; nothing reads it as data.
    bool measured = false;
    Index majors = 0, minors = 0, factorizations = 0;
    Index fact_saved = 0, watchdog = 0, full_step_majors = 0;
    Index soc_steps = 0, soc_applied = 0, border_refine = 0, eqp_refine = 0;
    Index restoration_iters = 0, elastic_activations = 0;
    Index n_cold = 0, n_warm = 0, n_hot = 0;
    Index status_failures = 0;
    Index qp_records = 0;
    // PHASE-6 TASK 1 -- the retry-cost split (continuation.h's
    // ContinuationResult note), zero on every healthy cell in this file and
    // read only by the failure-economics cell at the bottom.
    // `failed_minors` is the share of `minors` spent on attempts that did not
    // converge, which is the quantity section 4.5(b) of the IPM comparison note
    // attributes 84 % of the nx = 10^5 sweep to.
    Index proposals_abandoned = 0, proposals_full_cost = 0, failed_minors = 0;
    // ---- not ledger-derivable ----
    std::size_t steps = 0;
    bool reached_p1 = false;
    Index predictor_calls = 0, predictor_degradations = 0, predictor_used = 0;
    Index res_total_majors = 0; // ContinuationResult's own sum; cross-checked
    // THE PARAMETER VALUES THIS ARM ACTUALLY VISITED, in order. Recorded per
    // arm (fix round 1) because the arms do NOT all visit the same ones -- see
    // the ArmIdx note -- and every ratio in the results note depends on which
    // grid it was computed across. check_grids_are_what_the_ratios_assume()
    // pins the relationships.
    std::vector<double> pgrid;
    double seconds = 0.0;
    // The ledger itself, kept so the hot section can walk the QP-level records
    // rather than re-running the sweep.
    Ledger ledger;
};

CellStats collect(const Ledger &ledger, const ContinuationResult &res) {
    CellStats s;
    s.measured = true;
    for (const SqpSolveRecord &r : ledger.sqp_records()) {
        s.majors += r.counters.major_iters;
        s.minors += r.counters.qp_minor_iters;
        s.factorizations += r.counters.factorizations;
        s.fact_saved += r.factorizations_saved;
        s.watchdog += r.watchdog_restores;
        s.full_step_majors += r.full_step_majors;
        s.soc_steps += r.soc_steps;
        s.soc_applied += r.soc_applied;
        s.border_refine += r.border_refine_steps;
        s.eqp_refine += r.eqp_refine_steps;
        s.restoration_iters += r.counters.restoration_iters;
        s.elastic_activations += r.counters.elastic_activations;
        switch (r.start_level_used) {
        case StartLevel::kCold:
            ++s.n_cold;
            break;
        // PHASE-6 TASK 5. Folded into n_warm on purpose: every arm of this
        // battery feeds a hand-off from a solve of the SAME model object, so
        // the hash matches and this case is unreachable here -- but folding
        // rather than dropping means that if it ever DID fire, the arm's
        // "was this ingested" accounting stays honest instead of silently
        // losing a solve. The battery's own pins would then move and say so.
        case StartLevel::kSeeded:
            ++s.n_warm;
            break;
        case StartLevel::kWarm:
            ++s.n_warm;
            break;
        case StartLevel::kHot:
            ++s.n_hot;
            break;
        }
        if (r.status != SqpStatus::kOptimal) {
            ++s.status_failures;
        }
    }
    s.qp_records = static_cast<Index>(ledger.records().size());
    s.proposals_abandoned = res.proposals_abandoned;
    s.proposals_full_cost = res.proposals_full_cost;
    for (const ContinuationStep &st : res.steps) {
        if (st.status != SqpStatus::kOptimal) {
            s.failed_minors += st.counters.qp_minor_iters;
        }
    }
    s.steps = res.steps.size();
    s.reached_p1 = res.reached_p1;
    s.predictor_calls = res.predictor_calls;
    s.predictor_degradations = res.predictor_degradations;
    s.res_total_majors = res.total_majors;
    for (const ContinuationStep &st : res.steps) {
        if (st.predictor_used) {
            ++s.predictor_used;
        }
        // Every family in this battery has parameter_dim() == 1, so one double
        // per step is the whole proposal; a multi-parameter family would need
        // the vector (and none is swept here -- see the note's §1.1).
        s.pgrid.push_back(st.p(0));
    }
    s.ledger = ledger;
    return s;
}

// The one SqpOptions every cell shares apart from the two axis fields.
//
// adaptive_mu IS OFF ON EVERY ARM, INCLUDING THE ONES THAT DO NOT NEED IT.
// The hot arm needs it: qp_engine.h's reuse condition (d) requires the
// EFFECTIVE (primal_delta, dual_mu) pair to be byte-identical across two
// consecutive solves, and the Task-10 adaptive schedule drives dual_mu off the
// KKT residual, which differs between them -- with the schedule on, kHot is
// unreachable by construction and the hot arm would be measuring nothing. It
// is then held off on the other three arms too so the arms differ in exactly
// one thing (the start), which is the only way the ratios mean anything.
SqpOptions battery_options(StartLevel level, bool full_step) {
    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    opts.start_level = level;
    opts.warm_full_step = full_step;
    return opts;
}

// The three continuation arms.
// PHASE-6 TASK 4 (M6) added the last parameter, `tune`, and DEFAULTED IT TO
// IDENTITY so every one of this file's existing call sites is unchanged: it is
// a hook for a cell that needs one SqpOptions field moved off battery_options'
// value (today exactly one does -- the failure-economics cell, which restores
// the pre-M6 fixed QP cap; see its own note).
template <typename Model>
CellStats run_sweep(Model &model, const Vec &p0, const Vec &p1, StartLevel level,
                    bool use_predictor, bool full_step, const ContinuationOptions &base,
                    std::vector<Vec> *grid_out,
                    const std::function<SqpOptions(SqpOptions)> &tune = {}) {
    SqpOptions sopts = battery_options(level, full_step);
    if (tune) {
        sopts = tune(sopts);
    }
    SqpDriver driver(sopts);
    Ledger ledger;
    driver.attach_ledger(&ledger, "sweep");
    ContinuationOptions copts = base;
    copts.use_predictor = use_predictor;
    const auto t0 = std::chrono::steady_clock::now();
    const ContinuationResult res = run_continuation(model, p0, p1, driver, copts);
    const auto t1 = std::chrono::steady_clock::now();
    CellStats s = collect(ledger, res);
    s.seconds = std::chrono::duration<double>(t1 - t0).count();
    if (grid_out != nullptr) {
        grid_out->clear();
        for (const ContinuationStep &st : res.steps) {
            grid_out->push_back(st.p);
        }
    }
    return s;
}

// The cold-each-step arm: the SAME parameter grid the warm arm visited (so the
// comparison is matched on p and not confounded by two different dp
// schedules), each point solved from scratch on its own driver.
template <typename Model>
CellStats run_cold_grid(Model &model, const std::vector<Vec> &grid, bool full_step) {
    Ledger ledger;
    ContinuationResult res;
    bool all_optimal = true;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SqpDriver driver(battery_options(StartLevel::kCold, full_step));
        driver.attach_ledger(&ledger, fmt::format("cold{}", i));
        model.set_parameters(grid[i]);
        const SqpSolution sol = driver.solve(model, model.start_point());
        all_optimal = all_optimal && sol.status == SqpStatus::kOptimal;
        ContinuationStep st;
        st.p = grid[i];
        st.x = sol.x;
        st.status = sol.status;
        st.counters = sol.counters;
        st.level = sol.counters.start_level_used;
        res.total_majors += sol.counters.major_iters;
        res.steps.push_back(std::move(st));
    }
    const auto t1 = std::chrono::steady_clock::now();
    // "Reached p1" for this arm means "solved every point on the grid": there
    // is no path being followed, so there is nothing else it could mean.
    res.reached_p1 = all_optimal;
    CellStats s = collect(ledger, res);
    s.seconds = std::chrono::duration<double>(t1 - t0).count();
    return s;
}

struct FamilyCells {
    std::string name;
    Index n = 0;
    // [0] = warm_full_step ON, [1] = OFF.
    CellStats cell[2][kArmCount];
};

// `run_predictor_off_axis` false leaves cell[1][kArmPred] unmeasured -- see
// THE RUNTIME BUDGET note. It is a per-family knob rather than a global one
// because exactly one family needs it.
template <typename Factory>
FamilyCells run_family(const char *name, Index n, Factory make, const Vec &p0, const Vec &p1,
                       const ContinuationOptions &base, bool run_predictor_off_axis = true) {
    FamilyCells fc;
    fc.name = name;
    fc.n = n;
    for (int f = 0; f < 2; ++f) {
        const bool fs = (f == 0);
        std::vector<Vec> warm_grid;
        std::vector<Vec> pred_grid;
        {
            auto m = make();
            fc.cell[f][kArmWarm] =
                run_sweep(m, p0, p1, StartLevel::kWarm, false, fs, base, &warm_grid);
        }
        if (f == 0 || run_predictor_off_axis) {
            auto m = make();
            fc.cell[f][kArmPred] =
                run_sweep(m, p0, p1, StartLevel::kWarm, true, fs, base, &pred_grid);
        }
        {
            auto m = make();
            fc.cell[f][kArmHot] = run_sweep(m, p0, p1, StartLevel::kHot, false, fs, base, nullptr);
        }
        {
            auto m = make();
            fc.cell[f][kArmCold] = run_cold_grid(m, warm_grid, fs);
        }
        // THE MATCHED BASELINE (fix round 1): the same from-scratch arm, run on
        // the PREDICTOR's own grid, so `pred` vs `cold@pred` is a per-solve
        // comparison at identical parameter values. Skipped exactly when the
        // predictor cell it is matched to was skipped -- it has no grid to run
        // on then.
        if (!pred_grid.empty()) {
            auto m = make();
            fc.cell[f][kArmColdPred] = run_cold_grid(m, pred_grid, fs);
        }
    }
    return fc;
}

// THE WHOLE GRID, computed exactly once per process.
const std::vector<FamilyCells> &battery() {
    static const std::vector<FamilyCells> table = [] {
        const ContinuationOptions plain;
        ContinuationOptions stress;
        stress.dp_init = 0.5;
        stress.dp_max = 0.5;
        stress.target_majors = 0; // never grow: every step is a LONG one
        std::vector<FamilyCells> t;
        t.push_back(
            run_family("F1", 2, [] { return F1BoxQp(0.0); }, p_vec(0.0), p_vec(1.0), plain));
        t.push_back(
            run_family("F2", 2, [] { return F2CircleNlp(0.0); }, p_vec(0.0), p_vec(1.0), plain));
        t.push_back(
            run_family("F2far", 2, [] { return FarStartF2(0.0); }, p_vec(0.0), p_vec(1.0), plain));
        t.push_back(run_family(
            "F3n50", 50, [] { return F3SpringChain(50, 0.5, 0.25); }, p_vec(0.25), p_vec(0.75),
            plain));
        t.push_back(run_family(
            "F3n1000", 1000, [] { return F3SpringChain(1000, 0.5, 0.25); }, p_vec(0.25),
            p_vec(0.75), plain, /*run_predictor_off_axis=*/false));
        t.push_back(run_family(
            "F3stress", 200, [] { return F3SpringChain(200, 0.5, 0.1); }, p_vec(0.1), p_vec(2.0),
            stress));
        return t;
    }();
    return table;
}

const FamilyCells &family(const char *name) {
    for (const FamilyCells &f : battery()) {
        if (f.name == name) {
            return f;
        }
    }
    // FATAL rather than ADD_FAILURE-and-fall-through (fix round 1). Returning
    // battery().front() on a mistyped name silently compared everything against
    // F1 and buried the real failure under a cascade of misleading diffs.
    // Project rule T6: the throw carries the name it could not find.
    throw std::invalid_argument(
        fmt::format("WarmStartBattery: no family named '{}' in the corpus", name));
}

// A whole-corpus dump, attached to the pinned assertions so a re-measurement
// (a backend change, a tuning change) reads the new table off the failure
// message instead of needing a debugger. It is also emitted once, at the end
// of the corpus test, as the results note's machine-produced data source.
std::string corpus_table() {
    std::string s = "\n";
    s += fmt::format("{:<9} {:<10} {:<5} {:<6} {:<7} {:<7} {:<6} {:<6} {:<3} {:<9} {:<7} {:<10} "
                     "{:<9} {}\n",
                     "family", "arm", "full", "steps", "majors", "minors", "fact", "saved", "wd",
                     "fullstep", "brdref", "lvl c/w/h", "pred c/d", "secs");
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            for (int a = 0; a < kArmCount; ++a) {
                const CellStats &c = f.cell[fi][a];
                if (!c.measured) {
                    s += fmt::format("{:<9} {:<10} {:<5} (not run -- see THE RUNTIME BUDGET)\n",
                                     f.name, arm_name(a), fi == 0 ? "on" : "off");
                    continue;
                }
                s += fmt::format(
                    "{:<9} {:<10} {:<5} {:<6} {:<7} {:<7} {:<6} {:<6} {:<3} {:<9} "
                    "{:<7} {:<10} {:<9} {:.3f}\n",
                    f.name, arm_name(a), fi == 0 ? "on" : "off", c.steps, c.majors, c.minors,
                    c.factorizations, c.fact_saved, c.watchdog, c.full_step_majors, c.border_refine,
                    fmt::format("{}/{}/{}", c.n_cold, c.n_warm, c.n_hot),
                    fmt::format("{}/{}", c.predictor_calls, c.predictor_degradations), c.seconds);
            }
        }
    }
    return s;
}

// =====================================================================
// THE SECTIONS. Each was a separate TEST until THE RUNTIME BUDGET note's
// process-count argument merged them; they are called in order from
// TEST(WarmStartBattery, Corpus) below and each stands alone.
// =====================================================================

// ---------------------------------------------------------------------
// (1) NOTHING FAILED ANYWHERE. The brief's zero-status-failures requirement,
// and the precondition for reading any other number in this file.
// ---------------------------------------------------------------------
void check_no_status_failures() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            for (int a = 0; a < kArmCount; ++a) {
                const CellStats &c = f.cell[fi][a];
                if (!c.measured) {
                    continue;
                }
                SCOPED_TRACE(fmt::format("{} / {} / full_step={}", f.name, arm_name(a),
                                         fi == 0 ? "on" : "off"));
                EXPECT_TRUE(c.reached_p1) << "the sweep stopped short" << corpus_table();
                EXPECT_EQ(c.status_failures, 0)
                    << "a solve in this cell exited non-kOptimal" << corpus_table();
                EXPECT_GT(c.steps, 0u);
            }
        }
    }
}

// ---------------------------------------------------------------------
// (2) THE LEDGER IS A FAITHFUL RECORD. Every other section reads counts off
// Ledger::sqp_records(); this one pins that those counts are the same ones the
// driver itself reported, so a ledger that silently stopped recording (or
// recorded twice) cannot make the rest of this file pass on wrong numbers.
// ---------------------------------------------------------------------
void check_ledger_agrees_with_the_driver() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            for (int a = 0; a < kArmCount; ++a) {
                const CellStats &c = f.cell[fi][a];
                if (!c.measured) {
                    continue;
                }
                SCOPED_TRACE(fmt::format("{} / {} / full_step={}", f.name, arm_name(a),
                                         fi == 0 ? "on" : "off"));
                // Exactly one SqpSolveRecord per solve, and a sweep makes
                // exactly one solve per recorded step.
                EXPECT_EQ(c.ledger.sqp_records().size(), c.steps);
                EXPECT_EQ(c.majors, c.res_total_majors)
                    << "the ledger's summed major_iters and ContinuationResult::total_majors "
                       "disagree -- one of the two is no longer measuring the solves that ran"
                    << corpus_table();
            }
        }
    }
}

// ---------------------------------------------------------------------
// (3) THE CHAIN. The first solve of a sweep is cold (there is nothing to warm
// from) and every step past it should resolve kWarm or above -- continuation.h
// says a later kCold "means the hand-off silently broke, which is invisible in
// the answers and is the single most useful thing in this record".
//
// PHASE-5 TASK 0 RE-MEASUREMENT (O-1's repair). It used to be broken in two
// cells and that was PINNED here rather than papered over: F1/warm+pred
// resolved kCold on 4 of its 5 solves and F3stress/warm+pred on 2 of 5,
// because a step that converged at ZERO majors emitted a hand-off carrying
// structure_hash == 0 and the next solve read that sentinel as a mismatch.
// sqp_driver.h's make_warm_start now probes the model's structure on exactly
// those exits (see TEST(WarmStartBattery, ZeroMajorStepKeepsTheWarmStartChain)
// for the mechanism, isolated from any sweep), so EVERY arm now chains
// perfectly and the predicted arm is checked by the same loop as the others
// rather than by a pinned list of exceptions.
// ---------------------------------------------------------------------
void check_cold_first_then_warm() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            // The two cold arms are all-cold by design and are excluded by name
            // rather than by an index range (fix round 1: kArmColdPred sits
            // ABOVE kArmHot, so the old `a = kArmWarm; a < kArmCount` range
            // would have swept it in).
            for (int a : {kArmWarm, kArmPred, kArmHot}) {
                const CellStats &c = f.cell[fi][a];
                if (!c.measured) {
                    continue;
                }
                SCOPED_TRACE(fmt::format("{} / {} / full_step={}", f.name, arm_name(a),
                                         fi == 0 ? "on" : "off"));
                ASSERT_FALSE(c.ledger.sqp_records().empty());
                EXPECT_EQ(c.ledger.sqp_records().front().start_level_used, StartLevel::kCold)
                    << "the FIRST solve of a sweep has nothing to warm from";
                EXPECT_GE(c.n_cold, 1);
            }
        }
        // EVERY warm-started arm chains perfectly: exactly one cold solve, the
        // one at p0, and every later step warm (or hot). kArmPred joined this
        // loop with O-1's repair -- before it, a predicted step that converged
        // at zero majors broke the very next step's hand-off (see this
        // section's own note above).
        for (int fi = 0; fi < 2; ++fi) {
            for (int a : {kArmWarm, kArmPred, kArmHot}) {
                const CellStats &c = f.cell[fi][a];
                if (!c.measured) {
                    continue; // today only kArmPred/kArmColdPred can be skipped
                }
                SCOPED_TRACE(fmt::format("{} / {} / full_step={}", f.name, arm_name(a),
                                         fi == 0 ? "on" : "off"));
                EXPECT_EQ(c.n_cold, 1) << "a step past the first re-solved COLD" << corpus_table();
                EXPECT_EQ(c.n_warm + c.n_hot, static_cast<Index>(c.steps) - 1);
            }
        }
    }
    // THE TWO CELLS THE DEFECT USED TO OWN, kept as NAMED pins rather than
    // left to the loop above -- they are the ones a regression would show up
    // in first, and naming them is what makes this a positive statement about
    // the repair instead of a silent absence. Re-measured (MKL Pardiso,
    // clang++) after Phase-5 Task 0: 4 -> 1 on F1/warm+pred (its predictor is
    // EXACT on the affine middle branch, so three of its four steps converge
    // at zero majors and USED to poison the next step's hand-off) and 2 -> 1
    // on F3stress/warm+pred.
    EXPECT_EQ(family("F1").cell[0][kArmPred].n_cold, 1)
        << "a zero-major predicted step no longer poisons the NEXT step's hand-off";
    EXPECT_EQ(family("F1").cell[1][kArmPred].n_cold, 1);
    EXPECT_EQ(family("F3stress").cell[0][kArmPred].n_cold, 1);
    EXPECT_EQ(family("F3stress").cell[1][kArmPred].n_cold, 1);
}

// ---------------------------------------------------------------------
// (3c) THE GRIDS THE RATIOS ARE COMPUTED ACROSS -- fix round 1, and the fix to
// the one framing defect the first release of this battery shipped.
//
// THE DEFECT: the note's headline predictor figure divided the predictor arm's
// TOTAL by the cold arm's TOTAL and described the result as a per-solve saving
// "on the same parameter grid". On F3n1000 that is false -- the predictor
// crosses p: 0.25 -> 0.75 in FOUR solves where cold and warm take SIX, because
// a predicted seed converges inside target_majors and continuation.h's
// controller then GROWS dp. The quotient was therefore the product of two
// different savings, and the cheaper-per-solve one was overstated.
//
// WHAT IS PINNED HERE, so the confound cannot come back silently:
//   - the WARM arm's grid is byte-identical to the COLD arm's, which is what
//     makes the cold-vs-warm ratios per-solve comparisons (this was already
//     asserted, but only through `steps`; it is now asserted on the VALUES);
//   - the PREDICTOR arm's grid is a PREFIX-FREE fact of its own: its length is
//     pinned per family, and it is byte-identical to kArmColdPred's, which is
//     what makes the (a) per-solve figures in the note's §2 legitimate;
//   - the ONE configuration where the predictor's grid differs from the warm
//     arm's is named and pinned, so a second one appearing is a test failure
//     rather than a silently wrong headline.
// ---------------------------------------------------------------------
void check_grids_are_what_the_ratios_assume() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            SCOPED_TRACE(fmt::format("{} / full_step={}", f.name, fi == 0 ? "on" : "off"));
            const CellStats &cold = f.cell[fi][kArmCold];
            const CellStats &warm = f.cell[fi][kArmWarm];
            const CellStats &pred = f.cell[fi][kArmPred];
            const CellStats &coldp = f.cell[fi][kArmColdPred];

            // cold IS warm's grid, by construction in run_family -- asserted so
            // the construction cannot drift.
            EXPECT_EQ(cold.pgrid, warm.pgrid)
                << "the cold baseline must re-solve the WARM arm's own parameter values"
                << corpus_table();
            // The hot arm resolves the same sweep as warm and must land on the
            // same values; otherwise the hot-vs-warm equalities in section (7)
            // would be comparing two different traversals.
            EXPECT_EQ(f.cell[fi][kArmHot].pgrid, warm.pgrid) << corpus_table();

            if (!pred.measured) {
                EXPECT_FALSE(coldp.measured)
                    << "the matched baseline has no grid to run on when its predictor cell was "
                       "skipped";
                continue;
            }
            ASSERT_TRUE(coldp.measured);
            // THE ONE THAT MAKES THE NOTE'S (a) FIGURES HONEST.
            EXPECT_EQ(coldp.pgrid, pred.pgrid)
                << "the matched baseline must re-solve the PREDICTOR's own parameter values"
                << corpus_table();
            // A predicted sweep can never need MORE proposals than an
            // unpredicted one here: dp only grows on a step that converged
            // within target_majors, and prediction only makes that likelier.
            EXPECT_LE(pred.pgrid.size(), warm.pgrid.size()) << corpus_table();
        }
    }

    // PINNED (measured, MKL Pardiso): F3n1000 is the SOLE configuration whose
    // predictor grid differs from its warm grid -- 4 proposals against 6. The
    // note reports that as its own separate benefit; a second family showing up
    // here means a headline ratio somewhere else has quietly become a product
    // of two effects too.
    for (const FamilyCells &f : battery()) {
        const std::size_t warm_steps = f.cell[0][kArmWarm].pgrid.size();
        const std::size_t pred_steps = f.cell[0][kArmPred].pgrid.size();
        if (f.name == "F3n1000") {
            EXPECT_EQ(warm_steps, 6u);
            EXPECT_EQ(pred_steps, 4u) << "the fewer-sweep-steps effect, pinned at its source";
        } else {
            EXPECT_EQ(pred_steps, warm_steps)
                << f.name
                << ": a NEW grid mismatch -- the note's per-solve ratios for this "
                   "family are no longer per-solve"
                << corpus_table();
        }
    }

    // AND THE MATCHED BASELINE IS THE COLD BASELINE wherever the two grids
    // agree: kArmColdPred re-runs the identical work, so every counter must
    // match. This is what lets the note quote one cold column for five of the
    // six configurations instead of two.
    for (const FamilyCells &f : battery()) {
        if (f.name == std::string("F3n1000")) {
            continue;
        }
        for (int fi = 0; fi < 2; ++fi) {
            SCOPED_TRACE(fmt::format("{} / full_step={}", f.name, fi == 0 ? "on" : "off"));
            EXPECT_EQ(f.cell[fi][kArmColdPred].majors, f.cell[fi][kArmCold].majors);
            EXPECT_EQ(f.cell[fi][kArmColdPred].minors, f.cell[fi][kArmCold].minors);
            EXPECT_EQ(f.cell[fi][kArmColdPred].factorizations, f.cell[fi][kArmCold].factorizations);
        }
    }
}

// ---------------------------------------------------------------------
// (4) THE HEADLINE INEQUALITY: warm-starting a sweep never costs more than
// solving each point from scratch, in majors OR in factorizations. Backend-
// independent. The two ties are named individually with their reasons -- see
// this file's WHY THE COLD BASELINE NEEDS A CONTROL note.
// ---------------------------------------------------------------------
void check_warm_never_costs_more_than_cold() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            const CellStats &cold = f.cell[fi][kArmCold];
            const CellStats &warm = f.cell[fi][kArmWarm];
            SCOPED_TRACE(fmt::format("{} / full_step={}", f.name, fi == 0 ? "on" : "off"));
            EXPECT_LE(warm.majors, cold.majors) << corpus_table();
            EXPECT_LE(warm.factorizations, cold.factorizations) << corpus_table();
            // Matched grids: the cold arm re-solves the warm arm's own
            // parameter values, so any difference is the START and nothing
            // else.
            EXPECT_EQ(cold.steps, warm.steps);
        }
    }

    // STRICT wherever a cold start has real work to do.
    for (const char *name : {"F2far", "F3n50", "F3n1000", "F3stress"}) {
        for (int fi = 0; fi < 2; ++fi) {
            const FamilyCells &f = family(name);
            EXPECT_LT(f.cell[fi][kArmWarm].majors, f.cell[fi][kArmCold].majors) << name;
            EXPECT_LT(f.cell[fi][kArmWarm].factorizations, f.cell[fi][kArmCold].factorizations)
                << name;
        }
    }

    // THE TWO TIES, pinned with their causes attached so neither can be
    // mistaken for a regression later.
    for (int fi = 0; fi < 2; ++fi) {
        const FamilyCells &f1 = family("F1");
        EXPECT_EQ(f1.cell[fi][kArmWarm].majors, f1.cell[fi][kArmCold].majors)
            << "F1 is a QP: ONE subproblem is the whole problem from any start, so the major "
               "count is start-independent and this tie is structural, not a regression";
        EXPECT_LT(f1.cell[fi][kArmWarm].factorizations, f1.cell[fi][kArmCold].factorizations)
            << "on F1 the warm-start win is entirely at the factorization level (the sweep's one "
               "engine keeps K0 across steps; the fresh-driver baseline cannot)";

        const FamilyCells &f2 = family("F2");
        const FamilyCells &f2far = family("F2far");
        EXPECT_EQ(f2.cell[fi][kArmWarm].majors, f2.cell[fi][kArmCold].majors)
            << "F2's own start_point() (0.5, 0.5) is within one Newton step of x*(p) for every p "
               "swept, so the cold arm is already near its floor -- a FIXTURE property";
        EXPECT_LT(f2far.cell[fi][kArmWarm].majors, f2far.cell[fi][kArmCold].majors)
            << "THE CONTROL: the identical family with a generic start point breaks the tie, so "
               "the F2 result measures the start point, not the warm-start subsystem";
    }
}

// ---------------------------------------------------------------------
// (5) THE PREDICTOR NEVER COSTS MORE MAJORS THAN THE PLAIN WARM ARM. The
// brief's warm+predictor <= warm, in the currency the brief names.
// Backend-independent.
// ---------------------------------------------------------------------
void check_predictor_never_costs_more_majors() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            const CellStats &pred = f.cell[fi][kArmPred];
            if (!pred.measured) {
                continue;
            }
            SCOPED_TRACE(fmt::format("{} / full_step={}", f.name, fi == 0 ? "on" : "off"));
            // vs WARM: both sweep p0 -> p1, so this is a per-TRAVERSAL claim
            // and the predictor is allowed to get there in fewer proposals.
            EXPECT_LE(pred.majors, f.cell[fi][kArmWarm].majors) << corpus_table();
            // vs the MATCHED cold baseline: identical parameter values, so this
            // is the PER-SOLVE claim, and it is the one the results note's
            // §2 (a) column reports. Before fix round 1 this compared against
            // kArmCold, whose grid is the WARM arm's -- on F3n1000 a 4-point
            // sweep against a 6-point one (see check_grids_are_what_the_ratios
            // _assume).
            ASSERT_TRUE(f.cell[fi][kArmColdPred].measured);
            EXPECT_LE(pred.majors, f.cell[fi][kArmColdPred].majors)
                << "the predictor must not cost more than solving the SAME parameter values "
                   "from scratch"
                << corpus_table();
            EXPECT_LE(pred.factorizations, f.cell[fi][kArmColdPred].factorizations)
                << corpus_table();
        }
    }
}

// ---------------------------------------------------------------------
// (5b) ...AND, SINCE PHASE-5 TASK 6, IT IS NOT PAID FOR IN MINORS EITHER.
//
// WHAT THIS SECTION USED TO ASSERT, because it matters that the reversal is
// deliberate and not a loosened test: on the two largest cells the predictor
// bought its major reduction with a large INCREASE in QP minor iterations (and
// wall time), because the predicted seed's inherited working set was wrong
// across an activation threshold and the QP paid to repair it. Measured per
// step on F3n1000 (warm+predictor), the single step 0.35 -> 0.55 crossing
// p_act = 0.5 cost 6 majors and 195 minors against the plain warm arm's 6 and
// 18. That was the results note's open item O-2, and this section asserted the
// defect (`pred.minors > warm.minors`) precisely so that its repair could not
// land unnoticed.
//
// IT LANDED (Task 6, predictor.h's RATIO TEST section): the same step now costs
// ONE major and TWO minors, because the prediction pins the one variable the
// far side actually has on its ceiling instead of ninety. So the ASSERTION IS
// INVERTED, in the same currency, on the same two cells -- the predictor must
// now cost STRICTLY FEWER minors than the plain warm arm, not more. The
// per-family totals behind it are pinned in kPins; this section is the
// directional claim, which is the thing a future change could break while
// leaving some pin coincidentally satisfied.
// ---------------------------------------------------------------------
void check_predictor_no_longer_trades_majors_for_minors() {
    const FamilyCells &big = family("F3n1000");
    EXPECT_LT(big.cell[0][kArmPred].majors, big.cell[0][kArmWarm].majors);
    EXPECT_LT(big.cell[0][kArmPred].minors, big.cell[0][kArmWarm].minors)
        << "O-2 has come back: the predictor's minor-iteration cost at n = 1000" << corpus_table();
    // And against the MATCHED per-solve baseline, which is the comparison the
    // 2.14x tax was actually reported in (§2 (a) of the results note).
    EXPECT_LT(big.cell[0][kArmPred].minors, big.cell[0][kArmColdPred].minors) << corpus_table();
    for (int fi = 0; fi < 2; ++fi) {
        const FamilyCells &st = family("F3stress");
        EXPECT_LT(st.cell[fi][kArmPred].majors, st.cell[fi][kArmWarm].majors);
        EXPECT_LT(st.cell[fi][kArmPred].minors, st.cell[fi][kArmWarm].minors) << corpus_table();
    }
}

// ---------------------------------------------------------------------
// (6) EVERY PREDICTION WAS A REAL PREDICTION. continuation.h passes a non-null
// PredictorOutcome on every predict() call precisely so a sweep in which every
// prediction silently DEGRADED to the identity is distinguishable from a
// healthy one. Expected, and measured: zero degradations corpus-wide.
// Backend-independent.
// ---------------------------------------------------------------------
void check_predictor_never_degrades() {
    for (const FamilyCells &f : battery()) {
        for (int fi = 0; fi < 2; ++fi) {
            const CellStats &pred = f.cell[fi][kArmPred];
            if (pred.measured) {
                SCOPED_TRACE(fmt::format("{} / full_step={}", f.name, fi == 0 ? "on" : "off"));
                EXPECT_GT(pred.predictor_calls, 0) << "the predicted arm never called predict()";
                EXPECT_EQ(pred.predictor_degradations, 0) << corpus_table();
                EXPECT_EQ(pred.predictor_used, pred.predictor_calls)
                    << "every call reported kPredicted, so every call was taken";
            }
            // The non-predicting arms must not be calling it at all.
            for (int a : {kArmCold, kArmWarm, kArmHot}) {
                EXPECT_EQ(f.cell[fi][a].predictor_calls, 0) << f.name << " / " << arm_name(a);
            }
        }
    }
}

// ---------------------------------------------------------------------
// (7) THE HOT LEVEL ACTUALLY SKIPPED FACTORIZATIONS -- the brief's ">= 1 hot
// step with qp_factorizations == 0 on F3", derived from the LEDGER and from
// nothing else.
//
// HOW THE LINKAGE IS MADE, since a QP-level SolveRecord does not name the
// driver solve it belongs to: the QP records are chronological, so walking
// them while consuming counters.major_iters of them per SqpSolveRecord
// partitions them by solve. That partition is only valid when every
// subproblem is a major -- an SOC re-solve or an elastic rung emits a QP
// record without incrementing major_iters -- so the walk ASSERTS the totals
// match before trusting itself, and fails loudly rather than mis-attributing.
// (Measured: on the F3 hot cells they match exactly, 12 == 12 and 30 == 30,
// because nothing on those cells triggers SOC or elastic.)
//
// WHY THE FIRST SUBPROBLEM AND NOT ANY: warm_start.h's `hot` handle is
// consumed once, at ingest, so only a solve's first subproblem is ever a reuse
// candidate; every later major builds a fresh linearization and pays. This is
// exactly what mutation M3 (this file's header) demonstrates.
// ---------------------------------------------------------------------
void check_hot_steps_skip_factorizations() {
    for (const char *name : {"F3n50", "F3n1000"}) {
        for (int fi = 0; fi < 2; ++fi) {
            const CellStats &hot = family(name).cell[fi][kArmHot];
            SCOPED_TRACE(fmt::format("{} / hot / full_step={}", name, fi == 0 ? "on" : "off"));

            ASSERT_EQ(hot.qp_records, hot.majors)
                << "the record-per-major partition below is only valid when every subproblem is "
                   "a major; an SOC or elastic re-solve on this cell would break it";
            ASSERT_GT(hot.n_hot, 0) << "no solve on the hot arm resolved kHot at all";
            EXPECT_EQ(hot.fact_saved, hot.n_hot)
                << "ledger.h defines factorizations_saved as exactly (start_level_used == kHot)";

            std::size_t qp_cursor = 0;
            Index hot_first_subproblems_at_zero = 0;
            for (const SqpSolveRecord &rec : hot.ledger.sqp_records()) {
                if (rec.counters.major_iters == 0) {
                    continue; // nothing was solved: no first subproblem to look at
                }
                const SolveRecord &first = hot.ledger.records()[qp_cursor];
                qp_cursor += static_cast<std::size_t>(rec.counters.major_iters);
                if (rec.start_level_used != StartLevel::kHot) {
                    continue;
                }
                EXPECT_TRUE(first.counters.k0_reused)
                    << "a kHot resolution means the engine's own reuse gate passed on this "
                       "solve's first subproblem";
                EXPECT_EQ(first.counters.factorizations, 0)
                    << "THE PIN: the first subproblem of a hot step paid NO factorization";
                if (first.counters.k0_reused && first.counters.factorizations == 0) {
                    ++hot_first_subproblems_at_zero;
                }
            }
            EXPECT_GT(hot_first_subproblems_at_zero, 0)
                << "the brief's requirement: at least one ledger-evidenced hot step at zero "
                   "factorizations on F3"
                << corpus_table();
        }
    }

    // WHAT KHOT IS AND IS NOT WORTH ON A ONE-DRIVER SWEEP, which the ledger
    // makes plain and prose would not: on F1 and F3 the hot arm's majors,
    // minors and factorizations are IDENTICAL to the warm arm's. The engine's
    // own K0 cache already skips those factorizations for a warm sweep on a
    // single driver; `hot` buys nothing there and its value is the
    // CROSS-INSTANCE hand-off (tests/test_warm_start.cpp's
    // HotReusesFactorization) that a single-driver sweep never exercises.
    for (const char *name : {"F1", "F3n50", "F3n1000", "F3stress"}) {
        for (int fi = 0; fi < 2; ++fi) {
            const FamilyCells &f = family(name);
            EXPECT_EQ(f.cell[fi][kArmHot].majors, f.cell[fi][kArmWarm].majors) << name;
            EXPECT_EQ(f.cell[fi][kArmHot].factorizations, f.cell[fi][kArmWarm].factorizations)
                << name;
            EXPECT_GT(f.cell[fi][kArmHot].fact_saved, 0) << name;
        }
    }
    // ON F2 KHOT IS UNREACHABLE and degrades silently, which is the contract
    // working: F2's Ji = [2x1, 2x2] changes VALUES at every iterate, so
    // qp_engine.h's reuse condition (c) can never hold across two solves.
    for (const char *name : {"F2", "F2far"}) {
        for (int fi = 0; fi < 2; ++fi) {
            const FamilyCells &f = family(name);
            EXPECT_EQ(f.cell[fi][kArmHot].n_hot, 0)
                << name << ": a values-hash mismatch must degrade to kWarm, silently";
            EXPECT_EQ(f.cell[fi][kArmHot].fact_saved, 0) << name;
            EXPECT_EQ(f.cell[fi][kArmHot].majors, f.cell[fi][kArmWarm].majors) << name;
        }
    }
}

// ---------------------------------------------------------------------
// (8) THE FULL-STEP DECISION INPUT. See this file's own FULL-STEP DECISION
// INPUT note; the results note has the recommendation. This section's job is
// to pin the NULL RESULT precisely enough that it stops being one the moment
// anything changes.
// ---------------------------------------------------------------------
void check_full_step_is_neutral() {
    for (const FamilyCells &f : battery()) {
        for (int a = 0; a < kArmCount; ++a) {
            const CellStats &on = f.cell[0][a];
            const CellStats &off = f.cell[1][a];
            SCOPED_TRACE(fmt::format("{} / {}", f.name, arm_name(a)));
            EXPECT_EQ(on.watchdog, 0)
                << "the watchdog never had to rescue anything" << corpus_table();
            if (!off.measured) {
                continue; // THE RUNTIME BUDGET's one omitted cell
            }
            EXPECT_EQ(on.majors, off.majors) << corpus_table();
            EXPECT_EQ(on.minors, off.minors) << corpus_table();
            EXPECT_EQ(on.factorizations, off.factorizations) << corpus_table();
            EXPECT_EQ(on.fact_saved, off.fact_saved);
            EXPECT_EQ(on.border_refine, off.border_refine) << corpus_table();
            EXPECT_EQ(on.elastic_activations, off.elastic_activations) << corpus_table();
            EXPECT_EQ(on.restoration_iters, off.restoration_iters);
            EXPECT_EQ(on.steps, off.steps);
            EXPECT_EQ(on.n_cold, off.n_cold);
            EXPECT_EQ(on.n_warm, off.n_warm);
            EXPECT_EQ(on.n_hot, off.n_hot);
            // THE LEVER IS OFF, NOT ABSENT: with warm_full_step false the mode
            // never arms, on any arm.
            EXPECT_EQ(off.full_step_majors, 0) << corpus_table();
            EXPECT_EQ(off.watchdog, 0) << corpus_table();
        }
        // THE NULL IS NOT "THE FEATURE NEVER RAN": with the lever on, the mode
        // armed for a substantial share of every warm and hot cell's majors.
        for (int a : {kArmWarm, kArmHot}) {
            const CellStats &on = f.cell[0][a];
            EXPECT_GT(on.full_step_majors, 0) << f.name << " / " << arm_name(a)
                                              << ": full-step mode never engaged" << corpus_table();
            EXPECT_LE(on.full_step_majors, on.majors) << "sqp_types.h: <= major_iters always";
        }
        // A cold solve cannot arm it -- sqp_types.h's "IT CANNOT AFFECT a cold
        // solve" clause, which the cold arm gets to pin for free.
        EXPECT_EQ(f.cell[0][kArmCold].full_step_majors, 0) << f.name;
    }
}

// ---------------------------------------------------------------------
// (9) THE WHOLE TABLE, PINNED. These are the observed values the results note
// reports; they are the backend-sensitive half of this file (see the
// ACCELERATE STANDING RULE note) and a diff here is a re-measurement, not
// automatically a defect. Pinned for the lever-ON copy only; the OFF copy is
// pinned equal to it by (8) above, which is both shorter and stronger than
// writing the same 24 rows twice.
// ---------------------------------------------------------------------
struct Pin {
    Index steps, majors, minors, factorizations, fact_saved, full_step_majors;
    Index n_cold, n_warm, n_hot;
    Index predictor_calls;
    Index border_refine;
    Index elastic_activations;
};

struct FamilyPin {
    const char *name;
    Pin arm[kArmCount]; // cold, warm, warm+pred, hot, cold@pred
};

// MEASURED ON MKL PARDISO, clang++, Release and Debug (identical). Column
// order matches Pin above: steps, majors, minors, factorizations,
// factorizations_saved, full_step_majors, then the resolved-level histogram
// (cold/warm/hot), predictor calls, border refinement steps, and elastic
// activations. Row order matches ArmIdx: cold, warm, warm+pred, hot,
// cold@pred.
const FamilyPin kPins[] = {
    {"F1",
     {{5, 5, 13, 5, 0, 0, 5, 0, 0, 0, 17, 0},
      {5, 5, 16, 1, 0, 4, 1, 4, 0, 0, 20, 0},
      // RE-MEASURED, Phase-5 Task 0 (O-1's repair): the level histogram moved
      // 4/1/0 -> 1/4/0. Nothing else in this row did -- the three zero-major
      // steps were already converging at zero majors, so accepting their
      // hand-off changes which LEVEL they resolve at and no count downstream
      // of it.
      {5, 1, 3, 1, 0, 0, 1, 4, 0, 4, 4, 0},
      {5, 5, 16, 1, 4, 4, 1, 0, 4, 0, 20, 0},
      {5, 5, 13, 5, 0, 0, 5, 0, 0, 0, 17, 0}}},
    {"F2",
     {{5, 8, 16, 8, 0, 0, 5, 0, 0, 0, 16, 0},
      {5, 8, 17, 8, 0, 7, 1, 4, 0, 0, 17, 0},
      {5, 7, 14, 7, 0, 6, 1, 4, 0, 4, 14, 0},
      {5, 8, 17, 8, 0, 7, 1, 4, 0, 0, 17, 0},
      {5, 8, 16, 8, 0, 0, 5, 0, 0, 0, 16, 0}}},
    {"F2far",
     {{5, 19, 158, 24, 0, 0, 5, 0, 0, 0, 159, 5},
      {5, 10, 45, 11, 0, 7, 1, 4, 0, 0, 45, 1},
      {5, 9, 42, 10, 0, 6, 1, 4, 0, 4, 42, 1},
      {5, 10, 45, 11, 0, 7, 1, 4, 0, 0, 45, 1},
      {5, 19, 158, 24, 0, 0, 5, 0, 0, 0, 159, 5}}},
    {"F3n50",
     {{4, 19, 38, 19, 0, 0, 4, 0, 0, 0, 38, 0},
      {4, 12, 23, 9, 0, 8, 1, 3, 0, 0, 23, 0},
      // RE-MEASURED, Phase-5 Task 6 (the ratio-tested predictor path,
      // predictor.h): 7/23/6 majors/minors/factorizations -> 6/12/5, and
      // full_step_majors 3 -> 2 because there is one fewer major to arm the
      // rule on. Steps, the level histogram and the predictor-call count are
      // unchanged -- the predictor is called the same number of times and the
      // dp schedule it drives is the same; only what each prediction hands the
      // QP changed. border_refine tracks minors here as it does everywhere
      // (open item O-6).
      {4, 6, 12, 5, 0, 2, 1, 3, 0, 3, 12, 0},
      {4, 12, 23, 9, 3, 8, 1, 0, 3, 0, 23, 0},
      {4, 19, 38, 19, 0, 0, 4, 0, 0, 0, 38, 0}}},
    {"F3n1000",
     {{6, 53, 156, 53, 0, 0, 6, 0, 0, 0, 156, 0},
      // PHASE-5 TASK 2's bench harness HARD-CODES A COPY of this exact
      // warm-arm (kArmWarm) row as its --self-check correctness gate --
      // bench/bench_scale.cpp's kF3n1000WarmPin (~line 356) and
      // run_self_check() (~line 369). If this row is ever re-measured/re-
      // pinned, the bench copy MUST be updated to match and --self-check
      // re-run (both builds) before that lands -- this is the grep-sweep
      // convention's own "the duplicate is discoverable from the side where
      // re-pins happen" instance.
      {6, 30, 84, 25, 0, 22, 1, 5, 0, 0, 84, 0},
      // RE-MEASURED, Phase-5 Task 6 -- THE O-2 CELL, and the headline of that
      // task: 15 majors / 220 minors / 14 factorizations -> 10 / 27 / 9, with
      // full_step_majors 7 -> 2 and wall time 0.306 s -> 0.021 s. The
      // pre-Task-6 predictor pinned NINETY variables on the ceiling at the one
      // step crossing p_act = 0.5, where the true far-side active set is
      // exactly ONE; the ratio-tested path pins the one. Steps (4), the level
      // histogram and the predictor-call count are unchanged: the same four
      // predictions are made at the same four parameter values.
      {4, 10, 27, 9, 0, 2, 1, 3, 0, 3, 27, 0},
      {6, 30, 84, 25, 5, 22, 1, 0, 5, 0, 84, 0},
      // THE MATCHED BASELINE, and the number fix round 1 turns on: solving the
      // predictor's OWN four parameter values from scratch costs 35 majors /
      // 103 minors / 35 factorizations, against the predictor's 10 / 27 / 9
      // (Phase-5 Task 6; it was 15 / 220 / 14, which is where the 2.14x minor
      // TAX the results note reported came from -- it is now a 0.26x saving).
      // The six-point cold arm's 53 / 156 / 53 in the row above is a LONGER
      // TRAVERSAL and is not the per-solve comparator.
      {4, 35, 103, 35, 0, 0, 4, 0, 0, 0, 103, 0}}},
    {"F3stress",
     {{5, 33, 98, 33, 0, 0, 5, 0, 0, 0, 98, 0},
      {5, 15, 39, 11, 0, 10, 1, 4, 0, 0, 39, 0},
      // RE-MEASURED, Phase-5 Task 0 (O-1's repair). The only row in the corpus
      // where accepting a zero-major hand-off changes WORK and not just the
      // level histogram: 2/3/0 -> 1/4/0, and with the reseeded working set and
      // multipliers the step that used to re-solve cold now costs one fewer
      // factorization (11 -> 10) and one fewer QP minor (94 -> 93, mirrored in
      // border_refine), while full_step_majors rises 5 -> 6 because the extra
      // warm-resolved solve arms the full-step rule for its own first major.
      // Majors (11) and the step count are unchanged.
      //
      // RE-MEASURED AGAIN, Phase-5 Task 6 (the ratio-tested predictor path):
      // 11 majors / 93 minors / 10 factorizations -> 7 / 18 / 6, with
      // full_step_majors 6 -> 2. Same shape as the F3n1000 row above -- this
      // family is the same ceiling crossing at n = 200 with dp pinned at 0.5,
      // so it overshot the same way and is repaired the same way.
      {5, 7, 18, 6, 0, 2, 1, 4, 0, 4, 18, 0},
      {5, 15, 39, 11, 4, 10, 1, 0, 4, 0, 39, 0},
      {5, 33, 98, 33, 0, 0, 5, 0, 0, 0, 98, 0}}},
};

// Shared by check_pinned_corpus_totals() (below) and the F7 scale cell's own
// pin check -- both compare a measured CellStats against a hand-transcribed
// Pin on the same twelve counters. Extracted per review M-5
// (task-5-review.md, fix round 1): before this, both call sites carried
// their own copy of the same twelve EXPECT_EQs, so a future Pin field
// addition had two places to update; now it has one. `table` is the
// caller's own table-dump string (corpus_table() or f7_scale_table()),
// computed once by the caller rather than once per assertion.
void check_pin(const CellStats &c, const Pin &p, const std::string &table) {
    EXPECT_EQ(c.steps, static_cast<std::size_t>(p.steps)) << table;
    EXPECT_EQ(c.majors, p.majors) << table;
    EXPECT_EQ(c.minors, p.minors) << table;
    EXPECT_EQ(c.factorizations, p.factorizations) << table;
    EXPECT_EQ(c.fact_saved, p.fact_saved) << table;
    EXPECT_EQ(c.full_step_majors, p.full_step_majors) << table;
    EXPECT_EQ(c.n_cold, p.n_cold) << table;
    EXPECT_EQ(c.n_warm, p.n_warm) << table;
    EXPECT_EQ(c.n_hot, p.n_hot) << table;
    EXPECT_EQ(c.predictor_calls, p.predictor_calls) << table;
    EXPECT_EQ(c.border_refine, p.border_refine) << table;
    EXPECT_EQ(c.elastic_activations, p.elastic_activations) << table;
}

void check_pinned_corpus_totals() {
    for (const FamilyPin &fp : kPins) {
        const FamilyCells &f = family(fp.name);
        for (int a = 0; a < kArmCount; ++a) {
            const CellStats &c = f.cell[0][a];
            const Pin &p = fp.arm[a];
            SCOPED_TRACE(fmt::format("{} / {}", fp.name, arm_name(a)));
            check_pin(c, p, corpus_table());
            // NOTHING IN THIS CORPUS REACHES THE SECOND-ORDER CORRECTION OR
            // THE ELIMINATION-PATH EQP REFINEMENT, and that is pinned so a
            // future change routing work through either says so here instead
            // of absorbing it into the aggregate counts. (eqp_refine_steps
            // reads 0 corpus-wide for a structural reason as well as an
            // empirical one: Task 0's R-1 removed the refinement loop that
            // incremented it. border_refine_steps, the border-mode
            // counterpart, is very much alive -- it tracks QP minors closely
            // -- and is pinned above rather than at zero.)
            EXPECT_EQ(c.soc_steps, 0);
            EXPECT_EQ(c.soc_applied, 0);
            EXPECT_EQ(c.eqp_refine, 0);
            // NOR DOES ANYTHING HERE ENTER THE RESTORATION PHASE. The
            // ELASTIC TIER, on the other hand, IS reached -- but only on
            // F2far, whose (4, -4) start is outside the unit disk, so the
            // first linearized subproblem there has no feasible point inside
            // the trust region and the tier engages exactly as designed. Both
            // are read off `counters` rather than off a flat field, because
            // SqpSolveRecord has no flat copy of either. elastic_activations
            // itself is checked inside check_pin() above, against p.
            EXPECT_EQ(c.restoration_iters, 0);
        }
    }
}

// =====================================================================
// THE ONE CORPUS TEST -- see THE RUNTIME BUDGET note for why it is one.
// =====================================================================
TEST(WarmStartBattery, Corpus) {
    {
        SCOPED_TRACE("no status failures");
        check_no_status_failures();
    }
    {
        SCOPED_TRACE("ledger agrees with the driver's own totals");
        check_ledger_agrees_with_the_driver();
    }
    {
        SCOPED_TRACE("cold first, then warm -- the hand-off chain");
        check_cold_first_then_warm();
    }
    {
        SCOPED_TRACE("the grids the ratios are computed across");
        check_grids_are_what_the_ratios_assume();
    }
    {
        SCOPED_TRACE("warm never costs more than cold");
        check_warm_never_costs_more_than_cold();
    }
    {
        SCOPED_TRACE("the predictor never costs more majors");
        check_predictor_never_costs_more_majors();
    }
    {
        SCOPED_TRACE("the predictor no longer trades majors for minors");
        check_predictor_no_longer_trades_majors_for_minors();
    }
    {
        SCOPED_TRACE("the predictor never degrades");
        check_predictor_never_degrades();
    }
    {
        SCOPED_TRACE("hot steps skip factorizations");
        check_hot_steps_skip_factorizations();
    }
    {
        SCOPED_TRACE("full-step-first is observationally neutral");
        check_full_step_is_neutral();
    }
    {
        SCOPED_TRACE("the pinned corpus totals");
        check_pinned_corpus_totals();
    }
    // THE EVIDENCE ITSELF. Emitted unconditionally, because this file is a
    // benchmark battery and the table IS its product: the results note quotes
    // a machine-produced dump rather than a transcribed one, and a
    // re-measurement on another backend starts by reading this.
    std::cout << corpus_table() << std::flush;
}

// ---------------------------------------------------------------------
// O-1's PIN, isolated from the sweep so it is a claim about the DRIVER rather
// than about a trajectory. It runs no part of the grid, so it is cheap enough
// to keep as its own test.
//
// WHAT IT USED TO PIN (Task 13, the defect). sqp_driver.h's make_warm_start
// ended with
//
//     w.structure_hash = qp_built ? detail::structural_hash(qp) : 0;
//
// and `qp_built` is false on a solve that CONVERGED IMMEDIATELY (the
// convergence test fires before the first subproblem is built, so
// major_iters == 0 -- sqp_types.h). The WarmStart such a solve returned was
// `valid` but carried structure_hash == 0, which the ingest rule reads as
// warm_start.h's hash-0 sentinel (then documented as "never computed"; since
// the repair, "no model was seen") and degrades to kCold -- so a
// zero-major solve produced a hand-off the very next solve could not use, and
// the better the predictor got the more often the chain broke. This test was
// written to make that defect EXECUTABLE evidence rather than prose.
//
// WHAT IT PINS NOW (Phase-5 Task 0, repair A). make_warm_start PROBES the
// model's structure at the exit point when no subproblem was ever built --
// the same build_subproblem/structural_hash machinery the ordinary path uses,
// at the same recipe (zero multipliers) the INGEST side's own probe uses -- so
// a zero-major exit emits the same hash any other exit on that model does and
// the chain survives. The two halves below are both load-bearing: the hash
// must be non-zero (or the ingest rejects it) AND it must equal the one an
// ordinary exit computes (or the ingest's own probe would not match it).
// ---------------------------------------------------------------------
TEST(WarmStartBattery, ZeroMajorStepKeepsTheWarmStartChain) {
    F1BoxQp model(0.5);
    SqpDriver driver(battery_options(StartLevel::kWarm, true));

    // Solve 1: an ordinary cold solve. It builds a subproblem, so its
    // hand-off carries a real structure hash.
    const SqpSolution s1 = driver.solve(model, model.start_point());
    ASSERT_EQ(s1.status, SqpStatus::kOptimal);
    ASSERT_EQ(s1.counters.major_iters, 1) << "F1 is a QP: one subproblem is the whole problem";
    ASSERT_NE(s1.warm_start.structure_hash, 0u);

    // Solve 2: re-solve at the SAME p from that hand-off. It ingests warm and
    // converges before building anything.
    const SqpSolution s2 = driver.solve(model, s1.warm_start.x, s1.warm_start);
    ASSERT_EQ(s2.status, SqpStatus::kOptimal);
    ASSERT_EQ(s2.counters.start_level_used, StartLevel::kWarm);
    ASSERT_EQ(s2.counters.major_iters, 0) << "already at x*(p): the convergence test fires first";

    // THE REPAIR: solve 2 built nothing, and its hand-off is still hash-valid,
    // with the SAME hash the subproblem-building solve 1 emitted.
    EXPECT_NE(s2.warm_start.structure_hash, 0u)
        << "make_warm_start probes the model when qp_built is false -- see this test's own note";
    EXPECT_EQ(s2.warm_start.structure_hash, s1.warm_start.structure_hash)
        << "the probe hashes the SAME model's H/Ae/Ai pattern, so it cannot differ from the "
           "hash a built subproblem produces";
    EXPECT_TRUE(s2.warm_start.valid);

    // ...so solve 3, offered it, resolves warm -- nothing about the model, the
    // point or the options changed, and now nothing about the hand-off does
    // either. (kWarm exactly, not merely >= kWarm: battery_options caps the
    // level there.)
    const SqpSolution s3 = driver.solve(model, s2.warm_start.x, s2.warm_start);
    EXPECT_EQ(s3.counters.start_level_used, StartLevel::kWarm)
        << "THE PIN: a zero-major solve's hand-off is accepted by the very next solve";
    EXPECT_EQ(s3.status, SqpStatus::kOptimal);
    EXPECT_EQ(s3.counters.major_iters, 0) << "still at x*(p), and now it starts from there";
}

// =====================================================================
// PHASE-5 TASK 5 -- WARM-START ECONOMICS AT SCALE, ONE F7 CELL AT n = 10^4.
//
// The corpus above is F1-F3 (n <= 1000); this repeats the same measurement
// -- the three-figure decomposition of docs/notes/2026-07-30-warm-start-
// battery-results.md sec 2 ((a) matched-grid per-solve, (b) traversal, (c)
// end-to-end), plus the hot arm and the pinned corpus-totals discipline --
// on F7CollocationChain (tests/support/scale_problems.h) at N = 2000 nodes,
// ns = 3, nc = 2, i.e. n = N*(ns+nc) = 10000. See
// docs/notes/2026-07-31-warm-start-at-scale.md for the full story, including
// the n = 10^5 and wide-crossing measurements this one cell deliberately does
// NOT attempt (see next paragraph).
//
// WHY p: 0.3 -> 0.54 AND NOT THE FAMILY'S OWN 0.3 -> 0.9 DEFAULT (bench's
// F7 sweep). p_activation() = 0.5*R = 0.5, so this range crosses the
// activation threshold -- the same qualitative event F3's own p: 0.25 -> 0.75
// crosses at p_act = 0.5 -- while staying below a REAL, MEASURED WALL: at
// N = 2000, a COLD start (the family's own flat start_point()) into the
// active window past roughly p = 0.548 fails kNumericalError under the
// library's THEN-DEFAULT QpOptions::max_iter = 500 (the last major's QP subproblem
// never gets under tolerance in 500 minors and the driver reports
// kNumericalError rather than absorbing it with another major) -- measured
// at p = 0.548..0.9 in steps of 0.005-0.05, EVERY one of which failed except
// an isolated p = 0.549 (a size-local exception, not a monotone boundary; see
// the note for the full scan and its correlation, partial and imperfect,
// with F7CollocationChain::junction_margin(p)).
//
// MARKED CORRECTION, PHASE-6 TASK 4 (M6): that 500 is no longer the library
// default -- QpOptions::max_iter now defaults to the size-derived sentinel
// (types.h), which at N = 2000 resolves to max(500, 5 * (10000 + 2000 + 4000))
// = 80000. The WALL described above was therefore a property of the old fixed
// cap, and this cell's own range choice (p: 0.3 -> 0.54) is more conservative
// than it now needs to be. It is deliberately NOT widened here: the cell's
// pinned counters are Phase-4 contract, Grant's Mac pass reproduced them
// bit-for-bit on Accelerate, and re-deriving them to chase a boundary this
// note already says is too sensitive to pin would trade a stable regression
// net for nothing. The finding belongs to docs/notes/2026-08-03-crash-basis.md
// Sec. 6, not here.
//
// That regime, and the fact
// that the CONTINUATION-DRIVEN warm arm hits the SAME wall on its own naive
// first proposal into it and then RECOVERS via ContinuationOptions' ordinary
// shrink-and-retry rule (paying several full-cost failed attempts along the
// way -- this is NOT a warm-start-specific mechanism, it is continuation.h's
// generic step-length control, seeded from the last good WARM hand-off), is
// this task's headline finding and is characterized at length in the note
// rather than in a pinned, necessarily-green regression test: a cell that
// must stay green forever is the wrong place to pin a boundary this
// sensitive to unrelated changes (float rounding, a compiler flip) moving it
// by one grid point. This cell therefore measures the CLEAN economics
// question (does warm cost less than cold at the SAME parameter values, at
// n = 10^4) on a range picked to answer it without depending on that wall's
// exact location, per this task's scope discipline (no engine changes, cost
// pathologies are findings to measure and record, not fix, and NOT to pin a
// regression test on their exact boundary).
//
// THE STEP SIZE IS A FIXED, SMALL dp (0.03, F3stress's own technique: dp_init
// = dp_max, target_majors = 0 so it never grows), NOT the corpus's usual
// growing ContinuationOptions{}. A SECOND, GENUINELY SURPRISING finding is why:
// measured with the ordinary growing schedule (dp_init = 0.1, doubling after
// every cheap step), the whole crossing from p0 to p1 = 0.54 happens in a
// SINGLE step of size 0.14 straight from the empty window -- and that one
// step costs the warm arm EXACTLY as many majors and factorizations as the
// matched cold baseline (5 = 5 both, a TIE with no factorization saving at
// all, which is a WEAKER tie than F1's in the F1-F3 corpus). The reason is
// structural, not a bug: neither start has ANY active path row to seed from,
// so the whole cost of that step is re-identifying a ~700-row active set from
// nothing, and a flat generic point and a warm point from an equally-empty
// neighbour are equally uninformative about WHICH rows that will be. Taking
// the SAME crossing in ten small steps instead lets each step's warm start
// carry forward an active-set guess that is only slightly stale, and the
// saving reappears (see the pinned numbers below). So on this family, AT
// THIS SCALE, whether warm-starting pays anything across an activation
// threshold is not just a question of cold-vs-warm -- it depends on how
// finely continuation's OWN step-length schedule resolves the crossing, which
// is a policy this file does not own (continuation.h's grow/target_majors)
// and is reported as a finding, not tuned around.
//
// THE PREDICTOR ARM IS DELIBERATELY NOT MEASURED IN THIS CELL -- a second,
// independent wall, sharper than the first. predict()'s fix-relax loop
// (predictor.h) re-derives the active set by adding/relaxing ROWS one at a
// time against a Schur complement that grows with every row, so its cost is
// governed by HOW MANY rows change activity between the seed and the target,
// not by ||dp||. F7's activation geometry makes that count explode
// independently of step size: T(p) = arcsin(R/p - 1)/pi (this family's
// header, F7-JCT), and arcsin's derivative diverges as its argument
// approaches 1, which is exactly what happens as p -> p_activation()+ -- so
// ANY step that starts at or below p_activation() and lands above it crosses
// a near-vertical tangent in active-row count, however small the step is in
// p. MEASURED (a standalone probe against this same N = 2000 model, library
// SqpOptions defaults, single predict() call, reverted, not committed):
// jumping the FULL empty-to-wide-window range (p: 0.4 -> 0.54, ~700 rows)
// did not return a single predict() call within a 900 s bound; narrowing the
// step to BARELY cross the threshold (p: 0.49 -> 0.51, the smallest crossing
// this family's own discretization can express before hitting the same
// singular derivative) still cost 51.6 s for ONE predict() call, against
// 0.06 s for the matched COLD solve and 0.9 s for the matched PLAIN WARM
// solve at the same two points -- i.e. shrinking the step by 7x (0.14 -> 0.02)
// bought roughly nothing, which is the signature of a cost governed by a
// geometric threshold rather than by step length. This is real, first data
// on the Task-9-P4 carry (docs/superpowers/plans/2026-07-30-scale.md) and is
// reported at length in the note; it is NOT safe to exercise inside a
// per-commit assertion, so the predictor is measured here by its ABSENCE --
// cell[0][kArmPred] and cell[0][kArmColdPred] stay unmeasured, and every
// existing `if (!c.measured)` guard already treats that correctly.
//
// battery_options() -- this file's one shared SqpOptions apart from the two
// swept fields -- is used UNCHANGED (default QpOptions: max_iter 500,
// schur_cap 128, border mode), so this cell is measured at the exact solver
// CONFIGURATION the F1-F3 corpus above is. IDENTICAL CONFIGURATION IS NOT
// IDENTICAL GRID (fix round 1 correction, task-5-review.md C-1): this cell's
// own 0.75x/0.81x majors/factorizations ratio against ITS OWN matched cold
// arm is directly comparable to the F1-F3 corpus's per-family ratios in that
// narrow sense, but it must NOT be read as "the Phase-4 headline at n=1e4" --
// F7's grid composition (8 of its 10 points are empty-window, where cold is
// already at its floor) sets this ratio near 1 regardless of n, which is the
// opposite of a decay-with-scale reading. See the note's Sec 3.2 for the
// matched-grid controls (F7 at N=100/1e3/1e4 nodes, F3 at n=1e3/1e4) that
// settle this.
//
// full_step is fixed ON. This task does not re-litigate Phase-4's full-step
// neutrality finding (results note sec 6, corroborated corpus-wide); adding
// the OFF copy here would double this cell's cost for a question already
// settled.
// =====================================================================

// Reuses FamilyCells/CellStats/Pin (this file's own corpus machinery) rather
// than inventing parallel types -- cell[1] (full_step OFF) AND the predictor
// arms (see the banner above) are deliberately left entirely unmeasured
// (CellStats::measured stays false), which every existing `if (!c.measured)`
// guard already treats correctly.
const FamilyCells &f7_scale_cell() {
    static const FamilyCells fc = [] {
        FamilyCells c;
        c.name = "F7n10000";
        c.n = 10000;
        const auto make = [] {
            return F7CollocationChain(/*nodes=*/2000, /*states=*/3,
                                      /*controls=*/2, /*p0=*/0.3,
                                      /*radius=*/1.0);
        };
        const Vec p0 = p_vec(0.3);
        const Vec p1 = p_vec(0.54);
        ContinuationOptions plain;
        plain.dp_init = 0.03;
        plain.dp_max = 0.03;
        plain.target_majors = 0;
        std::vector<Vec> warm_grid;
        {
            auto m = make();
            c.cell[0][kArmWarm] = run_sweep(m, p0, p1, StartLevel::kWarm, /*use_predictor=*/false,
                                            /*full_step=*/true, plain, &warm_grid);
        }
        {
            auto m = make();
            c.cell[0][kArmHot] = run_sweep(m, p0, p1, StartLevel::kHot, /*use_predictor=*/false,
                                           /*full_step=*/true, plain, nullptr);
        }
        {
            auto m = make();
            c.cell[0][kArmCold] = run_cold_grid(m, warm_grid, /*full_step=*/true);
        }
        return c;
    }();
    return fc;
}

std::string f7_scale_table() {
    const FamilyCells &f = f7_scale_cell();
    std::string s = "\nF7n10000 (N = 2000 nodes, n = 10000), p: 0.3 -> 0.54, full_step = on "
                    "(predictor arms not measured -- see this file's own banner)\n";
    s += fmt::format("{:<10} {:<6} {:<7} {:<7} {:<6} {:<6} {:<10} {:<9}\n", "arm", "steps",
                     "majors", "minors", "fact", "saved", "lvl c/w/h", "pred c/d");
    for (int a = 0; a < kArmCount; ++a) {
        const CellStats &c = f.cell[0][a];
        if (!c.measured) {
            s += fmt::format("{:<10} (not run)\n", arm_name(a));
            continue;
        }
        s += fmt::format("{:<10} {:<6} {:<7} {:<7} {:<6} {:<6} {:<10} {:<9}\n", arm_name(a),
                         c.steps, c.majors, c.minors, c.factorizations, c.fact_saved,
                         fmt::format("{}/{}/{}", c.n_cold, c.n_warm, c.n_hot),
                         fmt::format("{}/{}", c.predictor_calls, c.predictor_degradations));
    }
    return s;
}

// SUITE: ScaleF7Slow, not WarmStartBattery -- tests/CMakeLists.txt's
// TEST_FILTER "-ScaleF7Slow.*" excludes it from the per-commit `ctest` run in
// BOTH builds. MEASURED: 5.6 s Release / 45.5 s Debug (counters bit-identical
// between the two, as everywhere else in this file), which would add ~38% to
// the Debug suite's ~120 s per-commit budget (tests/CMakeLists.txt's own
// figure) for one cell -- exactly the case that banner's own "anything added
// here must carry the same justification" asks for. Run directly with
// `--gtest_filter='ScaleF7Slow.F7AtTenThousandVariablesSweep'`, or as part of
// the phase-gate's own `--gtest_filter='ScaleF7Slow.*'` sweep.
TEST(ScaleF7Slow, F7AtTenThousandVariablesSweep) {
    const FamilyCells &f = f7_scale_cell();
    const CellStats &cold = f.cell[0][kArmCold];
    const CellStats &warm = f.cell[0][kArmWarm];
    const CellStats &hot = f.cell[0][kArmHot];

    // ---- (1) nothing failed anywhere, on the three arms this cell measures ----
    for (int a : {kArmCold, kArmWarm, kArmHot}) {
        const CellStats &c = f.cell[0][a];
        SCOPED_TRACE(arm_name(a));
        ASSERT_TRUE(c.measured);
        EXPECT_TRUE(c.reached_p1) << f7_scale_table();
        EXPECT_EQ(c.status_failures, 0) << f7_scale_table();
    }
    // THE PREDICTOR ARMS ARE NOT RUN HERE -- see this file's own banner above
    // f7_scale_cell().
    EXPECT_FALSE(f.cell[0][kArmPred].measured) << f7_scale_table();
    EXPECT_FALSE(f.cell[0][kArmColdPred].measured) << f7_scale_table();

    // ---- (2) O-7: the grids the ratios below assume ----
    EXPECT_EQ(cold.pgrid, warm.pgrid)
        << "the cold baseline must re-solve the WARM arm's own parameter values"
        << f7_scale_table();
    EXPECT_EQ(hot.pgrid, warm.pgrid) << f7_scale_table();

    // ---- (3) the headline: warm never costs more than cold, matched grid ----
    EXPECT_LE(warm.majors, cold.majors) << f7_scale_table();
    EXPECT_LE(warm.factorizations, cold.factorizations) << f7_scale_table();
    EXPECT_LT(warm.factorizations, cold.factorizations)
        << "the sweep's one engine keeps K0 across steps; the fresh-driver cold baseline cannot"
        << f7_scale_table();

    // ---- (4) the hot arm actually resolved kHot at least once. UNLIKE the
    // F1-F3 corpus's check_hot_steps_skip_factorizations() (this file's own
    // section (7)), this cell's ONE kHot resolution is a ZERO-MAJOR step (the
    // point handed forward already satisfied tolerance -- major_iters == 0,
    // so there is no "first subproblem" to check a factorization count
    // against; ledger.h's factorizations_saved is defined purely from
    // start_level_used and is exactly as meaningful here). A step-by-step
    // "first subproblem paid no factorization" check is therefore not
    // reproduced for this family/range; see the note for a discussion of
    // whether F7's Ji (which carries x's own VALUES, not merely its pattern --
    // this family's own header) makes a NON-zero-major kHot resolution
    // structurally rare, mirroring F2/F2far's kHot-unreachable finding in the
    // F1-F3 corpus (check_hot_steps_skip_factorizations()'s own note).
    ASSERT_EQ(hot.qp_records, hot.majors)
        << "the record-per-major partition needs every subproblem to be a major"
        << f7_scale_table();
    ASSERT_GT(hot.n_hot, 0) << "no solve on the hot arm resolved kHot at all" << f7_scale_table();
    EXPECT_EQ(hot.fact_saved, hot.n_hot) << f7_scale_table();

    // ---- (5) the pinned counters, as observed. MEASURED ON MKL PARDISO,
    // clang++, Release and Debug (identical), same column order as this
    // file's own Pin/kPins (steps, majors, minors, factorizations,
    // fact_saved, full_step_majors, n_cold, n_warm, n_hot, predictor_calls,
    // border_refine, elastic_activations). ----
    const FamilyPin want{"F7n10000",
                         {{10, 16, 160, 16, 0, 0, 10, 0, 0, 0, 160, 0},
                          {10, 12, 98, 13, 0, 11, 1, 9, 0, 0, 98, 0},
                          // kArmPred, kArmColdPred: NOT MEASURED (this file's own banner above
                          // f7_scale_cell()) -- the row exists so FamilyPin's shape matches
                          // every other entry in kPins, and is never read (the pin-check loop
                          // below iterates {kArmCold, kArmWarm, kArmHot} only).
                          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
                          {10, 12, 98, 13, 1, 11, 1, 8, 1, 0, 98, 0},
                          {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}};
    for (int a : {kArmCold, kArmWarm, kArmHot}) {
        const CellStats &c = f.cell[0][a];
        const Pin &p = want.arm[a];
        SCOPED_TRACE(arm_name(a));
        ASSERT_TRUE(c.measured) << f7_scale_table();
        check_pin(c, p, f7_scale_table());
    }

    std::cout << f7_scale_table() << std::flush;
}

// =====================================================================
// PHASE-6 TASK 1 -- PROPOSAL-FAILURE ECONOMICS, ONE F7 CELL AT
// N = 700 NODES (n = 3500 VARIABLES; F7 carries ns + nc = 5 variables per
// node, so every size below is stated in BOTH units).
//
// WHAT THIS CELL IS FOR. Every other cell in this file measures what a warm
// start buys on a sweep that WORKS. This one measures what a sweep pays for
// the proposals it throws away, because that -- not identification -- is
// where the nx = 10^5 head-to-head lost:
// docs/notes/2026-08-01-psiopt-first-comparison.md section 4.5(b) attributes
// 84 % of that sweep's QP minors and 86 % of its wall to five FAILED
// continuation proposals, each paid at full price. The repair is
// ContinuationOptions::probe_budget (continuation.h's RETRY ECONOMICS note),
// and this cell is its pinned regression fixture at a size a test can run.
//
// THE FIXTURE. F7 at N = 700 nodes (n = 3500 variables), p: 0.51 -> 0.95 --
// the Task-9 head-to-head's own parameter range -- with dp_init = 0.44, i.e.
// the whole range proposed as ONE step. That is deliberately too long: the
// first two proposals fail in the WIDE-WINDOW regime (the same regime, and
// the same cap-pinned kNumericalError shape, as all five nx = 10^5 failures),
// and the controller shrinks its way down to a step that gets through.
//
// FIX ROUND 1 RESIZED IT, from N = 2000 nodes (n = 10^4) to N = 700, on
// Grant's ruling. The first version could not be run in DEBUG at all -- it
// had spent over an hour of CPU without finishing and was killed -- so its
// pins were Release-only, in a file whose entire discipline is that every
// counter is bit-identical between the two builds, and the phase gate's
// documented `--gtest_filter='ScaleF7Slow.*'` sweep would have started it in
// Debug and had to kill it. At N = 700 the cell keeps the wide-window failure
// shape and the multi-abandonment structure (two of each) and costs 34.8 s
// Release / 613.3 s Debug -- runnable, and PINNED FROM BOTH BUILDS below. The
// nx = 10^5 economics stay where a 36-minute measurement belongs: in
// docs/notes/2026-08-02-controller-retry-economics.md, with committed
// artifacts under prototypes/psiopt_bridge/results/.
//
// THE TWO ARMS ARE THE SAME SWEEP with the probe budget OFF (which, together
// with the growth suspension held off, IS the pre-Task-1 controller) and at
// its default. Holding the suspension off in both arms is what makes this a
// one-lever cell; tests/test_continuation.cpp's
// GrowthSuspensionSkipsTheStepAfterAFailure is the other lever's fixture.
//
// WHAT THIS CELL DOES *NOT* EXERCISE, stated so it is not read as coverage it
// does not have: the budget's RATIO term. Every converged step here costs
// 15-80 minors, so max(2 * last_good, kProbeBudgetFloor) is the FLOOR at
// every step -- as it is at every other in-suite size (continuation.h's
// kProbeBudgetFloor note, and the note's section 6.1 on the surviving M8/M9
// mutations).
// =====================================================================

struct FailureEconomics {
    CellStats off; // probe_budget = 0: the pre-Task-1 controller
    CellStats on;  // probe_budget = ContinuationOptions' default
};

const FailureEconomics &f7_failure_economics_cell() {
    static const FailureEconomics fe = [] {
        FailureEconomics e;
        const auto make = [] {
            return F7CollocationChain(/*nodes=*/700, /*states=*/3, /*controls=*/2, /*p0=*/0.51,
                                      /*radius=*/1.0);
        };
        const Vec p0 = p_vec(0.51);
        const Vec p1 = p_vec(0.95);
        ContinuationOptions base;
        base.dp_init = 0.44;
        base.dp_max = 0.44;
        base.suspend_growth_after_failure = false; // one lever at a time
        base.probe_budget = 0;
        // MARKED CORRECTION, PHASE-6 TASK 4 (M6). `cap500` is NEW: it restores,
        // EXPLICITLY, the QP minor cap that was the library default when this
        // cell was pinned, through the escape hatch M6 was required to keep
        // (types.h's QpOptions::max_iter precedence rule).
        //
        // WHY, and it is a RESULT rather than a maintenance chore. Under M6's
        // size-derived default this cell (N = 700 nodes, base = n + mi +
        // #bounded = 3500 + 700 + 1400 = 5600, cap = 28000) STOPS FAILING: the
        // probe_budget = 0 arm crosses p 0.51 -> 0.95 in ONE 0.44 step, 2 steps
        // total, 6 majors, 19031 minors, ZERO status failures -- so a fixture
        // whose entire subject is "what a FAILED proposal costs" would have
        // been measuring nothing. Restoring the cap keeps the twelve pinned
        // counters below (which Grant's Mac pass reproduced bit-for-bit on
        // Accelerate -- docs/notes/2026-08-01-accelerate-register-3.md, item
        // D22) testing exactly the trajectory they were derived on.
        //
        // AND THE M6 READING ITSELF IS NOT LOST -- it is the uncomfortable half
        // of docs/notes/2026-08-03-crash-basis.md Sec. 6.3: 2460 capped minors
        // against 19031 size-derived ones on the same sweep, because the fixed
        // cap was acting as an implicit "this proposal is too hard, back off"
        // signal that the continuation controller has no other source for.
        // That is recorded and priced in the note; it is not pinned here,
        // because this cell already costs ~35 s (Release) / ~613 s (Debug) and
        // a third arm would not earn its place beside the smaller version of
        // the same measurement in tests/test_continuation.cpp's arms D and E.
        const auto cap500 = [](SqpOptions o) {
            o.qp.max_iter = 500;
            return o;
        };
        {
            auto m = make();
            e.off = run_sweep(m, p0, p1, StartLevel::kWarm, /*use_predictor=*/false,
                              /*full_step=*/true, base, nullptr, cap500);
        }
        base.probe_budget = ContinuationOptions{}.probe_budget; // the shipped default
        {
            auto m = make();
            e.on = run_sweep(m, p0, p1, StartLevel::kWarm, /*use_predictor=*/false,
                             /*full_step=*/true, base, nullptr, cap500);
        }
        return e;
    }();
    return fe;
}

std::string f7_failure_economics_table() {
    const FailureEconomics &e = f7_failure_economics_cell();
    std::string s = "\nF7 failure economics (N = 700 nodes, n = 3500), p: 0.51 -> 0.95, "
                    "dp_init = 0.44\n";
    s += fmt::format("{:<14} {:<6} {:<7} {:<7} {:<6} {:<12} {:<9} {:<10}\n", "probe_budget",
                     "steps", "majors", "minors", "fact", "failed min.", "abandon", "full-cost");
    for (const CellStats *c : {&e.off, &e.on}) {
        s += fmt::format("{:<14} {:<6} {:<7} {:<7} {:<6} {:<12} {:<9} {:<10}\n",
                         c == &e.off ? 0 : ContinuationOptions{}.probe_budget, c->steps, c->majors,
                         c->minors, c->factorizations, c->failed_minors, c->proposals_abandoned,
                         c->proposals_full_cost);
    }
    return s;
}

// SUITE: ScaleF7Slow, for the same reason F7AtTenThousandVariablesSweep is --
// tests/CMakeLists.txt's TEST_FILTER excludes it from the per-commit ctest in
// both builds. MEASURED (fix round 1, at N = 700 nodes / n = 3500 variables):
// 34.8 s Release and 613.3 s Debug on one otherwise-idle run of this machine;
// a later Release run of the same cell measured 22.4 s. WALL IS INFORMATIONAL
// HERE, as everywhere in this project -- these are order-of-magnitude figures
// for whoever budgets the phase gate's `--gtest_filter='ScaleF7Slow.*'` sweep,
// not pinned values, and only the COUNTERS below are asserted (identically in
// both builds, which is the property fix round 1 resized this cell to
// restore). The probe_budget = 0 arm is about two thirds of the total in
// either build -- the cost this task exists to stop paying, and the reason
// this cell stays out of the per-commit run.
TEST(ScaleF7Slow, F7ProposalFailureEconomics) {
    const FailureEconomics &e = f7_failure_economics_cell();
    const CellStats &off = e.off;
    const CellStats &on = e.on;
    const std::string table = f7_failure_economics_table();

    // ---- (1) both arms follow the path to p1, and visit the SAME parameter
    // values in the same order. The probe budget is a COST rule: it must not
    // move a single proposal. ----
    ASSERT_TRUE(off.reached_p1) << table;
    ASSERT_TRUE(on.reached_p1) << table;
    EXPECT_EQ(on.pgrid, off.pgrid) << "abandonment moved a proposal" << table;

    // ---- (2) the fixture is a FAILURE fixture, which is what makes it worth
    // running: without failures it would be measuring nothing. ----
    ASSERT_GT(off.status_failures, 0) << table;
    EXPECT_EQ(off.status_failures, on.status_failures) << table;
    EXPECT_EQ(off.proposals_abandoned, 0) << "nothing is abandoned with the budget off" << table;
    EXPECT_EQ(on.proposals_full_cost, 0) << "every failure should have been caught" << table;
    EXPECT_EQ(on.proposals_abandoned, off.proposals_full_cost) << table;

    // ---- (3) THE HEADLINE, in its backend-independent form ----
    EXPECT_LT(on.failed_minors, off.failed_minors) << table;
    EXPECT_LT(on.minors, off.minors) << table;
    EXPECT_LT(on.factorizations, off.factorizations) << table;
    EXPECT_LT(on.majors, off.majors) << table;

    // ---- (4) PINNED, as observed. MEASURED ON MKL PARDISO, clang++, in
    // RELEASE AND DEBUG, identical (which is this file's own discipline and
    // the reason fix round 1 resized the cell -- see the banner). Read the
    // numbers off the table on a re-measurement. ----
    EXPECT_EQ(off.steps, 6u) << table;
    EXPECT_EQ(off.majors, 17) << table;
    EXPECT_EQ(off.minors, 2460) << table;
    EXPECT_EQ(off.factorizations, 1513) << table;
    EXPECT_EQ(off.failed_minors, 2319) << table; // 94.3 % of 2460
    EXPECT_EQ(off.proposals_full_cost, 2) << table;

    EXPECT_EQ(on.steps, 6u) << table;
    EXPECT_EQ(on.majors, 14) << table;
    EXPECT_EQ(on.minors, 960) << table;
    EXPECT_EQ(on.factorizations, 387) << table;
    EXPECT_EQ(on.failed_minors, 819) << table; // 85.3 % of 960
    EXPECT_EQ(on.proposals_abandoned, 2) << table;

    // ---- (5) AND THE CONVERGED WORK IS UNTOUCHED: what the budget bought is
    // exactly the failed proposals' surplus, so the two arms' converged
    // minors agree. (off: 2460 - 2319 = 141; on: 960 - 819 = 141.) ----
    EXPECT_EQ(on.minors - on.failed_minors, off.minors - off.failed_minors)
        << "a converged step's cost moved" << table;

    std::cout << table << std::flush;
}

// =====================================================================
// PHASE-6 TASK 5 -- THE CROSSOVER-CHAIN CELL.
//
// THE QUESTION. StartLevel::kSeeded exists so that an object with no usable
// structural hash can still hand its VALUES to a solve. The interior-point
// crossover (warm_start.h's from_interior_point) is one of the two producers it
// was built for, and the Phase-5 head-to-head
// (docs/notes/2026-08-01-psiopt-first-comparison.md) is where that producer's
// motivating consumer lives: the IPM engine runs to near-KKT on F7, this engine polishes.
// Through Phase 5 the polish could receive nothing but an x0. This cell
// measures what it receives now.
//
// **THE ONE THING THIS CELL CANNOT DO, STATED FIRST BECAUSE IT BOUNDS EVERY
// NUMBER BELOW: IT DOES NOT USE THE IPM ENGINE'S OWN MULTIPLIERS, BECAUSE THE SHIPPED
// BRIDGE CANNOT PRODUCE THEM.** prototypes/psiopt_bridge/run_comparison.py
// reaches the IPM engine through `tychopy.solvers.OptimizationProblem`, which is
// tychopy's only route to the IPM engine for a generic NLP, and whose entire Python
// surface is set_vars / return_vars / add_equal_con / add_inequal_con /
// add_objective (verified against tycho's own
// src/bindings/solvers/optimization_problem_bind.cpp at this task's HEAD).
// `return_vars` returns the PRIMAL VECTOR AND NOTHING ELSE; the IPM binding
// itself exposes settings and run info, never a multiplier vector. So there is
// no way, today, to get a real IPM dual into a WarmStart from Python, and
// the bridge's own `--dump-solution` format carries x alone.
//
// WHAT THIS CELL DOES INSTEAD, and why it is still the right measurement:
//   - THE PROBLEM IS THE BRIDGE'S OWN. F7CollocationChain(N, 3, 2, ...), the
//     exact family and shape the head-to-head measures (bench_scale.cpp's F7
//     arm and generate_f7.py both construct it with states = 3, controls = 2),
//     at N = 100 nodes / nx = 500 variables -- the size of the note's
//     `sweep_n100` row -- and at the bridge's own tolerances (kkt_tol =
//     feas_tol = 1e-8).
//   - THE IP ITERATE IS SYNTHESIZED THE WAY THIS PROJECT ALREADY SYNTHESIZES
//     ONE, by the recipe tests/test_warm_start.cpp's
//     CrossoverRecoversExactSolveHs14 shipped in Phase 4: take a converged
//     reference point, displace x by a small amount, and give each ACTIVE row
//     the barrier residue slack = -mu/lambda that an interior-point method
//     stops at. mu = 1e-8 is the bridge's own IPM `bar_tol` default.
//   - SO WHAT IS MEASURED IS THE CHAIN'S DRIVER-SIDE HALF: given a near-KKT
//     primal-dual point of the shape an IP method hands over, does the seeded
//     ingest buy counted work against feeding the same point as a bare x0?
//     That is the half kSeeded is responsible for. The half it is not -- how
//     good the IPM engine's actual duals are -- is not measurable from here at all,
//     and this cell does not pretend to.
//
// THE FINDING IS CARRIED FORWARD, not swallowed: closing the real chain needs
// either a multiplier accessor on tychopy's OptimizationProblem or a C++-side
// IPM bridge. docs/notes/2026-08-04-kseeded-ingest.md records it as an open
// item.
// =====================================================================
TEST(WarmStartBattery, CrossoverChainOnTheBridgeFamilyBeatsCold) {
    constexpr Index kNodes = 100; // nx = 500, the note's sweep_n100 row
    constexpr double kP = 0.9;
    constexpr double kMu = 1.0e-8; // the bridge's IPM bar_tol default

    const auto make = [] {
        return F7CollocationChain(/*nodes=*/kNodes, /*states=*/3, /*controls=*/2, /*p0=*/kP,
                                  /*radius=*/1.0);
    };

    // ---- the reference: a converged solve, standing in for the point an IP
    // method would have driven to. ----
    F7CollocationChain ref_model = make();
    SqpDriver ref_driver(battery_options(StartLevel::kCold, /*full_step=*/false));
    const SqpSolution ref = ref_driver.solve(ref_model, ref_model.start_point());
    ASSERT_EQ(ref.status, SqpStatus::kOptimal);
    ASSERT_EQ(ref_model.n(), 500) << "the bridge's sweep_n100 shape";
    ASSERT_GT(ref.lambda_i.maxCoeff(), 0.0) << "F7's path window is active at p = 0.9";
    const Index nx = ref_model.n();

    // ---- THE CROSSOVER RUNS ON ITS OWN DEFAULTS, AND THAT IS THE PHASE-7
    // TASK-0 REPAIR SHOWING UP HERE RATHER THAN A FIXTURE SIMPLIFICATION.
    //
    // THROUGH PHASE 6 this fixture set `ip_opts.activity_tol = 1.0e-3` by
    // hand, with a derivation this comment carried at length: the old
    // inference rule was |slack_i(j)| < activity_tol * max(1, |lambda_i(j)|),
    // whose max(1, .) floor makes the test ABSOLUTE for any multiplier below
    // O(1); a collocation multiplier is O(h) by construction (F7's own
    // lambda_i* = h * nu(t_k, p)), so at N = 100 the live prices run 1.41e-4
    // to 8.08e-3 while the barrier residue an IP method stops at is mu/lambda,
    // i.e. 1.24e-6 to 7.11e-5 -- and the shipped default of 1e-6 sat below the
    // residue on EVERY active row here and inferred an EMPTY active set
    // (measured: 0 of 92). 1e-3 was derived, not tuned: above the largest
    // residue (7.11e-5), below the smallest genuine slack on an inactive row
    // (1.43e-2), ~14x clear of both. The gap WIDENED with refinement, since
    // lambda ~ h shrinks while mu does not, and by N = 1600 even the derived
    // 1e-6 default recovered 0 of 1486 rows.
    //
    // PHASE-7 TASK 0 REPLACED THE RULE (warm_start.h's ACTIVITY INFERENCE
    // note; docs/notes/2026-08-06-activity-tol-repair.md): the test is now
    // relative to the hand-off's OWN dual scale, so no per-mesh tolerance has
    // to be derived by a caller at all, and THE OVERRIDE IS DELETED. Every
    // number below is unchanged -- the repaired default reads the same 92-row
    // window this fixture always asserted, which is the byte-identity claim
    // the repair was required to make.
    const IpCrossoverOptions ip_opts;

    // MEASURED (MKL Pardiso, clang++ Release, MKL_NUM_THREADS=1). Four IP
    // displacements, three decades apart, so the benefit is not an artifact of
    // handing the driver a point that was already the answer:
    //
    //   |dx|inf   cold (majors/minors/fact)   seeded          minor ratio
    //   1e-9      1 / 71 / 1                  0 /  0 / 0      -- (free)
    //   1e-7      1 / 48 / 1                  1 /  2 / 1      24.0x
    //   1e-5      2 / 49 / 2                  2 /  4 / 2      12.3x
    //   1e-3      2 / 49 / 2                  2 /  4 / 2      12.3x
    //
    // THE MECHANISM IS MINOR COUNT, NOT MAJORS, and it is the same mechanism
    // the Phase-5 head-to-head named as the one that matters
    // (docs/notes/2026-08-01-psiopt-first-comparison.md): the majors agree at
    // every displacement past the free one, while the QP's walk to the active
    // set collapses from ~48-71 minors to 2-4, because the crossover's own
    // activity inference hands over the 92-row active window that the cold arm
    // has to discover a ratio test at a time. That is precisely what the ingest
    // gap was throwing away on this path.
    struct Row {
        double eps;
        Index cold_majors, cold_minors, cold_fact;
        Index seeded_majors, seeded_minors, seeded_fact;
    };
    const std::vector<Row> expected = {
        {1e-9, 1, 71, 1, 0, 0, 0},
        {1e-7, 1, 48, 1, 1, 2, 1},
        {1e-5, 2, 49, 2, 2, 4, 2},
        {1e-3, 2, 49, 2, 2, 4, 2},
    };

    std::string table =
        fmt::format("crossover chain, F7 N={} nx={} p={} mu={} activity_rel_tol={}\n"
                    "  |dx|inf   arm      level   majors  minors  fact\n",
                    kNodes, nx, kP, kMu, ip_opts.activity_rel_tol);

    for (const Row &want : expected) {
        SCOPED_TRACE(fmt::format("IP displacement {:g}", want.eps));

        // ---- the IP iterate: displaced primal, barrier-residue slacks on the
        // active rows, the model's own cI elsewhere. Deterministic, no RNG. ----
        Vec x_ip = ref.x;
        for (Index i = 0; i < nx; ++i) {
            x_ip(i) += want.eps * std::sin(static_cast<double>(i));
        }
        const Vec ci_at_ip = ref_model.eval_ci(x_ip);
        Vec slack_i = ci_at_ip;
        for (Index j = 0; j < ref_model.mi(); ++j) {
            if (ref.lambda_i(j) > 0.0) {
                slack_i(j) = -kMu / ref.lambda_i(j);
            }
        }
        // F7's control box is INACTIVE at the optimum by construction
        // (|u*| = 0.5 < 1), so both IP bound multipliers are zero there --
        // which is also what the bridge's own mapping produces, since it
        // carries the box as inequality ROWS and those rows price at zero too.
        const Vec z_lower = Vec::Zero(nx);
        const Vec z_upper = Vec::Zero(nx);

        const WarmStart crossover =
            from_interior_point(x_ip, ref.lambda_e, ref.lambda_i, slack_i, z_lower, z_upper,
                                ref_model.lower(), ref_model.upper(), ip_opts);
        ASSERT_TRUE(crossover.valid);
        ASSERT_EQ(crossover.structure_hash, 0u)
            << "no model was hashed -- that is the whole premise";
        ASSERT_EQ(crossover.qp_working_set.active_ineq().size(), 92u)
            << "the crossover infers F7's own 92-row active window, or the seeded arm has no hint";

        // ---- ARM A (kCold): the same point, as a bare x0. This is exactly
        // what the chain could deliver through Phase 5. ----
        F7CollocationChain cold_model = make();
        SqpDriver cold_driver(battery_options(StartLevel::kCold, /*full_step=*/false));
        const SqpSolution cold = cold_driver.solve(cold_model, crossover.x);

        // ---- ARM B (kSeeded): the whole object. ----
        F7CollocationChain seeded_model = make();
        SqpDriver seeded_driver(battery_options(StartLevel::kWarm, /*full_step=*/false));
        const SqpSolution seeded = seeded_driver.solve(seeded_model, crossover.x, crossover);

        table += fmt::format(
            "  {:8g}  cold     {:6}  {:6}  {:6}  {:6}\n"
            "  {:8g}  seeded   {:6}  {:6}  {:6}  {:6}\n",
            want.eps, to_string(cold.counters.start_level_used), cold.counters.major_iters,
            cold.counters.qp_minor_iters, cold.counters.factorizations, want.eps,
            to_string(seeded.counters.start_level_used), seeded.counters.major_iters,
            seeded.counters.qp_minor_iters, seeded.counters.factorizations);

        ASSERT_EQ(cold.status, SqpStatus::kOptimal);
        ASSERT_EQ(seeded.status, SqpStatus::kOptimal);
        EXPECT_EQ(cold.counters.start_level_used, StartLevel::kCold);
        EXPECT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded)
            << "THE PIN: the crossover object reaches the ingest";
        EXPECT_EQ(seeded.counters.n_seeded, 1);
        EXPECT_EQ(seeded.counters.seeded_clamped, 0)
            << "this synthesized IP point carries no wrong-signed price";

        // PINNED BOTH ARMS (MKL-observed; a re-measurement on another backend
        // is a re-measurement, not automatically a defect -- the INEQUALITIES
        // below are what must hold on any backend).
        EXPECT_EQ(cold.counters.major_iters, want.cold_majors);
        EXPECT_EQ(cold.counters.qp_minor_iters, want.cold_minors);
        EXPECT_EQ(cold.counters.factorizations, want.cold_fact);
        EXPECT_EQ(seeded.counters.major_iters, want.seeded_majors);
        EXPECT_EQ(seeded.counters.qp_minor_iters, want.seeded_minors);
        EXPECT_EQ(seeded.counters.factorizations, want.seeded_fact);

        // THE COUNTED BENEFIT, as inequalities that survive a re-measurement.
        EXPECT_LT(seeded.counters.qp_minor_iters, cold.counters.qp_minor_iters / 4)
            << "THE ACCEPTANCE PIN: the crossover chain costs under a quarter of the cold arm's "
               "QP minors at every displacement measured";
        EXPECT_LE(seeded.counters.major_iters, cold.counters.major_iters);
        EXPECT_LE(seeded.counters.factorizations, cold.counters.factorizations);

        // Same answer, so the cheaper arm is not cheaper by being wrong. Both
        // are compared against the REFERENCE solve, not against each other, so
        // a pair that agreed on a wrong point would still fail.
        EXPECT_LT((seeded.x - ref.x).cwiseAbs().maxCoeff(), 1e-5);
        EXPECT_LT((cold.x - ref.x).cwiseAbs().maxCoeff(), 1e-5);
        EXPECT_NEAR(seeded.f, ref.f, 1e-8);
    }

    std::cout << table << std::flush;
}

} // namespace
} // namespace hven::solvers
