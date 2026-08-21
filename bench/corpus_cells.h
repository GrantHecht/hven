#pragma once

// bench/corpus_cells.h — PHASE-7 TASK 1: the replay corpus's cell table and
// engine interface. See docs/notes/2026-08-06-corpus-design.md for the full
// design rationale (tag semantics, the census, the gate definitions verbatim
// from the plan). This header is the MEASURING INSTRUMENT Phase 7's later
// tasks (3-6) build the SSN engine against; no new engine exists yet.
//
// RECIPES, NOT MATRICES (spec section 3). A CorpusCell is a deterministic
// GENERATION SPEC -- family, node count, p-path, start taxonomy, constraint
// window -- replayed through the committed bench generators
// (tests/sqp/support/scale_problems.h's F7CollocationChain, the same family
// bench/bench_scale.cpp and bench/bench_f7_cold.cpp already drive). Nothing
// here stores a matrix or a solved point; every cell is re-derived from the
// family's own analytic surface and the library's ordinary solve path each
// time it is replayed, which is also what makes the determinism invariant
// (tests/test_corpus_cells.cpp) meaningful rather than circular.
//
// THIS IS BENCH-LOCAL, NOT PART OF THE LIBRARY SURFACE -- same standing as
// bench/bench_cli.h and tests/sqp/support/*.h: nothing in include/ or src/
// depends on it, and it is never installed.
//
// =============================================================================
// THE ENGINE INTERFACE (spec section 3)
// =============================================================================
//
// A corpus run is `(cell, engine, config) -> counters`. TASK 6 WIRED THE
// SECOND ENGINE UP: `run_cell` dispatches "walk" and "ssn" through ONE
// implementation (`detail::run_cell_engine`) that differs between them in
// EXACTLY ONE FIELD, `SqpOptions::qp_mode` -- same generators, same starts,
// same tolerances, same budgets, same number of solves. That is what makes the
// two artifacts a comparison rather than two studies. Task 1 shipped "ssn"
// throwing because the enumerator's consumer did not exist yet; Task 3 landed
// the enumerator, Task 5 the driver dispatch, and this task replays the census
// against it.
//
// TASK 6 ALSO ADDED THE MODEL-LEVEL KKT GATE to every row of every engine --
// see CorpusRow's own block and the W1-W5 pre-registration below. The short
// version: `status` is what the DRIVER believes, and a corpus that scores a
// phase on speed must not credit a fast wrong answer.
//
// =============================================================================
// THE FIVE-WAY START TAXONOMY, AND WHAT EACH ONE ACTUALLY RUNS
// =============================================================================
//
// The taxonomy is about HOW THE START WAS BUILT, not about the StartLevel
// (warm_start.h) a solve resolves to -- those are different axes, and a cell
// row's actual resolved level is exactly what a later task's gate reads to
// judge whether a taxonomy earned the level its name promises.
//
//   kNeutralCold      model.start_point() -- the family's own generic start,
//                     fed through the 2-arg solve() (always resolves kCold).
//   kPhysicsInformed  a SMALL, deterministic displacement of the family's own
//                     analytic optimum x*(p) (kPhysicsInformedDisp below),
//                     still through the 2-arg solve() -- so it is cold BY
//                     RESOLUTION but starts in a physically sensible
//                     neighbourhood rather than the generic flat profile.
//   kCorrupted        a genuine WarmStart object (built exactly as
//                     kFullWarm's is, one cold solve at p0 on the SAME
//                     driver) that is then DELIBERATELY DAMAGED before being
//                     fed to the 3-arg solve() at p -- see
//                     detail::corrupt_warm_start below for the exact recipe
//                     and its provenance. Exercises the driver's robustness
//                     to a stale/inconsistent hand-off, which is exactly the
//                     class of input Phase 7's escape-rate gate (G4) prices.
//   kActivityOnly     the interior-point crossover
//                     (warm_start.h's `from_interior_point`): kPhysics-
//                     Informed's OWN primal rollout carrying a SYNTHESIZED
//                     central-path hand-off (duals/slacks at the family's
//                     analytic surface, i.e. an EXACT activity hint), fed
//                     through the 3-arg solve() -- resolves
//                     StartLevel::kSeeded by construction (from_interior_
//                     point's structure_hash is unconditionally 0). Its
//                     matched control is the kPhysicsInformed cell at the
//                     same (N, window): same primal, no hint. See
//                     detail::f7_ip_iterate's own note for why the primal is
//                     NOT x*(p) (fix round 1, I2) and
//                     detail::crossover_mu_for_n's for the mu choice and its
//                     provenance (Task-0 carry).
//   kFullWarm         TWO solves on the SAME driver instance: cold at p0,
//                     then the 3-arg solve() at p fed that exit's own
//                     WarmStart -- the ordinary warm-start hand-off this
//                     whole project is built around. Resolves kWarm or kHot
//                     depending on whether the second solve's first
//                     subproblem actually reused the first's factorization
//                     (qp_engine.h's own reuse-eligibility conditions).
//
// A CorpusRow ONLY reports the counters of the cell's OWN designated solve
// (the one at parameter `p`) -- for kCorrupted/kFullWarm the SETUP solve at
// p0 is real work the runner pays but does not fold into the row, exactly as
// CLAUDE.md's own "one crossing step" convention reads a chained sweep: the
// question a corpus row answers is "how much did THIS hand-off cost", not
// "how much did it cost to build a hand-off to measure".
//
// FIX ROUND 1 (I1): BECAUSE the setup hop is not in the row, it is also not
// in the row's WALL DEADLINE. `run_cell` takes a `on_setup_complete` callback
// which the walk engine invokes at exactly the instant the setup work (model
// construction AND, for kCorrupted/kFullWarm, the p0 solve) is finished and
// the DESIGNATED solve is about to start; bench_corpus.cpp uses it to restart
// its deadline clock, so a budget-exhausted setup is reported as its OWN
// status (`dnf_setup`) rather than being misattributed to the taxonomy's own
// hand-off. Both phases are separately bounded -- neither is unbounded.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <fmt/format.h>

#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

#include "support/nlp_kkt_check.h"
#include "support/scale_problems.h"

namespace hven::solvers::corpus {

using hven::Index;
using hven::Vec;
using hven::solvers::from_interior_point;
using hven::solvers::IpCrossoverOptions;
using hven::solvers::QpMode;
using hven::solvers::SqpCounters;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SqpStatus;
using hven::solvers::SsnCounters;
using hven::solvers::StartLevel;
using hven::solvers::WarmStart;
using hven::solvers::test_support::F7CollocationChain;
using hven::solvers::test_support::NlpKktResidual;
using hven::solvers::test_support::self_check_kkt;

// =============================================================================
// TYPES (spec section 3's "produces" list -- later tasks rely on these exact
// names and this exact namespace, hven::solvers::corpus).
// =============================================================================

// An alias over the bench generators' own --family switch values
// (bench_scale.cpp's --family F3|F7). Only kF7 is populated in this task's
// census (the brief's grid names F7 both windows only -- F3 has no window
// split and no path rows, so it does not exercise Phase 7's SSN target);
// kF3 exists so the enum is the SAME set bench_scale.cpp already exposes,
// not a narrower one a later task would have to widen.
enum class BenchFamily { kF7, kF3 };

enum class StartTaxonomy { kNeutralCold, kPhysicsInformed, kCorrupted, kActivityOnly, kFullWarm };

// The Signorini-risk axis (spec section 3): bound-arc cells carry no active
// path row (F7's empty-window regime, p <= p_activation() = R/2); path-
// interface cells carry an active path-row window (p > R/2). Named after
// bench_f7_cold.cpp's own "empty window" / "wide window" split, which this
// enum tags rather than re-derives.
enum class ConstraintFamily { kBoundArc, kPathInterface };

struct CorpusCell {
    const char *id;
    BenchFamily family;
    Index n_nodes;
    double p0, p;
    // Reserved for a cell that measures one point of a MULTI-STEP p0 -> p
    // sweep (e.g. a specific step of a continuation run) rather than a
    // single p0 -> p hop. Every cell in this task's census is a single hop,
    // so `step_index` is always 0 here -- ABSENT, not silently unused: no
    // taxonomy in this task's design needs more than one hop, and a future
    // task that does can add multi-step cells without changing this field's
    // meaning or this one's value.
    int step_index;
    StartTaxonomy start;
    ConstraintFamily ctag;
    bool degenerate;
};

// ONE row = ONE cell's designated solve (see the file banner). `factorizations`
// and `qp_minors` are that solve's own SqpCounters::factorizations /
// qp_minor_iters (whole-solve sums, the same convention every bench CSV in
// this project already reports off a Ledger -- see bench_scale.cpp's own CSV
// SCHEMA note). `escapes` is the SSN engine's QP-level escape-to-walk count,
// summed over the solve; identically 0 for `engine == "walk"` today, since
// the walk engine has nothing to escape TO -- Task 3+ is what makes this
// field move. `kkt_residual` is the last history row's SqpIterate::
// kkt_residual (sqp_types.h), or -1.0 (WarmStart's own "never populated"
// convention) on a solve whose history is empty (converged at x0 with no
// subproblem built). `wall_s` is informational only, per this project's
// standing timing-honesty rule (ledger.h's own note) -- never a regression
// contract, and MKL_NUM_THREADS=1 for any quoted wall.
struct CorpusRow {
    const char *cell_id;
    int factorizations;
    int qp_minors;
    int escapes;
    SqpStatus status;
    double kkt_residual;
    double wall_s;
    // FIX ROUND 1 (C3). THE PER-QP READING THE GATES ACTUALLY NAME. One entry
    // per QP SUBPROBLEM the designated solve built, in history order, read
    // straight off SqpIterate::qp_factorizations (sqp_types.h) on every
    // history row with `qp_solved` -- the library already carried this
    // counter, so no include/ or src/ file changed to get it (the brief's own
    // "only if a tag needs a counter that does not exist -- expected: none").
    //
    // WHY THIS AND NOT `factorizations`. G1/G2 are pre-registered as
    // "<= 12 / <= 25 numeric factorizations PER QP"; `factorizations` above is
    // the WHOLE-SOLVE sum over every subproblem (plus the driver's own
    // out-of-subproblem factorizations), so scoring the gate off it is not
    // scoring the gate as written -- on the walk baseline it inflated the
    // p95 by three orders of magnitude. `evaluate_gates` POOLS this vector
    // across the gate population and takes the median/p95 of the pool, so one
    // multi-major cell contributes one value per subproblem rather than one
    // value per solve. `factorizations` is retained unchanged for continuity
    // with every other bench CSV in this project (and because the whole-solve
    // sum is the number a cost comparison wants).
    //
    // EMPTY on a solve that built no subproblem (converged at x0), and empty
    // on a DNF row (nothing was safely measured past the kill) -- see
    // `CorpusOutcome` for how a DNF is scored instead.
    std::vector<int> qp_factorizations;

