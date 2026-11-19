// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).
//
// THE PLATFORM-SOLVE RESPONSE (M8 sparse-equil-v4; detail/kkt/kkt_factor.h). The SQP
// runs its sparse KKT factors on the platform session without matching or scaling; a
// refined answer through platform numerics that perturbed pivots and still misses
// kPlatformSolveFailureTol in the equilibrated metric requests a retake on the
// maximum-product-matching scaled D K D, and that answer, if its numerics still perturb
// pivots and the backward error or the engine's own residual check fails it, escalates
// to the weighted-matching session. The response is per factorization.

#include <cmath>
#include <cstdio>
#include <limits>
#include <string>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/detail/qp/walk/eqp_solve.h>
#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"
#include "support/route_reference.h"

using namespace hven;
using namespace hven::detail;
using hven::linear::DensePolicy;
using hven::linear::FactorArm;

namespace {

using LMat = Eigen::Matrix<long double, Eigen::Dynamic, Eigen::Dynamic>;
using LVec = Eigen::Matrix<long double, Eigen::Dynamic, 1>;

// The coupled F3 fixture of test_qp_dense_arm.cpp (C341 F3): K = S K0 S at mu = 0,
// delta = 0, S = diag(1, 1, s | s, 1), x2 coupled to the unit block. The exact answer
// is S^-1 K0^-1 (S rhs), K0 solved in long double.
struct F3 {
    QpProblem qp;
    Vec ystar;
};
F3 coupled_f3(double s) {
    Eigen::MatrixXd H0(3, 3);
    H0 << 2.0, 0.5, 0.3, 0.5, 1.0, 0.2, 0.3, 0.2, 1.0;
    Eigen::MatrixXd A0(2, 3);
    A0 << 1.0, 1.0, 1.0, 1.0, -1.0, 0.5;
    Vec g0(3);
    g0 << -1.0, 0.3, 0.7;
    Vec b0(2);
    b0 << 0.4, 0.2;
    Vec sv(3);
    sv << 1.0, 1.0, s;
    Vec sr(2);
    sr << s, 1.0;
    F3 f;
    const Eigen::MatrixXd H = sv.asDiagonal() * H0 * sv.asDiagonal();
    f.qp.H = H.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    f.qp.g = g0.cwiseProduct(sv);
    const Eigen::MatrixXd A = sr.asDiagonal() * A0 * sv.asDiagonal();
    f.qp.Ae = A.sparseView();
    f.qp.be = b0.cwiseProduct(sr);
    f.qp.Ai.resize(0, 3);
    f.qp.bi = Vec(0);
    f.qp.lower = Vec::Constant(3, -1e30);
    f.qp.upper = Vec::Constant(3, 1e30);
    LMat K0 = LMat::Zero(5, 5);
    K0.topLeftCorner(3, 3) = H0.cast<long double>();
    K0.bottomLeftCorner(2, 3) = A0.cast<long double>();
    K0.topRightCorner(3, 2) = A0.transpose().cast<long double>();
    LVec r0(5);
    r0 << -g0.cast<long double>(), b0.cast<long double>();
    const LVec y0 = K0.fullPivLu().solve(r0);
    Vec S(5);
    S << sv, sr;
    f.ystar = y0.cast<double>().cwiseQuotient(S);
    return f;
}

QpOptions mu_free() {
    QpOptions o;
    o.dual_regularization = 0.0;
    o.primal_regularization = 0.0;
    return o;
}

double worst_relerr(const Vec &y, const Vec &ystar) {
    return (y - ystar).cwiseAbs().cwiseQuotient(ystar.cwiseAbs()).maxCoeff();
}

} // namespace

