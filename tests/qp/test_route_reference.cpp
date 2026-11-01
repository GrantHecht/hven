// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_route_reference.cpp -- M7 WX B0: the sentinel for the reference build
// (tests/common_support/route_reference.h; docs/testing.md "The reference build").
//
// Exact route pins are measured at MKL's AUTO dispatch on the derivation box's CPU
// (mkl_dispatch_signature.h). Two things put a reference build off-reference in fact while
// HVEN_TEST_ROUTE_REFERENCE still says 1, and each would fail a scatter of opaque route cells:
// MKL_CBWR set (a forced CNR branch moves the same fixtures, design section 1.2), and the binary
// running on a host whose dispatch is not the one tests/CMakeLists.txt probed at configure time
// (an AVX-512 runner moved 39 cells, WX-R13). This one readable test fails first and names the
// cause.

#include <cstdlib>
#include <string>

#include <gtest/gtest.h>

#include "../common_support/route_reference.h"

#if !defined(USE_ACCELERATE_SPARSE) && !defined(_WIN32)
#include <mkl.h>
#endif
#if defined(__linux__) && defined(__x86_64__) && !defined(USE_ACCELERATE_SPARSE)
#include "../common_support/mkl_dispatch_signature.h"
#define HVEN_TEST_HAS_MKL_DISPATCH_SIGNATURE 1
#endif

TEST(RouteReference, TheReferenceBuildRunsAtMklsAutoDispatch) {
#if defined(HVEN_TEST_HAS_MKL_DISPATCH_SIGNATURE)
    const hven::test_support::MklDispatchSignature sig = hven::test_support::observe_mkl_dispatch();
    ::testing::Test::RecordProperty("mkl_dispatch", hven::test_support::describe(sig));
#endif
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

    EXPECT_TRUE(hven::test_support::is_reference_mkl_dispatch(sig))
        << "this reference build is running on a host whose MKL dispatch is not the one the "
           "route pins were measured at: "
        << hven::test_support::describe(sig)
        << ". tests/CMakeLists.txt probed a reference host at configure time; this binary was "
           "carried elsewhere. Every HVEN_TEST_ROUTE_REFERENCE pin is invalid here -- "
           "reconfigure on this host, which selects the portable arms.";
#else
    GTEST_SKIP() << "not a reference build (Linux x86-64 clang MKL at the derivation box's "
                    "dispatch; see the mkl_dispatch property): no route pins are asserted "
                    "here, so there is no dispatch to guard";
#endif
}
