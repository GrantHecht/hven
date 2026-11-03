// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// SolveResult::message, built once per solve in the public solver handles after
// the engine returns: one line, from the status, the engine's own stop facts and
// the options that set the limits. Informational; nothing parses it.

#include <string>

#include "hven/core/solver_status.h"
#include "hven/drivers/solve_result.h"

namespace hven::solvers {

struct IpmOptions;
struct SqpOptions;
struct SqpResult;

namespace detail {

/// @brief The message of an interior-point solve.
/// @param result The engine's result (status, returned_best_iterate).
/// @param reason The stop reason of the last phase that ran.
/// @param options The options the solve ran under.
/// @param budget The per-call budget the solve ran under.
std::string ipm_result_message(const SolveResult &result, IpmStopReason reason,
                               const IpmOptions &options, const SolveBudget &budget);

/// @brief The message of an SQP solve.
std::string sqp_result_message(const SqpResult &result, const SqpOptions &options,
                               const SolveBudget &budget);

} // namespace detail
} // namespace hven::solvers
