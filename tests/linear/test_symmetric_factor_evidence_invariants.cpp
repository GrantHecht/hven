// Backend-agnostic evidence-honesty guards for SymmetricFactor -- written
// ONCE, compiled and run on BOTH backends (see tests/CMakeLists.txt: this
// file, unlike test_symmetric_factor.cpp, is NOT gated by `if(NOT APPLE)`).
// Every assertion here is phrased purely in terms of the public API and the
// frozen contract's per-backend semantics table, so it is
// meaningful -- and, on whichever platform this build targets, actually
// exercised against a REAL backend -- regardless of which one that is. See
// docs/testing.md for how this fits alongside the two backend-specific
// fault-injection tests.

#include <cmath>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace {

using hven::Index;
using hven::Mat;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::InertiaEvidence;
using hven::linear::SymmetricFactor;

SpMatRM upper_csr(const Mat &values) {
    const Index n = values.rows();
    std::vector<Eigen::Triplet<double>> triplets;
    for (Index i = 0; i < n; ++i) {
        for (Index j = i; j < n; ++j) {
            if (i == j || values(i, j) != 0.0) {
                triplets.emplace_back(static_cast<int>(i), static_cast<int>(j), values(i, j));
            }
        }
    }
    SpMatRM A(n, n);
    A.setFromTriplets(triplets.begin(), triplets.end());
    A.makeCompressed();
    return A;
}

// 4x4 discrete Laplacian tridiag(-1, 2, -1): symmetric positive definite,
// analytic inertia (4, 0, 0). SPD rather than indefinite/singular on
// purpose: this file's job is to check evidence-reporting INVARIANTS that
// hold regardless of the matrix's character, not to provoke either
// backend's edge-case handling (that is test_symmetric_factor.cpp's job on
// MKL, and the fault-injection suite's job for paths no real fixture
// reaches).
Mat spd4() {
    Mat A(4, 4);
    A << 2, -1, 0, 0, /**/ -1, 2, -1, 0, /**/ 0, -1, 2, -1, /**/ 0, 0, -1, 2;
    return A;
}

// The evidence-honesty invariants every backend's InertiaEvidence must
// satisfy, checked against whatever a real factorization on THIS platform's
// backend actually reports. Shared by every test below rather than repeated,
// so the invariant list itself has one definition.
void expect_evidence_invariants(const InertiaEvidence &evidence) {
    // kQueryFailed means the counts are INVALID, never a zero-filled
    // plausible-looking triple (the frozen contract's fabrication fix).
    // Vacuous on a healthy factorization -- the backend-specific
    // fault-injection tests are what actually drive a real backend into
    // this state -- but a regression that let a failed query report
    // zero-filled counts on some future input would trip this the moment
    // this test's own fixture happened to hit it.
    if (evidence.state == InertiaEvidence::State::kQueryFailed) {
        EXPECT_EQ(evidence.n_pos, -1);
        EXPECT_EQ(evidence.n_neg, -1);
        EXPECT_EQ(evidence.n_zero, -1);
        EXPECT_FALSE(evidence.perturbed_pivots.has_value())
            << "perturbed_pivots must stay absent alongside a failed query, never zero-filled";
    }

    // perturbed_pivots is a backend-qualified optional: present with
    // Pardiso's semantics on MKL, absent on Accelerate. Whichever
    // it is, a PRESENT value is never negative -- it is a count.
    if (evidence.perturbed_pivots.has_value()) {
        EXPECT_GE(*evidence.perturbed_pivots, 0);
    }
}

class SymmetricFactorEvidenceInvariants : public ::testing::Test {
  protected:
    SymmetricFactor factor{SymmetricFactor::Options{}};
};

TEST_F(SymmetricFactorEvidenceInvariants, ObservedEvidenceSatisfiesTheHonestyInvariants) {
    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

    expect_evidence_invariants(outcome.inertia);
    expect_evidence_invariants(factor.inertia());
}

// The predicate's own contract: false before the first successful
// factorization, and false whenever this engine has no usable numerics. True
// regardless of which
// backend answers it, since neither backend can have usable numerics yet.
TEST_F(SymmetricFactorEvidenceInvariants, PartialSolveUnsupportedBeforeAnyFactorization) {
    EXPECT_FALSE(factor.supports_partial_solve());
}

