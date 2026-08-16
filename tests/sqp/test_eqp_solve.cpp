#include <gtest/gtest.h>

#include <hven/detail/qp/eqp_solve.h>

#include "support/dense_oracle.h"

using namespace hven::solvers;
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

    detail::KktFactor kkt;
    auto eqp = solve_eqp(qp, ws, kkt, QpOptions{});

    EXPECT_LT((eqp.x - oracle.x).norm(), 1e-10);
}

TEST(EqpSolve, MatchesOracleOnEqualityOnlyProblem) {
    QpProblem qp = equality_only_qp();
    auto oracle = solve_dense_oracle(qp);
    WorkingSet ws = working_set_from_oracle(qp, oracle);

    detail::KktFactor kkt;
    auto eqp = solve_eqp(qp, ws, kkt, QpOptions{});

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

    detail::KktFactor kkt;
    EqpResult unrefined;
    auto refined = solve_eqp(qp, ws, kkt, QpOptions{}, &unrefined);

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
    // engine's eqp_refine_steps aggregate makes at solve level (types.h).
    EXPECT_EQ(refined.refine_steps, 0);
}