    // =========================================================================
    // TASK 6, INSTRUMENT REQUIREMENT 1: THE MODEL-LEVEL KKT GATE.
    // =========================================================================
    //
    // Recomputed FROM THE MODEL at the returned point on EVERY row, both
    // engines, by tests/sqp/support/nlp_kkt_check.h's `self_check_kkt` -- the same
    // function tests/test_hs_battery.cpp gates its 27 problems with, reused
    // rather than re-derived. Four residuals, `kkt_verdict` the ruling on them
    // (see `kkt_gate_verdict` below for the pre-registered rule and the two
    // scale denominators it divides by).
    //
    // WHY THIS EXISTS. Task-5's re-review, finding NF-2, carried the opus
    // review's own section-7 minimum forward as a Task-6 REQUIREMENT: "any
    // kSsn scale row must be gated on a model-level KKT check including
    // complementarity, or a refusal-heavy row reads as a speed win." Task 5's
    // own history is the argument -- the kSsn tier certified a subproblem at
    // complementarity 6.311e-01 before the tier-3 refinement repaired it, the
    // repair carries a 12-of-27 REFUSAL residue, and a refused refinement is
    // back on the `fb_tol * ||lambda||inf` bound -- a bound that is only as
    // good as ||lambda||inf is small. (Task 6 fix round 1: the earlier wording
    // here said "on a corpus whose duals are 1e6-scale BY CONSTRUCTION". That
    // premise is FALSE on this census -- every gated row of both arms reads
    // dual_scale = x_scale = 1.0, so the multipliers are O(1). The REQUIREMENT
    // is unaffected: NF-2 exists because a refused refinement leaves
    // subproblem complementarity on a bound the driver never re-checks at the
    // MODEL level, which is true at any dual scale.) Nothing in the pre-Task-6
    // instrument could
    // have caught that: `status` is what the driver believes, and the driver's
    // own convergence test does not gate NLP complementarity at all
    // (sqp_driver.h's WHAT IS MEASURED BUT NOT GATED note).
    //
    // A ROW THAT FAILS THIS CHECK IS A WRONG-ANSWER ROW, IN ITS OWN CATEGORY,
    // AND NEVER A SPEED WIN: `evaluate_gates` charges it exactly as it charges
    // a DNF (the worst case, P3), so no gate can be passed by answering
    // quickly and incorrectly.
    double kkt_stationarity = -1.0;
    double kkt_primal = -1.0;
    double kkt_dual_sign = -1.0;
    double kkt_complementarity = -1.0;
    // The two denominators the rule divides by, recorded so a reader can
    // re-score the rule from the artifact alone without re-running anything.
    double dual_scale = -1.0; // max(1, ||lambda_e||inf, ||lambda_i||inf, ||z||inf)
    double x_scale = -1.0;    // max(1, ||x||inf)
    // Strictly negative inequality multipliers AT THE RETURNED POINT (the
    // magnitude half of re-review NF-1 lives in `kkt_dual_sign`; this is its
    // count half at the NLP scale, beside the driver-scale
    // SsnCounters::ssn_refine_neg_duals below).
    int neg_ineq_duals = -1;

    // The SSN kernel's own counters for this solve, verbatim off
    // SqpCounters::ssn. All zero under `engine == "walk"` -- structurally, not
    // by convention: no SSN subproblem is ever solved there.
    SsnCounters ssn{};
};

// =============================================================================
// TASK 6's PRE-REGISTERED KKT GATE (instrument requirement 1). WRITTEN, AND
// CALIBRATED ON THE WALK ARM, BEFORE ANY kSsn ROW EXISTED.
// =============================================================================
//
// W1 -- WHICH ROWS ARE GATED. Every row that CLAIMS kOptimal. A row exiting
//      kMaxIter/kNumericalError/kInfeasible/kBudgetExhausted claims nothing
//      about the point it returns, so re-checking it would manufacture
//      "wrong answers" out of honest failures; those rows report their
//      residuals in the artifact and carry the verdict `unchecked`.
//
// W2 -- THE GATED QUANTITIES ARE THREE: stationarity, primal feasibility and
//      COMPLEMENTARITY -- the three Task 6's brief names verbatim ("check
//      stationarity, feasibility, AND complementarity at the model level on
//      every row that claims kOptimal"), and complementarity is the whole
//      point of the requirement (the Task-5 defect it was written for, and
//      Phase 5's own Task-7 defect one phase earlier, were both a kOptimal
//      certificate at a point that failed complementarity and nothing else).
//
//      DUAL SIGN IS MEASURED ON EVERY ROW AND IS *NOT* IN THE GATE, and that
//      split is deliberate rather than convenient. It is the other half of the
//      same review: finding NF-1 asked Task 6 to MEASURE dual sign on the
//      scale corpus ("do not infer from HS") because the tier-3 refinement
//      adopts face prices of arbitrary sign with no driver-side drop rule to
//      re-gate them -- a telemetry request, where NF-2's was a gate request.
//      Folding it into the gate would answer a question Grant has not been
//      asked yet, on a threshold this repository sets at 1e-9 for HS
//      (tests/test_hs_battery.cpp's kSelfCheckDualSign) and has never set at
//      scale. So: `kkt_dual_sign` is recorded on every row, reported beside
//      the gate, and the COUNTERFACTUAL ("how many rows would be wrong
//      answers if dual feasibility joined the gate at the HS battery's own
//      1e-9, relative") is reported with it -- see `dual_sign_would_fail`.
//      Grant rules; this instrument does not.
//
//      ORDERING, STATED PLAINLY BECAUSE IT MATTERS. This split was made from
//      the brief's own enumeration, which predates every row -- but it was
//      WRITTEN DOWN after one kSsn smoke row (f7_n750_path_neutral_control)
//      had already been read, and that row's dual_sign was 8.2e-07 against the
//      walk's exact 0.0. The decision is not "the gate that lets this row
//      pass": it is the brief's own list, and the reading it excludes is
//      published beside the one it includes precisely so nobody has to take
//      that on trust.
//
// W3 -- THE THRESHOLD IS RELATIVE TO THE ROW'S OWN SCALE. The rule is
//      therefore
//
//          stationarity   <= kKktGateRel * max(1, ||lambda||inf, ||z||inf)
//          dual_sign      <= same
//          complementarity<= same
//          primal         <= kKktGateRel * max(1, ||x||inf)
//
//      with kKktGateRel = 1e-6: two orders of slack over the 1e-8 the solve
//      claims, and exactly the absolute figure this repository already gates
//      its HS battery and its scale smoke tests at (tests/test_hs_battery.cpp,
//      tests/test_scale_smoke.cpp), lifted onto the scale denominator the
//      corpus needs. PRIMAL keeps a PRIMAL denominator: feasibility is not a
//      dual quantity and scaling it by ||lambda|| would let a large multiplier
//      buy slack on constraint violation, which is precisely the direction
//      this gate must not bend.
//
//      (and, for the counterfactual reading only, dual_sign <= kKktDualSignRel
//      * max(1, ||lambda||inf, ||z||inf) at the HS battery's own 1e-9.)
//
//      THE PRECEDENT for the relative form is this project's own: Phase-7
//      Task 0 replaced `activity_tol`'s absolute 1e-6 with a rule relative to
//      the hand-off's own dual scale for the same reason on the same family
//      (docs/notes/2026-08-06-activity-tol-repair.md).
//
//      CORRECTION, TASK 6 FIX ROUND 1 -- THE RELATIVE FORM IS *NOT*
//      LOAD-BEARING ON THIS ARTIFACT. This banner used to justify W3 with
//      "and it has to be: this corpus's multipliers are 1e6-scale by
//      construction, so an ABSOLUTE 1e-6 gate would fail every correct row
//      and measure nothing." THAT PREMISE IS FALSE HERE, and it was measured
//      rather than assumed: `dual_scale` and `x_scale` read exactly 1.0 on
//      EVERY gated row of BOTH arms of the shipped battery, so both
//      denominators are 1 and the rule reduces to an ABSOLUTE 1e-6.
//      **No row in that battery is decided by the denominator.** The relative
//      form is kept anyway, and the reason is the surviving one: it is the
//      SAFE rule if a later cell does carry large multipliers (nothing in
//      the census guarantees dual_scale == 1), and it costs nothing where it
//      is inert. It must not be advertised as necessary on this corpus,
//      because it is not. See docs/notes/2026-08-08-ssn-gate-battery.md
//      section 1.3 for the measurement.
//
// W4 -- CALIBRATION. The rule was fixed against the WALK arm (the incumbent,
//      whose subproblem complementarity is exact by construction) before any
//      kSsn row was read: every kOptimal walk row must clear it, with the
//      margins tabulated in the battery note. A rule the incumbent cannot
//      pass is a broken rule, not a finding.
// =============================================================================

enum class KktVerdict { kUnchecked, kOk, kWrong };

inline const char *to_string(KktVerdict v) {
    switch (v) {
    case KktVerdict::kOk:
        return "ok";
    case KktVerdict::kWrong:
        return "wrong";
    case KktVerdict::kUnchecked:
        break;
    }
    return "unchecked";
}

inline KktVerdict kkt_verdict_from_string(const std::string &s) {
    if (s == "ok") {
        return KktVerdict::kOk;
    }
    if (s == "wrong") {
        return KktVerdict::kWrong;
    }
    return KktVerdict::kUnchecked;
}

// W3's constants. See the block above for the derivation and the precedent.
// kKktGateRel is the GATE's; kKktDualSignRel is the COUNTERFACTUAL's, and it is
// tests/test_hs_battery.cpp's own kSelfCheckDualSign to the digit -- the point
// of the counterfactual is to ask what the standard this repository already
// holds itself to would say at scale, not to invent a second one.
constexpr double kKktGateRel = 1.0e-6;
constexpr double kKktDualSignRel = 1.0e-9;

// Is this row's check meaningful at all? W1: only a row that CLAIMED kOptimal,
// and only one that actually carries a recorded check (an old 14-column
// artifact does not).
inline bool kkt_check_applies(const CorpusRow &row) {
    return row.status == SqpStatus::kOptimal && row.kkt_stationarity >= 0.0;
}

// W1-W3 in code, over a row's own recorded residuals and scales. A row with no
// recorded check (an old artifact, or a DNF) is `kUnchecked` and is never
// charged as wrong -- absence of evidence is not evidence of a wrong answer.
inline KktVerdict kkt_gate_verdict(const CorpusRow &row) {
    if (!kkt_check_applies(row)) {
        return KktVerdict::kUnchecked;
    }
    const double dual_bound = kKktGateRel * std::max(1.0, row.dual_scale);
    const double primal_bound = kKktGateRel * std::max(1.0, row.x_scale);
    const bool ok = row.kkt_stationarity <= dual_bound && row.kkt_complementarity <= dual_bound &&
                    row.kkt_primal <= primal_bound;
    return ok ? KktVerdict::kOk : KktVerdict::kWrong;
}

// THE COUNTERFACTUAL (W2's second half). True iff this row would ALSO be a
// wrong answer under a gate that included dual feasibility at the HS battery's
// own threshold, relative to the row's own dual scale. Reported beside the
// gate, never folded into it.
inline bool dual_sign_would_fail(const CorpusRow &row) {
    return kkt_check_applies(row) &&
           row.kkt_dual_sign > kKktDualSignRel * std::max(1.0, row.dual_scale);
}

// FIX ROUND 1 (I1). Which phase of a cell a wall deadline killed. The
// distinction is a REPORTING one, not a scoring one: both DNF phases score
// identically (worst case, see `evaluate_gates`), but attributing a
// budget-exhausted SETUP hop to the taxonomy's own hand-off was a real
// misattribution in the first baseline -- six of its fourteen DNF rows were
// recorded under kCorrupted/kFullWarm when what actually exhausted the budget
// was their COLD setup solve at p0.
enum class DnfPhase { kNone, kSetup, kSolve };

inline const char *dnf_phase_status_string(DnfPhase p) {
    switch (p) {
    case DnfPhase::kSetup:
        return "dnf_setup";
    case DnfPhase::kSolve:
        return "dnf_budget";
    case DnfPhase::kNone:
        break;
    }
    return "";
}

// TASK 6. THE THIRD WAY A CELL CAN FAIL TO PRODUCE AN ANSWER, and it is not
// hypothetical: the kSsn arm THREW on four path-interface cells of this
// census, out of SsnEngine::solve's own caller-error check on a seeded `z`.
//
// Task 1 had two non-answers (a setup DNF and a solve DNF) and treated an
// abnormally-exiting child as a HARD ERROR that aborted the whole sweep --
// correct while the only engine was the walk, which cannot throw from inside a
// solve, and wrong now. An engine that throws on a corpus cell is an OUTCOME
// the corpus has to be able to report, exactly as a timeout is: losing the row
// loses the finding, and aborting the sweep loses the other 56.
//
// IT IS CHARGED THE WORST CASE, like every other non-answer (P3/W5): an engine
// that threw has not answered, and must never score better than one that
// answered slowly. It keeps its OWN status string, never a `dnf_*` one,
// because the two say completely different things about the engine and a
// reader must not have to guess which happened.
//
// THE PARENT ONLY TREATS EXIT CODE 1 THIS WAY -- bench_corpus.cpp main()'s
// documented T6 exit, i.e. "an exception was caught and its message printed".
// A signal, or any other code, stays a hard error: a segfault is not a
// measurement.
constexpr const char *kEngineErrorStatusString = "engine_error";

// ONE cell's outcome: either a real CorpusRow or a wall-deadline DNF. This is
// what `evaluate_gates` scores, and it lives HERE rather than in the runner
// because the gate population is defined by the CELL's tags -- pushing that
// filtering into bench_corpus.cpp (as the first issue of this task did) put
// the load-bearing half of the verdict in untested glue.
struct CorpusOutcome {
    const CorpusCell *cell = nullptr;
    CorpusRow row{}; // valid iff phase == kNone
    DnfPhase dnf_phase = DnfPhase::kNone;
    // The ENFORCED DEADLINE on a DNF (the one honest number a killed cell
    // has), 0.0 otherwise. Informational, never scored.
    double dnf_wall_s = 0.0;
    // TASK 6. The cell's designated solve THREW (see kEngineErrorStatusString
    // above). Mutually exclusive with `dnf_phase`: a killed child never got to
    // throw, and a child that threw was never killed.
    bool engine_error = false;
    // The exception's own message, as the child recorded it. Informational --
    // it reaches the console and the report, never the CSV (a message with
    // commas in it is not a CSV field, and this artifact's schema is API).
    std::string engine_error_what;