// Absent perturbation evidence means composability is unverifiable, so
// the predicate must answer false -- the conservative rung, checked against
// whatever this platform's backend actually reports rather than assumed.
TEST_F(SymmetricFactorEvidenceInvariants, AbsentPerturbedPivotsImpliesPartialSolveUnsupported) {
    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

    if (!outcome.inertia.perturbed_pivots.has_value()) {
        EXPECT_FALSE(factor.supports_partial_solve());
    }
}

// The per-backend semantics table, checked by construction rather than
// assumed: MKL's zero class
// is DERIVED (dim - n_pos - n_neg); Accelerate's is a native 3-way report.
// The two platforms disagree on the expected value, so this is the one
// assertion in this file that needs to know which backend it is running
// against -- via the standard predefined macro, not a hand-rolled build
// option, so there is nothing else for src/CMakeLists.txt's platform split
// to keep in sync.
TEST_F(SymmetricFactorEvidenceInvariants, ZeroClassDerivationMatchesTheBackendContract) {
    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

#if defined(__APPLE__)
    EXPECT_FALSE(outcome.inertia.zero_is_derived) << "Accelerate reports the zero class natively";
#else
    EXPECT_TRUE(outcome.inertia.zero_is_derived) << "MKL Pardiso derives it as dim - n_pos - n_neg";
#endif
}

// FactorEvidence's per-backend semantics table, checked the same way as
// ZeroClassDerivationMatchesTheBackendContract above -- and the free/costly
// split: at default Options (collect_factor_mflops off), each
// backend's UNCONDITIONAL field (MKL's factor_nonzeros, Accelerate's
// factor_size_bytes -- both collected with no Options field to gate them)
// is present, while every other field stays absent -- no field is ever a
// fabricated zero. Evidence lives on FactorizeOutcome, not SolveInfo -- see
// FactorEvidence's own doc comment (symmetric_factor.h) for why.
TEST_F(SymmetricFactorEvidenceInvariants, DefaultOptionsMatchTheFreeCostlySplit) {
    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

#if defined(__APPLE__)
    EXPECT_FALSE(outcome.factor.factor_nonzeros.has_value())
        << "Accelerate has no nonzero-count counter";
    EXPECT_FALSE(outcome.factor.factor_mflops.has_value()) << "Accelerate reports no cost estimate";
    ASSERT_TRUE(outcome.factor.factor_size_bytes.has_value())
        << "factor_size_bytes is unconditional on Accelerate -- no Options field gates it";
    EXPECT_GT(*outcome.factor.factor_size_bytes, 0);
#else
    ASSERT_TRUE(outcome.factor.factor_nonzeros.has_value())
        << "factor_nonzeros is unconditional on MKL -- no Options field gates it";
    EXPECT_GE(*outcome.factor.factor_nonzeros, 0);
    EXPECT_FALSE(outcome.factor.factor_mflops.has_value())
        << "factor_mflops stays opt-in (collect_factor_mflops) even at default Options";
    EXPECT_FALSE(outcome.factor.factor_size_bytes.has_value())
        << "MKL never populates the Accelerate-only byte-size field";
#endif
}

// collect_factor_mflops's costly opt-in is genuinely MKL-only now: it
// THROWS at construction on Accelerate (see
// test_symmetric_factor_pardiso_only_options.cpp), so this test is
// internally platform-split rather than shared, following the same
// convention that file documents. The MKL half proves the opt-in actually
// populates factor_mflops in addition to the always-present
// factor_nonzeros; the Apple half proves the construction-time throw
// (already covered elsewhere) is what a caller hits instead of a
// runtime no-op.
TEST(SymmetricFactorFactorEvidence, CollectFactorMflopsAddsTheCostlyFieldOnMkl) {
    SymmetricFactor::Options opts;
#if defined(__APPLE__)
    opts.collect_factor_mflops = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
#else
    opts.collect_factor_mflops = true;
    SymmetricFactor factor{opts};

    Mat A4(4, 4);
    A4 << 2, -1, 0, 0, /**/ -1, 2, -1, 0, /**/ 0, -1, 2, -1, /**/ 0, 0, -1, 2;
    const SpMatRM A = upper_csr(A4);
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, hven::linear::FactorizeOutcome::Status::kOk);

    ASSERT_TRUE(outcome.factor.factor_nonzeros.has_value());
    EXPECT_GE(*outcome.factor.factor_nonzeros, 0);
    ASSERT_TRUE(outcome.factor.factor_mflops.has_value());
    EXPECT_GE(*outcome.factor.factor_mflops, 0);
    EXPECT_FALSE(outcome.factor.factor_size_bytes.has_value())
        << "MKL reports an entry count and a cost estimate, never a byte size";
#endif
}

} // namespace
