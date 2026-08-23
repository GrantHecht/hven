// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Phase-B growth of DenseSymmetricFactor: Triangle selection,
// try_factorize()'s outcome-returning entry, and BunchKaufmanBlockEvidence.
//
// Kept separate from test_dense_symmetric_factor.cpp on purpose. That file
// is the pre-growth contract's witness -- it was written against the
// single-argument, throw-on-singular surface and is left byte-for-byte
// untouched by the change that grew the surface, so its continued passing
// is evidence that nothing under an existing caller moved. The pins BELOW
// re-assert that preservation from the outside (same throw, same message
// shape, same numbers out of factorize(A) as out of the new
// factorize(A, kUpper)), so the preservation claim does not rest on
// "nobody edited that file" alone.

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <Eigen/Eigenvalues>
#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/linear/dense_symmetric_factor.h"

namespace {

using hven::Index;
using hven::Mat;
using hven::Vec;
using hven::linear::BunchKaufmanBlockEvidence;
using hven::linear::DenseFactorizeOutcome;
using hven::linear::DenseSymmetricFactor;
using hven::linear::Triangle;

// Same tolerance rationale as the pre-growth suite: these fixtures are
// exact small rationals that dsytrf/dsytrs round-trip to within a few ULPs.
constexpr double kTightRelTol = 1e-14;

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

// A symmetric indefinite 3x3 with a genuinely mixed inertia (2 positive, 1
// negative -- eigenvalues ~ {4.30, 2.05, -3.35}), used as the general-case
// fixture below. Nonsingular, so every triangle/entry-point arm has a
// well-defined answer to compare against.
Mat indefinite3x3() {
    Mat A(3, 3);
    A << 4, 1, 0, 1, -3, 1, 0, 1, 2;
    return A;
}

// Exactly rank-1 and symmetric: dsytrf finds a real zero pivot here (the
// pre-growth suite's own singular fixture, reused deliberately so the
// throwing and non-throwing entry points are compared on ONE matrix).
Mat singular2x2() {
    Mat A(2, 2);
    A << 1, 1, 1, 1;
    return A;
}

// Negative-eigenvalue count from Eigen's own symmetric eigensolver: an
// oracle INDEPENDENT of the Bunch-Kaufman block walk under test, so a pin
// against it is not the walk checking itself. Sylvester's law of inertia
// guarantees the block walk must agree with it exactly (a nonsingular
// symmetric matrix has the same inertia as the D of any of its
// congruence-factorizations), for either triangle.
Index eigen_neg_count(const Mat &A) {
    Eigen::SelfAdjointEigenSolver<Mat> es(A);
    EXPECT_EQ(es.info(), Eigen::Success);
    Index neg = 0;
    for (Index i = 0; i < es.eigenvalues().size(); ++i) {
        if (es.eigenvalues()(i) < 0.0) {
            ++neg;
        }
    }
    return neg;
}

std::vector<double> sorted_abs_eigs(const BunchKaufmanBlockEvidence &e) {
    std::vector<double> v = e.block_abs_eigs;
    std::sort(v.begin(), v.end());
    return v;
}

} // namespace

// --- Existing-contract preservation ----------------------------------------

