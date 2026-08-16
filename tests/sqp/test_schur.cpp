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
