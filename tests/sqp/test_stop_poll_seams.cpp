// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-5 G4: the intra-iteration stop polls, on the StopPollSeam
// (hven/core/detail/stop_poll.h; docs/testing.md). The design page's test-plan rows
// (docs/notes/2026-11-m8-ap5-interface-design.md section 3.6):
//
//   T3  a token stop between inertia-ladder attempts returns the iteration's start
//       point. Pinned against the same solve stopped at the top of that iteration
//       (the token requested from that iteration's event): every returned field is
//       bit-equal, so the ladder's abandon point leaves the evaluation exact. (Inside
//       restoration the exit is the same code as the iteration-top stop's, G2's stash
//       included; no small fixture runs a ladder rung there.)
//   T4  a token stop inside the main subproblem's minor loop (walk, SSN, IPQP) and
//       inside an elastic re-solve: the stopped-at row for x_k (qp_solved false),
//       history.size() == major_iters + 1, x == x_k. Pinned against the solve capped
//       at the same major (SolveBudget::max_iterations), whose terminal row is the
//       same row. Inside the SQP restoration sub-solve it composes with G2: the
//       restoration-entry point.
//   T9  armed and never fired, each QP mode: the unarmed solve, counter for counter.
//
// With G3's best tracker (armed on every solve here: a token can fire): a ladder stop
// scores its point into the tracker once, as the iteration-top stop does, and publishes
// the same best_iterate; an SQP abandon publishes the capped twin's best_iterate, and a
// callback's kStopReturnBest on the abandoned row is what it is on the exit test's
// token-stopped row (the token labelled the exit before that row's event fired).
//
// The seam requests the stop on the test's StopSource at a chosen armed poll, which
// no public hook can reach (no user code runs between a poll and the work it guards).

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/detail/core/stop_poll.h>
#include <hven/core/stop.h>
#include <hven/detail/solver/result_access.h>
#include <hven/detail/sqp/sqp_solver_impl.h>
#include <hven/detail/model/nlp_model.h>
#include <hven/detail/model/problem_access.h>
#include <hven/detail/warmstart/warm_start_access.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/ipm/ipm_solver_types.h>
#include <hven/trace/json_lines_trace_sink.h>
#include <hven/solver/solve_result.h>
#include <hven/sqp/sqp_solver.h>
#include <hven/sqp/sqp_solver_types.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "../drivers/support/hs071_problem.h"
#include "support/hs_problems.h"

namespace {

using hven::CallbackAction;
using hven::CallbackScope;
using hven::Index;
using hven::IpmOptions;
using hven::IpmResult;
using hven::IpmSolver;
using hven::IterationEvent;
using hven::IterationPayload;
using hven::Problem;
using hven::QpMode;
using hven::SolveBudget;
using hven::SolveStatus;
using hven::SqpElasticRungEngine;
using hven::SqpHessianMode;
using hven::SqpOptions;
using hven::SqpResult;
using hven::SqpSolver;
using hven::StopReason;
using hven::StopSource;
using hven::Verbosity;
using hven::detail::NlpModel;
using hven::detail::SqpSolverAccess;
using hven::detail::StopPollSite;
using Seam = hven::detail::testing::StopPollSeam;
using Vec = Eigen::VectorXd;
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

constexpr double kInf = std::numeric_limits<double>::infinity();

struct SeamReset {
    SeamReset() { Seam::reset(); }
    ~SeamReset() { Seam::reset(); }
};

bool same_scalar(double a, double b) {
    if (std::isnan(a) && std::isnan(b)) {
        return true;
    }
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool bit_equal(const Vec &a, const Vec &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
        if (!same_scalar(a[i], b[i])) {
            return false;
        }
    }
    return true;
}

// The returned point and its declared evaluation, bit for bit.
void expect_same_point(const hven::SolveResult &a, const hven::SolveResult &b) {
    EXPECT_TRUE(bit_equal(a.x, b.x));
    EXPECT_TRUE(bit_equal(a.lambda, b.lambda));
    EXPECT_TRUE(bit_equal(a.z, b.z));
    EXPECT_TRUE(bit_equal(a.g, b.g));
    EXPECT_TRUE(same_scalar(a.f, b.f));
    EXPECT_TRUE(same_scalar(a.stationarity, b.stationarity));
    EXPECT_TRUE(same_scalar(a.feasibility, b.feasibility));
    EXPECT_TRUE(same_scalar(a.complementarity, b.complementarity));
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_EQ(a.returned_pre_restoration_point, b.returned_pre_restoration_point);
    using hven::detail::WarmStartAccess;
    EXPECT_EQ(a.warm_start.empty(), b.warm_start.empty());
    if (!a.warm_start.empty() && !b.warm_start.empty()) {
        EXPECT_TRUE(bit_equal(WarmStartAccess::primal(a.warm_start),
                              WarmStartAccess::primal(b.warm_start)));
        EXPECT_TRUE(
            bit_equal(WarmStartAccess::eq(a.warm_start), WarmStartAccess::eq(b.warm_start)));
    }
}

// The best iterate both solves published, bit for bit; `elapsed_seconds` is a clock read.
void expect_same_best(const hven::SolveResult &a, const hven::SolveResult &b) {
    EXPECT_EQ(a.returned_best_iterate, b.returned_best_iterate);
    ASSERT_EQ(a.best_iterate.has_value(), b.best_iterate.has_value());
    if (!a.best_iterate) {
        return;
    }
    const hven::IterationSnapshot &x = *a.best_iterate;
    const hven::IterationSnapshot &y = *b.best_iterate;
    EXPECT_EQ(x.iteration, y.iteration);
    EXPECT_EQ(x.recovering, y.recovering);
    EXPECT_TRUE(same_scalar(x.f, y.f));
    EXPECT_TRUE(same_scalar(x.stationarity, y.stationarity));
    EXPECT_TRUE(same_scalar(x.feasibility, y.feasibility));
    EXPECT_TRUE(same_scalar(x.complementarity, y.complementarity));
    EXPECT_TRUE(same_scalar(x.step_norm, y.step_norm));
    ASSERT_EQ(x.ipm.has_value(), y.ipm.has_value());
    if (x.ipm && y.ipm) {
        EXPECT_EQ(x.ipm->phase, y.ipm->phase);
        EXPECT_TRUE(same_scalar(x.ipm->mu, y.ipm->mu));
        EXPECT_TRUE(same_scalar(x.ipm->alpha_primal, y.ipm->alpha_primal));
        EXPECT_TRUE(same_scalar(x.ipm->alpha_dual, y.ipm->alpha_dual));
        EXPECT_EQ(x.ipm->inertia_corrections, y.ipm->inertia_corrections);
        EXPECT_EQ(x.ipm->line_search_trials, y.ipm->line_search_trials);
    }
    ASSERT_EQ(x.sqp.has_value(), y.sqp.has_value());
    if (x.sqp && y.sqp) {
        EXPECT_EQ(x.sqp->depth, y.sqp->depth);
        EXPECT_TRUE(same_scalar(x.sqp->trust_radius, y.sqp->trust_radius));
        EXPECT_TRUE(same_scalar(x.sqp->violation_l1, y.sqp->violation_l1));
        EXPECT_EQ(x.sqp->qp_solved, y.sqp->qp_solved);
        EXPECT_EQ(x.sqp->verdict, y.sqp->verdict);
    }
    EXPECT_TRUE(bit_equal(x.x, y.x));
    EXPECT_TRUE(bit_equal(x.lambda, y.lambda));
    EXPECT_TRUE(bit_equal(x.z, y.z));
    EXPECT_TRUE(bit_equal(x.g, y.g));
    EXPECT_TRUE(bit_equal(x.violation, y.violation));
    EXPECT_TRUE(bit_equal(x.z_lower, y.z_lower));
    EXPECT_TRUE(bit_equal(x.z_upper, y.z_upper));
}

void expect_same_row(const hven::IterationRecord &a, const hven::IterationRecord &b) {
    EXPECT_EQ(a.iteration, b.iteration);
    EXPECT_TRUE(same_scalar(a.f.value(), b.f.value()));
    EXPECT_TRUE(same_scalar(a.stationarity, b.stationarity));
    EXPECT_TRUE(same_scalar(a.feasibility, b.feasibility));
    EXPECT_TRUE(same_scalar(a.complementarity, b.complementarity));
    EXPECT_TRUE(same_scalar(std::max(a.stationarity, a.feasibility),
                            std::max(b.stationarity, b.feasibility)));
    EXPECT_TRUE(same_scalar(a.sqp->violation_l1, b.sqp->violation_l1));
    EXPECT_TRUE(same_scalar(a.sqp->trust_radius, b.sqp->trust_radius));
    EXPECT_TRUE(same_scalar(a.sqp->regularization_dual.value_or(0.0),
                            b.sqp->regularization_dual.value_or(0.0)));
    EXPECT_TRUE(same_scalar(a.step_norm.value_or(0.0), b.step_norm.value_or(0.0)));
    EXPECT_EQ(a.sqp->qp_solved, b.sqp->qp_solved);
    EXPECT_EQ(a.sqp->qp_status.value_or(hven::QpStatus::kOptimal),
              b.sqp->qp_status.value_or(hven::QpStatus::kOptimal));
    EXPECT_EQ(a.sqp->qp_minor_iterations.value_or(0), b.sqp->qp_minor_iterations.value_or(0));
    EXPECT_EQ(a.sqp->qp_factorizations.value_or(0), b.sqp->qp_factorizations.value_or(0));
    EXPECT_EQ(a.sqp->verdict.value_or(hven::StepVerdict::kReject),
              b.sqp->verdict.value_or(hven::StepVerdict::kReject));
    EXPECT_EQ(a.sqp->elastic, b.sqp->elastic);
    EXPECT_EQ(a.soc, b.soc);
    EXPECT_EQ(a.sqp->active_rows, b.sqp->active_rows);
    EXPECT_EQ(a.sqp->active_lower_sides, b.sqp->active_lower_sides);
    EXPECT_EQ(a.sqp->active_upper_sides, b.sqp->active_upper_sides);
    EXPECT_TRUE(same_scalar(a.sqp->ipqp_least_infeasible_primal.value_or(0.0),
                            b.sqp->ipqp_least_infeasible_primal.value_or(0.0)));
}

// A kernel that honours the stop polls no more, and none starts after it: every
// site's armed-poll count is what it was when the seam requested the stop.
void expect_no_poll_after_the_stop() {
    for (int i = 0; i < 4; ++i) {
        EXPECT_EQ(Seam::polls[i], Seam::polls_at_fire[i]) << "site " << i;
    }
}

// ===========================================================================
// T3: THE INERTIA LADDER
// ===========================================================================

// min x0 x1  s.t.  x0 + x1 = 2,  0 <= x <= 2. The Hessian's curvature along the
// constraint is -2, so the early iterations' KKT matrices carry the wrong inertia
// and the ladder runs.
class SaddleOnALine final : public hven::Nlp {
  public:
    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 1}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        xl << 0.0, 0.0;
        xu << 2.0, 2.0;
        gl << 2.0;
        gu << 2.0;
    }
    double eval_f(hven::ConstVecRef x) const override { return x[0] * x[1]; }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override { g << x[1], x[0]; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { g[0] = x[0] + x[1]; }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef, hven::VecRef v) const override { v << 1.0, 1.0; }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 1;
        c << 0;
    }
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef,
                      hven::VecRef v) const override {
        v << obj_factor;
    }
    std::string name() const override { return "SaddleOnALine"; }
};

