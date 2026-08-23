// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <cstring>

#include <gtest/gtest.h>

#include "hven/core/version.h"

#ifndef HVEN_VERSION_STRING
#error "HVEN_VERSION_STRING must be defined by the build (see tests/CMakeLists.txt)"
#endif

// Compares hven::version() against the same HVEN_VERSION_STRING macro
// version.cpp is built with (both derived from this CMake configuration's
// project(hven VERSION ...) at configure time) rather than a hardcoded
// literal, so a version() implementation that silently drifts from
// PROJECT_VERSION — e.g. a stale hand-copied literal reintroduced later —
// fails this test instead of passing tautologically.
TEST(Version, NonNullNonEmptyMatchesProjectVersion) {
    const char *v = hven::version();
    ASSERT_NE(v, nullptr);
    EXPECT_GT(std::strlen(v), 0u);
    EXPECT_STREQ(v, HVEN_VERSION_STRING);
}
