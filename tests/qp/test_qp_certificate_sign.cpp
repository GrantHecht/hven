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
//
// THE BOUND HALF (SDL-4): a point off its own box is a KKT point of the working set, not of the QP
// (a pass-over leak); it is refused kNumericalError (certificate_bound_refusals), never
// kInfeasible. Seam fixtures on a feasible QP, and BRITGAS's QN subproblem 5 (natural, false
// kInfeasible at e9cb2472).
//
// THE ROW HALF (SDL-6): a structural row violation is kInfeasible only on a verified Farkas
// certificate, else kNumericalError (certificate_row_refusals). Natural fixtures NET3/MSS2 (false
// kInfeasible at e9cb2472) and DALLASM (a true one, still certified).

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

// THE BOUND HALF (SDL-4). The certificate reads the walk's own point whenever the polish is
// skipped or rejected -- a KKT point of the working set, not of the QP -- so a free column left
// past its box (a pass-over leak) is no evidence about the QP. sign_box_qp is feasible; before
// SDL-4 each presented point below came back kInfeasible.
QpSolution present_off_box(const QpOptions &opts, double x0) {
    Cert::reset();
    Cert::x = Vec(3);
    Cert::x << x0, 0.0, 0.5;
    Cert::z = Vec(3);
    Cert::z << 5.0, -5.0, 0.0;
    Cert::armed = true;
    QpEngine engine{opts};
    QpSolution s = engine.solve(sign_box_qp());
    Cert::armed = false;
    return s;
}

void expect_bound_refused(const QpSolution &s, double x0) {
    ASSERT_EQ(Cert::presentations, 1) << "the certificate never read the presented pair";
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
    EXPECT_EQ(s.counters.certificate_bound_refusals, 1);
    EXPECT_EQ(s.counters.certificate_sign_refusals, 0);
    EXPECT_EQ(s.x(0), x0); // the walk's point is reported, multipliers cleared
    EXPECT_EQ(s.z.lpNorm<Eigen::Infinity>(), 0.0);
}

TEST(QpCertificateSign, BoundHalfOffBoxPointOnAFeasibleQpIsNumericalErrorNotInfeasible) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        expect_bound_refused(present_off_box(sign_opts(alg), -0.1), -0.1); // below lower(0) = 0
        expect_bound_refused(present_off_box(sign_opts(alg), 1.5), 1.5);   // above upper(0) = 1
    }
}

// The unpolished path: both regularizations 0, so the polish is skipped and the certificate
// reads the walk's iterate as it stands.
TEST(QpCertificateSign, BoundHalfOnTheUnpolishedPathIsNumericalErrorNotInfeasible) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpOptions o = sign_opts(alg);
        o.dual_mu = 0.0;
        o.primal_delta = 0.0;
        const QpSolution s = present_off_box(o, -0.1);
        EXPECT_EQ(s.counters.terminal_polish_solves, 0);
        EXPECT_EQ(s.counters.terminal_polish_adopted, 0);
        expect_bound_refused(s, -0.1);
    }
}

// Inside the structural margin the bound half still tolerates the point (E4).
TEST(QpCertificateSign, BoundHalfInsideTheMarginStillCertifies) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpOptions o = sign_opts(alg);
        const QpSolution s = present_off_box(o, -0.5 * o.feas_tol);
        ASSERT_EQ(Cert::presentations, 1);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.certificate_bound_refusals, 0);
    }
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

// SDL-4: the polished point leaves its box (presented: x2 = -0.5 below its lower bound 0, the
// signs right). The polish is undone; the incumbent is in its box and certified.
QpSolution present_polished_off_box(const QpOptions &opts, bool first_only) {
    Cert::reset();
    Cert::x = Vec(3);
    Cert::x << -2.0, -2.0, -0.5;
    Cert::z = Vec(3);
    Cert::z << 0.0, 0.0, 3.0;
    Cert::first_only = first_only;
    Cert::armed = true;
    QpEngine engine{opts};
    QpSolution s = engine.solve(polish_qp());
    Cert::armed = false;
    return s;
}