struct IpmRun {
    IpmResult result;
    std::vector<int> ladder_polls_at_event; // the seam's ladder-poll count as each event fired
    std::vector<bool> recovering;
    std::vector<Vec> x_at_event;
};

// One solve with a token armed. `stop_on_event >= 0`: the token is requested from that
// event (so the iteration-top poll right after it honours it). The seam fires per its
// own settings.
template <class Model>
IpmRun run_ipm(const IpmOptions &opts, const Vec &x0, int stop_on_event, StopSource &source) {
    IpmRun run;
    SolveBudget budget;
    budget.stop = source.token();
    Problem problem(std::make_shared<Model>());
    IpmSolver solver(opts);
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            run.ladder_polls_at_event.push_back(
                Seam::polls[static_cast<int>(StopPollSite::kIpmLadder)]);
            run.recovering.push_back(e.recovering);
            run.x_at_event.push_back(e.x);
            if (static_cast<int>(run.recovering.size()) - 1 == stop_on_event) {
                source.request_stop();
            }
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll);
    run.result = solver.solve(problem, x0, {}, budget);
    return run;
}

IpmOptions quiet_ipm() {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

// The iteration whose ladder the `poll`-th ladder poll (1-based) belongs to: the last
// event that fired before it.
int iteration_of_ladder_poll(const IpmRun &dry, int poll) {
    int k = -1;
    for (std::size_t e = 0; e < dry.ladder_polls_at_event.size(); ++e) {
        if (dry.ladder_polls_at_event[e] < poll) {
            k = static_cast<int>(e);
        }
    }
    return k;
}

TEST(StopPollSeam, T3ATokenStopBetweenLadderAttemptsReturnsTheIterationsStartPoint) {
    const SeamReset reset;
    Vec x0(2);
    x0 << 1.2, 0.8;
    for (const hven::IpmInertiaMode mode :
         {hven::IpmInertiaMode::kClassic, hven::IpmInertiaMode::kProximalRegularization}) {
        SCOPED_TRACE(mode == hven::IpmInertiaMode::kClassic ? "classic" : "proximal");
        IpmOptions opts = quiet_ipm();
        opts.advanced.inertia_mode = mode;

        Seam::reset();
        StopSource dry_source;
        const IpmRun dry = run_ipm<SaddleOnALine>(opts, x0, -1, dry_source);
        const int total = Seam::polls[static_cast<int>(StopPollSite::kIpmLadder)];
        ASSERT_GE(total, 2) << "premise: the ladder runs (wrong inertia on this fixture)";
        ASSERT_EQ(dry.result.status, SolveStatus::kOptimal) << dry.result.message;

        // The first poll, and a later one, each against its own top-of-iteration twin.
        for (const int poll : {1, total}) {
            SCOPED_TRACE(poll);
            const int k = iteration_of_ladder_poll(dry, poll);
            ASSERT_GE(k, 0);

            Seam::reset();
            StopSource source;
            Seam::source = &source;
            Seam::fire_site = static_cast<int>(StopPollSite::kIpmLadder);
            Seam::fire_at = poll;
            const IpmRun stopped = run_ipm<SaddleOnALine>(opts, x0, -1, source);
            ASSERT_EQ(Seam::fired_at, poll);
            expect_no_poll_after_the_stop();
            const Index stopped_factorizations = stopped.result.work.factorizations;

            Seam::reset();
            StopSource twin_source;
            const IpmRun twin = run_ipm<SaddleOnALine>(opts, x0, k, twin_source);

            EXPECT_EQ(stopped.result.status, SolveStatus::kInterrupted) << stopped.result.message;
            EXPECT_EQ(stopped.result.stop_reason, StopReason::kStopRequested);
            EXPECT_EQ(stopped.result.message, twin.result.message);
            EXPECT_EQ(stopped.recovering.size(), static_cast<std::size_t>(k + 1))
                << "no extra iteration, no extra event";
            EXPECT_EQ(twin.recovering.size(), static_cast<std::size_t>(k + 1));
            EXPECT_TRUE(bit_equal(stopped.result.x, stopped.x_at_event.back()))
                << "the iteration's start point, the last event's";
            expect_same_point(stopped.result, twin.result);
            // The ladder's attempts were spent before the stop; nothing after it.
            EXPECT_GT(stopped_factorizations, twin.result.work.factorizations);
        }
    }
}

// T3 with the best tracker (M8 AP-5 G3): a ladder stop scores the iteration's point into
// the tracker inside the shared stop exit, once, as the iteration-top stop does -- the
// published best_iterate is the top-of-iteration twin's, field for field. On this
// fixture the start point already satisfies the linear equality, so under criterion
// kEquality (set here; ties go to the later iterate) the stopped-at point is the best: a
// ladder exit that skipped the scoring would publish the previous iterate (or none at
// iteration 0). Under return_best too, which never substitutes on a stop.
TEST(StopPollSeam, T3ALadderStopScoresItsPointIntoTheBestTracker) {
    const SeamReset reset;
    Vec x0(2);
    x0 << 1.2, 0.8;
    for (const bool return_best : {false, true}) {
        for (const hven::IpmInertiaMode mode :
             {hven::IpmInertiaMode::kClassic, hven::IpmInertiaMode::kProximalRegularization}) {
            SCOPED_TRACE(return_best ? "return_best" : "default");
            SCOPED_TRACE(mode == hven::IpmInertiaMode::kClassic ? "classic" : "proximal");
            IpmOptions opts = quiet_ipm();
            opts.advanced.inertia_mode = mode;
            opts.common.return_best =
                return_best ? hven::ReturnBest::kOnAnyNonOptimal : hven::ReturnBest::kNever;

            Seam::reset();
            StopSource dry_source;
            const IpmRun dry = run_ipm<SaddleOnALine>(opts, x0, -1, dry_source);
            const int total = Seam::polls[static_cast<int>(StopPollSite::kIpmLadder)];
            ASSERT_GE(total, 2) << "premise: the ladder runs";

            for (const int poll : {1, total}) {
                SCOPED_TRACE(poll);
                const int k = iteration_of_ladder_poll(dry, poll);
                ASSERT_GE(k, 0);

                Seam::reset();
                StopSource source;
                Seam::source = &source;
                Seam::fire_site = static_cast<int>(StopPollSite::kIpmLadder);
                Seam::fire_at = poll;
                const IpmRun stopped = run_ipm<SaddleOnALine>(opts, x0, -1, source);
                ASSERT_EQ(Seam::fired_at, poll);

                Seam::reset();
                StopSource twin_source;
                const IpmRun twin = run_ipm<SaddleOnALine>(opts, x0, k, twin_source);

                ASSERT_EQ(stopped.result.status, SolveStatus::kInterrupted);
                EXPECT_FALSE(stopped.result.returned_best_iterate)
                    << "a token stop never substitutes";
                ASSERT_TRUE(stopped.result.best_iterate.has_value())
                    << "the armed tracker publishes";
                EXPECT_EQ(stopped.result.best_iterate->iteration, static_cast<Index>(k))
                    << "the stopped-at point was scored";
                EXPECT_TRUE(bit_equal(stopped.result.best_iterate->x, stopped.result.x));
                expect_same_best(stopped.result, twin.result);
            }
        }
    }
}

// ===========================================================================
// T4: THE SQP MAIN SUBPROBLEM'S MINOR LOOPS
// ===========================================================================

SqpOptions quiet_sqp(QpMode mode) {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.qp_mode = mode;
    return o;
}

Vec hs071_start() {
    Vec x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

StopPollSite site_of(QpMode mode) {
    switch (mode) {
    case QpMode::kWalk:
        return StopPollSite::kWalk;
    case QpMode::kSsn:
        return StopPollSite::kSsn;
    case QpMode::kIpqp:
        return StopPollSite::kIpqp;
    }
    return StopPollSite::kWalk;
}

struct SqpRun {
    SqpResult result;
    Index events = 0;
    bool disarmed = false; // no stop flag left on the driver or any QP engine after the solve
};

// HS071 behind a wrapper whose Hessian evaluation can request the stop: on the
// `stop_at`-th call (1-based; 0 never), i.e. while a major's subproblem is being built.
// Its Jacobian evaluation can too (`jac_stop_at`): the full evaluation an accepted step's
// commit pays, after the post-push poll, so the next major's exit test sees the stop.
struct Hs071StopOnHessian final : hven::Nlp {
    hven_drivers_tests::Hs071Problem inner;
    StopSource *source = nullptr;
    int stop_at = 0;
    mutable int calls = 0;
    int jac_stop_at = 0;
    mutable int jac_calls = 0;
    hven::NlpDimensions dimensions() const override { return inner.dimensions(); }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        inner.bounds(xl, xu, gl, gu);
    }
    double eval_f(hven::ConstVecRef x) const override { return inner.eval_f(x); }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        inner.eval_grad_f(x, g);
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { inner.eval_g(x, g); }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        inner.jacobian_structure(r, c);
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        inner.eval_jacobian(x, v);
        if (++jac_calls == jac_stop_at && source != nullptr) {
            source->request_stop();
        }
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        inner.hessian_structure(r, c);
    }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        inner.eval_hessian(x, obj_factor, lambda, v);
        if (++calls == stop_at && source != nullptr) {
            source->request_stop();
        }
    }
    std::string name() const override { return "Hs071StopOnHessian"; }
};

