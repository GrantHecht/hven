// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_sqp_ladder_seams.cpp — the M7 W2 ladder round's driver fixtures through the
// HVEN_TESTING ladder seam (hven/detail/drivers/ladder_testing.h; docs/testing.md,
// the sixth seam). Compiled only into hven_sqp_seam_tests.
//
//   L1  DALLASM's shape: restoration succeeds, the next QP at the feasible point
//       fails, the radius reaches the floor and the once-per-solve cap refuses the
//       second request -- spelled kNumericalError, never kInfeasible, at h <= feas_tol.
//   L3  READING6's shape: a QP ended by an E2 termination at h > feas_tol opens the
//       elastic tier; at a feasible iterate the shrink ladder stays (the DALLASM
//       major-1 guard); a plain kNumericalError keeps the shrink ladder; the guard's
//       h reads the box (L2).
//   SDL-4 a certificate bound-half refusal is routed as an E2 termination, and to the elastic
//       tier at any h when its iterate is past the shrink ladder's box tolerance.
//   CERT-1 (M7 W2 C12) a restoration that converges where every row is inside
//       feas_tol but their l1 sum is not certifies nothing (BRITGAS under W1); since
//       SNR it resumes, the resume test reading the convergence test's inf-norm.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/drivers/ladder_testing.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "support/qp_as_nlp_model.h"

namespace hven::solvers {
namespace {

using test_support::QpAsNlpModel;
using Seam = detail::testing::SqpLadderSeam;

struct SeamReset {
    SeamReset() { Seam::reset(); }
    ~SeamReset() { Seam::reset(); }
};

// test_sqp_restoration.cpp's RunawayValleyModel (SecondRequestIsCappedAndReported):
// restoration is entered once and RESUMES at a feasible point.
class RunawayValleyModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return -10.0 * x(1) - 0.01 * x(0) + 0.0005 * (x(0) * x(0) + x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.001 * x(0) - 0.01, 0.001 * x(1) - 10.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c << x(1) + 1000.0 * x(0) * x(0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale * 0.001 + lambda_e(0) * 2000.0;
        h.insert(1, 1) = obj_scale * 0.001;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2000.0 * x(0);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -1e20);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 1e20);
        return u;
    }
    Vec start_point() const override {
        Vec x(2);
        x << 0.0, 0.5;
        return x;
    }
};

// min 1/2 |x|^2 - x0  s.t.  x0 + x1 = 2,  -10 <= x <= 10.  Optimum (1.5, 0.5).
QpAsNlpModel line_model(const Vec &x0) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 1.0;
    qp.H.insert(1, 1) = 1.0;
    qp.g = Vec(2);
    qp.g << -1.0, 0.0;
    Eigen::MatrixXd A(1, 2);
    A << 1.0, 1.0;
    qp.Ae = A.sparseView();
    qp.be = Vec::Constant(1, 2.0);
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return QpAsNlpModel(std::move(qp), x0);
}

Vec vec2(double a, double b) {
    Vec v(2);
    v << a, b;
    return v;
}

SqpOptions quiet() {
    SqpOptions o;
    o.common.print_level = 10;
    // THE SEAM'S FORCED FAILURE KEEPS THE OPTIMAL STEP in `qs.x` -- a failure no
    // real walk hands back -- so the M7 W2 R2 failed-subproblem step would simply
    // take it. These fixtures pin the ladder's ROUTING of a failure: it is off here.
    o.qp_failure_step = false;
    return o;
}

void expect_line_optimum(const SqpResult &r) {
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x(0), 1.5, 1e-6);
    EXPECT_NEAR(r.x(1), 0.5, 1e-6);
}

} // namespace

// L1: the DALLASM shape (E2-D3 Q3). Restoration resumes at a feasible point; every QP
// after it fails; the shrink ladder reaches the radius floor; the cap refuses the
// second request. Before the ladder round that refusal was spelled kInfeasible.
TEST(SqpLadderSeam, L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::only_after_resume = true;
    RunawayValleyModel model;
    SqpOptions o = quiet();
    o.tr_init = 8.0; // SecondRequestIsCappedAndReported's options
    o.max_iter = 200;
    o.qp_failure_budget = 1000; // the shrink ladder runs to the floor
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    // The premise: one restoration, serviced, resumed inside feas_tol; then failures.
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    ASSERT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    ASSERT_LE(sol.counters.restoration_h_out, o.feas_tol);
    ASSERT_LE(sol.counters.restoration_feas_out, o.feas_tol) << "the resume decision's reading";
    ASSERT_GE(Seam::forced, 1);
    // The refused second request: an honest failure, no claim about the model.
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_FALSE(sol.infeasibility_certified);
    // ...at a point inside feas_tol, which is what made kInfeasible false.
    EXPECT_LE(std::abs(model.eval_ce(sol.x)(0)), o.feas_tol);
    // The F4 guard held: at the feasible iterate the failures took the shrink ladder.
    EXPECT_GE(sol.counters.qp_failures_routed, 1);
}

