// Throw-path coverage for SymmetricFactor::Options::weighted_matching, the
// one remaining Pardiso-only option: a non-default value must THROW
// std::invalid_argument at construction on the Accelerate backend, which has
// no equivalent concept -- never silently ignored.
//
// Options::ordering is NO LONGER Pardiso-only (M1 POST-FREEZE AMENDMENT,
// hven/linear/symmetric_factor.h's own doc comment on Options::ordering):
// every Ordering value now maps onto a real Accelerate order method, so this
// file also asserts the inverse of what its name might suggest for that
// option -- ALL FOUR values are accepted (never throw) on both backends.
// This file's name predates that amendment and is kept for continuity with
// the surrounding test suite; weighted_matching is the option it is actually
// about now.
//
// Platform-gated the same way the backend split itself is (src/CMakeLists.txt):
// the `#if defined(__APPLE__)` half below asserts the Accelerate acceptance/
// throw split; the `#else` half asserts the same values on the MKL platform,
// which is the backend weighted_matching actually configures. The Apple half
// is syntax-checked on Linux and also compiles and executes against the real
// framework in macOS CI; see docs/testing.md for the syntax lane's narrower
// claim ceiling.

#include <stdexcept>

#include <gtest/gtest.h>

#include "hven/linear/symmetric_factor.h"

namespace {

using hven::linear::SymmetricFactor;

#if defined(__APPLE__)

// The locked mapping's Accelerate half (frozen spec A.3): every Ordering
// value is accepted at construction on this backend now -- kBackendDefault
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

#endif

} // namespace