// One solve of `problem` with a token armed, on `solver` when given (a re-solve) or on a
// fresh one.
SqpRun run_problem(Problem &problem, const SqpOptions &opts, StopSource &source,
                   std::optional<Index> max_iterations, SqpSolver *reuse = nullptr) {
    SqpRun run;
    SolveBudget budget;
    budget.stop = source.token();
    budget.max_iterations = max_iterations;
    std::optional<SqpSolver> own;
    SqpSolver &solver = reuse != nullptr ? *reuse : own.emplace(opts);
    solver.set_iteration_callback([&](const IterationEvent &) {
        ++run.events;
        return CallbackAction::kContinue;
    });
    run.result = solver.solve(problem, hs071_start(), {}, budget);
    run.disarmed = SqpSolverAccess::qp_stop_disarmed(solver);
    return run;
}

SqpRun run_hs071(const SqpOptions &opts, StopSource &source, std::optional<Index> max_iterations,
                 SqpSolver *reuse = nullptr) {
    Problem problem(std::make_shared<hven_drivers_tests::Hs071Problem>());
    return run_problem(problem, opts, source, max_iterations, reuse);
}

// The abandoned subproblem's work is in the stopped solve's aggregates: at least the
// capped twin's, and strictly more of the stopped kernel's own when it spent any.
enum class AbandonedWork { kAtLeast, kWalkMinors, kSsnSpent, kIpqpSpent, kNone };

void expect_abandoned_work(const SqpResult &stopped, const SqpResult &capped, AbandonedWork w) {
    const hven::SqpCounters &s = stopped.counters;
    const hven::SqpCounters &c = capped.counters;
    if (w == AbandonedWork::kNone) {
        EXPECT_EQ(s.factorizations, c.factorizations) << "no kernel ran for the abandoned major";
        EXPECT_EQ(s.qp_minor_iters, c.qp_minor_iters);
        EXPECT_EQ(s.ssn.ssn_iters, c.ssn.ssn_iters);
        EXPECT_EQ(s.ipqp.ipqp_iters, c.ipqp.ipqp_iters);
        EXPECT_EQ(s.symbolic_analyses, c.symbolic_analyses);
        return;
    }
    EXPECT_GE(s.factorizations, c.factorizations);
    EXPECT_GE(s.qp_minor_iters, c.qp_minor_iters);
    EXPECT_GE(s.ssn.ssn_iters, c.ssn.ssn_iters);
    EXPECT_GE(s.ipqp.ipqp_iters, c.ipqp.ipqp_iters);
    switch (w) {
    case AbandonedWork::kWalkMinors:
        EXPECT_GT(s.qp_minor_iters, c.qp_minor_iters) << "the walk's minors are counted";
        break;
    case AbandonedWork::kSsnSpent:
        EXPECT_GT(s.ssn.ssn_iters, c.ssn.ssn_iters) << "the SSN's minors are counted";
        EXPECT_GT(s.factorizations, c.factorizations) << "and so are its factorizations";
        break;
    case AbandonedWork::kIpqpSpent:
        EXPECT_GT(s.ipqp.ipqp_iters, c.ipqp.ipqp_iters) << "the IPQP's minors are counted";
        EXPECT_GT(s.factorizations, c.factorizations) << "and so are its factorizations";
        break;
    default:
        break;
    }
}

