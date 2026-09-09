// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/drivers/test_callback.cpp -- ONE PER-ITERATION CALLBACK, BOTH ENGINES
// (M6 W5 T8.6).
//
// `set_iteration_callback(IterationCallback)` / `clear_iteration_callback()`
// exist on the interior-point solver and on the SQP driver, over ONE event type
// in DECLARED space and CALLER units, and `CallbackAction::kStop` is honoured on
// both -- which is what makes `SolveStatus::kInterrupted` reachable at all
// (design section 2.3: "neither reports kInterrupted today" was true until this
// task). That is a statement about the SHARED surface, so it is pinned here
// rather than in either engine's suite.
//
// WHAT EACH ENGINE PROMISES, and what the nine tests below hold it to:
//
//   SQP -- one event per HISTORY ROW, fired at the single site that emits them,
//   in the caller's units even on a scaled solve (design section 2.7 behaviour
//   change (3)); `depth` counts restoration nesting and a stop taken inside a
//   restoration sub-solve LATCHES on the parent; a stop is honoured before the
//   next subproblem is built, so `history.size() == major_iters + 1` survives
//   it; converged beats stop.
//
//   INTERIOR-POINT -- one event per `ipm.iter` row, fired at the TOP of the
//   iteration that row belongs to, so the event describes the COMMITTED point
//   of the previous step with the diagnostics of THAT point (behaviour change
//   (2)); a stop is honoured pre-factorization and ends the PHASE SEQUENCE.
//
//   BOTH -- an exception from the callback propagates out of solve(), the
//   abandoned solve writes no end event and no ledger record of its own, and
//   the solver is reusable.

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/core/ledger.h>
#include <hven/drivers/interior_point_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>
#include <hven/model/nlp_model_aggregate.h>
#include <hven/model/nlp_problem_model.h>
#include <hven/model/nlp_solver.h>

#include "support/hs071_problem.h"

namespace {

using hven::Index;
using hven::solvers::CallbackAction;
using hven::solvers::IterationEvent;
using hven::solvers::Ledger;
using hven::solvers::NlpModel;
using hven::solvers::NlpProblemModel;
using hven::solvers::NLPSolver;
using hven::solvers::SolveStatus;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using Vec = Eigen::VectorXd;
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

constexpr double kInfBound = std::numeric_limits<double>::infinity();

// ---------------------------------------------------------------------------
// THE RECORDER. An IterationEvent's four vector views are borrowed and valid
// for the call only, so an oracle that wants them afterwards COPIES them --
// which is exactly what this does, and what a caller would have to do.
// ---------------------------------------------------------------------------
struct Seen {
    Index iteration = 0;
    std::optional<Index> phase;
    std::optional<Index> depth;
    double f = 0.0;
    double stationarity = 0.0;
    double feasibility_e = 0.0;
    double feasibility_i = 0.0;
    double complementarity = 0.0;
    double step_norm = 0.0;
    std::optional<double> radius;
    std::optional<double> mu;
    Vec x, lambda_e, lambda_i, z;
    double elapsed_seconds = 0.0;
};

Seen copy_of(const IterationEvent &e) {
    Seen s;
    s.iteration = e.iteration;
    s.phase = e.phase;
    s.depth = e.depth;
    s.f = e.f;
    s.stationarity = e.stationarity;
    s.feasibility_e = e.feasibility_e;
    s.feasibility_i = e.feasibility_i;
    s.complementarity = e.complementarity;
    s.step_norm = e.step_norm;
    s.radius = e.radius;
    s.mu = e.mu;
    s.x = e.x;
    s.lambda_e = e.lambda_e;
    s.lambda_i = e.lambda_i;
    s.z = e.z;
    s.elapsed_seconds = e.elapsed_seconds;
    return s;
}

/// @brief Records every event and never stops.
struct Recorder {
    std::vector<Seen> seen;
    hven::solvers::IterationCallback hook() {
        return [this](const IterationEvent &e) {
            this->seen.push_back(copy_of(e));
            return CallbackAction::kContinue;
        };
    }
};

Vec hs071_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

SqpOptions quiet_sqp() {
    SqpOptions o;
    o.common.print_level = 10;
    return o;
}

hven::solvers::IpmOptions quiet_ipm(const NLPSolver &solver) {
    hven::solvers::IpmOptions o = solver.optimizer_->options();
    o.common.print_level = 10;
    return o;
}

// ---------------------------------------------------------------------------
// FIXTURE: THE FUNNEL-STALLING VALLEY, which is the tree's documented route
// into the restoration phase and back out of it.
//
//     min  -10 x1 + 5 x1^2 - 0.01 x0 + 0.0005 x0^2
//     s.t. cE(x) = x1 + 1000 x0^2 = 0,      x_start = (0, 0.5)
//
// VERBATIM from tests/sqp/test_sqp_restoration.cpp's StalledValleyModel, whose
// own comment carries the hand-derived stall schedule, the restoration entry
// and the optimum. It is copied rather than shared because that model is a
// local of a .cpp in the SQP suite; nothing about it is edited here, and the
// only property this file uses is that a solve of it ENTERS RESTORATION and
// then RESUMES -- which is what gives depth-1 events to stop at.
// ---------------------------------------------------------------------------
class StalledValleyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        return -10.0 * x(1) + 5.0 * x(1) * x(1) - 0.01 * x(0) + 0.0005 * x(0) * x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << -0.01 + 0.001 * x(0), -10.0 + 10.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) + 1000.0 * x(0) * x(0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_factor, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_factor * 0.001 + 2000.0 * lambda_e(0);
        h.insert(1, 1) = obj_factor * 10.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 2000.0 * x(0);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.5;
        return x;
    }
};