    bool dnf() const { return dnf_phase != DnfPhase::kNone; }
    // No answer of any kind was produced: a wall-deadline kill, or a throw.
    bool no_answer() const { return dnf() || engine_error; }

    // TASK 6, instrument requirement 1. A row that CLAIMED kOptimal and failed
    // the model-level KKT gate. RE-DERIVED from the row's own residuals rather
    // than read off a stored verdict column, so an artifact cannot assert its
    // own innocence (the `--from-csv` reader additionally rejects a row whose
    // stored verdict disagrees with the re-derivation, the same discipline the
    // qp_subproblems consistency check already applies).
    bool wrong_answer() const {
        return !no_answer() && kkt_gate_verdict(row) == KktVerdict::kWrong;
    }
};

// =============================================================================
// THE GATES (pre-registered, disabled until Task 6 -- spec's Global
// Constraints, transcribed VERBATIM):
//
//   G1  SSN warm-mode median <= 12 numeric factorizations per QP on
//       path-constraint-heavy cells;
//   G2  SSN warm-mode p95 <= 25 on the same cells;
//   G3  no median growth N = 5,000 -> N = 20,000 at fixed physical problem
//       (same family, same p-path, refined mesh);
//   G4  escape/fallback rate < 2% of corpus QPs outside cells tagged
//       intentionally-degenerate.
//
// evaluate_gates() COMPILES this arithmetic now so it is mutation-testable
// before there is an SSN row to score, but nothing in this task (or its
// runner) ASSERTS its `pass[]` verdicts -- --score-gates below controls
// whether bench_corpus.cpp's main() even calls it, and Task 6 is where a
// failing gate becomes a build-breaking assertion.
//
// =============================================================================
// PRE-REGISTRATION, FIX ROUND 1: the four readings the prose above does not
// fix on its own. RECORDED BEFORE ANY SSN ROW EXISTS (the walk engine is
// still the only engine in the tree, and --engine ssn throws), which is the
// entire point -- see docs/notes/2026-08-06-corpus-design.md section 5.
// =============================================================================
//
// P1. G1/G2's POPULATION is "path-constraint-heavy cells" INTERSECTED with
//     "SSN warm-mode": cells with `ctag == kPathInterface` AND
//     `start` in {kFullWarm, kActivityOnly} AND NOT `degenerate`. The gate
//     text says "SSN warm-mode ... on path-heavy cells" and the first issue
//     of this task silently read only the second half, putting cold rows
//     (kNeutralCold/kPhysicsInformed) into a warm-mode statistic. kCorrupted
//     is a DAMAGED hand-off, deliberately out of contract, and is excluded
//     for the same reason `degenerate` is: it is not what "warm mode" is
//     promising. Encoded in `in_g1_g2_population` below, not in the runner.
//
// P2. G1/G2's QUANTITY is PER QP SUBPROBLEM (CorpusRow::qp_factorizations),
//     POOLED across the population -- a cell that builds k subproblems
//     contributes k values. Median and p95 are nearest-rank over that pool.
//
// P3. A DNF IS SCORED, NEVER SKIPPED. A cell killed at its wall deadline
//     (either phase) conceptually costs +infinity factorizations; it is
//     charged, in every gate, as `kDnfChargedSubproblems` subproblems each
//     costing `kDnfFactorizationSentinel` factorizations. That sentinel is
//     budget-clamped (a finite int, so the arithmetic stays exact) and is
//     chosen so far above G1's 12 and G2's 25 that any median or p95 it
//     reaches is above threshold. CONSEQUENCES, all intended:
//       * G1/G2: a DNF row's contribution dominates any finishing row's, so
//         a TIMING-OUT ENGINE CAN NEVER OUTSCORE A FINISHING ONE.
//       * G3: any pair with a DNF on EITHER side contributes +sentinel to the
//         growth vector -- a pair that cannot be measured at both sizes is
//         not evidence of "no growth", and the ruling is that it counts AS
//         growth.
//       * G4: a DNF contributes `kDnfChargedSubproblems` to the denominator
//         and the same number to the numerator (every charged subproblem
//         counts as an escape), so it cannot dilute the rate either.
//     The first issue of this task did the opposite everywhere (DNF rows were
//     dropped before the populations were built), which made "do not finish"
//     the cheapest way to pass three of the four gates.
//
// P4. G4's DENOMINATOR IS QP SUBPROBLEMS, not rows -- "< 2% of corpus QPs",
//     verbatim. The per-row denominator the first issue used was quantized at
//     1/36 = 2.78%, i.e. coarser than the threshold it was testing, making
//     the gate de facto "zero escaping CELLS".
//
// P5. G3's POPULATION IS PATH-CONSTRAINT-HEAVY, NON-DEGENERATE CELLS, paired
//     at N = 5000 <-> N = 20000 on (window, taxonomy). G2 says "on the same
//     cells" and G3 continues that sentence -- "same family, same p-path,
//     refined mesh" describes the PAIRING, not the population. The warm-mode
//     half of P1 is NOT applied here, because G3's own text does not carry
//     it and five pairs is a healthier sample than two.
//
//     WHY THIS IS NOT A FREE CHOICE, and why it was made before any SSN row
//     existed: with EVERY non-degenerate cell in scope the census supplies 10
//     pairs, 5 of them empty-window (bound-arc) cells that cost exactly one
//     factorization per QP at every N, for every engine, by construction
//     (the working set never changes there -- scale-study-cold.md's Arm A).
//     Five structurally-flat pairs against five measured ones puts the
//     nearest-rank median ON a flat pair, so G3 reports "no growth" no matter
//     what the measured half did. Measured on this task's own walk baseline:
//     the unrestricted reading gives g3_growth = 0.000 PASS while ALL FIVE
//     path pairs are DNF-at-N=20000 -- the same "a non-finishing engine
//     outscores a finishing one" pathology P3 exists to close, wearing
//     different clothes. The restriction is engine-independent and can only
//     make the gate HARDER (it removes pairs no engine can fail), so it
//     cannot flatter SSN either.
//
// G3's per-row statistic is that row's OWN median per-QP factorization count
// (sentinel on a DNF, per P3), and g3_growth is the median of the paired
// deltas; the gate passes at <= 0.
//
// W5 (TASK 6, and it is P3's rule applied to a second kind of non-answer). A
//     WRONG-ANSWER ROW -- one that claimed kOptimal and failed the model-level
//     KKT gate above -- IS CHARGED EXACTLY AS A DNF IS, in every gate. The
//     reasoning is P3's verbatim with one word changed: a DNF measured
//     nothing, a wrong answer measured something that is not an answer, and in
//     both cases crediting the row's own factorization count would let a gate
//     be passed by not solving the problem. Task 5's own history is why this is
//     not hypothetical (a kSsn subproblem certified at complementarity 0.631),
//     and Phase 5's Task 7 defect -- a warm solve certifying kOptimal in zero
//     majors at a non-KKT point -- is the same class one phase earlier.
//     `charged_as_worst_case` is the one predicate both rules go through.
//
// P1-BOTH-WAYS (TASK 6). `in_g1_g2_population` takes an `include_corrupted`
//     flag and `evaluate_gates` forwards it. This is a REPORTING requirement
//     the corpus-design note's §5.1 imposed on this task, not a new ruling:
//     P1's kCorrupted exclusion was disclosed as a post-hoc LOOSENING that
//     moves the bar in the scored engine's favour, so Task 6 reports G1/G2
//     under both readings and Grant ratifies against the numbers.
// =============================================================================

struct GateVerdict {
    double g1_median = 0.0;
    double g2_p95 = 0.0;
    double g3_growth = 0.0;
    double g4_escape_rate = 0.0;
    bool pass[4] = {false, false, false, false};
    // PARTIAL-POOL GUARD (final branch review, WAVE #4; T1 NEW-4). `median`/
    // `percentile` return 0.0 on an empty input, which is `<= 12.0`/`<= 0.0`
    // and therefore a VACUOUS PASS -- a `--cells` invocation over a subset of
    // the census (e.g. no 5000<->20000 pair present) can read G1/G3 PASS
    // having scored nothing. These sizes let a caller distinguish "passed"
    // from "nothing to score"; `--score-gates` asserts on them below.
    std::size_t g1g2_pool_size = 0;
    std::size_t g3_pair_count = 0;
};

// P3's budget-clamped stand-in for "+infinity factorizations". 1e6 is 8e4x
// G1's threshold and 4e4x G2's, and 100x the largest whole-solve
// factorization count this family has ever produced (39744, the N = 2000
// wide-window stall) -- so it is unreachable by measurement, while staying a
// finite int that keeps every median/p95/delta exact integer arithmetic.
constexpr int kDnfFactorizationSentinel = 1000000;

// P3's charge in SUBPROBLEMS. Equal to `detail::kMajorMaxIter` (the option
// set's own major cap, and therefore the most subproblems any cell in this
// corpus can build) -- a DNF is charged the WORST CASE it could have been on
// its way to, not one token subproblem, so it also dominates in COUNT and not
// only in value. static_assert'd against kMajorMaxIter below, where that
// constant is defined.
constexpr int kDnfChargedSubproblems = 10;

namespace detail {

// Nearest-rank percentile over a COPY of `values` (sorted in place), 0 on an
// empty input. `q` in [0, 1]; the nearest-rank index is
// ceil(q * n) - 1, clamped into range -- the same method the p95 gate names
// (G2) with no interpolation, so the reported value is always one this
// corpus actually produced. `q <= 0` clamps to the first element rather than
// underflowing the unsigned index (unreachable from the gates, which only
// ever pass 0.5 and 0.95, but a silent wrap is not a thing to leave lying
// around in a scored path).
inline double percentile(std::vector<double> values, double q) {
    if (values.empty()) {
        return 0.0;
    }
    std::sort(values.begin(), values.end());
    if (q <= 0.0) {
        return values.front();
    }
    const auto n = static_cast<double>(values.size());
    auto idx = static_cast<std::size_t>(std::ceil(q * n)) - 1;
    idx = std::min(idx, values.size() - 1);
    return values[idx];
}

inline double median(std::vector<double> values) { return percentile(std::move(values), 0.5); }

// The GATE ARITHMETIC, separated from the population-building above it so the
// thresholds and the percentile convention can be pinned (and mutated) on
// hand-built vectors with no cell table in sight. `escape_denominator` of 0
// reports a rate of 0 (vacuously clean); every threshold is verbatim from the
// spec: G1 <= 12, G2 <= 25, G3 <= 0, G4 < 0.02.
inline GateVerdict gate_arithmetic(std::vector<double> pooled_qp_factorizations,
                                   std::vector<double> growths, double escape_numerator,
                                   double escape_denominator) {
    GateVerdict v;
    v.g1g2_pool_size = pooled_qp_factorizations.size();
    v.g3_pair_count = growths.size();
    v.g1_median = median(pooled_qp_factorizations);
    v.g2_p95 = percentile(std::move(pooled_qp_factorizations), 0.95);
    v.pass[0] = v.g1_median <= 12.0;
    v.pass[1] = v.g2_p95 <= 25.0;

    v.g3_growth = median(std::move(growths));
    v.pass[2] = v.g3_growth <= 0.0;

    v.g4_escape_rate = escape_denominator <= 0.0 ? 0.0 : escape_numerator / escape_denominator;
    v.pass[3] = v.g4_escape_rate < 0.02;
    return v;
}

// P3's charge, as a vector of per-QP values, for one DNF row.
inline std::vector<double> dnf_charged_values() {
    return std::vector<double>(static_cast<std::size_t>(kDnfChargedSubproblems),
                               static_cast<double>(kDnfFactorizationSentinel));
}

// P3 (and TASK 6's W5, the wrong-answer extension of it): the outcomes that are
// charged the worst case rather than their own measured cost. A DNF measured
// nothing; a WRONG ANSWER measured something that is not an answer, and
// crediting its factorization count would let a gate be passed by being fast
// and incorrect -- the exact failure the first issue's DNF-skipping had in a
// different costume.
inline bool charged_as_worst_case(const CorpusOutcome &o) {
    return o.no_answer() || o.wrong_answer();
}

// The per-QP factorization values ONE outcome contributes to a pooled
// percentile: its own measured per-subproblem counts, or the worst-case charge
// if it DNF'd or answered wrongly.
inline std::vector<double> outcome_qp_values(const CorpusOutcome &o) {
    if (charged_as_worst_case(o)) {
        return dnf_charged_values();
    }
    std::vector<double> out;
    out.reserve(o.row.qp_factorizations.size());
    for (const int f : o.row.qp_factorizations) {
        out.push_back(static_cast<double>(f));
    }
    return out;
}

// G3's per-row statistic (see the pre-registration block): the row's own
// median per-QP factorization count, or the sentinel on a DNF. A solve that
// built no subproblem at all reports 0 -- it really did zero factorizations.
inline double outcome_qp_median(const CorpusOutcome &o) {
    if (charged_as_worst_case(o)) {
        return static_cast<double>(kDnfFactorizationSentinel);
    }
    return median(outcome_qp_values(o));
}

} // namespace detail

// P1's population predicate, exposed so tests can pin the ruling directly
// rather than inferring it from a verdict.
//
// `include_corrupted` IS THE BOTH-WAYS REPORTING REQUIREMENT, not a new
// ruling. P1's exclusion of kCorrupted was disclosed in fix round 1 as a
// POST-HOC LOOSENING -- it removes the most expensive rows in the whole warm
// family from a median and a p95, which is strictly easier for whatever engine
// is being scored, and the corpus-design note's own §5.1 requires that "Task 6
// must therefore report G1 and G2 BOTH ways, with and without kCorrupted, and
// Grant ratifies the population against those numbers rather than against this
// paragraph." The default (`false`) is the pre-registered reading; the flag
// exists so the SAME rows produce the other one with no re-sweep.
inline bool in_g1_g2_population(const CorpusCell &cell, bool include_corrupted = false) {
    if (cell.ctag != ConstraintFamily::kPathInterface || cell.degenerate) {
        return false;
    }
    return cell.start == StartTaxonomy::kFullWarm || cell.start == StartTaxonomy::kActivityOnly ||
           (include_corrupted && cell.start == StartTaxonomy::kCorrupted);
}

// P5's population predicate: path-constraint-heavy and non-degenerate, with
// no warm-mode restriction. Exposed for the same reason P1's is.
inline bool in_g3_population(const CorpusCell &cell) {
    return cell.ctag == ConstraintFamily::kPathInterface && !cell.degenerate;
}

// THE GATE EVALUATOR. One argument, the run's own outcomes (the brief's
// `evaluate_gates(rows)` shape): every population filter, the G3 pairing and
// the DNF charge live HERE, where tests reach them, rather than in the
// runner's glue.
inline GateVerdict evaluate_gates(const std::vector<CorpusOutcome> &outcomes,
                                  bool include_corrupted = false) {
    std::vector<double> pooled;
    for (const CorpusOutcome &o : outcomes) {
        if (o.cell == nullptr || !in_g1_g2_population(*o.cell, include_corrupted)) {
            continue;
        }
        const std::vector<double> vals = detail::outcome_qp_values(o);
        pooled.insert(pooled.end(), vals.begin(), vals.end());
    }

    // G3 (P5): match every (window, taxonomy) present at BOTH N = 5000 and
    // N = 20000 inside the path-heavy, non-degenerate population. O(n^2) over
    // at most ~60 outcomes.
    std::vector<double> growths;
    for (const CorpusOutcome &a : outcomes) {
        if (a.cell == nullptr || a.cell->n_nodes != 5000 || !in_g3_population(*a.cell)) {
            continue;
        }
        for (const CorpusOutcome &b : outcomes) {
            if (b.cell == nullptr || b.cell->n_nodes != 20000 || !in_g3_population(*b.cell) ||
                b.cell->ctag != a.cell->ctag || b.cell->start != a.cell->start) {
                continue;
            }
            // P3: an unmeasurable side is growth, not an absence. W5: so is a
            // side that answered wrongly.
            growths.push_back(detail::charged_as_worst_case(a) || detail::charged_as_worst_case(b)
                                  ? static_cast<double>(kDnfFactorizationSentinel)
                                  : detail::outcome_qp_median(b) - detail::outcome_qp_median(a));
            break;
        }
    }

    // G4: escapes per QP SUBPROBLEM over the non-degenerate corpus (P4), with
    // a DNF charged as all-escaping (P3).
    double escapes = 0.0;
    double subproblems = 0.0;
    for (const CorpusOutcome &o : outcomes) {
        if (o.cell == nullptr || o.cell->degenerate) {
            continue;
        }
        if (detail::charged_as_worst_case(o)) {
            escapes += static_cast<double>(kDnfChargedSubproblems);
            subproblems += static_cast<double>(kDnfChargedSubproblems);
            continue;
        }
        escapes += static_cast<double>(o.row.escapes);
        subproblems += static_cast<double>(o.row.qp_factorizations.size());
    }

    return detail::gate_arithmetic(std::move(pooled), std::move(growths), escapes, subproblems);
}

// =============================================================================
// TAG -> RUN CONSTANTS, each with its own provenance.
// =============================================================================

namespace detail {

// F7's own activation threshold is p_activation() = R/2 = 0.5 (scale_problems.h).
// Bound-arc (empty-window) cells sit at 0.45/0.40 -- both comfortably below
// 0.5, so the kFullWarm/kCorrupted SOURCE point never crosses into the
// active regime mid-hop there. Path-interface (wide-window) cells reuse 0.85
// EXACTLY -- the identification-stall-study's own p
// (docs/notes/2026-08-03-identification-stall-study.md, every invocation in
// its Appendix A) and docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2's
// Arm B -- so this corpus's degenerate cells (N = 800, N = 2000) are the SAME
// recipe those notes already measured, per the brief's "use its committed
// values, don't re-derive". 0.80 is the SOURCE point for kCorrupted/
// kFullWarm's setup hop, same regime as the 0.85 target.
//
// A CROSS-REGIME SOURCE (bound-arc 0.45 -> path-interface 0.85) WAS TRIED
// AND MEASURED WORSE, and the reason is worth recording so it is not
// retried: CLAUDE.md's own headline ("warm-start's advantage is scale-
// invariant per family ... across F7 N=100-10,000") is earned on SMALL,
// SAME-REGIME hops -- continuation.h's adaptive dp schedule never proposes a
// jump that crosses an activation threshold in one step. A single 3-arg
// warm solve from an EMPTY active window straight to an ~85%-active one has
// to discover essentially the whole working set regardless of the warm
// primal/dual seed, which is the SAME identification cost a cold solve
// pays -- it does not relocate the cost to the (now cheap) setup step, it
// just fails to buy back the warm advantage on the step that matters. A
// smoke measurement of two such cells (N = 5000 path-interface,
// kFullWarm/kCorrupted) did not finish inside 120 s where the same-regime
// design's target step is expected to be CHEAP (Phase-5's own warm-cost
// finding) -- so same-regime (0.80 -> 0.85) is what ships.
constexpr double kBoundArcP = 0.45;
constexpr double kBoundArcP0 = 0.40;
constexpr double kPathInterfaceP = 0.85;
constexpr double kPathInterfaceP0 = 0.80;

// The committed cap (identification-stall-study.md Sec. 3's own invocation,
// `hven_sqp_f7_cold <N> 0.85 20000 512 refactorize`; scale-study-cold.md
// Sec. 4.2's N = 800/2000 rows) applied UNIFORMLY to every path-interface
// cell in this corpus, healthy or not. This is a Task-1 TRACTABILITY choice,
// stated plainly: at N ranges this note never measured (5000/10000/20000)
// the true per-subproblem demand is unknown, and the alternative -- the
// library's own size-derived default (qp_engine.h's derived_qp_max_iter,
// 5 * (n + mi + #bounded) = 40 * N on this family) -- was rejected because it
// scales FASTER than this cap and would make an already-known-hard cell's
// wall time scale with N^2 instead of failing at a bounded cost. A cell that
// reports kNumericalError under this cap is reporting the walk engine's real
// cold-start ceiling at that N, exactly as the committed notes already
// found -- not a runner defect. See corpus-design.md Sec. 4 for the full
// argument and the observed wall times.
constexpr Index kPathInterfaceQpMaxIter = 20000;

// SqpOptions::max_iter (MAJORS), applied to every cell. Every healthy F7
// solve in every prior measurement note used <= 5 majors
// (bench_f7_cold.cpp's own banner: "every solve in the study used <= 5");
// every FAILING wide-window cold cell this project has measured also gave
// up within 3-5 majors (scale-study-cold.md Sec. 4.2's N = 1500/2000 rows).
// 10 is double the observed ceiling on either side -- enough headroom that
// no healthy cell in this census is at risk of hitting it, and a bound on
// how many times a pathological cell can re-pay the minor cap above.
constexpr Index kMajorMaxIter = 10;

// P3's DNF charge is "the most subproblems this option set can build", which
// IS the major cap -- pinned here so the two can never drift apart silently.
static_assert(kDnfChargedSubproblems == static_cast<int>(kMajorMaxIter),
              "kDnfChargedSubproblems must equal kMajorMaxIter -- see the pre-registration "
              "block's P3");

// =============================================================================
// THE PER-SOLVE MINOR-ITERATION BUDGET -- SECONDARY, DEFENSE-IN-DEPTH ONLY.
// =============================================================================
//
// `kMajorMaxIter` x `kPathInterfaceQpMaxIter` alone bounds a solve at
// 10 x 20000 = 200000 minors worst case, ~12 h at N = 20000's own measured
// per-minor cost (~1.07e-5 s/minor/N, scale-study-cold.md Sec. 4.2). This
// budget cuts that to (roughly) kMinorBudget + one major's worth, via
// SqpDriver's own 4-arg solve(model, x0, warm, minor_budget) overload
// (Phase-6 Task 1's "controller retry economics" lever, checked BETWEEN
// majors, returns kMaxIter with SqpCounters::probe_budget_stops == 1 and
// real counters-so-far). It is NOT, on its own, the wall-clock bound this
// corpus needs: kCorrupted/kFullWarm pay TWO independent solves (a SETUP
// hop and the reported TARGET hop), each separately eligible to spend up to
// this many minors, so the CELL's total is not tightly bounded by this
// number alone -- measured directly: with only this budget in place, the
// three affected cells were STILL running 30+ minutes into what would have
// been another multi-hour, effectively unbounded, run. See
// detail::wall_budget_seconds below and bench_corpus.cpp's fork/exec
// deadline enforcement for the PRIMARY mechanism this corpus actually
// relies on; this minor budget is kept as a cheap secondary net (it costs
// nothing and further bounds a solve's OWN internal cost once the process
// is already inside its wall deadline).
constexpr Index kMinorBudget = 50000;

// =============================================================================
// THE PER-CELL WALL-CLOCK DEADLINE (controller intervention, this task) --
// THE PRIMARY MECHANISM. An unbounded runner is a correctness gap, not a
// convenience miss: Tasks 2/6 replay this corpus, and a DNF has to be a ROW
// it can report, not a process that never returns. Enforcement itself lives
// in bench_corpus.cpp (it needs to fork/exec and kill a child process,
// which is orchestration, not engine-interface code); this function is the
// PURE, testable part -- the nx -> seconds mapping -- shared by the runner
// and tests/test_corpus_cells.cpp.
//
// =============================================================================
// THE BAND, RE-DERIVED IN FIX ROUND 1 (C2). THE RULE, STATED FIRST:
//
//   R1. THREE-TIMES MARGIN OVER EVIDENCE. Where a COMMITTED, UNCONTENDED,
//       single-threaded runtime R exists in this repository's own notes for a
//       census cell -- or for the HARDEST taxonomy at that cell's own N and
//       window, which is what actually has to fit -- the cell's tier must
//       satisfy tier >= 3 * R. Three, not two: this project's own contention
//       measurements put an 8-way sweep a few percent over an isolated run,
//       and the previous band's failure mode was a 7% margin (279 s against
//       300 s) that ordinary scheduling noise closed, DELETING the named
//       frozen cell's reference row from the artifact every later task cites.
//   R2. MONOTONE IN SIZE. Tiers are non-decreasing in nx. The previous band
//       inherited a NON-monotonicity from its source (900 s at nx = 10^4 but
//       600 s at nx = 10^5) which inverted exactly where it hurt: the hardest
//       SMALL cell in the census got the tightest budget.
//   R3. SIZE-SCALED WHERE THERE IS NO EVIDENCE, WITH A DECLARED CEILING. See
//       the ceiling paragraph below.
//
// THE EVIDENCE (docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2's Arm B
// table, MKL_NUM_THREADS=1, uncontended, the same p = 0.85 wide window and
// the same qp.max_iter = 20000 cap this corpus uses; kNeutralCold is the
// hardest taxonomy at every N in this census, so its figure is the binding
// one for the whole N):
//
//   N =  750  ->  73.9 s  (kOptimal, 9297 minors)          3R =  222 s
//   N =  800  -> 279   s  (kOptimal, 30165 minors -- THE   3R =  837 s
//                          NAMED FROZEN CELL's own row)
//   N = 1000  -> 121   s  (kOptimal, 11043 minors)         3R =  363 s
//   N = 2000  -> 819   s  (kNumericalError, 40004 minors)  3R = 2457 s
//   N = 825   -> no committed figure; bracketed by 750 and 1000, both far
//                inside the tier those two share.
//   N >= 5000 -> NO COMMITTED FIGURE OF ANY KIND. Sec. 4.2's largest
//                FINISHING wide-window row is n = 7500 (N = 1500) at 434 s;
//                n = 10^4 (N = 2000) is the largest row of any kind.
//
// THE TIERS (each value is the smallest round number clearing its band's
// binding 3R, in the 900/2700/3600 ladder the first tier fixes):
//
//   nx <=   8000  ->  900 s   binding: N = 800's 3R = 837 s
//   nx <= 10^4    -> 2700 s   binding: N = 2000's 3R = 2457 s
//   nx >  10^4    -> 3600 s   CEILING, see below
//
// THE CEILING, AND WHY IT IS NOT A DODGE. No cell above nx = 10^4 has ever
// been observed to finish this recipe. Size-scaling from the evidence that
// does exist gives no usable number: Sec. 4.2's own identification cost grows
// like N^2.8 across its N = 1000 -> 2000 step (121 s -> 819 s), which
// extrapolates to ~1e4 s at N = 5000 and ~1e6 s at N = 20000 -- multi-day
// budgets that no sweep can pay and that would not change a single row's
// STATUS, only how long the runner waits before writing it. So the ladder
// SATURATES at 3600 s (4x the floor tier, 1.33x the 10^4 tier), and the
// honest reading of a saturated DNF is written into the gates rather than
// into the budget: the pre-registration block's P3 charges a DNF as the
// WORST case in every gate, so a saturated ceiling can only ever make the
// walk engine look WORSE, never better. A ceiling that cannot flatter the
// incumbent is a tractability choice; one that could would be a bar defect.
//
// EVERY CENSUS CELL'S TIER, AND ITS MARGIN OVER THE EVIDENCE:
//
//   N =  750 (nx =  3750) ->  900 s   12.2x its 73.9 s reference
//   N =  800 (nx =  4000) ->  900 s    3.2x its  279 s reference  <- binding
//   N =  825 (nx =  4125) ->  900 s   no reference (bracketed)
//   N = 1000 (nx =  5000) ->  900 s    7.4x its  121 s reference
//   N = 2000 (nx = 10000) -> 2700 s    3.3x its  819 s reference  <- binding
//   N = 5000 (nx = 25000) -> 3600 s   no reference (ceiling)
//   N = 10000(nx = 50000) -> 3600 s   no reference (ceiling)
//   N = 20000(nx =100000) -> 3600 s   no reference (ceiling)
//
// AND THE BUDGET IS PER PHASE, NOT PER CELL (fix round 1, I1): a
// kCorrupted/kFullWarm cell's SETUP hop gets its own full tier and its own
// status (`dnf_setup`) -- so the tier above is what the REPORTED solve gets,
// undiluted by the cost of building its hand-off, and a cell's total wall is
// bounded by 2x its tier.
constexpr Index kWallBudgetFloorNx = 8000;
constexpr Index kWallBudgetMidNx = 10000;
constexpr double kWallBudgetFloorSeconds = 900.0;
constexpr double kWallBudgetMidSeconds = 2700.0;
constexpr double kWallBudgetCeilingSeconds = 3600.0;

inline double wall_budget_seconds(Index nx) {
    if (nx <= kWallBudgetFloorNx) {
        return kWallBudgetFloorSeconds;
    }
    if (nx <= kWallBudgetMidNx) {
        return kWallBudgetMidSeconds;
    }
    return kWallBudgetCeilingSeconds;
}

// F7's own nx = 5 * n_nodes (5 variables/node: 3 states + 2 controls, this
// corpus's fixed shape -- see make_model below). Only F7 is populated in
// this census (BenchFamily::kF7), so this is not generalized over family;
// a future F3 cell would need its own mapping here.
inline double wall_budget_for_cell(const CorpusCell &cell) {
    return wall_budget_seconds(static_cast<Index>(5 * cell.n_nodes));
}

// A stable fingerprint of the TABLE ABOVE, stamped into every CSV this corpus
// writes (bench_corpus.cpp's provenance header) so a reader can tell at a
// glance whether two artifacts were produced under the same budgets -- the
// reviewer's biggest cannot-verify on the first baseline was exactly this.
// FNV-1a over the five constants' decimal forms; it changes if any tier
// boundary or any tier value changes, and nothing else in this file affects
// it.
inline std::uint64_t budget_table_hash() {
    const std::string material =
        fmt::format("{}|{}|{:.6f}|{:.6f}|{:.6f}", kWallBudgetFloorNx, kWallBudgetMidNx,
                    kWallBudgetFloorSeconds, kWallBudgetMidSeconds, kWallBudgetCeilingSeconds);
    // The seed is the FNV-1a 64-bit offset basis with its final digit dropped
    // (14695981039346656037 mistyped as 19 digits). INTENTIONALLY AS-IS: the
    // pinned budget_table_hash 0x357aee91dee27391 -- asserted in
    // tests/sqp/test_corpus_cells.cpp and stamped into six baseline CSV
    // headers under bench/baselines/ -- was computed with this value, so
    // "correcting" it silently breaks all seven. Any change here is a
    // declared re-derivation under CLAUDE.md section 7, never a tidy-up.
    std::uint64_t h = 1469598103934665603ULL;
    for (const char c : material) {
        h ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        h *= 1099511628211ULL;
    }
    return h;
}

// bench_scale.cpp's own bench_options() tolerances/levers (IPM-bridge-
// comparable, and the convention every Phase 5-7 bench CSV in this project
// already reports under) -- reused verbatim rather than re-derived so this
// corpus's numbers sit on the same footing as every prior sweep.
constexpr double kKktTol = 1e-8;
constexpr double kFeasTol = 1e-8;

// kPhysicsInformed's displacement off x*(p) -- large enough that the start
// is not the analytic optimum itself (which would converge in zero majors
// and measure nothing), small enough that it stays a "physically informed"
// neighbourhood rather than a second cold start. Same deterministic
// sin-index pattern Task 0's ip_repair::f7_ip_iterate uses for its own
// primal displacement (no RNG anywhere in this corpus).
constexpr double kPhysicsInformedDisp = 1.0e-3;

// kCorrupted's damage magnitude -- an order above kPhysicsInformedDisp,
// deliberately large enough that the geometric activity at the corrupted x
// disagrees with the stale hand-off's own activity hint/working set for a
// meaningful fraction of rows (an internally INCONSISTENT hand-off, the
// class of input the driver's ingest-side complementarity clear
// (sqp_driver.h's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY note) and
// Phase 7's escape-rate gate both exist to be safe against).
constexpr double kCorruptedXDisp = 0.05;

// kActivityOnly's synthetic central-path barrier level at the reference mesh
// this project has actually MEASURED the false-active guard at
// (docs/notes/2026-08-06-activity-tol-repair.md Sec. 5.3): mu = 1e-9 holds
// (0 false actives) through N = 1600. This corpus's crossover cells reach
// N up to 20000, well past that reference, so `crossover_mu_for_n` below
// scales mu DOWN in proportion to N/kActivityMuRefN -- the same O(1/N) trend
// the boundary's own closed form follows -- rather than assume 1e-9 stays
// tight. This is the "tight-mu" branch of the Task-0 carry ("a fine-mesh
// crossover cell must hand over at mu <= dual_tol * min|cI| ... OR carry a
// hint-quality tag"); it is NOT independently re-verified past N = 6400 and
// is carried to Task 6 (cross-check against `ip_activity_inferred` and each
// cell's own analytic active set before trusting a gate built on it).
constexpr double kActivityMuAtRefN = 1.0e-9;
constexpr double kActivityMuRefN = 1600.0;

inline double crossover_mu_for_n(Index n_nodes) {
    const double n = static_cast<double>(n_nodes);
    return kActivityMuAtRefN * std::min(1.0, kActivityMuRefN / n);
}

// String storage for programmatically-generated cell ids (the main grid,
// below) -- a std::set so existing elements' addresses never move on further
// insertion, unlike std::vector, which is what lets CorpusCell::id (a bare
// const char*) point into it safely for the corpus's whole, effectively-
// program, lifetime (the pool is a function-local static).
//
// AN INTERNER, NOT AN APPEND LOG: `pooled_id` returns the EXISTING entry when
// the same id is requested twice, so a caller that invokes build_all_cells()
// more than once (only tests do) neither grows the pool without bound nor
// hands out two different pointers for one id.
inline std::set<std::string> &id_pool() {
    static std::set<std::string> pool;
    return pool;
}

inline const char *pooled_id(std::string s) {
    return id_pool().insert(std::move(s)).first->c_str();
}

inline const char *taxonomy_tag(StartTaxonomy t) {
    switch (t) {
    case StartTaxonomy::kNeutralCold:
        return "neutral";
    case StartTaxonomy::kPhysicsInformed:
        return "physics";
    case StartTaxonomy::kCorrupted:
        return "corrupted";
    case StartTaxonomy::kActivityOnly:
        return "activity";
    case StartTaxonomy::kFullWarm:
        return "warm";
    }
    return "unknown";
}

inline const char *window_tag(ConstraintFamily c) {
    return c == ConstraintFamily::kBoundArc ? "bound" : "path";
}

} // namespace detail

// Public forwarder -- the runner stamps this into every artifact's provenance
// header, so it is part of the surface, not an implementation detail.
inline std::uint64_t budget_table_hash() { return detail::budget_table_hash(); }

inline const char *to_string(StartTaxonomy t) { return detail::taxonomy_tag(t); }
inline const char *to_string(ConstraintFamily c) { return detail::window_tag(c); }
inline const char *to_string(BenchFamily f) { return f == BenchFamily::kF7 ? "F7" : "F3"; }

// =============================================================================
// THE CENSUS (spec section 3's "~60 cells"; the brief's grid, verbatim: N in
// {1000, 2000, 5000, 10000, 20000} x F7's two windows x the 5-way taxonomy,
// plus the two named degenerate cells).
// =============================================================================
//
// MAIN GRID: 5 N values x 2 windows x 5 taxonomies = 50 cells, EVERY
// combination present (no absences to comment here). N = 2000/path-interface
// is ALSO the named "N = 2000 wide-window stall"
// (docs/notes/2026-08-03-identification-stall-study.md Sec. 4/Sec. 9;
// scale-study-cold.md Sec. 4.2's own "status UNKNOWN" row) -- all 5 of its
// taxonomies are tagged `degenerate = true` rather than adding a duplicate
// cell, since the brief names the STALL, not a fifth taxonomy of it.
//
// PLUS the N = 800 named frozen cell
// (identification-stall-study.md Sec. 3.4's own committed row: cap 20000,
// kOptimal, 30165 minors, degen_run_max = 9365 -- the classical-cycling
// signature) at all 5 taxonomies, `degenerate = true` -- N = 800 is not in
// the brief's own N-grid, so these are ADDITIONAL cells, not a substitution.
//
// PLUS two HEALTHY CONTROLS flanking it, N = 750 and N = 825
// (identification-stall-study.md Sec. 3.4's own control rows: degen == 0 at
// both, one grid step either side of the stall) -- kNeutralCold only, since
// the study's own point about them is specifically the COLD walk's
// degenerate-vs-not signature, not the taxonomy census.
//
// TOTAL: 50 + 5 + 2 = 57 cells.
inline std::vector<CorpusCell> build_all_cells() {
    std::vector<CorpusCell> cells;
    cells.reserve(57);

    constexpr Index kMainGridN[] = {1000, 2000, 5000, 10000, 20000};
    constexpr ConstraintFamily kWindows[] = {ConstraintFamily::kBoundArc,
                                             ConstraintFamily::kPathInterface};
    constexpr StartTaxonomy kTaxonomies[] = {
        StartTaxonomy::kNeutralCold, StartTaxonomy::kPhysicsInformed, StartTaxonomy::kCorrupted,
        StartTaxonomy::kActivityOnly, StartTaxonomy::kFullWarm};

    for (const Index n_nodes : kMainGridN) {
        for (const ConstraintFamily window : kWindows) {
            const double p = window == ConstraintFamily::kBoundArc ? detail::kBoundArcP
                                                                   : detail::kPathInterfaceP;
            const double p0 = window == ConstraintFamily::kBoundArc ? detail::kBoundArcP0
                                                                    : detail::kPathInterfaceP0;
            const bool degenerate = (n_nodes == 2000 && window == ConstraintFamily::kPathInterface);
            for (const StartTaxonomy start : kTaxonomies) {
                // p0 is a real, distinct source point only for the two
                // taxonomies that actually hop from it (kCorrupted,
                // kFullWarm); the other three never read it, and it is set
                // equal to `p` for them purely so the field is never a
                // meaningless sentinel -- see the taxonomy note above.
                const bool uses_p0 =
                    start == StartTaxonomy::kCorrupted || start == StartTaxonomy::kFullWarm;
                const char *id = detail::pooled_id(fmt::format("f7_n{}_{}_{}", n_nodes,
                                                               detail::window_tag(window),
                                                               detail::taxonomy_tag(start)));
                cells.push_back(CorpusCell{id, BenchFamily::kF7, n_nodes, uses_p0 ? p0 : p, p,
                                           /*step_index=*/0, start, window, degenerate});
            }
        }
    }

    // The N = 800 frozen cell, all 5 taxonomies -- see the census note above.
    for (const StartTaxonomy start : kTaxonomies) {
        const bool uses_p0 =
            start == StartTaxonomy::kCorrupted || start == StartTaxonomy::kFullWarm;
        const char *id =
            detail::pooled_id(fmt::format("f7_n800_path_{}", detail::taxonomy_tag(start)));
        cells.push_back(CorpusCell{id, BenchFamily::kF7, /*n_nodes=*/800,
                                   uses_p0 ? detail::kPathInterfaceP0 : detail::kPathInterfaceP,
                                   detail::kPathInterfaceP, /*step_index=*/0, start,
                                   ConstraintFamily::kPathInterface, /*degenerate=*/true});
    }

    // The two healthy controls -- see the census note above.
    cells.push_back(CorpusCell{"f7_n750_path_neutral_control", BenchFamily::kF7,
                               /*n_nodes=*/750, detail::kPathInterfaceP, detail::kPathInterfaceP,
                               /*step_index=*/0, StartTaxonomy::kNeutralCold,
                               ConstraintFamily::kPathInterface, /*degenerate=*/false});
    cells.push_back(CorpusCell{"f7_n825_path_neutral_control", BenchFamily::kF7,
                               /*n_nodes=*/825, detail::kPathInterfaceP, detail::kPathInterfaceP,
                               /*step_index=*/0, StartTaxonomy::kNeutralCold,
                               ConstraintFamily::kPathInterface, /*degenerate=*/false});

    return cells;
}

inline const std::vector<CorpusCell> &all_cells() {
    static const std::vector<CorpusCell> cells = build_all_cells();
    return cells;
}

inline const CorpusCell *find_cell(const std::string &id) {
    for (const CorpusCell &c : all_cells()) {
        if (id == c.id) {
            return &c;
        }
    }
    return nullptr;
}

// =============================================================================
// THE WALK ENGINE'S IMPLEMENTATION OF THE INTERFACE
// =============================================================================

namespace detail {

inline SqpOptions corpus_options() {
    SqpOptions opts;
    opts.kkt_tol = kKktTol;
    opts.feas_tol = kFeasTol;
    opts.max_iter = kMajorMaxIter;
    opts.adaptive_mu = false; // bench_scale.cpp's own choice, so kHot stays reachable
    opts.warm_full_step = true;
    return opts;
}

// TASK 6's THREE LEVERS, and why they are a struct rather than three
// arguments. Two of the four measurement arms the reviews ordered move a
// SHIPPED OPTION (`ssn_prox_carry`) or select a KERNEL (`qp_mode`); a third
// (the uncertain-set ablation) cannot be reached from SqpOptions at all and is
// run as a patched-header recompile, exactly as Task 4's own ablation arms
// were. Bundling them means the runner, the tests and the arms all name the
// same object, and a future arm adds a field rather than a fourth positional
// argument to five functions.
//
// EVERY FIELD'S DEFAULT IS THE SHIPPED DEFAULT. A run that passes no lever is
// the product configuration, and bench_corpus.cpp stamps any non-default into
// the artifact's own provenance header (the same discipline the hidden
// wall-budget overrides already carry).
struct EngineConfig {
    QpMode qp_mode = QpMode::kWalk;
    bool ssn_prox_carry = false; // SqpOptions::ssn_prox_carry, default-false per Task 5
    // PHASE-7 TASK 6b PHASE B -- the research levers' corpus arms. Each is a
    // real SqpOptions/SsnOptions field shipping at the default below, so an
    // arm is a flag on the runner rather than a patched-header recompile, and
    // the provenance header records any non-default exactly as the two above.
    bool ssn_certify_from_face = false;                          // R5 (Gould's lemma)
    SsnSigmaRule ssn_sigma_rule = SsnSigmaRule::kLadder;         // R1
    SsnHintRule ssn_hint_rule = SsnHintRule::kIterationZeroFree; // R2
    SsnInfeasibilityRule ssn_infeasibility_rule = SsnInfeasibilityRule::kSymptoms; // R4
};

inline SqpOptions options_for_cell(const CorpusCell &cell, const EngineConfig &cfg = {}) {
    SqpOptions opts = corpus_options();
    if (cell.ctag == ConstraintFamily::kPathInterface) {
        opts.qp.max_iter = kPathInterfaceQpMaxIter;
    }
    opts.qp_mode = cfg.qp_mode;
    opts.ssn_prox_carry = cfg.ssn_prox_carry;
    opts.ssn_certify_from_face = cfg.ssn_certify_from_face;
    opts.ssn_sigma_rule = cfg.ssn_sigma_rule;
    opts.ssn_hint_rule = cfg.ssn_hint_rule;
    opts.ssn_infeasibility_rule = cfg.ssn_infeasibility_rule;
    return opts;
}

// The central-path synthesis Task 0's tests/test_warm_start.cpp
// (namespace ip_repair, f7_ip_iterate) already established for this exact
// family: s_j = mu/lambda_i*(j) on the analytically active rows, s_j =
// -cI_j(x*) on the rest, lambda_i(j) = mu/s_j everywhere. Reproduced here
// (not shared -- that namespace lives in a .cpp) because it is the ONLY
// route to a from_interior_point crossover this project has, and it is
// small enough that re-deriving it is safer than reaching across a test
// binary boundary.
struct IpIterate {
    Vec x, lambda_e, lambda_i, slack_i, z_lower, z_upper;
};

// `x` is the PRIMAL the hand-off carries; the duals/slacks are always built
// at the family's own analytic surface x*(p), which is what makes this an
// EXACT-ACTIVITY hint. FIX ROUND 1 (I2): the first issue of this task passed
// x = x*(p) as well, so every kActivityOnly cell started AT the analytic
// optimum and the driver certified optimality at x0 without ever building a
// QP -- 11 of 57 cells doing literally zero work by construction, six of them
// inside G1/G2's own population. A crossover hand-off is not supposed to be
// the answer; it is supposed to be a good activity guess at a nearby point,
// which is exactly what a real interior-point iterate x(mu) != x* is. The
// primal now comes from kPhysicsInformed's OWN rollout (x*(p) + the same
// deterministic kPhysicsInformedDisp * sin(i) displacement), so the taxonomy
// measures what its name promises: "how much identification does an exact
// activity hint save over the same physics-informed primal alone" -- the
// kPhysicsInformed cell at the same (N, window) is its matched control.
//
// The duals stay at x*: `from_interior_point` infers activity from the
// dual/slack pair ALONE (warm_start.h's ACTIVITY INFERENCE note -- it never
// re-evaluates the model at x), so displacing the primal leaves the hint
// exact while making the solve real.
inline IpIterate f7_ip_iterate(const F7CollocationChain &model, double p, double mu, const Vec &x) {
    const Vec x_star = model.x_star(p);
    const Vec lambda_i_star = model.lambda_i_star(p);
    const Vec ci_star = model.eval_ci(x_star);
    const auto analytic = model.active_set(p);

    IpIterate it;
    it.x = x;
    it.lambda_e = model.lambda_e_star(p);
    it.lambda_i = Vec::Zero(model.mi());
    it.slack_i = Vec::Zero(model.mi());
    for (Index j = 0; j < model.mi(); ++j) {
        const bool active = analytic.ineq_active[static_cast<std::size_t>(j)] != 0;
        const double s = active ? mu / lambda_i_star(j) : -ci_star(j);
        it.slack_i(j) = -s;
        it.lambda_i(j) = mu / s;
    }
    // F7's control box is inactive at the optimum for every p in the design
    // range (Task 0's own reading, warm_start.h/test_warm_start.cpp), so both
    // IP bound multipliers vanish.
    it.z_lower = Vec::Zero(model.n());
    it.z_upper = Vec::Zero(model.n());
    return it;
}

// kCorrupted's damage: drop the (foreign, untrustworthy) hot handle, then
// displace x by a deterministic, no-RNG sin-index pattern of magnitude
// kCorruptedXDisp -- see that constant's own note for why displacing x
// (rather than only the duals) is the recipe that is guaranteed to matter
// regardless of which of WarmStart's several redundant activity encodings
// (ineq_active/bound_active vs qp_working_set) a given ingest path actually
// reads: the geometric activity AT THE NEW x disagrees with the stale
// hand-off's own hint no matter which encoding is consulted.
inline WarmStart corrupt_warm_start(WarmStart warm) {
    warm.hot = nullptr;
    for (Index i = 0; i < warm.x.size(); ++i) {
        warm.x(i) += kCorruptedXDisp * std::sin(7.0 * static_cast<double>(i));
    }
    return warm;
}

inline F7CollocationChain make_model(const CorpusCell &cell) {
    return F7CollocationChain(cell.n_nodes, /*states=*/3, /*controls=*/2, cell.p, /*radius=*/1.0);
}

// The last history row's kkt_residual, or -1.0 (WarmStart's own "never
// populated" sentinel convention) on an empty history.
inline double last_kkt_residual(const SqpSolution &sol) {
    return sol.history.empty() ? -1.0 : sol.history.back().kkt_residual;
}

// TASK 6, instrument requirement 1 + the NF-1 telemetry. Recomputes the whole
// KKT quadruple FROM THE MODEL at the point the driver returned, and records
// the two scale denominators the pre-registered rule divides by. Called on
// EVERY row of EVERY engine -- the walk column is what calibrates the rule
// (W4), so it cannot be an ssn-only path.
//
// COST: one gradient and two Jacobian evaluations at the returned point, i.e.
// strictly less than one major iteration of the solve it is checking, at any
// N in this census. Nothing is factorized.
inline void record_kkt_check(const hven::solvers::NlpModel &model, const SqpSolution &sol,
                             double bound_tol, CorpusRow &row) {
    const NlpKktResidual r = self_check_kkt(model, sol, bound_tol);
    row.kkt_stationarity = r.stationarity;
    row.kkt_primal = r.primal;
    row.kkt_dual_sign = r.dual_sign;
    row.kkt_complementarity = r.complementarity;

    double dual_scale = 1.0;
    if (sol.lambda_e.size() > 0) {
        dual_scale = std::max(dual_scale, sol.lambda_e.template lpNorm<Eigen::Infinity>());
    }
    if (sol.lambda_i.size() > 0) {
        dual_scale = std::max(dual_scale, sol.lambda_i.template lpNorm<Eigen::Infinity>());
    }
    if (sol.z.size() > 0) {
        dual_scale = std::max(dual_scale, sol.z.template lpNorm<Eigen::Infinity>());
    }
    row.dual_scale = dual_scale;
    row.x_scale = sol.x.size() > 0 ? std::max(1.0, sol.x.template lpNorm<Eigen::Infinity>()) : 1.0;

    int negatives = 0;
    for (Index j = 0; j < sol.lambda_i.size(); ++j) {
        if (sol.lambda_i(j) < 0.0) {
            ++negatives;
        }
    }
    row.neg_ineq_duals = negatives;
}

inline CorpusRow row_from_solution(const CorpusCell &cell, const SqpSolution &sol, double wall_s) {
    CorpusRow row{};
    row.cell_id = cell.id;
    row.factorizations = static_cast<int>(sol.counters.factorizations);
    row.qp_minors = static_cast<int>(sol.counters.qp_minor_iters);
    // SUBPROBLEMS HANDED OFF TO THE WALK, off the driver's own aggregate.
    // Structurally 0 under qp_mode == kWalk (no SSN subproblem is ever solved,
    // so nothing can escape); Task 6 is what makes this field move.
    row.escapes = static_cast<int>(sol.counters.ssn.ssn_escapes);
    row.ssn = sol.counters.ssn;
    row.status = sol.status;
    row.kkt_residual = last_kkt_residual(sol);
    row.wall_s = wall_s;
    // C3: the PER-QP reading the gates name. `qp_factorizations` is
    // documented "meaningful iff qp_solved" (sqp_types.h), so a
    // stopped-AT-iterate row contributes nothing -- it built no subproblem.
    for (const hven::solvers::SqpIterate &it : sol.history) {
        if (it.qp_solved) {
            row.qp_factorizations.push_back(static_cast<int>(it.qp_factorizations));
        }
    }
    return row;
}

// Every solve this runner makes goes through here -- see kMinorBudget's own
// note for why. `warm` defaults to an invalid (default-constructed)
// WarmStart, which resolves kCold exactly as the 2-arg solve() overload
// does; passing a real one (kCorrupted/kActivityOnly/kFullWarm's own
// producers) is unaffected beyond gaining the same budget.
// `budget` defaults to the corpus's own kMinorBudget; overridable ONLY so
// tests/test_corpus_cells.cpp can prove the budget actually TRUNCATES a
// solve (a tiny explicit budget on a fixture that needs more than that many
// minors) without needing a fixture that burns through 50000 real minors to
// exercise the same code path. Every call site in run_cell_walk below uses
// the default.
inline SqpSolution budgeted_solve(SqpDriver &driver, const hven::solvers::NlpModel &model,
                                  const Vec &x0, const WarmStart &warm = WarmStart{},
                                  Index budget = kMinorBudget) {
    return driver.solve(model, x0, warm, budget);
}

// kPhysicsInformed's own rollout: the family's analytic optimum displaced by
// a deterministic, no-RNG sin-index pattern. Shared with kActivityOnly, which
// uses it as its PRIMAL (see f7_ip_iterate's own note, I2).
inline Vec physics_informed_start(const F7CollocationChain &model, double p) {
    Vec x0 = model.x_star(p);
    for (Index i = 0; i < x0.size(); ++i) {
        x0(i) += kPhysicsInformedDisp * std::sin(static_cast<double>(i));
    }
    return x0;
}

// Invoked (once, by every taxonomy) at the instant the cell's SETUP work is
// done and the DESIGNATED solve is about to begin. Empty by default; the
// runner passes one to restart its wall deadline -- see the file banner's
// I1 note.
using SetupCompleteFn = std::function<void()>;

inline void notify_setup_complete(const SetupCompleteFn &fn) {
    if (fn) {
        fn();
    }
}

// THE KKT GATE IS OUTSIDE THE TIMED WINDOW, deliberately: `wall_s` has to stay
// comparable to the walk baseline this task scores against, and that baseline
// was measured by a binary with no such check in it. The check is charged to
// nobody's wall.
template <typename Fn>
CorpusRow timed_row(const CorpusCell &cell, const hven::solvers::NlpModel &model,
                    const SetupCompleteFn &on_setup_complete, Fn &&solve_target) {
    notify_setup_complete(on_setup_complete);
    const auto t0 = std::chrono::steady_clock::now();
    const SqpSolution sol = solve_target();
    const auto t1 = std::chrono::steady_clock::now();
    CorpusRow row = row_from_solution(cell, sol, std::chrono::duration<double>(t1 - t0).count());
    record_kkt_check(model, sol, kFeasTol, row);
    return row;
}

// THE ENGINE INTERFACE'S IMPLEMENTATION, both kernels. `cfg.qp_mode` is the
// ONLY thing that differs between the walk arm and the SSN arm: the same
// generators, the same starts, the same budgets, the same driver, the same
// number of solves. That is what makes the two CSV columns a comparison rather
// than two studies.
inline CorpusRow run_cell_engine(const CorpusCell &cell, const EngineConfig &cfg,
                                 const SetupCompleteFn &on_setup_complete = {}) {
    F7CollocationChain model = make_model(cell);
    const SqpOptions opts = options_for_cell(cell, cfg);

    switch (cell.start) {
    case StartTaxonomy::kNeutralCold: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = model.start_point();
        return timed_row(cell, model, on_setup_complete,
                         [&] { return budgeted_solve(driver, model, x0); });
    }
    case StartTaxonomy::kPhysicsInformed: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = physics_informed_start(model, cell.p);
        return timed_row(cell, model, on_setup_complete,
                         [&] { return budgeted_solve(driver, model, x0); });
    }
    case StartTaxonomy::kCorrupted: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p0));
        // The SETUP hop is budgeted too, not just the reported target solve
        // -- an unbounded setup step would hang the runner exactly as an
        // unbounded target step would, and the row never reports it either
        // way (see the file banner). A budget-truncated seed is still a
        // valid (if less converged) WarmStart to feed forward -- every exit
        // of solve() is "safe to feed forward" by this project's own
        // contract (warm_start.h). It is bounded by its OWN wall deadline
        // too, and a setup that exhausts it is reported as `dnf_setup`, not
        // as this taxonomy's own hand-off failing (I1).
        const SqpSolution seed = budgeted_solve(driver, model, model.start_point());
        const WarmStart corrupted = corrupt_warm_start(seed.warm_start);
        model.set_parameters(Vec::Constant(1, cell.p));
        return timed_row(cell, model, on_setup_complete,
                         [&] { return budgeted_solve(driver, model, corrupted.x, corrupted); });
    }
    case StartTaxonomy::kActivityOnly: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p));
        const double mu = crossover_mu_for_n(cell.n_nodes);
        const IpIterate it =
            f7_ip_iterate(model, cell.p, mu, physics_informed_start(model, cell.p));
        const WarmStart crossover =
            from_interior_point(it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower, it.z_upper,
                                model.lower(), model.upper(), IpCrossoverOptions{});
        return timed_row(cell, model, on_setup_complete,
                         [&] { return budgeted_solve(driver, model, crossover.x, crossover); });
    }
    case StartTaxonomy::kFullWarm: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p0));
        const SqpSolution seed = budgeted_solve(driver, model, model.start_point());
        model.set_parameters(Vec::Constant(1, cell.p));
        return timed_row(cell, model, on_setup_complete, [&] {
            return budgeted_solve(driver, model, seed.warm_start.x, seed.warm_start);
        });
    }
    }
    throw std::invalid_argument(fmt::format(
        "corpus::run_cell_engine: cell '{}' carries an unrecognised StartTaxonomy", cell.id));
}

