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
#include <iostream>
#include <optional>
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

// See hven/detail/linear/fault_injection.h for the exact scope this is
// faithful within: valid ONLY on a session that has never previously
// factorized successfully. Shared by both platform halves below --
// FactorizeFaultInjector itself is a both-backends injector.
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

// =============================================================================
// The failed-factorize evidence pin (ORDERED, gate-blocking --
// docs/retarget-design-sqp.md §7.2): `inertia()` reports non-kObserved after
// a FAILED factorize. The §7.2 reuse gate's usable-numerics conjunct
// (`inertia().state == kObserved`) rests on exactly this contract clause;
// symmetric_factor.h states the not-factorized and stale-adopt cases
// explicitly, and this test is the failed-factorize case's dedicated pin --
// the conjunct may not ship before it exists. No real fixture makes MKL
// refuse a symmetric indefinite factorization (static pivot perturbation
// gets through everything tried), so the pin lives here, on the existing
// FactorizeFaultInjector seam, exactly as §7.2 sanctions -- within that
// injector's faithful scope (a session that has never factorized
// successfully; the deeper succeeded-then-fails scenario stays
// inspection-only, see docs/testing.md). One backend-neutral pin body,
// compiled per-platform like everything else in this file.
// =============================================================================
TEST(FailedFactorizeEvidencePin, InertiaReportsNonObservedAfterAFailedFactorize) {
    SymmetricFactor factor{SymmetricFactor::Options{}};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);

    {
        FactorizeFaultGuard guard(/*code=*/-4);
        const FactorizeOutcome outcome = factor.factorize(A);
        ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kBackendError);
    }

    // The pin itself: after a FAILED factorize, the evidence is
    // non-kObserved -- which is precisely the state the §7.2 reuse gate's
    // usable-numerics conjunct treats as "detach and rebuild".
    EXPECT_NE(factor.inertia().state, InertiaEvidence::State::kObserved)
        << "a failed factorize invalidates the current numerics, so inertia() must not report "
           "kObserved -- the reuse gate's usable-numerics conjunct rests on this";
    EXPECT_EQ(factor.inertia().state, InertiaEvidence::State::kUnavailable)
        << "specifically kUnavailable: there is no factorization to describe, and describing the "
           "previous one would be a fabrication";

    // And the conjunct flips back exactly when a factorization succeeds --
    // the discriminating half that keeps the pin above from passing for a
    // reason unrelated to the failure.
    const FactorizeOutcome ok = factor.factorize(A); // injector disengaged
    ASSERT_EQ(ok.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(factor.inertia().state, InertiaEvidence::State::kObserved);
}

#if !defined(__APPLE__)

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
// weighted_matching must also be set -- matrix_scaling alone now throws at
// construction (see symmetric_factor_mkl.cpp's constructor and
// test_symmetric_factor_pardiso_only_options.cpp's
// MatrixScalingTrueAloneThrowsOnMkl).
TEST(PardisoIparmObservation, MatrixScalingTrueWritesExactly1) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    opts.weighted_matching = true;
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
// code) -- asserted as an exact VALUE, not merely that some write ran.
// iparm[17] (factor_nonzeros) is NOT gated by this option -- it is
// written unconditionally regardless, with no guard and therefore no
// observable of its own -- so this test's only claim is about iparm[18]
// specifically, verifiable on its own dedicated flag/value pair without
// depending on any observation of iparm[17].
TEST(PardisoIparmObservation, CollectFactorMflopsTrueRequestsIparm18) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.collect_factor_mflops = true;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::factor_mflops_was_written);
    EXPECT_EQ(PardisoIparmObserver::factor_mflops_written_value, -1);
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

// =============================================================================
// The two NEW don't-write states (pivot_perturb_exp / max_refinement_iters,
// docs/retarget-design-sqp.md §2): act evidence for both states, in the same
// did-the-write-execute shape as every guarded write above. These two are the
// original ordering ambiguity in its strongest form: the entries' pardisoinit
// defaults are exactly what a skipped write leaves behind, so no boundary
// read can ever distinguish "left it alone" from "wrote the same value" --
// only the write-site observable can.
// =============================================================================

