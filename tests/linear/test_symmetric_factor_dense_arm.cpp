// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The dense arm behind SymmetricFactor (M8 DN-1; docs/notes/2026-11-m8-dn1-design.md §13,
// T1-T8 and T11). Written against the public surface plus the two detail helpers the design
// names (the block walk and the size check), on whichever backend this build targets: the
// fallback session is Pardiso on MKL and Accelerate on Apple, and every expectation about it is
// "equal to a kNever factor's", never a backend-specific number.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Cholesky>
#include <Eigen/Eigenvalues>
#include <gtest/gtest.h>

#include "hven/core/errors.h"
#include "hven/core/types.h"
#include "hven/detail/kkt/kkt_calls.h"
#include "hven/detail/linear/dense_session.h"
#include "hven/linear/symmetric_factor.h"

namespace {

using hven::Index;
using hven::Mat;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::DensePolicy;
using hven::linear::FactorArm;
using hven::linear::FactorizeOutcome;
using hven::linear::InertiaEvidence;
using hven::linear::SymmetricFactor;

SymmetricFactor::Options dense_options() {
    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kAlways;
    return o;
}

// Upper-triangle CSR of a dense symmetric matrix: every diagonal entry and every nonzero
// upper entry is stored.
SpMatRM upper_csr(const Mat &m) {
    const Index n = m.rows();
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (i == j || m(i, j) != 0.0) {
                t.emplace_back(static_cast<int>(i), static_cast<int>(j), m(i, j));
            }
        }
    }
    SpMatRM A(n, n);
    A.setFromTriplets(t.begin(), t.end());
    A.makeCompressed();
    return A;
}

// A DN-0 KKT dump converted to text (tests/linear/fixtures/dn1/): n nnz, the row pointer, the
// column indices, the values as hex floats.
SpMatRM load_fixture(const std::string &name) {
    const std::string path = std::string(HVEN_DN1_FIXTURES) + "/" + name + ".txt";
    std::ifstream f(path);
    if (!f) {
        throw std::runtime_error("cannot open fixture " + path);
    }
    std::string line;
    std::getline(f, line); // provenance comment
    Index n = 0;
    Index nnz = 0;
    f >> n >> nnz;
    SpMatRM A(n, n);
    A.reserve(nnz);
    A.makeCompressed();
    A.resizeNonZeros(nnz);
    for (Index i = 0; i <= n; ++i) {
        f >> A.outerIndexPtr()[i];
    }
    for (Index k = 0; k < nnz; ++k) {
        f >> A.innerIndexPtr()[k];
    }
    for (Index k = 0; k < nnz; ++k) {
        std::string tok;
        f >> tok;
        A.valuePtr()[k] = std::strtod(tok.c_str(), nullptr);
    }
    if (!f) {
        throw std::runtime_error("short fixture " + path);
    }
    return A;
}

Mat dense_of(const SpMatRM &A) {
    Mat m = Mat::Zero(A.rows(), A.cols());
    for (Index i = 0; i < A.outerSize(); ++i) {
        for (SpMatRM::InnerIterator it(A, i); it; ++it) {
            m(it.row(), it.col()) = it.value();
            m(it.col(), it.row()) = it.value();
        }
    }
    return m;
}

void expect_outcome_equal(const FactorizeOutcome &a, const FactorizeOutcome &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.backend_code, b.backend_code);
    EXPECT_EQ(a.arm, b.arm);
    EXPECT_EQ(a.inertia.state, b.inertia.state);
    EXPECT_EQ(a.inertia.positive, b.inertia.positive);
    EXPECT_EQ(a.inertia.negative, b.inertia.negative);
    EXPECT_EQ(a.inertia.zero, b.inertia.zero);
    EXPECT_EQ(a.inertia.zero_is_derived, b.inertia.zero_is_derived);
    EXPECT_EQ(a.inertia.perturbed_pivots, b.inertia.perturbed_pivots);
    EXPECT_EQ(a.factor.factor_nonzeros, b.factor.factor_nonzeros);
    EXPECT_EQ(a.factor.factor_mflops, b.factor.factor_mflops);
    EXPECT_EQ(a.factor.factor_size_bytes, b.factor.factor_size_bytes);
    EXPECT_EQ(a.factor.dense_factor_entries, b.factor.dense_factor_entries);
}

// The design's T1 fixture set.
std::vector<Mat> t1_fixtures() {
    std::vector<Mat> out;
    Mat diag = Mat::Zero(4, 4);
    diag.diagonal() << 3.0, -2.0, 5.0, -7.0;
    out.push_back(diag);
    Mat swap(2, 2);
    swap << 0.0, 1.0, 1.0, 0.0; // forces a 2x2 pivot
    out.push_back(swap);
    // A KKT system [H A'; A 0] with a zero (2,2) block.
    Mat kkt = Mat::Zero(5, 5);
    kkt.topLeftCorner(3, 3) << 4.0, 1.0, 0.0, 1.0, 3.0, 0.5, 0.0, 0.5, 2.0;
    kkt(3, 0) = kkt(0, 3) = 1.0;
    kkt(3, 1) = kkt(1, 3) = 1.0;
    kkt(4, 1) = kkt(1, 4) = -1.0;
    kkt(4, 2) = kkt(2, 4) = 2.0;
    out.push_back(kkt);
    // HS071's KKT at x = (1, 5, 5, 1) with unit multipliers (n = 4, two constraints).
    Mat hs = Mat::Zero(6, 6);
    hs.topLeftCorner(4, 4) << 4.0, 7.0, 2.0, 12.0, 7.0, 2.0, 1.0, 1.0, 2.0, 1.0, 2.0, 1.0, 12.0,
        1.0, 1.0, 2.0;
    const double g1[4] = {25.0, 5.0, 5.0, 25.0}; // grad of x1 x2 x3 x4
    const double g2[4] = {2.0, 10.0, 10.0, 2.0}; // grad of sum x_i^2
    for (int j = 0; j < 4; ++j) {
        hs(4, j) = hs(j, 4) = g1[j];
        hs(5, j) = hs(j, 5) = g2[j];
    }
    out.push_back(hs);
    return out;
}

// ---------------------------------------------------------------------------------------
// T1: the lifecycle under kAlways matches kNever. Both arms are backward stable; on the HS071
// KKT fixture their solutions differ by up to 2.9e-12 relative (conditioning, not error), so
// solutions are compared at kArmRelTol and each dense residual at 1e-12.
constexpr double kArmRelTol = 1e-10;

