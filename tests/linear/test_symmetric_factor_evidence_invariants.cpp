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
#include <cstdint>
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

// =============================================================================
// The live thread-count setter (SymmetricFactor::set_num_threads)
// =============================================================================
//
// Written here rather than in test_symmetric_factor.cpp because the setter's
// contract is backend-NEUTRAL in exactly the way this file exists for: the
// validation rule, the "nothing else moves" guarantee, and the Options round
// trip through adopt() are the same claims on both backends, and each has an
// observable in the public API alone.
//
// It matters most on Accelerate. There the count is stored and applied to
// nothing (see Options::num_threads' best-effort-absent contract), so the
// STORED value is the entire observable behaviour -- and until these tests
// existed, that backend's copy of the setter was compiled and never
// executed for the round trip, since test_symmetric_factor.cpp is gated on
// NOT APPLE. (The engine-level KKT tests do call the setter on macOS; what
// they do not exercise is the validation rule or the adopt() round trip,
// which is precisely what the three tests below pin.)

// The argument rule is the constructor's own, applied before anything moves:
// a rejected count leaves the configured one exactly as it was.
TEST(SymmetricFactorThreadCount, ANegativeCountIsRejectedAndLeavesTheCountAlone) {
    SymmetricFactor::Options opts;
    opts.num_threads = 1;
    SymmetricFactor factor{opts};

    EXPECT_THROW(factor.set_num_threads(-1), std::invalid_argument);
    EXPECT_EQ(factor.num_threads(), 1);
}

// The whole point of the setter: it moves a value the backend reads per
// call, so nothing that names the current factorization may move with it.
// analyze_count, session_id() and epoch() are the public observables for
// "the symbolic analysis, the session and the committed numerics all
// survived"; a factorize() that then succeeds is the fourth, since
// factorize() throws when there is no analysis to reuse.
TEST(SymmetricFactorThreadCount, ANewCountCostsNoAnalysisNoSessionAndNoEpoch) {
    SymmetricFactor::Options opts;
    opts.num_threads = 1;
    SymmetricFactor factor{opts};

    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    ASSERT_EQ(factor.factorize(A).status, hven::linear::FactorizeOutcome::Status::kOk);

    const std::uint64_t session_before = factor.session_id();
    const std::uint64_t epoch_before = factor.epoch();

    factor.set_num_threads(2);

    EXPECT_EQ(factor.num_threads(), 2);
    EXPECT_EQ(factor.counters().analyze_count, 1);
    EXPECT_EQ(factor.session_id(), session_before);
    EXPECT_EQ(factor.epoch(), epoch_before);

    ASSERT_EQ(factor.factorize(A).status, hven::linear::FactorizeOutcome::Status::kOk);
    EXPECT_EQ(factor.counters().analyze_count, 1) << "the symbolic analysis survived the setter";
    EXPECT_EQ(factor.epoch(), epoch_before + 1);
}

// The round trip that proves the count reached the SESSION and not merely
// this engine's own copy: adopt() rebuilds its Options from the session's
// stored configuration, so an adopter of a session whose count was moved
// after the analysis must report the moved value. A setter that updated only
// the engine would leave this reporting the analyze-time count instead.
//
// This is the one assertion that covers the Accelerate stored-only contract
// end to end: on that backend, being stored faithfully is all the count
// does.
TEST(SymmetricFactorThreadCount, AdoptReportsTheCountSetAfterTheAnalysis) {
    SymmetricFactor::Options opts;
    opts.num_threads = 1;
    SymmetricFactor factor{opts};

    const SpMatRM A = upper_csr(spd4());
    factor.analyze(A);
    ASSERT_EQ(factor.factorize(A).status, hven::linear::FactorizeOutcome::Status::kOk);

    factor.set_num_threads(3);

    const SymmetricFactor adopted = SymmetricFactor::adopt(factor.share());
    EXPECT_EQ(adopted.num_threads(), 3)
        << "adopt() reads the session's CURRENT count, so a co-owner's later set is what an "
           "adopter receives";
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