// The new overload with kUpper must be the single-argument call, not merely
// something like it: same triangle, same numbers, same evidence. If these
// ever diverge, every pre-growth pin in the sibling suite is describing a
// path its callers no longer take.
TEST(DenseSymmetricFactorGrowth, UpperOverloadIsTheSingleArgumentCall) {
    const Mat A = indefinite3x3();
    Mat RHS(3, 2);
    RHS << 1, 0, 2, 1, -1, 4;

    DenseSymmetricFactor implicit_upper;
    implicit_upper.factorize(A);
    ASSERT_TRUE(implicit_upper.factorized());
    EXPECT_EQ(implicit_upper.triangle(), Triangle::kUpper); // the default, unstated

    DenseSymmetricFactor explicit_upper;
    explicit_upper.factorize(A, Triangle::kUpper);
    ASSERT_TRUE(explicit_upper.factorized());
    EXPECT_EQ(explicit_upper.triangle(), Triangle::kUpper);

    Mat X_implicit(3, 2);
    implicit_upper.solve(RHS, X_implicit);
    Mat X_explicit(3, 2);
    explicit_upper.solve(RHS, X_explicit);
    // BIT-identical, not merely close: same routine, same uplo, same input.
    for (Index i = 0; i < 3; ++i) {
        for (Index j = 0; j < 2; ++j) {
            EXPECT_DOUBLE_EQ(X_implicit(i, j), X_explicit(i, j)) << "at (" << i << ", " << j << ")";
        }
    }

    const auto ev_implicit = implicit_upper.block_evidence();
    const auto ev_explicit = explicit_upper.block_evidence();
    ASSERT_TRUE(ev_implicit.has_value());
    ASSERT_TRUE(ev_explicit.has_value());
    EXPECT_EQ(ev_implicit->neg_eigs, ev_explicit->neg_eigs);
    ASSERT_EQ(ev_implicit->block_abs_eigs.size(), ev_explicit->block_abs_eigs.size());
    for (std::size_t i = 0; i < ev_implicit->block_abs_eigs.size(); ++i) {
        EXPECT_DOUBLE_EQ(ev_implicit->block_abs_eigs[i], ev_explicit->block_abs_eigs[i]);
    }
}

// The throw-on-exact-singularity contract is UNCHANGED for both throwing
// entry points: same exception type, same message shape (prefix, positive
// info code, and the singular-cause clause), same factorized() == false.
// Pinned as a full string comparison around the info digits rather than a
// substring probe, because "same message shape" is the actual promise made
// to the two existing consumers.
TEST(DenseSymmetricFactorGrowth, FactorizeStillThrowsOnExactSingularity) {
    const Mat A = singular2x2();

    for (const bool use_overload : {false, true}) {
        DenseSymmetricFactor f;
        bool threw = false;
        try {
            if (use_overload) {
                f.factorize(A, Triangle::kUpper);
            } else {
                f.factorize(A);
            }
        } catch (const std::runtime_error &e) {
            threw = true;
            const std::string msg = e.what();
            const std::string prefix = "DenseSymmetricFactor::factorize: LAPACKE_dsytrf failed, "
                                       "info=";
            const std::string suffix = " (matrix is exactly singular (zero pivot))";
            ASSERT_GE(msg.size(), prefix.size() + suffix.size()) << msg;
            EXPECT_EQ(msg.substr(0, prefix.size()), prefix) << msg;
            EXPECT_EQ(msg.substr(msg.size() - suffix.size()), suffix) << msg;
            const std::string digits =
                msg.substr(prefix.size(), msg.size() - prefix.size() - suffix.size());
            EXPECT_GT(std::stoi(digits), 0) << msg;
        }
        EXPECT_TRUE(threw) << "use_overload=" << use_overload;
        EXPECT_FALSE(f.factorized());
        EXPECT_EQ(f.dim(), 2); // the attempt's dim is still reported
        EXPECT_FALSE(f.block_evidence().has_value());
    }
}

// The lower triangle takes the same throw, with the same message: the
// non-throwing behavior lives ONLY on try_factorize, never on a triangle.
TEST(DenseSymmetricFactorGrowth, FactorizeLowerAlsoThrowsOnExactSingularity) {
    DenseSymmetricFactor f;
    EXPECT_THROW({ f.factorize(singular2x2(), Triangle::kLower); }, std::runtime_error);
    EXPECT_FALSE(f.factorized());
}

// Validation failures keep their type on the new overload, and name the
// entry point the caller actually used.
TEST(DenseSymmetricFactorGrowth, OverloadValidationThrowsAndNamesItsEntryPoint) {
    DenseSymmetricFactor f;

    Mat non_square(2, 3);
    non_square.setZero();
    try {
        f.factorize(non_square, Triangle::kLower);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        // The overload's diagnostics name factorize(), exactly as the
        // single-argument form's do -- the message a caller sees does not
        // depend on which of the two spellings it used.
        EXPECT_EQ(std::string(e.what()),
                  "DenseSymmetricFactor::factorize: matrix must be square, got 2x3");
    }

    Mat empty(0, 0);
    EXPECT_THROW({ f.factorize(empty, Triangle::kLower); }, std::invalid_argument);

    try {
        (void)f.try_factorize(non_square, Triangle::kLower);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string msg = e.what();
        EXPECT_NE(msg.find("DenseSymmetricFactor::try_factorize:"), std::string::npos) << msg;
    }
}