// ---------------------------------------------------------------------------
// FIXTURE: AN INFEASIBLE NLP, whose restoration phase runs to a certificate
// rather than resuming.
//
//     min  x0 + x1   s.t.  x0^2 + x1^2 = 1,  x0 + x1 = 2      x_start = (2, 2)
//
// VERBATIM from tests/sqp/test_sqp_restoration.cpp's InfeasibleCircleLineModel
// (see that file for the derivation of the elastic-then-restoration
// trajectory), copied for the same reason StalledValleyModel is. What this file
// uses it for is the OTHER restoration route: the sub-solve here exits at a
// still-infeasible point, so a stop taken inside it arrives at the parent as
// `rs.status == kInterrupted` through enter_restoration's status switch --
// where the resuming fixture above exercises only the forwarder's latch.
// ---------------------------------------------------------------------------
class InfeasibleCircleLineModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 2.0, 2.0;
        return x;
    }
};

// ---------------------------------------------------------------------------
// FIXTURE: A MODEL THE CALLER'S OWN START POINT CANNOT BE EVALUATED AT.
//
// x0 itself is finite -- the driver validates that -- but f, its gradient and
// the constraint row are all NaN there, so the driver's very first KKT
// measurement is non-finite and it takes the ZERO-MAJOR exit: one history row,
// no subproblem, kNumericalError.
// ---------------------------------------------------------------------------
class NonFiniteStartModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    static double nan() { return std::numeric_limits<double>::quiet_NaN(); }

    double eval_f(const Vec &) const override { return nan(); }
    Vec eval_grad(const Vec &) const override { return Vec::Constant(2, nan()); }
    Vec eval_ce(const Vec &) const override { return Vec::Constant(1, nan()); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 1.0;
        h.insert(1, 1) = 1.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = nan();
        j.insert(0, 1) = nan();
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInfBound);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInfBound);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 1.0); }
};

// An SQP-side view of HS071, kept alive with its bridge.
struct Hs071View {
    std::shared_ptr<hven::solvers::NLPProblem> problem;
    std::shared_ptr<NlpProblemModel> model;
    std::shared_ptr<hven::solvers::NlpModelAggregate> bridge;

    Hs071View()
        : problem(std::make_shared<hven_drivers_tests::Hs071Problem>()),
          model(std::make_shared<NlpProblemModel>(problem)),
          bridge(std::make_shared<hven::solvers::NlpModelAggregate>(model)) {}
};

bool bit_equal(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i])) {
            return false;
        }
    }
    return true;
}

} // namespace

// ===========================================================================
// (1) THE SQP'S EVENTS ARE ITS HISTORY ROWS, IN THE CALLER'S UNITS
// ===========================================================================

