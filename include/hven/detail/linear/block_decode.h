// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The sign and magnitude read of one symmetric 2x2 Bunch-Kaufman D block,
// shared by DenseSymmetricFactor::block_evidence() and the dense arm's block
// walk (dense_session.cpp).

#include <algorithm>
#include <cmath>

#include "hven/core/types.h"

namespace hven::linear::detail {

// The two eigenvalues of the symmetric 2x2 block [[a11, a21], [a21, a22]]:
// their magnitudes, algebraically larger one first, and how many are
// negative. Signs come from the determinant and the trace, never from the
// eigenvalues themselves: det < 0 is one of each sign, det > 0 is two of the
// trace's sign, det == 0 is {trace, 0}. The block is first scaled by an exact
// power of two so its largest entry lies in [0.5, 1) -- nothing below can
// overflow, and every sign is that of the unscaled block. det is a difference
// of products taken with fma (Kahan), within a few ulps of the exact value, so
// its sign is right whenever det is not exactly zero. The larger magnitude is
// |trace|/2 + r, r = sqrt(((a11 - a22)/2)^2 + a21^2), a sum of non-negatives;
// the smaller is |det| over it. (trace +- sqrt(trace^2 - 4 det))/2 cancels
// when |det| << trace^2 and can return 0 or the wrong sign for the smaller
// one. Returns false iff an entry is non-finite. `det_zero` is true iff the
// computed (scaled) determinant is exactly 0.0, including the all-zero block.
struct Block2x2 {
    double abs_first = 0.0;
    double abs_second = 0.0;
    Index neg = 0;
    bool det_zero = false;
};

inline bool decode_2x2(double a11, double a21, double a22, Block2x2 &out) {
    if (!std::isfinite(a11) || !std::isfinite(a21) || !std::isfinite(a22)) {
        return false;
    }
    const double m = std::max({std::abs(a11), std::abs(a21), std::abs(a22)});
    if (m == 0.0) {
        out = Block2x2{};
        out.det_zero = true;
        return true;
    }
    int e = 0;
    (void)std::frexp(m, &e);
    const double b11 = std::ldexp(a11, -e);
    const double b21 = std::ldexp(a21, -e);
    const double b22 = std::ldexp(a22, -e);
    const double w = b21 * b21;
    const double det = std::fma(b11, b22, -w) + std::fma(-b21, b21, w);
    const double half_trace = 0.5 * (b11 + b22);
    const double half_diff = 0.5 * (b11 - b22);
    const double big = std::abs(half_trace) + std::sqrt(std::fma(half_diff, half_diff, w));
    const double small = std::abs(det) / big; // big >= 1/2 after the scaling
    const double abs_big = std::ldexp(big, e);
    const double abs_small = std::ldexp(small, e);
    if (half_trace >= 0.0) {
        out.abs_first = abs_big;
        out.abs_second = abs_small;
    } else {
        out.abs_first = abs_small;
        out.abs_second = abs_big;
    }
    out.det_zero = false;
    if (det < 0.0) {
        out.neg = 1;
    } else if (det > 0.0) {
        out.neg = half_trace < 0.0 ? 2 : 0;
    } else {
        out.neg = half_trace < 0.0 ? 1 : 0;
        out.det_zero = true;
    }
    return true;
}

} // namespace hven::linear::detail
