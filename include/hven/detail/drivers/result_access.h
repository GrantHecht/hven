// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The engines' own row blocks on a result (SolveResult::EngineRows): equality and
// inequality multipliers and residuals in the engine's internal split, beside the
// public lambda and g in the problem's row space. For library internals and tests.

#include "hven/drivers/solve_result.h"

namespace hven::solvers::detail {

struct ResultAccess {
    static auto &rows(SolveResult &result) noexcept { return result.engine_rows_; }
    static const auto &rows(const SolveResult &result) noexcept { return result.engine_rows_; }
};

/// @brief The engine's own row blocks: lambda_e, lambda_i, ce, ci.
inline auto &engine_rows(SolveResult &result) noexcept { return ResultAccess::rows(result); }
inline const auto &engine_rows(const SolveResult &result) noexcept {
    return ResultAccess::rows(result);
}

} // namespace hven::solvers::detail