// The stopped solve against the solve capped at the same major: the same point, the
// same rows, the same best iterate. Only the abandoned subproblem's work differs. A token
// stop never substitutes the best (kStopReturnBest is a callback's), and the twin's cap
// does not either (return_best_on_max_iterations off), so both return x_k; both track
// (a callback and a token arm the tracker) and publish the best over the same rows.
void expect_stopped_at_major(const SqpResult &stopped, const SqpResult &capped) {
    EXPECT_EQ(stopped.status, SolveStatus::kInterrupted) << stopped.message;
    EXPECT_EQ(stopped.stop_reason, StopReason::kStopRequested);
    EXPECT_EQ(capped.status, SolveStatus::kIterationLimit) << capped.message;
    EXPECT_FALSE(stopped.returned_best_iterate);
    EXPECT_FALSE(capped.returned_best_iterate);
    EXPECT_TRUE(stopped.best_iterate.has_value()) << "an armed tracker publishes on a stop";
    expect_same_best(stopped, capped);
    ASSERT_FALSE(stopped.history.empty());
    EXPECT_FALSE(stopped.history.back().sqp->qp_solved) << "the stopped-at row";
    EXPECT_EQ(static_cast<Index>(stopped.history.size()), stopped.counters.major_iters + 1);
    EXPECT_EQ(stopped.counters.major_iters, capped.counters.major_iters);
    ASSERT_EQ(stopped.history.size(), capped.history.size());
    for (std::size_t i = 0; i < stopped.history.size(); ++i) {
        SCOPED_TRACE(i);
        expect_same_row(stopped.history[i], capped.history[i]);
    }
    EXPECT_TRUE(bit_equal(stopped.x, capped.x));
    EXPECT_TRUE(bit_equal(stopped.lambda, capped.lambda));
    EXPECT_TRUE(bit_equal(stopped.z, capped.z));
    EXPECT_TRUE(bit_equal(stopped.g, capped.g));
    EXPECT_TRUE(same_scalar(stopped.f, capped.f));
    EXPECT_TRUE(same_scalar(stopped.stationarity, capped.stationarity));
    EXPECT_TRUE(same_scalar(stopped.feasibility, capped.feasibility));
    EXPECT_TRUE(same_scalar(stopped.complementarity, capped.complementarity));
}

AbandonedWork spent_by(QpMode mode) {
    switch (mode) {
    case QpMode::kWalk:
        return AbandonedWork::kWalkMinors;
    case QpMode::kSsn:
        return AbandonedWork::kSsnSpent;
    case QpMode::kIpqp:
        return AbandonedWork::kIpqpSpent;
    }
    return AbandonedWork::kAtLeast;
}

// Per armed solve of `site`, its poll count, from a run that fires nothing.
std::vector<int> polls_per_solve_of(const SqpOptions &opts, StopPollSite site) {
    std::vector<int> polls;
    Seam::reset();
    StopSource source;
    Seam::record_polls_per_solve = &polls;
    Seam::record_site = static_cast<int>(site);
    (void)run_hs071(opts, source, std::nullopt);
    Seam::record_polls_per_solve = nullptr;
    return polls;
}

// T4 on HS071 under `opts`: the stop fires inside the kernel's subproblem from major 1
// on, at the middle poll of the longest one (`first_poll`: at its first poll instead).
void t4_main_subproblem(const SqpOptions &opts, bool first_poll) {
    const QpMode mode = opts.qp_mode;
    {
        const int site = static_cast<int>(site_of(mode));

        Seam::reset();
        StopSource dry_source;
        const SqpRun dry = run_hs071(opts, dry_source, std::nullopt);
        ASSERT_EQ(dry.result.status, SolveStatus::kOptimal) << dry.result.message;
        ASSERT_EQ(dry.result.counters.restoration_iters, 0) << "premise: no restoration";
        const int solves = Seam::solves[site];
        ASSERT_GE(solves, 2) << "premise: this mode's kernel solves subproblems here";

        // The fire point: the middle poll of the subproblem with the most polls, which the
        // unstopped solve follows with at least one more (so a kernel that ignored the stop
        // would poll again). Recorded per solve from a run that fires nothing.
        const std::vector<int> polls_per_solve = polls_per_solve_of(opts, site_of(mode));
        ASSERT_EQ(static_cast<int>(polls_per_solve.size()), solves);
        // From the second subproblem on: the comparison caps the solve at the stopped major.
        int fire_solve = 2;
        for (int s = 2; s <= solves; ++s) {
            if (polls_per_solve[static_cast<std::size_t>(s - 1)] >
                polls_per_solve[static_cast<std::size_t>(fire_solve - 1)]) {
                fire_solve = s;
            }
        }
        const int most = polls_per_solve[static_cast<std::size_t>(fire_solve - 1)];
        ASSERT_GE(most, 3) << "premise: a subproblem takes three minors or more";
        const int fire_index = first_poll ? 0 : most / 2;

        Seam::reset();
        StopSource source;
        Seam::source = &source;
        Seam::fire_site = site;
        Seam::fire_solve = fire_solve;
        Seam::fire_index = fire_index;
        const SqpRun stopped = run_hs071(opts, source, std::nullopt);
        ASSERT_GE(Seam::fired_at, 1) << "the seam reached the chosen poll";
        expect_no_poll_after_the_stop();
        EXPECT_EQ(stopped.events, static_cast<Index>(stopped.result.history.size()))
            << "the stopped-at row's event fired";
        EXPECT_TRUE(stopped.disarmed) << "the stop flag is withdrawn at the solve's exit";

        const Index k = stopped.result.counters.major_iters;
        ASSERT_GE(k, 1);
        StopSource cap_source;
        Seam::reset();
        const SqpRun capped = run_hs071(opts, cap_source, k);
        expect_stopped_at_major(stopped.result, capped.result);
        // At the first poll the walk has taken no minor; the tiers may have paid their
        // set-up, so only "at least" holds there.
        expect_abandoned_work(stopped.result, capped.result,
                              first_poll ? AbandonedWork::kAtLeast : spent_by(mode));
    }
}

TEST(StopPollSeam, T4ATokenStopInsideTheMainSubproblemReturnsXkOnEachQpMode) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        t4_main_subproblem(quiet_sqp(mode), false);
    }
}

// The first poll of a subproblem (index 0): the kernel is entered and leaves at once.
TEST(StopPollSeam, T4ATokenStopAtASubproblemsFirstPollReturnsXkOnEachQpMode) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        t4_main_subproblem(quiet_sqp(mode), true);
    }
}

// Through the L-BFGS lift (QnLiftBridge wraps every kernel's solve): the same polls.
TEST(StopPollSeam, T4ATokenStopThroughTheLbfgsLiftReturnsXkOnEachQpMode) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        SqpOptions opts = quiet_sqp(mode);
        opts.hessian_mode = SqpHessianMode::kLbfgsDamped;
        t4_main_subproblem(opts, false);
    }
}

// A stop in major 0: the start point, its row, and no major counted.
TEST(StopPollSeam, T4ATokenStopInMajorZeroReturnsTheStartPoint) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        const StopPollSite site = site_of(mode);
        const std::vector<int> polls = polls_per_solve_of(opts, site);
        ASSERT_FALSE(polls.empty());
        Seam::reset();
        StopSource source;
        Seam::source = &source;
        Seam::fire_site = static_cast<int>(site);
        Seam::fire_solve = 1;
        Seam::fire_index = polls[0] / 2;
        const SqpRun stopped = run_hs071(opts, source, std::nullopt);
        ASSERT_GE(Seam::fired_at, 1);
        expect_no_poll_after_the_stop();
        const SqpResult &r = stopped.result;
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_EQ(r.counters.major_iters, 0);
        ASSERT_EQ(r.history.size(), 1u);
        EXPECT_FALSE(r.history[0].sqp->qp_solved);
        EXPECT_EQ(stopped.events, 1);
        EXPECT_TRUE(bit_equal(r.x, hs071_start()));
        EXPECT_TRUE(stopped.disarmed);
    }
}

// A stop requested while a major's subproblem is BUILT (the model's Hessian evaluation)
// abandons the major before any kernel is entered: no kernel work at all for it.
TEST(StopPollSeam, T4ATokenStopDuringTheSubproblemBuildEntersNoKernel) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        const int site = static_cast<int>(site_of(mode));
        auto model = std::make_shared<Hs071StopOnHessian>();
        Problem problem(model);
        Seam::reset();
        StopSource source;
        model->source = &source;
        model->stop_at = 3;
        const SqpRun stopped = run_problem(problem, opts, source, std::nullopt);
        const int kernel_solves = Seam::solves[site];
        const SqpResult &r = stopped.result;
        ASSERT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        const Index k = r.counters.major_iters;
        ASSERT_GE(k, 1);
        EXPECT_EQ(kernel_solves, static_cast<int>(k)) << "no kernel began for the abandoned major";
        EXPECT_TRUE(stopped.disarmed);

        auto plain = std::make_shared<Hs071StopOnHessian>();
        Problem plain_problem(plain);
        Seam::reset();
        StopSource cap_source;
        const SqpRun capped = run_problem(plain_problem, opts, cap_source, k);
        expect_stopped_at_major(r, capped.result);
        expect_abandoned_work(r, capped.result, AbandonedWork::kNone);
    }
}

// One HS071 solve whose callback answers kStopReturnBest to every event after the token
// was requested (and kContinue before), recording the Jacobian calls at each event.
struct BestAskRun {
    SqpResult result;
    std::vector<int> jac_calls_at_event;
    int asked = 0; // events answered kStopReturnBest
    bool disarmed = false;
};

