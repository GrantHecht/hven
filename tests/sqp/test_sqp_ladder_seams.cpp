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
//   CERT-1 (M7 W2 C12) a restoration that converges where every row is inside
//       feas_tol but their l1 sum is not certifies nothing (BRITGAS under W1).

#include <cmath>
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

// CERT-1 (M7 W2 C12): K rows that each miss feasibility by kMiss < feas_tol,
//
//     min x1^2  s.t.  cE_k(x) = x0^2 + kMiss = 0 (k < K),  -3 <= x <= 3,
//
// so the l1 violation is at least K*kMiss = 3e-6 > feas_tol everywhere while the
// inf-norm reading kMiss passes feas_tol at x0 = 0 -- and there the rows'
// gradients vanish, BRITGAS's shape under the W1 guard (flows at their bounds,
// the pipe rows' Jacobian zero, l1 4.49e-6, inf 4.23e-7, certified).
class SpreadMissModel : public NlpModel {
  public:
    static constexpr Index kRows = 10;
    static constexpr double kMiss = 3e-7;
    Index n() const override { return 2; }
    Index me() const override { return kRows; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.0, 2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(kRows, x(0) * x(0) + kMiss); }
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

// Every walk QP of the top-level driver fails (a plain kNumericalError, the shrink
// ladder), so the radius floor sends the solve to restoration, whose own walk is
// untouched; the feasibility problem converges toward x0 = 0 and stops there.
TEST(SqpLadderSeam, CertOneAPointTheConvergenceTestCallsFeasibleIsNoCertificate) {
    const SeamReset guard;
    Seam::fail_walk_qps = 1000;
    Seam::as_e2_termination = false;
    SpreadMissModel model;
    SqpOptions o = quiet();
    o.max_iter = 200;
    o.qp_failure_budget = 1000; // the shrink ladder runs to the floor
    SqpSolver driver(o);
    const SqpResult sol = driver.solve(model);

    // THE PREMISE: one restoration, its sub-solve converged, not resumed (l1), at a
    // point whose rows are each inside feas_tol (inf).
    ASSERT_EQ(sol.counters.restoration_entries, 1);
    ASSERT_EQ(sol.counters.restoration_sub_status, RestorationSubStatus::kOptimal);
    ASSERT_GT(sol.counters.restoration_h_out, o.feas_tol);
    const Vec ce = model.eval_ce(sol.x);
    ASSERT_LE(ce.lpNorm<Eigen::Infinity>(), o.feas_tol);
    ASSERT_GT(ce.lpNorm<1>(), o.feas_tol);

    // THE CLAIM IS WITHHELD: no certificate and no kInfeasible at a point this
    // solve's own convergence test calls feasible.
    EXPECT_FALSE(sol.infeasibility_certified);
    EXPECT_EQ(sol.status, SolveStatus::kNumericalError);
}

} // namespace hven::solvers
