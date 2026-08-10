// Fault-injection coverage for backend paths no real fixture can provoke --
// see docs/testing.md for the seam design and why it is shaped this way.
// This file is compiled ONLY into the standalone hven_fault_injection_tests
// executable (tests/CMakeLists.txt), never into hven_tests: it is the one
// place hven/detail/linear/fault_injection.h's injectors are exercised.
//
// Both halves below are guarded by the standard __APPLE__ predefined macro
// so this single source file compiles cleanly into whichever platform's
// standalone target tests/CMakeLists.txt builds -- exactly one half is ever
// active in a given build, mirroring the production backend split.

#include <array>
#include <stdexcept>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/detail/linear/fault_injection.h"
#include "hven/linear/symmetric_factor.h"

// MKL-only: needed for the ordering/weighted_matching don't-write-by-default
// coverage below, which compares against a fresh, independent pardisoinit()
// call (see PardisoinitReference). Not needed by anything Apple-gated in
// this file, so scoped the same way the rest of the MKL half is.
#if !defined(__APPLE__)
#include "hven/detail/linear/pardiso_session.h"
#endif

namespace {

using hven::Index;
using hven::Mat;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::FactorizeOutcome;
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

// 3x3 SPD tridiagonal -- an ordinary, healthy fixture. The point of every
// test below is that the INJECTED fault is what makes the backend look like
// it failed; the matrix itself is deliberately unremarkable.
Mat spd3() {
    Mat A(3, 3);
    A << 2, -1, 0, /**/ -1, 2, -1, /**/ 0, -1, 2;
    return A;
}

#if !defined(__APPLE__)

// See hven/detail/linear/fault_injection.h for the exact scope this is
// faithful within: valid ONLY on a session that has never previously
// factorized successfully.
class FactorizeFaultGuard {
  public:
    explicit FactorizeFaultGuard(int code) {
        hven::linear::detail::testing::FactorizeFaultInjector::injected_backend_code = code;
        hven::linear::detail::testing::FactorizeFaultInjector::active = true;
    }
    ~FactorizeFaultGuard() {
        hven::linear::detail::testing::FactorizeFaultInjector::active = false;
    }

    FactorizeFaultGuard(const FactorizeFaultGuard &) = delete;
    FactorizeFaultGuard &operator=(const FactorizeFaultGuard &) = delete;
};

// The MKL factorize-failure epoch test the factorize-failure disclosure
// carried as an open item: no real fixture makes Pardiso refuse a symmetric
// indefinite factorization (static pivot perturbation gets through
// everything tried), so this is the first executable coverage of
// SymmetricFactor::factorize()'s error branch. Pins exactly the invariants
// FactorSession::factorize's own inspection-only comment names: no epoch
// bump, the engine reports kUnavailable inertia, and solves/share are
// refused until a real factorization succeeds.
TEST(MklFactorizeFaultInjection, FirstFactorizeFailureLeavesNoUsableNumericsAndDoesNotBumpEpoch) {
    SymmetricFactor factor{SymmetricFactor::Options{}};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);
    ASSERT_EQ(factor.epoch(), 0u);

    FactorizeOutcome outcome;
    {
        FactorizeFaultGuard guard(/*code=*/42);
        outcome = factor.factorize(A);
    }

    EXPECT_EQ(outcome.status, FactorizeOutcome::Status::kBackendError);
    EXPECT_EQ(outcome.backend_code, 42);
    EXPECT_EQ(outcome.inertia.state, InertiaEvidence::State::kUnavailable);

    EXPECT_EQ(factor.epoch(), 0u) << "a failed factorize() must not advance the epoch";
    EXPECT_EQ(factor.counters().analyze_count, 1);
    EXPECT_EQ(factor.counters().factorize_count, 1)
        << "the call reached the (injected) backend and returned, so it counts";
    EXPECT_EQ(factor.inertia().state, InertiaEvidence::State::kUnavailable);

    Vec rhs = Vec::Ones(3);
    Vec x(3);
    EXPECT_THROW(factor.solve(rhs, x), std::runtime_error);
    EXPECT_THROW(factor.share(), std::runtime_error);
}

// Companion sanity check: with the injector disengaged, the exact same
// session factorizes normally -- confirms the guard above is what produced
// the failure, not some other defect in this fixture or target.
TEST(MklFactorizeFaultInjection, TheSameSessionFactorizesNormallyOnceTheInjectorIsDisengaged) {
    SymmetricFactor factor{SymmetricFactor::Options{}};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);

    const auto outcome = factor.factorize(A); // injector inactive by default
    EXPECT_EQ(outcome.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(factor.epoch(), 1u);
    EXPECT_EQ(outcome.inertia.state, InertiaEvidence::State::kObserved);
}

