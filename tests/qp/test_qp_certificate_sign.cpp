// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_certificate_sign.cpp -- M7 W2 SDL, RDV-5: THE CERTIFICATE'S SIGN HALF (qp_engine.h,
// section 5). The walk's one certified exit read the iterate against its box (the bound half)
// and the rows, but not the SIGNS of the multipliers it exports: those were judged by the drop
// rule alone, before the terminal polish replaced them and with re-entered (exempt) members
// excused. A rejected prototype that let the walk past a saddle-exit miss reached kOptimal on
// SAWPATH with a lower pin priced z = -1.25e12. The fixtures present such a pair through the
// certificate seam (qp_certificate_seam.h), on the production statements that follow it.
// Before the sign half each presented pair below came back kOptimal with the presented z.

#include <cmath>
#include <limits>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;

namespace {

using Cert = hven::solvers::detail::testing::QpCertificateSeam;
using Obs = hven::solvers::detail::testing::QpWalkObserver;

struct SignSeamGuard {
    SignSeamGuard() {
        Obs::reset();
        Cert::reset();
    }
    ~SignSeamGuard() {
        Cert::reset();
        Obs::reset();
    }
};

/// Strictly convex, optimum x = (0, 0, 0.5): x0 at its LOWER bound (z0 = +5), x1 at its UPPER
/// bound (z1 = -5), x2 free. The stationarity scale at the optimum is max(1, |Hx|, |g|) = 5.
QpProblem sign_box_qp() {
    QpProblem qp;
    const Index n = 3;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g = Vec(n);
    qp.g << 5.0, -5.0, -0.5;
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec(n);
    qp.lower << 0.0, -1.0, -1.0;
    qp.upper = Vec(n);
    qp.upper << 1.0, 0.0, 1.0;
    return qp;
}

constexpr double kScale = 5.0;

QpOptions sign_opts(WorkingSetLinearAlgebra alg) {
    QpOptions o;
    o.ws_algebra = alg;
    return o;
}

QpSolution present(const QpProblem &qp, const QpOptions &opts, double z0, double z1) {
    Cert::reset();
    Cert::x = Vec(3);
    Cert::x << 0.0, 0.0, 0.5;
    Cert::z = Vec(3);
    Cert::z << z0, z1, 0.0;
    Cert::armed = true;
    QpEngine engine{opts};
    QpSolution s = engine.solve(qp);
    Cert::armed = false;
    return s;
}

void expect_refused(const QpSolution &s) {
    ASSERT_EQ(Cert::presentations, 1) << "the certificate never read the presented pair";
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
    EXPECT_EQ(s.counters.certificate_sign_refusals, 1);
    EXPECT_EQ(s.z.lpNorm<Eigen::Infinity>(), 0.0);
}

void expect_certified(const QpSolution &s, double z0, double z1) {
    ASSERT_EQ(Cert::presentations, 1) << "the certificate never read the presented pair";
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
    EXPECT_EQ(s.z(0), z0);
    EXPECT_EQ(s.z(1), z1);
}

constexpr WorkingSetLinearAlgebra kAlgebras[] = {WorkingSetLinearAlgebra::kSchurBorder,
                                                 WorkingSetLinearAlgebra::kRefactorize};

} // namespace

TEST(QpCertificateSign, SeamUnarmedCertifiesTheOptimumWithNoRefusal) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        QpEngine engine{sign_opts(alg)};
        const QpSolution s = engine.solve(sign_box_qp());
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(Cert::presentations, 0);
        EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
        EXPECT_NEAR(s.z(0), 5.0, 1e-6);
        EXPECT_NEAR(s.z(1), -5.0, 1e-6);
    }
}

// RDV-5's shape: a lower pin priced -1.25e12 on an otherwise exact optimum.
TEST(QpCertificateSign, Rdv5LowerPinWithHugeWrongSignIsRefused) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_refused(present(sign_box_qp(), sign_opts(alg), -1.25e12, -5.0));
    }
}

TEST(QpCertificateSign, UpperPinWithWrongSignIsRefused) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_refused(present(sign_box_qp(), sign_opts(alg), 5.0, 1e-3));
    }
}

TEST(QpCertificateSign, NanMultiplierIsRefused) {
    SignSeamGuard guard;
    expect_refused(present(sign_box_qp(), sign_opts(WorkingSetLinearAlgebra::kRefactorize),
                           std::numeric_limits<double>::quiet_NaN(), -5.0));
}

// The tolerance is the KKT-point test's own: opt_tol * scale, scale = 5 here.
TEST(QpCertificateSign, WrongSignInsideTheRelativeToleranceIsCertified) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpOptions o = sign_opts(alg);
        const double tol = o.opt_tol * kScale;
        expect_certified(present(sign_box_qp(), o, -0.5 * tol, -5.0), -0.5 * tol, -5.0);
        expect_certified(present(sign_box_qp(), o, 5.0, tol), 5.0, tol);
        expect_refused(present(sign_box_qp(), o, -2.0 * tol, -5.0));
    }
}

TEST(QpCertificateSign, WorstWrongSignReadsPinsAndUnshiftedWorkingRows) {
    QpProblem qp = sign_box_qp();
    qp.Ai = Eigen::MatrixXd::Ones(2, 3).sparseView();
    qp.bi = Vec::Constant(2, 10.0);
    WorkingSet ws(3, 2);
    ws.bound_state()[0] = BoundState::kAtLower;
    ws.bound_state()[1] = BoundState::kAtUpper;
    ws.bound_state()[2] = BoundState::kFixed;
    ws.add_ineq(0);
    ws.add_ineq(1);
    Vec z(3);
    z << -2.0, 3.0, -100.0; // kFixed is not sign-constrained
    Vec li(2);
    li << -7.0, -4.0;
    EXPECT_EQ(detail::worst_wrong_sign(qp, ws, li, z, nullptr), 7.0);
    Vec shift(2);
    shift << 1.0, 0.0; // row 0 is still being driven to feasibility: no sign
    EXPECT_EQ(detail::worst_wrong_sign(qp, ws, li, z, &shift), 4.0);
    li << 1.0, 1.0;
    EXPECT_EQ(detail::worst_wrong_sign(qp, ws, li, z, nullptr), 3.0);
    z(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(detail::worst_wrong_sign(qp, ws, li, z, nullptr)));
    EXPECT_FALSE(
        detail::dual_signs_hold(std::numeric_limits<double>::quiet_NaN(), 1.0, QpOptions{}));
}