TEST(QpCertificateSign, PolishedPointOffItsBoxFallsBackToACertifiedIncumbent) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution s = present_polished_off_box(polish_opts(alg), true);
        ASSERT_EQ(Cert::presentations, 2) << "the incumbent was not read";
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.terminal_polish_bound_reverts, 1);
        EXPECT_EQ(s.counters.terminal_polish_sign_reverts, 0);
        EXPECT_EQ(s.counters.certificate_bound_refusals, 0);
        EXPECT_GE(s.x(2), 0.0); // the incumbent's point, not the presented -0.5
        EXPECT_NEAR(s.z(2), 3.0, 1e-6);
    }
}

// The incumbent is off its box too: kNumericalError, never kInfeasible.
TEST(QpCertificateSign, IncumbentOffItsBoxIsNumericalErrorNotInfeasible) {
    SignSeamGuard guard;
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const QpSolution s = present_polished_off_box(polish_opts(alg), false);
        ASSERT_EQ(Cert::presentations, 2);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(s.counters.terminal_polish_bound_reverts, 1);
        EXPECT_EQ(s.counters.certificate_bound_refusals, 1);
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
        std::printf("[ sdl ] DALLASM_QN_qp10 alg=%d status=%d minors=%ld refusals=%ld zmx=%ld "
                    "reverts=%ld bound_refusals=%ld\n",
                    static_cast<int>(alg), static_cast<int>(s.status),
                    static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.certificate_sign_refusals),
                    static_cast<long>(s.counters.zero_motion_exhausted),
                    static_cast<long>(s.counters.terminal_polish_sign_reverts),
                    static_cast<long>(s.counters.certificate_bound_refusals));
        // Never a false kInfeasible on this feasible convex QP.
        EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        if (alg == WorkingSetLinearAlgebra::kSchurBorder) {
            // The border walk reaches the certificate, the polish flips a sign, and the
            // incumbent fallback is taken: the incumbent stands off its box (SDL-4).
            EXPECT_GE(s.counters.terminal_polish_sign_reverts, 1);
            EXPECT_EQ(s.counters.certificate_bound_refusals, 1);
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

// SDL-4's natural path: BRITGAS's QN-label subproblem (solve 5 of the cell; feasible -- HiGHS phase
// 1 at zero violation -- and convex, Clarabel f* = 46659152.05). At e9cb2472 the walk ended with
// free column 326 at 1.0176 against its upper bound 1 (a pass-over leak), the polish was rejected,
// and the bound half read kInfeasible.
TEST(QpCertificateSign, BritgasQnSubproblemOffBoxLeakIsNotInfeasible) {
    SignSeamGuard guard;
    const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
        std::string(HVEN_QP_E2D4_FIXTURES) + "/SDL4_BRITGAS_QN_qp5.txt");
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        QpOptions o = d.opts;
        o.ws_algebra = alg;
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        std::printf("[ sdl ] BRITGAS_QN_qp5 alg=%d status=%d minors=%ld bound_refusals=%ld "
                    "polish_rejected=%ld x326=%.10g\n",
                    static_cast<int>(alg), static_cast<int>(s.status),
                    static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.certificate_bound_refusals),
                    static_cast<long>(s.counters.terminal_polish_rejected), s.x(326));
        EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        // The premise, live: the walk's point is off its box by the leak, the polish refused.
        EXPECT_GT(s.x(326) - d.qp.upper(326), 1e-2);
        EXPECT_EQ(s.counters.terminal_polish_rejected, 1);
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
        EXPECT_EQ(s.counters.certificate_bound_refusals, 1);
    }
}

