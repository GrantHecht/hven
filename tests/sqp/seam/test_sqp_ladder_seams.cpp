// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_sqp_ladder_seams.cpp — the M7 W2 ladder round's driver fixtures through the
// HVEN_TESTING ladder seam (hven/detail/sqp/ladder_testing.h; docs/testing.md,
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
//   WC  (M8 walk cap) an elastic rung returned kOptimal OFF its rows at s = 0 -- what the
//       uncapped footprint excuse produced -- is not `closed` (the rung-residual bound),
//       and a zero elastic step at h > feas_tol that a report DID call usable is
//       exhaustion, not an unjudged accept: both route to restoration.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/model/problem_access.h>
#include <hven/detail/solver/result_access.h>
#include <hven/detail/sqp/globalization/funnel.h>
#include <hven/detail/sqp/ladder_testing.h>
#include <hven/detail/sqp/sqp_solver_impl.h>
#include <hven/sqp/sqp_solver_types.h>

#include "qp/support/qp_as_nlp_model.h"

using namespace hven::detail;

namespace hven {
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
    o.common.verbosity = hven::Verbosity::kSilent;
    // THE SEAM'S FORCED FAILURE KEEPS THE OPTIMAL STEP in `qs.x` -- a failure no
    // real walk hands back -- so the M7 W2 R2 failed-subproblem step would simply
    // take it. These fixtures pin the ladder's ROUTING of a failure: it is off here.
    o.advanced.qp_failure_step = false;
    // These fixtures pin every row read against a counted query; the column rule's
    // start-point Jacobian probe (default kAuto since M7 W2 SCW) is a counted query
    // that reads no row value, and is pinned in test_column_scaling.cpp.
    o.column_scaling = SqpColumnScaling::kOff;
    return o;
}

void expect_line_optimum(const SqpResult &r) {
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_NEAR(r.x(0), 1.5, 1e-6);
    EXPECT_NEAR(r.x(1), 0.5, 1e-6);
}

} // namespace

// L1: the DALLASM shape (E2-D3 Q3). Restoration resumes at a feasible point; every QP
// after it fails; the shrink ladder reaches the radius floor; the second request, with no
// step accepted since the resume, is refused. Before the ladder round that refusal was
// spelled kInfeasible; since RST-B it is kStalled beside kRestorationNoProgress (it was
// kNumericalError).
TEST(SqpLadderSeam, L1ARefusedRequestAtAFeasiblePointIsNeverInfeasible) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::only_after_resume = true;
    RunawayValleyModel model;
    SqpOptions o = quiet();
    o.trust_region_initial = 8.0; // SecondRequestIsCappedAndReported's options
    o.common.max_iterations = 200;
    o.advanced.max_consecutive_qp_failures = 1000; // the shrink ladder runs to the floor
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    // The premise: one restoration, serviced, resumed inside feas_tol; then failures.
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    ASSERT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    ASSERT_LE(sol.counters.restoration_h_out, o.common.feasibility_tol);
    ASSERT_LE(sol.counters.restoration_feas_out, o.common.feasibility_tol)
        << "the resume decision's reading";
    ASSERT_GE(Seam::forced, 1);
    // The refused second request: no progress, no claim about the model.
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationNoProgress);
    EXPECT_FALSE(sol.infeasibility_certified);
    // ...at a point inside feas_tol, which is what made kInfeasible false.
    EXPECT_LE(std::abs(model.eval_ce(sol.x)(0)), o.common.feasibility_tol);
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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().sqp->elastic);
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
    o.trust_region_initial = 10.0;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_FALSE(sol.history.front().sqp->elastic);
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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

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
    o.trust_region_initial = 10.0; // the QP can reach the box: it is kOptimal before the seam
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

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
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

        EXPECT_EQ(Seam::forced, 1);
        EXPECT_GE(sol.counters.elastic_activations, 1);
        EXPECT_EQ(sol.counters.qp_failures_routed, 0);
        ASSERT_FALSE(sol.history.empty());
        EXPECT_TRUE(sol.history.front().sqp->elastic);
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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().sqp->elastic);
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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_TRUE(sol.history.front().sqp->elastic);
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
    o.advanced.qp_failure_step = true;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
    EXPECT_EQ(sol.counters.elastic_activations, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// SDL-6's guard: at a FEASIBLE iterate the QP is feasible (d = 0), so a row refusal there is the
// walk's, and the shrink ladder stays.
// RF-1 design 3.3 (declared move of the SDL-6 pin): a row refusal at a feasible iterate goes to
// the elastic tier, at any h, instead of the shrink ladder.
TEST(SqpLadderSeam, Rf1ARowRefusalAtAFeasibleIterateGoesElastic) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_row_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpSolver driver(quiet());
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    expect_line_optimum(sol);
}

// SDL-6 x R2: a row refusal at a feasible iterate is not a point of the subproblem's feasible
// set; it is not offered as the failed subproblem's step (qp_failure_step ON), and nothing is
// judged.
TEST(SqpLadderSeam, Sdl6ARowRefusalIsNotOfferedAsAFailureStep) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_row_refusal = true;
    QpAsNlpModel model = line_model(vec2(0.0, 2.0));
    SqpOptions o = quiet();
    o.advanced.qp_failure_step = true;
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
    // RF-1 design 3.3: routed to the elastic tier, not the shrink ladder.
    EXPECT_EQ(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
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
    o.common.verbosity = hven::Verbosity::kSilent;
    o.row_scaling = false; // the seam's forced point is in the engine's variables
    o.trust_region_initial = 1.0;
    o.common.max_iterations = 50;
    EXPECT_TRUE(o.advanced.qp_failure_step) << "the shipped default these fixtures exercise";
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
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    // The premise: one forced failure, at major 1, whose own step was judged and taken.
    ASSERT_EQ(Seam::forced, 1);
    ASSERT_GE(sol.history.size(), 3u);
    EXPECT_EQ(sol.history[1].sqp->qp_status.value_or(hven::QpStatus::kOptimal),
              QpStatus::kNumericalError);
    EXPECT_EQ(sol.history[1].sqp->verdict.value_or(hven::StepVerdict::kReject),
              StepVerdict::kAcceptObjectiveStep);
    EXPECT_EQ(sol.counters.qp_failure_steps_judged, 1);
    EXPECT_EQ(sol.counters.qp_failure_steps_accepted, 1);
    EXPECT_DOUBLE_EQ(sol.history[2].f.value(), 0.3) << "the failed walk's point was taken";
    // The carried price is not a certificate: major 2 is not the last.
    EXPECT_GT(sol.history.size(), 3u) << "converged at (0.3, 0) on a carried multiplier";

    EXPECT_EQ(sol.status, SolveStatus::kOptimal);
    EXPECT_NEAR(sol.f, 0.0, 1e-9);
    EXPECT_NEAR(sol.x(0), 0.0, 1e-9);
    EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
    ASSERT_EQ(hven::detail::engine_rows(sol).lambda_i.size(), 1);
    EXPECT_LE(std::abs(hven::detail::engine_rows(sol).lambda_i(0) * model.eval_ci(sol.x)(0)),
              o.common.optimality_tol);
}

