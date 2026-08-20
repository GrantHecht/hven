// tests/sqp/test_hs_battery.cpp — Task 11: the Hock-Schittkowski battery, the
// first WHOLE-SOLVER validation in this project. Every driver mechanism that
// Tasks 4-10 landed (funnel trust region, second-order correction, elastic
// tier, restoration phase, adaptive dual regularization) runs together, from
// each problem's PUBLISHED start point, over the 27 problems
// tests/sqp/support/hs_problems.h ships.
//
// WHAT THIS FILE ASSERTS, AND IN WHAT ORDER OF AUTHORITY:
//
//   1. THE KKT SELF-CHECK (support/nlp_kkt_check.h) is the primary guard. It
//      is recomputed FROM THE MODEL at the returned point -- gradients,
//      Jacobians, constraint values, bounds -- and never reads the driver's
//      own residual measurement, so a driver that mismeasured cannot certify
//      itself. Every kOptimal exit in the tables below must pass it.
//   2. THE CITED f* is a cross-check on top of the self-check, not a
//      substitute for it: a transcription that is self-consistently WRONG
//      passes the self-check (it is a KKT point of the wrong problem) and
//      it also passes the derivative checker (its own derivatives are
//      internally consistent). Only f* catches it. The canonical shape is a
//      wrong EXPONENT -- HS77's (x3-1)^2 vs (x3-1)^3 differ by 0.09 in the
//      objective at the published x* and by nothing at all in any derivative
//      identity, so f* is the only guard that can separate them, and
//      hs_problems.h's HS77 note records that arithmetic.
//
//      NO TRANSCRIPTION ERROR WAS ACTUALLY CAUGHT by either guard: every
//      formula in hs_problems.h was right on its first pass and every cited
//      port agrees with every other. The guards are here because they are
//      cheap and their failure modes are disjoint, not because either has a
//      scalp.
//   3. THE COUNTERS are pinned as OBSERVED VALUES with headroom, per this
//      suite's house style. A budget is a regression tripwire, not a claim
//      that the exact count is correct.
//
// THREE PROBLEMS ARE EXCUSED, and an excused problem is never skipped: its
// ACTUAL observed behaviour (status, point, objective, counters) is pinned
// exactly as a clean problem's is, so a regression still fails the suite. Only
// the "f == the cited GLOBAL f*" claim is dropped, and only with the failure
// mode analysed AND an issue-style entry carried in
// docs/notes/2026-07-29-hs-battery-results.md:
//
//     HS15 -> open item O-6 (local minimum of a disconnected branch)
//     HS25 -> open item O-7 (start point is a numerically exact stationary
//                            point; f* = 0 is below the noise floor)
//     HS33 -> open item O-8 (local solution at a corner of the box)
//
// All three are LOCAL-SOLUTION or NOISE-FLOOR outcomes rather than solver
// defects, and each open entry records what would close it.
//
// THE TWO ws_algebra MODES NOW AGREE ON EVERY PROBLEM'S STATUS. They did not
// when this file was written: HS26 was solved to f* in kRefactorize mode and
// reported kInfeasible in kSchurBorder mode -- the DEFAULT mode -- because its
// very first subproblem, feasible and bounded, came back kInfeasible from the
// border path. Task 11b found the cause (one step of iterative refinement is
// not enough when the border stack is what makes K0 invertible; see
// bordered_eqp.h's LOAD-BEARING BORDERS note) and fixed it, which is why the
// two tables below are now identical and the divergence set is empty.
// BorderModeFalseInfeasible keeps the reduced 3-variable QP as a regression
// test.
//
// BATTERY RUNTIME: measured ~80 ms for both modes over all 27 problems
// (Release), far inside the brief's 30 s budget. max_iter is bounded per
// problem so that a future regression that traps a problem costs a bounded
// number of subproblems rather than the whole budget.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>

#include "support/hs_problems.h"
#include "support/nlp_kkt_check.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::test_support::hs_numbers;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;
using hven::solvers::test_support::NlpKktResidual;
using hven::solvers::test_support::self_check_kkt;

