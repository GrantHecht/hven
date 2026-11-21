// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).
//
// THE RETAINED ANALYSES (M8 PF-5; detail/kkt/kkt_factor.h). A KktFactor keeps up to
// kRetainedAnalyses analyzed sessions besides its current one and refactorizes into one
// of them, without a backend analysis, when K's pattern returns. These tests pin:
//   - IDENTITY: a refactorization into a retained session reports the same status, arm,
//     inertia and perturbed pivots as a fresh analysis plus factorization of the same K,
//     and solves bit for bit the same -- on the sparse arm and on a dense-sized pattern.
//     The check is backend-neutral: on the macOS CI lane it is the Accelerate
//     observation of the same claim;
//   - THE KEY: a retained entry whose hash equals K's but whose pattern does not is a
//     miss (the exact comparison), never a hit;
//   - THE BOUND: at most kRetainedAnalyses entries, most recently used first, the least
//     recently used one freed;
//   - THE COUNTS: kkt_counters() and the dense tally carry across a swap; every hit is
//     recorded on the open AnalysisReuseTally scopes and is not a backend analysis.

#include <cstdint>
#include <cstring>
#include <set>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_factor.h>
#include <hven/linear/symmetric_factor.h>

using namespace hven;
using namespace hven::detail;
using hven::linear::DensePolicy;
using hven::linear::FactorArm;
using hven::linear::FactorizeOutcome;
using hven::linear::SymmetricFactor;

namespace {

// A deterministic generator in [0, 1): the fixtures must not depend on a standard
// library's distribution.
struct Lcg {
    std::uint64_t s;
    explicit Lcg(std::uint64_t seed) : s(seed * 0x9E3779B97F4A7C15ULL + 1) {}
    double next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return static_cast<double>(s >> 11) * 0x1p-53;
    }
};

// K = [[H, J^T], [J, -mu I]], upper triangle stored, every diagonal present. The
// pattern (H's off-diagonals, J's columns) is drawn from `pattern_seed` alone, the
// values from `value_seed`: equal pattern seeds give equal patterns.
SpMatRM make_kkt(Index n, Index m, std::uint64_t pattern_seed, std::uint64_t value_seed,
                 double mu) {
    Lcg pr(pattern_seed);
    Lcg vr(value_seed);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(i, i, 1.0 + vr.next());
        if (i + 1 < n && pr.next() < 0.3) {
            t.emplace_back(i, i + 1, vr.next() - 0.5);
        }
    }
    for (Index j = 0; j < m; ++j) {
        std::set<Index> cols;
        while (cols.size() < 3) {
            cols.insert(static_cast<Index>(pr.next() * static_cast<double>(n)) % n);
        }
        for (const Index c : cols) {
            t.emplace_back(c, n + j, vr.next() + 0.25);
        }
        t.emplace_back(n + j, n + j, -mu);
    }
    SpMatRM K(n + m, n + m);
    K.setFromTriplets(t.begin(), t.end());
    K.makeCompressed();
    return K;
}

Vec rhs_of(Index dim) {
    Vec b(dim);
    for (Index i = 0; i < dim; ++i) {
        b(i) = 1.0 + static_cast<double>(i % 7) / 7.0;
    }
    return b;
}

bool bitwise_equal(const Vec &a, const Vec &b) {
    return a.size() == b.size() &&
           std::memcmp(a.data(), b.data(), static_cast<std::size_t>(a.size()) * sizeof(double)) ==
               0;
}

