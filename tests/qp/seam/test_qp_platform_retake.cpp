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

#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/detail/linear/fault_injection.h>
#include <hven/detail/qp/walk/bordered_eqp.h>
#include <hven/detail/qp/walk/eqp_solve.h>
#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"
#include "qp/support/qp_engine_test_access.h"
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
        const EqpResult e = solve_eqp(f.qp, ws, Vec::Zero(3), EqpDualCentre{}, k, o);
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
    // THE SESSION READ: the evidence is the scaled session's own report, not the platform's.
    // The counts above are K's on every session (Sylvester) and how many pivots each
    // session perturbs is the backend's numerics, so neither tells the sessions apart; the
    // platform session is put on -K0 (K0 = the s = 1 fixture's K: K's pattern, well
    // conditioned, inertia 2 positive, 3 negative), which kkt_inertia must not see.
    EXPECT_EQ(kkt_inertia(k).perturbed_pivots, k.scaled_perturbed_pivots);
    const SpMatRM flipped = -assemble_kkt(coupled_f3(1.0).qp, WorkingSet(3, 0), o).K;
    ASSERT_EQ(pattern_hash(flipped), pattern_hash(a.K));
    const auto flip_platform = [&] {
        ASSERT_EQ(k.factor.factorize(flipped).status, hven::linear::FactorizeOutcome::Status::kOk);
        ASSERT_EQ(k.factor.inertia().positive, 2);
        ASSERT_EQ(k.factor.inertia().negative, 3);
    };
    flip_platform();
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
    EXPECT_EQ(kkt_inertia(k).perturbed_pivots, k.matched_session->inertia().perturbed_pivots);
    flip_platform(); // the matched session's read, the same way
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

// THE ESCALATION ON A REAL FIXTURE (YORKNET restoration qp26, the round's measured case). At one
// minor the platform numerics perturb a pivot and the refined answer misses the backward error;
// the scaled retake still perturbs two, and its answer misses again (the backward error AND the
// EQP-site residual check, ratio 1.7e21) -- so the escalation fires on the backward error, and this
// fixture does not isolate the refusal clause (r2's I4 experiment: unchanged without it; the
// clause's fixture is LakesQp0EscalatesOnTheEngineRefusal) -- and the matched retake answers.
// Reference build: kOptimal at the landing's certified point in 23 minors on both algebras (27
// without a response), one scaled and one matched retake (Release). Off the reference build the
// route is recorded; the outcome contract is the walk's own (QpYrkRefusedExchange).
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
#if HVEN_TEST_RELEASE_BUILD
        // Release (the build type, not NDEBUG): the measured route. MEASURED under Debug's rounding
        // the walk reaches the same point in 23 minors on a route whose platform answers all pass
        // (no retake).
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

namespace {

using hven::linear::detail::testing::AnalyzeFaultInjector;

// The injector is a process-global static: disarmed by the guard, so a failed assertion cannot
// leave the next test running under it.
struct ArmedAnalyzeFault {
    ArmedAnalyzeFault() { AnalyzeFaultInjector::active = true; }
    ~ArmedAnalyzeFault() { AnalyzeFaultInjector::active = false; }
    ArmedAnalyzeFault(const ArmedAnalyzeFault &) = delete;
    ArmedAnalyzeFault &operator=(const ArmedAnalyzeFault &) = delete;
};

// F3 at s = 1e10 factored on the platform, armed (the fixture's platform answer fails).
struct ArmedF3 {
    F3 f = coupled_f3(1e10);
    KktAssembly a = assemble_kkt(f.qp, WorkingSet(3, 0), mu_free());
    Vec rhs = Vec(5);
    KktFactor k{0, DensePolicy::kNever};
    ArmedF3() {
        rhs << -f.qp.g, f.qp.be;
        (void)factorize_checked(k, a.K, DenseShiftSite{3});
    }
};

} // namespace