// L3: the READING6 shape (E2-D1 Q1). QP 0 at h = 2 ends kNumericalError through an
// E2 termination: the elastic tier opens, and the solve reaches the optimum.
TEST(SqpLadderSeam, L3AnE2TerminationAtAnInfeasibleIterateOpensTheElasticTier) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    QpAsNlpModel model = line_model(vec2(0.0, 0.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().elastic_applied);
    expect_line_optimum(sol);
}

// L3's control: the same failure WITHOUT an E2 counter keeps the shrink ladder. The
// radius starts at 10 so the halved QP is still feasible and nothing else goes elastic.
TEST(SqpLadderSeam, L3APlainNumericalErrorKeepsTheShrinkLadder) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    QpAsNlpModel model = line_model(vec2(0.0, 0.0));
    SqpOptions o = quiet();
    o.tr_init = 10.0;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_FALSE(sol.history.front().elastic_applied);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// L3's guard: the DALLASM major-1 case. At a FEASIBLE iterate (h = 0) the elastic
// QP would carry no slack and be the failed QP again, so the shrink ladder stays.
TEST(SqpLadderSeam, L3AtAFeasibleIterateTheShrinkLadderStays) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// L3 x L2: the guard's h reads the box. Rows satisfied (x0 + x1 = 2) but both
// components outside [-10, 10] -- projection off, so the solve stands there. A
// rows-only h is 0 and would shrink; with the box it is 6 and the tier opens.
TEST(SqpLadderSeam, L3TheGuardReadsTheBox) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    QpAsNlpModel model = line_model(vec2(-12.0, 14.0));
    SqpOptions o = quiet();
    o.project_start_point = false;
    o.tr_init = 10.0; // the QP can reach the box: it is kOptimal before the seam
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    expect_line_optimum(sol);
}

// SDL-4: a certificate bound-half refusal (kNumericalError, formerly kInfeasible) whose iterate
// stands off its box past feas_tol is no step the shrink ladder can retry; it opens the elastic
// tier at every h -- here also at a FEASIBLE iterate, where an E2 termination would shrink.
// Without the route the solve ends kNumericalError at the first major.
TEST(SqpLadderSeam, Sdl4ABoundRefusalOffItsBoxTakesTheElasticTierAtEveryH) {
    for (const Vec &x0 : {vec2(0.0, 2.0), vec2(0.0, 0.0)}) {
        const SeamReset guard;
        SCOPED_TRACE(::testing::Message() << "x0 = (" << x0(0) << ", " << x0(1) << ")");
        Seam::fail_walk_qps = 1;
        Seam::as_e2_termination = false;
        Seam::as_bound_refusal = true;
        Seam::forced_point = {12.0, -10.0}; // rows met, x0 past its upper bound 10
        QpAsNlpModel model = line_model(x0);
        SqpSolver driver(quiet());
        const SqpResult sol = driver.solve(model);

        EXPECT_EQ(Seam::forced, 1);
        EXPECT_GE(sol.counters.elastic_activations, 1);
        EXPECT_EQ(sol.counters.qp_failures_routed, 0);
        ASSERT_FALSE(sol.history.empty());
        EXPECT_TRUE(sol.history.front().elastic_applied);
        expect_line_optimum(sol);
    }
}

// SDL-4's E2 guard: a bound refusal whose iterate the shrink ladder can retry (here the solved
// step, in its box) is routed as an E2 termination -- at a feasible iterate the ladder stays.
TEST(SqpLadderSeam, Sdl4ARetryableBoundRefusalAtAFeasibleIterateKeepsTheShrinkLadder) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_bound_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// SDL-4's `linearization_violated ||` disjunct: a bound refusal whose iterate the shrink ladder
// can retry, at h = 2 > feas_tol, opens the elastic tier (a smaller box cannot cure a violated
// linearization). Without the disjunct only the retryability test routes, and this one shrinks.
TEST(SqpLadderSeam, Sdl4ARetryableBoundRefusalAtAnInfeasibleIterateTakesTheElasticTier) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_bound_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 0.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().elastic_applied);
    expect_line_optimum(sol);
}

// SDL-6: a certificate ROW-HALF refusal (kNumericalError, formerly kInfeasible without a verified
// certificate) keeps its former route at an infeasible iterate: the elastic tier opens.
// Without the route the plain kNumericalError takes the shrink ladder (L3's control).
TEST(SqpLadderSeam, Sdl6ARowRefusalAtAnInfeasibleIterateOpensTheElasticTier) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_row_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 0.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().elastic_applied);
    expect_line_optimum(sol);
}

// SDL-4 x R2: a retryable bound refusal at a feasible iterate is a step known past the
// trust-region window; it is not offered as the failed subproblem's step (qp_failure_step ON),
// so the shrink ladder routes it and nothing is judged.
TEST(SqpLadderSeam, Sdl4ABoundRefusalIsNotOfferedAsAFailureStep) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_bound_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpOptions o = quiet();
    o.qp_failure_step = true;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// SDL-6's guard: at a FEASIBLE iterate the QP is feasible (d = 0), so a row refusal there is the