// =============================================================================
// Ordering / weighted-matching don't-write-by-default: executable coverage
// (PardisoIparmObserver, fault_injection.h) for the rule that
// SymmetricFactor::Options::ordering and ::weighted_matching are guaranteed
// by inspection alone in the normal (non-HVEN_TESTING) build -- see
// pardiso_session.cpp's two guarded iparm writes and docs/testing.md.
// =============================================================================

// Independent ground truth: calls MKL's OWN pardisoinit for mtype = -2 (the
// only mtype hven's SymmetricFactor ever builds -- config_from,
// symmetric_factor_mkl.cpp), completely bypassing hven's analyze() and its
// guarded writes. hven's own iparm[1]/iparm[12] at DEFAULT Options must
// equal these exactly; comparing against a FRESH call like this one, rather
// than a literal constant, is what keeps that assertion correct across MKL
// versions whose pardisoinit default differs (measured on this toolchain:
// iparm[1]'s pardisoinit default is 2 on MKL 2025.3, 3 on 2026.0/2026.1;
// iparm[12]'s is 0 on all three).
//
// Coverage: because the linked MKL's own ordering default equals
// kParallelNestedDissection's contract value (3), this VALUE-LEVEL
// observation cannot on its own distinguish "left iparm[1] alone" from
// "wrote 3" on THIS MKL. That gap is closed outright, and version-
// independently, by the DID-THE-WRITE-EXECUTE observable recorded at the two
// guarded write sites themselves (PardisoIparmObserver::ordering_was_written
// and its matching twin) — asserted below alongside every value assertion.
// The hook that produces it sits inside the MPL-derived session file rather
// than at the adapter boundary this project prefers to instrument at; that
// is a sanctioned deviation with its reasoning in docs/testing.md, not a rule
// being broken. The matching half (0 vs 1) was already unambiguous at the
// value level on every MKL version; it carries the same flag for symmetry and
// for the same mutation-resistance.
struct PardisoinitReference {
    MKL_INT ordering;
    MKL_INT weighted_matching;
};

PardisoinitReference pardisoinit_reference() {
    std::array<void *, hven::linear::detail::kPardisoSlots> pt{};
    std::array<MKL_INT, hven::linear::detail::kPardisoSlots> iparm{};
    MKL_INT mtype = -2;
    pardisoinit(pt.data(), &mtype, iparm.data());
    return PardisoinitReference{iparm[1], iparm[12]};
}

using hven::linear::detail::testing::PardisoIparmObserver;

// At default Options, hven's analyze() must leave iparm[1]/iparm[12] EXACTLY
// where pardisoinit put them -- the don't-write-by-default rule's core
// claim. Asserted by comparison to a fresh, independent pardisoinit() call
// (never a literal constant), so this test does not silently start failing
// (or, worse, silently stop meaning anything) across an MKL upgrade that
// moves pardisoinit's own default.
TEST(PardisoIparmObservation, DefaultOptionsLeaveOrderingAndMatchingAtThePardisoinitDefault) {
    PardisoIparmObserver::reset();
    SymmetricFactor factor{SymmetricFactor::Options{}};
    factor.analyze(upper_csr(spd3()));

    const PardisoinitReference reference = pardisoinit_reference();
    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_EQ(PardisoIparmObserver::last_ordering_iparm, reference.ordering);
    EXPECT_EQ(PardisoIparmObserver::last_weighted_matching_iparm, reference.weighted_matching);

    // The claim the value comparison above cannot always make: not merely
    // that the entries still hold pardisoinit's values, but that NEITHER
    // ASSIGNMENT RAN. This one is version-independent -- no backend default
    // can make it vacuous -- and it is what the don't-write-by-default rule
    // actually says. The two non-default tests below assert the same flags
    // TRUE, which is what keeps these two from passing for want of a live
    // observable.
    EXPECT_FALSE(PardisoIparmObserver::ordering_was_written)
        << "default Options must not execute the iparm[1] write at all";
    EXPECT_FALSE(PardisoIparmObserver::weighted_matching_was_written)
        << "default Options must not execute the iparm[12] write at all";

    // The identical claim for the option set's five other guarded writes
    // -- see fault_injection.h's own doc comment on why every new knob
    // carries this flag even where the value-level ambiguity that
    // originally motivated it for ordering does not independently arise
    // for each one. iparm[17] (factor_nonzeros) carries NO flag here: it is
    // written unconditionally, not guarded -- see
    // FactorSession::factor_nonzeros()'s own doc comment.
    EXPECT_FALSE(PardisoIparmObserver::matrix_scaling_was_written)
        << "default Options must not execute the iparm[10] write at all";
    EXPECT_FALSE(PardisoIparmObserver::pivot_strategy_was_written)
        << "default Options must not execute the iparm[20] write at all";
    EXPECT_FALSE(PardisoIparmObserver::factorization_algorithm_was_written)
        << "default Options must not execute the iparm[23] write at all";
    EXPECT_FALSE(PardisoIparmObserver::solve_parallelism_was_written)
        << "default Options must not execute the iparm[24] write at all";
    EXPECT_FALSE(PardisoIparmObserver::cnr_was_written)
        << "default Options must not execute the iparm[33] write at all";
    EXPECT_FALSE(PardisoIparmObserver::factor_mflops_was_written)
        << "default Options must not execute the iparm[18] write at all";
}