// I1 AND THE DISARM: a retake that fails -- the retake session's analysis throws a BackendError
// (AnalyzeFaultInjector, the HVEN_TESTING seam; docs/testing.md), or the scaling refuses its input
// with std::invalid_argument -- never escapes the engine. The platform's numerics stay in place,
// bit for bit, with the check disarmed (the response's bound), on both paths a retake is taken by:
// in place (the eliminated path) and by request (K0's next factorization).
TEST(QpPlatformRetake, AFailedRetakeLeavesThePlatformNumericsDisarmed) {
    {
        SCOPED_TRACE("in place, the scaled session's analysis throws");
        ArmedF3 t;
        ASSERT_TRUE(platform_solve_check_applies(t.k));
        const Vec ref = solve_vec(t.k, t.rhs);
        bool changed = true;
        {
            const ArmedAnalyzeFault armed;
            EXPECT_NO_THROW(changed = retake_in_place(t.k, t.a.K, PlatformRetake::kScaled));
        }
        EXPECT_FALSE(changed); // the answer in hand is already the platform's
        EXPECT_EQ(t.k.numerics, PlatformRetake::kNone);
        EXPECT_FALSE(platform_solve_check_applies(t.k));
        EXPECT_EQ(t.k.scaled_retakes, 0); // no factorization ran
        EXPECT_FALSE(t.k.scaled_analyzed);
        EXPECT_EQ(solve_vec(t.k, t.rhs), ref);
    }
    {
        SCOPED_TRACE("by request, the scaled session's analysis throws");
        ArmedF3 t;
        request_platform_retake(t.k, PlatformRetake::kScaled);
        {
            // K's pattern is analyzed already: only the retake session analyzes.
            const ArmedAnalyzeFault armed;
            EXPECT_NO_THROW((void)factorize_checked(t.k, t.a.K, DenseShiftSite{3}));
        }
        EXPECT_EQ(t.k.requested, PlatformRetake::kNone);
        EXPECT_FALSE(t.k.rebuild_pending);
        EXPECT_EQ(t.k.numerics, PlatformRetake::kNone);
        EXPECT_FALSE(platform_solve_check_applies(t.k));
        EXPECT_EQ(t.k.scaled_retakes, 0);
    }
    {
        SCOPED_TRACE("in place, the scaling refuses its input (std::invalid_argument)");
        ArmedF3 t;
        SpMatRM bad = t.a.K; // a below-diagonal entry: symmetric_matching_scaling refuses it
        bad.coeffRef(4, 0) = 1.0;
        bad.makeCompressed();
        bool changed = true;
        EXPECT_NO_THROW(changed = retake_in_place(t.k, bad, PlatformRetake::kScaled));
        EXPECT_FALSE(changed);
        EXPECT_EQ(t.k.numerics, PlatformRetake::kNone);
        EXPECT_FALSE(platform_solve_check_applies(t.k));
    }
#ifndef __APPLE__
    {
        SCOPED_TRACE("in place, the escalation's matched session's analysis throws");
        ArmedF3 t;
        ASSERT_TRUE(retake_in_place(t.k, t.a.K, PlatformRetake::kScaled));
        ASSERT_EQ(t.k.numerics, PlatformRetake::kScaled);
        ASSERT_TRUE(platform_solve_check_applies(t.k));
        bool changed = false;
        {
            const ArmedAnalyzeFault armed;
            EXPECT_NO_THROW(changed = retake_in_place(t.k, t.a.K, PlatformRetake::kMatched));
        }
        EXPECT_TRUE(changed); // back on the platform's numerics: the caller solves again
        EXPECT_EQ(t.k.numerics, PlatformRetake::kNone);
        EXPECT_TRUE(t.k.retake_scale.size() == 0);
        EXPECT_FALSE(platform_solve_check_applies(t.k));
        EXPECT_EQ(t.k.matched_retakes, 0);
        KktFactor ref(0, DensePolicy::kNever);
        (void)factorize_checked(ref, t.a.K, DenseShiftSite{3});
        EXPECT_EQ(solve_vec(t.k, t.rhs), solve_vec(ref, t.rhs));
    }
#endif
}

// M6: the scaled solve checks its sizes itself (Eigen's assert is compiled out in Release).
TEST(QpPlatformRetake, TheScaledSolveRefusesAMismatchedRightHandSide) {
    ArmedF3 t;
    ASSERT_TRUE(retake_in_place(t.k, t.a.K, PlatformRetake::kScaled));
    ASSERT_EQ(t.k.numerics, PlatformRetake::kScaled);
    EXPECT_THROW((void)solve_vec(t.k, Vec::Ones(4)), std::invalid_argument);
    SpMatRM loose = t.a.K;
    loose.uncompress();
    EXPECT_THROW((void)retake_in_place(t.k, loose, PlatformRetake::kScaled), std::invalid_argument);
}

