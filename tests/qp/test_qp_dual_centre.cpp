// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_dual_centre.cpp -- M8 P-1: THE DUAL PROXIMAL CENTRE of the walk's EQP (eqp_solve.h).
// A face whose two equality rows are nearly dependent (sigma_min^2 = eps^2/4 = 2.5e-9, below
// dual_mu = 1e-8): centred at 0 the regularized answer's multipliers are a fraction of the exact
// ones; centred at the exact multipliers the answer IS the exact point; fed back, the centre
// contracts toward it (the proximal method of multipliers).

#include <cmath>
#include <stdexcept>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/qp/walk/eqp_solve.h>

using namespace hven;
using namespace hven::detail;

namespace {

constexpr double kEps = 1e-4;

// min 1/2||x||^2 + g'x  s.t.  x0 + x1 = 1,  x0 + (1 + eps) x1 = 1 + eps/4,  x2 <= 1 (working).
QpProblem near_dependent_face() {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Identity(3, 3).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 1.0, -2.0, -0.5;
    Eigen::MatrixXd ae(2, 3);
    ae << 1.0, 1.0, 0.0, 1.0, 1.0 + kEps, 0.0;
    qp.Ae = ae.sparseView();
    qp.be = Vec(2);
    qp.be << 1.0, 1.0 + 0.25 * kEps;
    Eigen::MatrixXd ai(1, 3);
    ai << 0.0, 0.0, 1.0;
    qp.Ai = ai.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    return qp;
}

WorkingSet face_ws() {
    WorkingSet ws(3, 1);
    ws.add_ineq(0);
    return ws;
}

// The face's exact KKT point [x; lambda_e; lambda_w], unregularized, dense.
Vec exact_point(const QpProblem &qp) {
    Eigen::MatrixXd k = Eigen::MatrixXd::Zero(6, 6);
    k.topLeftCorner(3, 3) = Eigen::MatrixXd::Identity(3, 3);
    Eigen::MatrixXd a(3, 3);
    a.topRows(2) = Eigen::MatrixXd(qp.Ae);
    a.row(2) = Eigen::MatrixXd(qp.Ai).row(0);
    k.topRightCorner(3, 3) = a.transpose();
    k.bottomLeftCorner(3, 3) = a;
    Vec rhs(6);
    rhs << -qp.g, qp.be, qp.bi;
    return k.fullPivLu().solve(rhs);
}

double multiplier_error(const EqpResult &r, const Vec &ex) {
    Vec l(3);
    l << r.lambda_e, r.lambda_w;
    return (l - ex.tail(3)).lpNorm<Eigen::Infinity>() / ex.tail(3).lpNorm<Eigen::Infinity>();
}

} // namespace

TEST(QpDualCentre, ZeroCentreIsBiasedExactCentreIsTheExactPoint) {
    const QpProblem qp = near_dependent_face();
    const WorkingSet ws = face_ws();
    const Vec ex = exact_point(qp);
    const Vec x_star = ex.head(3);
    const QpOptions o; // dual_mu = primal_delta = 1e-8
    ASSERT_DOUBLE_EQ(o.dual_regularization, 1e-8);

    KktFactor k0(0, linear::DensePolicy::kNever);
    const EqpResult zero = solve_eqp(qp, ws, x_star, EqpDualCentre{}, k0, o);
    // Two solves' worth of contraction (mu/(mu + sigma^2))^2 = 0.64 of the exact multipliers left.
    EXPECT_GT(multiplier_error(zero, ex), 0.3);

    const Vec le = ex.segment(3, 2);
    const Vec li = Vec::Constant(1, ex(5));
    KktFactor k1(0, linear::DensePolicy::kNever);
    const EqpResult centred = solve_eqp(qp, ws, x_star, EqpDualCentre{&le, &li, nullptr}, k1, o);
    EXPECT_LT(multiplier_error(centred, ex), 1e-8);
    EXPECT_LT((centred.x - x_star).lpNorm<Eigen::Infinity>(), 1e-10);
    EXPECT_FALSE(centred.residual_refused);
    // Only the rhs moved: the same factorization, the same inertia.
    EXPECT_EQ(k0.factor.inertia().positive, k1.factor.inertia().positive);
    EXPECT_EQ(k0.factor.inertia().negative, k1.factor.inertia().negative);
}

TEST(QpDualCentre, FedBackTheCentreContractsTowardTheExactMultipliers) {
    const QpProblem qp = near_dependent_face();
    const WorkingSet ws = face_ws();
    const Vec ex = exact_point(qp);
    const Vec x_star = ex.head(3);
    const QpOptions o;
    Vec le = Vec::Zero(2);
    Vec li = Vec::Zero(1);
    double prev = 1.0;
    for (int it = 0; it < 6; ++it) {
        KktFactor k(0, linear::DensePolicy::kNever);
        const EqpResult r = solve_eqp(qp, ws, x_star, EqpDualCentre{&le, &li, nullptr}, k, o);
        const double e = multiplier_error(r, ex);
        EXPECT_LT(e, 0.7 * prev) << it;
        prev = e;
        le = r.lambda_e;
        li = r.lambda_w;
    }
    EXPECT_LT(prev, 0.1);
}

TEST(QpDualCentre, AMissizedCentreIsRefused) {
    const QpProblem qp = near_dependent_face();
    const WorkingSet ws = face_ws();
    const Vec bad = Vec::Zero(5);
    KktFactor k(0, linear::DensePolicy::kNever);
    EXPECT_THROW((void)solve_eqp(qp, ws, Vec::Zero(3), EqpDualCentre{&bad, nullptr, nullptr}, k,
                                 QpOptions{}),
                 std::invalid_argument);
    EXPECT_THROW((void)solve_eqp(qp, ws, Vec::Zero(3), EqpDualCentre{nullptr, &bad, nullptr}, k,
                                 QpOptions{}),
                 std::invalid_argument);
}