// A JUDGED, REJECTED FAILURE STEP IS NOT CHARGED TO THE BUDGET (R2 review, RIDER-5).
// Three forced failures keep the walk's solved step p = Delta at x = 0. RF-1 A (declared pin
// move, design 3.1): the retry ladder takes R1 once (Delta 1 -> 0.5), then R3 (the other
// algebra) at the SAME radius, whose forced failure is the attempt before it again -- a replay,
// counted and not charged (interpretation 1). So every trial (Delta = 1, 0.5, 0.5) raises f and
// is rejected, routed and uncharged; the third failure has no rung left and the floor rule sends
// the iterate to restoration, which converges at once and hands back to a solve that finishes.
// With the step off the same three failures charge two (the replay is not charged).
TEST(SqpLadderSeam, R2AJudgedRejectedStepIsNotChargedToTheBudget) {
    for (const bool on : {true, false}) {
        SCOPED_TRACE(on ? "qp_failure_step on" : "qp_failure_step off");
        const SeamReset guard;
        Seam::fail_walk_qps = 3;
        Seam::as_e2_termination = false;
        CubicRiseModel model;
        SqpOptions o = failure_step_options();
        o.advanced.qp_failure_step = on;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

        ASSERT_EQ(Seam::forced, 3);
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 1.0 / std::sqrt(30.0), 1e-6);
        EXPECT_EQ(sol.counters.qp_retry_rung_shrink, 1);
        EXPECT_EQ(sol.counters.qp_retry_rung_algebra, 1);
        EXPECT_EQ(sol.counters.qp_retry_replays, 1);
        EXPECT_EQ(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.counters.qp_failures_routed, 3);
        if (on) {
            EXPECT_EQ(sol.counters.qp_failure_steps_judged, 3);
            EXPECT_EQ(sol.counters.qp_failure_steps_accepted, 0);
            EXPECT_EQ(sol.counters.qp_failures_in_a_row_peak, 0) << "rejections are never charged";
        } else {
            EXPECT_EQ(sol.counters.qp_failure_steps_judged, 0);
            EXPECT_EQ(sol.counters.qp_failures_in_a_row_peak, 2) << "the replay is not charged";
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
    o.common.max_iterations = 200;
    o.advanced.max_consecutive_qp_failures = 1000; // every failure is routed
    SqpSolver driver(o);
    // The nesting depth of every row event, in order: the sub-solve's rows arrive at depth 1.
    std::vector<Index> depths;
    driver.set_iteration_callback(
        [&depths](const IterationEvent &e) {
            depths.push_back((e.sqp ? e.sqp->depth : Index{0}));
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

    std::printf(
        "[ snr ] cert-one premise: status=%d reason=%d majors=%ld routed=%ld rungs=%ld/%ld/%ld/%ld "
        "elastic=%ld resto=%ld floor=%ld x=(%.9g, %.9g)\n",
        static_cast<int>(sol.status), static_cast<int>(sol.stop_reason),
        static_cast<long>(sol.counters.major_iters),
        static_cast<long>(sol.counters.qp_failures_routed),
        static_cast<long>(sol.counters.qp_retry_rung_shrink),
        static_cast<long>(sol.counters.qp_retry_rung_cold),
        static_cast<long>(sol.counters.qp_retry_rung_algebra),
        static_cast<long>(sol.counters.qp_retry_rung_regularization),
        static_cast<long>(sol.counters.elastic_activations),
        static_cast<long>(sol.counters.restoration_entries),
        static_cast<long>(sol.counters.trust_region_floor_exits), sol.x(0), sol.x(1));
    // RF-1 A+B2 (declared, CONFIG-SPLIT): the after-rungs elastic route now answers every routed
    // failure, and whether a ladder comes back unusable before x0 reaches 0 is decided by the last
    // bits. Release (the reference build): 58 majors, 29 routed (R1 once, R3 28 times), one
    // restoration at x0 = 2.3e-6 -- the premise below. Debug: 46 majors, 23 routed, the elastic
    // steps alone carry x0 to 1.1e-12 and the solve converges without a restoration. Both end
    // kOptimal with no claim; only the reference build reaches the premise, and the Debug route is
    // pinned as read.
#ifndef NDEBUG
    if (sol.counters.restoration_entries == 0) {
        EXPECT_EQ(sol.status, SolveStatus::kOptimal) << sol.message;
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_GE(sol.counters.elastic_activations, 1);
        EXPECT_LE(std::abs(sol.x(0)), 1e-9);
        return;
    }
#endif
    // THE PREMISE: one restoration, its sub-solve converged at a point whose rows are
    // each inside feas_tol (inf) while their l1 sum is not.
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    ASSERT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    ASSERT_GT(sol.counters.restoration_h_out, o.common.feasibility_tol);
    ASSERT_LE(sol.counters.restoration_feas_out, o.common.feasibility_tol);
    const Vec ce = model.eval_ce(sol.x);
    ASSERT_LE(ce.lpNorm<Eigen::Infinity>(), o.common.feasibility_tol);
    ASSERT_GT(ce.lpNorm<1>(), o.common.feasibility_tol);
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
    EXPECT_GT(sol.history.front().sqp->violation_l1, 1.0) << "the requesting phase at x0 = (1, 1)";
    const auto first_nested = std::find(depths.begin(), depths.end(), Index{1});
    ASSERT_NE(first_nested, depths.end()) << "the sub-solve emitted no row";
    const auto requester = std::find(first_nested, depths.end(), Index{0});
    ASSERT_NE(requester, depths.end()) << "the requester's row";
    const auto rows_before = std::count(depths.begin(), first_nested, Index{0});
    EXPECT_EQ(std::count(requester, depths.end(), Index{1}), 0) << "one sub-solve: the cap refused";
    const auto rows_after = std::count(requester, depths.end(), Index{0});
    ASSERT_GE(rows_after, 2) << "the entry did not resume";
    ASSERT_EQ(static_cast<long>(sol.history.size()), rows_before + rows_after);
    const IterationRecord &resumed = sol.history[static_cast<std::size_t>(rows_before + 1)];
    EXPECT_DOUBLE_EQ(resumed.sqp->violation_l1, sol.counters.restoration_h_out);

    // THE CLAIM IS WITHHELD: no certificate and no kInfeasible at a point this
    // solve's own convergence test calls feasible.
    EXPECT_FALSE(sol.infeasibility_certified);
    // RF-1 A (declared pin move, design 3.1; was kAcceptable at C, kNumericalError before): the
    // resumed solve's failed QPs no longer exhaust a shrink ladder into a refused second request.
    // With the rungs spent at a violated linearization they go to the elastic tier, whose steps
    // finish the solve at the restored point: kOptimal by the convergence test's own inf-norm
    // reading -- the same reading that resumed it. The claim is still withheld.
    EXPECT_EQ(sol.status, SolveStatus::kOptimal) << sol.message;
    EXPECT_EQ(sol.stop_reason, StopReason::kNone);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_EQ(sol.counters.restoration_reduction_resumes, 0)
        << "a feasible resume, not a reduction";
}

// M7 W2 P1E -- THE SUFFICIENT-REDUCTION RESUME. The model above, entered at x0 = (1, 1)
// (h_in = 10): the first walk QP is forced to fail, and trust_region_min sits just under
// trust_region_initial so the first shrink hits the floor. Each row misses by kNoClaimMiss >
// feas_tol, so the sub-solve's point fails the resume test; it ends kNumericalError, certifying
// nothing (SNR re-premise: CERT-1's inf <= feas_tol point, the fixture's point until SNR, now
// resumes plainly). WHAT ENDS THE SUB-SOLVE: its rows bounce around x0 = 0, where their gradient
// vanishes, until a rejected step's shrink crosses the inherited trust_region_min and its
// radius-floor request, nested, is refused. Off: that exit makes no claim and ends the solve there.
// At k = 0.5 it RESUMES, above feas_tol, and is counted. ARM UNCOVERED: the reduction resume's
// other arm, a sub-solve kOptimal without a claim, is not reached by this model at any miss in
// (feas_tol, 2 feas_tol] (the same exit ends it), so no fixture here exercises it.
namespace {
constexpr double kNoClaimMiss = 1.5e-6;
SqpResult reduction_resume_solve(double k) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    o.advanced.restoration_resume_reduction = k;
    SqpSolver driver(o);
    return driver.solve(hven::detail::problem_ref(model), (model).start_point());
}
} // namespace

TEST(SqpLadderSeam, TheReductionResumeTakesTheNoClaimPointAndCountsIt) {
    const double feas_tol = SqpOptions{}.common.feasibility_tol;
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
    // The sub-solve's own exit decides the parent's, and its words travel (M8 B5).
    EXPECT_EQ(off.message, "numerical error: the feasibility restoration sub-solve failed: a "
                           "restoration request inside it could not be serviced");
    EXPECT_EQ(on.counters.restoration_reduction_resumes, 1);
    EXPECT_GT(on.counters.major_iters, off.counters.major_iters) << "the solve went on";
    EXPECT_NE(on.status, SolveStatus::kInfeasible);
}

// RF-1 -- THE ACCEPTABLE LABEL IS THE PARENT'S ALONE. The reduction-resume fixture's off arm:
// the sub-solve ends kNumericalError (its nested request refused) and the parent ends
// restoration_failed at the entry point (h = 10). Under a tier wide enough to admit both points
// the PARENT is labelled kAcceptable and the sub-solve is not: its status is read by the
// parent's switch, and a label there would move the path.
TEST(SqpLadderSeam, Rf1ARestorationFailureIsLabelledAtTheTopLevelOnly) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    // The off arm's resume rule (RF-1 ruling B2): with the phase now handing back its best-h
    // point the default reduction resume (0.1) would fire, and the two arms must differ in the
    // tier only.
    o.advanced.restoration_resume_reduction = 0.0;
    o.termination.acceptable_stationarity_tol = 1e10;
    o.termination.acceptable_equality_tol = 1e10;
    o.termination.acceptable_inequality_tol = 1e10;
    o.termination.acceptable_complementarity_tol = 1e10;
    const SqpResult wide =
        SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    const SqpResult off = reduction_resume_solve(0.0);
    EXPECT_EQ(wide.counters.restoration_sub_status, RestorationSubStatus::kNumericalError)
        << "the sub-solve keeps its own status";
    EXPECT_EQ(wide.status, SolveStatus::kAcceptable);
    EXPECT_EQ(wide.stop_reason, StopReason::kNone);
    EXPECT_NE(wide.message.find("exit restoration_failed"), std::string::npos) << wide.message;
    EXPECT_EQ(wide.counters.major_iters, off.counters.major_iters);
    EXPECT_EQ(wide.counters.restoration_iters, off.counters.restoration_iters);
    EXPECT_EQ(wide.counters.factorizations, off.counters.factorizations);
    EXPECT_EQ(wide.f, off.f);
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
// The first walk QP is forced to fail and trust_region_min sits above half the initial radius, so
// the retry ladder's R1 crosses the floor and restoration is entered at the start point; the
// restart radius (max(trust_region_min, 0.1 x)) is the floor itself, and the resumed solve's QPs
// all succeed. Both curvature sources. RF-1 A (declared re-premise, design 3.1): the old premise
// -- seven forced failures, each halving the radius down to a floor of 0.01 -- no longer reaches
// restoration at the start point, since the ladder has four rungs and then goes elastic.
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
    constexpr int kFails = 1;        // R1's shrink 1 -> 0.5 crosses trust_region_min = 0.75
    for (const SqpHessianMode mode : {SqpHessianMode::kExact, SqpHessianMode::kLbfgsDamped}) {
        SCOPED_TRACE(mode == SqpHessianMode::kExact ? "exact" : "lbfgs");
        const SeamReset guard;
        Seam::fail_walk_qps = kFails;
        Seam::as_e2_termination = false;
        QpAsNlpModel model = steep_line_model(vec2(1.5 + kOff, 1.5));
        SqpOptions o = quiet();
        o.hessian_mode = mode;
        o.trust_region_initial = 1.0;
        o.trust_region_min = 0.75;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());

        // THE PREMISE: one restoration, entered at the start point, whose row misses by more
        // than feas_tol while its sigma-scaled slack sits inside feas_tol of zero.
        ASSERT_EQ(Seam::forced, kFails);
        ASSERT_EQ(sol.counters.restoration_entries, 1);
        ASSERT_NEAR(sol.counters.restoration_h_in, kSigma * kOff, 1e-12);
        ASSERT_GT(sol.counters.restoration_h_in, o.common.feasibility_tol);
        ASSERT_LT(sol.counters.restoration_h_in / kSigma, o.common.feasibility_tol);

        // THE CLAIM IS NEVER MADE, and the phase did real work: a slack whose row is violated
        // beyond feas_tol is not at its bound, so the sub-solve cannot stop at its major 0.
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_NE(sol.status, SolveStatus::kInfeasible);
        EXPECT_GE(sol.counters.restoration_iters, 1);
        EXPECT_LE(sol.counters.restoration_h_out, o.common.feasibility_tol)
            << "the phase restores and resumes";
        EXPECT_LE(sol.counters.restoration_feas_out, o.common.feasibility_tol)
            << "the resume decision's reading";
        // AND THE SOLVE GOES ON TO THE OPTIMUM of the feasible problem.
        EXPECT_EQ(sol.status, SolveStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 2.5, 1e-6);
        EXPECT_NEAR(sol.x(1), 2.5, 1e-6);
    }
}