// M3 AND M4: the scaled session keeps its analysis while K's pattern holds (D K D has K's pattern),
// and a session the current numerics do not live in is freed on request.
TEST(QpPlatformRetake, TheScaledSessionIsAnalyzedOncePerPatternAndReleasedWhenIdle) {
    ArmedF3 t;
    ASSERT_TRUE(retake_in_place(t.k, t.a.K, PlatformRetake::kScaled));
    ASSERT_TRUE(t.k.scaled_analyzed);
    const std::uint64_t session = t.k.scaled_session->session_id();
    (void)factorize_checked(t.k, t.a.K, DenseShiftSite{3});
    ASSERT_TRUE(retake_in_place(t.k, t.a.K, PlatformRetake::kScaled));
    EXPECT_EQ(t.k.scaled_session->session_id(), session); // no second analysis
    EXPECT_EQ(t.k.scaled_retakes, 2);
    EXPECT_LE(worst_relerr(solve_vec(t.k, t.rhs), t.f.ystar), 1e-14);
    release_idle_retake_sessions(t.k); // the numerics live in it: kept
    ASSERT_NE(t.k.scaled_session, nullptr);
    EXPECT_EQ(t.k.matched_session, nullptr);
    (void)factorize_checked(t.k, t.a.K, DenseShiftSite{3});
    release_idle_retake_sessions(t.k);
    EXPECT_EQ(t.k.scaled_session, nullptr);
    EXPECT_FALSE(t.k.scaled_analyzed);
}

namespace {

// F3 plus a decoupled x4 (H44 = 1) pinned at its upper bound 1: a pin border on a K0 that is F3's
// own ill-scaled KKT block.
QpProblem f3_with_pin(const F3 &f) {
    QpProblem qp;
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(4, 4);
    H.topLeftCorner(3, 3) = Eigen::MatrixXd(f.qp.H);
    H(3, 3) = 1.0;
    qp.H = H.sparseView();
    qp.g = Vec::Zero(4);
    qp.g.head(3) = f.qp.g;
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(2, 4);
    A.leftCols(3) = Eigen::MatrixXd(f.qp.Ae);
    qp.Ae = A.sparseView();
    qp.be = f.qp.be;
    qp.Ai.resize(0, 4);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(4, -1e30);
    qp.upper = Vec::Constant(4, 1e30);
    qp.lower(3) = -1.0;
    qp.upper(3) = 1.0;
    return qp;
}

} // namespace

// I3: THE BORDERED ANSWER CARRIES THE VERDICT (platform_retake_verdict, the one function both
// sites call). K0's platform numerics fail the check on the bordered system: solve_bordered_eqp
// reports the scaled retake, the caller requests it on K0's factor, the NEXT K0 factorization
// takes it, and the bordered answer through those numerics passes and is exact.
TEST(QpPlatformRetake, TheBorderedAnswerCarriesTheVerdict) {
    const F3 f = coupled_f3(1e10);
    const QpProblem qp = f3_with_pin(f);
    const QpOptions o = mu_free();
    const QpEngine engine(o, 0, DensePolicy::kNever);
    BorderState border(0, DensePolicy::kNever);
    QpCounters counters;
    const WorkingSet k0_ws(4, 0);
    WorkingSet ws(4, 0);
    ws.bound_state()[3] = BoundState::kAtUpper;
    QpEngineTestAccess::build_border(engine, qp, k0_ws, ws, border, counters, o);
    ASSERT_EQ(border.ledger.size(), 1u);
    const auto solve = [&] {
        return solve_bordered_eqp(qp, border.k0, border.k0_rows, *border.schur, border.ledger, ws,
                                  Vec::Zero(4), EqpDualCentre{}, o, nullptr);
    };
    const EqpResult first = solve();
#ifndef __APPLE__
    EXPECT_EQ(first.platform_retake, PlatformRetake::kScaled);
    request_platform_retake(border.kkt, first.platform_retake);
    EXPECT_TRUE(border.schur->needs_refactorization());
    QpEngineTestAccess::build_border(engine, qp, k0_ws, ws, border, counters, o);
    ASSERT_EQ(border.kkt.numerics, PlatformRetake::kScaled);
    EXPECT_FALSE(border.kkt.rebuild_pending);
    const EqpResult second = solve();
    EXPECT_EQ(second.platform_retake, PlatformRetake::kNone);
    EXPECT_FALSE(second.residual_refused);
    Vec y(5);
    y << second.x.head(3), second.lambda_e;
    EXPECT_LE(worst_relerr(y, f.ystar), 1e-12);
    EXPECT_EQ(second.x(3), 1.0);
#else
    // Accelerate answers F3 exactly (TheCoupledScaledFixtureIsSolvedAtEveryScale).
    EXPECT_EQ(first.platform_retake, PlatformRetake::kNone);
#endif
}

