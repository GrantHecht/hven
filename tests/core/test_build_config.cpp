// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>

#include <hven/core/build_config.h>

HVEN_STATIC_ASSERT_EIGEN_ALIGNMENT();

// The generated constant is the alignment the build actually uses: the library's own sources
// and this test see the same value, and it is a power of two Eigen can honor.
TEST(BuildConfig, BuiltAlignmentMatchesThisTranslationUnit) {
    EXPECT_EQ(hven::kBuiltEigenMaxAlignBytes, EIGEN_MAX_ALIGN_BYTES);
    EXPECT_TRUE(hven::eigen_alignment_matches());
    EXPECT_GT(hven::kBuiltEigenMaxAlignBytes, 0);
    EXPECT_EQ(hven::kBuiltEigenMaxAlignBytes & (hven::kBuiltEigenMaxAlignBytes - 1), 0);
}
