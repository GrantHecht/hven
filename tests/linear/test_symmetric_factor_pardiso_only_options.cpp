// Throw-path coverage for SymmetricFactor's Pardiso-only options:
// weighted_matching, matrix_scaling, pivot_strategy, factorization_algorithm,
// solve_parallelism, cnr_threads and collect_factor_mflops all THROW
// std::invalid_argument at construction on the Accelerate backend for any
// non-default value, which has no equivalent concept for any of them --
// never silently ignored. The inverse Accelerate-only option,
// accelerate_zero_tolerance, throws on MKL instead for the identical reason
// with the roles swapped.
//
// Options::ordering is NOT Pardiso-only (see
// hven/linear/symmetric_factor.h's own doc comment on it): every Ordering
// value maps onto a real Accelerate order method. This file also asserts
// the inverse of what its name might suggest for that option -- every
// value is accepted (never throws) on both backends. This file's name
// predates that surface and is kept for continuity with the surrounding
// test suite; the throwing options are what it is actually about now.
//
// The MKL half additionally covers the documented MULTI-OPTION interactions
// (cnr_threads + ordering, factorization_algorithm + ordering,
// factorization_algorithm + matrix_scaling/weighted_matching) that only
// exist to validate on the backend where the individual fields are accepted
// at all -- see symmetric_factor_mkl.cpp's constructor for the Intel
// documentation citations backing each one.
//
// Platform-gated the same way the backend split itself is (src/CMakeLists.txt):
// the `#if defined(__APPLE__)` half below asserts the Accelerate acceptance/
// throw split; the `#else` half asserts the same values on the MKL platform,
// which is the backend most of these options actually configure. The Apple
// half is syntax-checked on Linux and also compiles and executes against the
// real framework in macOS CI; see docs/testing.md for the syntax lane's
// narrower claim ceiling.

#include <stdexcept>

#include <gtest/gtest.h>

#include "hven/linear/symmetric_factor.h"

