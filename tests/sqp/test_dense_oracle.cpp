#include <gtest/gtest.h>

#include "support/dense_oracle.h"

using namespace tycho::sqp;

static QpProblem simple_box_qp() {
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

TEST(DenseOracle, KnownSolution) {
    // Unconstrained min is (1,2); constraint x0+x1<=1 is active.
    // KKT: x = (0, 1), lambda_i = 1, x0 at lower bound with z0 = 0.
    auto sol = solve_dense_oracle(simple_box_qp());
    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 0.0, 1e-12);
    EXPECT_NEAR(sol.x(1), 1.0, 1e-12);
    EXPECT_TRUE(sol.ineq_active[0]);
    EXPECT_EQ(sol.bound_state[0], BoundState::kAtLower);
}

TEST(DenseOracle, EqualityOnly) {
    // min 1/2||x||^2 s.t. x0 + x1 = 2  ->  x = (1,1), lambda_e = -1 (sign: grad(f) + Ae^T lambda =
    // 0)
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
    auto sol = solve_dense_oracle(qp);
    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-12);
    EXPECT_NEAR(sol.x(1), 1.0, 1e-12);
    EXPECT_NEAR(sol.lambda_e(0), -1.0, 1e-12);
}

TEST(DenseOracle, EqualityPlusActiveInequality) {
    // Regression for the me-offset bug in the dual-feasibility sign check:
    // min 1/2(x0^2 + x1^2) s.t. x0 + x1 = 3 (equality), x0 <= 1 (inequality).
    // Hand KKT: x = (1, 2); stationarity grad(f) + Ae^T le + Ai^T li = 0 gives
    // component 1: 2 + le = 0 -> le = -2; component 0: 1 + le + li = 0 ->
    // li = 1 >= 0. The buggy check read le's sign where li's belongs and
    // rejected this (unique) optimum, making the oracle throw.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 3.0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    auto sol = solve_dense_oracle(qp);
    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-12);
    EXPECT_NEAR(sol.x(1), 2.0, 1e-12);
    EXPECT_NEAR(sol.lambda_e(0), -2.0, 1e-12);
    EXPECT_NEAR(sol.lambda_i(0), 1.0, 1e-12);
    EXPECT_TRUE(sol.ineq_active[0]);
}

TEST(DenseOracle, EqualityPlusActiveBound) {
    // min 1/2||x||^2 s.t. x0 + x1 = 2 (equality), x1 <= 0.5 (upper bound).
    // Equality-only optimum (1,1) violates the bound, so x1 pins at 0.5 and
    // x0 = 1.5. Hand KKT with grad(f) + Ae^T le - z = 0:
    //   component 0: 1.5 + le = 0        -> le = -1.5
    //   component 1: 0.5 + le - z1 = 0   -> z1 = -1.0  (z <= 0 at upper: ok)
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
    qp.upper = Vec(2);
    qp.upper << 1e20, 0.5;
    auto sol = solve_dense_oracle(qp);
    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.5, 1e-12);
    EXPECT_NEAR(sol.x(1), 0.5, 1e-12);
    EXPECT_NEAR(sol.lambda_e(0), -1.5, 1e-12);
    EXPECT_NEAR(sol.z(1), -1.0, 1e-12);
    EXPECT_EQ(sol.bound_state[1], BoundState::kAtUpper);
}

TEST(DenseOracle, InfeasibleThrows) {
    // x0 <= -1 and -x0 <= -1 (i.e. x0 >= 1) cannot both hold.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(1, 1).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(1);
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 1);
    Aid << 1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, -1.0);
    qp.lower = Vec::Constant(1, -1e20);
    qp.upper = Vec::Constant(1, 1e20);
    EXPECT_THROW(solve_dense_oracle(qp), std::runtime_error);
}

TEST(DenseOracle, CombinatoricsGuardThrows) {
    // mi = 21 exceeds the 2^20 enumeration guard.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(1, 1).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(1);
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Ones(21, 1);
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Zero(21);
    qp.lower = Vec::Constant(1, -1e20);
    qp.upper = Vec::Constant(1, 1e20);
    EXPECT_THROW(solve_dense_oracle(qp), std::invalid_argument);
}

TEST(QpProblem, ValidateRejectsLowerTriangleHEntries) {
    // F6: every consumer of qp.H (kkt_assembly.h's Hessian block,
    // qp_engine.h's price()) reads it via selfadjointView<Upper>() /
    // symmetrizes off of the "H stores only row <= col" convention alone. A
    // caller who instead populates a lower-triangle entry gets a DIFFERENT
    // matrix interpreted silently -- the dense oracle wouldn't even catch it,
    // since it symmetrizes explicitly and so agrees with the wrong answer.
    // validate() must reject this directly.
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1.0, 0.0,         // row 0
        0.5, 1.0;           // row 1: Hd(1,0) = 0.5 is a LOWER-triangle entry
    qp.H = Hd.sparseView(); // stores (1,0), unlike every fixture elsewhere in
                            // this suite, which goes through
                            // .triangularView<Eigen::Upper>() first.
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    EXPECT_THROW(qp.validate(), std::invalid_argument);
}