// --- try_factorize ---------------------------------------------------------

// The outcome path: an exactly singular matrix is REPORTED, not thrown --
// and the object is left in exactly the state a failed factorize leaves it
// in (not factorized, no evidence, solve still throws). That last part is
// the border stack's own contract: it keeps the singular factorization as
// state and refuses to solve against it.
TEST(DenseSymmetricFactorGrowth, TryFactorizeReportsExactSingularityWithoutThrowing) {
    DenseSymmetricFactor f;
    DenseFactorizeOutcome outcome = DenseFactorizeOutcome::kOk;
    EXPECT_NO_THROW({ outcome = f.try_factorize(singular2x2(), Triangle::kLower); });
    EXPECT_EQ(outcome, DenseFactorizeOutcome::kExactlySingular);

    EXPECT_FALSE(f.factorized());
    EXPECT_EQ(f.dim(), 2);
    EXPECT_EQ(f.triangle(), Triangle::kLower);
    EXPECT_FALSE(f.block_evidence().has_value());

    Mat RHS = Mat::Ones(2, 1);
    Mat X(2, 1);
    EXPECT_THROW({ f.solve(RHS, X); }, std::runtime_error);
}

// The success path: kOk leaves a factorization every bit as usable as
// factorize()'s, and numerically identical to it on the same triangle.
TEST(DenseSymmetricFactorGrowth, TryFactorizeOkMatchesFactorize) {
    const Mat A = indefinite3x3();
    Mat RHS(3, 1);
    RHS << 1, 2, -1;

    DenseSymmetricFactor thrown_form;
    thrown_form.factorize(A, Triangle::kLower);
    Mat X_thrown(3, 1);
    thrown_form.solve(RHS, X_thrown);

    DenseSymmetricFactor try_form;
    EXPECT_EQ(try_form.try_factorize(A, Triangle::kLower), DenseFactorizeOutcome::kOk);
    ASSERT_TRUE(try_form.factorized());
    Mat X_try(3, 1);
    try_form.solve(RHS, X_try);

    for (Index i = 0; i < 3; ++i) {
        EXPECT_DOUBLE_EQ(X_try(i, 0), X_thrown(i, 0)) << "at row " << i;
    }
    EXPECT_EQ(try_form.block_evidence()->neg_eigs, thrown_form.block_evidence()->neg_eigs);
}

// A successful factorization followed by a singular one must not leave the
// earlier factorization readable: the state a caller sees after
// kExactlySingular is "nothing usable here", not the previous matrix's.
TEST(DenseSymmetricFactorGrowth, TryFactorizeSingularClearsAnEarlierUsableFactorization) {
    DenseSymmetricFactor f;
    f.factorize(indefinite3x3());
    ASSERT_TRUE(f.factorized());
    ASSERT_TRUE(f.block_evidence().has_value());

    EXPECT_EQ(f.try_factorize(singular2x2(), Triangle::kUpper),
              DenseFactorizeOutcome::kExactlySingular);
    EXPECT_FALSE(f.factorized());
    EXPECT_FALSE(f.block_evidence().has_value());
    EXPECT_EQ(f.dim(), 2);

    Mat RHS = Mat::Ones(3, 1);
    Mat X(3, 1);
    EXPECT_THROW({ f.solve(RHS, X); }, std::runtime_error); // not the stale 3x3 solve
}

// --- Triangle plumbing -----------------------------------------------------