BestAskRun run_asking_best(const std::shared_ptr<Hs071StopOnHessian> &model, const SqpOptions &opts,
                           StopSource &source) {
    BestAskRun run;
    Problem problem(model);
    SolveBudget budget;
    budget.stop = source.token();
    SqpSolver solver(opts);
    solver.set_iteration_callback([&](const IterationEvent &) {
        run.jac_calls_at_event.push_back(model->jac_calls);
        if (source.stop_requested()) {
            ++run.asked;
            return CallbackAction::kStopReturnBest;
        }
        return CallbackAction::kContinue;
    });
    run.result = solver.solve(problem, hs071_start(), {}, budget);
    run.disarmed = SqpSolverAccess::qp_stop_disarmed(solver);
    return run;
}

// G3's kStopReturnBest around an abandon, each QP mode. Inside a major no callback runs
// between the exit test and the subproblem, so the abandon is always the token's and its
// label is set before the abandoned row's event fires; a kStopReturnBest answered there is
// what it is on the exit test's token-stopped row: the token's exit, x_k returned, the
// best published but not substituted. Pinned against the solve whose token is requested
// during major k-1's commit and so stops at major k's exit test, with the same callback.
// (A callback stop latched BEFORE the abandon would label it the callback's and take the
// best through the same exit; no callback can run between the exit test and the abandon,
// so that composition is not reachable at depth 0.)
TEST(StopPollSeam, T4AnAbandonAnsweredWithKStopReturnBestIsTheExitTestTokenStop) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        const int site = static_cast<int>(site_of(mode));
        const std::vector<int> polls = polls_per_solve_of(opts, site_of(mode));
        ASSERT_GE(polls.size(), 2u);
        int fire_solve = 2;
        for (int s = 2; s <= static_cast<int>(polls.size()); ++s) {
            if (polls[static_cast<std::size_t>(s - 1)] >
                polls[static_cast<std::size_t>(fire_solve - 1)]) {
                fire_solve = s;
            }
        }
        const int most = polls[static_cast<std::size_t>(fire_solve - 1)];
        ASSERT_GE(most, 3) << "premise: a subproblem takes three minors or more";

        Seam::reset();
        StopSource dry_source;
        const BestAskRun dry =
            run_asking_best(std::make_shared<Hs071StopOnHessian>(), opts, dry_source);
        ASSERT_EQ(dry.result.status, SolveStatus::kOptimal) << dry.result.message;

        Seam::reset();
        StopSource source;
        Seam::source = &source;
        Seam::fire_site = site;
        Seam::fire_solve = fire_solve;
        Seam::fire_index = most / 2;
        const BestAskRun stopped =
            run_asking_best(std::make_shared<Hs071StopOnHessian>(), opts, source);
        ASSERT_GE(Seam::fired_at, 1);
        expect_no_poll_after_the_stop();
        const SqpResult &r = stopped.result;
        const Index k = r.counters.major_iters;
        ASSERT_GE(k, 1);
        ASSERT_EQ(stopped.asked, 1) << "the abandoned row's event answered kStopReturnBest";
        EXPECT_TRUE(stopped.disarmed);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_FALSE(r.returned_best_iterate) << "the token labelled the exit";
        ASSERT_TRUE(r.best_iterate.has_value());
        ASSERT_FALSE(r.history.empty());
        EXPECT_FALSE(r.history.back().sqp->qp_solved);

        // The exit-test twin: the token requested by major k-1's commit (its Jacobian
        // evaluation, after the post-push poll), seen at major k's exit test.
        ASSERT_GE(dry.jac_calls_at_event.size(), static_cast<std::size_t>(k));
        auto twin_model = std::make_shared<Hs071StopOnHessian>();
        Seam::reset();
        StopSource twin_source;
        twin_model->source = &twin_source;
        twin_model->jac_stop_at = dry.jac_calls_at_event[static_cast<std::size_t>(k - 1)] + 1;
        const BestAskRun twin = run_asking_best(twin_model, opts, twin_source);
        const SqpResult &t = twin.result;
        ASSERT_EQ(t.counters.major_iters, k) << "premise: the twin stops at major k's exit test";
        ASSERT_EQ(Seam::solves[site], fire_solve - 1) << "premise: no kernel entered for major k";
        ASSERT_EQ(twin.asked, 1);
        EXPECT_EQ(t.status, SolveStatus::kInterrupted) << t.message;
        EXPECT_EQ(t.stop_reason, StopReason::kStopRequested);
        EXPECT_EQ(r.message, t.message);
        ASSERT_EQ(r.history.size(), t.history.size());
        for (std::size_t i = 0; i < r.history.size(); ++i) {
            SCOPED_TRACE(i);
            expect_same_row(r.history[i], t.history[i]);
        }
        expect_same_point(r, t);
        expect_same_best(r, t);
    }
}

// A re-solve on the same SqpSolver after an intra-subproblem stop is a fresh solver's.
TEST(StopPollSeam, AResolveAfterAnIntraSubproblemStopIsAFreshSolve) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        const StopPollSite site = site_of(mode);
        const std::vector<int> polls = polls_per_solve_of(opts, site);
        ASSERT_GE(polls.size(), 2u);

        SqpSolver reused(opts);
        Seam::reset();
        StopSource source;
        Seam::source = &source;
        Seam::fire_site = static_cast<int>(site);
        Seam::fire_solve = 2;
        Seam::fire_index = polls[1] / 2;
        const SqpRun stopped = run_hs071(opts, source, std::nullopt, &reused);
        ASSERT_GE(Seam::fired_at, 1);
        ASSERT_EQ(stopped.result.status, SolveStatus::kInterrupted);

        Seam::reset();
        StopSource again_source;
        const SqpRun again = run_hs071(opts, again_source, std::nullopt, &reused);
        Seam::reset();
        StopSource fresh_source;
        const SqpRun fresh = run_hs071(opts, fresh_source, std::nullopt);

        // The engines' symbolic analyses outlive a solve: that count is a re-solve's after an
        // unstopped first solve, everything else a fresh solver's.
        SqpSolver twin(opts);
        Seam::reset();
        StopSource first_source;
        (void)run_hs071(opts, first_source, std::nullopt, &twin);
        StopSource twin_source;
        const SqpRun twin_again = run_hs071(opts, twin_source, std::nullopt, &twin);

        const SqpResult &a = again.result;
        const SqpResult &b = fresh.result;
        EXPECT_EQ(a.status, b.status) << a.message;
        EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
        EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
        EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
        EXPECT_EQ(a.counters.symbolic_analyses, twin_again.result.counters.symbolic_analyses);
        EXPECT_EQ(a.counters.evals_full, b.counters.evals_full);
        EXPECT_EQ(a.counters.ssn.ssn_iters, b.counters.ssn.ssn_iters);
        EXPECT_EQ(a.counters.ipqp.ipqp_iters, b.counters.ipqp.ipqp_iters);
        EXPECT_EQ(a.counters.start_level_used, b.counters.start_level_used);
        ASSERT_EQ(a.history.size(), b.history.size());
        for (std::size_t i = 0; i < a.history.size(); ++i) {
            SCOPED_TRACE(i);
            expect_same_row(a.history[i], b.history[i]);
        }
        EXPECT_TRUE(bit_equal(a.x, b.x));
        EXPECT_TRUE(bit_equal(a.lambda, b.lambda));
        EXPECT_TRUE(bit_equal(a.z, b.z));
        EXPECT_EQ(again.events, fresh.events);
        EXPECT_TRUE(again.disarmed);
    }
}

// The elastic re-solve: InfeasibleCircleLineModel's first subproblem is infeasible, so
// the walk's second solve is the elastic ladder's first rung (tests/sqp/
// test_sqp_restoration.cpp derives the trajectory; copied verbatim from
// tests/drivers/test_callback.cpp).
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
        static const Vec l = Vec::Constant(2, -kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInf);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 2.0, 2.0;
        return x;
    }
};

struct ModelRun {
    SqpResult result;
    Index events = 0;
    std::vector<int> walk_solves_at_event;
    std::vector<int> solves_at_event[4]; // per site (StopPollSite order)
    std::vector<Index> depth;
    std::vector<Vec> x_at_event;
    int solves[4] = {0, 0, 0, 0}; // the seam's armed solves per site, at the solve's end
    bool disarmed = false;
    std::string trace; // JSON lines, when asked for
};