// The byte-identity proof for the amendment's defaults: a default-constructed
// Options must still EXECUTE both writes, with the identical values the
// formerly-unconditional statements wrote (iparm[7] = 0, iparm[9] = 8), so
// existing consumers -- the interior-point mapping and every default-built
// engine -- see zero change. Proven as an act, not argued from the diff.
TEST(PardisoIparmObservation, DefaultOptionsStillWriteRefinementCapZeroAndPivotPerturbEight) {
    PardisoIparmObserver::reset();
    SymmetricFactor factor{SymmetricFactor::Options{}};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::max_refinement_was_written)
        << "the default (0) is a WRITTEN value -- the don't-write state is opt-in, never the "
           "default";
    EXPECT_EQ(PardisoIparmObserver::max_refinement_written_value, 0);
    EXPECT_TRUE(PardisoIparmObserver::pivot_perturb_was_written)
        << "the default (8) is a WRITTEN value -- the don't-write state is opt-in, never the "
           "default";
    EXPECT_EQ(PardisoIparmObserver::pivot_perturb_written_value, 8);
}

// The don't-write act itself: std::nullopt must not execute either write at
// all -- pardisoinit's own iparm[7]/iparm[9] values survive -- and the
// engine still factorizes and solves normally against the inherited entries.
TEST(PardisoIparmObservation, NulloptRefinementCapAndPivotPerturbExecuteNoWriteAtAll) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.pivot_perturb_exp = std::nullopt;
    opts.max_refinement_iters = std::nullopt;
    SymmetricFactor factor{opts};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_FALSE(PardisoIparmObserver::max_refinement_was_written)
        << "nullopt must not execute the iparm[7] write at all";
    EXPECT_FALSE(PardisoIparmObserver::pivot_perturb_was_written)
        << "nullopt must not execute the iparm[9] write at all";

    // Plumbing companion: the inherited (pardisoinit-defaulted) entries
    // drive an ordinary factorize/solve -- the don't-write state is a
    // configuration choice, not a degraded engine.
    const FactorizeOutcome outcome = factor.factorize(A);
    ASSERT_EQ(outcome.status, FactorizeOutcome::Status::kOk);
    EXPECT_EQ(outcome.inertia.state, InertiaEvidence::State::kObserved);
    const Vec b = Vec::Ones(3);
    Vec x(3);
    const auto info = factor.solve(b, x);
    EXPECT_TRUE(info.refinement_iters.has_value());
    EXPECT_LT((b - Vec((spd3() * x))).norm(), 1e-10);
}

// A present non-default value in the optional writes verbatim, exactly as
// the plain int always did -- the present-value branch is the old
// unconditional statement, not a reinterpretation.
TEST(PardisoIparmObservation, PresentRefinementCapAndPivotPerturbWriteTheRequestedValues) {
    PardisoIparmObserver::reset();
    SymmetricFactor::Options opts;
    opts.pivot_perturb_exp = 10;
    opts.max_refinement_iters = 2;
    SymmetricFactor factor{opts};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::recorded);
    EXPECT_TRUE(PardisoIparmObserver::max_refinement_was_written);
    EXPECT_EQ(PardisoIparmObserver::max_refinement_written_value, 2);
    EXPECT_TRUE(PardisoIparmObserver::pivot_perturb_was_written);
    EXPECT_EQ(PardisoIparmObserver::pivot_perturb_written_value, 10);
}

// =============================================================================
// A backend-default premise a design decision elsewhere relies on
// =============================================================================