// THE ENTRY RULE'S PROGRESS GUARD (M7 W2 RST; SqpOptions::max_restoration_entries).
// L1A's shape with the cap well above one: restoration resumes at a feasible point,
// every QP after it fails, and the loop accepts NO step before it asks again. The
// request comes from the point the phase has just restored, so it is refused -- one
// entry, no claim: at a cap of one by the cap (kNumericalError), above it as no progress
// (RST-B: kStalled beside kRestorationNoProgress, formerly kNumericalError).
TEST(SqpLadderSeam, ARequestWithNoAcceptedStepSinceTheResumeIsRefusedAtAnyCap) {
    for (const Index cap : {Index{1}, Index{10}}) {
        SCOPED_TRACE(cap);
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::only_after_resume = true;
        RunawayValleyModel model;
        SqpOptions o = quiet();
        o.trust_region_initial = 8.0;
        o.common.max_iterations = 200;
        o.advanced.max_consecutive_qp_failures = 1000;
        o.advanced.max_restoration_entries = cap;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
        ASSERT_GE(Seam::forced, 1);
        EXPECT_EQ(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.status, cap == 1 ? SolveStatus::kNumericalError : SolveStatus::kStalled);
        EXPECT_EQ(sol.stop_reason,
                  cap == 1 ? StopReason::kRestorationRefused : StopReason::kRestorationNoProgress);
        EXPECT_FALSE(sol.infeasibility_certified);
        EXPECT_LE(std::abs(model.eval_ce(sol.x)(0)), o.common.feasibility_tol);
        // The second request is the first one tested: unproductive, within the tolerance.
        EXPECT_EQ(sol.counters.restoration_unproductive_entries, 1);
        EXPECT_EQ(sol.counters.restoration_no_progress_requests, 0);
        // RST-B: below the cap the refusal is the entry rule's and is counted as such; the cap's
        // own refusal is not. Either way the point it was refused at is recorded, here feasible.
        EXPECT_EQ(sol.counters.restoration_no_accept_refusals, cap == 1 ? 0 : 1);
        EXPECT_LE(sol.counters.restoration_refusal_feasibility, o.common.feasibility_tol);
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
        o.trust_region_initial = 8.0;
        o.common.max_iterations = 200;
        o.advanced.max_consecutive_qp_failures = 1000;
        o.advanced.max_restoration_entries = cap;
        SqpSolver driver(o);
        const SqpResult sol = driver.solve(hven::detail::problem_ref(model), (model).start_point());
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
            o.common.max_iterations = 200;
            o.advanced.restoration_curvature = arm.curvature;
            const SqpResult sol =
                SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
            ASSERT_EQ(sol.status, SolveStatus::kInfeasible);
            ASSERT_EQ(sol.counters.restoration_curvature_switches, 0) << "fixture premise";
            hess_unforced = model.n_hess;
        }
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = 1;
        CountingCircleLineModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        o.advanced.restoration_curvature = arm.curvature;
        const SqpResult sol =
            SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
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
    o.common.max_iterations = 200;
    ASSERT_EQ(o.advanced.restoration_curvature, SqpRestorationCurvature::kModelThenRegularized);
    SqpSolver solver(o);
    int depth1_rows = 0;
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            if ((e.sqp ? e.sqp->depth : Index{0}) >= 1) {
                ++depth1_rows;
                return CallbackAction::kStop;
            }
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    const SqpResult sol = solver.solve(hven::detail::problem_ref(model), (model).start_point());
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
    o.common.max_iterations = 300;
    ASSERT_EQ(o.advanced.restoration_curvature, SqpRestorationCurvature::kModelThenRegularized);
    const SqpResult sol =
        SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(Seam::forced_restoration, 1);
    EXPECT_EQ(sol.counters.restoration_curvature_switches, 0);
    EXPECT_EQ(sol.counters.eval_hess_calls, 0);
    EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
}

// THE SWITCH'S SECOND TRIGGER (M7 W2 RST rider M1). The phase's first walk subproblem is
// forced kInfeasible and its elastic ladder exhausted. Under kModelThenRegularized the
// refusal of the nested request is the switch: the phase restarts from the unmoved iterate on
// the regularized model and certifies the same infeasible point. RF-1 B2 (declared pin move,
// design 4.3): under kModel the exhausted ladder is now a routed failure inside the phase -- a
// rung is left at that iterate, so the phase goes on and certifies the same point (was: the
// nested request refused, numerical_error, no claim).
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
        o.common.max_iterations = 200;
        o.advanced.restoration_curvature = curvature;
        const SqpResult sol =
            SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
        ASSERT_EQ(Seam::forced_restoration, 1);
        ASSERT_EQ(Seam::exhaust_restoration_elastic, 0) << "the forced exit reached the knob";
        EXPECT_GE(sol.counters.elastic_activations, 1) << "the elastic tier ran first";
        EXPECT_EQ(sol.counters.restoration_entries, 1);
        EXPECT_EQ(sol.counters.restoration_curvature_switches, switches ? 1 : 0);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        const double t = 1.0 / std::sqrt(2.0);
        EXPECT_NEAR(sol.x(0), t, 1e-5);
        EXPECT_NEAR(sol.x(1), t, 1e-5);
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

namespace {
// F9's inconsistent duplicate pair (test_equality_basis_driver.cpp) WITHOUT the equality basis:
// c0 = x0 - 1, c1 = x0 - 1.001, the start (1.0005, 0) their least-squares point and the
// objective's own stationary point, so QP 0 is inconsistent and its elastic optimum is the zero
// step with both slacks open (l1 0.001): an exhausted ladder at h = 0.001.
QpAsNlpModel inconsistent_pair_model() {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.g = Vec(2);
    qp.g << -2.0 * 1.0005, 0.0;
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 0.0, 1.0, 0.0;
    qp.Ae = A.sparseView();
    qp.be = vec2(1.0, 1.001);
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return QpAsNlpModel(std::move(qp), vec2(1.0005, 0.0));
}

// The pair's first major under the seam: the ladder ran, the row is an elastic row that
// asked for restoration, and the solve ended where the unseamed control does, far inside
// max_iter -- never the zero step committed to the iteration cap.
void expect_first_major_restores(const SqpResult &sol, const SqpResult &control,
                                 const SqpOptions &o) {
    EXPECT_GE(Seam::off_rows_forced, 1);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    EXPECT_GE(sol.counters.restoration_entries, 1);
    // The solve's first elastic row (QP 0's, with the equality basis off).
    const auto row = std::find_if(sol.history.begin(), sol.history.end(),
                                  [](const IterationRecord &r) { return r.sqp->elastic; });
    ASSERT_NE(row, sol.history.end());
    EXPECT_EQ(row->sqp->verdict, StepVerdict::kRestore);
    EXPECT_NE(sol.status, SolveStatus::kIterationLimit);
    EXPECT_LT(sol.counters.major_iters, o.common.max_iterations);
    EXPECT_EQ(sol.status, control.status);
}
} // namespace

// WC (3): the rung the excuse returned -- p = 0, s = 0, kOptimal, both rows open by 5e-4 -- read
// `closed` on its slacks and handed the driver a usable zero step. The rung-residual bound reads
// the open rows: not closed, not reduced, no objective promise, so the ladder is exhausted. The
// pair's QP 0 directly (p0 = -5e-4 and p0 = +5e-4), both arms; `report_reads_slacks_only` shows
// the seam reproduces the defect's verdict.
TEST(SqpLadderSeam, WalkCapARungOffItsRowsIsNotClosed) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 0.0, 1.0, 0.0;
    qp.Ae = A.sparseView();
    qp.be = vec2(-5.0e-4, 5.0e-4);
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = vec2(-10.0 - 1.0005, -10.0);
    qp.upper = vec2(10.0 - 1.0005, 10.0);
    for (const auto policy :
         {SqpOptions{}.linear.dense_factorization, hven::linear::DensePolicy::kNever}) {
        for (const bool slacks_only : {false, true}) {
            SCOPED_TRACE(::testing::Message() << "policy " << static_cast<int>(policy)
                                              << " slacks_only " << slacks_only);
            const SeamReset guard;
            Seam::off_rows_rungs = 1;
            Seam::report_reads_slacks_only = slacks_only;
            const SqpOptions o = quiet();
            QpEngine engine(o.qp, 0, policy);
            SqpCounters counters;
            const ElasticLadderReport report =
                run_elastic_ladder(engine, qp, ElasticSeedSource{},
                                   std::numeric_limits<double>::infinity(), o, counters);
            ASSERT_EQ(Seam::off_rows_forced, 1);
            ASSERT_EQ(report.qp_status, QpStatus::kOptimal);
            EXPECT_EQ(report.slack_l1, 0.0) << "the seam's rung: every slack shut";
            EXPECT_EQ(report.step_norm, 0.0) << "the seam's rung: the zero step";
            EXPECT_NEAR(report.rung_residual_l1, 1.0e-3, 1e-15) << "both rows open by 5e-4";
            EXPECT_FALSE(report.promises_f);
            if (slacks_only) {
                EXPECT_TRUE(report.closed) << "the defect's verdict, reproduced";
                EXPECT_TRUE(report.usable);
            } else {
                EXPECT_FALSE(report.closed);
                EXPECT_FALSE(report.reduced);
                EXPECT_FALSE(report.usable) << "exhausted: the driver restores";
            }
        }
    }
}

// WC (2): the same rung judged on its slacks alone (the report before the bound) is closed and
// usable, so the driver receives a zero elastic step at h = 0.001 > feas_tol. KLV's ||d|| = 0
// short-circuit would commit it unjudged at every major; it is exhaustion, routed to restoration.
TEST(SqpLadderSeam, WalkCapAZeroElasticStepAtAnInfeasibleIterateIsExhaustion) {
    for (const auto policy :
         {SqpOptions{}.linear.dense_factorization, hven::linear::DensePolicy::kNever}) {
        SCOPED_TRACE(static_cast<int>(policy));
        QpAsNlpModel model = inconsistent_pair_model();
        SqpOptions o = quiet();
        o.linear.dense_factorization = policy;
        o.common.max_iterations = 100;
        o.advanced.equality_basis = false; // QP 0 carries both rows: the elastic tier's own
        Seam::reset();
        const SqpResult control =
            SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
        ASSERT_FALSE(control.history.empty());
        ASSERT_TRUE(control.history.front().sqp->elastic);
        ASSERT_EQ(control.history.front().sqp->verdict.value_or(hven::StepVerdict::kReject),
                  StepVerdict::kRestore)
            << "fixture premise: the genuine ladder is exhausted at major 0";
        const SeamReset guard;
        Seam::off_rows_rungs = 1000;
        Seam::report_reads_slacks_only = true;
        const SqpResult sol =
            SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
        expect_first_major_restores(sol, control, o);
    }
}

// A RUNG OFF ITS ROWS DOES NOT STOP THE CLIMB. Consistent rows (x0 = 5e-4, twice): the genuine
// first rung is shut and the ladder stops there. With that rung replaced by the zero step off
// its rows (slacks 0, rows open by 1e-3) the slacks read shut but the rows do not, so the ladder
// escalates.
TEST(SqpLadderSeam, WalkCapARungOffItsRowsDoesNotStopTheEscalation) {
    QpProblem qp;
    qp.H = SpMatRM(2, 2);
    qp.H.insert(0, 0) = 2.0;
    qp.H.insert(1, 1) = 2.0;
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 0.0, 1.0, 0.0;
    qp.Ae = A.sparseView();
    qp.be = vec2(5.0e-4, 5.0e-4);
    qp.Ai = SpMatRM(0, 2);
    qp.bi = Vec(0);
    qp.lower = vec2(-10.0 - 1.0005, -10.0);
    qp.upper = vec2(10.0 - 1.0005, 10.0);
    Index escalations_control = -1;
    for (const int forced : {0, 1}) {
        SCOPED_TRACE(forced);
        const SeamReset guard;
        Seam::off_rows_inloop_rungs = forced;
        const SqpOptions o = quiet();
        QpEngine engine(o.qp, 0, SqpOptions{}.linear.dense_factorization);
        SqpCounters counters;
        const ElasticLadderReport report = run_elastic_ladder(
            engine, qp, ElasticSeedSource{}, std::numeric_limits<double>::infinity(), o, counters);
        ASSERT_EQ(Seam::off_rows_forced, forced);
        ASSERT_EQ(report.qp_status, QpStatus::kOptimal);
        if (forced == 0) {
            escalations_control = counters.elastic_escalations;
            EXPECT_EQ(escalations_control, 0) << "fixture premise: the genuine rung is shut";
        } else {
            EXPECT_GE(counters.elastic_escalations, 1) << "the off-rows rung is not shut";
        }
    }
}

