// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// THE ABSENT-BOUND RULE (CommonOptions::infinite_bound).
//
// A declared bound at or beyond the threshold IS the infinity of its sign:
//   b >= +infinite_bound  ->  +inf        b <= -infinite_bound  ->  -inf
// and every other value is kept verbatim. So a lower bound of -1e20 and an upper
// bound of +1e20 are absent at the default threshold, and the rest of the library
// sees only -inf / +inf for them. A bound at infinity on the WRONG side (a lower
// bound at +1e20, an upper bound at -1e20) becomes the infinity that describes no
// interval, and is refused by the same checks that refuse a lower bound of +inf
// today (an equality at infinity, a variable fixed at infinity, lower above upper).
// NaN passes through unchanged and is refused by name by the caller.
//
// Applied once per route, where a declaration's bounds enter an engine's
// transcription: NlpProblemModel (an Nlp's variable and row bounds),
// NlpModelAssembly (the SQP's variable box over a model) and NonLinearProgram's
// set_variable_bound / update_variable_bound_values (the interior-point box,
// every route). The threshold is validated > 0 (and not NaN) by the options;
// infinity means "every finite value is a real bound", and is the identity here.

#include <cmath>
#include <limits>
#include <string>

#include <fmt/format.h>

#include "hven/core/types.h"

namespace hven::solvers::detail {

/// CommonOptions{}.infinite_bound, for the layers that are built before any
/// options exist (a Problem's first transcription, the internal routes). Tied to
/// the public default by a static_assert in src/drivers/common_options.cpp.
inline constexpr double kDefaultInfiniteBound = 1e20;

/// The bound @p b as the rule reads it under @p infinite_bound.
inline double normalize_bound(double b, double infinite_bound) noexcept {
    if (b >= infinite_bound) {
        return std::numeric_limits<double>::infinity();
    }
    if (b <= -infinite_bound) {
        return -std::numeric_limits<double>::infinity();
    }
    return b;
}

/// True when the finite value @p b is read as an infinity (the values counted as
/// "absent bounds" in the statistics and the solve-begin trace events).
inline bool reads_as_absent(double b, double infinite_bound) noexcept {
    return std::isfinite(b) && (b >= infinite_bound || b <= -infinite_bound);
}

/// Normalizes every entry of @p v in place; returns how many finite entries were
/// read as an infinity.
template <class V> Index normalize_bounds(V &v, double infinite_bound) noexcept {
    Index absent = 0;
    for (Index i = 0; i < v.size(); ++i) {
        if (reads_as_absent(v[i], infinite_bound)) {
            ++absent;
        }
        v[i] = normalize_bound(v[i], infinite_bound);
    }
    return absent;
}

/// The clause a refusal appends when the threshold changed what the caller wrote:
/// empty when neither raw value was read as an infinity.
inline std::string absent_bound_clause(double raw_lower, double raw_upper, double infinite_bound) {
    if (!reads_as_absent(raw_lower, infinite_bound) &&
        !reads_as_absent(raw_upper, infinite_bound)) {
        return {};
    }
    return fmt::format(" (read as [{}, {}]: a bound at or beyond infinite_bound = {} is the "
                       "infinity of its sign)",
                       normalize_bound(raw_lower, infinite_bound),
                       normalize_bound(raw_upper, infinite_bound), infinite_bound);
}

} // namespace hven::solvers::detail