// I2: A PENDING K0 RETAKE IS NOT THE NO-OP LATCH. A border stack of pins over K0's own rows is the
// pins-only dead end rebuild_would_be_noop() latches on; a pending retake request makes the same
// rebuild a refactorization on another session, so the pass rebuilds (taking the request) and no
// conditioning latch is counted.
TEST(QpPlatformRetake, APendingK0RetakeIsNotTheNoOpLatch) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Identity(3, 3).sparseView();
    qp.g = Vec::Constant(3, -1.0);
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -10.0);
    qp.upper = Vec::Constant(3, 10.0);
    qp.upper(2) = 0.5;
    QpOptions o;
    o.working_set_algebra = QpWorkingSetAlgebra::kSchurBorder;
    const QpEngine engine(o, 0, DensePolicy::kNever);
    BorderState border(0, DensePolicy::kNever);
    QpCounters counters;
    const WorkingSet k0_ws(3, 0);
    WorkingSet ws(3, 0);
    ws.bound_state()[2] = BoundState::kAtUpper;
    QpEngineTestAccess::build_border(engine, qp, k0_ws, ws, border, counters, o);
    ASSERT_TRUE(QpEngineTestAccess::rebuild_would_be_noop(ws, border));
    request_platform_retake(border.kkt, PlatformRetake::kScaled);
    EXPECT_FALSE(QpEngineTestAccess::rebuild_would_be_noop(ws, border));
    KktFactor kkt(0, DensePolicy::kNever);
    const EqpResult r =
        QpEngineTestAccess::border_solve(engine, qp, ws, Vec::Zero(3), kkt, border, counters, o);
    EXPECT_EQ(counters.border_latches_conditioning, 0);
    EXPECT_FALSE(border.latched);
    EXPECT_EQ(border.kkt.requested, PlatformRetake::kNone);
    EXPECT_FALSE(border.kkt.rebuild_pending);
    EXPECT_NEAR(r.x(0), 1.0, 1e-10);
    EXPECT_EQ(r.x(2), 0.5);
}

// I2: A REQUEST DOES NOT SURVIVE INTO THE NEXT QP SOLVE. A request left pending on the K0 a hot
// handle carries (as a solve that ended between the request and the rebuild would leave it) is
// dropped when the next solve starts: the warm re-solve reuses K0 untouched -- no factorization,
// no retake -- and nothing is pending after it.
TEST(QpPlatformRetake, ARequestDoesNotSurviveIntoTheNextSolve) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Identity(2, 2).sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, -2.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd ai(1, 2);
    ai << 1.0, 1.0;
    qp.Ai = ai.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    QpOptions o;
    o.working_set_algebra = QpWorkingSetAlgebra::kSchurBorder;
    QpEngine engine(o, 0, DensePolicy::kNever);
    const QpSolution cold = engine.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    const auto hot = engine.hot_state();
    ASSERT_NE(hot, nullptr);
    request_platform_retake(hot->border->kkt, PlatformRetake::kScaled);
    const QpSolution warm = engine.solve(qp, cold);
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_TRUE(warm.counters.k0_reused);
    EXPECT_EQ(warm.counters.factorizations, 0);
    EXPECT_EQ(warm.counters.sparse_scaled_retakes, 0);
    EXPECT_EQ(hot->border->kkt.requested, PlatformRetake::kNone);
    EXPECT_FALSE(hot->border->kkt.rebuild_pending);
}

// I4: THE REFUSAL CLAUSE ON A REAL FIXTURE. LAKES's first walk solve that took a REFUSAL-ONLY
// escalation (tests/qp/fixtures/e2-d4/FORMAT.txt, SEQ4_LAKES_qp0.txt): scaled numerics that still
// perturb a pivot answer with a backward error inside kPlatformSolveFailureTol while the EQP-site
// residual check refuses the answer (dual_mu 1e-8, 78 rows on 103-108 free variables, not
// over-determined); the clause escalates and the matched retake's answer is ACCEPTED. Without the
// clause (r2's Me1) the scaled answers are refused instead (sparse_retake_refusals). Reference
// build: kOptimal in 131 minors, 3 scaled and 2 matched retakes, 0 retake refusals.
TEST(QpPlatformRetake, LakesQp0EscalatesOnTheEngineRefusal) {
    const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
        std::string(HVEN_QP_E2D4_FIXTURES) + "/SEQ4_LAKES_qp0.txt");
    const QpSolution s = hven::test_support::solve_qp_dump(d, d.opts);
    std::printf("[ retake ] LAKES qp0 status=%d minors=%ld scaled=%ld matched=%ld refusals=%ld\n",
                static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.sparse_scaled_retakes),
                static_cast<long>(s.counters.sparse_matched_retakes),
                static_cast<long>(s.counters.sparse_retake_refusals));
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(s.counters.sparse_matched_retakes, 1);
    EXPECT_EQ(s.counters.sparse_retake_refusals, 0);
#else
    hven::test_support::record_off_reference_route(
        "SEQ4_LAKES_qp0_retake",
        "status=" + std::to_string(static_cast<int>(s.status)) +
            " minors=" + std::to_string(s.counters.minor_iters) +
            " scaled=" + std::to_string(s.counters.sparse_scaled_retakes) +
            " matched=" + std::to_string(s.counters.sparse_matched_retakes));
#endif
}