namespace {

using hven::linear::SymmetricFactor;

#if defined(__APPLE__)

// The backend-neutral ordering mapping's Accelerate half: every Ordering
// value is accepted at construction on this backend -- kBackendDefault
// -> SparseOrderDefault, kMinimumDegree -> SparseOrderAMD, kNestedDissection
// -> SparseOrderMetis, kParallelNestedDissection -> SparseOrderMTMetis (with
// the OS-availability downgrade to SparseOrderMetis where the host lacks it,
// exercised inside accelerate_ordering_code() at analyze() time, not here at
// construction).
TEST(SymmetricFactorPardisoOnlyOptions, AllOrderingValuesAreAcceptedAtConstructionOnAccelerate) {
    for (const auto ordering : {SymmetricFactor::Options::Ordering::kBackendDefault,
                                SymmetricFactor::Options::Ordering::kMinimumDegree,
                                SymmetricFactor::Options::Ordering::kNestedDissection,
                                SymmetricFactor::Options::Ordering::kParallelNestedDissection}) {
        SymmetricFactor::Options opts;
        opts.ordering = ordering;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, WeightedMatchingTrueThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.weighted_matching = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, MatrixScalingTrueThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultPivotStrategyThrowsAtConstruction) {
    for (const auto strategy : {SymmetricFactor::Options::PivotStrategy::kOneByOne,
                                SymmetricFactor::Options::PivotStrategy::kTwoByTwo,
                                SymmetricFactor::Options::PivotStrategy::kOneByOneNoAutoRefine,
                                SymmetricFactor::Options::PivotStrategy::kTwoByTwoNoAutoRefine}) {
        SymmetricFactor::Options opts;
        opts.pivot_strategy = strategy;
        EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultFactorizationAlgorithmThrowsAtConstruction) {
    for (const auto algorithm : {SymmetricFactor::Options::FactorizationAlgorithm::kClassic,
                                 SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel}) {
        SymmetricFactor::Options opts;
        opts.factorization_algorithm = algorithm;
        EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultSolveParallelismThrowsAtConstruction) {
    for (const auto mode : {SymmetricFactor::Options::SolveParallelism::kAdaptivePartitioning,
                            SymmetricFactor::Options::SolveParallelism::kSequential,
                            SymmetricFactor::Options::SolveParallelism::kMatrixPartitionParallel}) {
        SymmetricFactor::Options opts;
        opts.solve_parallelism = mode;
        EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, PositiveCnrThreadsThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.cnr_threads = 4;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, CollectFactorMflopsTrueThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.collect_factor_mflops = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

// accelerate_zero_tolerance is the inverse case: it is Accelerate's OWN
// option, so a present value must NOT throw here.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceDoesNotThrowOnAccelerate) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 1e-20;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, DefaultOptionsDoNotThrow) {
    EXPECT_NO_THROW(SymmetricFactor{SymmetricFactor::Options{}});
}

#else // !defined(__APPLE__)

// The inverse guard: on the MKL platform these are real Pardiso options, so
// the exact same non-default values accepted on Accelerate must not throw
// here either.
TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultOrderingDoesNotThrowOnMkl) {
    for (const auto ordering : {SymmetricFactor::Options::Ordering::kMinimumDegree,
                                SymmetricFactor::Options::Ordering::kNestedDissection,
                                SymmetricFactor::Options::Ordering::kParallelNestedDissection}) {
        SymmetricFactor::Options opts;
        opts.ordering = ordering;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, WeightedMatchingTrueDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.weighted_matching = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, MatrixScalingTrueDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultPivotStrategyDoesNotThrowOnMkl) {
    for (const auto strategy : {SymmetricFactor::Options::PivotStrategy::kOneByOne,
                                SymmetricFactor::Options::PivotStrategy::kTwoByTwo,
                                SymmetricFactor::Options::PivotStrategy::kOneByOneNoAutoRefine,
                                SymmetricFactor::Options::PivotStrategy::kTwoByTwoNoAutoRefine}) {
        SymmetricFactor::Options opts;
        opts.pivot_strategy = strategy;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

// kClassic never carries the two-level ordering/scaling/matching
// restriction (it's not the two-level algorithm), so it does not throw at
// kBackendDefault ordering. kTwoLevel is exercised separately below, at an
// ordering that actually satisfies its own restriction, so this test does
// not conflate "not Pardiso-only" with "no other requirement" -- see the
// interaction tests further down.
TEST(SymmetricFactorPardisoOnlyOptions, ClassicFactorizationAlgorithmDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kClassic;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions,
     TwoLevelFactorizationAlgorithmDoesNotThrowWithCompatibleOrdering) {
    SymmetricFactor::Options opts;
    opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    EXPECT_NO_THROW(SymmetricFactor{opts});

    opts.ordering = SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultSolveParallelismDoesNotThrowOnMkl) {
    for (const auto mode : {SymmetricFactor::Options::SolveParallelism::kAdaptivePartitioning,
                            SymmetricFactor::Options::SolveParallelism::kSequential,
                            SymmetricFactor::Options::SolveParallelism::kMatrixPartitionParallel}) {
        SymmetricFactor::Options opts;
        opts.solve_parallelism = mode;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

// Positive cnr_threads alone (with the ordering CNR actually requires) does
// not throw -- the throw tested below is specifically about the ordering
// interaction, not about cnr_threads being positive per se.
TEST(SymmetricFactorPardisoOnlyOptions, PositiveCnrThreadsDoesNotThrowOnMklWithCompatibleOrdering) {
    SymmetricFactor::Options opts;
    opts.cnr_threads = 4;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, CollectFactorMflopsTrueDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.collect_factor_mflops = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

// The inverse case: accelerate_zero_tolerance is Accelerate-only, so a
// present value MUST throw on MKL.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceThrowsOnMkl) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 1e-20;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

// =============================================================================
// Documented MKL option interactions (symmetric_factor_mkl.cpp's
// constructor) -- Intel citations live there, not repeated per-test here.
// =============================================================================

// CNR mode is documented reproducible only under the non-parallel nested
// dissection ordering (iparm[1] = 2). Every other Ordering value throws,
// INCLUDING kBackendDefault, whose floating value coincides with the
// documented-incompatible parallel variant on the MKL this was verified
// against -- see cnr_threads' own doc comment.
TEST(SymmetricFactorPardisoOnlyOptions, PositiveCnrThreadsThrowsWithIncompatibleOrderingOnMkl) {
    for (const auto ordering : {SymmetricFactor::Options::Ordering::kBackendDefault,
                                SymmetricFactor::Options::Ordering::kMinimumDegree,
                                SymmetricFactor::Options::Ordering::kParallelNestedDissection}) {
        SymmetricFactor::Options opts;
        opts.cnr_threads = 4;
        opts.ordering = ordering;
        EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
    }
}

// The two-level factorization algorithm only supports nested dissection
// orderings (iparm[1] = 2 or 3). kBackendDefault and kMinimumDegree throw.
TEST(SymmetricFactorPardisoOnlyOptions,
     TwoLevelFactorizationAlgorithmThrowsWithIncompatibleOrderingOnMkl) {
    for (const auto ordering : {SymmetricFactor::Options::Ordering::kBackendDefault,
                                SymmetricFactor::Options::Ordering::kMinimumDegree}) {
        SymmetricFactor::Options opts;
        opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel;
        opts.ordering = ordering;
        EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
    }
}

// The two-level factorization algorithm is documented incompatible with
// scaling and matching, even with an otherwise-compatible ordering.
TEST(SymmetricFactorPardisoOnlyOptions, TwoLevelFactorizationAlgorithmThrowsWithScalingOnMkl) {
    SymmetricFactor::Options opts;
    opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    opts.matrix_scaling = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, TwoLevelFactorizationAlgorithmThrowsWithMatchingOnMkl) {
    SymmetricFactor::Options opts;
    opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    opts.weighted_matching = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

#endif

} // namespace