// The check fires only where the engine's own refinement against K could not repair the
// platform factor. MEASURED (MKL, the reference build): the raw platform solve is off by
// 7.8e-9 at s = 1e4 and 4.9e-5 at 1e6, and solve_eqp's refinement brings both to 1e-16, so
// nothing is retaken; from s = 1e8 the refined answer misses by >= 2.3e-4 (the walk ended
// kNumericalError there before the response), the scaled retake answers exactly and the
// walk certifies the exact answer at every scale. The scaled retake still reports one
// perturbed pivot there, but its answer passes both the backward error and the engine's
// residual check, so nothing escalates. Accelerate (exact throughout) never retakes.
TEST(QpPlatformRetake, TheCoupledScaledFixtureIsSolvedAtEveryScale) {
    for (const double s : {1.0, 1e4, 1e6, 1e8, 1e10, 1e12}) {
        SCOPED_TRACE(::testing::Message() << "s " << s);
        const F3 f = coupled_f3(s);
        const QpOptions o = mu_free();
        const WorkingSet ws(3, 0);
        KktFactor k(0, DensePolicy::kNever);
        const EqpResult e = solve_eqp(f.qp, ws, Vec::Zero(3), k, o);
        Vec ye(5);
        ye << e.x, e.lambda_e;
        EXPECT_FALSE(e.residual_refused);
        EXPECT_FALSE(e.retake_refused);
        EXPECT_LE(worst_relerr(ye, f.ystar), 1e-14);
        const QpSolution w = QpEngine(o, 0, DensePolicy::kNever).solve(f.qp);
        Vec yw(5);
        yw << w.x, w.lambda_e;
        EXPECT_EQ(static_cast<int>(w.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(worst_relerr(yw, f.ystar), 1e-14);
        EXPECT_EQ(k.matched_retakes, 0);
        EXPECT_EQ(w.counters.sparse_matched_retakes, 0);
        EXPECT_EQ(w.counters.sparse_retake_refusals, 0);
#ifndef __APPLE__
        EXPECT_EQ(k.numerics == PlatformRetake::kScaled, s >= 1e8);
        EXPECT_EQ(k.scaled_retakes, s >= 1e8 ? 1 : 0);
        EXPECT_EQ(w.counters.sparse_scaled_retakes > 0, s >= 1e8);
#else
        EXPECT_EQ(k.numerics, PlatformRetake::kNone);
        EXPECT_EQ(w.counters.sparse_scaled_retakes, 0);
#endif
    }
}

// The trigger, the escalation and the per-factorization lifetime, driven by hand.
TEST(QpPlatformRetake, TheResponseIsTieredAndPerFactorization) {
    const F3 f = coupled_f3(1e10);
    const QpOptions o = mu_free();
    const KktAssembly a = assemble_kkt(f.qp, WorkingSet(3, 0), o);
    Vec rhs(5);
    rhs << -f.qp.g, f.qp.be;
    const Vec garbage = Vec::Constant(5, 1.0);
    Vec nan = f.ystar;
    nan(4) = std::numeric_limits<double>::quiet_NaN();

    // The dense arm has its own check: never judged here.
    KktFactor dense(0, DensePolicy::kAlways);
    const auto fd = factorize_checked(dense, a.K, DenseShiftSite{3});
    ASSERT_EQ(fd.arm, FactorArm::kDense);
    EXPECT_FALSE(platform_solve_check_applies(dense));
    EXPECT_EQ(platform_solve_response(dense, a.K, rhs, garbage, true), PlatformRetake::kNone);

    KktFactor k(0, DensePolicy::kNever);
    const auto fo = factorize_checked(k, a.K, DenseShiftSite{3});
    ASSERT_EQ(fo.arm, FactorArm::kSparse);
    EXPECT_EQ(k.numerics, PlatformRetake::kNone);
    // Armed: Pardiso reported perturbed pivots; Accelerate reports no count at all.
    EXPECT_TRUE(platform_solve_check_applies(k));
    // THE TRIGGER: the backward error only (the engine's refusal does not count on the
    // platform's own numerics); a NaN misses.
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, garbage, false), PlatformRetake::kScaled);
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, nan, false), PlatformRetake::kScaled);
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, f.ystar, false), PlatformRetake::kNone);
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, f.ystar, true), PlatformRetake::kNone);

    // The scaled retake: requested, taken by the next factorization, solved through
    // D (D K D)^-1 D; K's inertia (3 positive, 2 negative) read from the scaled session.
    request_platform_retake(k, PlatformRetake::kScaled);
    EXPECT_TRUE(k.rebuild_pending);
    (void)factorize_checked(k, a.K, DenseShiftSite{3});
    EXPECT_FALSE(k.rebuild_pending);
    EXPECT_EQ(k.requested, PlatformRetake::kNone);
    ASSERT_EQ(k.numerics, PlatformRetake::kScaled);
    EXPECT_EQ(k.scaled_retakes, 1);
    EXPECT_EQ(kkt_inertia(k).positive, 3);
    EXPECT_EQ(kkt_inertia(k).negative, 2);
    EXPECT_LE(worst_relerr(solve_vec(k, rhs), f.ystar), 1e-14);

#ifndef __APPLE__
    // THE ESCALATION: judged only while the scaled numerics still perturb pivots (they do
    // here); then the engine's refusal alone fails an answer the backward error passes.
    ASSERT_GT(k.scaled_perturbed_pivots.value_or(0), 0);
    EXPECT_TRUE(platform_solve_check_applies(k));
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, f.ystar, false), PlatformRetake::kNone);
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, f.ystar, true), PlatformRetake::kMatched);
    EXPECT_EQ(platform_solve_response(k, a.K, rhs, garbage, false), PlatformRetake::kMatched);

    request_platform_retake(k, PlatformRetake::kMatched);
    (void)factorize_checked(k, a.K, DenseShiftSite{3});
    ASSERT_EQ(k.numerics, PlatformRetake::kMatched);
    EXPECT_EQ(k.matched_retakes, 1);
    EXPECT_FALSE(platform_solve_check_applies(k)); // never judged again
    EXPECT_EQ(kkt_inertia(k).positive, 3);
    EXPECT_EQ(kkt_inertia(k).negative, 2);
    EXPECT_LE(worst_relerr(solve_vec(k, rhs), f.ystar), 1e-14);