// THE ROW HALF (SDL-6): a terminal structural row violation is kInfeasible only on a VERIFIED
// Farkas certificate (verify_terminal_farkas), else kNumericalError (certificate_row_refusals).
// Natural fixtures from the QN label at e9cb2472, each read kInfeasible there (site 1331):
//   NET3 solve 98: feasible (HiGHS phase 1 at 0; Clarabel f* = -683.26); the walk dead-ends on an
//     exactly rank-deficient working set with an inconsistent rhs, working rows violated by 0.108.
//   MSS2 solve 58: feasible (phase 1 at 0), equality rows only; a consistent, numerically
//     rank-deficient Ae (694 of 703) leaves a dual_mu footprint of 3.4e-6 the mu-free refinement
//     does not remove.
//   DALLASM solve 0: infeasible (phase 1 min l1 violation 0.448; Clarabel PrimalInfeasible); the
//     certificate needs the box (7 pins) and comes from the inverse-iteration candidate.
namespace {

struct RowHalfRun {
    QpSolution s;
    double eq_res = 0.0;
    double row_res = 0.0;
};

RowHalfRun run_row_half(const hven::test_support::QpDump &d, WorkingSetLinearAlgebra alg) {
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    RowHalfRun r;
    r.s = hven::test_support::solve_qp_dump(d, o);
    if (d.qp.me() > 0) {
        r.eq_res = (d.qp.Ae * r.s.x - d.qp.be).lpNorm<Eigen::Infinity>();
    }
    for (Index j = 0; j < d.qp.mi(); ++j) {
        r.row_res = std::max(r.row_res, d.qp.Ai.row(j).dot(r.s.x) - d.qp.bi(j));
    }
    return r;
}

void print_row_half(const char *name, WorkingSetLinearAlgebra alg, const RowHalfRun &r) {
    std::printf("[ sdl6 ] %s alg=%d status=%d minors=%ld row_refusals=%ld tfc=%ld eq_res=%.3e "
                "row_res=%.3e\n",
                name, static_cast<int>(alg), static_cast<int>(r.s.status),
                static_cast<long>(r.s.counters.minor_iters),
                static_cast<long>(r.s.counters.certificate_row_refusals),
                static_cast<long>(r.s.counters.terminal_farkas_certificates), r.eq_res, r.row_res);
}

hven::test_support::QpDump row_half_fixture(const char *name) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + name);
}

} // namespace

TEST(QpCertificateSign, Net3QnSubproblemInconsistentFaceIsNotInfeasible) {
    SignSeamGuard guard;
    const auto d = row_half_fixture("SDL6_NET3_QN_qp98.txt");
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const RowHalfRun r = run_row_half(d, alg);
        print_row_half("NET3_QN_qp98", alg, r);
        EXPECT_NE(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(r.s.counters.terminal_farkas_certificates, 0);
    }
    // The in-cell path, live: the walk's own point violates its rows.
    const RowHalfRun r = run_row_half(d, d.opts.ws_algebra);
    EXPECT_GT(r.row_res, 1e-2);
    EXPECT_EQ(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kNumericalError));
    EXPECT_EQ(r.s.counters.certificate_row_refusals, 1);
}

TEST(QpCertificateSign, Mss2QnSubproblemMuFootprintIsNotInfeasible) {
    SignSeamGuard guard;
    const auto d = row_half_fixture("SDL6_MSS2_QN_qp58.txt");
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const RowHalfRun r = run_row_half(d, alg);
        print_row_half("MSS2_QN_qp58", alg, r);
        EXPECT_NE(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_EQ(r.s.counters.terminal_farkas_certificates, 0);
    }
    const RowHalfRun r = run_row_half(d, d.opts.ws_algebra);
    EXPECT_GT(r.eq_res, 1e-6);
    EXPECT_EQ(r.s.counters.terminal_mu_refines, 1);
    EXPECT_EQ(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kNumericalError));
    EXPECT_EQ(r.s.counters.certificate_row_refusals, 1);
}

TEST(QpCertificateSign, DallasmQnInfeasibleSubproblemKeepsAVerifiedVerdict) {
    SignSeamGuard guard;
    const auto d = row_half_fixture("SDL6_DALLASM_QN_qp0.txt");
    for (const auto alg : kAlgebras) {
        SCOPED_TRACE(static_cast<int>(alg));
        const RowHalfRun r = run_row_half(d, alg);
        print_row_half("DALLASM_QN_qp0", alg, r);
        EXPECT_EQ(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kInfeasible));
        EXPECT_GE(r.s.counters.terminal_farkas_certificates +
                      r.s.counters.dep_infeasible_certificates,
                  1);
        EXPECT_EQ(r.s.counters.certificate_row_refusals, 0);
    }
    const RowHalfRun r = run_row_half(d, d.opts.ws_algebra);
    EXPECT_GT(r.eq_res, 1e-1);
    EXPECT_EQ(static_cast<int>(r.s.status), static_cast<int>(QpStatus::kInfeasible));
    EXPECT_EQ(r.s.counters.terminal_farkas_certificates, 1);
    EXPECT_EQ(r.s.counters.certificate_row_refusals, 0);
    EXPECT_FALSE(r.s.farkas.present); // the terminal certificate is not blocker-shaped
}