// enable_scaling ON, deliberately: an event built from the engine's own vectors
// without the engine->caller map would agree with the history on an UNSCALED
// solve and disagree on this one, so only this arm proves behaviour change (3).
// The test asserts the scaling really engaged before it reads anything.
TEST(Callback, SqpEventRowsEqualHistoryInCallerUnits) {
    Hs071View view;
    SqpOptions opts = quiet_sqp();
    opts.enable_scaling = true;

    SqpDriver driver(opts);
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    const SqpSolution sol = driver.solve(*view.bridge, hs071_start());

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_TRUE(sol.scaling.active) << "fixture premise: the scaling must engage, or the "
                                       "engine->caller map below is the identity and this "
                                       "test proves nothing about behaviour change (3)";
    ASSERT_NE(sol.scaling.obj, 1.0) << "fixture premise: a non-unit objective factor";

    // ONE EVENT PER ROW, IN ORDER.
    ASSERT_EQ(rec.seen.size(), sol.history.size());
    ASSERT_GT(rec.seen.size(), 1u) << "non-vacuous: the solve really iterates";
    for (std::size_t k = 0; k < rec.seen.size(); ++k) {
        SCOPED_TRACE("row " + std::to_string(k));
        const Seen &e = rec.seen[k];
        EXPECT_EQ(e.iteration, static_cast<Index>(k));
        EXPECT_FALSE(e.phase.has_value()) << "phase is the interior-point engine's field";
        ASSERT_TRUE(e.depth.has_value());
        EXPECT_EQ(*e.depth, 0) << "a top-level major is nesting depth 0";
        // The row's own caller-unit columns, bit for bit: the event is built
        // from the SAME `exported` row the push and the trace line carry.
        EXPECT_EQ(std::bit_cast<std::uint64_t>(e.f),
                  std::bit_cast<std::uint64_t>(sol.history[k].f));
        EXPECT_EQ(std::bit_cast<std::uint64_t>(e.step_norm),
                  std::bit_cast<std::uint64_t>(sol.history[k].step_norm));
        ASSERT_TRUE(e.radius.has_value());
        EXPECT_EQ(std::bit_cast<std::uint64_t>(*e.radius),
                  std::bit_cast<std::uint64_t>(sol.history[k].tr_radius));
        EXPECT_FALSE(e.mu.has_value()) << "mu is the interior-point engine's field";
        EXPECT_EQ(e.x.size(), sol.x.size());
    }

    // AND THE VECTORS ARE THE CALLER'S TOO. The terminal row is pushed at the
    // exit conjunction, from the very (x, lambda) `finish` then exports, so the
    // point matches bit for bit and the prices match to the sign sweep and the
    // re-measurement `finish` takes on a scaled solve.
    const Seen &last = rec.seen.back();
    EXPECT_TRUE(bit_equal(last.x, sol.x)) << "the terminal event's point IS the returned point";
    ASSERT_EQ(last.lambda_e.size(), sol.lambda_e.size());
    for (Eigen::Index i = 0; i < last.lambda_e.size(); ++i) {
        EXPECT_NEAR(last.lambda_e[i], sol.lambda_e[i], 1e-9 * (1.0 + std::abs(sol.lambda_e[i])));
    }
    ASSERT_EQ(last.lambda_i.size(), sol.lambda_i.size());
    for (Eigen::Index i = 0; i < last.lambda_i.size(); ++i) {
        EXPECT_NEAR(last.lambda_i[i], sol.lambda_i[i], 1e-9 * (1.0 + std::abs(sol.lambda_i[i])));
    }

    // FALSIFIABILITY: the engine-unit multipliers this solve ran at are NOT the
    // caller-unit ones, so a map that had been skipped would be caught. The
    // objective factor is what separates them.
    EXPECT_NE(sol.scaling.obj, 1.0);
}

// THE UNSCALED CONTROL, and the stronger half of the same claim: with no map to
// apply, the terminal event's four SHARED diagnostics are the very numbers the
// result reports -- one arithmetic, computed once, not two that happen to agree.
TEST(Callback, SqpTerminalEventDiagnosticsAreTheResultsOwn) {
    Hs071View view;
    SqpDriver driver(quiet_sqp());
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    const SqpSolution sol = driver.solve(*view.bridge, hs071_start());

    ASSERT_EQ(sol.status, SolveStatus::kOptimal);
    ASSERT_FALSE(sol.scaling.active) << "this arm is the UNSCALED control";
    ASSERT_FALSE(rec.seen.empty());
    const Seen &last = rec.seen.back();
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.stationarity),
              std::bit_cast<std::uint64_t>(sol.stationarity));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.feasibility_e),
              std::bit_cast<std::uint64_t>(sol.feasibility_e));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.feasibility_i),
              std::bit_cast<std::uint64_t>(sol.feasibility_i));
    EXPECT_EQ(std::bit_cast<std::uint64_t>(last.complementarity),
              std::bit_cast<std::uint64_t>(sol.complementarity));
}

// ===========================================================================
// (2) kStop ON THE SQP
// ===========================================================================