// Proof the triangle argument actually selects which half LAPACK reads, and
// is not merely stored: the OTHER half of each fixture is filled with NaN,
// so a factorization that read it would poison every number downstream.
TEST(DenseSymmetricFactorGrowth, EachTriangleReadsOnlyItsOwnHalf) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Mat A = indefinite3x3();

    const Vec x_true = (Vec(3) << 1.0, 2.0, 3.0).finished();
    const Vec b = A * x_true;

    Mat lower_only = A; // strict upper triangle poisoned
    lower_only(0, 1) = nan;
    lower_only(0, 2) = nan;
    lower_only(1, 2) = nan;

    Mat upper_only = A; // strict lower triangle poisoned
    upper_only(1, 0) = nan;
    upper_only(2, 0) = nan;
    upper_only(2, 1) = nan;

    DenseSymmetricFactor lower;
    lower.factorize(lower_only, Triangle::kLower);
    ASSERT_TRUE(lower.factorized());
    EXPECT_EQ(lower.triangle(), Triangle::kLower);
    Mat X_lower(3, 1);
    lower.solve(b, X_lower);
    expect_mat_near(X_lower, x_true, kTightRelTol);
    const auto ev_lower = lower.block_evidence();
    ASSERT_TRUE(ev_lower.has_value());
    for (const double e : ev_lower->block_abs_eigs) {
        EXPECT_TRUE(std::isfinite(e)) << "block evidence read the poisoned triangle";
    }

    DenseSymmetricFactor upper;
    upper.factorize(upper_only, Triangle::kUpper);
    ASSERT_TRUE(upper.factorized());
    Mat X_upper(3, 1);
    upper.solve(b, X_upper);
    expect_mat_near(X_upper, x_true, kTightRelTol);
    const auto ev_upper = upper.block_evidence();
    ASSERT_TRUE(ev_upper.has_value());
    for (const double e : ev_upper->block_abs_eigs) {
        EXPECT_TRUE(std::isfinite(e)) << "block evidence read the poisoned triangle";
    }
}

// Triangle-overload equivalence on a fully symmetric fixture: the two
// triangles describe the SAME matrix, so the solved system and the inertia
// must agree.
//
// What they agree on is stated precisely, because "identical results" is
// not true of everything here: Bunch-Kaufman with 'U' eliminates from the
// last column and with 'L' from the first, so the pivot sequences and the
// resulting D blocks genuinely differ, and the per-block eigenvalue LISTS
// are not required to match entry for entry. What IS invariant is the
// solved system (to solve accuracy) and the negative-eigenvalue count
// (exactly -- Sylvester's law).
TEST(DenseSymmetricFactorGrowth, UpperAndLowerAgreeOnSolveAndInertia) {
    const Mat A = indefinite3x3();
    Mat RHS(3, 2);
    RHS << 1, 0, 2, 1, -1, 4;

    DenseSymmetricFactor upper;
    upper.factorize(A, Triangle::kUpper);
    Mat X_upper(3, 2);
    upper.solve(RHS, X_upper);

    DenseSymmetricFactor lower;
    lower.factorize(A, Triangle::kLower);
    Mat X_lower(3, 2);
    lower.solve(RHS, X_lower);

    expect_mat_near(X_lower, X_upper, kTightRelTol);
    // ... and both actually solve A x = rhs, so agreeing is not agreeing on
    // the same wrong answer.
    expect_mat_near(A * X_upper, RHS, kTightRelTol);

    const auto ev_upper = upper.block_evidence();
    const auto ev_lower = lower.block_evidence();
    ASSERT_TRUE(ev_upper.has_value());
    ASSERT_TRUE(ev_lower.has_value());
    EXPECT_EQ(ev_upper->neg_eigs, ev_lower->neg_eigs);
    EXPECT_EQ(ev_upper->neg_eigs, eigen_neg_count(A));
}

// A factorization's triangle is per-attempt state, exactly like dim(): a
// later single-argument factorize() must not decode against the previous
// call's triangle. Poisoned halves again, so a stale triangle produces NaN
// rather than a subtly different number.
TEST(DenseSymmetricFactorGrowth, RefactorizeResetsTheTriangle) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const Mat A = indefinite3x3();
    const Vec x_true = (Vec(3) << 1.0, 2.0, 3.0).finished();
    const Vec b = A * x_true;

    Mat lower_only = A;
    lower_only(0, 1) = nan;
    lower_only(0, 2) = nan;
    lower_only(1, 2) = nan;

    Mat upper_only = A;
    upper_only(1, 0) = nan;
    upper_only(2, 0) = nan;
    upper_only(2, 1) = nan;

    DenseSymmetricFactor f;
    f.factorize(lower_only, Triangle::kLower);
    ASSERT_EQ(f.triangle(), Triangle::kLower);

    f.factorize(upper_only); // single-argument: back to kUpper
    EXPECT_EQ(f.triangle(), Triangle::kUpper);
    Mat X(3, 1);
    f.solve(b, X);
    expect_mat_near(X, x_true, kTightRelTol);
}

