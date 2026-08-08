#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/linear/dense_symmetric_factor.h"

namespace {

using hven::Index;
using hven::Mat;
using hven::linear::DenseSymmetricFactor;

// Relative tolerance for the analytic (known-inverse / known-solution)
// cases below, all of which are exact rational values representable to
// full double precision -- dsytrf/dsytrs round-trip them to within a few
// ULPs, so the brief's 1e-14 rel bound (not a looser one) holds in
// practice; verified by running this suite at 1e-14 before committing it.
constexpr double kTightRelTol = 1e-14;

// |actual - expected| <= rel_tol * max(1, |expected|) -- a relative bound
// for |expected| >= 1, falling back to an absolute bound of rel_tol itself
// for |expected| < 1 (several fixtures below have expected entries in
// (0, 1), e.g. 1/4, 1/3, 1/2, 3/4, where a pure relative bound would demand
// unreasonably more digits than a raw double affords no benefit for).
// rel_tol == 1e-14 either way, so the absolute floor this degenerates to
// is already tight.
void expect_mat_near(const Mat &actual, const Mat &expected, double rel_tol) {
    ASSERT_EQ(actual.rows(), expected.rows());
    ASSERT_EQ(actual.cols(), expected.cols());
    for (Index i = 0; i < actual.rows(); ++i) {
        for (Index j = 0; j < actual.cols(); ++j) {
            const double e = expected(i, j);
            const double scale = std::max(1.0, std::abs(e));
            EXPECT_NEAR(actual(i, j), e, rel_tol * scale) << "at (" << i << ", " << j << ")";
        }
    }
}

} // namespace

// SPD 2x2, known inverse: the n=2 discrete-Laplacian tridiag(-1,2,-1)
// matrix, whose closed-form inverse is (A^-1)_{ij} = min(i,j)*(n+1-max(i,j))/(n+1)
// (1-indexed). Solving against the identity reproduces A^-1 directly.
TEST(DenseSymmetricFactor, Spd2x2ReproducesKnownInverse) {
    Mat A(2, 2);
    A << 2, -1, -1, 2;

    Mat expected_inv(2, 2);
    expected_inv << 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0;

    DenseSymmetricFactor f;
    f.factorize(A);
    EXPECT_TRUE(f.factorized());
    EXPECT_EQ(f.dim(), 2);

    Mat X(2, 2);
    f.solve(Mat::Identity(2, 2), X);
    expect_mat_near(X, expected_inv, kTightRelTol);
}

// SPD 3x3, known inverse: the n=3 member of the same tridiag(-1,2,-1)
// family.
TEST(DenseSymmetricFactor, Spd3x3ReproducesKnownInverse) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    Mat expected_inv(3, 3);
    expected_inv << 3.0 / 4.0, 1.0 / 2.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 1.0 / 2.0, 1.0 / 4.0,
        1.0 / 2.0, 3.0 / 4.0;

    DenseSymmetricFactor f;
    f.factorize(A);
    EXPECT_TRUE(f.factorized());
    EXPECT_EQ(f.dim(), 3);

    Mat X(3, 3);
    f.solve(Mat::Identity(3, 3), X);
    expect_mat_near(X, expected_inv, kTightRelTol);
}

// Symmetric INDEFINITE 3x3 -- this component's actual use case (the
// Schur-complement border it will later sit under is not generally
// definite). The (0,0) diagonal entry is exactly zero, forcing dsytrf's
// Bunch-Kaufman pivoting to take a 2x2 block for the leading 2x2
// submatrix [[0,1],[1,0]] (eigenvalues +-1) rather than a 1x1 pivot;
// combined with the trailing -1 the full matrix has eigenvalues {1,-1,-1}
// -- genuinely indefinite, not merely non-SPD by construction.
TEST(DenseSymmetricFactor, SymmetricIndefinite3x3SolvesKnownSystem) {
    Mat A(3, 3);
    A << 0, 1, 0, 1, 0, 0, 0, 0, -1;

    const hven::Vec x_true = (hven::Vec(3) << 1.0, 2.0, 3.0).finished();
    const hven::Vec b = A * x_true;

    DenseSymmetricFactor f;
    f.factorize(A);
    ASSERT_TRUE(f.factorized());

    Mat X(3, 1);
    f.solve(b, X);
    expect_mat_near(X, x_true, kTightRelTol);
}