// =============================================================================
// PHASE-7 TASK 2 (PIQP acquisition oracle). THE CELL'S OWN FIRST QP,
// UNSOLVED. Mirrors run_cell_walk's per-taxonomy setup EXACTLY -- same p0
// hop, same corruption/crossover construction -- up to the point where that
// function calls budgeted_solve on the DESIGNATED (target) hop; this returns
// the QpProblem that call would build as its own first subproblem, instead
// of solving it. This is bit-for-bit what SqpDriver::solve's first iteration
// builds for the same (x, lambda_e, lambda_i) at obj_scale = 1
// (hven::solvers::build_subproblem, sqp_driver.h) -- the driver's own first call
// is exactly this one, so a caller replaying this QP through an external
// solver is replaying the SAME subproblem the walk baseline's own first
// major iteration solved, not a re-derived approximation of it.
//
// `on_setup_complete` fires at the SAME seam run_cell_walk's does (right
// before the designated hop's own first evaluation) -- reused as-is so a
// caller wrapping this in the runner's own wall-deadline machinery (or, for
// bench/bench_corpus.cpp's --dump-qp, a shell-level `timeout`) can restart
// its clock exactly where run_cell_walk's own DNF-phase attribution does.
//
// kCorrupted/kFullWarm still pay the REAL setup hop -- a genuine cold solve
// at p0 -- because that is what produces the (x, lambda_e, lambda_i) their
// designated hop's first QP is actually built from; there is no shortcut
// that skips it without dumping a different QP than the one being asked for.
inline QpProblem first_qp_for_cell(const CorpusCell &cell,
                                   const SetupCompleteFn &on_setup_complete = {}) {
    F7CollocationChain model = make_model(cell);
    const SqpOptions opts = options_for_cell(cell);

    auto first_qp_at = [&](const Vec &x, const Vec &lambda_e, const Vec &lambda_i) {
        notify_setup_complete(on_setup_complete);
        return hven::solvers::build_subproblem(model, x, lambda_e, lambda_i);
    };

    switch (cell.start) {
    case StartTaxonomy::kNeutralCold: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = model.start_point();
        return first_qp_at(x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    }
    case StartTaxonomy::kPhysicsInformed: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const Vec x0 = physics_informed_start(model, cell.p);
        return first_qp_at(x0, Vec::Zero(model.me()), Vec::Zero(model.mi()));
    }
    case StartTaxonomy::kCorrupted: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p0));
        const SqpSolution seed = budgeted_solve(driver, model, model.start_point());
        const WarmStart corrupted = corrupt_warm_start(seed.warm_start);
        model.set_parameters(Vec::Constant(1, cell.p));
        return first_qp_at(corrupted.x, corrupted.lambda_e, corrupted.lambda_i);
    }
    case StartTaxonomy::kActivityOnly: {
        model.set_parameters(Vec::Constant(1, cell.p));
        const double mu = crossover_mu_for_n(cell.n_nodes);
        const IpIterate it =
            f7_ip_iterate(model, cell.p, mu, physics_informed_start(model, cell.p));
        const WarmStart crossover =
            from_interior_point(it.x, it.lambda_e, it.lambda_i, it.slack_i, it.z_lower, it.z_upper,
                                model.lower(), model.upper(), IpCrossoverOptions{});
        return first_qp_at(crossover.x, crossover.lambda_e, crossover.lambda_i);
    }
    case StartTaxonomy::kFullWarm: {
        SqpDriver driver(opts);
        model.set_parameters(Vec::Constant(1, cell.p0));
        const SqpSolution seed = budgeted_solve(driver, model, model.start_point());
        model.set_parameters(Vec::Constant(1, cell.p));
        return first_qp_at(seed.warm_start.x, seed.warm_start.lambda_e, seed.warm_start.lambda_i);
    }
    }
    throw std::invalid_argument(fmt::format(
        "corpus::first_qp_for_cell: cell '{}' carries an unrecognised StartTaxonomy", cell.id));
}

} // namespace detail

