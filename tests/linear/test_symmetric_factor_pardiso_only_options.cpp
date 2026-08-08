// Throw-path coverage for the two Pardiso-only options on
// SymmetricFactor::Options (ordering, weighted_matching): a non-default
// value must THROW std::invalid_argument at construction on the Accelerate
// backend, which has no equivalent concept for either -- never silently
// ignored.
//
// Platform-gated the same way the backend split itself is (src/CMakeLists.txt):
// the `#if defined(__APPLE__)` half below asserts the throw; the
// `#else` half asserts the inverse guard -- the exact same non-default
// values are ACCEPTED (never throw) on the MKL platform, which is the
// backend they actually configure. The Apple half is exercised for real only
// via the Accelerate syntax-check lane
// (scripts/check_accelerate_syntax_linux.sh) on this Linux development pass
// -- see docs/testing.md for what that lane does and does not prove.

#include <stdexcept>

#include <gtest/gtest.h>

#include "hven/linear/symmetric_factor.h"

namespace {

using hven::linear::SymmetricFactor;

#if defined(__APPLE__)

TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultOrderingThrowsAtConstruction) {
    SymmetricFactor::Options opts;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);

    opts.ordering = SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    EXPECT_THROW(SymmetricFactor{opts}, std::invalid_argument);
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
// the exact same non-default values that throw on Accelerate must not throw
// here.
TEST(SymmetricFactorPardisoOnlyOptions, NonDefaultOrderingDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.ordering = SymmetricFactor::Options::Ordering::kNestedDissection;
    EXPECT_NO_THROW(SymmetricFactor{opts});

    opts.ordering = SymmetricFactor::Options::Ordering::kParallelNestedDissection;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

TEST(SymmetricFactorPardisoOnlyOptions, WeightedMatchingTrueDoesNotThrowOnMkl) {
    SymmetricFactor::Options opts;
    opts.weighted_matching = true;
    EXPECT_NO_THROW(SymmetricFactor{opts});
}

#endif

} // namespace