TEST(DenseArm, LifecycleMatchesTheSparseArm) {
    for (const Mat &m : t1_fixtures()) {
        const SpMatRM A = upper_csr(m);
        SymmetricFactor dense(dense_options());
        SymmetricFactor sparse(SymmetricFactor::Options{});
        dense.analyze(A);
        sparse.analyze(A);
        const FactorizeOutcome od = dense.factorize(A);
        const FactorizeOutcome os = sparse.factorize(A);
        ASSERT_EQ(od.status, FactorizeOutcome::Status::kOk);
        ASSERT_EQ(od.arm, FactorArm::kDense);
        ASSERT_EQ(os.arm, FactorArm::kSparse);

        const Index n = m.rows();
        // single RHS
        Vec b = Vec::LinSpaced(n, 1.0, static_cast<double>(n));
        Vec xd(n);
        Vec xs(n);
        dense.solve(b, xd);
        sparse.solve(b, xs);
        EXPECT_LE((xd - xs).lpNorm<Eigen::Infinity>(),
                  kArmRelTol * std::max(1.0, xs.lpNorm<Eigen::Infinity>()));
        EXPECT_LE((m * xd - b).lpNorm<Eigen::Infinity>(), 1e-12 * b.lpNorm<Eigen::Infinity>());

        // multi RHS, contiguous
        Mat B = Mat::Random(n, 3);
        Mat Xd(n, 3);
        Mat Xs(n, 3);
        dense.solve(B, Xd);
        sparse.solve(B, Xs);
        EXPECT_LE((Xd - Xs).cwiseAbs().maxCoeff(),
                  kArmRelTol * std::max(1.0, Xs.cwiseAbs().maxCoeff()));
        EXPECT_LE((m * Xd - B).cwiseAbs().maxCoeff(),
                  1e-12 * std::max(1.0, B.cwiseAbs().maxCoeff()));

        // zero columns: accepted, no backend call
        const Index solves = dense.counters().solve_count;
        Mat B0(n, 0);
        Mat X0(n, 0);
        dense.solve(B0, X0);
        EXPECT_EQ(dense.counters().solve_count, solves);

        // non-contiguous X: a block of a wider matrix
        Mat wide = Mat::Zero(n + 3, 5);
        dense.solve(B, wide.block(1, 1, n, 3));
        EXPECT_EQ((wide.block(1, 1, n, 3) - Xd).cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(wide.row(0).cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(wide.col(0).cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(wide.col(4).cwiseAbs().maxCoeff(), 0.0);
    }
}

// ---------------------------------------------------------------------------------------
// T2: the evidence of a dense-taken factorization.

TEST(DenseArm, EvidenceIsObservedCountedAndUnperturbed) {
    Mat m(3, 3);
    m << 2.0, 1.0, 0.0, 1.0, -3.0, 1.0, 0.0, 1.0, 4.0;
    const Eigen::SelfAdjointEigenSolver<Mat> es(m);
    Index neg = 0;
    for (Index i = 0; i < 3; ++i) {
        neg += es.eigenvalues()(i) < 0.0 ? 1 : 0;
    }
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(dense_options());
    f.analyze(A);
    const FactorizeOutcome o = f.factorize(A);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(o.arm, FactorArm::kDense);
    EXPECT_EQ(o.backend_code, 0);
    EXPECT_EQ(o.inertia.state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(o.inertia.negative, neg);
    EXPECT_EQ(o.inertia.positive, 3 - neg);
    EXPECT_EQ(o.inertia.zero, 0);
    EXPECT_FALSE(o.inertia.zero_is_derived);
    EXPECT_FALSE(o.inertia.perturbed_pivots.has_value());
    EXPECT_FALSE(o.factor.factor_nonzeros.has_value());
    EXPECT_FALSE(o.factor.factor_mflops.has_value());
    EXPECT_FALSE(o.factor.factor_size_bytes.has_value());
    ASSERT_TRUE(o.factor.dense_factor_entries.has_value());
    EXPECT_EQ(*o.factor.dense_factor_entries, 6);

    // inertia() reads the same evidence back.
    const InertiaEvidence again = f.inertia();
    EXPECT_EQ(again.negative, neg);
    EXPECT_FALSE(again.perturbed_pivots.has_value());
}

// LAKES mat_0013 (DN-0, IPM, n = 168): decidable under the scaled reference with 78 negatives;
// block_evidence()'s former (trace +- disc)/2 decode read 72.
TEST(DenseArm, LakesMatrixCountsTheReferenceNegatives) {
    const SpMatRM A = load_fixture("lakes_ipm_0013");
    ASSERT_EQ(A.rows(), 168);
    SymmetricFactor f(dense_options());
    f.analyze(A);
    const FactorizeOutcome o = f.factorize(A);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(o.arm, FactorArm::kDense);
    EXPECT_EQ(o.inertia.negative, 78);
    EXPECT_EQ(o.inertia.positive, 90);
    EXPECT_EQ(o.inertia.zero, 0);
    EXPECT_EQ(*o.factor.dense_factor_entries, 168 * 169 / 2);
}

// ---------------------------------------------------------------------------------------
// T2b: the block walk on synthetic D blocks (uplo 'U', column-major, leading dimension n).

TEST(DenseArmBlockWalk, IllScaledNegativeDeterminantCountsOneNegative) {
    // [[1e10, 1], [1, 1e-12]]: det = 1e-2 - 1 < 0, |det| << trace^2, so trace^2 - 4 det rounds
    // to trace^2 and (trace - disc)/2 reads exactly 0.
    std::vector<double> a = {1e10, 0.0, 1.0, 1e-12};
    std::vector<int> ipiv = {-1, -1};
    const auto c = hven::linear::detail::dense_block_walk(a.data(), 2, ipiv.data());
    EXPECT_FALSE(c.singular);
    EXPECT_EQ(c.negative, 1);
    EXPECT_EQ(c.positive, 1);
}

TEST(DenseArmBlockWalk, SignsFromDeterminantAndTrace) {
    // 1x1 (-2), 2x2 [[-3, 1], [1, -1]] (det 2 > 0, trace < 0: two negatives), 1x1 (5).
    const Index n = 4;
    std::vector<double> a(16, 0.0);
    a[0 * n + 0] = -2.0;
    a[1 * n + 1] = -3.0;
    a[2 * n + 1] = 1.0; // D(1, 2)
    a[2 * n + 2] = -1.0;
    a[3 * n + 3] = 5.0;
    std::vector<int> ipiv = {1, -2, -2, 4};
    const auto c = hven::linear::detail::dense_block_walk(a.data(), n, ipiv.data());
    EXPECT_FALSE(c.singular);
    EXPECT_EQ(c.negative, 3);
    EXPECT_EQ(c.positive, 1);
}

TEST(DenseArmBlockWalk, ZeroDeterminantIsSingular) {
    std::vector<double> a = {1.0, 0.0, 2.0, 4.0}; // [[1, 2], [2, 4]]
    std::vector<int> ipiv = {-1, -1};
    EXPECT_TRUE(hven::linear::detail::dense_block_walk(a.data(), 2, ipiv.data()).singular);
    std::vector<double> z = {0.0, 0.0, 0.0, 0.0};
    EXPECT_TRUE(hven::linear::detail::dense_block_walk(z.data(), 2, ipiv.data()).singular);
}

TEST(DenseArmBlockWalk, NonFiniteIsSingular) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    std::vector<int> ipiv2 = {-1, -1};
    for (const double bad : {nan, inf, -inf}) {
        for (int slot : {0, 2, 3}) {
            std::vector<double> a = {1.0, 0.0, 3.0, -1.0};
            a[static_cast<std::size_t>(slot)] = bad;
            EXPECT_TRUE(hven::linear::detail::dense_block_walk(a.data(), 2, ipiv2.data()).singular)
                << "slot " << slot;
        }
        std::vector<double> one = {bad};
        std::vector<int> ipiv1 = {1};
        EXPECT_TRUE(hven::linear::detail::dense_block_walk(one.data(), 1, ipiv1.data()).singular);
    }
    // Finite entries whose determinant overflows unscaled: the walk scales, so it is not singular.
    std::vector<double> big = {1e300, 0.0, 1e300, -1e300};
    EXPECT_FALSE(hven::linear::detail::dense_block_walk(big.data(), 2, ipiv2.data()).singular);
}

TEST(DenseArmBlockWalk, MalformedPivotDataThrows) {
    std::vector<double> a = {1.0};
    std::vector<int> ipiv = {-1}; // a 2x2 block opening on the last index
    EXPECT_THROW(hven::linear::detail::dense_block_walk(a.data(), 1, ipiv.data()),
                 std::logic_error);
    // A 2x2 block whose two entries differ is not dsytrf's marking.
    std::vector<double> b = {2.0, 1.0, 1.0, -1.0};
    std::vector<int> ipiv2 = {-1, -2};
    EXPECT_THROW(hven::linear::detail::dense_block_walk(b.data(), 2, ipiv2.data()),
                 std::logic_error);
    std::vector<int> ipiv3 = {-2, 2};
    EXPECT_THROW(hven::linear::detail::dense_block_walk(b.data(), 2, ipiv3.data()),
                 std::logic_error);
}

// ---------------------------------------------------------------------------------------
// T3: exactly singular -> the platform session's outcome, field for field.

TEST(DenseArm, ExactlySingularFallsBackToThePlatformSession) {
    for (const char *name : {"burkehan_sqp_0007", "bt8_ipm_0001"}) {
        SCOPED_TRACE(name);
        const SpMatRM A = load_fixture(name);
        SymmetricFactor dense(dense_options());
        SymmetricFactor sparse(SymmetricFactor::Options{});
        dense.analyze(A);
        sparse.analyze(A);
        const FactorizeOutcome od = dense.factorize(A);
        const FactorizeOutcome os = sparse.factorize(A);
        EXPECT_EQ(od.arm, FactorArm::kSparse);
        expect_outcome_equal(od, os);
        EXPECT_EQ(dense.counters().dense_fallback_count, 1);
        EXPECT_EQ(dense.counters().dense_factorize_count, 0);
        EXPECT_EQ(dense.counters().factorize_count, 1);
        if (od.status == FactorizeOutcome::Status::kOk) {
            const Vec b = Vec::Ones(A.rows());
            Vec xd(A.rows());
            Vec xs(A.rows());
            dense.solve(b, xd);
            sparse.solve(b, xs);
            for (Index i = 0; i < xd.size(); ++i) {
                EXPECT_EQ(xd(i), xs(i));
            }
            EXPECT_EQ(dense.inertia().negative, sparse.inertia().negative);
        }
    }
}

// ---------------------------------------------------------------------------------------
// T4: identity across arms.

TEST(DenseArm, EpochIsMonotoneAndSessionIdsAreDistinctAcrossArms) {
    Mat good(2, 2);
    good << 2.0, 1.0, 1.0, -1.0;
    Mat bad(2, 2);
    bad << 1.0, 0.0, 0.0, 0.0; // a zero pivot: dsytrf info > 0
    const SpMatRM G = upper_csr(good);
    // Same pattern for both: store the (0,1) entry explicitly in `bad`.
    SpMatRM B = G;
    B.coeffRef(0, 0) = 1.0;
    B.coeffRef(0, 1) = 0.0;
    B.coeffRef(1, 1) = 0.0;

    SymmetricFactor f(dense_options());
    f.analyze(G);
    const std::uint64_t dense_id = f.session_id();
    EXPECT_NE(dense_id, 0U);
    EXPECT_EQ(f.epoch(), 0U);

    ASSERT_EQ(f.factorize(G).arm, FactorArm::kDense); // dense, epoch 1
    EXPECT_EQ(f.epoch(), 1U);
    EXPECT_EQ(f.session_id(), dense_id);
    auto h_dense = f.share();

    ASSERT_EQ(f.factorize(B).arm, FactorArm::kSparse); // fallback (fork), epoch 2
    EXPECT_EQ(f.epoch(), 2U);
    const std::uint64_t fb_id = f.session_id();
    EXPECT_NE(fb_id, dense_id);

    ASSERT_EQ(f.factorize(B).arm, FactorArm::kSparse); // fallback (reuse), epoch 3
    EXPECT_EQ(f.epoch(), 3U);
    EXPECT_EQ(f.session_id(), fb_id);
    EXPECT_EQ(f.counters().analyze_count, 1);

    ASSERT_EQ(f.factorize(G).arm, FactorArm::kDense); // dense again, epoch 4
    EXPECT_EQ(f.epoch(), 4U);
    EXPECT_EQ(f.session_id(), dense_id);

    ASSERT_EQ(f.factorize(B).arm, FactorArm::kSparse); // fallback (fork again), epoch 5
    EXPECT_EQ(f.epoch(), 5U);
    EXPECT_NE(f.session_id(), fb_id);
    EXPECT_NE(f.session_id(), dense_id);

    // The handle emitted at epoch 1 names a session whose numerics a singular attempt has
    // since overwritten: refused as a backend failure.
    EXPECT_EQ(h_dense->epoch(), 1U);
    EXPECT_EQ(h_dense->session_id(), dense_id);
    Vec x(2);
    EXPECT_THROW(h_dense->solve(Vec::Ones(2), x), hven::BackendError);

    // analyze() forks the dense session: a new id, the epoch carried forward, and an
    // outstanding handle on the old session keeps solving.
    ASSERT_EQ(f.factorize(G).arm, FactorArm::kDense); // epoch 6
    auto h6 = f.share();
    f.analyze(G);
    EXPECT_NE(f.session_id(), dense_id);
    EXPECT_EQ(f.epoch(), 6U);
    Vec y(2);
    h6->solve(Vec::Ones(2), y);
    EXPECT_LE((good * y - Vec::Ones(2)).lpNorm<Eigen::Infinity>(), 1e-14);
}

// ---------------------------------------------------------------------------------------
// T5: share() / adopt() on dense numerics.

TEST(DenseArm, ShareAndAdoptOnDenseNumerics) {
    Mat m(3, 3);
    m << 4.0, 1.0, 0.0, 1.0, -2.0, 1.0, 0.0, 1.0, 3.0;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(dense_options());
    f.analyze(A);
    ASSERT_EQ(f.factorize(A).arm, FactorArm::kDense);
    auto h = f.share();

    // full reuse
    SymmetricFactor g = SymmetricFactor::adopt(h);
    EXPECT_EQ(g.session_id(), f.session_id());
    EXPECT_EQ(g.epoch(), f.epoch());
    EXPECT_EQ(g.counters().analyze_count, 0);
    Vec x(3);
    g.solve(Vec::Ones(3), x);
    EXPECT_LE((m * x - Vec::Ones(3)).lpNorm<Eigen::Infinity>(), 1e-14);
    EXPECT_EQ(g.inertia().negative, 1);
    EXPECT_FALSE(g.inertia().perturbed_pivots.has_value());

    // the adopter's factorize reuses the dense symbolic and advances the shared epoch
    ASSERT_EQ(g.factorize(A).arm, FactorArm::kDense);
    EXPECT_EQ(g.counters().analyze_count, 0);
    EXPECT_EQ(g.epoch(), h->epoch() + 1);

    // epoch mismatch: an adopter of the stale handle is refused until it factorizes
    SymmetricFactor stale = SymmetricFactor::adopt(h);
    EXPECT_THROW(stale.solve(Vec::Ones(3), x), std::logic_error);
    ASSERT_EQ(stale.factorize(A).arm, FactorArm::kDense);
    stale.solve(Vec::Ones(3), x);

    // a co-owner's exactly singular attempt leaves the dense session without numerics
    auto h2 = stale.share();
    SpMatRM S = A;
    for (Index k = 0; k < S.nonZeros(); ++k) {
        S.valuePtr()[k] = 0.0;
    }
    const FactorizeOutcome fb = stale.factorize(S);
    EXPECT_EQ(fb.arm, FactorArm::kSparse);
    EXPECT_THROW(h2->solve(Vec::Ones(3), x), hven::BackendError);

    // pattern mismatch at the first factorize of an adopted engine
    Mat m2 = Mat::Identity(3, 3);
    SymmetricFactor h3_src(dense_options());
    h3_src.analyze(upper_csr(m2));
    ASSERT_EQ(h3_src.factorize(upper_csr(m2)).arm, FactorArm::kDense);
    SymmetricFactor h3 = SymmetricFactor::adopt(h3_src.share());
    EXPECT_THROW(h3.factorize(A), std::invalid_argument);
}

// T5 (review C270 finding 1): the shared dense session's epoch never rewinds and never repeats
// under co-ownership. The adopter falls back (its own view moves to its fallback session), the
// emitter then advances the shared dense session twice, and the adopter goes dense again
// proposing an epoch the session has already committed: the session takes the next one.
TEST(DenseArm, SharedDenseEpochNeverRewindsUnderCoOwnership) {
    Mat good(2, 2);
    good << 2.0, 1.0, 1.0, -1.0;
    const SpMatRM G = upper_csr(good);
    SpMatRM G2 = G; // same pattern, different numerics
    G2.coeffRef(0, 0) = 5.0;
    SpMatRM B = G; // exactly singular on the same pattern
    B.coeffRef(0, 0) = 1.0;
    B.coeffRef(0, 1) = 0.0;
    B.coeffRef(1, 1) = 0.0;

    std::set<std::pair<std::uint64_t, std::uint64_t>> pairs; // every (session_id, epoch) seen
    const auto record = [&pairs](const SymmetricFactor &f) {
        EXPECT_TRUE(pairs.emplace(f.session_id(), f.epoch()).second)
            << "(session " << f.session_id() << ", epoch " << f.epoch() << ") named twice";
    };

    SymmetricFactor e(dense_options());
    e.analyze(G);
    ASSERT_EQ(e.factorize(G).arm, FactorArm::kDense); // shared dense session, epoch 1
    record(e);
    const std::uint64_t dense_id = e.session_id();
    SymmetricFactor a = SymmetricFactor::adopt(e.share());

    ASSERT_EQ(a.factorize(B).arm, FactorArm::kSparse); // the adopter falls back (fork)
    record(a);
    const std::uint64_t a_prior = a.epoch();
    EXPECT_NE(a.session_id(), dense_id);

    ASSERT_EQ(e.factorize(G).arm, FactorArm::kDense);
    record(e);
    ASSERT_EQ(e.factorize(G).arm, FactorArm::kDense);
    record(e);
    const std::uint64_t e_prior = e.epoch();
    auto h_e = e.share(); // G's numerics at e_prior

    ASSERT_EQ(a.factorize(G2).arm, FactorArm::kDense); // proposes a_prior + 1 <= e_prior
    EXPECT_LE(a_prior + 1, e_prior);
    record(a);
    EXPECT_EQ(a.session_id(), dense_id);
    EXPECT_GT(a.epoch(), e_prior);
    EXPECT_GT(a.epoch(), a_prior);
    EXPECT_EQ(e.epoch(), a.epoch()); // the emitter reads the shared session

    // The handle on G's numerics names (dense_id, e_prior), which the session no longer holds:
    // its adopter is refused rather than handed G2's numerics under G's identity.
    SymmetricFactor stale = SymmetricFactor::adopt(h_e);
    Vec x(2);
    EXPECT_THROW(stale.solve(Vec::Ones(2), x), std::logic_error);
    // The current handle solves G2.
    SymmetricFactor fresh = SymmetricFactor::adopt(a.share());
    fresh.solve(Vec::Ones(2), x);
    EXPECT_LE((dense_of(G2) * x - Vec::Ones(2)).lpNorm<Eigen::Infinity>(), 1e-14);
}

// T5 (review C270 finding 2): adopt() of a handle on the platform session keeps the emitter's
// dense-arm policy. A dense-selected pattern's fallback handle gives an adopter that attempts
// the dense arm at its first factorize(); kAuto's threshold travels with it.
TEST(DenseArm, AdoptOfAFallbackHandleKeepsTheDensePolicy) {
    Mat good(2, 2);
    good << 2.0, 1.0, 1.0, -1.0;
    const SpMatRM G = upper_csr(good);
    SpMatRM B = G;
    B.coeffRef(0, 0) = 1.0;
    B.coeffRef(0, 1) = 0.0;
    B.coeffRef(1, 1) = 0.0;

    SymmetricFactor e(dense_options());
    e.analyze(G);
    ASSERT_EQ(e.factorize(B).arm, FactorArm::kSparse); // the emitter's fallback session
    auto h = e.share();
    SymmetricFactor a = SymmetricFactor::adopt(h);
    EXPECT_EQ(a.session_id(), h->session_id());
    EXPECT_EQ(a.epoch(), h->epoch());

    const FactorizeOutcome od = a.factorize(G);
    EXPECT_EQ(od.arm, FactorArm::kDense);
    EXPECT_EQ(a.counters().analyze_count, 0);
    EXPECT_EQ(a.counters().dense_factorize_count, 1);
    EXPECT_GT(a.epoch(), h->epoch());
    EXPECT_NE(a.session_id(), h->session_id());
    Vec x(2);
    a.solve(Vec::Ones(2), x);
    EXPECT_LE((good * x - Vec::Ones(2)).lpNorm<Eigen::Infinity>(), 1e-14);

    // Its next singular attempt falls back like the emitter's would.
    EXPECT_EQ(a.factorize(B).arm, FactorArm::kSparse);
    EXPECT_EQ(a.counters().dense_fallback_count, 1);
    EXPECT_EQ(a.counters().factorize_count, 2);

    // kAuto with max_dense_dimension = 1: the 2x2 pattern is sparse-selected, and the adopter of
    // its handle selects a 1x1 pattern for the dense arm, as the emitter would.
    SymmetricFactor::Options auto_opts;
    auto_opts.dense_factorization = DensePolicy::kAuto;
    auto_opts.max_dense_dimension = 1;
    SymmetricFactor e2(auto_opts);
    e2.analyze(G);
    ASSERT_EQ(e2.factorize(G).arm, FactorArm::kSparse);
    SymmetricFactor a2 = SymmetricFactor::adopt(e2.share());
    EXPECT_EQ(a2.factorize(G).arm, FactorArm::kSparse);
    Mat one(1, 1);
    one << -3.0;
    a2.analyze(upper_csr(one));
    EXPECT_EQ(a2.factorize(upper_csr(one)).arm, FactorArm::kDense);
}

// Review C270 test gap: Factorization::solve(Mat) and Factorization::inertia() on a dense
// handle, and both after a co-owner's exactly singular attempt emptied the shared session.
TEST(DenseArm, HandleSolveAndInertiaOnDenseNumerics) {
    Mat m(3, 3);
    m << 4.0, 1.0, 0.0, 1.0, -2.0, 1.0, 0.0, 1.0, 3.0;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(dense_options());
    f.analyze(A);
    ASSERT_EQ(f.factorize(A).arm, FactorArm::kDense);
    auto h = f.share();

    Mat rhs(3, 2);
    rhs << 1.0, -2.0, 0.5, 3.0, -1.0, 0.25;
    Mat X(3, 2);
    h->solve(rhs, X);
    EXPECT_LE((m * X - rhs).lpNorm<Eigen::Infinity>(), 1e-14);

    const InertiaEvidence ev = h->inertia();
    EXPECT_EQ(ev.state, InertiaEvidence::State::kObserved);
    EXPECT_EQ(ev.positive, 2);
    EXPECT_EQ(ev.negative, 1);
    EXPECT_EQ(ev.zero, 0);
    EXPECT_FALSE(ev.zero_is_derived);
    EXPECT_FALSE(ev.perturbed_pivots.has_value());

    SymmetricFactor g = SymmetricFactor::adopt(h);
    SpMatRM S = A;
    for (Index k = 0; k < S.nonZeros(); ++k) {
        S.valuePtr()[k] = 0.0;
    }
    ASSERT_EQ(g.factorize(S).arm, FactorArm::kSparse);
    EXPECT_EQ(h->inertia().state, InertiaEvidence::State::kUnavailable);
    EXPECT_THROW(h->solve(rhs, X), hven::BackendError);
}

// ---------------------------------------------------------------------------------------
// T6: no partial solves on dense numerics.

TEST(DenseArm, PartialSolvesAreRefusedOnDenseNumerics) {
    Mat m(2, 2);
    m << 3.0, 1.0, 1.0, 2.0;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(dense_options());
    f.analyze(A);
    EXPECT_FALSE(f.supports_partial_solve());
    ASSERT_EQ(f.factorize(A).arm, FactorArm::kDense);
    EXPECT_FALSE(f.supports_partial_solve());
    Vec x(2);
    EXPECT_THROW(f.solve_partial(SymmetricFactor::SolvePhase::kForward, Vec::Ones(2), x),
                 std::logic_error);
    // a dense solve reports no refinement
    EXPECT_FALSE(f.solve(Vec::Ones(2), x).refinement_steps.has_value());
}

// ---------------------------------------------------------------------------------------
// T7: options validation and the size check.

TEST(DenseArm, OptionsValidation) {
    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kAuto;
    EXPECT_THROW(SymmetricFactor{o}, std::invalid_argument); // no threshold
    o.max_dense_dimension = 0;
    EXPECT_THROW(SymmetricFactor{o}, std::invalid_argument);
    o.max_dense_dimension = 4;
    EXPECT_NO_THROW(SymmetricFactor{o});

    for (DensePolicy p : {DensePolicy::kNever, DensePolicy::kAlways}) {
        SymmetricFactor::Options q;
        q.dense_factorization = p;
        q.max_dense_dimension = 4;
        EXPECT_THROW(SymmetricFactor{q}, std::invalid_argument);
    }

    SymmetricFactor::Options cnr;
    cnr.dense_factorization = DensePolicy::kAlways;
    cnr.conditional_reproducibility_threads = 2;
    cnr.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    EXPECT_THROW(SymmetricFactor{cnr}, std::invalid_argument);
}

TEST(DenseArm, AutoSelectsByDimension) {
    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kAuto;
    o.max_dense_dimension = 3;
    SymmetricFactor f(o);
    const SpMatRM small = upper_csr(Mat::Identity(3, 3));
    const SpMatRM large = upper_csr(Mat::Identity(4, 4));
    f.analyze(small);
    EXPECT_EQ(f.factorize(small).arm, FactorArm::kDense);
    f.analyze(large);
    EXPECT_EQ(f.factorize(large).arm, FactorArm::kSparse);
    f.analyze(small);
    EXPECT_EQ(f.factorize(small).arm, FactorArm::kDense);
}

#ifndef __APPLE__
// A Pardiso-only knob configures the fallback session only; a dense-taken factorization is
// unaffected by it. (On Accelerate those knobs throw at construction, as for kNever.)
TEST(DenseArm, PardisoOnlyKnobsAreInertOnDenseTakenFactorizations) {
    Mat m(3, 3);
    m << 4.0, 1.0, 0.0, 1.0, -2.0, 1.0, 0.0, 1.0, 3.0;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor::Options plain = dense_options();
    SymmetricFactor::Options knobs = dense_options();
    knobs.weighted_matching = true;
    knobs.matrix_scaling = true;
    knobs.pivot_strategy = SymmetricFactor::Options::PivotStrategy::kOneByOne;
    knobs.pivot_perturbation_exponent = 13;
    knobs.max_refinement_steps = 3;
    knobs.collect_factor_mflops = true;
    SymmetricFactor a(plain);
    SymmetricFactor b(knobs);
    a.analyze(A);
    b.analyze(A);
    const FactorizeOutcome oa = a.factorize(A);
    const FactorizeOutcome ob = b.factorize(A);
    expect_outcome_equal(oa, ob);
    Vec xa(3);
    Vec xb(3);
    a.solve(Vec::Ones(3), xa);
    b.solve(Vec::Ones(3), xb);
    for (Index i = 0; i < 3; ++i) {
        EXPECT_EQ(xa(i), xb(i));
    }
}
#endif

TEST(DenseArm, SizeCheckRefusesWhatLapackOrMemoryCannotAddress) {
    using hven::linear::detail::validate_dense_dimension;
    EXPECT_NO_THROW(validate_dense_dimension(1));
    EXPECT_NO_THROW(validate_dense_dimension(2000));
    EXPECT_THROW(validate_dense_dimension(0), std::invalid_argument);
    // Beyond LAPACK's 32-bit integer.
    EXPECT_THROW(validate_dense_dimension(Index{1} << 31), std::invalid_argument);
    // Within LAPACK's integer, but 8 n^2 overflows a 64-bit size (n > 2^30).
    try {
        validate_dense_dimension(Index{1500000000});
        ADD_FAILURE() << "expected a throw";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("8*n^2"), std::string::npos) << e.what();
    }
    // The largest n whose 8 n^2 fits: accepted by the check (nothing is allocated here).
    EXPECT_NO_THROW(validate_dense_dimension(Index{1073741823}));
}

// ---------------------------------------------------------------------------------------
// T8: the pattern guard.

TEST(DenseArm, PatternGuardRunsAsOnTheSparseArm) {
    Mat m(3, 3);
    m << 4.0, 1.0, 0.0, 1.0, -2.0, 1.0, 0.0, 1.0, 3.0;
    const SpMatRM A = upper_csr(m);
    Mat m2 = m;
    m2(0, 2) = m2(2, 0) = 0.5; // one more stored entry: a different pattern
    const SpMatRM A2 = upper_csr(m2);

    SymmetricFactor f(dense_options());
    f.analyze(A);
    EXPECT_THROW(f.factorize(A2), std::invalid_argument);
    EXPECT_EQ(f.counters().pattern_verify_count, 1);
    EXPECT_EQ(f.counters().factorize_count, 0);
    ASSERT_EQ(f.factorize(A, SymmetricFactor::PatternCheck::kAssumeAnalyzed).arm,
              FactorArm::kDense);
    EXPECT_EQ(f.counters().pattern_verify_count, 1);
    EXPECT_EQ(f.counters().factorize_count, 1);
    // Under kAssumeAnalyzed a matrix whose entry count is not the analyzed pattern's is still
    // refused before the scatter (a bounds guard, not a pattern check).
    EXPECT_THROW(f.factorize(A2, SymmetricFactor::PatternCheck::kAssumeAnalyzed),
                 std::invalid_argument);
    EXPECT_EQ(f.counters().factorize_count, 1);
}

// ---------------------------------------------------------------------------------------
// T11: the counter identity on a mixed run.

TEST(DenseArm, CounterIdentityOnAMixedRun) {
    Mat m(2, 2);
    m << 2.0, 1.0, 1.0, -1.0;
    const SpMatRM G = upper_csr(m);
    SpMatRM B = G;
    B.coeffRef(0, 1) = 0.0;
    B.coeffRef(1, 1) = 0.0;
    const SpMatRM L = upper_csr(Mat::Identity(4, 4));

    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kAuto;
    o.max_dense_dimension = 2;
    SymmetricFactor f(o);
    f.analyze(G);
    f.factorize(G); // dense
    f.factorize(B); // fallback
    f.factorize(G); // dense
    f.analyze(L);
    f.factorize(L); // sparse-selected
    f.factorize(L); // sparse-selected
    const auto &c = f.counters();
    EXPECT_EQ(c.dense_factorize_count, 2);
    EXPECT_EQ(c.dense_fallback_count, 1);
    EXPECT_EQ(c.factorize_count, 5);
    EXPECT_EQ(c.factorize_count, c.dense_factorize_count + c.dense_fallback_count + 2);
    EXPECT_EQ(c.analyze_count, 2);
}

// M8 DN-1d: DenseSingularPolicy::kReport. P is positive definite with det 2^-12 against
// ||P|| ~ 2^40 (rcond ~ 1e-28): the pivot factorization takes it with nonzero pivots.
Mat near_singular_pd() {
    Mat m(2, 2);
    m << 1.0, std::ldexp(1.0, 20), std::ldexp(1.0, 20), std::ldexp(1.0, 40) + std::ldexp(1.0, -12);
    return m;
}

SymmetricFactor::Options report_options() {
    SymmetricFactor::Options o = dense_options();
    o.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;
    return o;
}

// An exact-zero column (stored zero diagonal): exactly singular.
Mat zero_column() {
    Mat m = Mat::Zero(2, 2);
    m(0, 0) = 1.0;
    return m;
}

void expect_count_identity(const SymmetricFactor &f) {
    const auto &c = f.counters();
    EXPECT_EQ(c.factorize_count,
              c.dense_factorize_count + c.dense_fallback_count + c.dense_singular_count);
}

TEST(DenseArmReport, FlagsASingularToWorkingPrecisionFactorAndKeepsItsNumerics) {
    const SpMatRM P = upper_csr(near_singular_pd());
    Mat good(2, 2);
    good << 2.0, 1.0, 1.0, -1.0;
    const SpMatRM G = upper_csr(good);

    SymmetricFactor bk(dense_options());
    bk.analyze(P);
    const FactorizeOutcome ob = bk.factorize(P);
    EXPECT_EQ(ob.arm, FactorArm::kDense);
    EXPECT_FALSE(ob.inertia.numerically_singular);
    EXPECT_EQ(bk.counters().dense_condition_estimate_count, 0);

    SymmetricFactor f(report_options());
    f.analyze(P);
    const FactorizeOutcome o = f.factorize(P);
    EXPECT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(o.arm, FactorArm::kDense);
    EXPECT_TRUE(o.inertia.numerically_singular);
    EXPECT_FALSE(o.inertia.perturbed_pivots.has_value());
    EXPECT_EQ(o.inertia.positive, 2);
    EXPECT_TRUE(f.inertia().numerically_singular);
    EXPECT_EQ(f.counters().dense_flagged_count, 1);
    EXPECT_EQ(f.counters().dense_condition_estimate_count, 1);
    EXPECT_EQ(f.counters().dense_fallback_count, 0);
    Vec x(2);
    f.solve(Vec::Ones(2), x); // the dense numerics, kept
    EXPECT_TRUE(x.allFinite());

    // A well-conditioned matrix: taken, not screened, not flagged.
    f.analyze(G);
    const FactorizeOutcome og = f.factorize(G);
    EXPECT_EQ(og.arm, FactorArm::kDense);
    EXPECT_FALSE(og.inertia.numerically_singular);
    EXPECT_EQ(f.counters().dense_condition_estimate_count, 1);
    EXPECT_EQ(f.counters().dense_flagged_count, 1);
    expect_count_identity(f);
}

TEST(DenseArmReport, AnExactlySingularFactorIsReportedNotRetaken) {
    const SpMatRM Z = upper_csr(zero_column());

    SymmetricFactor fallback(dense_options());
    fallback.analyze(Z);
    EXPECT_EQ(fallback.factorize(Z).arm, FactorArm::kSparse);
    EXPECT_EQ(fallback.counters().dense_fallback_count, 1);

    SymmetricFactor f(report_options());
    f.analyze(Z);
    const FactorizeOutcome o = f.factorize(Z);
    EXPECT_EQ(o.status, FactorizeOutcome::Status::kBackendError);
    EXPECT_EQ(o.backend_code, hven::linear::kDenseSingular);
    EXPECT_EQ(o.arm, FactorArm::kDense);
    EXPECT_TRUE(o.inertia.numerically_singular);
    EXPECT_EQ(f.inertia().state, InertiaEvidence::State::kUnavailable);
    Vec x(2);
    EXPECT_THROW(f.solve(Vec::Ones(2), x), std::logic_error);
    EXPECT_EQ(f.counters().dense_singular_count, 1);
    EXPECT_EQ(f.counters().dense_fallback_count, 0);
    expect_count_identity(f);
}

TEST(DenseArmReport, TheEquilibratedRefactorReportsPerturbedPivotsAndSolvesInASpace) {
    const SpMatRM Z = upper_csr(zero_column());
    SymmetricFactor f(report_options());
    f.analyze(Z);
    ASSERT_EQ(f.factorize(Z).status, FactorizeOutcome::Status::kBackendError);
    // D = diag(2, 4): D Z D + sigma S = diag(4 + sigma, -sigma), and a solve returns
    // D (D Z D + sigma S)^-1 D b.
    const double sigma = 1e-6;
    const double d[2] = {2.0, 4.0};
    const FactorizeOutcome o = f.factorize_equilibrated(Z, 1, d, sigma);
    EXPECT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(o.arm, FactorArm::kDense);
    EXPECT_FALSE(o.inertia.numerically_singular);
    ASSERT_TRUE(o.inertia.perturbed_pivots.has_value());
    EXPECT_EQ(*o.inertia.perturbed_pivots, 2);
    EXPECT_EQ(o.inertia.positive, 1);
    EXPECT_EQ(o.inertia.negative, 1);
    ASSERT_TRUE(f.inertia().perturbed_pivots.has_value());
    Vec x(2);
    f.solve(Vec::Ones(2), x);
    EXPECT_NEAR(x(0), 4.0 / (4.0 + sigma), 1e-15);
    EXPECT_NEAR(x(1), -16.0 / sigma, 1e-6);
    EXPECT_EQ(f.counters().dense_shift_refactor_count, 1);
    EXPECT_EQ(f.counters().dense_singular_count, 1);
    expect_count_identity(f);

    // An unshifted factorization clears the perturbed report and the scale.
    const SpMatRM I2 = upper_csr(Mat::Identity(2, 2));
    f.analyze(I2);
    ASSERT_EQ(f.factorize_equilibrated(I2, 1, d, sigma).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(f.factorize(I2).status, FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(f.inertia().perturbed_pivots.has_value());
    f.solve(Vec::Ones(2), x);
    EXPECT_EQ(x(0), 1.0);
    EXPECT_EQ(x(1), 1.0);

    // A flagged equilibrated factorization is reported, not counted in dense_flagged_count.
    const SpMatRM P = upper_csr(near_singular_pd());
    f.analyze(P);
    const double ones[2] = {1.0, 1.0};
    const Index flagged_before = f.counters().dense_flagged_count;
    EXPECT_TRUE(f.factorize_equilibrated(P, 2, ones, 0.0).inertia.numerically_singular);
    EXPECT_EQ(f.counters().dense_flagged_count, flagged_before);

    const double bad[2] = {1.0, 0.0};
    EXPECT_THROW(f.factorize_equilibrated(P, 1, ones, -1.0), std::invalid_argument);
    EXPECT_THROW(f.factorize_equilibrated(P, 3, ones, 1.0), std::invalid_argument);
    EXPECT_THROW(f.factorize_equilibrated(P, 1, ones, std::nan("")), std::invalid_argument);
    EXPECT_THROW(f.factorize_equilibrated(P, 1, nullptr, 1.0), std::invalid_argument);
    EXPECT_THROW(f.factorize_equilibrated(P, 1, bad, 1.0), std::invalid_argument);
    SymmetricFactor bk(dense_options());
    bk.analyze(P);
    EXPECT_THROW(bk.factorize_equilibrated(P, 1, ones, 1.0), std::logic_error);
    SymmetricFactor::Options small = report_options();
    small.dense_factorization = DensePolicy::kAuto;
    small.max_dense_dimension = 1;
    SymmetricFactor sparse(small); // n = 2 is sparse-selected
    sparse.analyze(P);
    EXPECT_THROW(sparse.factorize_equilibrated(P, 1, ones, 1.0), std::logic_error);
    EXPECT_THROW(sparse.retake_on_platform(P), std::logic_error);
}

TEST(DenseArmReport, RetakeOnPlatformTakesTheSparseArmAndCounts) {
    const SpMatRM P = upper_csr(near_singular_pd());
    SymmetricFactor never(SymmetricFactor::Options{});
    never.analyze(P);
    const FactorizeOutcome reference = never.factorize(P);

    SymmetricFactor f(report_options());
    f.analyze(P);
    const FactorizeOutcome o = f.retake_on_platform(P);
    EXPECT_EQ(o.arm, FactorArm::kSparse);
    EXPECT_EQ(o.status, reference.status);
    EXPECT_EQ(f.counters().dense_retake_count, 1);
    EXPECT_EQ(f.counters().dense_fallback_count, 1);
    // The next factorize() attempts the dense arm again.
    EXPECT_EQ(f.factorize(P).arm, FactorArm::kDense);
    expect_count_identity(f);
}

TEST(DenseArmReport, RefusedUnderNeverAndCarriedByHandles) {
    SymmetricFactor::Options never;
    never.dense_singular_policy = hven::linear::DenseSingularPolicy::kReport;
    EXPECT_THROW(SymmetricFactor{never}, std::invalid_argument);

    const SpMatRM P = upper_csr(near_singular_pd());
    // A dense handle: the adopter reads the flag and reports on its own factorizations.
    SymmetricFactor e(report_options());
    e.analyze(P);
    ASSERT_TRUE(e.factorize(P).inertia.numerically_singular);
    SymmetricFactor a = SymmetricFactor::adopt(e.share());
    EXPECT_TRUE(a.inertia().numerically_singular);
    EXPECT_TRUE(a.factorize(P).inertia.numerically_singular);
    EXPECT_EQ(a.counters().dense_flagged_count, 1);
    // A platform-session handle (after a retake) carries the policy too.
    ASSERT_EQ(e.retake_on_platform(P).arm, FactorArm::kSparse);
    SymmetricFactor b = SymmetricFactor::adopt(e.share());
    const FactorizeOutcome ob = b.factorize(P);
    EXPECT_EQ(ob.arm, FactorArm::kDense);
    EXPECT_TRUE(ob.inertia.numerically_singular);
}

// diag(1, d): the rook D-ratio is d, and rcond (1-norm) is d. The screen at 1e-15 and the flag
// at 2^-53 are each bracketed by a factor of 1.25.
TEST(DenseArmReport, TheDetectorBracketsBothThresholds) {
    const double eps2 = std::ldexp(1.0, -53);
    struct Case {
        double d;
        Index estimates;
        bool flagged;
    };
    for (const Case c : {Case{1.25e-15, 0, false}, Case{0.8e-15, 1, false},
                         Case{1.25 * eps2, 1, false}, Case{0.8 * eps2, 1, true}}) {
        Mat m = Mat::Zero(2, 2);
        m(0, 0) = 1.0;
        m(1, 1) = c.d;
        const SpMatRM A = upper_csr(m);
        SymmetricFactor f(report_options());
        f.analyze(A);
        const FactorizeOutcome o = f.factorize(A);
        ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk) << c.d;
        EXPECT_EQ(f.counters().dense_condition_estimate_count, c.estimates) << c.d;
        EXPECT_EQ(o.inertia.numerically_singular, c.flagged) << c.d;
    }
}

TEST(DenseArmReport, TheRookWalkReadsTwoByTwoBlocks) {
    Mat m = Mat::Zero(3, 3);
    m(0, 1) = m(1, 0) = 1.0;
    m(2, 2) = 1.0;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(report_options());
    f.analyze(A);
    const FactorizeOutcome o = f.factorize(A);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(o.inertia.positive, 2);
    EXPECT_EQ(o.inertia.negative, 1);
    EXPECT_FALSE(o.inertia.numerically_singular);
    Vec x(3);
    f.solve(Vec::Ones(3), x);
    EXPECT_LE((dense_of(A) * x - Vec::Ones(3)).lpNorm<Eigen::Infinity>(), 1e-15);

    // A lone negative entry, or one on the last index, is not dsytrf_rook's output.
    const double a[4] = {0.0, 1.0, 1.0, 0.0};
    const int bad_pair[2] = {-1, 2};
    EXPECT_THROW((void)hven::linear::detail::dense_rook_walk(a, 2, bad_pair), std::logic_error);
    const double b[4] = {1.0, 1.0, 1.0, 0.0};
    const int bad_last[2] = {1, -2};
    EXPECT_THROW((void)hven::linear::detail::dense_rook_walk(b, 2, bad_last), std::logic_error);
}

// Badly scaled but well posed (entries over 12 decades, rcond ~ 1e-12): not screened, and the
// rook solve is accurate.
TEST(DenseArmReport, ABadlyScaledWellPosedMatrixIsNotFlagged) {
    Mat m(3, 3);
    m << 1e6, 1.0, 0.0, 1.0, 2.0, 1e-3, 0.0, 1e-3, -1e-6;
    const SpMatRM A = upper_csr(m);
    SymmetricFactor f(report_options());
    f.analyze(A);
    const FactorizeOutcome o = f.factorize(A);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(o.inertia.numerically_singular);
    EXPECT_EQ(f.counters().dense_condition_estimate_count, 0);
    const Vec b(Vec::Ones(3));
    Vec x(3);
    f.solve(b, x);
    const Vec ref = m.ldlt().solve(b);
    EXPECT_LE((x - ref).lpNorm<Eigen::Infinity>(), 1e-9 * ref.lpNorm<Eigen::Infinity>());
}

// THE CATASTROPHIC CLASS: an E0 roster factorization of MESH (eqp site, mu = 0) whose
// Bunch-Kaufman solve misses its own system by ~1e8 of the walk's scale. Flagged under kReport;
// the equilibrated shift (sigma = 2^-30 on D K D) answers it within the walk's row test.
TEST(DenseArmReport, TheCatastrophicMeshFactorIsFlaggedAndTheShiftedSolvePasses) {
    const SpMatRM K = load_fixture("mesh_sqp_eqp_1135");
    const Index n = K.rows();
    const Index np = 25;
    Vec b(n);
    {
        std::ifstream f(std::string(HVEN_DN1_FIXTURES) + "/mesh_sqp_eqp_1135.rhs.txt");
        std::string line;
        std::getline(f, line);
        for (Index i = 0; i < n; ++i) {
            std::string tok;
            f >> tok;
            b(i) = std::strtod(tok.c_str(), nullptr);
        }
        ASSERT_TRUE(static_cast<bool>(f));
    }
    const Mat Kd = dense_of(K);
    // The walk's row ratio at mu = 0: ||(b - K x)_rows||inf / (0.1 (1 + ||b_rows||inf)).
    const auto row_ratio = [&](const Vec &x) {
        const Vec r = b - Kd * x;
        return r.tail(n - np).lpNorm<Eigen::Infinity>() /
               (0.1 * (1.0 + b.tail(n - np).lpNorm<Eigen::Infinity>()));
    };

    SymmetricFactor bk(dense_options());
    bk.analyze(K);
    ASSERT_EQ(bk.factorize(K).arm, FactorArm::kDense);
    Vec xb(n);
    bk.solve(b, xb);
    EXPECT_GT(row_ratio(xb), 1.0); // the premise

    SymmetricFactor f(report_options());
    f.analyze(K);
    ASSERT_TRUE(f.factorize(K).inertia.numerically_singular);
    const Vec d = hven::detail::equilibration_scale(K);
    const FactorizeOutcome o =
        f.factorize_equilibrated(K, np, d.data(), hven::detail::kDenseEquilibratedShift);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    EXPECT_FALSE(o.inertia.numerically_singular);
    Vec x(n);
    f.solve(b, x);
    EXPECT_LE(row_ratio(x), 1.0);
}

// The threads count follows set_threads() on a dense engine, and survives into an adopter.
TEST(DenseArm, ThreadsRoundTrip) {
    SymmetricFactor f(dense_options());
    const SpMatRM A = upper_csr(Mat::Identity(2, 2));
    f.analyze(A);
    f.set_threads(2);
    EXPECT_EQ(f.threads(), 2);
    ASSERT_EQ(f.factorize(A).arm, FactorArm::kDense);
    SymmetricFactor g = SymmetricFactor::adopt(f.share());
    EXPECT_EQ(g.threads(), 2);
    Vec x(2);
    g.solve(Vec::Ones(2), x);
    EXPECT_EQ(x(0), 1.0);
}

} // namespace
