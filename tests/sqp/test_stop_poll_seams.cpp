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
// The seam requests the stop on the test's StopSource at a chosen armed poll, which
// no public hook can reach (no user code runs between a poll and the work it guards).

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include <hven/core/detail/stop_poll.h>
#include <hven/core/stop.h>
#include <hven/detail/drivers/result_access.h>
#include <hven/detail/model/nlp_model.h>
#include <hven/detail/model/problem_access.h>
#include <hven/detail/warmstart/warm_start_access.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "../drivers/support/hs071_problem.h"

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
using hven::SqpOptions;
using hven::SqpResult;
using hven::SqpSolver;
using hven::StopReason;
using hven::StopSource;
using hven::Verbosity;
using hven::detail::NlpModel;
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

void expect_same_row(const hven::SqpIterate &a, const hven::SqpIterate &b) {
    EXPECT_EQ(a.trial, b.trial);
    EXPECT_TRUE(same_scalar(a.f, b.f));
    EXPECT_TRUE(same_scalar(a.stationarity, b.stationarity));
    EXPECT_TRUE(same_scalar(a.feasibility, b.feasibility));
    EXPECT_TRUE(same_scalar(a.complementarity, b.complementarity));
    EXPECT_TRUE(same_scalar(a.kkt_residual, b.kkt_residual));
    EXPECT_TRUE(same_scalar(a.violation_l1, b.violation_l1));
    EXPECT_TRUE(same_scalar(a.tr_radius, b.tr_radius));
    EXPECT_TRUE(same_scalar(a.mu, b.mu));
    EXPECT_TRUE(same_scalar(a.step_norm, b.step_norm));
    EXPECT_EQ(a.qp_solved, b.qp_solved);
    EXPECT_EQ(a.qp_status, b.qp_status);
    EXPECT_EQ(a.qp_minor_iters, b.qp_minor_iters);
    EXPECT_EQ(a.qp_factorizations, b.qp_factorizations);
    EXPECT_EQ(a.verdict, b.verdict);
    EXPECT_EQ(a.elastic_applied, b.elastic_applied);
    EXPECT_EQ(a.soc_applied, b.soc_applied);
    EXPECT_EQ(a.active_rows, b.active_rows);
    EXPECT_EQ(a.active_lower_sides, b.active_lower_sides);
    EXPECT_EQ(a.active_upper_sides, b.active_upper_sides);
    EXPECT_TRUE(same_scalar(a.ipqp_least_infeasible_primal, b.ipqp_least_infeasible_primal));
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
};

SqpRun run_hs071(const SqpOptions &opts, StopSource &source, std::optional<Index> max_iterations) {
    SqpRun run;
    SolveBudget budget;
    budget.stop = source.token();
    budget.max_iterations = max_iterations;
    Problem problem(std::make_shared<hven_drivers_tests::Hs071Problem>());
    SqpSolver solver(opts);
    solver.set_iteration_callback([&](const IterationEvent &) {
        ++run.events;
        return CallbackAction::kContinue;
    });
    run.result = solver.solve(problem, hs071_start(), {}, budget);
    return run;
}

// The stopped solve against the solve capped at the same major: the same point, the
// same rows. Only the abandoned subproblem's work differs.
void expect_stopped_at_major(const SqpResult &stopped, const SqpResult &capped) {
    EXPECT_EQ(stopped.status, SolveStatus::kInterrupted) << stopped.message;
    EXPECT_EQ(stopped.stop_reason, StopReason::kStopRequested);
    EXPECT_EQ(capped.status, SolveStatus::kIterationLimit) << capped.message;
    ASSERT_FALSE(stopped.history.empty());
    EXPECT_FALSE(stopped.history.back().qp_solved) << "the stopped-at row";
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

TEST(StopPollSeam, T4ATokenStopInsideTheMainSubproblemReturnsXkOnEachQpMode) {
    const SeamReset reset;
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions opts = quiet_sqp(mode);
        const int site = static_cast<int>(site_of(mode));

        Seam::reset();
        StopSource dry_source;
        const SqpRun dry = run_hs071(opts, dry_source, std::nullopt);
        ASSERT_EQ(dry.result.status, SolveStatus::kOptimal) << dry.result.message;
        ASSERT_EQ(dry.result.counters.restoration_iters, 0) << "premise: no restoration";
        const int solves = Seam::solves[site];
        ASSERT_GE(solves, 2) << "premise: this mode's kernel solves subproblems here";

        // A minor past the first of a subproblem in the second half (the first poll of
        // the last subproblem when none has two).
        // The fire point: the middle poll of the subproblem with the most polls, which the
        // unstopped solve follows with at least one more (so a kernel that ignored the stop
        // would poll again). Recorded per solve from a run that fires nothing.
        std::vector<int> polls_per_solve;
        Seam::reset();
        StopSource rec_source;
        Seam::record_polls_per_solve = &polls_per_solve;
        Seam::record_site = site;
        (void)run_hs071(opts, rec_source, std::nullopt);
        Seam::record_polls_per_solve = nullptr;
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
        const int fire_index = most / 2;

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

        const Index k = stopped.result.counters.major_iters;
        ASSERT_GE(k, 1);
        StopSource cap_source;
        Seam::reset();
        const SqpRun capped = run_hs071(opts, cap_source, k);
        expect_stopped_at_major(stopped.result, capped.result);
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
    std::vector<Index> depth;
    std::vector<Vec> x_at_event;
};

ModelRun run_model(const NlpModel &model, StopSource &source, std::optional<Index> max_iterations) {
    ModelRun run;
    SolveBudget budget;
    budget.stop = source.token();
    budget.max_iterations = max_iterations;
    SqpOptions opts;
    opts.common.verbosity = Verbosity::kSilent;
    SqpSolver solver(opts);
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            ++run.events;
            run.walk_solves_at_event.push_back(Seam::solves[static_cast<int>(StopPollSite::kWalk)]);
            run.depth.push_back(e.depth.value_or(0));
            run.x_at_event.push_back(e.x);
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    run.result = solver.solve(hven::detail::problem_ref(model), model.start_point(),
                              hven::WarmStart{}, budget);
    return run;
}

TEST(StopPollSeam, T4ATokenStopInsideAnElasticResolveReturnsXk) {
    const SeamReset reset;
    InfeasibleCircleLineModel model;
    StopSource dry_source;
    const ModelRun dry = run_model(model, dry_source, std::nullopt);
    // The first major whose subproblem the elastic tier answered, before restoration.
    std::size_t k = 0;
    while (k < dry.result.history.size() && !dry.result.history[k].elastic_applied) {
        ++k;
    }
    ASSERT_GE(k, 1u) << "premise: an earlier major's event counts its walk solves";
    ASSERT_LT(k, dry.result.history.size()) << "premise: the elastic tier answers a subproblem";
    ASSERT_EQ(dry.depth[k], 0);
    // Major k's walk solves start after event k-1's count: the main subproblem, then the
    // elastic ladder's first rung.
    const int fire_solve = dry.walk_solves_at_event[k - 1] + 2;

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
    EXPECT_EQ(r.counters.restoration_iters, 0) << "the elastic answer never reached restoration";
    EXPECT_GE(r.counters.elastic_activations, 1);
    EXPECT_FALSE(r.history.back().elastic_applied);
    EXPECT_EQ(stopped.events, static_cast<Index>(r.history.size()));

    Seam::reset();
    StopSource cap_source;
    const ModelRun capped = run_model(model, cap_source, static_cast<Index>(k));
    expect_stopped_at_major(r, capped.result);
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