// The engine interface's one entry point: `(cell, engine) -> CorpusRow`.
// `engine` is "walk" or "ssn" (bench_corpus.cpp's own --engine values).
//
// PHASE-7 TASK 6 WIRED "ssn" UP. Task 1 shipped this function throwing on that
// name because `SqpOptions::qp_mode = kSsn` did not exist; Task 3 landed the
// enumerator, Task 5 landed the driver dispatch, and this task is the one that
// replays the census against it. The two arms differ in EXACTLY the one field
// (`EngineConfig::qp_mode`) -- see `run_cell_engine`.
//
// T6: an unrecognised engine name throws std::invalid_argument with the reason
// folded into the message; tests/test_corpus_cells.cpp pins it.
inline CorpusRow run_cell(const CorpusCell &cell, const std::string &engine,
                          const detail::SetupCompleteFn &on_setup_complete = {},
                          const detail::EngineConfig &levers = {}) {
    detail::EngineConfig cfg = levers;
    if (engine == "walk") {
        cfg.qp_mode = QpMode::kWalk;
        return detail::run_cell_engine(cell, cfg, on_setup_complete);
    }
    if (engine == "ssn") {
        cfg.qp_mode = QpMode::kSsn;
        return detail::run_cell_engine(cell, cfg, on_setup_complete);
    }
    throw std::invalid_argument(
        fmt::format("run_cell: '{}' is not a known engine (expected walk|ssn)", engine));
}

} // namespace hven::solvers::corpus
