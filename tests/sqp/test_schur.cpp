// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>
#include <hven/detail/kkt/schur_complement.h>
using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;

// K0 = [H Aᵀ; A 0] with H = diag(2,3), A = [1 1]. Saddle point: inertia (2,1,0).
// (Repeated verbatim from the dissolved test_kkt_system.cpp's make_kkt3().)
// hven's SymmetricFactor validates the STRUCTURAL diagonal at analyze() --
// every row's diagonal entry must be present in the pattern, value zero or
// not (symmetric_factor.h; the engine's own assemblies emit it
// unconditionally). The dissolved seam forwarded the pattern unvalidated, so
// these dense-built fixtures could drop the zero diagonal via sparseView();
// this builder keeps it explicit, exactly as test_kkt_calls.cpp's fixture
// does.
static SpMatRM upper_with_structural_diag(const Eigen::MatrixXd &D) {
    SpMatRM K(D.rows(), D.cols());
    for (Eigen::Index r = 0; r < D.rows(); ++r) {
        for (Eigen::Index c = r; c < D.cols(); ++c) {
            if (r == c || D(r, c) != 0.0) {
                K.insert(r, c) = D(r, c);
            }
        }
    }
    K.makeCompressed();
    return K;
}

static SpMatRM make_kkt3() {
    Eigen::MatrixXd D(3, 3);
    D << 2, 0, 1, 0, 3, 1, 1, 1, 0;
    return upper_with_structural_diag(D);
}

TEST(Schur, BorderedSolveMatchesDirectFactorization) {
    // Border with one constraint row a = (0,1,0), i.e. additionally pin
    // variable 1. Compare against factorizing the 4x4 system directly.
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);
    Vec v(3);
    v << 0, 1, 0;
    schur.add_border(v, -opts.dual_mu);
    Vec rhs(4);
    rhs << 1, 2, 0.5, 0;
    Vec x = schur.solve(rhs);

    Eigen::MatrixXd K4 = Eigen::MatrixXd::Zero(4, 4);
    K4.topLeftCorner(3, 3) = Eigen::MatrixXd(K0).selfadjointView<Eigen::Upper>();
    K4.block(0, 3, 3, 1) = v;
    K4.block(3, 0, 1, 3) = v.transpose();
    K4(3, 3) = -opts.dual_mu;
    Vec x_ref = K4.fullPivLu().solve(rhs);
    EXPECT_LT((x - x_ref).norm(), 1e-9);

    schur.drop_border(0);
    EXPECT_EQ(schur.dim(), 0);
}

TEST(Schur, RefactorizationTriggerFires) {
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    opts.schur_cap = 2;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);

    Vec v0(3);
    v0 << 1, 0, 0;
    Vec v1(3);
    v1 << 0, 1, 0;
    Vec v2(3);
    v2 << 0, 0, 1;

    schur.add_border(v0, 1.0);
    EXPECT_FALSE(schur.needs_refactorization());
    schur.add_border(v1, 2.0);
    EXPECT_FALSE(schur.needs_refactorization());
    schur.add_border(v2, 3.0); // dim() == 3 > schur_cap == 2
    EXPECT_TRUE(schur.needs_refactorization());
}

TEST(Schur, DropBorderMatchesDirectFactorizationOfSurvivor) {
    // Add two borders, drop the first, and check solve() matches a 4x4
    // system directly factorized from K0 + the SECOND border only.
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);

    Vec v1(3);
    v1 << 0, 1, 0;
    Vec v2(3);
    v2 << 1, 0, 0;
    schur.add_border(v1, -opts.dual_mu);
    schur.add_border(v2, -opts.dual_mu);
    ASSERT_EQ(schur.dim(), 2);

    schur.drop_border(0); // removes v1; v2 becomes the sole survivor
    ASSERT_EQ(schur.dim(), 1);

    Vec rhs(4);
    rhs << 1, 2, 0.5, 0;
    Vec x = schur.solve(rhs);

    Eigen::MatrixXd K4 = Eigen::MatrixXd::Zero(4, 4);
    K4.topLeftCorner(3, 3) = Eigen::MatrixXd(K0).selfadjointView<Eigen::Upper>();
    K4.block(0, 3, 3, 1) = v2;
    K4.block(3, 0, 1, 3) = v2.transpose();
    K4(3, 3) = -opts.dual_mu;
    Vec x_ref = K4.fullPivLu().solve(rhs);
    EXPECT_LT((x - x_ref).norm(), 1e-9);
}

