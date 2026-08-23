// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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
// (matrix_scaling + weighted_matching, cnr_threads + ordering,
// factorization_algorithm + ordering, factorization_algorithm +
// matrix_scaling/weighted_matching) that only exist to validate on the
// backend where the individual fields are accepted at all -- see
// symmetric_factor_mkl.cpp's constructor for the Intel documentation
// citations backing each one.
//
// accelerate_zero_tolerance's validity checks (finite, > 0) run on
// Accelerate regardless of platform -- construction-validated here even
// though this box has no real Accelerate framework to link against, and
// executed for real in macOS CI. Whether the accepted value is forwarded
// to Accelerate's own zeroTolerance field EXACTLY is unobservable from
// this environment: the Accelerate syntax-check lane
// (scripts/check_accelerate_syntax_linux.sh) compiles with `-fsyntax-only`
// against a hand-written stub and never links or runs an executable, so
// there is no seam here to assert a forwarded value against -- only
// construction-time acceptance/rejection is covered below.
//
// Platform-gated the same way the backend split itself is (src/CMakeLists.txt):
// the `#if defined(__APPLE__)` half below asserts the Accelerate acceptance/
// throw split; the `#else` half asserts the same values on the MKL platform,
// which is the backend most of these options actually configure. The Apple
// half is syntax-checked on Linux and also compiles and executes against the
// real framework in macOS CI; see docs/testing.md for the syntax lane's
// narrower claim ceiling.

#include <limits>
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
// option, so a present, VALID value must NOT throw here.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceDoesNotThrowOnAccelerate) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 1e-20;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

// A present value that fails its own validity check (finite, > 0) still
// throws on Accelerate -- being this backend's OWN option does not mean
// every value is accepted, only that a well-formed one is not rejected
// merely for being Pardiso-foreign.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceRejectsZero) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 0.0;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceRejectsNegative) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = -1e-20;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceRejectsNaN) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceRejectsPositiveInfinity) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = std::numeric_limits<double>::infinity();
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceRejectsNegativeInfinity) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = -std::numeric_limits<double>::infinity();
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
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

// matrix_scaling requires weighted_matching on this backend's matrix type
// (mtype = -2) -- see symmetric_factor_mkl.cpp's constructor for the
// Intel citation. Scaling alone THROWS; scaling with matching does not.
TEST(SymmetricFactorPardisoOnlyOptions, MatrixScalingTrueAloneThrowsOnMkl) {
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, MatrixScalingTrueWithWeightedMatchingDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.matrix_scaling = true;
    opts.weighted_matching = true;
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
// weighted_matching is set alongside matrix_scaling here specifically so
// that matrix_scaling's own requires-weighted_matching precondition (see
// its doc comment) is satisfied -- this isolates the two-level-forbids-
// scaling rule under test from that separate rule, which would otherwise
// throw first for a different reason and make this test pass without
// actually exercising the two-level interaction.
TEST(SymmetricFactorPardisoOnlyOptions, TwoLevelFactorizationAlgorithmThrowsWithScalingOnMkl) {
    SymmetricFactor::Options opts;
    opts.factorization_algorithm = SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    opts.matrix_scaling = true;
    opts.weighted_matching = true;
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