// hven's own surface leaves iparm[10] (matrix scaling) and iparm[33] (CNR
// thread count) untouched at their default Options -- that is the whole
// point of don't-write-by-default. A separate decision (the interior-point
// retarget design note) chose to treat "hven doesn't write these" and "the
// migrating engine explicitly writes 0 to these" as having IDENTICAL effect
// on this backend, rather than growing the surface a write-the-default
// semantic to make the acts match. That choice is only sound as long as
// pardisoinit's own defaults for both entries are actually 0 on the linked
// MKL -- if a future MKL version moves either default, the two acts stop
// being effect-equivalent and the decision needs revisiting.
//
// This test pins that premise, reading PardisoIparmObserver's
// post_pardisoinit_* fields -- NOT its ordinary last_*/recorded pair, which
// are read at the adapter boundary after analyze() fully returns. That
// distinction matters here specifically: this session's own phase-11 call
// (which analyze() runs as part of the very call this test makes) was
// found, empirically, to overwrite iparm[33] with its own output before
// analyze() returns, so a boundary-timed read would report phase 11's
// output, not pardisoinit's default, and this test would either assert the
// wrong thing or pass for the wrong reason. See
// FactorSession::analyze's own comment (pardiso_session.cpp) at the capture
// site for the full argument and how it was found.
TEST(BackendDefaultPremise, MklPardisoinitLeavesScalingAndCnrAtZero) {
    PardisoIparmObserver::reset();
    SymmetricFactor factor{SymmetricFactor::Options{}};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::post_pardisoinit_recorded);
    EXPECT_EQ(PardisoIparmObserver::post_pardisoinit_matrix_scaling_iparm, 0)
        << "the retarget design note's effect-parity decision for iparm[10] assumes pardisoinit "
           "leaves it at 0 on the linked MKL -- it no longer does, so that decision needs "
           "revisiting with this evidence in hand";
    EXPECT_EQ(PardisoIparmObserver::post_pardisoinit_cnr_iparm, 0)
        << "the retarget design note's effect-parity decision for iparm[33] assumes pardisoinit "
           "leaves it at 0 on the linked MKL -- it no longer does, so that decision needs "
           "revisiting with this evidence in hand";

    // Durable probe evidence, not a claim this test enforces: iparm[18]'s
    // own pardisoinit default has separately been observed (by hand, once)
    // to diverge from Intel's documented default on this MKL -- see
    // Options::collect_factor_mflops's own doc comment. Recorded here so
    // that observation lives in the test suite and is reproduced on every
    // run, rather than resting on a one-off manual probe. No EXPECT/ASSERT
    // on its value: unlike iparm[10]/iparm[33] above, no decision in this
    // codebase currently depends on which way it goes.
    const int post_pardisoinit_iparm18 =
        PardisoIparmObserver::post_pardisoinit_factor_mflops_request_iparm;
    RecordProperty("post_pardisoinit_iparm18", post_pardisoinit_iparm18);
    std::cout << "[BackendDefaultPremise] post-pardisoinit iparm[18] (Mflop-report request) = "
              << post_pardisoinit_iparm18 << " (recorded, not asserted)\n";
}

// The pardisoinit-defaults canary the two NEW don't-write states rest on
// (docs/retarget-design-sqp.md §2.4) -- load-bearing, not decorative. The
// SQP retarget's parity argument depends on the EFFECTIVE values the
// don't-write states inherit: the audited seam never wrote iparm[7] or
// iparm[9], so its effective refinement cap and pivot-perturbation exponent
// were pardisoinit's own -- observed as 2 and 8 on the audited MKL -- and
// the migrated engine's census byte-identity depends on the same effective
// values surviving. If a future MKL moves either default, this test fails
// loudly instead of the parity silently breaking, exactly the
// iparm[10]/iparm[33] precedent above applied to the two entries this seam's
// parity rests on. Same capture point and same reason as that precedent:
// only the post-pardisoinit record answers "what did pardisoinit default
// this to" (see FactorSession::analyze's own comment at the capture site).
//
// THE PINNED iparm[9] VALUE IS 8, NOT THE 13 THE DESIGN NOTE ORIGINALLY
// PREDICTED -- and this canary's own first run is what caught that. 13 is
// Intel's documented iparm[9] default for NONSYMMETRIC matrices (mtype =
// 11); for symmetric indefinite (mtype = -2, the only class this session
// ever builds) Intel documents 8, a fresh standalone pardisoinit() probe
// outside hven's session code observes 8, and the repo's own prior records
// (docs/retarget-design.md's pivot_perturb_exp row;
// docs/consumed-surface-audit.md:87-90) already said 8. The note is amended
// (docs/retarget-design-sqp.md, the three "13 on the audited build" sites)
// as a declared correction, not a silent one. Note the coincidence this
// exposes: pardisoinit's 8 EQUALS hven's own written default, so on this
// MKL the don't-write state and the written default differ as ACTS but not
// in effective value -- which is exactly why only the was_written flags,
// never a value read, can pin the act.
TEST(BackendDefaultPremise, MklPardisoinitDefaultsTheRefinementCapAndPivotPerturbExponent) {
    PardisoIparmObserver::reset();
    SymmetricFactor factor{SymmetricFactor::Options{}};
    factor.analyze(upper_csr(spd3()));

    ASSERT_TRUE(PardisoIparmObserver::post_pardisoinit_recorded);
    EXPECT_EQ(PardisoIparmObserver::post_pardisoinit_refinement_cap_iparm, 2)
        << "the SQP retarget's refinement-default parity assumes pardisoinit's full-solve "
           "refinement cap (iparm[7]) is 2 on the linked MKL -- it no longer is, so the "
           "don't-write state now inherits a DIFFERENT effective cap and the census pins that "
           "depend on it need re-deriving with this evidence in hand";
    EXPECT_EQ(PardisoIparmObserver::post_pardisoinit_pivot_perturb_iparm, 8)
        << "the SQP retarget's parity assumes pardisoinit's pivot perturbation exponent "
           "(iparm[9]) is 8 on the linked MKL (Intel's documented symmetric-indefinite default; "
           "see this test's own comment for the corrected-from-13 history) -- it no longer is, "
           "so the don't-write state now inherits a DIFFERENT effective exponent and that "
           "decision needs revisiting with this evidence in hand";
}