TEST(Schur, DropBorderThrowsOnOutOfRange) {
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);
    EXPECT_THROW(schur.drop_border(0), std::out_of_range);

    Vec v(3);
    v << 0, 1, 0;
    schur.add_border(v, -opts.dual_mu);
    EXPECT_THROW(schur.drop_border(1), std::out_of_range);
    EXPECT_THROW(schur.drop_border(-1), std::out_of_range);
}

TEST(Schur, IndefiniteSchurSolveMatchesDirect) {
    // Two borders producing a genuinely indefinite (mixed-sign eigenvalues)
    // 2x2 Schur complement: v1 pins variable 1 with d1=-mu (as in the tests
    // above); v2=(1,0,1) carries a large positive d2=5.0, coupled to v1
    // off-diagonally through K0^-1. By hand (see task-8-report.md, Fix round
    // 1): C ~= [[-0.20000001, -0.2], [-0.2, 4.8]], eig(C) ~= [-0.208, 4.808]
    // -- one negative, one positive eigenvalue, neither block trivial.
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);

    Vec v1(3);
    v1 << 0, 1, 0;
    Vec v2(3);
    v2 << 1, 0, 1;
    const double d1 = -opts.dual_mu;
    const double d2 = 5.0;
    schur.add_border(v1, d1);
    schur.add_border(v2, d2);
    ASSERT_EQ(schur.dim(), 2);

    Vec rhs(5);
    rhs << 1, 2, 0.5, 0.3, -0.7;
    Vec x = schur.solve(rhs);

    Eigen::MatrixXd K5 = Eigen::MatrixXd::Zero(5, 5);
    K5.topLeftCorner(3, 3) = Eigen::MatrixXd(K0).selfadjointView<Eigen::Upper>();
    K5.block(0, 3, 3, 1) = v1;
    K5.block(3, 0, 1, 3) = v1.transpose();
    K5.block(0, 4, 3, 1) = v2;
    K5.block(4, 0, 1, 3) = v2.transpose();
    K5(3, 3) = d1;
    K5(4, 4) = d2;
    Vec x_ref = K5.fullPivLu().solve(rhs);
    EXPECT_LT((x - x_ref).norm(), 1e-8);

    // True inertia of C, computed independently of SchurComplement via
    // SelfAdjointEigenSolver on the same 2x2 matrix built by hand.
    Eigen::MatrixXd K0d = Eigen::MatrixXd(K0).selfadjointView<Eigen::Upper>();
    Eigen::MatrixXd K0inv = K0d.inverse();
    Eigen::Matrix2d C;
    C(0, 0) = d1 - v1.dot(K0inv * v1);
    C(1, 1) = d2 - v2.dot(K0inv * v2);
    C(0, 1) = C(1, 0) = -v1.dot(K0inv * v2);
    Eigen::SelfAdjointEigenSolver<Eigen::Matrix2d> eig(C);
    const Index true_neg = (eig.eigenvalues().array() < 0.0).count();
    ASSERT_EQ(true_neg, 1); // sanity-check the hand algebra above
    EXPECT_EQ(schur.expected_neg_eigs_delta(), true_neg);
}