// The verify check: K analyzed and factorized on a fresh session from k's own options,
// against what k's factorization of K reported (`o`) and what it solves.
void expect_identical_to_fresh(const KktFactor &k, const SpMatRM &K, const FactorizeOutcome &o) {
    SymmetricFactor fresh(k.options);
    fresh.analyze(K);
    const FactorizeOutcome f = fresh.factorize(K);
    EXPECT_EQ(static_cast<int>(o.status), static_cast<int>(f.status));
    EXPECT_EQ(static_cast<int>(o.arm), static_cast<int>(f.arm));
    EXPECT_EQ(o.backend_code, f.backend_code);
    EXPECT_EQ(static_cast<int>(o.inertia.state), static_cast<int>(f.inertia.state));
    EXPECT_EQ(o.inertia.positive, f.inertia.positive);
    EXPECT_EQ(o.inertia.negative, f.inertia.negative);
    EXPECT_EQ(o.inertia.zero, f.inertia.zero);
    EXPECT_EQ(o.inertia.perturbed_pivots, f.inertia.perturbed_pivots);
    EXPECT_EQ(o.inertia.numerically_singular, f.inertia.numerically_singular);
    ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
    const Vec b = rhs_of(K.rows());
    Vec x_retained(K.rows());
    Vec x_fresh(K.rows());
    k.factor.solve(b, x_retained);
    fresh.solve(b, x_fresh);
    EXPECT_TRUE(bitwise_equal(x_retained, x_fresh))
        << "max |diff| " << (x_retained - x_fresh).cwiseAbs().maxCoeff();
}

struct Case {
    const char *name;
    Index n;
    Index m;
    double mu;
    DensePolicy dense;
    FactorArm arm;
};

class RetainedAnalysesIdentity : public ::testing::TestWithParam<Case> {};

// The cycle A, B, C, A', B', C', A'' (primes: new values on the same pattern). Every
// primed factorization is a hit: no backend analysis, one reuse each, and the
// verify check holds on every one of them.
TEST_P(RetainedAnalysesIdentity, AReusedRefactorizationIsBitIdenticalToAFreshAnalysis) {
    const Case c = GetParam();
    KktFactor k(0, c.dense);
    const std::uint64_t patterns[3] = {11, 12, 13};
    Index perturbed_seen = 0;
    for (int round = 0; round < 3; ++round) {
        for (const std::uint64_t p : patterns) {
            SCOPED_TRACE(::testing::Message() << "round " << round << " pattern " << p);
            const SpMatRM K =
                make_kkt(c.n, c.m, p, 100 * p + static_cast<std::uint64_t>(round), c.mu);
            const AnalysisReuseTally reuses;
            const hven::linear::AnalysisTally analyses;
            const AnalysisDecision decision = analysis_decision(k, K);
            EXPECT_TRUE(decision.needed);
            const FactorizeOutcome o = factorize_checked(k, K, decision);
            EXPECT_EQ(o.arm, c.arm);
            EXPECT_EQ(reuses.reuses(), round == 0 ? 0 : 1);
            if (c.arm == FactorArm::kSparse) {
                // A hit pays no platform analysis; a miss pays exactly one.
                EXPECT_EQ(analyses.analyses(), round == 0 ? 1 : 0);
            }
            perturbed_seen += o.inertia.perturbed_pivots.value_or(0);
            expect_identical_to_fresh(k, K, o);
        }
    }
    EXPECT_EQ(k.retained.size(), 2u);
    RecordProperty("perturbed_pivots_seen", static_cast<int>(perturbed_seen));
}

INSTANTIATE_TEST_SUITE_P(
    PF5, RetainedAnalysesIdentity,
    ::testing::Values(
        // Above the SQP's dense threshold: Pardiso / Accelerate's sparse arm.
        Case{"sparse_regularized", 200, 80, 1e-8, DensePolicy::kAuto, FactorArm::kSparse},
        // A zero (2,2) block: the saddle point a session without matching perturbs.
        Case{"sparse_saddle", 200, 80, 0.0, DensePolicy::kNever, FactorArm::kSparse},
        // n + m <= 105 under the SQP's kAuto: the dense arm's session is the one retained.
        Case{"dense_sized", 60, 30, 1e-8, DensePolicy::kAuto, FactorArm::kDense},
        Case{"dense_sized_platform", 60, 30, 1e-8, DensePolicy::kNever, FactorArm::kSparse}),
    [](const ::testing::TestParamInfo<Case> &info) { return std::string(info.param.name); });