namespace {

// The brief's convergence bar: f within 1e-6 RELATIVE of the target, with an
// absolute floor of 1 so that a target of 0 (HS1, HS26, HS28, HS38 ...) is
// held to 1e-6 outright rather than to nothing.
constexpr double kFRelTol = 1e-6;

// The bar the in-test KKT self-check must clear at a kOptimal exit. Note that
// stationarity is held to 1e-6 -- SqpOptions::kkt_tol -- and NOT tighter: the
// self-check re-derives the residual from the model, so it is allowed to be
// slightly worse than the driver's own measurement (different active-set
// rounding at a weakly-active row), and demanding better would be asserting
// about arithmetic noise. Observed worst over the whole battery: 4.77e-07,
// on HS30, whose optimal active set is genuinely degenerate.
constexpr double kSelfCheckStationarity = 1e-6;
constexpr double kSelfCheckPrimal = 1e-6;
constexpr double kSelfCheckDualSign = 1e-9;
constexpr double kSelfCheckComplementarity = 1e-6;

// One row of the battery's expectation table.
//
// `excuse` IS THE WHOLE DIFFERENCE between a clean problem and an excused
// one. When it is null, `f_target` is the CITED f* carried by
// make_hs(number).f_star and the row asserts the brief's bar. When it is set,
// `f_target` is the OBSERVED objective at the point this solver actually
// reaches, the row still asserts everything else (status, KKT self-check,
// budget), and the string is the one-line reason -- expanded in the results
// note, which is the excuse's real home.
struct Expect {
    int number;
    SqpStatus status;
    Index max_iter;     // per-problem budget passed to SqpOptions
    Index major_budget; // observed majors + headroom
    double f_target;    // NaN => use the cited f*
    const char *excuse; // null => not excused; f_target must be the cited f*
};

constexpr double kUseCitedFStar = std::numeric_limits<double>::quiet_NaN();

// ---------------------------------------------------------------------
// THE TABLE. Every `major_budget` below is the OBSERVED major count in that
// mode, rounded up with roughly 30% headroom; every `f_target` on an excused
// row is the OBSERVED objective to the digits the solver reproduces run to
// run. Observed majors, kSchurBorder mode, in table order:
//   HS1 29, HS3 4, HS5 4, HS6 8, HS7 9, HS10 14, HS11 7, HS12 6, HS14 5,
//   HS15 7, HS22 2, HS24 3, HS25 0, HS26 17, HS27 4, HS28 3, HS30 11,
//   HS33 4, HS35 1, HS38 49, HS39 9, HS40 4, HS43 7, HS45 3, HS76 2,
//   HS77 12, HS79 4.
// kRefactorize mode reproduces every one of these, HS26 included (Task 11b).
// ---------------------------------------------------------------------
std::vector<Expect> border_table() {
    return {
        {1, SqpStatus::kOptimal, 60, 40, kUseCitedFStar, nullptr},
        {3, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        {5, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        {6, SqpStatus::kOptimal, 60, 15, kUseCitedFStar, nullptr},
        {7, SqpStatus::kOptimal, 60, 15, kUseCitedFStar, nullptr},
        {10, SqpStatus::kOptimal, 60, 20, kUseCitedFStar, nullptr},
        {11, SqpStatus::kOptimal, 60, 12, kUseCitedFStar, nullptr},
        {12, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        {14, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        // EXCUSED -- see the results note (open item O-6). HS15's feasible set
        // is DISCONNECTED (x1 x2 >= 1 with x1 <= 0.5 admits x1 in (0, 0.5]
        // with x2 >= 2, and x1 < 0 with x2 <= 1/x1 < 0, with nothing in
        // between -- x1 = 0 satisfies neither), and the published start point
        // (-2, 1) is in the NEGATIVE branch. The global f* = 306.5 lives in
        // the other one, and no continuous path connects them.
        //
        // THE POINT REACHED IS EXCUSED ON THIS BATTERY'S OWN KKT EVIDENCE,
        // NOT ON A CITATION: f = 360.3797671705 at
        // x = (-0.7921232205, -1.2624298520) with the self-check clean
        // (stationarity 1.7e-10). No published source quoting 360.379 for
        // HS15 was located, so "a local minimum of the branch it starts in"
        // is a statement this suite VERIFIES rather than one it cites --
        // which is sufficient, because the excusal only needs the returned
        // point to be a genuine KKT point that a local method must reach
        // from that start.
        {15, SqpStatus::kOptimal, 60, 12, 360.37976717049241,
         "converges to the local minimum of the disconnected branch its published start "
         "point lies in; the global f*=306.5 is in the other branch"},
        {22, SqpStatus::kOptimal, 60, 8, kUseCitedFStar, nullptr},
        {24, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        // EXCUSED -- the battery's deliberate trap (open item O-7); see
        // hs_problems.h's HS25 note for the arithmetic. Every exponential is
        // dead at the published
        // start point, so f is a CONSTANT 32.835 to fourteen digits there and
        // its gradient (~1e-11) is two orders BELOW the double-precision noise
        // floor of f itself. The driver correctly certifies a KKT point in
        // ZERO majors; the true minimum 0 is unreachable by any method that
        // reads f in IEEE double without rescaling the problem first.
        {25, SqpStatus::kOptimal, 60, 0, 32.834999999663594,
         "start point is a numerically exact stationary point: |grad f| ~ 1e-11 sits two "
         "orders below f's own roundoff floor, so f* = 0 is unreachable without rescaling"},
        // THE TASK 11b ROW. This was the battery's one both-mode divergence:
        // border mode got kInfeasible from the FIRST subproblem -- feasible,
        // bounded and convex -- routed it into the elastic tier, exhausted the
        // tier and stopped at the start point with f = 21.16, while
        // kRefactorize solved it to f* in 17 majors. Both modes now reach f*
        // in 17, and BorderModeFalseInfeasible keeps the reduced QP pinned.
        {26, SqpStatus::kOptimal, 60, 25, kUseCitedFStar, nullptr},
        {27, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        {28, SqpStatus::kOptimal, 60, 8, kUseCitedFStar, nullptr},
        {30, SqpStatus::kOptimal, 60, 16, kUseCitedFStar, nullptr},
        // EXCUSED (open item O-8) -- HS33 has a LOCAL solution at (0, 0, 2)
        // with f = -4, verified here as a KKT point by hand (see the results
        // note), and the published start point (0, 0, 3) is already sitting
        // on the two bounds that define it: x1 = x2 = 0 are both at their
        // lower bounds there, so the solve only has x3 left to move and slides
        // it down the ci2 sphere to 2. Reaching the global (0, sqrt2, sqrt2)
        // requires LEAVING a bound that no descent direction asks it to leave.
        // KKT self-check clean at the returned point.
        {33, SqpStatus::kOptimal, 60, 10, -3.9999999999737859,
         "converges to a local solution (0,0,2), f = -4, verified here as a KKT point; the "
         "published start point sits on both bounds that define it"},
        {35, SqpStatus::kOptimal, 60, 5, kUseCitedFStar, nullptr},
        {38, SqpStatus::kOptimal, 120, 70, kUseCitedFStar, nullptr},
        {39, SqpStatus::kOptimal, 60, 15, kUseCitedFStar, nullptr},
        {40, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
        {43, SqpStatus::kOptimal, 60, 12, kUseCitedFStar, nullptr},
        {45, SqpStatus::kOptimal, 60, 8, kUseCitedFStar, nullptr},
        {76, SqpStatus::kOptimal, 60, 8, kUseCitedFStar, nullptr},
        {77, SqpStatus::kOptimal, 60, 18, kUseCitedFStar, nullptr},
        {79, SqpStatus::kOptimal, 60, 10, kUseCitedFStar, nullptr},
    };
}

// kRefactorize expects EXACTLY what kSchurBorder does -- there is currently no
// divergence between the two modes at all (Task 11b closed the last one, HS26).
// The function is kept as a copy-with-patch rather than collapsed into an alias
// so that a future mode-specific expectation has an obvious, single place to be
// written down, spelled out in code rather than left implicit.
std::vector<Expect> refactorize_table() {
    std::vector<Expect> t = border_table();
    // (no patches: the two modes agree on every row)
    return t;
}

SqpOptions options_for(const Expect &e, WorkingSetLinearAlgebra alg) {
    SqpOptions opts;
    opts.max_iter = e.max_iter;
    opts.qp.ws_algebra = alg;
    return opts;
}

const char *status_name(SqpStatus s) {
    switch (s) {
    case SqpStatus::kOptimal:
        return "kOptimal";
    case SqpStatus::kMaxIter:
        return "kMaxIter";
    case SqpStatus::kInfeasible:
        return "kInfeasible";
    case SqpStatus::kNumericalError:
        return "kNumericalError";
    case SqpStatus::kBudgetExhausted:
        return "kBudgetExhausted";
    }
    return "?";
}

// Records the per-problem row of the results note's table onto the gtest XML,
// so a failing run on another machine reports the outcome that produced it.
void record_row(const Expect &e, const char *tag, const SqpSolution &sol,
                const NlpKktResidual &chk) {
    ::testing::Test::RecordProperty(
        fmt::format("hs{}_{}", e.number, tag),
        fmt::format("status={} f={:.12g} maj={} minor={} fact={} acc={} rej={} soc={} el={} "
                    "esc={} rest={} stat={:.3g} prim={:.3g} compl={:.3g}",
                    status_name(sol.status), sol.f, sol.counters.major_iters,
                    sol.counters.qp_minor_iters, sol.counters.factorizations,
                    sol.counters.steps_accepted, sol.counters.rejected_steps,
                    sol.counters.soc_steps, sol.counters.elastic_activations,
                    sol.counters.elastic_escalations, sol.counters.restoration_iters,
                    chk.stationarity, chk.primal, chk.complementarity));
}

// Runs one table entry and asserts everything the battery claims about it.
void check_one(const Expect &e, WorkingSetLinearAlgebra alg, const char *tag) {
    SCOPED_TRACE(::testing::Message() << "HS" << e.number << " (" << tag << " mode)");
    // THE Expect EXCUSAL INVARIANT, ENFORCED: a null excuse must pair with
    // kUseCitedFStar (NaN) -- the row asserts the CITED f* -- and a non-null
    // excuse must pair with an actual OBSERVED f_target. Without this, a row
    // that sets `excuse` but forgets to also replace kUseCitedFStar would
    // silently keep asserting the cited value and never surface as the
    // excused row it claims to be; the reverse (an excuse-free row with a
    // NaN-valued but non-cited target) is equally a silent contract break.
    ASSERT_EQ(e.excuse == nullptr, std::isnan(e.f_target))
        << "HS" << e.number
        << ": Expect's excuse/f_target pairing is broken -- see the excusal contract above";
    const HsProblem p = make_hs(e.number);
    const SqpOptions opts = options_for(e, alg);
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    const NlpKktResidual chk = self_check_kkt(*p.model, sol, opts.feas_tol);
    record_row(e, tag, sol, chk);

    ASSERT_EQ(sol.status, e.status)
        << "expected " << status_name(e.status) << ", got " << status_name(sol.status)
        << (e.excuse ? fmt::format(" [excused: {}]", e.excuse) : std::string());
    EXPECT_LE(sol.counters.major_iters, e.major_budget);

    const double target = std::isnan(e.f_target) ? p.f_star : e.f_target;
    EXPECT_LE(std::abs(sol.f - target), kFRelTol * std::max(1.0, std::abs(target)))
        << "f = " << sol.f << " vs target " << target
        << (e.excuse ? " (EXCUSED target, not the cited f*)" : " (cited f*)")
        << "; source: " << p.source;

    // THE KKT SELF-CHECK, and it applies at a kOptimal exit only. Every row in
    // these tables is kOptimal today (Task 11b closed the one that was not), so
    // the guard is currently unconditional in effect; it is kept conditional
    // because a row that legitimately exits kInfeasible or kMaxIter returns the
    // iterate the loop stopped at, and asserting a clean KKT quadruple there
    // would be asserting away exactly what the status is reporting.
    if (sol.status == SqpStatus::kOptimal) {
        EXPECT_LT(chk.stationarity, kSelfCheckStationarity);
        EXPECT_LT(chk.primal, kSelfCheckPrimal);
        EXPECT_LT(chk.dual_sign, kSelfCheckDualSign);
        EXPECT_LT(chk.complementarity, kSelfCheckComplementarity);
    }

    // Counter invariants that hold on EVERY row regardless of outcome. These
    // are the sqp_types.h contracts, re-checked once per battery problem
    // rather than only on the handful of fixtures that motivated each one.
    EXPECT_GE(sol.counters.qp_minor_iters, 0);
    EXPECT_EQ(sol.counters.major_iters,
              sol.counters.steps_accepted + sol.counters.rejected_steps +
                  static_cast<Index>(std::count_if(sol.history.begin(), sol.history.end(),
                                                   [](const SqpIterate &h) {
                                                       return h.qp_solved &&
                                                              h.verdict == StepVerdict::kRestore;
                                                   })))
        << "SqpCounters' major_iters = accepted + rejected + kRestore rows identity";
    for (const SqpIterate &h : sol.history) {
        if (h.qp_solved && !h.elastic_applied) {
            // The trust region is a hard box on the step (the elastic tier
            // gets its radius as real bounds instead, so it is exempt -- see
            // SqpIterate::elastic_applied).
            EXPECT_LE(h.step_norm, h.tr_radius * (1.0 + 1e-9))
                << "trial " << h.trial << " exceeded its radius";
        }
    }
}

// Solves every problem in one mode and returns the statuses, for the
// divergence comparison.
std::vector<SqpStatus> statuses_in(WorkingSetLinearAlgebra alg, const std::vector<Expect> &table) {
    std::vector<SqpStatus> out;
    for (const Expect &e : table) {
        const HsProblem p = make_hs(e.number);
        SqpDriver driver(options_for(e, alg));
        out.push_back(driver.solve(*p.model).status);
    }
    return out;
}

} // namespace

// =========================================================================
// The battery proper, once per ws_algebra mode.
// =========================================================================

TEST(HsBattery, BorderMode) {
    const auto t0 = std::chrono::steady_clock::now();
    for (const Expect &e : border_table()) {
        check_one(e, WorkingSetLinearAlgebra::kSchurBorder, "border");
    }
    const double secs =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    ::testing::Test::RecordProperty("battery_border_seconds", fmt::format("{:.3f}", secs));
    // The brief's runtime budget, asserted rather than assumed. Observed
    // ~0.04 s in Release and ~0.5 s in Debug; 30 s is three orders of
    // headroom, so this trips only on a genuine trap, which is what it is for.
    EXPECT_LT(secs, 30.0) << "battery must stay inside its 30 s budget";
}

TEST(HsBattery, RefactorizeMode) {
    for (const Expect &e : refactorize_table()) {
        check_one(e, WorkingSetLinearAlgebra::kRefactorize, "refactorize");
    }
}

// =========================================================================
// Mode divergence (carry item from Task 9: run the WHOLE battery in BOTH
// modes and record any STATUS divergence per problem).
// =========================================================================

// THE DIVERGENCE SET IS ASSERTED TO BE EMPTY, which is stronger than it looks
// and is the reason this test is written as a set comparison rather than a
// per-row EXPECT: it pins the set EXACTLY in both directions, so a NEW
// divergence in any of the 27 problems fails here rather than appearing
// silently, and so does a stale expectation left behind by a fix.
//
// It was {HS26} until Task 11b: the border path's false kInfeasible on HS26's
// first subproblem was this battery's sole both-mode status divergence, and
// closing it emptied the set. BorderModeFalseInfeasible below is the reduced
// regression test for that fix.
TEST(HsBattery, ModeStatusDivergenceIsEmpty) {
    const std::vector<Expect> bt = border_table();
    const std::vector<SqpStatus> border = statuses_in(WorkingSetLinearAlgebra::kSchurBorder, bt);
    const std::vector<SqpStatus> refac = statuses_in(WorkingSetLinearAlgebra::kRefactorize, bt);
    ASSERT_EQ(border.size(), refac.size());

    std::vector<int> diverged;
    for (std::size_t k = 0; k < border.size(); ++k) {
        if (border[k] != refac[k]) {
            diverged.push_back(bt[k].number);
            ::testing::Test::RecordProperty(fmt::format("divergence_hs{}", bt[k].number),
                                            fmt::format("border={} refactorize={}",
                                                        status_name(border[k]),
                                                        status_name(refac[k])));
        }
    }
    EXPECT_EQ(diverged, std::vector<int>{})
        << "the border/refactorize status divergence set changed; see "
           "docs/notes/2026-07-29-hs-battery-results.md";
}

// TASK 11b'S REGRESSION TEST -- the bug that was, reduced away from the driver
// entirely: HS26's FIRST subproblem, transcribed by hand from the model at
// x0 = (-2.6, 2, 2) with lambda_e = 0 and the default radius 1.
//
//   min  0.5 p^T H p + g^T p   s.t.  Ae p = 0,  |p|inf <= 1
//   H  = [[2,-2,0],[-2,2,0],[0,0,0]]   (PSD, rank 1)
//   g  = (-9.2, 9.2, 0)
//   Ae = [5, -10.4, 32],  be = 0
//
// p = 0 IS FEASIBLE, so the QP cannot be infeasible; H is positive
// SEMIdefinite and the box is compact, so it cannot be unbounded either. The
// answer is kOptimal at p = (1, -1, -0.48125), which satisfies Ae p = 0
// exactly (5 + 10.4 - 32*0.48125 = 0).
//
// HISTORY. kRefactorize always returned that. kSchurBorder -- the DEFAULT --
// returned kInfeasible from p = (1, -1, -0.481522163), 8.7e-3 off the single
// equality row: it stopped on an iterate that does not satisfy the row and
// called that infeasibility. This test pinned the wrong behaviour until the
// cause was found.
//
// THE CAUSE, for the record, because Task 11's first guess was wrong and the
// wrong guess is instructive. It is NOT pivot spread across the H block and it
// is NOT a scaling problem of the kind Task 8's elastic sigma-scaling fixed.
// It is that the border stack is LOAD-BEARING for K0's invertibility here:
// null(H) and null(Ae) intersect along (32, 32, 5.4), so the seed K0 -- built
// from the EMPTY working set, before either bound is pinned -- is EXACTLY
// singular (sigma_min = 3e-16 unregularized) and only primal_delta = 1e-8
// makes it factorizable at all. The two pins that resolve it arrive as
// borders, so the Schur form computes ||K0^-1 v|| ~ 7e7 and cancels it back
// down to an O(1) answer, losing the first digit; the ONE step of iterative
// refinement the bordered solve used to take recovered only two of the eight
// lost digits (contraction ~0.018/step), leaving the 8.7e-3 above.
// bordered_eqp.h's refine_bordered_solve_iterated now keeps stepping while the
// residual is above the regularization footprint, which converges it.
TEST(HsBattery, BorderModeFalseInfeasible) {
    QpProblem qp;
    qp.g = (Vec(3) << -9.2, 9.2, 0.0).finished();
    {
        SpMatRM H(3, 3);
        const std::vector<Eigen::Triplet<double>> t = {{0, 0, 2.0}, {0, 1, -2.0}, {0, 2, 0.0},
                                                       {1, 1, 2.0}, {1, 2, 0.0},  {2, 2, 0.0}};
        H.setFromTriplets(t.begin(), t.end());
        qp.H = H;
    }
    {
        Eigen::SparseMatrix<double, Eigen::RowMajor> Ae(1, 3);
        const std::vector<Eigen::Triplet<double>> a = {{0, 0, 5.0}, {0, 1, -10.4}, {0, 2, 32.0}};
        Ae.setFromTriplets(a.begin(), a.end());
        qp.Ae = Ae;
    }
    qp.be = Vec::Zero(1);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    qp.validate();

    QpOptions refac_opts;
    refac_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    refac_opts.tr_radius = 1.0;
    QpEngine refac(refac_opts);
    const QpSolution good = refac.solve(qp);

    QpOptions border_opts;
    border_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    border_opts.tr_radius = 1.0;
    QpEngine border(border_opts);
    const QpSolution bor = border.solve(qp);

    // THE ORACLE HALF: kRefactorize is this project's equivalence oracle
    // (qp_types.h's WorkingSetLinearAlgebra note), and it gets this right.
    ASSERT_EQ(good.status, QpStatus::kOptimal);
    EXPECT_LT(std::abs((qp.Ae * good.x)(0) - qp.be(0)), 1e-12)
        << "the oracle's answer must satisfy the one equality row";
    EXPECT_LE(good.x.lpNorm<Eigen::Infinity>(), 1.0 + 1e-12);

    // THE FORMERLY-BUGGY HALF. Both modes must now agree on the status, on the
    // vertex, and on the answer.
    const Vec expected = (Vec(3) << 1.0, -1.0, -0.48125).finished();
    ASSERT_EQ(bor.status, QpStatus::kOptimal)
        << "border mode must not report infeasibility on a QP that p = 0 satisfies";
    EXPECT_LT((good.x - expected).lpNorm<Eigen::Infinity>(), 1e-12);
    EXPECT_LT((bor.x - expected).lpNorm<Eigen::Infinity>(), 1e-8)
        << "border mode's answer, x = " << bor.x.transpose();
    EXPECT_LE(bor.x.lpNorm<Eigen::Infinity>(), 1.0 + 1e-12);

    // THE ROW RESIDUAL is the quantity the engine's infeasibility test reads,
    // and it is what was 8.7e-3. The bound asserted here is 1e-6 rather than
    // the oracle's 1e-12 ON PURPOSE and is not slack for its own sake: the
    // bordered refinement stops once the unregularized residual falls below the
    // regularization footprint it exists to remove (bordered_eqp.h), which on
    // this system is dual_mu * |pin dual| ~ 5.2e-8, and 32 * the resulting
    // 1.6e-9 error in p3 lands at ~5e-8. Refining past that point would make
    // border mode strictly MORE accurate than the oracle it is required to
    // match, which is the trade this fix deliberately does not make.
    const double residual = std::abs((qp.Ae * bor.x)(0) - qp.be(0));
    EXPECT_LT(residual, 1e-6) << "border mode's equality-row residual";
    ::testing::Test::RecordProperty("border_row_residual", fmt::format("{:.6g}", residual));
}

// THE REFINEMENT-STEP COUNTERS, on the fixture that motivated the iterated
// loop in the first place (Phase-3 carry: the Accelerate audit checklist's
// §(f) asks auditors to compare step counts across backends, which was not
// instrumented until now).
//
// WHAT IS PINNED AND WHY IN THIS SHAPE. border_refine_steps counts TOTAL kept
// steps including each bordered solve's mandatory first (solver_counters.h), so on a
// solve that spends S bordered EQP solves it is >= S by construction and the
// EXCESS over S is what the iterated loop bought. This test pins:
//   (i)  the total as an exact OBSERVED value, so a change in either the
//        number of bordered solves or the per-solve step count fails here,
//   (ii) the total strictly exceeding minor_iters, which is the mode-
//        independent statement that SOME solve took more than the single
//        step this engine used to take -- the actual claim the fix makes,
//   (iii) refactorize mode reporting border_refine_steps == 0 (it never
//        enters the bordered path) and BOTH modes reporting
//        eqp_refine_steps == 0 at the default eqp_refine = false.
TEST(HsBattery, RefinementStepCountersOnBorderRepro) {
    QpProblem qp;
    qp.g = (Vec(3) << -9.2, 9.2, 0.0).finished();
    {
        SpMatRM H(3, 3);
        const std::vector<Eigen::Triplet<double>> t = {{0, 0, 2.0}, {0, 1, -2.0}, {0, 2, 0.0},
                                                       {1, 1, 2.0}, {1, 2, 0.0},  {2, 2, 0.0}};
        H.setFromTriplets(t.begin(), t.end());
        qp.H = H;
    }
    {
        Eigen::SparseMatrix<double, Eigen::RowMajor> Ae(1, 3);
        const std::vector<Eigen::Triplet<double>> a = {{0, 0, 5.0}, {0, 1, -10.4}, {0, 2, 32.0}};
        Ae.setFromTriplets(a.begin(), a.end());
        qp.Ae = Ae;
    }
    qp.be = Vec::Zero(1);
    qp.Ai = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    qp.validate();

    QpOptions border_opts;
    border_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    border_opts.tr_radius = 1.0;
    const QpSolution bor = QpEngine{border_opts}.solve(qp);
    ASSERT_EQ(bor.status, QpStatus::kOptimal);

    ::testing::Test::RecordProperty(
        "hs26_repro_border_counters",
        fmt::format("minor={} fact={} border_refine={} eqp_extra={}", bor.counters.minor_iters,
                    bor.counters.factorizations, bor.counters.border_refine_steps,
                    bor.counters.eqp_refine_steps));

    // OBSERVED on this machine, Release and Debug agree (results note §(f)):
    // 3 bordered EQP solves, 3 total steps -- the mandatory step only, zero
    // extra steps earned. Accelerate's LDLTTPP with inf-norm equilibration
    // solves the cancellation-prone stack to inside the regularization
    // footprint immediately, so the Task 11b digit-loss this loop repairs on
    // MKL (10 steps over 4 solves) does not manifest; BorderModeFalseInfeasible
    // passes unmodified. This is the opposite of the budget-exhaustion
    // symptom §(f) of the audit checklist watched for.
#ifdef USE_ACCELERATE_SPARSE
    EXPECT_EQ(bor.counters.border_refine_steps, 3);
    EXPECT_EQ(bor.counters.border_refine_steps, bor.counters.minor_iters)
        << "every bordered EQP solve satisfied the footprint rule after its single mandatory "
           "step";
#else
    // OBSERVED on this machine (Release and Debug agree): minor_iters == 4,
    // factorizations == 1, border_refine_steps == 10. Four bordered EQP
    // solves means a baseline of 4 mandatory steps, so SIX of the ten are
    // extra steps the iterated loop earned -- and the loop's per-solve budget
    // is 10, so no single solve is anywhere near exhausting it.
    EXPECT_EQ(bor.counters.border_refine_steps, 10);
    EXPECT_GT(bor.counters.border_refine_steps, bor.counters.minor_iters)
        << "at least one bordered solve must have taken more than the single mandatory step -- "
           "that is the whole content of the Task 11b fix";
#endif
    EXPECT_EQ(bor.counters.eqp_refine_steps, 0)
        << "solve_eqp has no iterated loop, so it takes no EXTRA refinement steps";

    QpOptions refac_opts;
    refac_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    refac_opts.tr_radius = 1.0;
    const QpSolution refac = QpEngine{refac_opts}.solve(qp);
    ASSERT_EQ(refac.status, QpStatus::kOptimal);
    EXPECT_EQ(refac.counters.border_refine_steps, 0)
        << "refactorize mode never enters the bordered path";
    EXPECT_EQ(refac.counters.eqp_refine_steps, 0) << "solve_eqp takes no EXTRA refinement steps";
}

// =========================================================================
// Carry-stack measurements. These tests exist to make the numbers in the
// results note re-derivable rather than transcribed, and to fail if the
// mechanism they describe starts behaving differently.
// =========================================================================

// TASK 7's DEFERRED kappa_soc GATE, adjudicated on battery data (carry item
// 2). Task 7 measured ~70% of SOC attempts returning kInfeasible on its OWN
// constructed fixtures and deferred a magnitude gate pending this battery.
//
// THE BATTERY'S ANSWER IS "NOT ENOUGH DATA TO SET A THRESHOLD": across all 27
// problems the correction is attempted exactly TWICE, both on HS77, and
// neither attempt is applied. So the standard test set produces a sample of
// two -- a rate, not a threshold -- and no kappa_soc value can be derived
// from it. See the results note's open section for what would be needed.
//
// What IS assertable, and is asserted here, is the shape: SOC is rare on
// standard problems (2 attempts in 27 solves), and when it does fire far from
// a solution it does not pay off, exactly as sqp_driver.h's A FAILED SOC
// RE-SOLVE note predicts.
TEST(HsBattery, SocIsRareAndUnprofitableAcrossTheBattery) {
    Index total_attempts = 0, total_applied = 0, problems_attempting = 0;
    for (const Expect &e : border_table()) {
        const HsProblem p = make_hs(e.number);
        SqpDriver driver(options_for(e, WorkingSetLinearAlgebra::kSchurBorder));
        const SqpSolution sol = driver.solve(*p.model);
        const Index applied = std::count_if(sol.history.begin(), sol.history.end(),
                                            [](const SqpIterate &h) { return h.soc_applied; });
        if (sol.counters.soc_steps > 0) {
            ++problems_attempting;
            ::testing::Test::RecordProperty(
                fmt::format("soc_hs{}", e.number),
                fmt::format("attempts={} applied={}", sol.counters.soc_steps, applied));
        }
        total_attempts += sol.counters.soc_steps;
        total_applied += applied;
    }
    ::testing::Test::RecordProperty(
        "soc_battery_total", fmt::format("attempts={} applied={} problems={}", total_attempts,
                                         total_applied, problems_attempting));

    // ASSERTED AS THE OBSERVED VALUES. HS77 is the only problem in the set
    // that produces a violation-INCREASING rejection at all (its trials 5 and
    // 6, where h goes 0.01194 -> 0.02516), which is the gate SOC already has.
    EXPECT_EQ(total_attempts, 2);
    EXPECT_EQ(total_applied, 0);
    EXPECT_EQ(problems_attempting, 1);
}

// TASK 8's ELASTIC TIER, measured across the battery (carry item 7's
// conditioning watch, and a tuning finding in its own right).
//
// FOUR PROBLEMS ACTIVATE THE TIER -- HS10 (8 activations), HS11 (2), HS15 (1)
// and HS27 (1) -- and EVERY SINGLE ACTIVATION runs the rho ladder ALL THE WAY
// to its 1e8 ceiling: escalations is exactly 6 x activations on all four, and
// 6 is the ladder's full height (kElasticRhoInit = 1e2 to kElasticRhoMax =
// 1e8). Not one activation anywhere in the battery is satisfied at a cheaper
// rung.
//
// HS26 USED TO BE A FIFTH, with 2 activations that climbed no ladder at all:
// its elastic subproblem was itself reported infeasible, because the border
// path's false kInfeasible (Task 11b) had routed a feasible QP into the tier
// in the first place. With that fixed HS26 never reaches the tier, so the
// counts here dropped from 5 problems / 14 activations to 4 / 12.
//
// THAT IS A COST, NOT A FAILURE: all four problems converge to their cited
// f*, and on HS10 the eight elastic majors are exactly what walks h from 599
// down to 23 before the plain linearization becomes consistent again. But it
// means kElasticRhoInit is, on this evidence, six rungs too low for standard
// problems -- HS10 spends 48 re-solves finding out something the first
// activation already established. Recorded as an open tuning item; NOT
// changed here, because the ladder's start value is Task 8's contract and
// this task is a measurement task.
TEST(HsBattery, EveryElasticActivationRunsTheRhoLadderToItsCeiling) {
    constexpr Index kLadderHeight = 6; // 1e2 -> 1e8 by decades
    Index problems_activating = 0, total_activations = 0;
    for (const Expect &e : border_table()) {
        const HsProblem p = make_hs(e.number);
        SqpDriver driver(options_for(e, WorkingSetLinearAlgebra::kSchurBorder));
        const SqpSolution sol = driver.solve(*p.model);
        if (sol.counters.elastic_activations == 0) {
            EXPECT_EQ(sol.counters.elastic_escalations, 0) << "HS" << e.number;
            continue;
        }
        ++problems_activating;
        total_activations += sol.counters.elastic_activations;
        ::testing::Test::RecordProperty(fmt::format("elastic_hs{}", e.number),
                                        fmt::format("activations={} escalations={}",
                                                    sol.counters.elastic_activations,
                                                    sol.counters.elastic_escalations));
        // A non-kOptimal exit can activate the tier and then EXHAUST it into a
        // kRestore, which is the one shape that does not climb the ladder --
        // the elastic subproblem is itself reported infeasible, so there is
        // nothing to escalate. No battery row does that today (HS26 did, off
        // the back of the Task 11b bug); the guard stays so that a row which
        // starts doing it fails the count below rather than this assertion.
        if (sol.status != SqpStatus::kOptimal) {
            continue;
        }
        EXPECT_EQ(sol.counters.elastic_escalations,
                  kLadderHeight * sol.counters.elastic_activations)
            << "HS" << e.number << ": an activation stopped short of the rho ceiling";
    }
    // ASSERTED AS THE OBSERVED VALUES: HS10, HS11, HS15, HS27 = 4 problems,
    // 8+2+1+1 = 12 activations.
    EXPECT_EQ(problems_activating, 4);
    EXPECT_EQ(total_activations, 12);
}

// TASK 10's ADAPTIVE mu SCHEDULE vs. THE SOC/ELASTIC RE-SOLVES (carry item
// 6): those re-solves build their own default-mu SolveOverrides, so a major
// whose main QP is solved at a SCHEDULED mu and whose SOC/elastic re-solve
// runs at the ENGINE DEFAULT is a mismatch waiting to happen.
//
// IT DOES NOT HAPPEN ON THIS BATTERY, and the measurement below says why
// rather than merely asserting the absence: the schedule is residual-driven,
// so it only leaves the engine default (1e-8) in the CONVERGENCE TAIL. Over
// all 27 problems mu departs from 1e-8 on exactly THREE rows -- HS26's trial
// 16, HS30's trial 10 and HS77's trial 11, each of them the LAST subproblem
// its solve builds -- whereas SOC and the elastic tier fire mid-course, where
// mu is still at the default. So the two mechanisms never share a major here
// and the artifact has no opportunity to appear.
//
// HS26 JOINED THAT LIST IN TASK 11b, and it is the same fix's doing: before
// it, border mode never got past HS26's first subproblem, so the solve had no
// convergence tail for the schedule to act in.
//
// THE ABSENCE IS CIRCUMSTANTIAL, NOT STRUCTURAL, and the test says so: the
// hazard is live on any problem whose residual falls fast enough to move mu
// while the linearization is still inconsistent. This battery contains no
// such problem, which is a fact about these 27 problems and not a guarantee
// about the driver. The `last_row` assertion is what pins the circumstance --
// if a scheduled mu ever appears anywhere but the final subproblem, the
// premise of the whole argument has changed and this test says which row.
TEST(HsBattery, AdaptiveMuNeverSharesAMajorWithSocOrElastic) {
    Index rows_off_default = 0, overlapping_rows = 0, non_final_rows = 0;
    for (const Expect &e : border_table()) {
        const HsProblem p = make_hs(e.number);
        const SqpOptions opts = options_for(e, WorkingSetLinearAlgebra::kSchurBorder);
        ASSERT_TRUE(opts.adaptive_mu) << "the lever under measurement must be ON";
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        // The index of the last row that actually built a subproblem. On an
        // iterate exit the final row has qp_solved == false (sqp_types.h's
        // two history shapes), so this is not simply history.size() - 1.
        Index last_solved = -1;
        for (const SqpIterate &h : sol.history) {
            if (h.qp_solved) {
                last_solved = h.trial;
            }
        }
        for (const SqpIterate &h : sol.history) {
            if (!h.qp_solved || h.mu == opts.qp.dual_mu) {
                continue;
            }
            ++rows_off_default;
            ::testing::Test::RecordProperty(
                fmt::format("mu_hs{}_trial{}", e.number, h.trial),
                fmt::format("mu={:.3g} soc={} elastic={} last_solved={}", h.mu,
                            h.soc_applied ? 1 : 0, h.elastic_applied ? 1 : 0, last_solved));
            if (h.soc_applied || h.elastic_applied) {
                ++overlapping_rows;
            }
            if (h.trial != last_solved) {
                ++non_final_rows;
            }
        }
    }
    // ASSERTED AS THE OBSERVED VALUES: three border-mode rows leave the engine
    // default -- HS26 trial 16, HS30 trial 10 and HS77 trial 11, all three at
    // mu = 1e-9 -- and each is the last subproblem of its solve.
    EXPECT_EQ(rows_off_default, 3);
    EXPECT_EQ(non_final_rows, 0)
        << "a scheduled mu now appears before the final subproblem, so it can overlap a "
           "SOC/elastic re-solve; re-read the results note's carry item 6";
    EXPECT_EQ(overlapping_rows, 0)
        << "a major now combines a scheduled mu with a default-mu re-solve; the "
           "interaction artifact Task 10 flagged is live -- see the results note";
}

// TASK 5's FUNNEL INITIALIZER tau_bar = 100 (carry item 5): it is loose, and
// the watch is for problems where the funnel accepts a WILD early step that
// costs extra majors.
//
// THE SIGNATURE IS PRESENT AND MEASURABLE, AND ON THIS BATTERY IT IS FREE.
// Fifteen of the 27 problems start with h == 0 exactly. THREE of those fifteen
// then let the iterate leave feasibility entirely before coming back:
//
//     HS12  h_max = 20.0    6 majors  (budget 10)
//     HS43  h_max = 17.4    7 majors  (budget 12)
//     HS26  h_max = 0.880  17 majors  (budget 25)
//
// An excursion to h = 20 from a feasible start is exactly what a tau_bar of
// 100 permits and a tighter initializer would have refused. But none of the
// three pays for it: HS12 and HS43 are the CHEAPEST inequality-constrained
// solves in the battery relative to their size, and HS26's 17 majors are the
// degenerate quartic's linear tail (see hs_problems.h's HS26 note), not the
// excursion -- its h is back under 0.22 by trial 2. So the carry-forward
// closes as "loose, demonstrably used, and not costing anything here"; see
// the results note for what a problem that DID pay would look like.
//
// A FOURTH PROBLEM (HS28) REGISTERS h_max = 5.55e-16 AND IS NOT AN EXCURSION:
// its single equality is linear and its start point satisfies it, so that is
// the roundoff of evaluating a linear form, which is why the sweep uses a
// threshold rather than h_max > 0.
//
// The invariant asserted per-problem is the weakest true statement: a solve
// that STARTS feasible must END feasible, however far it wandered between.
TEST(HsBattery, FeasibleStartsEndFeasibleDespiteTheLooseFunnelWidth) {
    // Well above the ~1e-16 roundoff of evaluating a satisfied linear row and
    // far below the smallest genuine excursion (0.88), so the classification
    // is not balanced on the threshold's value.
    constexpr double kExcursion = 1e-9;
    Index feasible_starts = 0, with_excursions = 0;
    for (const Expect &e : refactorize_table()) {
        const HsProblem p = make_hs(e.number);
        SqpDriver driver(options_for(e, WorkingSetLinearAlgebra::kRefactorize));
        const SqpSolution sol = driver.solve(*p.model);
        if (sol.history.empty() || sol.history.front().violation_l1 > kExcursion) {
            continue;
        }
        ++feasible_starts;
        double h_max = 0.0;
        for (const SqpIterate &h : sol.history) {
            h_max = std::max(h_max, h.violation_l1);
        }
        if (h_max > kExcursion) {
            ++with_excursions;
            ::testing::Test::RecordProperty(fmt::format("funnel_excursion_hs{}", e.number),
                                            fmt::format("h_max={:.6g} majors={} budget={}", h_max,
                                                        sol.counters.major_iters, e.major_budget));
            // THE COST CLAIM, asserted rather than asserted-about: an
            // excursion that were costing extra majors would show up as a
            // solve pressing against its own budget.
            EXPECT_LE(sol.counters.major_iters, e.major_budget)
                << "HS" << e.number << "'s funnel excursion now costs majors";
        }
        EXPECT_LE(sol.history.back().violation_l1, sol.history.front().violation_l1 + 1e-6)
            << "HS" << e.number << " started feasible and ended less so";
    }
    // ASSERTED AS THE OBSERVED VALUES: 15 feasible starts, 3 of them taking a
    // genuine excursion (HS12, HS26, HS43).
    EXPECT_EQ(feasible_starts, 15);
    EXPECT_EQ(with_excursions, 3);
}

// THE RESTORATION PHASE AND ITS CERTIFICATE (carry item 4): watch for battery
// problems that reach kInfeasible with infeasibility_certified set, and
// verify by hand whether the point really is an infeasible stationary point.
//
// NOTHING IN THE BATTERY REACHES THE CERTIFICATE, which is the right outcome:
// all 27 problems are feasible, so a certified local-infeasibility exit would
// be a WRONG ANSWER, not a finding. Since Task 11b there is no kInfeasible exit
// in either table at all; while there was one (HS26 in border mode) it
// correctly reported infeasibility_certified == FALSE -- the "could not make
// progress" shape of that status, exactly as SqpStatus's own note requires,
// with the restoration phase never running (restoration_iters == 0), so there
// was never a certificate to make. The kInfeasible arm below is kept as a
// standing guard on that distinction.
TEST(HsBattery, NoBatteryProblemEverCertifiesInfeasibility) {
    for (auto alg :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        const std::vector<Expect> table =
            alg == WorkingSetLinearAlgebra::kSchurBorder ? border_table() : refactorize_table();
        for (const Expect &e : table) {
            SCOPED_TRACE(::testing::Message() << "HS" << e.number);
            const HsProblem p = make_hs(e.number);
            SqpDriver driver(options_for(e, alg));
            const SqpSolution sol = driver.solve(*p.model);
            EXPECT_FALSE(sol.infeasibility_certified)
                << "HS" << e.number << " is a FEASIBLE problem; a certificate here is wrong";
            if (sol.status == SqpStatus::kInfeasible) {
                EXPECT_EQ(sol.counters.restoration_iters, 0)
                    << "HS" << e.number
                    << ": a kInfeasible exit on a feasible battery problem is an elastic "
                       "exhaustion, which never enters restoration";
            }
        }
    }
}

// AGGREGATE COST, for the results note's summary row and as a coarse
// regression tripwire on the whole solver at once: a change that makes every
// problem 20% more expensive shows up here even when no individual budget
// trips.
TEST(HsBattery, AggregateCostIsRecordedAndBounded) {
    Index majors = 0, minors = 0, facts = 0, accepted = 0, rejected = 0;
    for (const Expect &e : border_table()) {
        const HsProblem p = make_hs(e.number);
        SqpDriver driver(options_for(e, WorkingSetLinearAlgebra::kSchurBorder));
        const SqpSolution sol = driver.solve(*p.model);
        majors += sol.counters.major_iters;
        minors += sol.counters.qp_minor_iters;
        facts += sol.counters.factorizations;
        accepted += sol.counters.steps_accepted;
        rejected += sol.counters.rejected_steps;
    }
    ::testing::Test::RecordProperty(
        "battery_border_totals",
        fmt::format("majors={} minors={} factorizations={} accepted={} rejected={}", majors, minors,
                    facts, accepted, rejected));
    // OBSERVED (kSchurBorder, all 27 problems): 228 majors, 862 minor
    // iterations, 320 factorizations, 210 accepted steps, 18 rejected. The
    // bounds are ~25% headroom on each -- loose enough that a legitimate
    // re-tuning does not trip them, tight enough that a systemic regression
    // does. They went UP at Task 11b, by exactly the 15 majors HS26 spends now
    // that border mode solves it instead of stalling on the first subproblem;
    // kRefactorize's totals over the same 27 problems are 228/860/910/210/18,
    // i.e. identical but for the factorization count bordering exists to cut.
    EXPECT_LE(majors, 285);
    EXPECT_LE(minors, 1080);
    EXPECT_LE(facts, 400);
    EXPECT_EQ(accepted + rejected, majors)
        << "every major is either an accepted step or a rejected one -- no kRestore rows "
           "remain in this battery since Task 11b";
}

// PHASE-6 TASK 4 -- THE CRASH BASIS, MEASURED ACROSS THE WHOLE BATTERY.
//
// SqpOptions::crash_basis seeds a COLD solve's first subproblem's working set
// from the geometric activity at x0. This test is the corpus-wide on/off A/B,
// and its verdict is a NULL RESULT WITH A MECHANISM, pinned so the mechanism
// cannot rot: see docs/notes/2026-08-03-crash-basis.md Sec. 3 and Sec. 7.
//
// WHAT THE BATTERY'S START POINTS ACTUALLY OFFER. Of 27 problems, 8 have
// anything within feas_tol of a constraint at their published start point:
//
//   ROWS   HS10 (1), HS11 (1), HS14 (1), HS15 (2), HS22 (2)  -- 7 in total
//   BOUNDS HS30 (1), HS33 (2), HS45 (2)                      -- 5 in total
//
// A NINTH, HS25, sits on a bound at x0 and is nonetheless seeded ZERO -- it
// converges in 0 majors and so never builds a subproblem for a seed to be
// offered to. That is the "a solve that never builds a subproblem never
// derives one" clause of the mechanism, exercised by a real row rather than
// argued, and it is asserted below.
//
// AND THE ROW SEEDS CHANGE NOTHING AT ALL. All five row-seeding problems are
// counter-identical between the arms, because those start points are on the
// VIOLATED side and qp_engine.h's homotopy (refresh_shifts) admits exactly
// those rows in its pre-loop pass with or without the lever. Only the BOUND
// seeds ever move a trajectory, and only on two problems.
//
// THE NET, ON THE WHOLE CORPUS: 862 minors -> 859 (-3, 0.35 %) and 320
// factorizations -> 323 (+3). That is the honest reading of this lever on
// standard problems -- not a win, not a regression, essentially nothing --
// and it is why the default stays off.
TEST(HsBattery, CrashBasisIsANullResultOnTheBatteryWithTwoBoundSeededExceptions) {
    Index seeded_rows = 0, seeded_bounds = 0;
    Index minors_off = 0, minors_on = 0, facts_off = 0, facts_on = 0;
    std::vector<int> changed, seeding;

    for (const Expect &e : border_table()) {
        const HsProblem p = make_hs(e.number);
        SqpOptions off_opts = options_for(e, WorkingSetLinearAlgebra::kSchurBorder);
        SqpOptions on_opts = off_opts;
        on_opts.crash_basis = true;

        SqpDriver a(off_opts);
        const SqpSolution off = a.solve(*p.model);
        SqpDriver b(on_opts);
        const SqpSolution on = b.solve(*p.model);

        // The lever may never move an ANSWER, only a cost.
        ASSERT_EQ(on.status, off.status) << "HS" << e.number;
        EXPECT_LE(std::abs(on.f - off.f), 1e-9 * std::max(1.0, std::abs(off.f)))
            << "HS" << e.number;
        EXPECT_EQ(off.counters.crash_seeded_rows, 0) << "HS" << e.number;
        EXPECT_EQ(off.counters.crash_seeded_bounds, 0) << "HS" << e.number;

        seeded_rows += on.counters.crash_seeded_rows;
        seeded_bounds += on.counters.crash_seeded_bounds;
        minors_off += off.counters.qp_minor_iters;
        minors_on += on.counters.qp_minor_iters;
        facts_off += off.counters.factorizations;
        facts_on += on.counters.factorizations;
        if (on.counters.crash_seeded_rows > 0 || on.counters.crash_seeded_bounds > 0) {
            seeding.push_back(e.number);
        }
        if (on.counters.major_iters != off.counters.major_iters ||
            on.counters.qp_minor_iters != off.counters.qp_minor_iters ||
            on.counters.factorizations != off.counters.factorizations) {
            changed.push_back(e.number);
            ::testing::Test::RecordProperty(
                fmt::format("crash_hs{}", e.number),
                fmt::format("seeded={}r/{}b maj {}->{} min {}->{} fact {}->{}",
                            on.counters.crash_seeded_rows, on.counters.crash_seeded_bounds,
                            off.counters.major_iters, on.counters.major_iters,
                            off.counters.qp_minor_iters, on.counters.qp_minor_iters,
                            off.counters.factorizations, on.counters.factorizations));
        }
    }
    ::testing::Test::RecordProperty("crash_battery_total",
                                    fmt::format("rows={} bounds={} minors {}->{} fact {}->{}",
                                                seeded_rows, seeded_bounds, minors_off, minors_on,
                                                facts_off, facts_on));

    // ASSERTED AS THE OBSERVED VALUES (MKL Pardiso, clang++). No Apple value
    // existed for these when they were pinned -- they were newer than the last
    // Mac pass -- so this comment said that if they moved on Accelerate the
    // counts belonged in the divergence register. TWO OF THEM MOVED, and they
    // are in it: entry M3-1 of
    // docs/notes/2026-08-14-accelerate-divergence-register.md, with the
    // per-backend arm below.
    EXPECT_EQ(seeded_rows, 7);
    EXPECT_EQ(seeded_bounds, 5);
    EXPECT_EQ(seeding, (std::vector<int>{10, 11, 14, 15, 22, 30, 33, 45}))
        << "HS25 sits on a bound at x0 but converges in 0 majors, so nothing is ever seeded";
    EXPECT_EQ(changed, (std::vector<int>{30, 33}))
        << "only BOUND seeds move a trajectory; every row seed is redundant with the homotopy";
#ifdef USE_ACCELERATE_SPARSE
    // M3-1. The battery total is exactly ONE minor lighter in each arm on
    // Apple Accelerate -- 861/858 against MKL Pardiso's 862/859 -- while
    // facts_off/facts_on, the seeded counts, and BOTH problem lists above are
    // byte-identical. One HS problem spends one fewer QP minor; the
    // answer-invariance half of this battery (status and objective, asserted
    // per problem above) is untouched, so this is a trajectory fork and not a
    // different answer.
    //
    // OBSERVED, NOT PREDICTED. macOS CI, runner class github-macos26-arm64,
    // Release, AppleClang, and stable across every m3 run that carried this
    // suite -- first at
    // https://github.com/GrantHecht/hven/actions/runs/31736708703 (0ba1b9f),
    // most recently https://github.com/GrantHecht/hven/actions/runs/31824897327
    // (6535566). A counter carries none of the machine/thread/configuration
    // context a float does, so ten agreeing runs on real Apple hardware is a
    // firmer basis for a pin than this lane can offer any float column.
    EXPECT_EQ(minors_off, 861);
    EXPECT_EQ(minors_on, 858);
#else
    EXPECT_EQ(minors_off, 862);
    EXPECT_EQ(minors_on, 859);
#endif
    EXPECT_EQ(facts_off, 320);
    EXPECT_EQ(facts_on, 323);
}

// THE TABLE MUST COVER EVERY SHIPPED PROBLEM. Without this a problem added to
// hs_problems.h could be derivative-checked and then never solved by anything.
TEST(HsBattery, TableCoversEveryShippedProblem) {
    std::vector<int> tabled;
    for (const Expect &e : border_table()) {
        tabled.push_back(e.number);
    }
    EXPECT_EQ(tabled, hs_numbers());
    EXPECT_EQ(refactorize_table().size(), border_table().size());
}