// walk's, and the shrink ladder stays.
TEST(SqpLadderSeam, Sdl6ARowRefusalAtAFeasibleIterateKeepsTheShrinkLadder) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_row_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// SDL-6 x R2: a row refusal at a feasible iterate is not a point of the subproblem's feasible
// set; it is not offered as the failed subproblem's step (qp_failure_step ON), so the shrink
// ladder routes it and nothing is judged.
TEST(SqpLadderSeam, Sdl6ARowRefusalIsNotOfferedAsAFailureStep) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_row_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpOptions o = quiet();
    o.qp_failure_step = true;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// ---------------------------------------------------------------------------
// M7 W2 R2 fix 1 -- the failed subproblem's own step through the seam
// ---------------------------------------------------------------------------

namespace {

// min x0 + 1/2 x1^2  s.t.  -x0 <= 0.  Optimum (0, 0), f* = 0, lambda* = 1. On every
// point with x1 = 0 the price lambda = 1 makes the gradient of the Lagrangian vanish,
// so stationarity alone cannot tell (0.3, 0) from the optimum; complementarity can.
class CarriedPriceModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return x(0) + 0.5 * x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override { return vec2(1.0, x(1)); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, -x(0)); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 2); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = -1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override { return vec2(0.0, 2.0); }

  private:
    Vec lo_ = Vec::Constant(2, -10.0);
    Vec up_ = Vec::Constant(2, 10.0);
};

// min -x + 10 x^3 on [0, 10] from x = 0: the model at 0 is linear (f'' = 0), so a
// step p > 0 predicts a decrease of p, while f rises for p > 1/sqrt(10).
class CubicRiseModel : public NlpModel {
  public:
    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return -x(0) + 10.0 * x(0) * x(0) * x(0); }
    Vec eval_grad(const Vec &x) const override {
        return Vec::Constant(1, -1.0 + 30.0 * x(0) * x(0));
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
        h.insert(0, 0) = obj_scale * 60.0 * x(0);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 1); }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 1); }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override { return Vec::Zero(1); }

  private:
    Vec lo_ = Vec::Zero(1);
    Vec up_ = Vec::Constant(1, 10.0);
};

SqpOptions failure_step_options() {
    SqpOptions o;
    o.common.print_level = 10;
    o.enable_scaling = false; // the seam's forced point is in the engine's variables
    o.tr_init = 1.0;
    o.max_iter = 50;
    EXPECT_TRUE(o.qp_failure_step) << "the shipped default these fixtures exercise";
    return o;
}

} // namespace

// THE CARRIED PRICE IS NOT A CERTIFICATE (R2 review, BLOCKING-1). Major 0 prices
// lambda = 1 at (0, 2) and steps to (0, 1). Major 1's walk fails standing on (0.3, 0):
// pred = 0.2 > 0, f 0.5 -> 0.3, h stays 0, so the strategy takes it, keeping lambda = 1
// -- priced at a point where the row was active, carried to one where it is not. There
// stationarity and feasibility are exactly 0 and complementarity is 0.3. Before fix 1
// the test skipped that conjunct and certified kOptimal at f = 0.3; it is now armed for
// carried multipliers, the next major re-prices them, and the solve reaches f* = 0.
TEST(SqpLadderSeam, R2ACarriedPriceIsNotACertificate) {
    const SeamReset guard;
    Seam::pass_walk_qps = 1;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.3, 0.0};
    CarriedPriceModel model;
    const SqpOptions o = failure_step_options();
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    // The premise: one forced failure, at major 1, whose own step was judged and taken.
    ASSERT_EQ(Seam::forced, 1);
    ASSERT_GE(sol.history.size(), 3u);
    EXPECT_EQ(sol.history[1].qp_status, QpStatus::kNumericalError);
    EXPECT_EQ(sol.history[1].verdict, StepVerdict::kAcceptF);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_accepted, 1);
    EXPECT_DOUBLE_EQ(sol.history[2].f, 0.3) << "the failed walk's point was taken";
    // The carried price is not a certificate: major 2 is not the last.
    EXPECT_GT(sol.history.size(), 3u) << "converged at (0.3, 0) on a carried multiplier";

    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_NEAR(sol.f, 0.0, 1e-9);
    EXPECT_NEAR(sol.x(0), 0.0, 1e-9);
    EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
    ASSERT_EQ(sol.lambda_i.size(), 1);
    EXPECT_LE(std::abs(sol.lambda_i(0) * model.eval_ci(sol.x)(0)), o.kkt_tol);
}