// THE KEY. The retained entry for A is forged to carry C's hash; C has A's order and
// stored-entry count but another pattern. Only the exact comparison tells them apart, and
// it must: C is a miss (a backend analysis, no reuse), and A's entry stays retained.
TEST(RetainedAnalyses, AnEqualHashOnAnotherPatternIsAMiss) {
    KktFactor k(0, DensePolicy::kNever);
    const Index n = 40;
    const Index m = 15;
    const SpMatRM A = make_kkt(n, m, 21, 1, 1e-8);
    const SpMatRM B = make_kkt(n, m, 22, 2, 1e-8);
    // A pattern with A's order and nnz but different columns: try seeds until one fits.
    SpMatRM C;
    for (std::uint64_t s = 100; s < 10000; ++s) {
        C = make_kkt(n, m, s, 3, 1e-8);
        if (C.nonZeros() == A.nonZeros() && hven::pattern_hash(C) != hven::pattern_hash(A)) {
            break;
        }
    }
    ASSERT_EQ(C.nonZeros(), A.nonZeros());
    ASSERT_NE(hven::pattern_hash(C), hven::pattern_hash(A));

    ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(k.retained.size(), 1u);
    ASSERT_EQ(k.retained[0].pattern_hash, hven::pattern_hash(A));
    // The forged collision.
    k.retained[0].pattern_hash = hven::pattern_hash(C);

    // (The verify check's own fresh analyses run after the counts are read.)
    {
        const AnalysisReuseTally reuses;
        const hven::linear::AnalysisTally analyses;
        const FactorizeOutcome o = factorize_checked(k, C);
        ASSERT_EQ(o.status, FactorizeOutcome::Status::kOk);
        EXPECT_EQ(reuses.reuses(), 0);
        EXPECT_EQ(analyses.analyses(), 1);
        expect_identical_to_fresh(k, C, o);
    }
    // C's miss retained B in front of the untouched (forged) A.
    ASSERT_EQ(k.retained.size(), 2u);
    EXPECT_EQ(k.retained[0].pattern_hash, hven::pattern_hash(B));
    EXPECT_EQ(k.retained[1].pattern_hash, hven::pattern_hash(C));

    // A true hit still hits.
    const SpMatRM B2 = make_kkt(n, m, 22, 5, 1e-8);
    const AnalysisReuseTally reuses;
    const hven::linear::AnalysisTally analyses;
    const FactorizeOutcome o2 = factorize_checked(k, B2);
    EXPECT_EQ(reuses.reuses(), 1);
    EXPECT_EQ(analyses.analyses(), 0);
    expect_identical_to_fresh(k, B2, o2);
}

// THE BOUND. Ten patterns P0..P9 in turn: the list holds the eight before the current
// one, most recently used first, and P0 has been freed. P1 (the least recently used
// survivor) is then a hit and P0 a miss.
TEST(RetainedAnalyses, TheListHoldsTheEightMostRecentlyUsedPatterns) {
    ASSERT_EQ(kRetainedAnalyses, 8u);
    KktFactor k(0, DensePolicy::kNever);
    const Index n = 30;
    const Index m = 10;
    std::vector<SpMatRM> P;
    std::set<std::uint64_t> hashes;
    for (std::uint64_t s = 31; P.size() < 10; ++s) {
        SpMatRM K = make_kkt(n, m, s, s, 1e-8);
        if (hashes.insert(hven::pattern_hash(K)).second) {
            P.push_back(std::move(K));
        }
    }
    for (const SpMatRM &K : P) {
        ASSERT_EQ(factorize_checked(k, K).status, FactorizeOutcome::Status::kOk);
        EXPECT_LE(k.retained.size(), kRetainedAnalyses);
    }
    ASSERT_EQ(k.retained.size(), kRetainedAnalyses);
    for (std::size_t i = 0; i < kRetainedAnalyses; ++i) {
        EXPECT_EQ(k.retained[i].pattern_hash, hven::pattern_hash(P[8 - i])) << "entry " << i;
    }
    {
        const AnalysisReuseTally reuses;
        ASSERT_EQ(factorize_checked(k, P[1]).status, FactorizeOutcome::Status::kOk);
        EXPECT_EQ(reuses.reuses(), 1);
        // The hit's entry left the list and the outgoing P9 entered it at the front.
        ASSERT_EQ(k.retained.size(), kRetainedAnalyses);
        EXPECT_EQ(k.retained[0].pattern_hash, hven::pattern_hash(P[9]));
    }
    {
        const AnalysisReuseTally reuses;
        const hven::linear::AnalysisTally analyses;
        ASSERT_EQ(factorize_checked(k, P[0]).status, FactorizeOutcome::Status::kOk);
        EXPECT_EQ(reuses.reuses(), 0);
        EXPECT_EQ(analyses.analyses(), 1);
        EXPECT_EQ(k.retained.size(), kRetainedAnalyses);
        EXPECT_EQ(k.retained[0].pattern_hash, hven::pattern_hash(P[1]));
    }
}