TEST(SqpLadderSeam, EveryRowBlockIsReadOncePerCountedQueryWithBothRowKinds) {
    for (const int forced : {0, 1}) {
        SCOPED_TRACE(forced);
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = forced;
        CountingCircleHalfPlaneModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        const SqpResult sol =
            SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
        ASSERT_EQ(Seam::forced_restoration, forced);
        ASSERT_GE(sol.counters.restoration_entries, 1) << "fixture premise: the phase runs";
        EXPECT_EQ(sol.counters.restoration_curvature_switches, forced);
        EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(sol.infeasibility_certified);
        EXPECT_EQ(model.n_ce, sol.counters.evals_full + sol.counters.evals_values);
        EXPECT_EQ(model.n_ci, sol.counters.evals_full + sol.counters.evals_values);
    }
}

namespace {
// test_sqp_solver.cpp's W2T7ScaledInconsistentEqualitiesModel (the T7F1 fixture): two parallel
// equalities s*(x0 + x1 -+ 2) = 0 on a box, inconsistent, so the solve ends in the restoration
// phase whose feasibility sub-solve is entered at an infeasible stationary point.
class ScaledInconsistentEqualitiesModel : public NlpModel {
  public:
    ScaledInconsistentEqualitiesModel(double s, double b, double a) : s_(s), b_(b), a_(a) {}
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * x.squaredNorm(); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << s_ * (x(0) + x(1) - 2.0), s_ * (x(0) + x(1) + 2.0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = s_;
        j.insert(0, 1) = s_;
        j.insert(1, 0) = s_;
        j.insert(1, 1) = s_;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        lo_ = Vec::Constant(2, -b_);
        return lo_;
    }
    const Vec &upper() const override {
        up_ = Vec::Constant(2, b_);
        return up_;
    }
    Vec start_point() const override { return Vec::Constant(2, a_); }

  private:
    double s_, b_, a_;
    mutable Vec lo_, up_;
};
} // namespace

// ZERO-STEP AGREEMENT (macOS/Accelerate runs 43719577164, 43720140103, 43720765090). Inside the
// restoration sub-solve the walk answered a step of 1.07e-10 where MKL answers exactly 0. The
// walk reads that as no motion (step_tol = feasibility_tol * max(1, |x|), 1e-9); the driver's
// zero-step short-circuit used 1e-12, judged the noise, and the funnel rejected it 34 times: the
// multipliers were discarded, the radius collapsed and the solve ended kNumericalError,
// uncertified. The seam adds 1e-10 to the walk's no-motion restoration steps; the verdict must
// stay the certified kInfeasible the unperturbed solve reaches, on every mode and both arms.
TEST(SqpLadderSeam, ZeroStepANoiseStepTheWalkReadsAsNoMotionIsAZeroStepToTheDriver) {
    for (const auto policy :
         {hven::linear::DensePolicy::kAuto, hven::linear::DensePolicy::kNever}) {
        for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpqp}) {
            SCOPED_TRACE(
                fmt::format("policy {} mode {}", static_cast<int>(policy), static_cast<int>(mode)));
            SqpOptions o = quiet();
            o.linear.dense_factorization = policy;
            o.advanced.equality_basis = false;
            o.qp_mode = mode;
            o.common.max_iterations = 60;
            ScaledInconsistentEqualitiesModel model(1.0e2, 2.0, 0.5);
            Seam::reset();
            const SqpResult control =
                SqpSolver(o).solve(hven::detail::problem_ref(model), model.start_point());
            ASSERT_EQ(control.status, SolveStatus::kInfeasible) << "fixture premise";
            ASSERT_TRUE(control.infeasibility_certified);
            const SeamReset guard;
            Seam::perturb_restoration_walk_step = 1.0e-10;
            const SqpResult sol =
                SqpSolver(o).solve(hven::detail::problem_ref(model), model.start_point());
            EXPECT_GE(Seam::perturbed_restoration_steps, 1) << "the seam must have fired";
            EXPECT_GE(sol.counters.restoration_iters, 1);
            EXPECT_EQ(sol.status, SolveStatus::kInfeasible);
            EXPECT_TRUE(sol.infeasibility_certified);
            EXPECT_LT(sol.counters.major_iters, o.common.max_iterations);
        }
    }
}

// ---------------------------------------------------------------------------
// RF-1 -- the retry ladder, the stagnation projection and the floor exit (design 3.1-3.4)
// ---------------------------------------------------------------------------

namespace {

Index rungs_taken(const SqpResult &r) {
    return r.counters.qp_retry_rung_shrink + r.counters.qp_retry_rung_cold +
           r.counters.qp_retry_rung_algebra + r.counters.qp_retry_rung_regularization;
}

SqpResult solve_line(const Vec &x0, const SqpOptions &o = quiet()) {
    QpAsNlpModel model = line_model(x0);
    SqpSolver driver(o);
    return driver.solve(hven::detail::problem_ref(model), (model).start_point());
}

} // namespace

// Routed failures at one feasible iterate each take a different rung: R1 (the forced step reaches
// past the shrunk radius), R3 (no seed to drop, so R2 is skipped), then none (R4 needs a dependent
// or zero-motion exit), so the floor rule requests restoration. The forced iterate is the same
// point each time and the shrink left the walk's counts unchanged: the second attempt is a replay.
TEST(Rf1RetryLadderSeam, EachRoutedFailureTakesADifferentRung) {
    const SeamReset guard;
    Seam::fail_walk_qps = 3;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.5, 1.5};
    const SqpResult sol = solve_line(vec2(0.0, 2.0));
    EXPECT_EQ(Seam::forced, 3);
    EXPECT_EQ(sol.counters.qp_failures_routed, 3);
    EXPECT_EQ(sol.counters.qp_retry_replays, 1);
    EXPECT_EQ(sol.counters.qp_retry_rung_shrink, 1);
    EXPECT_EQ(sol.counters.qp_retry_rung_cold, 0);
    EXPECT_EQ(sol.counters.qp_retry_rung_algebra, 1);
    EXPECT_EQ(rungs_taken(sol), 2);
    EXPECT_EQ(sol.counters.restoration_entries, 1);
    expect_line_optimum(sol);
}

// A dependent-blocker failure reaches R4 once the others are spent (budget raised to four).
TEST(Rf1RetryLadderSeam, ADependentBlockerFailureReachesTheRegularizationRung) {
    const SeamReset guard;
    Seam::fail_walk_qps = 4;
    Seam::as_e2_termination = true; // at a feasible iterate the E2 route does not apply
    Seam::forced_point = {0.5, 1.5};
    SqpOptions o = quiet();
    o.advanced.max_consecutive_qp_failures = 4;
    const SqpResult sol = solve_line(vec2(0.0, 2.0), o);
    EXPECT_EQ(Seam::forced, 4);
    EXPECT_EQ(sol.counters.qp_retry_rung_regularization, 1);
    EXPECT_EQ(rungs_taken(sol), 3); // R1, R3, R4
    expect_line_optimum(sol);
}

// THE REPLAY GUARD: three failures with one fingerprint are one charged failure and two replays.
TEST(Rf1RetryLadderSeam, AnIdenticalFailureIsCountedAsAReplayAndNotCharged) {
    const SeamReset guard;
    Seam::fail_walk_qps = 3;
    Seam::as_e2_termination = false;
    Seam::as_replay = true;
    Seam::forced_point = {0.5, 1.5};
    const SqpResult sol = solve_line(vec2(0.0, 2.0));
    EXPECT_EQ(Seam::forced, 3);
    EXPECT_EQ(sol.counters.qp_retry_replays, 2);
    EXPECT_EQ(sol.counters.qp_failures_routed, 3);
    EXPECT_EQ(sol.counters.qp_failures_in_a_row_peak, 1);
    expect_line_optimum(sol);
}

// AFTER THE RUNGS, an infeasible linearization goes to the elastic tier (design 3.1).
TEST(Rf1RetryLadderSeam, SpentRungsAtAnInfeasibleIterateGoElastic) {
    const SeamReset guard;
    Seam::fail_walk_qps = 4;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.5, 0.5};
    const SqpResult sol = solve_line(vec2(0.0, 0.0));
    EXPECT_EQ(Seam::forced, 4);
    EXPECT_EQ(sol.counters.qp_failures_routed, 3);
    EXPECT_GE(sol.counters.elastic_activations, 1);
    expect_line_optimum(sol);
}

// THE STAGNATION PROJECTION (design 3.2): an off-box stagnation iterate is projected and routed.
TEST(Rf1RetryLadderSeam, AnOffBoxStagnationIterateIsProjectedAndRouted) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::as_stagnation_stop = true;
    Seam::forced_point = {0.0, 15.0}; // outside the box [-10, 10]
    const SqpResult sol = solve_line(vec2(0.0, 2.0));
    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_box_projections, 1);
    EXPECT_EQ(sol.counters.qp_failures_routed, 1);
    expect_line_optimum(sol);
}

// ... and only a stagnation stop: any other off-box failure is refused as before.
TEST(Rf1RetryLadderSeam, AnOffBoxFailureOfAnotherKindIsNotProjected) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.0, 15.0};
    const SqpResult sol = solve_line(vec2(0.0, 2.0));
    EXPECT_EQ(Seam::forced, 1);
    EXPECT_EQ(sol.counters.qp_failure_box_projections, 0);
    EXPECT_EQ(sol.counters.qp_failures_routed, 0);
    EXPECT_NE(sol.status, SolveStatus::kOptimal);
}

// THE FLOOR EXIT (design 3.4): failures that promise nothing at a feasible point end the solve
// instead of entering a no-op restoration -- at the floor as the floor exit, and (RST-B F3; the
// RF-1 pin read trust_region_floor here) with no rung left above the floor as the subproblem's
// own failure, the same point and path under its honest label.
TEST(Rf1RetryLadderSeam, ANoMotionFailureAtAFeasiblePointEndsAtTheFloor) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.0, 2.0}; // the iterate itself: a zero step, pred 0
    SqpOptions o = quiet();
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75; // one shrink of 1.0 crosses the floor
    const SqpResult sol = solve_line(vec2(0.0, 2.0), o);
    EXPECT_EQ(sol.counters.trust_region_floor_exits, 1);
    EXPECT_EQ(sol.counters.restoration_entries, 0);
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kTrustRegionFloor);
}

TEST(Rf1RetryLadderSeam, ANoMotionFailureAboveTheFloorIsTheSubproblemFailure) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.0, 2.0};
    SqpOptions o = quiet();
    o.trust_region_initial = 1.0; // the rungs are spent at Delta = 1, ten orders above the floor
    const SqpResult sol = solve_line(vec2(0.0, 2.0), o);
    EXPECT_EQ(sol.counters.trust_region_floor_exits, 0);
    EXPECT_EQ(sol.counters.restoration_entries, 0);
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(sol.stop_reason, StopReason::kSubproblemFailure);
    EXPECT_EQ(sol.x(0), 0.0);
    EXPECT_EQ(sol.x(1), 2.0);
}