// Multi-RHS solve: a single solve() call over several RHS columns must
// match what solving each column individually produces.
TEST(DenseSymmetricFactor, MultiRhsSolveMatchesPerColumnSolve) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    Mat RHS(3, 3);
    RHS << 1, 0, 5, 2, 1, -3, -1, 4, 2;

    DenseSymmetricFactor f;
    f.factorize(A);

    Mat X_multi(3, 3);
    f.solve(RHS, X_multi);

    for (Index col = 0; col < 3; ++col) {
        Mat X_single(3, 1);
        f.solve(RHS.col(col), X_single);
        expect_mat_near(X_multi.col(col), X_single, kTightRelTol);
    }
}

// Identity matrix: solving I x = rhs must reproduce rhs exactly (up to
// rounding), and I's own inverse is I.
TEST(DenseSymmetricFactor, IdentityMatrixSolveReproducesRhs) {
    Mat I = Mat::Identity(4, 4);

    DenseSymmetricFactor f;
    f.factorize(I);

    Mat RHS(4, 2);
    RHS << 1, 5, 2, 6, 3, 7, 4, 8;

    Mat X(4, 2);
    f.solve(RHS, X);
    expect_mat_near(X, RHS, kTightRelTol);
}

// Non-contiguous X: the border-stack consumers this component exists for
// will typically solve directly into a sub-block of a larger owned matrix
// rather than into a freshly allocated one, so solve()'s copy-through-
// scratch fallback (taken whenever X.outerStride() != X.rows()) needs its
// own coverage, not just the always-contiguous X used by every other test
// above.
//
// A column-offset block of a same-height parent (e.g. big.block(0, 1, n,
// k) inside an n x (k+2) matrix) is deceptively still CONTIGUOUS by this
// class's own definition: consecutive columns of a column-major matrix
// are back-to-back in memory regardless of which column range is sliced,
// as long as every row is included, so outerStride() == rows() there and
// the fast in-place path fires. Confirmed directly against Eigen before
// writing this test (a column-offset block reports
// outerStride()==rows()==3 for a 3x(3+2) parent). What genuinely breaks
// contiguity in column-major storage is restricting ROWS: a block that
// takes only the first n rows out of a TALLER parent has each column's
// live data contiguous internally (innerStride()==1, so it still binds to
// MatRef) but consecutive columns are parent.rows() apart, not n apart --
// exactly what Eigen::Ref<Mat>'s Dynamic OuterStride exists to represent,
// and exactly the case this test constructs.
TEST(DenseSymmetricFactor, NonContiguousXTakesScratchPathAndMatchesContiguousResult) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    Mat RHS(3, 2);
    RHS << 1, 0, 2, 1, -1, 4;

    DenseSymmetricFactor f;
    f.factorize(A);

    Mat X_contiguous(3, 2);
    f.solve(RHS, X_contiguous);

    // Taller parent (5 rows) than the block written into (3 rows): the
    // block's outerStride is the PARENT's row count (5), not its own (3).
    Mat big = Mat::Constant(5, 2, std::numeric_limits<double>::quiet_NaN());
    auto X_block = big.block(0, 0, 3, 2);
    ASSERT_NE(X_block.outerStride(), X_block.rows())
        << "test fixture does not actually exercise the non-contiguous path";

    f.solve(RHS, X_block);
    expect_mat_near(X_block, X_contiguous, kTightRelTol);
}

// Zero-column RHS: X.cols() == 0 is a valid (degenerate) shape match
// against an equally-empty RHS and must not be handed to LAPACK as an
// empty/null buffer -- solve() returns immediately in this case.
TEST(DenseSymmetricFactor, ZeroColumnRhsIsAcceptedAndNoOp) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    DenseSymmetricFactor f;
    f.factorize(A);

    Mat RHS(3, 0);
    Mat X(3, 0);
    EXPECT_NO_THROW({ f.solve(RHS, X); });
    EXPECT_EQ(X.rows(), 3);
    EXPECT_EQ(X.cols(), 0);
}

// --- Error paths -----------------------------------------------------------

TEST(DenseSymmetricFactor, SolveBeforeFactorizeThrows) {
    DenseSymmetricFactor f;
    Mat RHS = Mat::Identity(2, 2);
    Mat X(2, 2);
    EXPECT_THROW({ f.solve(RHS, X); }, std::runtime_error);
}

