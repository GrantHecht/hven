// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The dense arm's hot path allocates nothing (M8 DN-1, design §8 and §13 T9): counters armed
// across 100 factorize() + solve() calls on a warmed dense session. Its own executable,
// because both counters are global to the binary it links into.
//
// Two levels:
//   - a global operator new replacement, on every platform: every C++ allocation;
//   - on Linux only (HVEN_DENSE_ALLOC_WRAP_MALLOC, set by tests/CMakeLists.txt), the C
//     allocator, through the linker's --wrap of malloc, calloc, realloc, posix_memalign,
//     aligned_alloc and memalign. --wrap rewrites the references of every object in this
//     static link -- hven's own, the statically linked MKL's (LAPACK's workspace and MKL's
//     memory manager included) and this test's -- so a C-level allocation anywhere in the
//     factorize/solve path that this link resolves is counted. It does not see allocations
//     made inside shared libraries (libiomp5, libstdc++, libc), whose references were bound
//     when they were built; the operator new counter covers libstdc++'s C++ allocations.
//   - elsewhere (macOS/Accelerate, Windows) only the operator new counter runs: Accelerate's
//     LAPACK is a shared framework no link-time wrap can reach. That limitation is stated,
//     not papered over.

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>
#include <vector>

#include <gtest/gtest.h>

#include "hven/core/types.h"
#include "hven/linear/symmetric_factor.h"

namespace {
std::atomic<bool> g_armed{false};
std::atomic<long> g_count{0};
std::atomic<long> g_malloc_count{0};

void count_malloc() {
    if (g_armed.load(std::memory_order_relaxed)) {
        g_malloc_count.fetch_add(1, std::memory_order_relaxed);
    }
}
} // namespace

#ifdef HVEN_DENSE_ALLOC_WRAP_MALLOC
extern "C" {
void *__real_malloc(std::size_t size);
void *__real_calloc(std::size_t count, std::size_t size);
void *__real_realloc(void *p, std::size_t size);
int __real_posix_memalign(void **p, std::size_t alignment, std::size_t size);
void *__real_aligned_alloc(std::size_t alignment, std::size_t size);
void *__real_memalign(std::size_t alignment, std::size_t size);

void *__wrap_malloc(std::size_t size) {
    count_malloc();
    return __real_malloc(size);
}
void *__wrap_calloc(std::size_t count, std::size_t size) {
    count_malloc();
    return __real_calloc(count, size);
}
void *__wrap_realloc(void *p, std::size_t size) {
    count_malloc();
    return __real_realloc(p, size);
}
int __wrap_posix_memalign(void **p, std::size_t alignment, std::size_t size) {
    count_malloc();
    return __real_posix_memalign(p, alignment, size);
}
void *__wrap_aligned_alloc(std::size_t alignment, std::size_t size) {
    count_malloc();
    return __real_aligned_alloc(alignment, size);
}
void *__wrap_memalign(std::size_t alignment, std::size_t size) {
    count_malloc();
    return __real_memalign(alignment, size);
}
}
#endif

void *operator new(std::size_t size) {
    if (g_armed.load(std::memory_order_relaxed)) {
        g_count.fetch_add(1, std::memory_order_relaxed);
    }
    if (void *p = std::malloc(size == 0 ? 1 : size)) {
        return p;
    }
    throw std::bad_alloc();
}
void *operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
void operator delete[](void *p, std::size_t) noexcept { std::free(p); }

namespace {

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::linear::DensePolicy;
using hven::linear::FactorArm;
using hven::linear::SymmetricFactor;

TEST(DenseArmAllocation, FactorizeAndSolveAllocateNothingOnAWarmedSession) {
    // A tridiagonal indefinite matrix, n = 40.
    const Index n = 40;
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(static_cast<int>(i), static_cast<int>(i), (i % 3 == 0) ? -2.0 : 3.0);
        if (i + 1 < n) {
            t.emplace_back(static_cast<int>(i), static_cast<int>(i + 1), 1.0);
        }
    }
    SpMatRM A(n, n);
    A.setFromTriplets(t.begin(), t.end());
    A.makeCompressed();

    SymmetricFactor::Options o;
    o.dense_factorization = DensePolicy::kAlways;
    SymmetricFactor f(o);
    f.analyze(A);
    Vec b = Vec::Ones(n);
    Vec x(n);
    ASSERT_EQ(f.factorize(A).arm, FactorArm::kDense); // warm-up
    f.solve(b, x);

    g_count.store(0);
    g_malloc_count.store(0);
    g_armed.store(true);
    for (int k = 0; k < 100; ++k) {
        A.valuePtr()[0] = -2.0 - 1e-3 * k;
        (void)f.factorize(A);
        f.solve(b, x);
    }
    g_armed.store(false);
    EXPECT_EQ(g_count.load(), 0);
    EXPECT_EQ(g_malloc_count.load(), 0);
    EXPECT_EQ(f.counters().dense_factorize_count, 101);
}

#ifdef HVEN_DENSE_ALLOC_WRAP_MALLOC
// The wrap is live: a malloc and a posix_memalign issued while armed are counted, so the zero
// above is a reading, not an unwired counter. Called through volatile pointers so the
// allocation cannot be elided.
TEST(DenseArmAllocation, MallocCounterIsWired) {
    void *(*volatile do_malloc)(std::size_t) = &std::malloc;
    int (*volatile do_memalign)(void **, std::size_t, std::size_t) = &posix_memalign;
    void (*volatile do_free)(void *) = &std::free;
    g_malloc_count.store(0);
    g_armed.store(true);
    void *p = do_malloc(64);
    void *q = nullptr;
    const int rc = do_memalign(&q, 64, 64);
    g_armed.store(false);
    do_free(p);
    do_free(q);
    EXPECT_EQ(rc, 0);
    EXPECT_EQ(g_malloc_count.load(), 2);
}
#endif

} // namespace
