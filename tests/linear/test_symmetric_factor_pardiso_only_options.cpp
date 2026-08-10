// Throw-path coverage for SymmetricFactor's Pardiso-only options:
// weighted_matching, matrix_scaling, pivot_strategy, factorization_algorithm,
// parallel_solve and cnr_threads all THROW std::invalid_argument at
// construction on the Accelerate backend for any non-default value, which
// has no equivalent concept for any of them -- never silently ignored. The
// inverse Accelerate-only option, accelerate_zero_tolerance, throws on MKL
// instead for the identical reason with the roles swapped.
//
// Options::ordering and Options::report_factor_evidence are NOT Pardiso-only
// (see hven/linear/symmetric_factor.h's own doc comments on each): every
// Ordering value maps onto a real Accelerate order method, and
// report_factor_evidence is honored -- with different per-backend evidence
// -- on both platforms. This file also asserts the inverse of what its name
// might suggest for those two options -- every value is accepted (never
// throws) on both backends. This file's name predates that surface and is
// kept for continuity with the surrounding test suite; the throwing options
// are what it is actually about now.
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
                                SymmetricFactor::Options::PivotStrategy::kE4,
                                SymmetricFactor::Options::PivotStrategy::kE6,
                                SymmetricFactor::Options::PivotStrategy::kE8,
                                SymmetricFactor::Options::PivotStrategy::kE13}) {
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

TEST(SymmetricFactorPardisoOnlyOptions, ParallelSolveTrueThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.parallel_solve = true;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, PositiveCnrThreadsThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.cnr_threads = 4;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

// accelerate_zero_tolerance is the inverse case: it is Accelerate's OWN
// option, so a present value must NOT throw here.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceDoesNotThrowOnAccelerate) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 1e-20;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

// report_factor_evidence is honored on both backends -- never throws here.
TEST(SymmetricFactorPardisoOnlyOptions, ReportFactorEvidenceDoesNotThrowOnAccelerate) {
    SymmetricFactor::Options opts;
    opts.report_factor_evidence = true;
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
                                SymmetricFactor::Options::PivotStrategy::kE4,
                                SymmetricFactor::Options::PivotStrategy::kE6,
                                SymmetricFactor::Options::PivotStrategy::kE8,
                                SymmetricFactor::Options::PivotStrategy::kE13}) {
        SymmetricFactor::Options opts;
        opts.pivot_strategy = strategy;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultFactorizationAlgorithmDoesNotThrowOnMkl) {
    for (const auto algorithm : {SymmetricFactor::Options::FactorizationAlgorithm::kClassic,
                                 SymmetricFactor::Options::FactorizationAlgorithm::kTwoLevel}) {
        SymmetricFactor::Options opts;
        opts.factorization_algorithm = algorithm;
        EXPECT_NO_THROW(SymmetricFactor{opts});
    }
}

TEST(SymmetricFactorPardisoOnlyOptions, ParallelSolveTrueDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.parallel_solve = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, PositiveCnrThreadsDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.cnr_threads = 4;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

// The inverse case: accelerate_zero_tolerance is Accelerate-only, so a
// present value MUST throw on MKL.
TEST(SymmetricFactorPardisoOnlyOptions, AccelerateZeroToleranceThrowsOnMkl) {
    SymmetricFactor::Options opts;
    opts.accelerate_zero_tolerance = 1e-20;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
}

TEST(SymmetricFactorPardisoOnlyOptions, ReportFactorEvidenceDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.report_factor_evidence = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

#endif

} // namespace
