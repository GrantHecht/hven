#include <algorithm>
#include <cmath>
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
// ULPs.
constexpr double kTightRelTol = 1e-13;

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

// Singular matrix: [[1,1],[1,1]] is exactly rank-1 and symmetric.
// Eliminating with a11=1 as the first pivot leaves an exact zero in the
// trailing 1x1 submatrix (1 - (1*1/1)*1 == 0), so dsytrf detects a genuine
// zero pivot rather than merely being fed a degenerate (all-zero) input.
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