// matrix_scaling = true's contract value (iparm[10] = 1) is a fixed part of
// the frozen surface, same shape as weighted_matching's own test above.
TEST(PardisoIparmObservation, MatrixScalingTrueWritesExactly1) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::matrix_scaling_was_written);
    EXPECT_EQ(PardisoIparmObserver::matrix_scaling_written_value, 1);
}

// Every named PivotStrategy value writes its own fixed contract code to
// iparm[20] -- EXACTLY Intel's own documented iparm[20] codes (see
// PivotStrategy's own doc comment, symmetric_factor.h), not a
// version-fragile assumption, so the literal codes below are correct to
// assert directly.
TEST(PardisoIparmObservation, PivotStrategyWritesItsOwnContractCode) {
    using PivotStrategy = SymmetricFactor::Options::PivotStrategy;
    const std::vector<std::pair<PivotStrategy, int>> cases = {
        {PivotStrategy::kOneByOne, 0},
        {PivotStrategy::kTwoByTwo, 1},
        {PivotStrategy::kOneByOneNoAutoRefine, 2},
        {PivotStrategy::kTwoByTwoNoAutoRefine, 3},
    };
    for (const auto &[strategy, code] : cases) {
        PardisoIparmObserver::reset();
        SymmetricFactor::Options opts;
        opts.pivot_strategy = strategy;
        SymmetricFactor factor{opts};
        factor.analyze(upper_csr(spd3()));

        ASSERT_TRUE(PardisoIparmObserver::recorded);
        EXPECT_TRUE(PardisoIparmObserver::pivot_strategy_was_written);
        EXPECT_EQ(PardisoIparmObserver::pivot_strategy_written_value, code);
    }
}

// Every named FactorizationAlgorithm value writes its own fixed contract
// code to iparm[23]. Ordering is pinned to kNestedDissection for both
// cases -- kTwoLevel requires it (see symmetric_factor_mkl.cpp's
// constructor); kClassic is indifferent to it, so pinning it for both
// keeps this loop uniform without changing what it tests.
TEST(PardisoIparmObservation, FactorizationAlgorithmWritesItsOwnContractCode) {
    using FactorizationAlgorithm = SymmetricFactor::Options::FactorizationAlgorithm;
    const std::vector<std::pair<FactorizationAlgorithm, int>> cases = {
        {FactorizationAlgorithm::kClassic, 0},
        {FactorizationAlgorithm::kTwoLevel, 1},
    };
    for (const auto &[algorithm, code] : cases) {
        PardisoIparmObserver::reset();
        SymmetricFactor::Options opts;
        opts.factorization_algorithm = algorithm;
        opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
        SymmetricFactor factor{opts};
        factor.analyze(upper_csr(spd3()));

        ASSERT_TRUE(PardisoIparmObserver::recorded);
        EXPECT_TRUE(PardisoIparmObserver::factorization_algorithm_was_written);
        EXPECT_EQ(PardisoIparmObserver::factorization_algorithm_written_value, code);
    }
}

