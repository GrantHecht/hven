// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// SolveResult::message, built once per solve in the public solver handles after
// the engine returns: one line, from the status, the engine's own stop facts and
// the options that set the limits. Informational; nothing parses it.

#include <exception>
#include <string>

#include "hven/core/solver_status.h"
#include "hven/drivers/solve_result.h"

namespace hven {
struct IpmOptions;
struct SqpOptions;
struct SqpResult;
} // namespace hven

namespace hven::detail {

/// @brief The message of an interior-point solve.
/// @param result The engine's result (status, returned_best_iterate).
/// @param reason The stop reason of the last phase that ran.
/// @param detail The engine's words for @p reason (IpmSolverImpl::
///        last_stop_detail()): the non-finite quantity, the last evaluation
///        failure, the exception's description, why an acceptable stop came
///        early. May be empty.
/// @param options The options the solve ran under.
/// @param budget The per-call budget the solve ran under.
std::string ipm_result_message(const SolveResult &result, IpmStopReason reason,
                               const std::string &detail, const IpmOptions &options,
                               const SolveBudget &budget);

/// @brief The message of an SQP solve; @p reason and @p detail as above, from
///        SqpSolverImpl::last_stop_reason() / last_stop_detail().
std::string sqp_result_message(const SqpResult &result, SqpStopReason reason,
                               const std::string &detail, const SqpOptions &options,
                               const SolveBudget &budget);

/// @brief One line describing an exception for a result message: its what() when
///        it derives from std::exception, a fixed phrase otherwise. Never throws
///        the exception it describes.
std::string describe_exception(const std::exception_ptr &error) noexcept;

} // namespace hven::detail