// RST-B F1: a zero-motion exhaustion reaches R4 once R1 and R3 are spent (three forced failures);
// F2: under qp_retry_regularization_first it is taken ahead of R3 (two forced failures, the third
// QP solves); without F1 neither happens.
TEST(Rf1RetryLadderSeam, AZeroMotionExhaustionReachesTheRegularizationRung) {
    const SeamReset guard;
    const auto run = [](int failures, bool first) {
        Seam::reset();
        Seam::fail_walk_qps = failures;
        Seam::as_e2_termination = false;
        Seam::as_zero_motion_exhausted = true;
        Seam::forced_point = {0.5, 1.5};
        SqpOptions o = quiet();
        o.advanced.qp_retry_regularization_first = first;
        o.advanced.max_restoration_entries = 0; // a ladder with no rung left ends the solve
        const SqpResult r = solve_line(vec2(0.0, 2.0), o);
        std::printf("[ rstb ] zero-motion failures=%d first=%d: shrink=%ld cold=%ld algebra=%ld "
                    "reg=%ld status=%d\n",
                    failures, first ? 1 : 0, static_cast<long>(r.counters.qp_retry_rung_shrink),
                    static_cast<long>(r.counters.qp_retry_rung_cold),
                    static_cast<long>(r.counters.qp_retry_rung_algebra),
                    static_cast<long>(r.counters.qp_retry_rung_regularization),
                    static_cast<int>(r.status));
        return r;
    };
    const SqpResult f1 = run(3, false);
    EXPECT_EQ(Seam::forced, 3);
    EXPECT_EQ(f1.counters.qp_retry_rung_algebra, 1);
    EXPECT_EQ(f1.counters.qp_retry_rung_regularization, 1) << "F1: R4 after R3";
    expect_line_optimum(f1);
    const SqpResult ordered = run(2, false);
    EXPECT_EQ(ordered.counters.qp_retry_rung_algebra, 1);
    EXPECT_EQ(ordered.counters.qp_retry_rung_regularization, 0);
    const SqpResult f2 = run(2, true);
    EXPECT_EQ(f2.counters.qp_retry_rung_algebra, 0);
    EXPECT_EQ(f2.counters.qp_retry_rung_regularization, 1) << "F2: R4 ahead of R3";
    expect_line_optimum(f2);
}

// ... and a failure that still promises a decrease requests restoration as before.
TEST(Rf1RetryLadderSeam, AFailureThatPromisesADecreaseStillRequestsRestoration) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.5, 1.5};
    const SqpResult sol = solve_line(vec2(0.0, 2.0));
    EXPECT_EQ(sol.counters.trust_region_floor_exits, 0);
    EXPECT_GE(sol.counters.restoration_entries, 1);
}

// THE FAILURE STILL PROPAGATES (moved here from SqpSolverQpFailure.RepeatedQpNumericalErrorStill
// Propagates, whose TwinSaddle fixture the RF-1 rungs now rescue -- that rescue is pinned on its
// own, SqpSolverQpFailure.TheRungsRescueTheTwinSaddleAtABudgetOfOne). Every walk QP is forced to
// fail at a feasible iterate whose forced step still promises a decrease (so the floor exit does
// not apply) with a budget of one routed failure: the first failure takes a rung, the second finds
// the budget spent at h <= feas_tol and ends the solve kSubproblemFailure at the start point, in
// both algebras.
namespace {
SqpResult propagating_failure_solve(QpWorkingSetAlgebra algebra, double acceptable) {
    SqpOptions o = quiet();
    o.qp.working_set_algebra = algebra;
    o.advanced.max_consecutive_qp_failures = 1;
    o.termination.acceptable_stationarity_tol = std::max(acceptable, o.common.optimality_tol);
    o.termination.acceptable_equality_tol = std::max(acceptable, o.common.feasibility_tol);
    o.termination.acceptable_inequality_tol = std::max(acceptable, o.common.feasibility_tol);
    o.termination.acceptable_complementarity_tol = std::max(acceptable, o.common.optimality_tol);
    Seam::reset();
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {0.5, 1.5};
    return solve_line(vec2(0.0, 2.0), o);
}
} // namespace

TEST(Rf1RetryLadderSeam, ARepeatedSubproblemFailureStillPropagates) {
    const SeamReset guard;
    for (const auto algebra :
         {QpWorkingSetAlgebra::kSchurBorder, QpWorkingSetAlgebra::kRefactorize}) {
        SCOPED_TRACE(algebra == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize");
        const SqpResult sol = propagating_failure_solve(algebra, 0.0);
        std::printf("[ rf1 ] propagate: status=%d reason=%d rows=%zu routed=%ld rungs=%ld "
                    "resto=%ld floor=%ld elastic=%ld msg=%s\n",
                    static_cast<int>(sol.status), static_cast<int>(sol.stop_reason),
                    sol.history.size(), static_cast<long>(sol.counters.qp_failures_routed),
                    static_cast<long>(rungs_taken(sol)),
                    static_cast<long>(sol.counters.restoration_entries),
                    static_cast<long>(sol.counters.trust_region_floor_exits),
                    static_cast<long>(sol.counters.elastic_activations), sol.message.c_str());
        EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
        EXPECT_EQ(sol.stop_reason, StopReason::kSubproblemFailure);
        EXPECT_EQ(sol.message,
                  "numerical error: the QP subproblem failed and its retry budget was spent");
        ASSERT_EQ(sol.history.size(), 2u);
        for (const IterationRecord &row : sol.history) {
            EXPECT_EQ(row.sqp->qp_status.value_or(QpStatus::kOptimal), QpStatus::kNumericalError);
            EXPECT_TRUE(row.sqp->qp_solved);
        }
        EXPECT_EQ(sol.counters.qp_failures_routed, 1);
        EXPECT_EQ(rungs_taken(sol), 1);
        EXPECT_EQ(sol.counters.steps_accepted, 0);
        EXPECT_EQ(sol.counters.restoration_entries, 0);
        EXPECT_EQ(sol.counters.trust_region_floor_exits, 0);
        EXPECT_EQ(sol.counters.elastic_activations, 0);
        EXPECT_EQ(sol.x(0), 0.0) << "the exit is at the start point";
        EXPECT_EQ(sol.x(1), 2.0);
    }
}

// The same exit under a tier wide enough to admit its point (moved here with the fixture above
// from Rf1AcceptableLabel, test_sqp_solver.cpp): the label moves, nothing else does.
TEST(Rf1RetryLadderSeam, ASubproblemFailureAtAnAcceptablePointIsLabelledAndNothingElseMoves) {
    const SeamReset guard;
    const SqpResult tight = propagating_failure_solve(QpWorkingSetAlgebra::kSchurBorder, 0.0);
    const SqpResult wide = propagating_failure_solve(QpWorkingSetAlgebra::kSchurBorder, 1e10);
    ASSERT_EQ(tight.status, SolveStatus::kNumericalError);
    ASSERT_EQ(tight.stop_reason, StopReason::kSubproblemFailure);
    EXPECT_EQ(wide.status, SolveStatus::kAcceptable);
    EXPECT_EQ(wide.stop_reason, StopReason::kNone);
    EXPECT_EQ(wide.message, "converged to an acceptable level: the solve stopped at a point "
                            "inside the acceptable tolerances: exit subproblem_failure");
    EXPECT_EQ(wide.counters.major_iters, tight.counters.major_iters);
    EXPECT_EQ(wide.counters.factorizations, tight.counters.factorizations);
    EXPECT_EQ(wide.history.size(), tight.history.size());
    EXPECT_EQ(wide.f, tight.f);
    ASSERT_EQ(wide.x.size(), tight.x.size());
    EXPECT_TRUE((wide.x.array() == tight.x.array()).all());
}

// RF-1 ruling S-1: a stalled phase ends kWorkLimit at its best-h point; the parent labels it
// kRestorationWorkLimit (kStalled), not the caller's kWorkLimit. RF-1c re-pin: a window no longer
// cuts before the phase has made kRestorationStallMinPasses passes, so the circle/line fixture
// (a phase of five passes) is not cut at any window, and the phase that is cut here is the
// bouncing spread phase of TheStallExitTakesTheReductionResume, whose best h goes flat.
TEST(Rf1RestorationSeam, AStalledPhaseIsTheRestorationWorkLimit) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    o.advanced.restoration_stall_factorizations = 2;
    o.advanced.restoration_resume_reduction = 0.0;
    const SqpResult sol =
        SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    EXPECT_EQ(sol.counters.restoration_entries, 1);
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationWorkLimit);
    EXPECT_FALSE(sol.infeasibility_certified);
}

// ... and the caller's own ceiling, when it is the smaller bound, stays kWorkLimit.
TEST(Rf1RestorationSeam, TheCallersCeilingInsideThePhaseStaysTheWorkLimit) {
    const SeamReset guard;
    CountingCircleLineModel model;
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.advanced.restoration_stall_factorizations = 100000;
    // Unforced, this fixture enters restoration once; a ceiling reached inside the phase.
    SqpOptions probe = o;
    const SqpResult free_run =
        SqpSolver(probe).solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(free_run.counters.restoration_entries, 1);
    // RST-B: the phase's own share of the solve's factorizations.
    EXPECT_GT(free_run.counters.restoration_factorizations, 0);
    EXPECT_LT(free_run.counters.restoration_factorizations, free_run.counters.factorizations);
    CountingCircleLineModel model2;
    o.common.max_factorizations = free_run.counters.factorizations - 1;
    const SqpResult sol =
        SqpSolver(o).solve(hven::detail::problem_ref(model2), (model2).start_point());
    EXPECT_EQ(sol.status, SolveStatus::kWorkLimit);
}

// RF-1c re-pin of RF-1 ruling S-1's two edges on one phase. The circle/line phase is entered at
// h_in = 1.0015 and certifies at h = 0.5858 in 5 majors; before RF-1c a window of 1 factorization
// cut it after one major and a window of 2 let it run. The window now needs
// kRestorationStallMinPasses + 1 observed passes, so a five-pass phase is never cut, at a window of
// 0, 1 or 2 alike (the fall test itself is pinned on the window object, Rf1RestorationStall).
TEST(Rf1RestorationSeam, APhaseShorterThanThePassFloorIsNotCutAtAnyWindow) {
    const auto solve = [](Index window) {
        const SeamReset guard;
        CountingCircleLineModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        o.advanced.restoration_stall_factorizations = window;
        return SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    };
    const SqpResult none = solve(1000000);
    ASSERT_EQ(none.status, SolveStatus::kInfeasible) << "fixture premise: the phase certifies";
    for (const Index window : {Index{0}, Index{1}, Index{2}}) {
        SCOPED_TRACE(window);
        const SqpResult cut = solve(window);
        EXPECT_NE(cut.status, SolveStatus::kStalled);
        EXPECT_NE(cut.stop_reason, StopReason::kRestorationWorkLimit);
        EXPECT_EQ(cut.status, none.status);
        EXPECT_EQ(cut.counters.restoration_iters, none.counters.restoration_iters);
        EXPECT_EQ(cut.counters.factorizations, none.counters.factorizations);
        EXPECT_EQ(cut.infeasibility_certified, none.infeasibility_certified);
    }
    const double t = 1.0 / std::sqrt(2.0);
    EXPECT_NEAR(none.x(0), t, 1e-5);
    EXPECT_NEAR(none.x(1), t, 1e-5);
}