// A JUDGED, REJECTED FAILURE STEP IS NOT CHARGED TO THE BUDGET (R2 review, RIDER-5).
// Three forced failures keep the walk's solved step p = Delta at x = 0: at Delta = 1
// and 0.5 the trial raises f (rejected, routed, uncharged); at 0.25 it lowers f and is
// taken. With the step off the same three failures are a charged run of three.
TEST(SqpLadderSeam, R2AJudgedRejectedStepIsNotChargedToTheBudget) {
    for (const bool on : {true, false}) {
        SCOPED_TRACE(on ? "qp_failure_step on" : "qp_failure_step off");
        const SeamReset guard;
        Seam::fail_walk_qps = 3;
        Seam::as_e2_termination = false;
        CubicRiseModel model;
        SqpOptions o = failure_step_options();
        o.qp_failure_step = on;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);

        ASSERT_EQ(Seam::forced, 3);
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 1.0 / std::sqrt(30.0), 1e-6);
        if (on) {
            EXPECT_EQ(sol.counters.qp_failure_steps_judged, 3);
            EXPECT_EQ(sol.counters.qp_failure_steps_accepted, 1);
            EXPECT_EQ(sol.counters.qp_failures_routed, 2) << "the two rejections are routed";
            EXPECT_EQ(sol.counters.qp_failures_in_a_row_peak, 0) << "and never charged";
        } else {
            EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
            EXPECT_EQ(sol.counters.qp_failures_routed, 3);
            EXPECT_EQ(sol.counters.qp_failures_in_a_row_peak, 3);
        }
    }
}

namespace {

// CERT-1 (M7 W2 C12): K rows that each miss feasibility by miss_ < feas_tol (3e-7 default),
//
//     min x1^2  s.t.  cE_k(x) = x0^2 + miss_ = 0 (k < K),  -3 <= x <= 3,
//
// so the l1 violation is at least K*miss_ = 3e-6 > feas_tol everywhere while the
// inf-norm reading miss_ passes feas_tol at x0 = 0 -- and there the rows'
// gradients vanish, BRITGAS's shape under the W1 guard (flows at their bounds,
// the pipe rows' Jacobian zero, l1 4.49e-6, inf 4.23e-7, certified).
class SpreadMissModel : public NlpModel {
  public:
    static constexpr Index kRows = 10;
    explicit SpreadMissModel(double miss = 3e-7) : miss_(miss) {}
    const double miss_;
    Index n() const override { return 2; }
    Index me() const override { return kRows; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.0, 2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(kRows, x(0) * x(0) + miss_); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e.sum();
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(kRows, 2);
        for (Index k = 0; k < kRows; ++k) {
            j.insert(k, 0) = 2.0 * x(0);
        }
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -3.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 3.0);
        return u;
    }
    Vec start_point() const override { return vec2(1.0, 1.0); }
};

} // namespace

// Every walk QP of the top-level driver fails (a plain kNumericalError), and the elastic ladder
// answers for it: its steps carry the parent toward x0 = 0 until a ladder comes back unusable and
// the elastic requester (row verdict kRestore) sends the solve to restoration, whose own walk is
// untouched and whose sub-solve converges at once at the point it was handed. SNR: the resume
// test reads the convergence test's inf-norm, so that point RESUMES; the resumed solve's QPs
// still fail and the once-per-solve cap refuses its next request.
TEST(SqpLadderSeam, CertOneAPointTheConvergenceTestCallsFeasibleIsNoCertificate) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    SpreadMissModel model;
    SqpOptions o = quiet();
    o.max_iter = 200;
    o.qp_failure_budget = 1000; // the shrink ladder runs to the floor
    SqpSolver driver(o);
    // The nesting depth of every row event, in order: the sub-solve's rows arrive at depth 1.
    std::vector<Index> depths;
    driver.set_iteration_callback([&depths](const IterationEvent &e) {
        depths.push_back(e.depth.value_or(0));
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model);

    // THE PREMISE: one restoration, its sub-solve converged at a point whose rows are
    // each inside feas_tol (inf) while their l1 sum is not.
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    ASSERT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    ASSERT_GT(sol.counters.restoration_h_out, o.feas_tol);
    ASSERT_LE(sol.counters.restoration_feas_out, o.feas_tol);
    const Vec ce = model.eval_ce(sol.x);
    ASSERT_LE(ce.lpNorm<Eigen::Infinity>(), o.feas_tol);
    ASSERT_GT(ce.lpNorm<1>(), o.feas_tol);
    std::printf("[ snr ] cert-one: status=%d majors=%ld resto_iters=%ld h_out=%.6g "
                "feas_out=%.6g\n",
                static_cast<int>(sol.status), static_cast<long>(sol.counters.major_iters),
                static_cast<long>(sol.counters.restoration_iters), sol.counters.restoration_h_out,
                sol.counters.restoration_feas_out);

    // THE ENTRY RESUMED (SNR), located by the event stream rather than by a route's verdict:
    // every request site pushes its own row AFTER the sub-solve, so the first depth-0 event
    // after the depth-1 run is the requester's, and a second one exists only if the parent
    // resumed. The resumed rows stand on the restored point (every later QP fails).
    ASSERT_FALSE(sol.history.empty());
    EXPECT_GT(sol.history.front().violation_l1, 1.0) << "the requesting phase at x0 = (1, 1)";
    const auto first_nested = std::find(depths.begin(), depths.end(), Index{1});
    ASSERT_NE(first_nested, depths.end()) << "the sub-solve emitted no row";
    const auto requester = std::find(first_nested, depths.end(), Index{0});
    ASSERT_NE(requester, depths.end()) << "the requester's row";
    const auto rows_before = std::count(depths.begin(), first_nested, Index{0});
    EXPECT_EQ(std::count(requester, depths.end(), Index{1}), 0) << "one sub-solve: the cap refused";
    const auto rows_after = std::count(requester, depths.end(), Index{0});
    ASSERT_GE(rows_after, 2) << "the entry did not resume";
    ASSERT_EQ(static_cast<long>(sol.history.size()), rows_before + rows_after);
    const SqpIterate &resumed = sol.history[static_cast<std::size_t>(rows_before + 1)];
    EXPECT_DOUBLE_EQ(resumed.violation_l1, sol.counters.restoration_h_out);

    // THE CLAIM IS WITHHELD: no certificate and no kInfeasible at a point this
    // solve's own convergence test calls feasible.
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(sol.counters.restoration_reduction_resumes, 0) << "the option is off";
}

