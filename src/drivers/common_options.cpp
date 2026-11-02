// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The one validator of CommonOptions. Every numeric check is the NEGATION of its
// acceptance condition, so a NaN (which compares false against everything) is
// refused; the library builds with -fno-finite-math-only for exactly this (see
// the banner of src/drivers/sqp_options.cpp). There is no floating-point
// arithmetic here, only comparisons and values handed to fmt.

#include "hven/detail/drivers/common_options_validate.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers::detail {

namespace {

template <typename E> void enum_in_range(E v, E last, const char *name) {
    const auto i = static_cast<long long>(v);
    if (i < 0 || i > static_cast<long long>(last))
        throw std::invalid_argument(fmt::format("{} is out of range, got {}", name, i));
}

void finite_positive(double v, const char *name) {
    if (!std::isfinite(v) || !(v > 0.0))
        throw std::invalid_argument(fmt::format("{} must be finite and > 0, got {}", name, v));
}

} // namespace

void validate_common_options(const CommonOptions &c, CommonOptionsFloors floors) {
    finite_positive(c.optimality_tol, "common.optimality_tol");
    finite_positive(c.feasibility_tol, "common.feasibility_tol");
    constexpr Index kMaxIterations = std::numeric_limits<int>::max();
    if (c.max_iterations < floors.max_iterations || c.max_iterations > kMaxIterations)
        throw std::invalid_argument(fmt::format("common.max_iterations must be in [{}, {}], got {}",
                                                floors.max_iterations, kMaxIterations,
                                                c.max_iterations));
    if (c.max_factorizations && *c.max_factorizations < 0)
        throw std::invalid_argument(fmt::format(
            "common.max_factorizations must be empty or >= 0, got {}", *c.max_factorizations));
    if (c.time_limit_seconds &&
        (!std::isfinite(*c.time_limit_seconds) || !(*c.time_limit_seconds > 0.0)))
        throw std::invalid_argument(
            fmt::format("common.time_limit_seconds must be empty or finite and > 0, got {}",
                        *c.time_limit_seconds));
    enum_in_range(c.verbosity, Verbosity::kIterations, "common.verbosity");
    if (c.threads < floors.threads)
        throw std::invalid_argument(
            fmt::format("common.threads must be >= {}, got {}", floors.threads, c.threads));
    enum_in_range(c.start_level, StartLevel::kHot, "common.start_level");
    if (!(c.infinite_bound > 0.0))
        throw std::invalid_argument(fmt::format(
            "common.infinite_bound must be > 0 (kInfinity is legal), got {}", c.infinite_bound));
}

} // namespace hven::solvers::detail