TEST(Callback, SqpStopReturnsInterruptedAtTheCurrentPoint) {
    // THE STOP INDEX. The callback stops on the event of history row 1; the
    // latch is then read at the NEXT major's exit conjunction, which sits above
    // build_subproblem -- so that major pushes its own row and solves nothing.
    constexpr Index kStopAtRow = 1;

    for (const bool budget_mode : {false, true}) {
        SCOPED_TRACE(budget_mode ? "budget_mode" : "ordinary");
        Hs071View view;
        SqpOptions opts = quiet_sqp();
        opts.budget_mode = budget_mode;

        SqpDriver driver(opts);
        std::vector<Seen> seen;
        driver.set_iteration_callback([&](const IterationEvent &e) {
            seen.push_back(copy_of(e));
            return e.iteration == kStopAtRow ? CallbackAction::kStop : CallbackAction::kContinue;
        });
        const SqpSolution sol = driver.solve(*view.bridge, hs071_start());

        // THE VERDICT. Under budget_mode too: an interrupt takes the ORDINARY
        // exit at the current point, never the budget-best substitution --
        // handing back a different iterate than the one the caller stopped on
        // would make the event a lie.
        EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
        EXPECT_NE(sol.status, SolveStatus::kBudgetExhausted);

        // THE SHAPE. One more row than majors, exactly as a capped solve: the
        // interrupted major pushed its row and built no subproblem.
        EXPECT_EQ(sol.history.size(), static_cast<std::size_t>(kStopAtRow) + 2u);
        EXPECT_EQ(sol.counters.major_iters, kStopAtRow + 1);
        EXPECT_EQ(static_cast<Index>(sol.history.size()), sol.counters.major_iters + 1);

        // ONE EVENT PER ROW STILL, the stop's own row included, and the LAST
        // event describes the point that is returned.
        ASSERT_EQ(seen.size(), sol.history.size());
        EXPECT_TRUE(bit_equal(seen.back().x, sol.x));

        // NON-VACUOUS: an uninterrupted solve of this problem takes strictly
        // more majors, so the stop really stopped something.
        SqpDriver full(opts);
        const SqpSolution ref = full.solve(*view.bridge, hs071_start());
        EXPECT_EQ(ref.status, SolveStatus::kOptimal);
        EXPECT_GT(ref.counters.major_iters, sol.counters.major_iters);
    }
}

