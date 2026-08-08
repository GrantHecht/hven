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
