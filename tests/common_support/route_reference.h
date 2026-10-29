// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#ifndef HVEN_TESTS_SUPPORT_ROUTE_REFERENCE_H
#define HVEN_TESTS_SUPPORT_ROUTE_REFERENCE_H

// The build every exact ROUTE pin was measured on (M7 WX design, docs/notes/
// 2026-10-m7-wx-ci-design.md section 2.1): Linux, x86-64, clang, MKL Pardiso at its AUTO
// dispatch. A route -- minors, factorizations, dep_* / certificate / saddle / latch counters,
// the status of a capped or knife-edge fixture -- is pinned there and nowhere else; off it the
// cell asserts its portable contract and RECORDS the route. This is not a backend switch:
// Windows runs MKL and is off-reference, so are macOS/Accelerate and any non-clang compiler.
//
// The arm shape (docs/testing.md "The reference build"):
//
//     #if HVEN_TEST_ROUTE_REFERENCE
//         <the exact pin, verbatim>
//     #else
//         hven::test_support::record_off_reference_route("<key>", "<observed counters>");
//         <the portable contract>
//     #endif
//
// Reached as "../common_support/route_reference.h" (the console_capture.h precedent).
// MKL_CBWR set on a reference build puts it off-reference in fact while the macro still says 1;
// the sentinel test RouteReference.TheReferenceBuildRunsAtMklsAutoDispatch fails that case.

#include <string>

#include <gtest/gtest.h>

#if defined(__linux__) && defined(__x86_64__) && defined(__clang__) &&                             \
    !defined(USE_ACCELERATE_SPARSE)
#define HVEN_TEST_ROUTE_REFERENCE 1
#else
#define HVEN_TEST_ROUTE_REFERENCE 0
#endif

namespace hven::test_support {

/// @brief Records the route observed off the reference build as a gtest property
///        `<key>_route` (it lands in the gtest XML), tagged so a reader cannot mistake it for
///        a pin. Call inside a TEST body.
inline void record_off_reference_route(const std::string &key, const std::string &observed) {
    ::testing::Test::RecordProperty(key + "_route",
                                    "UNOBSERVED-ROUTE off the reference build: " + observed);
}

} // namespace hven::test_support

#endif // HVEN_TESTS_SUPPORT_ROUTE_REFERENCE_H