// M7 W2 P1E -- THE SUFFICIENT-REDUCTION RESUME. The model above, entered at x0 = (1, 1)
// (h_in = 10): the first walk QP is forced to fail, and tr_min sits just under tr_init so the
// first shrink hits the floor. Each row misses by kNoClaimMiss > feas_tol, so the sub-solve's
// point fails the resume test; it ends kNumericalError, certifying nothing (SNR re-premise:
// CERT-1's inf <= feas_tol point, the fixture's point until SNR, now resumes plainly). WHAT ENDS
// THE SUB-SOLVE: its rows bounce around x0 = 0, where their gradient vanishes, until a rejected
// step's shrink crosses the inherited tr_min and its radius-floor request, nested, is refused.
// Off: that exit makes no claim and ends the solve there. At k = 0.5 it RESUMES, above feas_tol,
// and is counted. ARM UNCOVERED: the reduction resume's other arm, a sub-solve kOptimal without a
// claim, is not reached by this model at any miss in (feas_tol, 2 feas_tol] (the same exit ends
// it), so no fixture here exercises it.
namespace {
constexpr double kNoClaimMiss = 1.5e-6;
SqpResult reduction_resume_solve(double k) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.max_iter = 200;
    o.tr_init = 1.0;
    o.tr_min = 0.75;
    o.restoration_resume_reduction = k;
    SqpSolver driver(o);
    return driver.solve(model);
}
} // namespace

TEST(SqpLadderSeam, TheReductionResumeTakesTheNoClaimPointAndCountsIt) {
    const double feas_tol = SqpOptions{}.feas_tol;
    const SqpResult off = reduction_resume_solve(0.0);
    const SqpResult on = reduction_resume_solve(0.5);
    for (const SqpResult *r : {&off, &on}) {
        // THE PREMISE, both arms: one measured entry at h_in = 10, its sub-solve ended at a
        // point inside half of it but outside feas_tol (the resume test's inf-norm).
        ASSERT_EQ(r->counters.restoration_entries, 1);
        ASSERT_EQ(r->counters.restoration_sub_status, RestorationSubStatus::kNumericalError);
        ASSERT_NEAR(r->counters.restoration_h_in, 10.0, 1e-4);
        ASSERT_GT(r->counters.restoration_h_out, feas_tol);
        ASSERT_LE(r->counters.restoration_h_out, 0.5 * r->counters.restoration_h_in);
        ASSERT_GT(r->counters.restoration_feas_out, feas_tol);
        EXPECT_FALSE(r->infeasibility_certified);
    }
    for (const SqpResult *r : {&off, &on}) {
        std::printf("[ p1e ] reduction resume k=%s: h_in=%.6g h_out=%.6g status=%d majors=%ld "
                    "resumes=%ld\n",
                    r == &off ? "0" : "0.5", r->counters.restoration_h_in,
                    r->counters.restoration_h_out, static_cast<int>(r->status),
                    static_cast<long>(r->counters.major_iters),
                    static_cast<long>(r->counters.restoration_reduction_resumes));
    }
    EXPECT_EQ(off.counters.restoration_reduction_resumes, 0);
    EXPECT_EQ(off.status, SolveStatus::kNumericalError) << "the sub-solve's no-claim exit";
    EXPECT_EQ(on.counters.restoration_reduction_resumes, 1);
    EXPECT_GT(on.counters.major_iters, off.counters.major_iters) << "the solve went on";
    EXPECT_NE(on.status, SolveStatus::kInfeasible);
}