// RF-1 ruling F-1 (the CRESC50 must-kill): a row refusal on the phase's curvature-switch run
// takes the switch, exactly as a plain failure does; RF-1's any-h elastic route would run the
// elastic tier at the phase's feasible start instead and bypass the regularized restart.
TEST(Rf1RestorationSeam, ARowRefusalOnTheSwitchRunTakesTheSwitch) {
    const auto solve = [](bool row_refusal) {
        const SeamReset guard;
        Seam::fail_restoration_walk_qps = 1;
        Seam::restoration_as_row_refusal = row_refusal;
        CountingCircleLineModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        SqpResult r = SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
        EXPECT_EQ(Seam::forced_restoration, 1);
        return r;
    };
    const SqpResult plain = solve(false);
    const SqpResult refused = solve(true);
    ASSERT_EQ(plain.counters.restoration_curvature_switches, 1);
    EXPECT_EQ(refused.counters.restoration_curvature_switches, 1);
    EXPECT_EQ(refused.counters.elastic_activations, plain.counters.elastic_activations);
    EXPECT_EQ(refused.counters.restoration_iters, plain.counters.restoration_iters);
    EXPECT_EQ(refused.status, SolveStatus::kInfeasible);
    EXPECT_TRUE(refused.infeasibility_certified);
}

// RF-1 design 4.4 as extended (the M19 must-kill): the reduction resume reads the stall's
// kWorkLimit. reduction_resume_solve's fixture, its phase bouncing around x0 = 0 at h ~ 1e-5
// (h_in = 10): with a window of 2 factorizations the phase stalls and hands back a point below
// 0.1 h_in, which resumes and is counted. With the reduction resume off, the same stall ends
// the solve kStalled / kRestorationWorkLimit: the stall, not another exit, ended the phase.
TEST(Rf1RestorationSeam, TheStallExitTakesTheReductionResume) {
    const auto solve = [](double k) {
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::as_e2_termination = false;
        Seam::forced_point = {1.0, 1.0};
        SpreadMissModel model(kNoClaimMiss);
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        o.trust_region_initial = 1.0;
        o.trust_region_min = 0.75;
        o.advanced.restoration_stall_factorizations = 2;
        o.advanced.restoration_resume_reduction = k;
        return SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    };
    const SqpResult off = solve(0.0);
    EXPECT_EQ(off.status, SolveStatus::kStalled);
    EXPECT_EQ(off.stop_reason, StopReason::kRestorationWorkLimit);
    EXPECT_EQ(off.counters.restoration_reduction_resumes, 0);
    const SqpResult on = solve(0.1);
    EXPECT_EQ(on.counters.restoration_entries, 1);
    EXPECT_EQ(on.counters.restoration_reduction_resumes, 1);
    EXPECT_LE(on.counters.restoration_h_out, 0.1 * on.counters.restoration_h_in);
    EXPECT_GT(on.counters.major_iters, off.counters.major_iters) << "the solve went on";
    EXPECT_FALSE(on.infeasibility_certified);
}

// RF-1 design 4.2 / ruling B2-1 (the M13 must-kill): the sub-driver hands back the point its
// order holds, not its last one. One linear row x0 = 0 entered at x0 = 20: every walk QP of the
// parent fails at its start point and every elastic ladder ends unusable (`off_rows_rungs`), so
// the parent requests restoration at major 0 (h_in = 20). Every phase iterate is feasible for the
// phase and lowers h (19, 17, 13 as the radius doubles), so unseamed the last point IS the best.
// The solve is capped at 4 majors, so the phase ends at the cap after three. With the seam's
// capture limit of 1 the order keeps the phase's first improvement (h = 19), and that point --
// worse than the phase's last (h = 13) -- is the one handed back. No unseamed small model was
// found that ends a phase at a non-optimal exit after a captured point worse than its last (the
// bouncing spread phase is never phase-feasible before it converges; the circle phase's
// walk-tolerance iterates are not capturable; RunawayValley's second phase bounces but captures
// nothing), so the limit makes the two points differ.
namespace {
class LinearRowModel final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override { return vec2(0.0, 2.0 * x(1)); }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0)); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(1, 1) = 2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -100.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 100.0);
        return u;
    }
    Vec start_point() const override { return vec2(20.0, 1.0); }
};

SqpResult linear_row_hand_back(int capture_limit) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {20.0, 1.0};
    Seam::off_rows_rungs = 1000;
    Seam::restoration_capture_limit = capture_limit;
    LinearRowModel model;
    SqpOptions o = quiet();
    o.common.max_iterations = 4;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    return SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
}
} // namespace

TEST(Rf1RestorationSeam, ThePhaseHandsBackItsBestPointNotItsLast) {
    const SqpResult free_run = linear_row_hand_back(0);
    ASSERT_EQ(free_run.counters.restoration_entries, 1);
    ASSERT_EQ(free_run.counters.restoration_iters, 3);
    EXPECT_EQ(free_run.status, SolveStatus::kIterationLimit);
    EXPECT_DOUBLE_EQ(free_run.counters.restoration_h_in, 20.0);
    EXPECT_DOUBLE_EQ(free_run.counters.restoration_h_out, 13.0) << "the last point is the best";
    const SqpResult limited = linear_row_hand_back(1);
    ASSERT_EQ(limited.counters.restoration_iters, 3) << "the limit moves no iterate";
    EXPECT_DOUBLE_EQ(limited.counters.restoration_h_out, 19.0)
        << "the order's point (the first improvement), not the phase's last";
    EXPECT_DOUBLE_EQ(limited.x(0), 19.0);
}