TEST(Callback, SqpConvergedBeatsStop) {
    Hs071View view;
    const SqpOptions opts = quiet_sqp();

    // The converged solve's terminal row index, taken first so the stop below
    // can be aimed at exactly that row and at no other.
    SqpDriver pilot(opts);
    const SqpSolution reference = pilot.solve(*view.bridge, hs071_start());
    ASSERT_EQ(reference.status, SolveStatus::kOptimal);
    ASSERT_GT(reference.history.size(), 1u);
    const Index terminal_row = static_cast<Index>(reference.history.size()) - 1;

    SqpDriver driver(opts);
    driver.set_iteration_callback([&](const IterationEvent &e) {
        return e.iteration == terminal_row ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const SqpSolution sol = driver.solve(*view.bridge, hs071_start());

    // CONVERGED BEATS STOP: the row the callback stopped on is the row the
    // convergence test had already passed, and the verdict was settled before
    // the callback ever saw it. The stop is a no-op there.
    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_EQ(sol.history.size(), reference.history.size());
    EXPECT_EQ(sol.counters.major_iters, reference.counters.major_iters);
    EXPECT_TRUE(bit_equal(sol.x, reference.x))
        << "a stop that cannot demote the verdict must not move the answer either";
}

TEST(Callback, SqpStopInsideRestorationLatchesAndReachesTheParent) {
    StalledValleyModel model;
    SqpOptions opts = quiet_sqp();
    opts.tr_init = 8.0; // the fixture's own radius schedule
    opts.max_iter = 60;

    // The reference solve: restoration runs, the solve resumes and converges.
    SqpDriver reference_driver(opts);
    const SqpSolution reference = reference_driver.solve(model);
    ASSERT_EQ(reference.status, SolveStatus::kOptimal);
    ASSERT_GE(reference.counters.restoration_iters, 1)
        << "fixture premise: the solve must actually enter restoration";

    SqpDriver driver(opts);
    std::vector<Seen> seen;
    bool stopped = false;
    std::size_t stop_index = 0;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        if (!stopped && e.depth.value_or(0) == 1) {
            stopped = true;
            stop_index = seen.size() - 1;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpSolution sol = driver.solve(model);

    ASSERT_TRUE(stopped) << "no depth-1 event ever fired, so nothing inside restoration was seen";

    // (a) THE NESTING IS REPORTED. Depth-0 events came first -- the majors that
    //     stalled the funnel -- and the sub-solve's own rows arrive at depth 1
    //     through the forwarder the parent installs.
    EXPECT_EQ(seen[stop_index].depth.value_or(-1), 1);
    ASSERT_GT(stop_index, 0u);
    EXPECT_EQ(seen[0].depth.value_or(-1), 0);

    // (b) THE LATCH REACHES THE PARENT: the outer solve reports kInterrupted,
    //     not the kOptimal the same fixture reaches when nothing stops it.
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);

    // (c) THE SUB-SOLVE ITSELF IS UNMOVED HERE, and that is the point of using
    //     the RESUMING fixture for this arm: its restoration phase converges on
    //     the very next row, so converged beats stop INSIDE the sub-solve too
    //     and the phase returns its own kOptimal at a feasible point. The stop
    //     reaches the parent through the FORWARDER's latch and through nothing
    //     else -- route (b). Route (a), where the sub-solve's own kInterrupted
    //     arrives at enter_restoration's status switch, is the circle fixture
    //     below.
    EXPECT_EQ(sol.counters.restoration_iters, reference.counters.restoration_iters);

    // (d) AND NO FURTHER TOP-LEVEL MAJOR: the parent's own subproblem count
    //     stops where the restoration request left it.
    EXPECT_LT(sol.counters.major_iters, reference.counters.major_iters);
    EXPECT_EQ(static_cast<Index>(sol.history.size()), sol.counters.major_iters + 1);
}

TEST(Callback, SqpStopInsideRestorationArrivesAsTheSubSolvesOwnVerdict) {
    InfeasibleCircleLineModel model;
    const SqpOptions opts = quiet_sqp();

    // The reference: restoration runs to a CERTIFICATE of local infeasibility.
    SqpDriver reference_driver(opts);
    const SqpSolution reference = reference_driver.solve(model);
    ASSERT_EQ(reference.status, SolveStatus::kInfeasible);
    ASSERT_TRUE(reference.infeasibility_certified)
        << "fixture premise: the restoration phase must reach the certificate, or the pin "
           "below cannot show a stop withholding it";
    ASSERT_GE(reference.counters.restoration_iters, 2);

    SqpDriver driver(opts);
    bool stopped = false;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        if (!stopped && e.depth.value_or(0) == 1) {
            stopped = true;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpSolution sol = driver.solve(model);
    ASSERT_TRUE(stopped);

    // THE SUB-SOLVE'S OWN VERDICT REACHES THE PARENT. Its point is still
    // infeasible, so the phase takes the "not feasible enough" exit and hands
    // back kInterrupted -- the case enter_restoration's status switch gained in
    // M6 W5 T8.6. Without that case the switch (which has no `default`) would
    // fall through and the parent would report whatever the previous
    // restoration left in st.resto.status.
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
    EXPECT_LT(sol.counters.restoration_iters, reference.counters.restoration_iters);

    // AND NO CERTIFICATE IS CLAIMED. Only a restoration phase that ran to its
    // OWN kOptimal certifies; a phase the caller stopped has proved nothing
    // about the model, and must not be allowed to say otherwise.
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_TRUE(reference.infeasibility_certified);
}

TEST(Callback, ZeroMajorExitFiresOnceOrNever) {
    NonFiniteStartModel model;
    SqpDriver driver(quiet_sqp());
    std::vector<Seen> seen;
    driver.set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        return CallbackAction::kContinue;
    });
    const SqpSolution sol = driver.solve(model);

    // ONCE. The non-finite-start exit pushes exactly one history row and solves
    // no subproblem, and the callback fires for that row like any other -- the
    // rule is "once", not "never".
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(sol.history.size(), 1u);
    EXPECT_EQ(sol.counters.major_iters, 0);
    ASSERT_EQ(seen.size(), 1u);

    // NaN IN ALL FOUR: nothing was measured at that point, and a zero would
    // read as a converged residual.
    EXPECT_TRUE(std::isnan(seen[0].stationarity));
    EXPECT_TRUE(std::isnan(seen[0].feasibility_e));
    EXPECT_TRUE(std::isnan(seen[0].feasibility_i));
    EXPECT_TRUE(std::isnan(seen[0].complementarity));
    EXPECT_EQ(seen[0].iteration, 0);
    EXPECT_EQ(seen[0].depth.value_or(-1), 0);
}

// ===========================================================================
// (3) THE INTERIOR-POINT ENGINE
// ===========================================================================

TEST(Callback, IpmContinuingEventObservesTheCommittedPoint) {
    NLPSolver solver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    solver.optimizer_->set_options(quiet_ipm(solver));
    solver.transcribe();

    Recorder rec;
    solver.optimizer_->set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.optimizer_->solve(*solver.nlp_, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(rec.seen.size(), 2u) << "non-vacuous: the solve really iterates";

    // THE FIRST EVENT IS THE START POINT, which nothing stepped to.
    EXPECT_EQ(rec.seen[0].step_norm, 0.0);
    ASSERT_EQ(rec.seen[0].x.size(), hs071_start().size());
    // THE POINT IT DESCRIBES IS THE SOLVE'S OWN START, which is x0 after the
    // interior initialization has pushed it off its bounds -- a displacement of
    // the order of that push (5e-3 on this fixture's widest coordinate) and not
    // a step, which is why `step_norm` above is exactly 0 while these are NEAR.
    EXPECT_LT((rec.seen[0].x - hs071_start()).lpNorm<Eigen::Infinity>(), 1e-2)
        << "the first event describes the point the solve started at";
    EXPECT_GT((rec.seen[0].x - hs071_start()).lpNorm<Eigen::Infinity>(), 0.0)
        << "and it is the PUSHED start, not the caller's raw x0 -- if this fires the "
           "initialization stopped moving the point and the bound above is vacuous";

    // EVERY LATER EVENT DESCRIBES THE COMMITTED POINT OF THE STEP BEFORE IT, so
    // the distance between consecutive events' points IS that event's
    // step_norm. This is the whole of behaviour change (2): the old late
    // callback fired below the factorization of the iterate it described, and
    // the diagnostics it carried were of the point before the commit.
    for (std::size_t k = 1; k < rec.seen.size(); ++k) {
        SCOPED_TRACE("event " + std::to_string(k));
        const double moved = (rec.seen[k].x - rec.seen[k - 1].x).lpNorm<Eigen::Infinity>();
        EXPECT_NEAR(moved, rec.seen[k].step_norm, 1e-9 * (1.0 + rec.seen[k].step_norm));
    }

    // AND THE LAST EVENT DESCRIBES THE RETURNED POINT: every in-loop exit
    // breaks above the commit, so the terminal event's iterate is the result's.
    ASSERT_EQ(rec.seen.back().x.size(), r.x.size());
    for (Eigen::Index i = 0; i < r.x.size(); ++i) {
        EXPECT_EQ(std::bit_cast<std::uint64_t>(rec.seen.back().x[i]),
                  std::bit_cast<std::uint64_t>(r.x[i]));
    }
}

namespace {
// A sink that counts `ipm.iter` lines and nothing else.
class IterCountingSink : public hven::solvers::TraceSink {
  public:
    Index rows = 0;
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { ++rows; }
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}
};
} // namespace

TEST(Callback, IpmTerminalRowStillFires) {
    NLPSolver solver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    solver.optimizer_->set_options(quiet_ipm(solver));
    solver.transcribe();

    IterCountingSink sink;
    solver.optimizer_->attach_trace(&sink);
    Recorder rec;
    solver.optimizer_->set_iteration_callback(rec.hook());
    const hven::solvers::IpmResult r = solver.optimizer_->solve(*solver.nlp_, hs071_start());

    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_GT(sink.rows, 1);
    // EXACTLY the trace's own row count -- the terminal row's event included,
    // which is the claim: moving the continuing dispatch to the top of the next
    // iteration loses no event and invents none.
    EXPECT_EQ(static_cast<Index>(rec.seen.size()), sink.rows);
    EXPECT_EQ(static_cast<Index>(rec.seen.size()), r.iterations);
}

TEST(Callback, IpmStopEndsThePhaseSequence) {
    NLPSolver solver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    {
        hven::solvers::IpmOptions o = quiet_ipm(solver);
        o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
        solver.optimizer_->set_options(std::move(o));
    }
    solver.transcribe();

    std::vector<Seen> seen;
    solver.optimizer_->set_iteration_callback([&](const IterationEvent &e) {
        seen.push_back(copy_of(e));
        // The second event of phase 0: the first describes the start point, so
        // stopping there would prove nothing about a committed iterate.
        return seen.size() == 2 ? CallbackAction::kStop : CallbackAction::kContinue;
    });
    const hven::solvers::IpmResult r = solver.optimizer_->solve(*solver.nlp_, hs071_start());

    EXPECT_EQ(r.status, SolveStatus::kInterrupted);
    EXPECT_EQ(solver.optimizer_->last_stop_reason(), hven::solvers::IpmStopReason::kInterrupted);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_EQ(r.phases[0].status, SolveStatus::kInterrupted);
    EXPECT_EQ(r.phases[0].stop_reason, hven::solvers::IpmStopReason::kInterrupted);
    EXPECT_FALSE(r.phases[1].ran) << "a stop ends the phase SEQUENCE, not only the phase";

    // NO WORK WAS SPENT ON THE STOPPED ITERATION. The stop is honoured
    // pre-factorization, so the factorization count stands where the callback
    // observed it -- the whole reason the interrupt is not folded into the
    // terminal conjunction.
    EXPECT_EQ(seen.size(), 2u) << "the solve stopped at the event that returned kStop";

    // AND THE CALLER'S OWN PHASE COUNT: every event carries the phase index it
    // belongs to, and both of these are phase 0's.
    for (const Seen &e : seen) {
        ASSERT_TRUE(e.phase.has_value());
        EXPECT_EQ(*e.phase, 0);
        EXPECT_FALSE(e.depth.has_value()) << "depth is the SQP's field";
        EXPECT_TRUE(e.mu.has_value()) << "mu is this engine's";
    }
}

// ===========================================================================
// (4) A THROWING CALLBACK, ON BOTH ENGINES
// ===========================================================================

TEST(Callback, ThrowingCallbackPropagatesAndTheSolverIsReusable) {
    // ---- the SQP ----
    {
        Hs071View view;
        Ledger ledger;
        SqpDriver driver(quiet_sqp());
        driver.attach_ledger(&ledger, "cb");

        std::ostringstream os;
        hven::solvers::JsonLinesTraceSink sink(os);
        driver.attach_trace(&sink);

        // THE SECOND ROW, so at least one subproblem has already been solved
        // and its QP-engine record already written when the throw happens.
        driver.set_iteration_callback([](const IterationEvent &e) -> CallbackAction {
            if (e.iteration == 1) {
                throw std::runtime_error("a callback that leaves the solve by throwing");
            }
            return CallbackAction::kContinue;
        });
        EXPECT_THROW((void)driver.solve(*view.bridge, hs071_start()), std::runtime_error);

        // NO RECORD OF THE ABANDONED SOLVE. The solve-begin/solve-end trace pair
        // and the ledger record are deliberately NON-RAII (W2's fix-round ruling
        // R1), so a throw out of the loop leaves a begin and no end.
        const std::string text = os.str();
        EXPECT_NE(text.find("\"sqp.solve.begin\""), std::string::npos);
        EXPECT_EQ(text.find("\"sqp.solve.end\""), std::string::npos);
        EXPECT_TRUE(ledger.sqp_records().empty());

        // BUT THE SUBPROBLEM RECORDS ALREADY EMITTED STAY, which is today's
        // behaviour for ANY throw out of the loop and is stated here so that a
        // later change does not "tidy" them away: they record work that was
        // really done.
        EXPECT_FALSE(ledger.records().empty())
            << "the QP engine's own records of the subproblems this solve DID run are not "
               "rolled back by the throw";
        const std::size_t qp_records_at_throw = ledger.records().size();

        // AND THE DRIVER IS REUSABLE: the in-flight guard cleared on the unwind
        // and the borrowed model went with the frame that borrowed it, so the
        // next solve is bit-for-bit a fresh driver's.
        driver.clear_iteration_callback();
        const SqpSolution again = driver.solve(*view.bridge, hs071_start());
        SqpDriver fresh(quiet_sqp());
        const SqpSolution baseline = fresh.solve(*view.bridge, hs071_start());
        EXPECT_EQ(again.status, SolveStatus::kOptimal);
        EXPECT_EQ(again.status, baseline.status);
        EXPECT_TRUE(bit_equal(again.x, baseline.x));
        EXPECT_TRUE(bit_equal(again.lambda_e, baseline.lambda_e));
        EXPECT_TRUE(bit_equal(again.lambda_i, baseline.lambda_i));
        EXPECT_EQ(again.counters.major_iters, baseline.counters.major_iters);
        EXPECT_EQ(again.history.size(), baseline.history.size());
        EXPECT_GT(ledger.records().size(), qp_records_at_throw)
            << "the reused driver's own subproblems are recorded too";
    }

    // ---- the interior-point engine ----
    {
        NLPSolver solver(std::make_shared<hven_drivers_tests::Hs071Problem>());
        solver.optimizer_->set_options(quiet_ipm(solver));
        solver.transcribe();

        std::ostringstream os;
        hven::solvers::JsonLinesTraceSink sink(os);
        solver.optimizer_->attach_trace(&sink);
        solver.optimizer_->set_iteration_callback([](const IterationEvent &e) -> CallbackAction {
            if (e.iteration == 1) {
                throw std::runtime_error("a callback that leaves the solve by throwing");
            }
            return CallbackAction::kContinue;
        });
        EXPECT_THROW((void)solver.optimizer_->solve(*solver.nlp_, hs071_start()),
                     std::runtime_error);
        const std::string text = os.str();
        EXPECT_NE(text.find("\"ipm.solve.begin\""), std::string::npos);
        EXPECT_EQ(text.find("\"ipm.solve.end\""), std::string::npos);

        solver.optimizer_->clear_iteration_callback();
        solver.optimizer_->attach_trace(nullptr);
        const hven::solvers::IpmResult again =
            solver.optimizer_->solve(*solver.nlp_, hs071_start());
        EXPECT_EQ(again.status, SolveStatus::kOptimal);
    }
}

// ===========================================================================
// (5) THE SURFACE ITSELF
// ===========================================================================

// A callback that only READS cannot move a solve. This is the pin the interior
// corpus leg's second run is the large-scale version of.
TEST(Callback, AReadOnlyCallbackChangesNothingOnEitherEngine) {
    // ---- the SQP ----
    {
        Hs071View view;
        SqpDriver with(quiet_sqp());
        Recorder rec;
        with.set_iteration_callback(rec.hook());
        const SqpSolution attached = with.solve(*view.bridge, hs071_start());

        SqpDriver without(quiet_sqp());
        const SqpSolution absent = without.solve(*view.bridge, hs071_start());

        EXPECT_EQ(attached.status, absent.status);
        EXPECT_TRUE(bit_equal(attached.x, absent.x));
        EXPECT_TRUE(bit_equal(attached.lambda_e, absent.lambda_e));
        EXPECT_TRUE(bit_equal(attached.lambda_i, absent.lambda_i));
        EXPECT_TRUE(bit_equal(attached.z, absent.z));
        EXPECT_EQ(attached.counters.major_iters, absent.counters.major_iters);
        EXPECT_EQ(attached.counters.qp_minor_iters, absent.counters.qp_minor_iters);
        EXPECT_EQ(attached.counters.factorizations, absent.counters.factorizations);
        EXPECT_EQ(attached.counters.evals_full, absent.counters.evals_full)
            << "an event evaluates nothing, so the evaluation bill cannot move";
        EXPECT_EQ(attached.counters.evals_values, absent.counters.evals_values);
    }

    // ---- the interior-point engine ----
    {
        NLPSolver with(std::make_shared<hven_drivers_tests::Hs071Problem>());
        with.optimizer_->set_options(quiet_ipm(with));
        with.transcribe();
        Recorder rec;
        with.optimizer_->set_iteration_callback(rec.hook());
        const hven::solvers::IpmResult attached = with.optimizer_->solve(*with.nlp_, hs071_start());

        NLPSolver without(std::make_shared<hven_drivers_tests::Hs071Problem>());
        without.optimizer_->set_options(quiet_ipm(without));
        without.transcribe();
        const hven::solvers::IpmResult absent =
            without.optimizer_->solve(*without.nlp_, hs071_start());

        EXPECT_EQ(attached.status, absent.status);
        EXPECT_EQ(attached.iterations, absent.iterations);
        EXPECT_TRUE(bit_equal(attached.x, absent.x));
        EXPECT_TRUE(bit_equal(attached.z, absent.z));
        EXPECT_EQ(attached.kkt_factor_counters.factorize_count,
                  absent.kkt_factor_counters.factorize_count);
        EXPECT_EQ(attached.kkt_analyses_this_call, absent.kkt_analyses_this_call);
    }
}

// clear_iteration_callback() really removes it, on both engines.
TEST(Callback, ClearingTheCallbackStopsTheEvents) {
    Hs071View view;
    SqpDriver driver(quiet_sqp());
    Recorder rec;
    driver.set_iteration_callback(rec.hook());
    (void)driver.solve(*view.bridge, hs071_start());
    const std::size_t first = rec.seen.size();
    ASSERT_GT(first, 0u);

    driver.clear_iteration_callback();
    (void)driver.solve(*view.bridge, hs071_start());
    EXPECT_EQ(rec.seen.size(), first);

    NLPSolver solver(std::make_shared<hven_drivers_tests::Hs071Problem>());
    solver.optimizer_->set_options(quiet_ipm(solver));
    solver.transcribe();
    Recorder ipm_rec;
    solver.optimizer_->set_iteration_callback(ipm_rec.hook());
    (void)solver.optimizer_->solve(*solver.nlp_, hs071_start());
    const std::size_t ipm_first = ipm_rec.seen.size();
    ASSERT_GT(ipm_first, 0u);
    solver.optimizer_->clear_iteration_callback();
    (void)solver.optimizer_->solve(*solver.nlp_, hs071_start());
    EXPECT_EQ(ipm_rec.seen.size(), ipm_first);
}
