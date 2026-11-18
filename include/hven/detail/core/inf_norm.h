// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// inf_norm.h -- NaN-sticky max reductions for refusal gates and keep-while-
// falling monitors, whose rule is "a NaN refuses".
//
// `v.lpNorm<Eigen::Infinity>()` is `cwiseAbs().maxCoeff()`, whose NaN behaviour
// Eigen documents as undefined; on this toolchain it reads a NaN only in the
// first element. These helpers equal it (and std::max) bit for bit on input
// without a NaN.
//
// UNDER core/, NOT detail/, so every tier may include it. The namespace is
// `hven::detail`.

#include <Eigen/Core>

namespace hven::detail {

/// ||v||inf, NaN if any element of `v` is NaN, else +inf if any is infinite;
/// 0 for an empty `v`. One pass.
template <typename Derived>
inline typename Eigen::NumTraits<typename Derived::Scalar>::Real
nan_sticky_inf_norm(const Eigen::MatrixBase<Derived> &v) {
    if (v.size() == 0) {
        return 0;
    }
    return v.cwiseAbs().template maxCoeff<Eigen::PropagateNaN>();
}

/// std::max(a, b), except that a NaN in either argument is returned.
inline double nan_sticky_max(double a, double b) {
    if (b != b) {
        return b;
    }
    return (a < b) ? b : a;
}

} // namespace hven::detail