// RF-1b (ruling C481; the YORKNET 055-057 must-kill): the curvature-switch run's switch exit hands
// back no best point, so the regularized restart starts from the SWITCH point, as before RF-1.
// linear_row_hand_back's phase (h 19, 17, 13 as the radius doubles), but its third walk subproblem
// fails, so the first run switches after two steps: its last point is h = 17. With the seam's
// capture limit of 1 the restoration order's best point is the first improvement (h = 19), worse
// than the switch point. The restart's start is the point the run handed back, so a solve with
// the limit must be the solve without it: the same phase rows (read off a callback at depth 1: the
// restart's first row stands at the point it was started from), the same counters, the same point.
// Handing the best point back instead starts the restart at h = 19 and moves them.
TEST(Rf1RestorationSeam, TheSwitchRunHandsBackItsSwitchPointNotItsBestPoint) {
    std::vector<double> rows_free, rows_limited;
    const auto solve = [](int capture_limit, std::vector<double> &phase_rows) {
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::as_e2_termination = false;
        Seam::forced_point = {20.0, 1.0};
        Seam::off_rows_rungs = 1000;
        Seam::pass_restoration_walk_qps = 2;
        Seam::fail_restoration_walk_qps = 1;
        Seam::restoration_capture_limit = capture_limit;
        LinearRowModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 40;
        o.trust_region_initial = 1.0;
        o.trust_region_min = 0.75;
        SqpSolver solver(o);
        solver.set_iteration_callback(
            [&phase_rows](const IterationEvent &e) {
                if ((e.sqp ? e.sqp->depth : Index{0}) >= 1) {
                    phase_rows.push_back(e.x(0));
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll, CallbackScope::kIncludeRestoration);
        const SqpResult r = solver.solve(hven::detail::problem_ref(model), (model).start_point());
        EXPECT_EQ(Seam::forced_restoration, 1) << "the switch fired once";
        if (capture_limit > 0) {
            EXPECT_EQ(Seam::restoration_captures, 1) << "the premise: the order held h = 19";
        }
        return r;
    };
    const SqpResult free_run = solve(0, rows_free);
    ASSERT_EQ(free_run.counters.restoration_entries, 1);
    ASSERT_EQ(free_run.counters.restoration_curvature_switches, 1) << "fixture premise";
    const SqpResult limited = solve(1, rows_limited);
    EXPECT_EQ(rows_limited, rows_free) << "the restart starts where the switch run ended";
    ASSERT_EQ(limited.counters.restoration_curvature_switches, 1);
    EXPECT_EQ(limited.counters.restoration_iters, free_run.counters.restoration_iters);
    EXPECT_EQ(limited.counters.factorizations, free_run.counters.factorizations);
    EXPECT_EQ(limited.counters.major_iters, free_run.counters.major_iters);
    EXPECT_EQ(limited.counters.restoration_h_out, free_run.counters.restoration_h_out);
    EXPECT_EQ(limited.status, free_run.status);
    EXPECT_EQ(limited.x(0), free_run.x(0));
    EXPECT_EQ(limited.x(1), free_run.x(1));
}

// RF-1c: a phase that keeps falling survives its window by the fall test, not by the pass floor
// (the pin the five-pass circle/line fixture could no longer give). LinearRowModel's phase, run to
// its end from a small trust radius (0.25, doubling), takes eight passes with h falling 20 -> 0,
// far more than 10% over any five passes. With a window of one factorization every pass spans the
// window, so only the pass floor and the fall test decide: the phase is past the floor from the
// sixth pass and survives it because its best h has fallen against the reference five passes back.
// It is not cut and the solve equals the solve under a window that never binds. (A radius of 1.0
// ends the phase on its sixth pass, before the floor can be told from the fall test; a radius of
// 0.02 falls under 10% per five passes and is rightly cut.)
TEST(Rf1RestorationSeam, AFallingPhaseSurvivesItsWindowByTheFallTest) {
    const auto solve = [](Index window, std::size_t &phase_rows) {
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::as_e2_termination = false;
        Seam::forced_point = {20.0, 1.0};
        Seam::off_rows_rungs = 1000;
        LinearRowModel model;
        SqpOptions o = quiet();
        o.common.max_iterations = 60;
        o.trust_region_initial = 0.25;
        o.trust_region_min = 0.01;
        o.advanced.restoration_stall_factorizations = window;
        SqpSolver solver(o);
        phase_rows = 0;
        solver.set_iteration_callback(
            [&phase_rows](const IterationEvent &e) {
                if (e.sqp && e.sqp->depth >= 1) {
                    ++phase_rows;
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll, CallbackScope::kIncludeRestoration);
        return solver.solve(hven::detail::problem_ref(model), (model).start_point());
    };
    std::size_t rows_never = 0, rows_one = 0;
    const SqpResult never = solve(1000000, rows_never);
    ASSERT_EQ(never.counters.restoration_entries, 1);
    ASSERT_GE(rows_never, hven::detail::kRestorationStallMinPasses + 3)
        << "fixture premise: the phase outlives the floor by two passes";
    const SqpResult one = solve(1, rows_one);
    EXPECT_NE(one.stop_reason, StopReason::kRestorationWorkLimit);
    EXPECT_EQ(rows_one, rows_never);
    EXPECT_EQ(one.counters.restoration_iters, never.counters.restoration_iters);
    EXPECT_EQ(one.counters.factorizations, never.counters.factorizations);
    EXPECT_EQ(one.status, never.status);
}

// RF-1c (the M2 must-kill): a regularized restart starts a fresh window. LinearRowModel's phase
// is the RF-1b fixture's; the spread phase here is the one whose best h goes flat. The first run
// takes `pass` walk subproblems and fails the next, which switches to the regularized restart; the
// stall window is one factorization, so only the pass floor holds the phase open. A restart that
// inherited the first run's samples would be cut after the passes it needs to reach six in total;
// a fresh window needs five passes of the restart's own: the rows after the switch are counted, so
// a change to the first run cannot mask the mutant.
TEST(Rf1RestorationSeam, ARestartNeedsTheFloorsPassesOfItsOwn) {
    constexpr int kFirstRunPasses = 12;
    // Phase rows, in order: each row's subproblem status. The first run's failed subproblem (the
    // only row whose status is not kOptimal before the cut) is the last row of the first run; the
    // rows after it are the restart's own.
    std::vector<QpStatus> statuses;
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    Seam::pass_restoration_walk_qps = kFirstRunPasses;
    Seam::fail_restoration_walk_qps = 1;
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    o.advanced.restoration_stall_factorizations = 1;
    o.advanced.restoration_resume_reduction = 0.0;
    SqpSolver solver(o);
    solver.set_iteration_callback(
        [&statuses](const IterationEvent &e) {
            if (e.sqp && e.sqp->depth >= 1) {
                statuses.push_back(e.sqp->qp_status);
            }
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, CallbackScope::kIncludeRestoration);
    const SqpResult sol = solver.solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(sol.counters.restoration_curvature_switches, 1) << "fixture premise: a restart ran";
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationWorkLimit);
    const auto failed = std::find_if(statuses.begin(), statuses.end(),
                                     [](QpStatus q) { return q != QpStatus::kOptimal; });
    ASSERT_NE(failed, statuses.end()) << "fixture premise: the first run's failed subproblem";
    const std::size_t first_run_rows = static_cast<std::size_t>(failed - statuses.begin()) + 1;
    ASSERT_EQ(first_run_rows, static_cast<std::size_t>(kFirstRunPasses) + 1)
        << "fixture premise: twelve passes and the failed one";
    // The restart's own rows: the floor's five passes behind the reference, and the sixth is cut.
    // Measured: 6 with the fresh window, 3 without (the first run's twelve passes already behind
    // it). Rows of the first run play no part in this count.
    const std::size_t restart_rows = statuses.size() - first_run_rows;
    EXPECT_EQ(restart_rows, hven::detail::kRestorationStallMinPasses + 1);
}

// =========================================================================
// RST-B: THE PROGRESS TEST ACROSS RESTORATION ENTRIES, on scripted verdicts.
//
// The line model (feasible start, linear row) under a seam that fails every main walk QP: each
// failed QP's step is offered as a failure step, and a strategy built through
// `advanced.make_strategy` decides it. Interval k (k = 0 before the first resume) accepts the
// first `accepts[k]` judged steps and rejects every later one; `solved[k]` of its QPs are let
// through by the seam first (`pass_walk_qps`), so those judged steps come from solved
// subproblems. A rejected failure step is routed through the rungs, and with none left the floor
// branch requests restoration (the step still promises a decrease), which on the line model
// resumes at once. The restoration phase runs the default funnel (`make_strategy` is not copied).
// =========================================================================
namespace {

struct EntryScript {
    std::vector<int> accepts; // per interval; past the end, `accepts_after`
    std::vector<int> solved;  // per interval; past the end, 0
    int accepts_after = 1;
    int resumes = 0;
    int judged = 0;
    int accepted_solved = 0; // accepted judged steps of solved QPs (read by the test)
};

class ScriptedEntryStrategy final : public GlobalizationStrategy {
  public:
    explicit ScriptedEntryStrategy(EntryScript *script) : script_(script) {}
    void reset(double) override { begin_interval(); }
    void resume_from_restoration(double) override {
        ++script_->resumes;
        begin_interval();
    }
    StepVerdict judge(const StepContext &) override {
        const auto k = static_cast<std::size_t>(script_->resumes);
        const int accepts =
            k < script_->accepts.size() ? script_->accepts[k] : script_->accepts_after;
        const bool accept = script_->judged < accepts;
        if (accept && script_->judged < solved_in_interval_) {
            ++script_->accepted_solved;
        }
        ++script_->judged;
        return accept ? StepVerdict::kAcceptObjectiveStep : StepVerdict::kReject;
    }

  private:
    void begin_interval() {
        script_->judged = 0;
        const auto k = static_cast<std::size_t>(script_->resumes);
        solved_in_interval_ = k < script_->solved.size() ? script_->solved[k] : 0;
        Seam::pass_walk_qps = solved_in_interval_;
    }
    EntryScript *script_;
    int solved_in_interval_ = 0;
};

SqpResult scripted_entry_solve(EntryScript &script, const SqpOptions &base) {
    Seam::reset();
    Seam::fail_walk_qps = 100000;
    Seam::as_e2_termination = false;
    SqpOptions o = base;
    o.advanced.qp_failure_step = true;
    o.trust_region_initial = 0.25;
    o.common.max_iterations = 400;
    o.advanced.make_strategy = [&script]() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<ScriptedEntryStrategy>(&script);
    };
    return solve_line(vec2(0.0, 2.0), o);
}

void print_entries(const char *tag, const SqpResult &r, const EntryScript &script) {
    std::printf("[ rstb ] %s: status=%d reason=%d entries=%ld unproductive=%ld no_progress=%ld "
                "resumes=%d accepted=%ld failure_accepted=%ld solved_accepted=%d resto_facts=%ld "
                "facts=%ld\n",
                tag, static_cast<int>(r.status), static_cast<int>(r.stop_reason),
                static_cast<long>(r.counters.restoration_entries),
                static_cast<long>(r.counters.restoration_unproductive_entries),
                static_cast<long>(r.counters.restoration_no_progress_requests), script.resumes,
                static_cast<long>(r.counters.steps_accepted),
                static_cast<long>(r.counters.qp_failure_steps_accepted), script.accepted_solved,
                static_cast<long>(r.counters.restoration_factorizations),
                static_cast<long>(r.counters.factorizations));
}

} // namespace

// THE COUNTERS (C'): every re-entry follows an interval whose only accepted step is a failure
// step, so each later request is unproductive, and from the third on past a tolerance of one --
// counted; refused only by the entry cap under a tolerance too large to bind.
TEST(RstbEntrySeam, FailureStepOnlyCyclesAreCountedAsUnproductive) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0};
    SqpOptions o = quiet();
    o.advanced.max_unproductive_restoration_entries = 1000000;
    const SqpResult sol = scripted_entry_solve(script, o);
    print_entries("report", sol, script);
    const Index cap = o.advanced.max_restoration_entries;
    ASSERT_EQ(sol.counters.restoration_entries, cap);
    EXPECT_EQ(sol.counters.qp_failure_steps_accepted, cap) << "one per resumed interval";
    EXPECT_EQ(sol.counters.restoration_unproductive_entries, cap) << "requests 2 .. cap + 1";
    EXPECT_EQ(sol.counters.restoration_no_progress_requests, 0) << "never past 1000000";
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError) << "the cap's refusal";
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationRefused);
    EXPECT_FALSE(sol.infeasibility_certified);
    // Each phase starts feasible on the line model and converges before it factorizes.
    EXPECT_EQ(sol.counters.restoration_factorizations, 0);
    EXPECT_EQ(sol.counters.restoration_budget_cuts, 0);
    EXPECT_EQ(sol.counters.restoration_no_accept_refusals, 0) << "a failure step was kept";
    EXPECT_LE(sol.counters.restoration_refusal_feasibility, o.common.feasibility_tol);
}

// B1 (default 1): the same cycles are refused at the second unproductive re-entry -- the third
// request -- kStalled beside kRestorationNoProgress, with no claim about the model.
TEST(RstbEntrySeam, FailureStepOnlyCyclesAreRefusedAtTheSecondUnproductiveReEntry) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0};
    const SqpResult sol = scripted_entry_solve(script, quiet());
    print_entries("b1", sol, script);
    EXPECT_EQ(sol.counters.restoration_entries, 2);
    EXPECT_EQ(sol.counters.qp_failure_steps_accepted, 2);
    EXPECT_EQ(sol.counters.restoration_unproductive_entries, 2);
    EXPECT_EQ(sol.counters.restoration_no_progress_requests, 1);
    EXPECT_EQ(sol.counters.restoration_no_accept_refusals, 0)
        << "B1's refusal, not the entry rule's";
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationNoProgress);
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_EQ(sol.message, "stalled: a restoration request was refused because the main loop made "
                           "no progress since the previous resume");
    // The point the request was refused at, on the convergence test's measure (I9).
    EXPECT_LE(sol.counters.restoration_refusal_feasibility, quiet().common.feasibility_tol);
}

// ... and under a tier wide enough to admit the point the refusal is labelled kAcceptable.
TEST(RstbEntrySeam, TheRefusalAtAnAcceptablePointIsLabelled) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0};
    SqpOptions o = quiet();
    o.termination.acceptable_stationarity_tol = 1e10;
    o.termination.acceptable_equality_tol = 1e10;
    o.termination.acceptable_inequality_tol = 1e10;
    o.termination.acceptable_complementarity_tol = 1e10;
    const SqpResult sol = scripted_entry_solve(script, o);
    EXPECT_EQ(sol.counters.restoration_entries, 2);
    EXPECT_EQ(sol.status, SolveStatus::kAcceptable);
    EXPECT_EQ(sol.stop_reason, StopReason::kNone);
    EXPECT_NE(sol.message.find("exit restoration_no_progress"), std::string::npos) << sol.message;
    EXPECT_FALSE(sol.infeasibility_certified);
}

// A productive interval -- one accepted step from a solved subproblem -- clears the run: the
// intervals read unproductive, productive, unproductive, unproductive, so the refusal moves from
// the third request to the fifth.
TEST(RstbEntrySeam, AProductiveIntervalClearsTheRun) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0, 1, 1, 1, 1};
    script.solved = {0, 0, 1, 0, 0};
    const SqpResult sol = scripted_entry_solve(script, quiet());
    print_entries("reset", sol, script);
    EXPECT_EQ(script.accepted_solved, 1);
    EXPECT_EQ(sol.counters.restoration_entries, 4);
    EXPECT_EQ(sol.counters.restoration_unproductive_entries, 3);
    EXPECT_EQ(sol.counters.restoration_no_progress_requests, 1);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationNoProgress);
}

