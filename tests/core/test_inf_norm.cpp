// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// nan_sticky_inf_norm and nan_sticky_max (hven/core/detail/inf_norm.h): a NaN at ANY index
// reads NaN -- including past every vectorization width and in the scalar tail -- and on
// input without a NaN the result is bit-identical to lpNorm<Infinity> / std::max.

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

#include <Eigen/Core>
#include <gtest/gtest.h>

#include <hven/core/detail/inf_norm.h>

namespace {

using hven::detail::nan_sticky_inf_norm;
using hven::detail::nan_sticky_max;
using Vec = Eigen::VectorXd;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

// Sizes spanning one element, the 2/4/8-lane packet widths, unrolled bodies and scalar tails.
constexpr int kSizes[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 64, 100, 1001};

Vec ramp(Eigen::Index n) {
    Vec v(n);
    for (Eigen::Index i = 0; i < n; ++i) {
        v(i) = (i % 2 == 0 ? 1.0 : -1.0) * (1.0 + 0.001 * static_cast<double>((i * 37) % n));
    }
    return v;
}

std::uint64_t bits(double x) { return std::bit_cast<std::uint64_t>(x); }

TEST(NanStickyInfNorm, ANaNAtEveryIndexReadsNaN) {
    for (const int n : kSizes) {
        for (int k = 0; k < n; ++k) {
            Vec v = ramp(n);
            v(k) = kNaN;
            EXPECT_TRUE(std::isnan(nan_sticky_inf_norm(v))) << "n=" << n << " k=" << k;
        }
    }
}

TEST(NanStickyInfNorm, ANaNFirstMiddleOrLastReadsNaNThroughExpressions) {
    for (const int n : kSizes) {
        const Vec d = Vec::Constant(n, 0.5);
        for (const int k : {0, n / 2, n - 1}) {
            Vec v = ramp(n);
            v(k) = kNaN;
            EXPECT_TRUE(std::isnan(nan_sticky_inf_norm(v.cwiseProduct(d)))) << n << " " << k;
            EXPECT_TRUE(std::isnan(nan_sticky_inf_norm(Vec(v - d)))) << n << " " << k;
            if (k > 0) {
                EXPECT_TRUE(std::isnan(nan_sticky_inf_norm(v.tail(n - 1)))) << n << " " << k;
            }
        }
    }
}

TEST(NanStickyInfNorm, AnInfReadsInfAndANaNBesideAnInfReadsNaN) {
    for (const int n : kSizes) {
        for (const int k : {0, n / 2, n - 1}) {
            Vec v = ramp(n);
            v(k) = -kInf;
            EXPECT_EQ(nan_sticky_inf_norm(v), kInf) << n << " " << k;
            if (n > 1) {
                v((k + 1) % n) = kNaN;
                EXPECT_TRUE(std::isnan(nan_sticky_inf_norm(v))) << n << " " << k;
            }
        }
    }
}

TEST(NanStickyInfNorm, AnEmptyVectorReadsZero) {
    EXPECT_EQ(nan_sticky_inf_norm(Vec(0)), 0.0);
    const Vec v = Vec::Ones(4);
    EXPECT_EQ(nan_sticky_inf_norm(v.head(0)), 0.0);
}

TEST(NanStickyInfNorm, OnFiniteInputItEqualsLpNormBitForBit) {
    std::mt19937_64 gen(20261111);
    std::uniform_real_distribution<double> mant(-1.0, 1.0);
    std::uniform_int_distribution<int> expo(-300, 300);
    for (const int n : kSizes) {
        for (int rep = 0; rep < 20; ++rep) {
            Vec v(n);
            for (int i = 0; i < n; ++i) {
                v(i) = std::ldexp(mant(gen), expo(gen));
            }
            if (rep == 0) {
                v.setZero();
                v(n - 1) = -0.0;
            }
            if (rep == 1) {
                v(n / 2) = std::numeric_limits<double>::denorm_min();
            }
            const double want = v.lpNorm<Eigen::Infinity>();
            EXPECT_EQ(bits(nan_sticky_inf_norm(v)), bits(want)) << "n=" << n << " rep=" << rep;
            const Vec d = v.reverse();
            EXPECT_EQ(bits(nan_sticky_inf_norm(v.cwiseProduct(d))),
                      bits(v.cwiseProduct(d).lpNorm<Eigen::Infinity>()));
        }
    }
}

TEST(NanStickyMax, ANaNInEitherArgumentIsKept) {
    EXPECT_TRUE(std::isnan(nan_sticky_max(kNaN, 1.0)));
    EXPECT_TRUE(std::isnan(nan_sticky_max(1.0, kNaN)));
    EXPECT_TRUE(std::isnan(nan_sticky_max(kNaN, kInf)));
    EXPECT_TRUE(std::isnan(nan_sticky_max(kInf, kNaN)));
    // As a running fold: a NaN anywhere survives every later element.
    for (const int n : kSizes) {
        for (const int k : {0, n / 2, n - 1}) {
            Vec v = ramp(n);
            v(k) = kNaN;
            double m = 0.0;
            for (int i = 0; i < n; ++i) {
                m = nan_sticky_max(m, std::abs(v(i)));
            }
            EXPECT_TRUE(std::isnan(m)) << n << " " << k;
        }
    }
}

TEST(NanStickyMax, WithoutANaNItIsStdMax) {
    const double xs[] = {-kInf, -2.0, -0.0, 0.0, 1e-300, 1.0, 3.5, kInf};
    for (const double a : xs) {
        for (const double b : xs) {
            EXPECT_EQ(bits(nan_sticky_max(a, b)), bits(std::max(a, b))) << a << " " << b;
        }
    }
}

} // namespace
