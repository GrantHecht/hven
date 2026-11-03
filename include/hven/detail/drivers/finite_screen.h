// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The non-finite screen both engines run on model output (M8 B5): a NaN or an
// infinity in an objective value, a constraint value or a derivative entry is a
// failed evaluation, exactly as a thrown EvaluationError is (core/errors.h).
//
// all_finite() reads the exponent field of each value, not its floating-point
// meaning: a double is NaN or infinite exactly when its eleven exponent bits are
// all set. The test is integer arithmetic, so no floating-point mode the library
// is built under (the fast-math family included) can fold it away, and the loop
// has no early exit, so it vectorises: about one instruction per value.

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace hven::solvers::detail {

/// @brief True when none of the @p count values at @p values is NaN or infinite.
inline bool all_finite(const double *values, std::size_t count) noexcept {
    constexpr std::uint64_t kExponent = 0x7ff0000000000000ULL;
    std::uint64_t non_finite = 0;
    for (std::size_t i = 0; i < count; ++i) {
        std::uint64_t bits;
        std::memcpy(&bits, values + i, sizeof bits);
        non_finite |= static_cast<std::uint64_t>((bits & kExponent) == kExponent);
    }
    return non_finite == 0;
}

/// @brief all_finite() over an Eigen dense vector or contiguous block.
template <class Vector> bool all_finite(const Vector &v) noexcept {
    return all_finite(v.data(), static_cast<std::size_t>(v.size()));
}

/// @brief True when @p value is neither NaN nor infinite.
inline bool is_finite(double value) noexcept { return all_finite(&value, 1); }

/// @brief Throws EvaluationError with the message "non-finite <quantity> at a trial
///        point". The interior-point engine's trial evaluators call it when the
///        screen fails, so a non-finite trial takes the same rejection path a
///        thrown EvaluationError does.
[[noreturn]] void throw_non_finite_trial(const char *quantity);

} // namespace hven::solvers::detail