// ---------------------------------------------------------------------------
// The live thread-count setter lands in the config the next backend call reads
// ---------------------------------------------------------------------------

using hven::linear::detail::testing::ThreadCountObserver;

// SymmetricFactor::set_num_threads claims that a mid-life change reaches
// SUBSEQUENT backend calls while the analysis, the session and the numerics
// all stand. The second half is checkable through the public API alone (see
// SymmetricFactor.ANewThreadCountKeepsTheAnalysisTheSessionAndTheNumerics in
// tests/linear/test_symmetric_factor.cpp, and the backend-neutral trio in
// test_symmetric_factor_evidence_invariants.cpp).
//
// This test covers the first half, and covers it EXACTLY AS FAR AS THE
// BOUNDARY REACHES: what it asserts is that the new count is sitting in the
// session's configuration -- the very field FactorSession::run_phase's
// thread scope reads -- as the next solve is issued, with no re-analysis in
// between. It does NOT measure what MKL ran at, and would pass unchanged if
// that thread scope were deleted; the scope's own behaviour is covered
// separately by
// SymmetricFactor.APerInstanceThreadCountRestoresTheCallersOwnThreadLocalOverride.
// See ThreadCountObserver's own doc comment (fault_injection.h) for the
// traced data flow that joins the two, and for why closing the seam from
// inside the session file is deliberately not done.
TEST(ThreadCountObservation, ANewCountLandsInTheConfigTheNextSolveReadsWithoutReAnalyzing) {
    SymmetricFactor::Options opts;
    opts.num_threads = 1;
    SymmetricFactor factor{opts};
    const SpMatRM A = upper_csr(spd3());
    factor.analyze(A);
    ASSERT_EQ(factor.factorize(A).status, FactorizeOutcome::Status::kOk);

    const Vec b = Vec::Ones(A.rows());
    Vec x(A.rows());

    ThreadCountObserver::reset();
    factor.solve(b, x);
    ASSERT_TRUE(ThreadCountObserver::recorded);
    EXPECT_EQ(ThreadCountObserver::last_config_num_threads, 1)
        << "the configured count is what the first solve applied";

    factor.set_num_threads(2);

    ThreadCountObserver::reset();
    factor.solve(b, x);
    ASSERT_TRUE(ThreadCountObserver::recorded);
    EXPECT_EQ(ThreadCountObserver::last_config_num_threads, 2)
        << "the new count must reach the very next backend call, with no rebuild in between";
    EXPECT_EQ(factor.counters().analyze_count, 1)
        << "and must reach it without costing a symbolic analysis";
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