#else
    // Accelerate: no perturbed-pivot report, so no escalation (and no matched tier).
    EXPECT_FALSE(k.scaled_perturbed_pivots.has_value());
    EXPECT_FALSE(platform_solve_check_applies(k));
#endif

    // PER FACTORIZATION: the next factorization, with nothing requested, is the
    // platform's own again, bit for bit.
    (void)factorize_checked(k, a.K, DenseShiftSite{3});
    EXPECT_EQ(k.numerics, PlatformRetake::kNone);
    EXPECT_TRUE(k.retake_scale.size() == 0);
    KktFactor ref(0, DensePolicy::kNever);
    (void)factorize_checked(ref, a.K, DenseShiftSite{3});
    EXPECT_EQ(solve_vec(k, rhs), solve_vec(ref, rhs));
}

#ifndef __APPLE__
// A request is consumed by the next factorization even when that factorization
// perturbs no pivot, which then keeps the platform numerics bit for bit and is not judged.
TEST(QpPlatformRetake, ARequestLeavesAnUnperturbedFactorizationOnThePlatform) {
    QpProblem qp;
    Eigen::MatrixXd H(2, 2);
    H << 2.0, 0.0, 0.0, 1.0;
    qp.H = H.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -1.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e30);
    qp.upper = Vec::Constant(2, 1e30);
    const KktAssembly a = assemble_kkt(qp, WorkingSet(2, 0), QpOptions{});
    KktFactor k(0, DensePolicy::kNever);
    KktFactor ref(0, DensePolicy::kNever);
    request_platform_retake(k, PlatformRetake::kScaled);
    const auto fo = factorize_checked(k, a.K, DenseShiftSite{2});
    (void)factorize_checked(ref, a.K, DenseShiftSite{2});
    ASSERT_EQ(*fo.inertia.perturbed_pivots, 0);
    EXPECT_EQ(k.numerics, PlatformRetake::kNone);
    EXPECT_EQ(k.requested, PlatformRetake::kNone);
    EXPECT_EQ(k.scaled_retakes, 0);
    EXPECT_FALSE(platform_solve_check_applies(k));
    const Vec rhs = Vec::Constant(2, 1.0);
    EXPECT_EQ(solve_vec(k, rhs), solve_vec(ref, rhs));
}
#endif

// THE ESCALATION ON A REAL FIXTURE (YORKNET restoration qp26, the round's measured case). At
// minor 15 the platform numerics perturb one pivot and the refined answer misses (eta 0.25);
// the scaled retake still perturbs five, and its answer passes the backward error (1.1e-7)
// while the EQP-site residual check refuses it (ratio 3.7e7) -- the escalation's own
// clause -- and the matched retake answers. Reference build: kOptimal at the landing's
// certified point in 23 minors on both algebras (27 without a response, 23 with the matched
// retake alone), one scaled and one matched retake (Release). Off the reference build the route is
// recorded; the outcome contract is the walk's own (QpYrkRefusedExchange).
TEST(QpPlatformRetake, YorknetRestorationQp26EscalatesToTheMatchedRetake) {
    for (const auto alg : {QpWorkingSetAlgebra::kSchurBorder, QpWorkingSetAlgebra::kRefactorize}) {
        const bool border = alg == QpWorkingSetAlgebra::kSchurBorder;
        SCOPED_TRACE(border ? "border" : "refactorize");
        const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
            std::string(HVEN_QP_E2D4_FIXTURES) + "/YRK_YORKNET_qp26.txt");
        QpOptions o = d.opts;
        o.working_set_algebra = alg;
        const QpSolution s = hven::test_support::solve_qp_dump(d, o);
        std::printf("[ retake ] qp26 %s status=%d minors=%ld scaled=%ld matched=%ld refusals=%ld\n",
                    border ? "border" : "refactorize", static_cast<int>(s.status),
                    static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.sparse_scaled_retakes),
                    static_cast<long>(s.counters.sparse_matched_retakes),
                    static_cast<long>(s.counters.sparse_retake_refusals));
#if HVEN_TEST_ROUTE_REFERENCE
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.minor_iters, 23);
#ifdef NDEBUG
        // Release: the measured route. MEASURED under Debug's rounding the walk reaches the
        // same point in 23 minors on a route whose platform answers all pass (no retake).
        EXPECT_GE(s.counters.sparse_scaled_retakes, 1);
        EXPECT_GE(s.counters.sparse_matched_retakes, 1);
        EXPECT_NEAR(hven::test_support::qp_dump_objective(d, s.x), 0.133368173127, 1e-6);
#endif
#else
        hven::test_support::record_off_reference_route(
            std::string("YRK_YORKNET_qp26_retake_") + (border ? "border" : "refactorize"),
            "status=" + std::to_string(static_cast<int>(s.status)) +
                " minors=" + std::to_string(s.counters.minor_iters) +
                " scaled=" + std::to_string(s.counters.sparse_scaled_retakes) +
                " matched=" + std::to_string(s.counters.sparse_matched_retakes));
        EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
#endif
    }
}