// Freed at QP-solve exit: the list goes, the current session and its numerics stay.
TEST(RetainedAnalyses, ReleaseFreesTheListAndKeepsTheCurrentNumerics) {
    KktFactor k(0, DensePolicy::kNever);
    const SpMatRM A = make_kkt(30, 10, 41, 1, 1e-8);
    const SpMatRM B = make_kkt(30, 10, 42, 2, 1e-8);
    ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(k.retained.size(), 1u);
    const std::uint64_t sid = k.factor.session_id();
    const std::uint64_t epoch = k.factor.epoch();
    release_retained_analyses(k);
    EXPECT_TRUE(k.retained.empty());
    EXPECT_EQ(k.factor.session_id(), sid);
    EXPECT_EQ(k.factor.epoch(), epoch);
    EXPECT_FALSE(needs_analysis(k, B));
    const Vec x = solve_vec(k, rhs_of(B.rows()));
    const Eigen::MatrixXd dense_b = Eigen::MatrixXd(B).selfadjointView<Eigen::Upper>();
    EXPECT_LT((dense_b * x - rhs_of(B.rows())).cwiseAbs().maxCoeff(), 1e-8);
    // A is analyzed again: nothing is retained.
    const AnalysisReuseTally reuses;
    ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(reuses.reuses(), 0);
}

// THE COUNTS. The cumulative counters and the dense tally are the KktFactor's across
// swaps: every factorization is counted once, whichever session ran it.
TEST(RetainedAnalyses, TheCountsCarryAcrossSwaps) {
    KktFactor k(0, DensePolicy::kAuto);
    const SpMatRM A = make_kkt(20, 8, 51, 1, 1e-8);
    const SpMatRM B = make_kkt(20, 8, 52, 2, 1e-8);
    const SpMatRM A2 = make_kkt(20, 8, 51, 3, 1e-8);
    Index dense = 0;
    Index flagged = 0;
    Index refactors = 0;
    Index retakes = 0;
    {
        const DenseTallyScope tally(k, dense, flagged, refactors, retakes);
        ASSERT_EQ(factorize_checked(k, A).arm, FactorArm::kDense);
        ASSERT_EQ(factorize_checked(k, B).arm, FactorArm::kDense);
        ASSERT_EQ(factorize_checked(k, A2).arm, FactorArm::kDense);
    }
    EXPECT_EQ(dense, 3);
    EXPECT_EQ(dense_response_tally(k).dense, 3);
    EXPECT_EQ(kkt_counters(k).factorize_count, 3);
    EXPECT_EQ(kkt_counters(k).dense_factorize_count, 3);
    EXPECT_EQ(kkt_counters(k).analyze_count, 2);
    // The current session is A's, factorized twice; B's one factorization is carried.
    EXPECT_EQ(k.factor.counters().factorize_count, 2);
}

// Nested tallies each see a hit (AnalysisTally's shape).
TEST(RetainedAnalyses, EveryOpenTallyRecordsAHit) {
    KktFactor k(0, DensePolicy::kNever);
    const SpMatRM A = make_kkt(30, 10, 61, 1, 1e-8);
    const SpMatRM B = make_kkt(30, 10, 62, 2, 1e-8);
    const AnalysisReuseTally outer;
    ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    {
        const AnalysisReuseTally inner;
        ASSERT_EQ(factorize_checked(k, A).status, FactorizeOutcome::Status::kOk);
        EXPECT_EQ(inner.reuses(), 1);
    }
    ASSERT_EQ(factorize_checked(k, B).status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(outer.reuses(), 2);
}

} // namespace