// A foreign value cast into the enum is caught at the boundary rather than
// handed to LAPACK as an unknown uplo char.
TEST(DenseSymmetricFactorGrowth, UnknownTriangleValueThrows) {
    const auto bogus = static_cast<Triangle>(7);
    const Mat A = indefinite3x3();

    DenseSymmetricFactor f;
    EXPECT_THROW({ f.factorize(A, bogus); }, std::invalid_argument);
    EXPECT_THROW({ (void)f.try_factorize(A, bogus); }, std::invalid_argument);
    EXPECT_FALSE(f.factorized());
}

// --- Block evidence --------------------------------------------------------

// Evidence is absent, not empty, before anything has been factorized -- the
// absent-never-zero rule InertiaEvidence follows. An empty list with a zero
// count would read as "no negative eigenvalues", which does not follow from
// a factorization that never ran.
TEST(DenseSymmetricFactorGrowth, EvidenceIsAbsentBeforeAnyFactorization) {
    const DenseSymmetricFactor f;
    EXPECT_FALSE(f.block_evidence().has_value());
}

// Known all-1x1 fixture with exact block values: a diagonal matrix's
// Bunch-Kaufman factorization IS the matrix, so the per-block |eigenvalue|s
// are its diagonal entries exactly (in whatever order pivoting produces),
// and the negative count is the number of negative diagonal entries.
TEST(DenseSymmetricFactorGrowth, EvidenceOnDiagonalFixtureIsExact) {
    Mat A(3, 3);
    A << 2, 0, 0, 0, -3, 0, 0, 0, 4;

    for (const Triangle t : {Triangle::kUpper, Triangle::kLower}) {
        DenseSymmetricFactor f;
        f.factorize(A, t);
        const auto ev = f.block_evidence();
        ASSERT_TRUE(ev.has_value());

        const std::vector<double> got = sorted_abs_eigs(*ev);
        ASSERT_EQ(got.size(), 3u); // one entry per dimension, not per block
        EXPECT_DOUBLE_EQ(got[0], 2.0);
        EXPECT_DOUBLE_EQ(got[1], 3.0);
        EXPECT_DOUBLE_EQ(got[2], 4.0);
        EXPECT_EQ(ev->neg_eigs, 1);
    }
}

// Known 2x2-block fixture: [[0,1],[1,0]] has a zero diagonal, so
// Bunch-Kaufman is FORCED to take a 2x2 pivot -- this is the fixture that
// exercises the trace/determinant/discriminant decode rather than the 1x1
// arm. Eigenvalues are exactly {+1, -1}.
TEST(DenseSymmetricFactorGrowth, EvidenceDecodesAForced2x2Block) {
    Mat A(2, 2);
    A << 0, 1, 1, 0;

    for (const Triangle t : {Triangle::kUpper, Triangle::kLower}) {
        DenseSymmetricFactor f;
        f.factorize(A, t);
        const auto ev = f.block_evidence();
        ASSERT_TRUE(ev.has_value());
        ASSERT_EQ(ev->block_abs_eigs.size(), 2u);
        EXPECT_NEAR(ev->block_abs_eigs[0], 1.0, kTightRelTol);
        EXPECT_NEAR(ev->block_abs_eigs[1], 1.0, kTightRelTol);
        EXPECT_EQ(ev->neg_eigs, 1);
    }
}

// General fixture, checked against two independent facts: the block
// eigenvalues' product is |det A| (D is congruent to A through a unit
// triangular factor, whose determinant is 1), and the negative count
// matches Eigen's symmetric eigensolver.
TEST(DenseSymmetricFactorGrowth, EvidenceMatchesDeterminantAndIndependentInertia) {
    const Mat A = indefinite3x3();
    const double det = A.determinant();

    for (const Triangle t : {Triangle::kUpper, Triangle::kLower}) {
        DenseSymmetricFactor f;
        f.factorize(A, t);
        const auto ev = f.block_evidence();
        ASSERT_TRUE(ev.has_value());
        ASSERT_EQ(ev->block_abs_eigs.size(), 3u);

        double product = 1.0;
        for (const double e : ev->block_abs_eigs) {
            product *= e;
        }
        EXPECT_NEAR(product, std::abs(det), 1e-12 * std::abs(det));
        EXPECT_EQ(ev->neg_eigs, eigen_neg_count(A));
    }
}