ModelRun run_model(const NlpModel &model, StopSource &source, std::optional<Index> max_iterations,
                   const SqpOptions &opts = quiet_sqp(QpMode::kWalk), bool trace = false) {
    ModelRun run;
    SolveBudget budget;
    budget.stop = source.token();
    budget.max_iterations = max_iterations;
    SqpSolver solver(opts);
    std::ostringstream os;
    hven::JsonLinesTraceSink sink(os);
    if (trace) {
        solver.attach_trace(&sink);
    }
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            ++run.events;
            for (int i = 0; i < 4; ++i) {
                run.solves_at_event[i].push_back(Seam::solves[i]);
            }
            run.walk_solves_at_event.push_back(Seam::solves[static_cast<int>(StopPollSite::kWalk)]);
            run.depth.push_back((e.sqp ? e.sqp->depth : Index{0}));
            run.x_at_event.push_back(e.x);
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    run.result = solver.solve(hven::detail::problem_ref(model), model.start_point(),
                              hven::WarmStart{}, budget);
    for (int i = 0; i < 4; ++i) {
        run.solves[i] = Seam::solves[i];
    }
    run.disarmed = SqpSolverAccess::qp_stop_disarmed(solver);
    if (trace) {
        solver.attach_trace(nullptr);
        run.trace = os.str();
    }
    return run;
}

// Lines of the trace stream that carry every one of `needles`.
Index count_lines(const std::string &stream, std::initializer_list<const char *> needles) {
    Index n = 0;
    std::istringstream in(stream);
    std::string line;
    while (std::getline(in, line)) {
        bool all = true;
        for (const char *k : needles) {
            all = all && line.find(k) != std::string::npos;
        }
        n += all ? 1 : 0;
    }
    return n;
}

TEST(StopPollSeam, T4ATokenStopInsideAnElasticResolveReturnsXk) {
    const SeamReset reset;
    InfeasibleCircleLineModel model;
    StopSource dry_source;
    const ModelRun dry = run_model(model, dry_source, std::nullopt);
    // The parent's rows among the events: restoration's rows (depth 1) interleave with them
    // on a route that enters restoration early, so a history index is not an event index.
    std::vector<std::size_t> parent_event;
    for (std::size_t e = 0; e < dry.depth.size(); ++e) {
        if (dry.depth[e] == 0) {
            parent_event.push_back(e);
        }
    }
    ASSERT_EQ(parent_event.size(), dry.result.history.size());
    // The first parent major (depth 0, after major 0) whose subproblem the elastic tier answered.
    std::size_t k = 1;
    while (k < dry.result.history.size() && !dry.result.history[k].sqp->elastic) {
        ++k;
    }
    ASSERT_LT(k, dry.result.history.size())
        << "premise: a depth-0 major after major 0 has its subproblem answered by the elastic tier";
    // Major k's walk solves start after its predecessor parent row's count: the main
    // subproblem, then the elastic ladder's first rung.
    const int fire_solve = dry.walk_solves_at_event[parent_event[k - 1]] + 2;

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = static_cast<int>(StopPollSite::kWalk);
    Seam::fire_solve = fire_solve;
    const ModelRun stopped = run_model(model, source, std::nullopt);
    ASSERT_GE(Seam::fired_at, 1);
    expect_no_poll_after_the_stop();
    const SqpResult &r = stopped.result;
    EXPECT_FALSE(r.returned_pre_restoration_point);
    EXPECT_EQ(r.history.size(), k + 1);
    EXPECT_GE(r.counters.elastic_activations, 1);
    EXPECT_FALSE(r.history.back().sqp->elastic);

    // The cap counts restoration's iterations too.
    Seam::reset();
    StopSource cap_source;
    const ModelRun capped =
        run_model(model, cap_source, r.counters.major_iters + r.counters.restoration_iters);
    expect_stopped_at_major(r, capped.result);
    EXPECT_EQ(r.counters.restoration_iters, capped.result.counters.restoration_iters);
}

// A stop inside a subproblem of the restoration sub-solve composes with G2: the
// sub-solve is cut short, so the parent returns the point restoration was entered from.
TEST(StopPollSeam, T4AStopInsideARestorationSubproblemReturnsTheEntryPoint) {
    const SeamReset reset;
    InfeasibleCircleLineModel model;
    StopSource dry_source;
    const ModelRun dry = run_model(model, dry_source, std::nullopt);
    ASSERT_TRUE(dry.result.infeasibility_certified) << "premise: restoration runs to a certificate";
    // The walk solves counted at the sub-solve's second event: the subproblems of its
    // first two majors are done by then, and the next solve is one of its own.
    int sub_events = 0;
    int fire_solve = 0;
    for (std::size_t e = 0; e < dry.depth.size(); ++e) {
        if (dry.depth[e] == 1 && ++sub_events == 2) {
            fire_solve = dry.walk_solves_at_event[e] + 1;
        }
    }
    ASSERT_GT(fire_solve, 0) << "premise: the sub-solve takes at least three majors";

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = static_cast<int>(StopPollSite::kWalk);
    Seam::fire_solve = fire_solve;
    const ModelRun stopped = run_model(model, source, std::nullopt);
    ASSERT_GE(Seam::fired_at, 1);
    expect_no_poll_after_the_stop();
    const SqpResult &r = stopped.result;
    EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
    EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
    EXPECT_TRUE(r.returned_pre_restoration_point) << r.message;
    EXPECT_FALSE(r.infeasibility_certified);
    EXPECT_LT(r.counters.restoration_iters, dry.result.counters.restoration_iters);
    // The parent's requesting row is pushed after the sub-solve returns: its event is the
    // last depth-0 one, and its point is the one restoration was entered from.
    ASSERT_FALSE(stopped.depth.empty());
    EXPECT_EQ(stopped.depth.back(), 0);
    std::size_t sub_rows = 0;
    for (Index d : stopped.depth) {
        sub_rows += d == 1 ? 1 : 0;
    }
    EXPECT_GE(sub_rows, 3u) << "the sub-solve's stopped-at row reached the callback";
    EXPECT_TRUE(bit_equal(r.x, stopped.x_at_event.back()));
}

// InfeasibleCircleLineModel whose Jacobian evaluation can request the stop (`jac_stop_at`,
// 1-based; 0 never).
class CircleLineStopOnJacobian final : public InfeasibleCircleLineModel {
  public:
    SpMatRM eval_jac_e(const Vec &x) const override {
        if (++jac_calls == jac_stop_at && source != nullptr) {
            source->request_stop();
        }
        return InfeasibleCircleLineModel::eval_jac_e(x);
    }
    StopSource *source = nullptr;
    int jac_stop_at = 0;
    mutable int jac_calls = 0;
};

struct RestoAskRun {
    SqpResult result;
    std::vector<Index> depth;
    std::vector<int> jac_calls_at_event;
    std::vector<int> walk_solves_at_event;
    int asked = 0;
    Index asked_depth = -1; // the depth of the event that answered kStopReturnBest
};

// The restoration fixture's solve, restoration rows delivered, the callback answering
// kStopReturnBest to every event after the token was requested.
RestoAskRun run_resto_asking_best(const CircleLineStopOnJacobian &model, StopSource &source) {
    RestoAskRun run;
    SolveBudget budget;
    budget.stop = source.token();
    SqpSolver solver(quiet_sqp(QpMode::kWalk));
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            run.depth.push_back((e.sqp ? e.sqp->depth : Index{0}));
            run.jac_calls_at_event.push_back(model.jac_calls);
            run.walk_solves_at_event.push_back(Seam::solves[static_cast<int>(StopPollSite::kWalk)]);
            if (source.stop_requested()) {
                if (++run.asked == 1) {
                    run.asked_depth = (e.sqp ? e.sqp->depth : Index{0});
                }
                return CallbackAction::kStopReturnBest;
            }
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    run.result = solver.solve(hven::detail::problem_ref(model), model.start_point(),
                              hven::WarmStart{}, budget);
    return run;
}

