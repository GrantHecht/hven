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

#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/detail/linear/fault_injection.h"
#include "hven/linear/symmetric_factor.h"

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

// The MKL factorize-failure epoch test task-5-report.md's I1 disclosure
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
// exists to cover (A.5: "counts INVALID, never zero-filled"). Unlike the MKL
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