// The tolerance's two ends: 0 refuses the first unproductive re-entry (the first entry itself is
// never tested, so it is serviced), and a large value never binds (the entry cap ends the solve).
TEST(RstbEntrySeam, ToleranceZeroRefusesTheFirstReEntryAndALargeOneNone) {
    const SeamReset guard;
    EntryScript zero_script;
    zero_script.accepts = {0};
    SqpOptions zero = quiet();
    zero.advanced.max_unproductive_restoration_entries = 0;
    const SqpResult z = scripted_entry_solve(zero_script, zero);
    print_entries("zero", z, zero_script);
    EXPECT_EQ(z.counters.restoration_entries, 1) << "the first entry is exempt";
    EXPECT_EQ(z.stop_reason, StopReason::kRestorationNoProgress);
    EntryScript large_script;
    large_script.accepts = {0};
    SqpOptions large = quiet();
    large.advanced.max_unproductive_restoration_entries = 1000000;
    const SqpResult l = scripted_entry_solve(large_script, large);
    EXPECT_EQ(l.counters.restoration_entries, large.advanced.max_restoration_entries);
    EXPECT_EQ(l.stop_reason, StopReason::kRestorationRefused);
}

// THE ENTRY CAP OUTRANKS THE NO-PROGRESS LABEL. At a cap of one the second request is refused by
// the cap, by the entry rule (nothing accepted since the resume) and by the progress test (a
// tolerance of 0) at once: the cap's label wins (numerical_error / restoration_refused); the
// progress test still counts the request, and the entry rule's counter does not (the cap refused).
TEST(RstbEntrySeam, TheEntryCapOutranksTheNoProgressLabel) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0, 0};
    script.accepts_after = 0;
    SqpOptions o = quiet();
    o.advanced.max_restoration_entries = 1;
    o.advanced.max_unproductive_restoration_entries = 0;
    const SqpResult sol = scripted_entry_solve(script, o);
    print_entries("cap-outranks", sol, script);
    EXPECT_EQ(sol.counters.restoration_entries, 1);
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationRefused);
    EXPECT_EQ(sol.counters.restoration_no_progress_requests, 1);
    EXPECT_EQ(sol.counters.restoration_no_accept_refusals, 0);
    EXPECT_FALSE(sol.infeasibility_certified);
}

// B2: the cumulative budget, when it is the binding bound, cuts the phase and is labelled
// kRestorationWorkLimit (kStalled); the caller's own ceiling, when smaller, stays kWorkLimit.
TEST(RstbBudgetSeam, ACumulativeBudgetCutIsTheRestorationWorkLimit) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    Seam::forced_point = {1.0, 1.0};
    SpreadMissModel model(kNoClaimMiss);
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.trust_region_initial = 1.0;
    o.trust_region_min = 0.75;
    o.advanced.restoration_stall_factorizations = 1000000; // the window never fires
    o.advanced.restoration_resume_reduction = 0.0;
    o.advanced.restoration_max_total_factorizations = 2;
    const SqpResult sol =
        SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    std::printf("[ rstb ] b2 cut: status=%d reason=%d entries=%ld resto_facts=%ld\n",
                static_cast<int>(sol.status), static_cast<int>(sol.stop_reason),
                static_cast<long>(sol.counters.restoration_entries),
                static_cast<long>(sol.counters.restoration_factorizations));
    EXPECT_EQ(sol.counters.restoration_entries, 1);
    EXPECT_LE(sol.counters.restoration_factorizations, 2);
    EXPECT_EQ(sol.counters.restoration_budget_cuts, 1) << "the cut is observable";
    EXPECT_EQ(sol.status, SolveStatus::kStalled);
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationWorkLimit);
    EXPECT_FALSE(sol.infeasibility_certified);
}

// B2 AND THE REDUCTION RESUME: a budget cut is the phase's own stop, like the stall window's, so
// the sufficient-reduction resume reads it. The same bouncing spread phase (h_in = 10, its points
// at h ~ 1e-5), cut by a budget of 2 with the stall window off: with the reduction resume at 0.1
// the cut phase's best point resumes and is counted, and the solve goes on.
TEST(RstbBudgetSeam, ABudgetCutTakesTheReductionResume) {
    const auto solve = [](double k) {
        const SeamReset guard;
        Seam::fail_walk_qps = 1000;
        Seam::as_e2_termination = false;
        Seam::forced_point = {1.0, 1.0};
        SpreadMissModel model(kNoClaimMiss);
        SqpOptions o = quiet();
        o.common.max_iterations = 200;
        o.trust_region_initial = 1.0;
        o.trust_region_min = 0.75;
        o.advanced.restoration_stall_factorizations = 1000000; // the window never fires
        o.advanced.restoration_resume_reduction = k;
        o.advanced.restoration_max_total_factorizations = 2;
        return SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    };
    const SqpResult off = solve(0.0);
    const SqpResult on = solve(0.1);
    std::printf("[ rstb ] b2 reduction: off status=%d cuts=%ld | on status=%d reason=%d "
                "entries=%ld cuts=%ld reduction_resumes=%ld majors=%ld\n",
                static_cast<int>(off.status),
                static_cast<long>(off.counters.restoration_budget_cuts),
                static_cast<int>(on.status), static_cast<int>(on.stop_reason),
                static_cast<long>(on.counters.restoration_entries),
                static_cast<long>(on.counters.restoration_budget_cuts),
                static_cast<long>(on.counters.restoration_reduction_resumes),
                static_cast<long>(on.counters.major_iters));
    ASSERT_EQ(off.stop_reason, StopReason::kRestorationWorkLimit);
    ASSERT_EQ(off.counters.restoration_budget_cuts, 1);
    EXPECT_GE(on.counters.restoration_budget_cuts, 1);
    EXPECT_GE(on.counters.restoration_reduction_resumes, 1) << "the cut phase's point resumed";
    EXPECT_LE(on.counters.restoration_h_out, 0.1 * on.counters.restoration_h_in);
    EXPECT_GT(on.counters.major_iters, off.counters.major_iters) << "the solve went on";
    EXPECT_FALSE(on.infeasibility_certified);
}

// B2 AT A FEASIBLE ENTRY POINT (declared behaviour): the budget never refuses a request. A phase
// entered at a point the convergence test already calls feasible converges at its start without
// factorizing, so it resumes -- and resets the radius -- whatever is left of the budget, even
// none. Such re-entries are bounded by the entry cap and the progress test only. The line model
// under failure-step-only cycles (every phase starts feasible): with a budget of 0 and the progress
// test off the entry cap ends the solve after `max_restoration_entries` resumed entries, no cut is
// counted and no restoration factorization spent; at the default tolerance the progress test
// refuses at the third request, exactly as without a budget.
TEST(RstbBudgetSeam, AZeroBudgetDoesNotRefuseAFeasibleEntryAndTheCapAndProgressTestBoundIt) {
    const SeamReset guard;
    EntryScript cap_script;
    cap_script.accepts = {0};
    SqpOptions o = quiet();
    o.advanced.max_unproductive_restoration_entries = 1000000;
    o.advanced.restoration_max_total_factorizations = 0;
    const SqpResult capped = scripted_entry_solve(cap_script, o);
    print_entries("b2-zero-cap", capped, cap_script);
    EXPECT_EQ(capped.counters.restoration_entries, o.advanced.max_restoration_entries);
    EXPECT_EQ(cap_script.resumes, o.advanced.max_restoration_entries) << "every entry resumed";
    EXPECT_EQ(capped.counters.restoration_budget_cuts, 0);
    EXPECT_EQ(capped.counters.restoration_factorizations, 0);
    EXPECT_EQ(capped.stop_reason, StopReason::kRestorationRefused) << "the cap's refusal";
    EntryScript b1_script;
    b1_script.accepts = {0};
    SqpOptions d = quiet();
    d.advanced.restoration_max_total_factorizations = 0;
    const SqpResult b1 = scripted_entry_solve(b1_script, d);
    EXPECT_EQ(b1.counters.restoration_entries, 2);
    EXPECT_EQ(b1.counters.restoration_budget_cuts, 0);
    EXPECT_EQ(b1.stop_reason, StopReason::kRestorationNoProgress);
}

TEST(RstbBudgetSeam, TheCallersSmallerCeilingStaysTheWorkLimit) {
    const SeamReset guard;
    CountingCircleLineModel model;
    SqpOptions o = quiet();
    o.common.max_iterations = 200;
    o.advanced.restoration_stall_factorizations = 100000;
    const SqpResult free_run =
        SqpSolver(o).solve(hven::detail::problem_ref(model), (model).start_point());
    ASSERT_EQ(free_run.counters.restoration_entries, 1);
    ASSERT_GT(free_run.counters.restoration_factorizations, 1);
    // A budget larger than what the phase needs and a caller ceiling one short of the free run:
    // the caller's ceiling is the smaller bound inside the phase.
    CountingCircleLineModel model2;
    o.common.max_factorizations = free_run.counters.factorizations - 1;
    o.advanced.restoration_max_total_factorizations = free_run.counters.factorizations;
    const SqpResult sol =
        SqpSolver(o).solve(hven::detail::problem_ref(model2), (model2).start_point());
    EXPECT_EQ(sol.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(sol.stop_reason, StopReason::kNone);
}

// B1: an accepted step from an optimal elastic rung is progress. Interval 1 accepts a failure step
// to an infeasible point (the seam's forced step), then the elastic tier's step there: the main QP
// at h > feas_tol fails through an E2 termination and the elastic ladder answers. At a tolerance of
// 0 the next request is tested and found productive, so it is serviced; interval 2 accepts nothing.
TEST(RstbEntrySeam, AnElasticRungAcceptIsProgress) {
    const SeamReset guard;
    EntryScript script;
    script.accepts = {0, 2};
    script.accepts_after = 0;
    SqpOptions o = quiet();
    o.advanced.max_unproductive_restoration_entries = 0;
    Seam::reset();
    Seam::fail_walk_qps = 100000;
    Seam::as_e2_termination = true;
    Seam::forced_point = {0.6, 0.6};
    o.advanced.qp_failure_step = true;
    o.trust_region_initial = 0.25;
    o.common.max_iterations = 400;
    o.advanced.make_strategy = [&script]() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<ScriptedEntryStrategy>(&script);
    };
    const SqpResult sol = solve_line(vec2(0.0, 2.0), o);
    print_entries("elastic", sol, script);
    std::printf("[ rstb ] elastic: activations=%ld\n",
                static_cast<long>(sol.counters.elastic_activations));
    ASSERT_GE(sol.counters.elastic_activations, 1);
    ASSERT_EQ(sol.counters.qp_failure_steps_accepted, 1);
    ASSERT_EQ(sol.counters.steps_accepted, 2) << "the failure step and the elastic step";
    EXPECT_EQ(sol.counters.restoration_entries, 2) << "the second request is productive";
    EXPECT_EQ(sol.counters.restoration_unproductive_entries, 1) << "the third is not";
    EXPECT_EQ(sol.stop_reason, StopReason::kRestorationNoProgress);
    std::printf("[ rstb ] elastic: refusal_feasibility=%.3e\n",
                sol.counters.restoration_refusal_feasibility);
    EXPECT_TRUE(std::isfinite(sol.counters.restoration_refusal_feasibility));
}

} // namespace hven