// The same inside the restoration sub-solve: a stop abandoning one of its subproblems,
// with kStopReturnBest answered on the sub-solve's abandoned row (a depth-1 event), ends
// the solve exactly as the stop seen at that sub-major's exit test does, with the same
// callback. Both reach the sub-solve's one stop exit, and (M8 AP-5 G7) the sub-solve's own
// label decides: the token ended it, so the answer is the no-op it is at depth 0 -- the
// parent's exit is the token's and returns the restoration-entry point, not its best.
TEST(StopPollSeam, T4ARestorationAbandonAnsweredWithKStopReturnBestIsTheExitTestStop) {
    const SeamReset reset;
    CircleLineStopOnJacobian dry_model;
    StopSource dry_source;
    const RestoAskRun dry = run_resto_asking_best(dry_model, dry_source);
    ASSERT_TRUE(dry.result.infeasibility_certified) << "premise: restoration runs to a certificate";
    // The sub-solve's second event: its third major's subproblem is the abandoned one.
    std::size_t e2 = 0;
    int sub_events = 0;
    for (std::size_t e = 0; e < dry.depth.size(); ++e) {
        if (dry.depth[e] == 1 && ++sub_events == 2) {
            e2 = e;
        }
    }
    ASSERT_GT(e2, 0u) << "premise: the sub-solve takes at least three majors";

    CircleLineStopOnJacobian model;
    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = static_cast<int>(StopPollSite::kWalk);
    Seam::fire_solve = dry.walk_solves_at_event[e2] + 1;
    const RestoAskRun stopped = run_resto_asking_best(model, source);
    ASSERT_GE(Seam::fired_at, 1);
    expect_no_poll_after_the_stop();
    ASSERT_GE(stopped.asked, 1);
    EXPECT_EQ(stopped.asked_depth, 1) << "the sub-solve's abandoned row answered first";

    // The exit-test twin: the token requested by the sub-solve's commit after its second
    // row (the Jacobian evaluation at the accepted point), seen at its next exit test.
    CircleLineStopOnJacobian twin_model;
    Seam::reset();
    StopSource twin_source;
    twin_model.source = &twin_source;
    twin_model.jac_stop_at = dry.jac_calls_at_event[e2] + 1;
    const RestoAskRun twin = run_resto_asking_best(twin_model, twin_source);
    ASSERT_EQ(Seam::solves[static_cast<int>(StopPollSite::kWalk)], dry.walk_solves_at_event[e2])
        << "premise: the twin's sub-solve entered no kernel after its second row";
    ASSERT_GE(twin.asked, 1);
    EXPECT_EQ(twin.asked_depth, 1);
    ASSERT_EQ(stopped.depth, twin.depth) << "premise: the same rows, the same depths";

    const SqpResult &r = stopped.result;
    const SqpResult &t = twin.result;
    EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
    EXPECT_EQ(r.status, t.status);
    EXPECT_EQ(r.stop_reason, t.stop_reason);
    EXPECT_EQ(r.message, t.message);
    EXPECT_EQ(r.returned_pre_restoration_point, t.returned_pre_restoration_point);
    EXPECT_EQ(r.counters.major_iters, t.counters.major_iters);
    EXPECT_EQ(r.counters.restoration_iters, t.counters.restoration_iters);
    ASSERT_EQ(r.history.size(), t.history.size());
    for (std::size_t i = 0; i < r.history.size(); ++i) {
        SCOPED_TRACE(i);
        expect_same_row(r.history[i], t.history[i]);
    }
    EXPECT_TRUE(bit_equal(r.x, t.x));
    EXPECT_TRUE(bit_equal(r.lambda, t.lambda));
    EXPECT_TRUE(bit_equal(r.z, t.z));
    expect_same_best(r, t);
    // G7: a token stop that already ended the sub-solve is not relabelled by the depth-1
    // answer, at any depth: the token's label and the restoration-entry point.
    EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
    EXPECT_FALSE(r.returned_best_iterate) << r.message;
    EXPECT_TRUE(r.returned_pre_restoration_point) << r.message;
}

// The second-order correction's re-solve is not a safe point: the walk is not armed for
// it. Copied verbatim from tests/drivers/test_callback.cpp (MaratosModel): at theta = 0.1
// the second major's rejected trial is rescued by a SOC re-solve.
class MaratosModel : public NlpModel {
  public:
    explicit MaratosModel(double theta) : theta_(theta) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return 2.0 * (x(0) * x(0) + x(1) * x(1) - 1.0) - x(0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 4.0 * x(0) - 1.0, 4.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &le, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 4.0 + le(0) * 2.0;
        h.insert(1, 1) = obj_scale * 4.0 + le(0) * 2.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, kInf);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << std::cos(theta_), std::sin(theta_);
        return x;
    }

  private:
    double theta_;
};

TEST(StopPollSeam, TheSocResolveIsNotArmed) {
    const SeamReset reset;
    MaratosModel model(0.1);
    StopSource source;
    const ModelRun run = run_model(model, source, std::nullopt);
    ASSERT_EQ(run.result.status, SolveStatus::kOptimal) << run.result.message;
    ASSERT_GE(run.result.counters.soc_steps, 1) << "premise: a SOC re-solve runs";
    ASSERT_EQ(run.result.counters.elastic_activations, 0);
    EXPECT_EQ(Seam::solves[static_cast<int>(StopPollSite::kWalk)], run.result.counters.major_iters)
        << "one armed walk solve per major: the SOC re-solves were not armed";
}

// The walk is re-armed after the SOC re-solve: a stop in the major after the
// correction's is seen inside that major's subproblem.
TEST(StopPollSeam, TheWalkIsReArmedAfterTheSocResolve) {
    const SeamReset reset;
    MaratosModel model(0.1);
    StopSource dry_source;
    const ModelRun dry = run_model(model, dry_source, std::nullopt);
    std::size_t m = 0;
    while (m < dry.result.history.size() && !dry.result.history[m].soc) {
        ++m;
    }
    ASSERT_LT(m + 1, dry.result.history.size()) << "premise: a major follows the SOC's";

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = static_cast<int>(StopPollSite::kWalk);
    Seam::fire_solve = dry.walk_solves_at_event[m] + 1; // major m+1's subproblem
    Seam::fire_index = 0;
    const ModelRun stopped = run_model(model, source, std::nullopt);
    ASSERT_GE(Seam::fired_at, 1) << "the walk was armed again after the correction";
    expect_no_poll_after_the_stop();
    EXPECT_EQ(stopped.result.counters.major_iters, static_cast<Index>(m + 1));
    EXPECT_TRUE(stopped.disarmed);

    Seam::reset();
    StopSource cap_source;
    const ModelRun capped = run_model(model, cap_source, static_cast<Index>(m + 1));
    expect_stopped_at_major(stopped.result, capped.result);
}

// The elastic rung on the IPQP tier's engine (`elastic_rung_engine = kTier`): its minor
// loop is a safe point too.
TEST(StopPollSeam, T4ATokenStopInsideAnIpqpTierElasticRungReturnsXk) {
    const SeamReset reset;
    InfeasibleCircleLineModel model;
    SqpOptions opts = quiet_sqp(QpMode::kWalk);
    opts.advanced.elastic_rung_engine = SqpElasticRungEngine::kTier;
    const int ipqp = static_cast<int>(StopPollSite::kIpqp);
    StopSource dry_source;
    Seam::reset();
    std::vector<int> polls;
    Seam::record_polls_per_solve = &polls;
    Seam::record_site = ipqp;
    const ModelRun dry = run_model(model, dry_source, std::nullopt, opts);
    Seam::record_polls_per_solve = nullptr;
    // The parent's rows among the events (restoration's rows are at depth 1, before the
    // row of the major that entered it), and the first parent major the elastic tier
    // answered. Its rungs run before any restoration that major enters.
    std::vector<std::size_t> parent_event;
    for (std::size_t e = 0; e < dry.depth.size(); ++e) {
        if (dry.depth[e] == 0) {
            parent_event.push_back(e);
        }
    }
    ASSERT_EQ(parent_event.size(), dry.result.history.size());
    std::size_t k = 0;
    for (std::size_t r = 1; r < dry.result.history.size() && k == 0; ++r) {
        if (dry.result.history[r].sqp->elastic) {
            k = r;
        }
    }
    ASSERT_GE(k, 1u) << "premise: the elastic tier answers a subproblem";
    // Major k's first IPQP solve is its elastic rung (the main subproblem is the walk's).
    const int fire_solve = dry.solves_at_event[ipqp][parent_event[k - 1]] + 1;
    ASSERT_LE(fire_solve, static_cast<int>(polls.size()));
    const int rung_polls = polls[static_cast<std::size_t>(fire_solve - 1)];
    ASSERT_GE(rung_polls, 2) << "premise: the rung takes two minors or more";

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = ipqp;
    Seam::fire_solve = fire_solve;
    Seam::fire_index = rung_polls / 2;
    const ModelRun stopped = run_model(model, source, std::nullopt, opts);
    ASSERT_GE(Seam::fired_at, 1);
    expect_no_poll_after_the_stop();
    const SqpResult &r = stopped.result;
    EXPECT_EQ(r.history.size(), k + 1);
    EXPECT_GE(r.counters.elastic_activations, 1);
    EXPECT_FALSE(r.history.back().sqp->elastic);
    EXPECT_TRUE(stopped.disarmed);

    // The cap counts restoration's iterations too.
    Seam::reset();
    StopSource cap_source;
    const ModelRun capped =
        run_model(model, cap_source, r.counters.major_iters + r.counters.restoration_iters, opts);
    expect_stopped_at_major(r, capped.result);
    EXPECT_EQ(r.counters.restoration_iters, capped.result.counters.restoration_iters);
    EXPECT_GT(r.counters.factorizations, capped.result.counters.factorizations);
}