// QC-1 -- NO CERTIFICATE FROM A NEAR-FEASIBLE ENTRY WHOSE SLACKS SIT INSIDE feas_tol.
//
//     min (x0 - 3)^2 + (x1 - 2)^2  s.t.  cE(x) = 100 (x0 - x1) = 0,  -10 <= x <= 10,
//
// feasible, optimum (2.5, 2.5). Entered at x = (1.5 + kOff, 1.5): the row misses by
// 100 kOff = 2.5e-5 > feas_tol, but the wrapper's slack column is sigma = 100 wide, so the
// slack sits at 2.5e-7 -- INSIDE feas_tol of its zero bound. ACOPR57's shape under a QN arm
// (nonlinear-only, m = 50): the sub-solve read every slack as bound-active, converged at its
// major 0 with zero multipliers, and the parent certified kInfeasible on a feasible problem.
// The first kFails walk QPs are forced to fail and every failure halves the radius, so the
// last one crosses tr_min and restoration is entered at the start point; the restart radius
// (0.1) leaves the resumed solve room above the floor. Both curvature sources.
namespace {
QpAsNlpModel steep_line_model(const Vec &x0) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.g = Vec(2);
    qp.g << -6.0, -4.0;
    Eigen::MatrixXd A(1, 2);
    A << 100.0, -100.0;
    qp.Ae = A.sparseView();
    qp.be = Vec::Zero(1);
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return QpAsNlpModel(std::move(qp), x0);
}
} // namespace

TEST(SqpLadderSeam, QcOneASlackInsideFeasTolIsNotABoundAtAViolatedRow) {
    constexpr double kOff = 2.5e-7;
    constexpr double kSigma = 100.0; // max(1, ||grad cE||inf) at the entry
    constexpr int kFails = 7;        // 1 / 2^7 < tr_min = 0.01 <= 1 / 2^6
    for (const SqpHessianMode mode : {SqpHessianMode::kExact, SqpHessianMode::kLbfgsDamped}) {
        SCOPED_TRACE(mode == SqpHessianMode::kExact ? "exact" : "lbfgs");
        const SeamReset guard;
        Seam::fail_walk_qps = kFails;
        Seam::as_e2_termination = false;
        QpAsNlpModel model = steep_line_model(vec2(1.5 + kOff, 1.5));
        SqpOptions o = quiet();
        o.hessian_mode = mode;
        o.tr_init = 1.0;
        o.tr_min = 0.01;
        o.qp_failure_budget = 1000; // the shrink ladder runs to the floor
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);

        // THE PREMISE: one restoration, entered at the start point, whose row misses by more
        // than feas_tol while its sigma-scaled slack sits inside feas_tol of zero.
        ASSERT_EQ(Seam::forced, kFails);
        ASSERT_EQ(sol.counters.restoration_entries, 1);
        ASSERT_NEAR(sol.counters.restoration_h_in, kSigma * kOff, 1e-12);
        ASSERT_GT(sol.counters.restoration_h_in, o.feas_tol);
        ASSERT_LT(sol.counters.restoration_h_in / kSigma, o.feas_tol);

        // THE CLAIM IS NEVER MADE, and the phase did real work: a slack whose row is violated
        // beyond feas_tol is not at its bound, so the sub-solve cannot stop at its major 0.
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_NE(sol.status, SolveStatus::kInfeasible);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        EXPECT_LE(sol.counters.restoration_h_out, o.feas_tol) << "the phase restores and resumes";
        EXPECT_LE(sol.counters.restoration_feas_out, o.feas_tol) << "the resume decision's reading";
        // AND THE SOLVE GOES ON TO THE OPTIMUM of the feasible problem.
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 2.5, 1e-6);
        EXPECT_NEAR(sol.x(1), 2.5, 1e-6);
    }
}

// THE ENTRY RULE'S PROGRESS GUARD (M7 W2 RST; SqpOptions::restoration_max_entries).
// L1A's shape with the cap well above one: restoration resumes at a feasible point,
// every QP after it fails, and the loop accepts NO step before it asks again. The
// request comes from the point the phase has just restored, so it is refused exactly
// as the once-per-solve rule refused it -- one entry, kNumericalError, no claim.
TEST(SqpLadderSeam, ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap) {
    for (const Index cap : {Index{1}, Index{10}}) {
        SCOPED_TRACE(cap);
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::only_after_resume = true;
        RunawayValleyModel model;
        SqpOptions o = quiet();
        o.tr_init = 8.0;
        o.max_iter = 200;
        o.qp_failure_budget = 1000;
        o.restoration_max_entries = cap;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);
        ASSERT_GE(Seam::forced, 1);
        EXPECT_EQ(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_LE(std::abs(model.eval_ce(sol.x)(0)), o.feas_tol);
    }
}