TEST(Schur, AntiDiagonalSchurBlock) {
    // Construct borders so C is (bit-for-bit) exactly anti-diagonal: pick
    // d_i = v_i^T K0^-1 v_i via the SAME K0 solve calls add_border
    // uses internally, so each diagonal entry of C cancels to exactly 0.0,
    // leaving C = [[0, c], [c, 0]] with c = -v1^T K0^-1 v2 ~= 0.2. This is
    // the pattern the reviewer found Eigen::LDLT silently mis-solves
    // (info()==NumericalIssue, garbage vectorD(), solve() wrong with no
    // exception raised) -- Bunch-Kaufman must instead form a 2x2 pivot block
    // here and solve it correctly (dim 2 is small enough that a 2x2 block
    // spans the WHOLE of C).
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);

    Vec v1(3);
    v1 << 1, 0, 0;
    Vec v2(3);
    v2 << 0, 1, 0;
    const double d1 = v1.dot(detail::solve_vec(kkt, v1));
    const double d2 = v2.dot(detail::solve_vec(kkt, v2));

    SchurComplement schur(kkt, opts);
    schur.add_border(v1, d1);
    schur.add_border(v2, d2);
    ASSERT_EQ(schur.dim(), 2);
    ASSERT_FALSE(schur.needs_refactorization());

    Vec rhs(5);
    rhs << 1, -2, 0.5, 0.25, -0.4;
    Vec x = schur.solve(rhs);

    Eigen::MatrixXd K5 = Eigen::MatrixXd::Zero(5, 5);
    K5.topLeftCorner(3, 3) = Eigen::MatrixXd(K0).selfadjointView<Eigen::Upper>();
    K5.block(0, 3, 3, 1) = v1;
    K5.block(3, 0, 1, 3) = v1.transpose();
    K5.block(0, 4, 3, 1) = v2;
    K5.block(4, 0, 1, 3) = v2.transpose();
    K5(3, 3) = d1;
    K5(4, 4) = d2;
    Vec x_ref = K5.fullPivLu().solve(rhs);
    EXPECT_LT((x - x_ref).norm(), 1e-8);

    // True inertia: 1 positive, 1 negative (the anti-diagonal 2x2 block has
    // eigenvalues +-c).
    EXPECT_EQ(schur.expected_neg_eigs_delta(), 1);
}

TEST(Schur, SingularOneByOneSchurBlockThrowsOnInertiaQuery) {
    // F3 regression: pick d == v^T K0^-1 v (the SAME computation add_border
    // does internally, as AntiDiagonalSchurBlock above already relies on) so
    // C = d - v^T K0^-1 v cancels to EXACTLY 0.0 for a single 1x1 border.
    // LAPACKE_dsytrf reports a zero pivot (info > 0) for that, so
    // needs_refactorization() must report true, and expected_neg_eigs_delta()
    // -- which used to silently return 0 for any singular C, a plausible but
    // WRONG "no negative eigenvalues" answer for a matrix with no usable
    // factorization -- must instead throw, matching solve()'s own behavior on
    // a singular C.
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);

    Vec v(3);
    v << 1, 0, 0;
    const double d = v.dot(detail::solve_vec(kkt, v)); // makes C = d - v^T K0^-1 v == 0.0 exactly

    SchurComplement schur(kkt, opts);
    schur.add_border(v, d);
    ASSERT_EQ(schur.dim(), 1);

    EXPECT_TRUE(schur.needs_refactorization());
    EXPECT_THROW(schur.expected_neg_eigs_delta(), std::runtime_error);
}

// M3 final review (FX-1 + FX-7b). A non-finite border makes C non-finite, and
// WHAT THE DENSE FACTOR DOES WITH THAT IS BACKEND-SPECIFIC -- which is the
// whole reason this fixture is split rather than shared:
//
//   * On MKL, LAPACKE screens the matrix for non-finite entries BEFORE calling
//     dsytrf and reports it as info < 0 -- an illegal ARGUMENT, not a fact
//     about A -- so DenseSymmetricFactor::try_factorize THROWS. That throw
//     escaping rebuild_schur() is the only non-allocation path that produces a
//     nonempty stack with no cached block evidence and singular_ == false:
//     FX-1's state, and the one add_border must roll back out of (FX-7b).
//
//   * On Apple there is no LAPACKE at all. hven's shim
//     (hven/detail/linear/lapacke_shim.h) reproduces only LAPACKE's workspace
//     management and deliberately NOT its NaN pre-screen, so dsytrf_ sees the
//     non-finite C itself and reports info > 0 -- an exact zero pivot -- which
//     try_factorize DEMOTES to kExactlySingular rather than throwing. The
//     border is committed and the state reached is the ordinary singular_ one.
//     OBSERVED on the macOS CI lane (M3 final review), not assumed: this arm
//     asserts a control-flow outcome that lane executed, and no Accelerate
//     float.
//
// So FX-1's new no-evidence branch is covered here on MKL only. On Apple it
// stays reachable, but only through allocation failure inside rebuild_schur
// (the C allocation, or the shim's own workspace vector), which puts it in the
// same untestable-without-allocator-injection class as FX-7(a) and FX-8.
//
// What BOTH backends owe, and what the shared tail below pins: whichever of
// the two states the backend produced, no evidence reader may dereference the
// absent evidence, and every one of them must tell the caller to stop
// bordering this factorization.
TEST(Schur, NonFiniteBorderNeverLeavesAReadableEvidenceState) {
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);

    Vec v1(3);
    v1 << 0, 1, 0;
    schur.add_border(v1, -opts.dual_mu);
    ASSERT_EQ(schur.dim(), 1);
    ASSERT_FALSE(schur.needs_refactorization());

    Vec v2(3);
    v2 << 1, 0, 0;
    const double nan_d = std::numeric_limits<double>::quiet_NaN();

