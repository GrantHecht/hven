// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <cstdio>

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/bordered_eqp.h>
#include <hven/detail/qp/walk/eqp_solve.h>

#include "qp/support/dense_oracle.h"

using namespace hven;
using namespace hven::detail;
using hven::Index;
using hven::Vec;

namespace {

// Repeated verbatim from test_dense_oracle.cpp's simple_box_qp() (Task 5) --
// this file deliberately does not include test_dense_oracle.cpp.
QpProblem simple_box_qp() {
    // min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

// Repeated verbatim (modulo local naming) from test_dense_oracle.cpp's
// EqualityOnly fixture.
QpProblem equality_only_qp() {
    // min 1/2||x||^2 s.t. x0 + x1 = 2  ->  x = (1,1), lambda_e = -1 (sign:
    // grad(f) + Ae^T lambda = 0)
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 2.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// Builds the WorkingSet the oracle found optimal for `qp`.
WorkingSet working_set_from_oracle(const QpProblem &qp, const QpSolution &oracle) {
    WorkingSet ws(qp.n(), qp.mi());
    for (Index i = 0; i < qp.n(); ++i) {
        ws.bound_state()[static_cast<std::size_t>(i)] =
            oracle.bound_state[static_cast<std::size_t>(i)];
    }
    for (Index r = 0; r < qp.mi(); ++r) {
        if (oracle.ineq_active[static_cast<std::size_t>(r)]) {
            ws.add_ineq(r);
        }
    }
    return ws;
}

} // namespace

TEST(EqpSolve, MatchesOracleOnItsOptimalWorkingSet) {
    QpProblem qp = simple_box_qp();
    auto oracle = solve_dense_oracle(qp);
    WorkingSet ws = working_set_from_oracle(qp, oracle);

    detail::KktFactor kkt(0, hven::linear::DensePolicy::kNever);
    // THE PROXIMAL CENTRE IS THE POINT UNDER TEST (M7 W2 E4; eqp_solve.h). The
    // proximal EQP's fixed point is the face's EXACT KKT point -- p = 0 makes its
    // stationarity row the unregularized one, for any delta -- so centring at the
    // oracle's own answer is what makes "matches the oracle" a statement about the
    // assembly, the sign convention and the refinement rather than about the
    // model's O(delta*||x* - x_k||) proximal bias, which is a property of the
    // model and not of this solve.
    auto eqp = solve_eqp(qp, ws, oracle.x, kkt, QpOptions{});

    EXPECT_LT((eqp.x - oracle.x).norm(), 1e-10);
}

TEST(EqpSolve, MatchesOracleOnEqualityOnlyProblem) {
    QpProblem qp = equality_only_qp();
    auto oracle = solve_dense_oracle(qp);
    WorkingSet ws = working_set_from_oracle(qp, oracle);

    detail::KktFactor kkt(0, hven::linear::DensePolicy::kNever);
    // THE PROXIMAL CENTRE IS THE POINT UNDER TEST (M7 W2 E4; eqp_solve.h). The
    // proximal EQP's fixed point is the face's EXACT KKT point -- p = 0 makes its
    // stationarity row the unregularized one, for any delta -- so centring at the
    // oracle's own answer is what makes "matches the oracle" a statement about the
    // assembly, the sign convention and the refinement rather than about the
    // model's O(delta*||x* - x_k||) proximal bias, which is a property of the
    // model and not of this solve.
    auto eqp = solve_eqp(qp, ws, oracle.x, kkt, QpOptions{});

    EXPECT_LT((eqp.x - oracle.x).norm(), 1e-10);
    ASSERT_EQ(eqp.lambda_e.size(), 1);
    EXPECT_NEAR(eqp.lambda_e(0), oracle.lambda_e(0), 1e-10);
}

TEST(EqpSolve, FixedVariableWithEqualityRefinementImprovesAccuracy) {
    // min 1/2||x||^2 s.t. 0.05*x0 + 0.05*x1 = 0.1 (i.e. x0 + x1 = 2, scaled
    // by 0.05 so its multiplier is amplified by 1/0.05 -- this makes the
    // O(mu*lambda_e) regularization error big enough to clear 1e-7 before
    // refinement, while refinement still drives it back down), x0 >= 1.5
    // (lower bound active). Equality-only optimum (1,1) violates the bound,
    // so x0 pins at 1.5 and x1 = 0.5.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 0.05, 0.05;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.1);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.lower(0) = 1.5;
    qp.upper = Vec::Constant(2, 1e20);

    auto oracle = solve_dense_oracle(qp);
    ASSERT_EQ(oracle.status, QpStatus::kOptimal);
    ASSERT_EQ(oracle.bound_state[0], BoundState::kAtLower);

    WorkingSet ws = working_set_from_oracle(qp, oracle);

    detail::KktFactor kkt(0, hven::linear::DensePolicy::kNever);
    EqpResult unrefined;
    // Centred at the point under test, as above: what this fixture is about is the
    // refinement step removing the O(mu*|lambda_e|) DUAL error, which the centring
    // does not touch (it is still 1e-7 here before the step).
    auto refined = solve_eqp(qp, ws, oracle.x, kkt, QpOptions{}, &unrefined);

    const double unrefined_err = (unrefined.x - oracle.x).norm();
    const double refined_err = (refined.x - oracle.x).norm();
    EXPECT_GT(unrefined_err, 1e-7);
    EXPECT_LT(refined_err, 1e-7);

    // Tolerances here are 1e-9 rather than 1e-10: the equality row is scaled
    // by 0.05 specifically to amplify lambda_e (and its regularization
    // error) by 1/0.05, so the refined residual is correspondingly larger
    // than in the other (unscaled) tests -- still comfortably below the
    // unrefined_err > 1e-7 bound checked above.
    EXPECT_NEAR(refined.x(0), oracle.x(0), 1e-9);
    EXPECT_NEAR(refined.x(1), oracle.x(1), 1e-9);
    ASSERT_EQ(refined.lambda_e.size(), 1);
    EXPECT_NEAR(refined.lambda_e(0), oracle.lambda_e(0), 1e-9);

    // solve_eqp takes its ONE mandatory step and has no iterated loop, so the
    // EXTRA-step counter is identically zero -- the same statement the QP
    // engine's eqp_refine_steps aggregate makes at solve level (solver_counters.h).
    EXPECT_EQ(refined.refine_steps, 0);
}

// ---- THE CONVERGED REFINEMENT (M8 QPC; eqp_solve.h's header section) ----------------------------
//
// A near-dependent CONSISTENT face: rows a1 = (1, 0), a2 = (1, eps), b = (1, 1 + eps/2), so the
// face is the single point x = (1, 0.5); H = I, g = 0, centred at x_k = 0. The multipliers are
// lambda = (1/eps - 1, -1/(2 eps)), |lambda| ~ 5e3 at eps = 1e-4, and the Schur complement's small
// eigenvalue is ~ eps^2 / 2 = 5e-9 < mu = 1e-8: one mu-free step leaves the rows off by a fraction
// mu / (mu + s) ~ 0.67 of what the regularized solve left, and each further step contracts by the
// same factor. The model step leaves the rows ~1e-5 off; converged, ten steps in all.
namespace {

QpProblem near_dependent_face(double eps) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 0.0, 1.0, eps;
    qp.Ae = A.sparseView();
    qp.be = Vec(2);
    qp.be << 1.0, 1.0 + 0.5 * eps;
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

double row_miss(const QpProblem &qp, const Vec &x) {
    return (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>();
}

} // namespace

TEST(EqpConvergedRefinement, EliminatedPathConvergesWhereTheModelStepLeavesTheFootprint) {
    const QpProblem qp = near_dependent_face(1e-4);
    const WorkingSet ws(2, 0);
    const Vec x_k = Vec::Zero(2);
    detail::KktFactor k1(0, hven::linear::DensePolicy::kNever);
    const EqpResult model = solve_eqp(qp, ws, x_k, k1, QpOptions{}, nullptr, EqpRefinement::kModel);
    detail::KktFactor k2(0, hven::linear::DensePolicy::kNever);
    const EqpResult conv =
        solve_eqp(qp, ws, x_k, k2, QpOptions{}, nullptr, EqpRefinement::kConverged);
    const double m = row_miss(qp, model.x);
    const double c = row_miss(qp, conv.x);
    std::printf("[ qpc-conv ] eliminated: model miss %.3e (steps %ld), converged miss %.3e (steps "
                "%ld)\n",
                m, static_cast<long>(model.refine_steps), c, static_cast<long>(conv.refine_steps));
    EXPECT_EQ(model.refine_steps, 0); // no trust region: the accuracy stop does not fire
    EXPECT_GE(conv.refine_steps, 3);
    EXPECT_LE(conv.refine_steps, detail::kMaxBorderRefineSteps - 1);
    EXPECT_GT(m, 1e-7);
    EXPECT_LT(c, 0.1 * m);
    // The same answer the walk would price toward: closer to the exact multipliers.
    EXPECT_LT(std::abs(conv.lambda_e(1) - (-0.5 / 1e-4)),
              std::abs(model.lambda_e(1) - (-0.5 / 1e-4)));
}

// A face the model step already resolves: converging changes nothing material (the loop stops at
// the floor or on the first non-decreasing step), and at dual_mu = 0 the mode is inert bit for bit.
TEST(EqpConvergedRefinement, AWellConditionedFaceAndTheUnregularizedSystemAreUnmoved) {
    const QpProblem qp = equality_only_qp();
    const WorkingSet ws(2, 0);
    const Vec x_k = Vec::Zero(2);
    detail::KktFactor k1(0, hven::linear::DensePolicy::kNever);
    const EqpResult model = solve_eqp(qp, ws, x_k, k1, QpOptions{}, nullptr, EqpRefinement::kModel);
    detail::KktFactor k2(0, hven::linear::DensePolicy::kNever);
    const EqpResult conv =
        solve_eqp(qp, ws, x_k, k2, QpOptions{}, nullptr, EqpRefinement::kConverged);
    EXPECT_LT((conv.x - model.x).lpNorm<Eigen::Infinity>(), 1e-12);
    QpOptions no_mu;
    no_mu.dual_regularization = 0.0;
    const QpProblem nd = near_dependent_face(1e-4);
    detail::KktFactor k3(0, hven::linear::DensePolicy::kNever);
    const EqpResult a = solve_eqp(nd, ws, x_k, k3, no_mu, nullptr, EqpRefinement::kModel);
    detail::KktFactor k4(0, hven::linear::DensePolicy::kNever);
    const EqpResult b = solve_eqp(nd, ws, x_k, k4, no_mu, nullptr, EqpRefinement::kConverged);
    EXPECT_EQ(a.x, b.x);
    EXPECT_EQ(a.lambda_e, b.lambda_e);
    EXPECT_EQ(a.refine_steps, b.refine_steps);
}

// The bordered refinement with an EMPTY border stack is solve_eqp's arithmetic: under kConverged it
// converges too (the unbordered path does), where the model rule takes its one step.
TEST(EqpConvergedRefinement, UnborderedBorderedPathConvergesLikeTheEliminatedOne) {
    const QpProblem qp = near_dependent_face(1e-4);
    const WorkingSet ws(2, 0);
    const QpOptions opts;
    const KktAssembly asm_ = assemble_kkt(qp, ws, opts);
    detail::KktFactor k(0, hven::linear::DensePolicy::kNever);
    (void)detail::factorize_checked(k, asm_.K, detail::DenseShiftSite{2});
    SchurComplement schur(k, opts);
    ASSERT_EQ(schur.dim(), 0);
    Vec rhs(4);
    rhs << 0.0, 0.0, qp.be(0), qp.be(1); // g = 0, x_k = 0: no proximal term
    const Vec sol = schur.solve(rhs);
    const std::vector<Vec> no_v;
    const std::vector<double> no_d;
    Index s_model = 0;
    Index s_conv = 0;
    const Vec ym =
        refine_bordered_solve_iterated(asm_.K, 2, no_v, no_d, schur, rhs, sol, opts, &s_model,
                                       nullptr, nullptr, false, EqpRefinement::kModel);
    const Vec yc =
        refine_bordered_solve_iterated(asm_.K, 2, no_v, no_d, schur, rhs, sol, opts, &s_conv,
                                       nullptr, nullptr, false, EqpRefinement::kConverged);
    const double m = row_miss(qp, ym.head(2));
    const double c = row_miss(qp, yc.head(2));
    std::printf("[ qpc-conv ] unbordered: model miss %.3e (steps %ld), converged miss %.3e (steps "
                "%ld)\n",
                m, static_cast<long>(s_model), c, static_cast<long>(s_conv));
    EXPECT_EQ(s_model, 1);
    EXPECT_GE(s_conv, 4);
    EXPECT_LT(c, 0.1 * m);
    // and it agrees with the eliminated path's converged answer
    detail::KktFactor k2(0, hven::linear::DensePolicy::kNever);
    const EqpResult e =
        solve_eqp(qp, ws, Vec::Zero(2), k2, opts, nullptr, EqpRefinement::kConverged);
    EXPECT_LT((e.x - yc.head(2)).lpNorm<Eigen::Infinity>(), 1e-6);
}