// THE ENTRY RULE'S CAP (M7 W2 RST). After the first resume FIVE walk subproblems pass
// (the loop accepts their steps) and every later one fails, so the loop asks again from a
// point it has moved to: a cap of one refuses that request, a cap above one services
// it, and the request after the second resume -- with no accepted step since -- is
// refused by the progress guard at any cap.
TEST(SqpLadderSeam, ARequestAfterAnAcceptedStepIsServicedOnlyBelowTheCap) {
    for (const Index cap : {Index{1}, Index{2}, Index{10}}) {
        SCOPED_TRACE(cap);
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::pass_walk_qps = 5;
        Seam::only_after_resume = true;
        RunawayValleyModel model;
        SqpOptions o = quiet();
        o.tr_init = 8.0;
        o.max_iter = 200;
        o.qp_failure_budget = 1000;
        o.restoration_max_entries = cap;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(model);
        ASSERT_GE(Seam::forced, 1);
        EXPECT_EQ(sol.counters.restoration_entries, cap == 1 ? 1 : 2);
        EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
        EXPECT_FALSE(sol.infeasibility_certified);
    }
}

// THE PHASE'S CURVATURE SWITCH (M7 W2 RST; SqpOptions::restoration_curvature). min x0 + x1
// s.t. x0^2 + x1^2 = 1, x0 + x1 = 2 is infeasible (test_sqp_restoration.cpp's fixture 1):
// the phase certifies at (1/sqrt 2, 1/sqrt 2). The seam fails the phase's FIRST walk
// subproblem. Under kModelThenRegularized that sub-solve stops there and the phase is
// restarted on the regularized model, whose curvature never reaches the caller's Hessian;
// under kModel the failure is routed by the sub-solve's own shrink ladder; under either the
// same certificate comes back.
namespace {
class CountingCircleLineModel final : public NlpModel {
  public:
    mutable Index n_hess = 0;
    mutable Index n_ce = 0;
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        ++n_ce;
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        ++n_hess;
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -1e20);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 1e20);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};
} // namespace

TEST(SqpLadderSeam, AFailedPhaseSubproblemRestartsThePhaseOnTheRegularizedModel) {
    struct Arm {
        SqpRestorationCurvature curvature;
        Index switches;
    };
    for (const Arm arm : {Arm{SqpRestorationCurvature::kModelThenRegularized, 1},
                          Arm{SqpRestorationCurvature::kModel, 0}}) {
        SCOPED_TRACE(static_cast<int>(arm.curvature));
        // The unforced solve first: the premise that no phase QP fails here on its own.
        Index hess_unforced = 0;
        {
            const SeamReset guard;
            CountingCircleLineModel model;
            SqpOptions o = quiet();
            o.max_iter = 200;
            o.restoration_curvature = arm.curvature;
            const SqpResult sol = SqpSolver(o).solve(model);
            ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
            ASSERT_EQ(sol.counters.restoration_curvature_switches, 0) << "fixture premise";
            hess_unforced = model.n_hess;
        }
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = 1;
        CountingCircleLineModel model;
        SqpOptions o = quiet();
        o.max_iter = 200;
        o.restoration_curvature = arm.curvature;
        const SqpResult sol = SqpSolver(o).solve(model);
        ASSERT_EQ(Seam::forced_restoration, 1);
        ASSERT_EQ(Seam::forced, 0) << "the top-level knobs are off";
        EXPECT_EQ(sol.counters.restoration_curvature_switches, arm.switches);
        // The same certified answer either way.
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
        if (arm.switches > 0) {
            // The restarted phase's curvature is the regularized model's, so the caller's
            // Hessian sees FEWER calls than the unforced exact phase made of it.
            EXPECT_LT(model.n_hess, hess_unforced);
        }
        // EVERY ROW EVALUATION OF THE CALLER'S MODEL IS A COUNTED QUERY, the restart's
        // own start evaluation and the regularized wrapper's reads of h included.
        EXPECT_EQ(model.n_ce, sol.counters.evals_full + sol.counters.evals_values);
    }
}