// A larger indefinite fixture that mixes 1x1 and 2x2 pivots (its leading
// 2x2 has a zero diagonal, forcing a 2x2 block; the trailing entries admit
// 1x1 pivots), so one walk covers both arms in sequence rather than one arm
// per fixture.
TEST(DenseSymmetricFactorGrowth, EvidenceOnAMixedBlockFixture) {
    Mat A(4, 4);
    A << 0, 1, 0, 0, 1, 0, 0, 0, 0, 0, 3, 0, 0, 0, 0, -5;

    for (const Triangle t : {Triangle::kUpper, Triangle::kLower}) {
        DenseSymmetricFactor f;
        f.factorize(A, t);
        const auto ev = f.block_evidence();
        ASSERT_TRUE(ev.has_value());
        ASSERT_EQ(ev->block_abs_eigs.size(), 4u);

        const std::vector<double> got = sorted_abs_eigs(*ev);
        EXPECT_NEAR(got[0], 1.0, kTightRelTol);
        EXPECT_NEAR(got[1], 1.0, kTightRelTol);
        EXPECT_NEAR(got[2], 3.0, kTightRelTol);
        EXPECT_NEAR(got[3], 5.0, kTightRelTol);
        // Eigenvalues are {+1, -1, 3, -5}: two negative.
        EXPECT_EQ(ev->neg_eigs, 2);
        EXPECT_EQ(ev->neg_eigs, eigen_neg_count(A));
    }
}

// A 2x2 block that is NOT the first block, which is what separates the two
// uplo conventions' pivot bookkeeping: LAPACK records a 2x2 block as an
// adjacent equal-negative pivot PAIR, spanning (k-1, k) under 'U' and
// (k, k+1) under 'L'. The walk scans forward and treats the first negative
// entry it meets as the block's top-left index, which is correct under
// both -- but only a fixture whose 2x2 block starts past index 0
// distinguishes that from an off-by-one pairing. Here a mispaired walk
// would decode [[3,0],[0,0]] plus a 1x1 zero, i.e. |eigs| {3, 0, 0} and
// zero negatives, instead of the true {3, 1, 1} with one negative.
TEST(DenseSymmetricFactorGrowth, EvidenceDecodesA2x2BlockPastTheFirstIndex) {
    Mat A(3, 3);
    A << 3, 0, 0, 0, 0, 1, 0, 1, 0; // eigenvalues {3, +1, -1}

    for (const Triangle t : {Triangle::kUpper, Triangle::kLower}) {
        DenseSymmetricFactor f;
        f.factorize(A, t);
        const auto ev = f.block_evidence();
        ASSERT_TRUE(ev.has_value());
        ASSERT_EQ(ev->block_abs_eigs.size(), 3u);

        const std::vector<double> got = sorted_abs_eigs(*ev);
        EXPECT_NEAR(got[0], 1.0, kTightRelTol);
        EXPECT_NEAR(got[1], 1.0, kTightRelTol);
        EXPECT_NEAR(got[2], 3.0, kTightRelTol);
        EXPECT_EQ(ev->neg_eigs, 1);
        EXPECT_EQ(ev->neg_eigs, eigen_neg_count(A));
    }
}

// Evidence tracks the CURRENT factorization: after a failed factorize it is
// absent even though a previous successful one is still the last thing this
// object factored.
TEST(DenseSymmetricFactorGrowth, EvidenceIsAbsentAfterAFailedFactorize) {
    DenseSymmetricFactor f;
    f.factorize(indefinite3x3());
    ASSERT_TRUE(f.block_evidence().has_value());

    EXPECT_THROW({ f.factorize(singular2x2()); }, std::runtime_error);
    EXPECT_FALSE(f.block_evidence().has_value());
}