// Every named SolveParallelism value writes its own fixed contract code to
// iparm[24] -- EXACTLY Intel's own documented iparm[24] codes (see
// SolveParallelism's own doc comment, symmetric_factor.h). kAdaptivePartitioning
// writes 0, not the historically-wrong "true writes 1" a bare bool once did
// -- see that enum's own naming-history note.
TEST(PardisoIparmObservation, SolveParallelismWritesItsOwnContractCode) {
    using SolveParallelism = SymmetricFactor::Options::SolveParallelism;
    const std::vector<std::pair<SolveParallelism, int>> cases = {
        {SolveParallelism::kAdaptivePartitioning, 0},
        {SolveParallelism::kSequential, 1},
        {SolveParallelism::kMatrixPartitionParallel, 2},
    };
    for (const auto &[mode, code] : cases) {
        PardisoIparmObserver::reset();
        SymmetricFactor::Options opts;
        opts.solve_parallelism = mode;
        SymmetricFactor factor{opts};
        factor.analyze(upper_csr(spd3()));

        ASSERT_TRUE(PardisoIparmObserver::recorded);
        EXPECT_TRUE(PardisoIparmObserver::solve_parallelism_was_written);
        EXPECT_EQ(PardisoIparmObserver::solve_parallelism_written_value, code);
    }
}

// cnr_threads writes its OWN value (not a fixed 0/1 flag) to iparm[33].
// Ordering is pinned to kNestedDissection -- the only ordering CNR mode is
// documented compatible with (see cnr_threads' own doc comment); without
// it, construction itself would throw before analyze() ever ran.
TEST(PardisoIparmObservation, CnrThreadsWritesTheRequestedCount) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.cnr_threads = 5;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::cnr_was_written);
    EXPECT_EQ(PardisoIparmObserver::cnr_written_value, 5);
}

// collect_factor_mflops = true writes iparm[18] = -1 (the Pardiso request
// code). iparm[17] (factor_nonzeros) is NOT gated by this option -- it is
// written unconditionally regardless -- so this test's only claim is about
// iparm[18] specifically, individually verifiable from
// iparm[17]'s own unconditional write (finding the shared-flag gap the
// PardisoIparmObserver::factor_mflops_was_written rename closes).
TEST(PardisoIparmObservation, CollectFactorMflopsTrueRequestsIparm18) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.collect_factor_mflops = true;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::factor_mflops_was_written);
}

// Functional companion: FactorEvidence's free/costly split, proven through
// the public API rather than just the request bit. Evidence
// lives on FactorizeOutcome, not SolveInfo -- see FactorEvidence's own doc
// comment (symmetric_factor.h) for why.
//
// factor_nonzeros is ALWAYS present after a successful factorization --
// no Options field gates it. factor_mflops is present only when
// collect_factor_mflops requested it. factor_size_bytes never appears on
// MKL.
TEST(PardisoIparmObservation, FactorNonzerosIsAlwaysPresentRegardlessOfCollectFactorMflops) {
    for (const bool collect_mflops : {false, true}) {
        SymmetricFactor::Options opts;
        opts.collect_factor_mflops = collect_mflops;
        SymmetricFactor factor{opts};
        const SpMatRM A = upper_csr(spd3());
        factor.analyze(A);
        const auto outcome = factor.factorize(A);
        ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);

        ASSERT_TRUE(outcome.factor.factor_nonzeros.has_value())
            << "factor_nonzeros must be present regardless of collect_factor_mflops";
        EXPECT_GE(*outcome.factor.factor_nonzeros, 0);
        EXPECT_EQ(outcome.factor.factor_mflops.has_value(), collect_mflops)
            << "factor_mflops must track collect_factor_mflops exactly";
        EXPECT_FALSE(outcome.factor.factor_size_bytes.has_value())
            << "MKL never populates the Accelerate-only byte-size field";
    }
}

// kMinimumDegree's contract value (iparm[1] = 0) is a fixed part of the
// public contract. 0 is also MKL_INT's own
// zero-initialized state, which is exactly why the *_was_written flag below
// -- not the value comparison -- is what actually distinguishes "wrote 0"
// from "never touched the array element pardisoinit itself may have left at
// 0" on some future MKL version; on every MKL currently linked here,
// pardisoinit's own iparm[1] default is 2 or 3 (never 0), so the value
// comparison alone already carries this test today, but the flag assertion
// is what keeps it version-independent.
TEST(PardisoIparmObservation, MinimumDegreeOrderingWritesExactly0) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.ordering = SymmetricFactor::Options::Ordering::kMinimumDegree;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_EQ(PardisoIparmObserver::last_ordering_iparm, 0);
    EXPECT_TRUE(PardisoIparmObserver::ordering_was_written);
    EXPECT_EQ(PardisoIparmObserver::ordering_written_value, 0);
}

