// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_route_reference.cpp -- M7 WX B0: the sentinel for the reference build
// (tests/common_support/route_reference.h; docs/testing.md "The reference build").
//
// Exact route pins are measured at MKL's AUTO dispatch. A run with MKL_CBWR set moves the same
// fixtures MKL's conditional-numerical-reproducibility branches move (design section 1.2), so on
// a reference build it would fail ten opaque cells. This one readable test fails first and names
// the cause.

#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "../common_support/route_reference.h"

#if !defined(USE_ACCELERATE_SPARSE) && !defined(_WIN32)
#include <mkl.h>
#endif

TEST(RouteReference, TheReferenceBuildRunsAtMklsAutoDispatch) {
#if HVEN_TEST_ROUTE_REFERENCE
    char version[256] = {0};
    ::mkl_get_version_string(version, static_cast<int>(sizeof(version)));
    ::testing::Test::RecordProperty("mkl_version", std::string(version));

    const char *cbwr = std::getenv("MKL_CBWR");
    EXPECT_TRUE(cbwr == nullptr)
        << "MKL_CBWR=" << (cbwr ? cbwr : "")
        << " is set on a reference build. Every exact ROUTE pin guarded by "
           "HVEN_TEST_ROUTE_REFERENCE (the QpNc4ConvexCertificate, and later QpE2WorkingSet, "
           "QpNc3SaddleExit, QpStagnationStop, ... route pins) was measured at MKL's AUTO "
           "dispatch and is invalid under a forced CBWR branch. Unset MKL_CBWR.";
#else
    GTEST_SKIP() << "not a reference build (Linux x86-64 clang MKL): no route pins are asserted "
                    "here, so there is no dispatch to guard";
#endif
}
