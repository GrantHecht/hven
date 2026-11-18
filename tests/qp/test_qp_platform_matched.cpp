// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).
//
// THE PLATFORM SOLVE CHECK AND THE ON-DEMAND MATCHED SESSION (M8 sparse-equil-v2;
// detail::platform_solve_failed, kkt_factor.h). MKL Pardiso runs the SQP's KKT factors
// without matching or scaling; a refined answer that still misses
// kPlatformSolveFailureTol in the equilibrated metric switches its factor to a
// weighted-matching session for good, and the EQP is solved again on it.

#include <cmath>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/detail/qp/walk/eqp_solve.h>
#include <hven/detail/qp/walk/walk_engine.h>

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
// 7.8e-9 at s = 1e4 and 4.9e-5 at 1e6, and solve_eqp's refinement brings both to 1e-16
// (refined backward error <= 6e-13), so nothing switches; from s = 1e8 the refined answer
// misses by >= 2.3e-4 (the walk ended kNumericalError there before this change), the factor
// switches and the walk certifies the exact answer at every scale. Accelerate (exact
// throughout, no perturbed-pivot count) never switches.
TEST(QpPlatformMatched, TheCoupledScaledFixtureIsSolvedAtEveryScale) {
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
        EXPECT_LE(worst_relerr(ye, f.ystar), 1e-14);
        const QpSolution w = QpEngine(o, 0, DensePolicy::kNever).solve(f.qp);
        Vec yw(5);
        yw << w.x, w.lambda_e;
        EXPECT_EQ(static_cast<int>(w.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_LE(worst_relerr(yw, f.ystar), 1e-14);
#ifndef __APPLE__
        EXPECT_EQ(k.matched_mode, s >= 1e8);
        EXPECT_EQ(k.matched_refactors, s >= 1e8 ? 1 : 0);
        EXPECT_EQ(w.counters.sparse_matched_refactors > 0, s >= 1e8);
#else
        EXPECT_FALSE(k.matched_mode);
        EXPECT_EQ(w.counters.sparse_matched_refactors, 0);
#endif
    }
}

// Unperturbed platform numerics, dense-arm numerics and matched numerics are never judged.
TEST(QpPlatformMatched, TheCheckAppliesOnlyToPerturbedPlatformNumerics) {
    const F3 f = coupled_f3(1e10);
    const QpOptions o = mu_free();
    const KktAssembly a = assemble_kkt(f.qp, WorkingSet(3, 0), o);
    Vec rhs(5);
    rhs << -f.qp.g, f.qp.be;
    const Vec garbage = Vec::Constant(5, 1.0);

    KktFactor dense(0, DensePolicy::kAlways);
    const auto fd = factorize_checked(dense, a.K, DenseShiftSite{3});
    ASSERT_EQ(fd.arm, FactorArm::kDense);
    EXPECT_FALSE(platform_solve_check_applies(dense));
    EXPECT_FALSE(platform_solve_failed(dense, a.K, rhs, garbage));

    KktFactor k(0, DensePolicy::kNever);
    const auto fo = factorize_checked(k, a.K, DenseShiftSite{3});
    ASSERT_EQ(fo.arm, FactorArm::kSparse);
#ifndef __APPLE__
    ASSERT_GT(*fo.inertia.perturbed_pivots, 0);
    EXPECT_TRUE(platform_solve_check_applies(k));
    EXPECT_TRUE(platform_solve_failed(k, a.K, rhs, garbage));
    EXPECT_FALSE(platform_solve_failed(k, a.K, rhs, f.ystar));
    // A NaN misses.
    Vec nan = f.ystar;
    nan(4) = std::nan("");
    EXPECT_TRUE(platform_solve_failed(k, a.K, rhs, nan));

    // Matched mode: the next perturbed factorization is retaken, judged no more, and its
    // inertia is read from the matched session (K's: 3 positive, 2 negative).
    enter_matched_mode(k);
    EXPECT_TRUE(k.matched_rebuild_pending);
    const auto fm = factorize_checked(k, a.K, DenseShiftSite{3});
    EXPECT_FALSE(k.matched_rebuild_pending);
    EXPECT_TRUE(k.matched_numerics);
    EXPECT_EQ(k.matched_refactors, 1);
    EXPECT_FALSE(platform_solve_check_applies(k));
    EXPECT_EQ(fm.inertia.positive, 3);
    EXPECT_EQ(fm.inertia.negative, 2);
    EXPECT_EQ(kkt_inertia(k).positive, 3);
    EXPECT_EQ(kkt_inertia(k).negative, 2);
    EXPECT_LE(worst_relerr(solve_vec(k, rhs), f.ystar), 1e-14);
#else
    EXPECT_FALSE(fo.inertia.perturbed_pivots.has_value());
    EXPECT_FALSE(platform_solve_check_applies(k));
    EXPECT_FALSE(platform_solve_failed(k, a.K, rhs, garbage));
#endif
}

#ifndef __APPLE__
// Matched mode is sticky but retakes only PERTURBED platform factorizations: an
// unperturbed one (s = 1, uncoupled) keeps the platform numerics bit for bit.
TEST(QpPlatformMatched, MatchedModeLeavesAnUnperturbedFactorizationOnThePlatform) {
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
    enter_matched_mode(k);
    const auto fo = factorize_checked(k, a.K, DenseShiftSite{2});
    (void)factorize_checked(ref, a.K, DenseShiftSite{2});
    ASSERT_EQ(*fo.inertia.perturbed_pivots, 0);
    EXPECT_FALSE(k.matched_numerics);
    EXPECT_EQ(k.matched_refactors, 0);
    const Vec rhs = Vec::Constant(2, 1.0);
    EXPECT_EQ(solve_vec(k, rhs), solve_vec(ref, rhs));
}
#endif