// kNestedDissection's contract value (Options amendment: iparm[1] = 2) is a
// fixed part of the frozen surface regardless of MKL version, so this one
// literal is the spec's own commitment, not a version-fragile assumption.
TEST(PardisoIparmObservation, NestedDissectionOrderingWritesExactly2) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_EQ(PardisoIparmObserver::last_ordering_iparm, 2);
    EXPECT_TRUE(PardisoIparmObserver::ordering_was_written);
    EXPECT_EQ(PardisoIparmObserver::ordering_written_value, 2);
}

// kParallelNestedDissection's contract value (iparm[1] = 3) is likewise
// fixed. NOTE: on the MKL version this repository currently links (2026.1),
// 3 is ALSO pardisoinit's own default for mtype = -2, so choosing
// kParallelNestedDissection today is a behavioral no-op on this box -- the value only diverges from
// "leave it alone" on older MKLs (2025.3's default was 2). The point of
// naming the option explicitly is exactly this: it PINS the choice against
// that version drift, rather than leaving it to whatever the linked MKL
// happens to default to.
TEST(PardisoIparmObservation, ParallelNestedDissectionOrderingWritesExactly3) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.ordering = SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_EQ(PardisoIparmObserver::last_ordering_iparm, 3);
    // On this MKL the value assertion above is satisfied by pardisoinit's own
    // default too, so THIS is the line that actually distinguishes the option
    // from doing nothing.
    EXPECT_TRUE(PardisoIparmObserver::ordering_was_written);
    EXPECT_EQ(PardisoIparmObserver::ordering_written_value, 3);
}

// weighted_matching = true's contract value (iparm[12] = 1) is likewise a
// fixed part of the frozen surface.
TEST(PardisoIparmObservation, WeightedMatchingTrueWritesExactly1) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.weighted_matching = true;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_EQ(PardisoIparmObserver::last_weighted_matching_iparm, 1);
    EXPECT_TRUE(PardisoIparmObserver::weighted_matching_was_written);
    EXPECT_EQ(PardisoIparmObserver::weighted_matching_written_value, 1);
}

#endif // !defined(__APPLE__)

#if defined(__APPLE__)

class InertiaQueryFaultGuard {
  public:
    explicit InertiaQueryFaultGuard(int rc) {
        hven::linear::detail::testing::InertiaQueryFaultInjector::injected_rc = rc;
        hven::linear::detail::testing::InertiaQueryFaultInjector::active = true;
    }
    ~InertiaQueryFaultGuard() {
        hven::linear::detail::testing::InertiaQueryFaultInjector::active = false;
    }

    InertiaQueryFaultGuard(const InertiaQueryFaultGuard &) = delete;
    InertiaQueryFaultGuard &operator=(const InertiaQueryFaultGuard &) = delete;
};

// The Accelerate kQueryFailed test the frozen contract's fabrication fix
// exists to cover: a query that ran and failed leaves its counts INVALID and
// never zero-filled, because a zero-filled triple reads exactly like a real
// one. Unlike the MKL
// test above, this injection is faithful in every scenario -- see
// hven/detail/linear/fault_injection.h -- because SparseGetInertia is a
// side-effect-free query against an ALREADY-successful factorization: the
// factorization itself is real, only the query's return code is injected.
TEST(AccelerateInertiaQueryFaultInjection, QueryFailureReportsKQueryFailedWithInvalidCounts) {
    SymmetricFactor factor{SymmetricFactor::Options{}};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);
    const auto outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);
    ASSERT_EQ(outcome.inertia.state, InertiaEvidence::State::kObserved)
        << "the real, uninjected factorize() must succeed first";

    InertiaEvidence evidence;
    {
        InertiaQueryFaultGuard guard(/*rc=*/-1);
        evidence = factor.inertia();
    }

    EXPECT_EQ(evidence.state, InertiaEvidence::State::kQueryFailed);
    EXPECT_EQ(evidence.n_pos, -1);
    EXPECT_EQ(evidence.n_neg, -1);
    EXPECT_EQ(evidence.n_zero, -1);
    EXPECT_FALSE(evidence.perturbed_pivots.has_value())
        << "must stay absent, not zero-filled, alongside the failed query";

    // Distinguishable from a genuine (0, 0, dim) zero-everywhere class by
    // construction: re-querying with the injector off returns a real
    // observation from the same, still-valid factorization.
    const InertiaEvidence real = factor.inertia();
    EXPECT_EQ(real.state, InertiaEvidence::State::kObserved);
}

#endif // defined(__APPLE__)

} // namespace