// A CONSUMER STOP ON THE SWITCH ROW IS HONOURED AT ONCE (M7 W2 RST-I1). The phase's first
// walk subproblem is forced to fail, so the sub-solve's first row is its switch row; a
// callback that returns kStop there must end the solve kInterrupted with no regularized
// restart run after it -- no further depth-1 row, and the restart's own evaluation of
// the caller's model never made.
TEST(SqpLadderSeam, AStopOnTheSwitchRowEndsTheSolveWithoutTheRegularizedRestart) {
    const SeamReset guard;
    Seam::fail_restoration_walk_qps = 1;
    CountingCircleLineModel model;
    SqpOptions o = quiet();
    o.max_iter = 200;
    ASSERT_EQ(o.restoration_curvature, SqpRestorationCurvature::kModelThenRegularized);
    SqpSolver solver(o);
    int depth1_rows = 0;
    solver.set_iteration_callback([&](const IterationEvent &e) {
        if (e.depth.value_or(0) >= 1) {
            ++depth1_rows;
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult sol = solver.solve(model);
    ASSERT_EQ(Seam::forced_restoration, 1);
    EXPECT_EQ(sol.counters.restoration_curvature_switches, 1) << "the stop came on the switch row";
    EXPECT_EQ(depth1_rows, 1) << "no regularized phase ran after the stop";
    EXPECT_EQ(sol.status, SolveStatus::kInterrupted);
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_EQ(model.n_ce, sol.counters.evals_full + sol.counters.evals_values);
}

// UNDER kLbfgsDamped THE SHIPPED kModelThenRegularized IS kModel: the phase's QN model is
// convex by construction, so a failed phase subproblem is routed by the sub-solve itself
// and no exact-Hessian call is ever made.
TEST(SqpLadderSeam, UnderTheQuasiNewtonModeAFailedPhaseSubproblemDoesNotSwitch) {
    const SeamReset guard;
    Seam::fail_restoration_walk_qps = 1;
    CountingCircleLineModel model;
    SqpOptions o = quiet();
    o.hessian_mode = SqpHessianMode::kLbfgsDamped;
    o.max_iter = 300;
    ASSERT_EQ(o.restoration_curvature, SqpRestorationCurvature::kModelThenRegularized);
    const SqpResult sol = SqpSolver(o).solve(model);
    ASSERT_EQ(Seam::forced_restoration, 1);
    EXPECT_EQ(sol.counters.restoration_curvature_switches, 0);
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
    EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
}

// THE SWITCH'S SECOND TRIGGER (M7 W2 RST rider M1). The phase's first walk subproblem is
// forced kInfeasible and its elastic ladder exhausted, so the sub-solve raises the NESTED
// restoration request, which a phase never services. Under kModel that refusal ends the
// sub-solve numerical_error, and the solve with it, no claim made. Under
// kModelThenRegularized the refusal is the switch: the phase restarts from the unmoved
// iterate on the regularized model and certifies the same infeasible point.
TEST(SqpLadderSeam, AnExhaustedElasticTierInsideThePhaseTakesTheRegularizedRestart) {
    for (const auto curvature :
         {SqpRestorationCurvature::kModelThenRegularized, SqpRestorationCurvature::kModel}) {
        const bool switches = curvature == SqpRestorationCurvature::kModelThenRegularized;
        SCOPED_TRACE(switches ? "kModelThenRegularized" : "kModel");
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = 1;
        Seam::exhaust_restoration_elastic = 1;
        CountingCircleLineModel model;
        SqpOptions o = quiet();
        o.max_iter = 200;
        o.restoration_curvature = curvature;
        const SqpResult sol = SqpSolver(o).solve(model);
        ASSERT_EQ(Seam::forced_restoration, 1);
        ASSERT_EQ(Seam::exhaust_restoration_elastic, 0) << "the forced exit reached the knob";
        EXPECT_GE(sol.counters.elastic_activations, 1) << "the elastic tier ran first";
        EXPECT_EQ(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.counters.restoration_curvature_switches, switches ? 1 : 0);
        if (switches) {
            EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
            EXPECT_TRUE(sol.infeasibility_certified);
            const double t = 1.0 / std::sqrt(2.0);
            EXPECT_NEAR(sol.x(0), t, 1e-5);
            EXPECT_NEAR(sol.x(1), t, 1e-5);
        } else {
            EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
            EXPECT_FALSE(sol.infeasibility_certified);
        }
        EXPECT_EQ(model.n_ce, sol.counters.evals_full + sol.counters.evals_values);
    }
}

// THE EVALUATION IDENTITY WITH BOTH ROW KINDS (M7 W2 RST rider M5). min x0 + x1 s.t.
// x0^2 + x1^2 = 1, 2 - x0 - x1 <= 0 is infeasible with the same h-minimizer as the
// equality pair above. The regularized wrapper reads h at a point it has not evaluated
// through BOTH eval_ce and eval_ci, and that is ONE values query: every row block of the
// caller's model is read exactly once per counted query, on the forced switch and off it.
namespace {
class CountingCircleHalfPlaneModel final : public NlpModel {
  public:
    mutable Index n_ce = 0;
    mutable Index n_ci = 0;
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        ++n_ce;
        Vec c(1);
        c << x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        ++n_ci;
        Vec c(1);
        c << 2.0 - x(0) - x(1);
        return c;
    }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.makeCompressed();
        return j;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = -1.0;
        j.insert(0, 1) = -1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -1e20);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 1e20);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};
} // namespace

TEST(SqpLadderSeam, EveryRowBlockIsReadOncePerCountedQueryWithBothRowKinds) {
    for (const int forced : {0, 1}) {
        SCOPED_TRACE(forced);
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = forced;
        CountingCircleHalfPlaneModel model;
        SqpOptions o = quiet();
        o.max_iter = 200;
        const SqpResult sol = SqpSolver(o).solve(model);
        ASSERT_EQ(Seam::forced_restoration, forced);
        ASSERT_GE(sol.counters.restoration_entries, 1) << "fixture premise: the phase runs";
        EXPECT_EQ(sol.counters.restoration_curvature_switches, forced);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_EQ(model.n_ce, sol.counters.evals_full + sol.counters.evals_values);
        EXPECT_EQ(model.n_ci, sol.counters.evals_full + sol.counters.evals_values);
    }
}

} // namespace hven::solvers