TEST(DenseSymmetricFactor, NonSquareFactorizeThrows) {
    DenseSymmetricFactor f;
    Mat A(2, 3);
    A.setZero();
    EXPECT_THROW({ f.factorize(A); }, std::invalid_argument);
}

TEST(DenseSymmetricFactor, EmptyFactorizeThrows) {
    DenseSymmetricFactor f;
    Mat A(0, 0);
    EXPECT_THROW({ f.factorize(A); }, std::invalid_argument);
}

TEST(DenseSymmetricFactor, RhsRowCountMismatchThrows) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    DenseSymmetricFactor f;
    f.factorize(A);

    Mat RHS_wrong_rows(2, 1);
    RHS_wrong_rows << 1, 1;
    Mat X(2, 1);
    EXPECT_THROW({ f.solve(RHS_wrong_rows, X); }, std::invalid_argument);
}

TEST(DenseSymmetricFactor, XShapeMismatchThrows) {
    Mat A(3, 3);
    A << 2, -1, 0, -1, 2, -1, 0, -1, 2;

    DenseSymmetricFactor f;
    f.factorize(A);

    Mat RHS(3, 1);
    RHS << 1, 1, 1;
    Mat X_wrong(2, 1); // wrong row count relative to RHS
    EXPECT_THROW({ f.solve(RHS, X_wrong); }, std::invalid_argument);

    Mat X_wrong_cols(3, 2); // wrong column count relative to RHS
    EXPECT_THROW({ f.solve(RHS, X_wrong_cols); }, std::invalid_argument);
}

// Singular matrix: [[1,1],[1,1]] is exactly rank-1 and symmetric -- a
// genuinely singular input (not a degenerate all-zero matrix), so dsytrf
// is expected to detect a real zero pivot somewhere in its Bunch-Kaufman
// elimination (which pivot ordering it picks, 1x1 or 2x2, is an algorithm
// detail this test does not assume).
TEST(DenseSymmetricFactor, SingularMatrixFactorizeThrowsWithInfoCode) {
    Mat A(2, 2);
    A << 1, 1, 1, 1;

    DenseSymmetricFactor f;
    bool threw = false;
    try {
        f.factorize(A);
    } catch (const std::runtime_error &e) {
        threw = true;
        const std::string msg = e.what();
        EXPECT_NE(msg.find("info="), std::string::npos) << msg;
        // The reported info code must be positive (LAPACKE's "exact zero
        // pivot" convention), not the "-i illegal argument" convention.
        const auto pos = msg.find("info=");
        ASSERT_NE(pos, std::string::npos);
        const int info = std::stoi(msg.substr(pos + 5));
        EXPECT_GT(info, 0);
    }
    EXPECT_TRUE(threw);
    EXPECT_FALSE(f.factorized());
}

// --- Refactorize-reuse -------------------------------------------------

TEST(DenseSymmetricFactor, RefactorizeWithDifferentDimLeavesNoStaleState) {
    DenseSymmetricFactor f;

    Mat A2(2, 2);
    A2 << 2, -1, -1, 2;
    f.factorize(A2);
    ASSERT_EQ(f.dim(), 2);

    Mat X2(2, 2);
    f.solve(Mat::Identity(2, 2), X2);
    Mat expected_inv2(2, 2);
    expected_inv2 << 2.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0, 2.0 / 3.0;
    expect_mat_near(X2, expected_inv2, kTightRelTol);

    Mat A3(3, 3);
    A3 << 2, -1, 0, -1, 2, -1, 0, -1, 2;
    f.factorize(A3);
    ASSERT_EQ(f.dim(), 3);

    Mat X3(3, 3);
    f.solve(Mat::Identity(3, 3), X3);
    Mat expected_inv3(3, 3);
    expected_inv3 << 3.0 / 4.0, 1.0 / 2.0, 1.0 / 4.0, 1.0 / 2.0, 1.0, 1.0 / 2.0, 1.0 / 4.0,
        1.0 / 2.0, 3.0 / 4.0;
    expect_mat_near(X3, expected_inv3, kTightRelTol);

    // Old dim's RHS shape must now be rejected -- dim() genuinely moved,
    // not merely reported a stale value.
    Mat RHS_old_dim = Mat::Identity(2, 2);
    Mat X_old_dim(2, 2);
    EXPECT_THROW({ f.solve(RHS_old_dim, X_old_dim); }, std::invalid_argument);
}