// The SSN warm grade the IPQP chain routes to (HS3 takes that route on most majors): a
// stop inside it abandons the subproblem -- not counted as a route, not traced as one,
// and no walk is started for it.
TEST(StopPollSeam, T4ATokenStopInsideTheSsnWarmGradeRoutesNothing) {
    const SeamReset reset;
    const auto p = hven::test_support::make_hs(3);
    SqpOptions opts = quiet_sqp(QpMode::kIpqp);
    const int ssn = static_cast<int>(StopPollSite::kSsn);
    const int walk = static_cast<int>(StopPollSite::kWalk);
    StopSource dry_source;
    Seam::reset();
    std::vector<int> polls;
    Seam::record_polls_per_solve = &polls;
    Seam::record_site = ssn;
    const ModelRun dry = run_model(*p.model, dry_source, std::nullopt, opts);
    Seam::record_polls_per_solve = nullptr;
    ASSERT_EQ(dry.result.status, SolveStatus::kOptimal) << dry.result.message;
    ASSERT_GE(dry.result.counters.ipqp.ipqp_to_ssn, 2) << "premise: the warm grade runs";
    ASSERT_EQ(static_cast<Index>(polls.size()), dry.result.counters.ipqp.ipqp_to_ssn);
    // The grade from major 1 on (event 0 counts major 0's).
    const int fire_solve = dry.solves_at_event[ssn][0] + 1;
    ASSERT_LE(fire_solve, static_cast<int>(polls.size()));

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = ssn;
    Seam::fire_solve = fire_solve;
    Seam::fire_index = polls[static_cast<std::size_t>(fire_solve - 1)] / 2;
    const ModelRun stopped = run_model(*p.model, source, std::nullopt, opts, true);
    ASSERT_GE(Seam::fired_at, 1);
    expect_no_poll_after_the_stop();
    EXPECT_TRUE(stopped.disarmed);
    const Index k = stopped.result.counters.major_iters;
    ASSERT_GE(k, 1);

    Seam::reset();
    StopSource cap_source;
    const ModelRun capped = run_model(*p.model, cap_source, k, opts, true);
    expect_stopped_at_major(stopped.result, capped.result);
    expect_abandoned_work(stopped.result, capped.result, AbandonedWork::kSsnSpent);
    const hven::SqpCounters &s = stopped.result.counters;
    const hven::SqpCounters &c = capped.result.counters;
    EXPECT_EQ(s.ipqp.ipqp_to_ssn, c.ipqp.ipqp_to_ssn) << "the abandoned grade is not a route";
    EXPECT_EQ(stopped.solves[walk], capped.solves[walk]) << "no walk started for it";
    EXPECT_EQ(count_lines(stopped.trace, {"\"ev\":\"ipqp.route\""}),
              count_lines(capped.trace, {"\"ev\":\"ipqp.route\""}))
        << "nor traced as one";
    EXPECT_EQ(count_lines(stopped.trace, {"\"site\":\"ssn_warm_grade\""}), s.ipqp.ipqp_to_ssn);
}

// The abandoned row is traced with the configured mode, as the exit test's row is, not
// with the abandoned kernel's: under kIpqp with the tier retired (HS77 at a two-iteration
// tier cap: three budget escapes, test_ipqp_dispatch.cpp) the walk owns later majors.
TEST(StopPollSeam, TheAbandonedRowIsTracedWithTheConfiguredMode) {
    const SeamReset reset;
    const auto p = hven::test_support::make_hs(77);
    SqpOptions opts = quiet_sqp(QpMode::kIpqp);
    opts.advanced.ipqp.hard_max_iterations = 2;
    const int walk = static_cast<int>(StopPollSite::kWalk);
    const int ipqp = static_cast<int>(StopPollSite::kIpqp);
    StopSource dry_source;
    Seam::reset();
    const ModelRun dry = run_model(*p.model, dry_source, std::nullopt, opts);
    ASSERT_GT(dry.result.counters.ipqp.ipqp_tier_retired_after, 0) << "premise: the tier retires";
    ASSERT_EQ(dry.result.counters.restoration_iters, 0) << "premise: one event per major";
    // The second major whose only kernel solve is one walk solve (the walk owns it outright),
    // so the rows before it include one the walk owns.
    std::size_t m = 0;
    int owned = 0;
    for (std::size_t e = 1; e < dry.solves_at_event[walk].size() && m == 0; ++e) {
        if (dry.depth[e] == 0 && dry.solves_at_event[ipqp][e] == dry.solves_at_event[ipqp][e - 1] &&
            dry.solves_at_event[walk][e] == dry.solves_at_event[walk][e - 1] + 1 && ++owned == 2) {
            m = e;
        }
    }
    ASSERT_GE(m, 1u) << "premise: a major the walk owns after the retirement";

    Seam::reset();
    StopSource source;
    Seam::source = &source;
    Seam::fire_site = walk;
    Seam::fire_solve = dry.solves_at_event[walk][m - 1] + 1;
    Seam::fire_index = 0;
    const ModelRun stopped = run_model(*p.model, source, std::nullopt, opts, true);
    ASSERT_GE(Seam::fired_at, 1);
    ASSERT_EQ(stopped.result.counters.major_iters, static_cast<Index>(m));

    Seam::reset();
    StopSource cap_source;
    const ModelRun capped = run_model(*p.model, cap_source, static_cast<Index>(m), opts, true);
    ASSERT_GE(count_lines(capped.trace, {"\"ev\":\"iter\"", "\"mode\":\"walk\""}), 1)
        << "premise: the walk owns a traced major before the stop";
    EXPECT_EQ(count_lines(stopped.trace, {"\"ev\":\"iter\"", "\"mode\":\"walk\""}),
              count_lines(capped.trace, {"\"ev\":\"iter\"", "\"mode\":\"walk\""}));
    EXPECT_EQ(count_lines(stopped.trace, {"\"ev\":\"iter\""}),
              count_lines(capped.trace, {"\"ev\":\"iter\""}));
}

// T9: armed and never fired, each QP mode: the unarmed solve.
TEST(StopPollSeam, T9AnArmedUnfiredSolveIsTheUnarmedSolveOnEachQpMode) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        Problem problem(std::make_shared<hven_drivers_tests::Hs071Problem>());
        SqpSolver plain_solver(opts);
        const SqpResult plain = plain_solver.solve(problem, hs071_start());
        Seam::reset();
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        SqpSolver armed_solver(opts);
        const SqpResult armed = armed_solver.solve(problem, hs071_start(), {}, budget);
        EXPECT_TRUE(SqpSolverAccess::qp_stop_disarmed(armed_solver));
        EXPECT_GT(Seam::polls[static_cast<int>(site_of(mode))], 0) << "the polls were armed";
        EXPECT_EQ(armed.status, plain.status);
        EXPECT_EQ(armed.counters.major_iters, plain.counters.major_iters);
        EXPECT_EQ(armed.counters.qp_minor_iters, plain.counters.qp_minor_iters);
        EXPECT_EQ(armed.counters.factorizations, plain.counters.factorizations);
        EXPECT_EQ(armed.counters.evals_full, plain.counters.evals_full);
        ASSERT_EQ(armed.history.size(), plain.history.size());
        for (std::size_t i = 0; i < armed.history.size(); ++i) {
            expect_same_row(armed.history[i], plain.history[i]);
        }
        EXPECT_TRUE(bit_equal(armed.x, plain.x));
        EXPECT_TRUE(bit_equal(armed.lambda, plain.lambda));
    }
}

} // namespace
