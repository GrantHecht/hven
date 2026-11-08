// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The dense arm's hot path allocates nothing (M8 DN-1, design §8 and §13 T9): a global
// operator new replacement counts allocations while armed, across 100 factorize() + solve()
// calls on a warmed dense session. Its own executable, because the replacement is global to
// the binary it links into.

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
} // namespace

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
    g_armed.store(true);
    for (int k = 0; k < 100; ++k) {
        A.valuePtr()[0] = -2.0 - 1e-3 * k;
        (void)f.factorize(A);
        f.solve(b, x);
    }
    g_armed.store(false);
    EXPECT_EQ(g_count.load(), 0);
    EXPECT_EQ(f.counters().dense_factorize_count, 101);
}

} // namespace