#if defined(__APPLE__)
    schur.add_border(v2, nan_d);
    EXPECT_EQ(schur.dim(), 2) << "the demoted-to-singular add commits its border";
#else
    EXPECT_THROW(schur.add_border(v2, nan_d), std::runtime_error);
    EXPECT_EQ(schur.dim(), 1) << "the failed add left no border behind";

    // Pinned by MESSAGE, not just by type: it is what distinguishes THIS state
    // (a rebuild that threw) from the exactly-singular one the readers already
    // handled, and so what proves the reader was reached through the
    // no-evidence branch rather than through singular_.
    try {
        (void)schur.expected_neg_eigs_delta();
        FAIL() << "expected_neg_eigs_delta() must refuse a stack with no usable factorization";
    } catch (const std::runtime_error &e) {
        EXPECT_NE(std::string(e.what()).find("threw before producing block evidence"),
                  std::string::npos)
            << e.what();
    }
#endif

    EXPECT_TRUE(std::isinf(schur.cond_estimate()));
    EXPECT_FALSE(schur.nearly_singular());
    EXPECT_TRUE(schur.needs_refactorization())
        << "there is no usable cached factorization, so the caller must rebuild K0 rather than "
           "keep bordering";
    EXPECT_THROW((void)schur.expected_neg_eigs_delta(), std::runtime_error);

    // The surviving borders are still real borders at their original indices:
    // dropping from the top empties the stack and restores a healthy one, which
    // it could not do if the add had skewed the three arrays against each other.
    while (schur.dim() > 0) {
        schur.drop_border(schur.dim() - 1);
    }
    EXPECT_EQ(schur.dim(), 0);
    EXPECT_FALSE(schur.needs_refactorization());
}

TEST(Schur, InertiaBookkeepingPinnedVariable) {
    // Hand algebra (see task-8-report.md): with K0 as above and v = (0,1,0),
    // d = -mu, C = d - v^T K0^-1 v is a 1x1 matrix equal to
    // -1e-8 - 0.2 ~= -0.2000000, i.e. strictly negative -- so C's LDLT has
    // exactly one negative diagonal entry.
    SpMatRM K0 = make_kkt3();
    QpOptions opts;
    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);
    SchurComplement schur(kkt, opts);
    Vec v(3);
    v << 0, 1, 0;
    schur.add_border(v, -opts.dual_mu);
    EXPECT_EQ(schur.expected_neg_eigs_delta(), 1);
}

// cond_estimate() is a pure RATIO of largest to smallest |block eigenvalue|,
// so at dim 1 it is structurally blind: max_abs == min_abs and the answer is
// exactly 1.0 however small the block is. A 1x1 Schur complement drifting
// toward zero would therefore sail past schur_cond_max right up to the moment
// LAPACKE_dsytrf reports an exact zero pivot -- and the solves it produces are
// meaningless long before that. needs_refactorization() must catch it on the
// magnitude of the block itself.
TEST(Schur, NearlySingularOneByOneNeedsRefactorization) {
    QpOptions opts;
    Eigen::MatrixXd K0d(1, 1);
    K0d << 1.0;
    SpMatRM K0 = K0d.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    K0.makeCompressed();

    detail::KktFactor kkt;
    detail::factorize_checked(kkt, K0);

    // C = d - v^T K0^-1 v = (1 + eps) - 1 = eps, a single 1x1 block.
    Vec v(1);
    v << 1.0;

    SchurComplement healthy(kkt, opts);
    healthy.add_border(v, 1.0 + 0.5); // C = 0.5: perfectly ordinary
    EXPECT_FALSE(healthy.needs_refactorization());

    SchurComplement sick(kkt, opts);
    sick.add_border(v, 1.0 + 1e-15);             // C ~ 1e-15: numerically singular
    EXPECT_DOUBLE_EQ(sick.cond_estimate(), 1.0); // the ratio is blind, as designed
    EXPECT_TRUE(sick.needs_refactorization());   // ... and the magnitude check is not
}
