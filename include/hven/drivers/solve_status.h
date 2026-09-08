// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The one solve-outcome vocabulary both engines report, with the mappings from
// the two engine-native enums onto it. The interior-point engine's NOTCONVERGED
// covers three different exits, so its mapping takes the stop reason the engine
// records alongside the flag (InteriorPointSolver::last_stop_reason()).

#include <hven/core/solver_status.h>
#include <hven/detail/drivers/interior_point_solver_fwd.h>

namespace hven::solvers {

/// @brief The verdict on a whole solve, reported by both engines.
///
/// Reachability differs per engine: the interior-point engine reports neither
/// kInfeasible nor kBudgetExhausted, the SQP engine reports none of kAcceptable,
/// kStalled and kDiverging, and neither reports kInterrupted today.
enum class SolveStatus {
    kOptimal = 0,
    kAcceptable = 1,
    kMaxIter = 2,
    kInfeasible = 3,
    kStalled = 4,
    kDiverging = 5,
    kNumericalError = 6,
    kBudgetExhausted = 7,
    kInterrupted = 8,
};

/// @brief Why the interior-point engine's last phase left its iteration loop.
///
/// kNone means the phase did not end NOTCONVERGED: the verdict itself says what
/// happened. Recorded per phase and reset at each phase start.
enum class IpmStopReason {
    kNone = 0,
    kIterationCap = 1,
    kRestorationLocallyInfeasible = 2,
    kStageStalled = 3,
};

/// @brief Maps a status to its lower-case display name.
/// @param status The status.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p status is not one of the nine.
const char *to_string(SolveStatus status);

/// @brief Maps a stop reason to its lower-case display name.
/// @param reason The stop reason.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p reason is not one of the four.
const char *to_string(IpmStopReason reason);

/// @brief The reporting order over all nine statuses, weakest verdict first.
///
/// kOptimal < kAcceptable < kInterrupted < kMaxIter < kBudgetExhausted <
/// kStalled < kInfeasible < kDiverging < kNumericalError. A reporting order
/// only; it decides nothing about phase termination.
///
/// @param status The status.
/// @return Its rank, 0 through 8; distinct for every status.
/// @throws std::invalid_argument if @p status is not one of the nine.
int severity(SolveStatus status);

/// @brief Maps an interior-point verdict and its stop reason onto SolveStatus.
///
/// A verdict the convergence check already holds wins over the stop reason, so a
/// stall at an acceptable iterate reports kAcceptable; NOTCONVERGED splits by
/// reason, and both abnormal exits report kStalled.
///
/// @param flag   The engine's convergence verdict.
/// @param reason The stop reason recorded for the same phase.
/// @return The shared status.
/// @throws std::invalid_argument if @p flag is not one of the five.
SolveStatus to_solve_status(hven::ConvergenceFlags flag, IpmStopReason reason);

/// @brief Maps an SQP verdict onto SolveStatus; the identity on its five values.
/// @param status The engine's verdict.
/// @return The shared status.
/// @throws std::invalid_argument if @p status is not one of the five.
SolveStatus to_solve_status(SqpStatus status);

} // namespace hven::solvers
