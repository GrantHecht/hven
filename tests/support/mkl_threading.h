// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// HVEN_MKL_THREADING=sequential (PF-6) links mkl_sequential: every MKL call is single-threaded and
// mkl_set_num_threads_local() is accepted and ignored, so its previous-value / observed-width
// contract cannot be observed. A test whose PREMISE is that MKL honours a thread-count override
// skips itself on that layer with its reason; nothing else about such a build is asserted away.
// HVEN_TEST_MKL_SEQUENTIAL is set by tests/CMakeLists.txt (0 or 1, always defined).
#if !defined(HVEN_TEST_MKL_SEQUENTIAL)
#error "HVEN_TEST_MKL_SEQUENTIAL is set by tests/CMakeLists.txt"
#endif

#if HVEN_TEST_MKL_SEQUENTIAL
#define HVEN_SKIP_IF_MKL_SEQUENTIAL() \
    GTEST_SKIP() << "HVEN_MKL_THREADING=sequential: MKL ignores thread-count overrides"
#else
#define HVEN_SKIP_IF_MKL_SEQUENTIAL() static_cast<void>(0)
#endif
