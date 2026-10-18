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
//
// THE INCUMBENT FALLBACK (HIGH-1): an adopted terminal polish that fails the sign half is undone
// and the pre-polish point is read by both halves. The polish re-prices a non-unique multiplier
// set, so its sign flip says nothing about optimality; the incumbent is certified only if it passes
// on its own, otherwise kNumericalError -- never kInfeasible. The seam presents the POLISHED pair
// (first read only, QpCertificateSeam::first_only); the incumbent is the walk's own point.
//
// The natural path: DALLASM's QN-label subproblem (solve 10 of the cell; convex, H = 8.9e4 I, a
// working set of rank 167 of 172). At fc567ce9 the terminal polish re-priced its face mu-free and
// the walk certified kOptimal with a lower pin priced z = -3.9e3 (relative 4.4e-2) at a point
// missing 7 of the optimum's active bounds (Clarabel). The dump was taken at the SDL head's first
// form (result line: kInfeasible, the polish refused); the QP is the same.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

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

// A QP whose terminal polish is adopted: an active inequality row under a large dual_mu leaves a
// complementarity footprint, and a pin (x2 at its lower bound, z2 = +3) gives the seam a sign to
// flip.
QpProblem polish_qp() {
    QpProblem qp;
    const Index n = 3;
    qp.H = Eigen::MatrixXd::Identity(n, n).sparseView();
    qp.g = Vec(n);
    qp.g << 1.0, 1.0, 3.0;
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    Eigen::MatrixXd a(1, n);
    a << 1.0, 1.0, 0.0;
    qp.Ai = a.sparseView();
    qp.bi = Vec::Constant(1, -4.0);
    qp.lower = Vec::Constant(n, -10.0);
    qp.lower(2) = 0.0;
    qp.upper = Vec::Constant(n, 10.0);
    return qp;
}

QpOptions polish_opts(WorkingSetLinearAlgebra alg) {
    QpOptions o;
    o.ws_algebra = alg;
    o.dual_mu = 1e-4;
    o.primal_delta = 1e-4; // leaves the free stage-1 polish a footprint to remove
    return o;
}

QpSolution present_polished(const QpOptions &opts, bool first_only) {
    Cert::reset();
    Cert::x = Vec(3);
    Cert::x << -2.0, -2.0, 0.0;
    Cert::z = Vec(3);
    Cert::z << 0.0, 0.0, -7.0; // the polished pin priced onto the wrong side
    Cert::first_only = first_only;
    Cert::armed = true;
    QpEngine engine{opts};
    QpSolution s = engine.solve(polish_qp());
    Cert::armed = false;
    return s;
}

TEST(QpCertificateSign, PolishPremiseTheTerminalPolishIsAdoptedUnarmed) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpEngine engine{polish_opts(alg)};
        const QpSolution s = engine.solve(polish_qp());
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_GE(s.counters.terminal_polish_adopted, 1);
        EXPECT_EQ(s.counters.terminal_polish_sign_reverts, 0);
        EXPECT_NEAR(s.z(2), 3.0, 1e-6);
    }
}

// The polish flips the pin's sign (presented); the incumbent is right-signed and is certified.
TEST(QpCertificateSign, PolishedSignFlipFallsBackToACertifiedIncumbent) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution s = present_polished(polish_opts(alg), true);
        ASSERT_EQ(Cert::presentations, 2) << "the incumbent was not read";
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.terminal_polish_sign_reverts, 1);
        EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
        EXPECT_NEAR(s.z(2), 3.0, 1e-6); // the incumbent's price, not the presented -7
    }
}

// The incumbent fails the sign half too: kNumericalError, never kInfeasible.
TEST(QpCertificateSign, IncumbentFailingTheSignHalfIsNumericalErrorNotInfeasible) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution s = present_polished(polish_opts(alg), false);
        ASSERT_EQ(Cert::presentations, 2);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(s.counters.terminal_polish_sign_reverts, 1);
        EXPECT_EQ(s.counters.certificate_sign_refusals, 1);
        EXPECT_EQ(s.z.lpNorm<Eigen::Infinity>(), 0.0);
    }
}

// The natural path (see the header): no kOptimal with a wrong-signed exported multiplier.
TEST(QpCertificateSign, DallasmQnSubproblemIsNotCertifiedWithAWrongSign) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
            std::string(HVEN_QP_E2D4_FIXTURES) + "/SDL_DALLASM_QN_qp10.txt");
        QpOptions o = d.opts;
        o.ws_algebra = alg;
        ASSERT_EQ(d.qp.mi(), 0); // the residual below assumes no inequality rows
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        std::printf("[ sdl ] DALLASM_QN_qp10 alg=%d status=%d minors=%ld refusals=%ld zmx=%ld\n",
                    static_cast<int>(alg), static_cast<int>(s.status),
                    static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.certificate_sign_refusals),
                    static_cast<long>(s.counters.zero_motion_exhausted));
        // Never a false kInfeasible on this feasible convex QP.
        EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            // The border walk reaches the certificate, the polish flips a sign, and the
            // incumbent fallback is taken: the incumbent is no certificate either.
            EXPECT_GE(s.counters.terminal_polish_sign_reverts, 1);
            EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        }
        // A sign refusal, the sign-checked kOptimal read below, or (refactorize) the walk's own
        // zero-motion exhaustion, which never reaches the certificate.
        EXPECT_TRUE(s.counters.certificate_sign_refusals >= 1 ||
                    s.counters.terminal_polish_sign_reverts >= 1 ||
                    s.status == QpStatus::kOptimal || s.counters.zero_motion_exhausted >= 1);
        if (s.status != QpStatus::kOptimal) {
            continue;
        }
        // Every pin's price, the TR-held ones included: there z is exported as 0 and the price
        // is the stationarity residual's entry (QpSolution::tr_active), signed by the side x
        // sits on (the TR box about the clamped seed).
        const QpProblem &qp = d.qp;
        const double tr =
            std::isfinite(d.overrides.tr_radius) ? d.overrides.tr_radius : d.opts.tr_radius;
        Vec r = qp.H.selfadjointView<Eigen::Upper>() * s.x + qp.g + qp.Ae.transpose() * s.lambda_e;
        const double scale = std::max(1.0, r.lpNorm<Eigen::Infinity>());
        for (Index i = 0; i < qp.n(); ++i) {
            const auto si = static_cast<std::size_t>(i);
            double price = s.z(i);
            bool at_lower = s.bound_state[si] == BoundState::kAtLower;
            bool at_upper = s.bound_state[si] == BoundState::kAtUpper;
            if (si < s.tr_active.size() && s.tr_active[si]) {
                const double x0 =
                    std::clamp(d.has_seed ? d.seed.x(i) : 0.0, qp.lower(i), qp.upper(i));
                price = r(i);
                at_lower = std::abs(s.x(i) - std::max(qp.lower(i), x0 - tr)) <=
                           std::abs(s.x(i) - std::min(qp.upper(i), x0 + tr));
                at_upper = !at_lower;
            }
            if (at_lower) {
                EXPECT_GE(price, -1e-8 * scale) << "lower pin " << i;
            } else if (at_upper) {
                EXPECT_LE(price, 1e-8 * scale) << "upper pin " << i;
            }
        }
    }
}
