// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The one solve-outcome vocabulary both engines report. The interior-point
// engine's iteration loop reaches its cap, its stall exit and its
// locally-infeasible restoration return with the same "not converged" verdict,
// so resolve_ipm_phase_status() takes the stop reason the engine records
// alongside it.

#include <hven/core/solver_status.h>

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

/// @brief Resolves ONE interior-point phase's raw verdict against the stop
///        reason recorded for the same phase.
///
/// The interior-point engine's iteration loop reaches its cap, its stall exit
/// and its locally-infeasible restoration return with the same "not converged"
/// verdict, and only the stop reason tells the three apart. This is where that
/// split lives, and the rule is:
///
///   * A verdict the convergence check ALREADY HOLDS wins over the stop reason
///     -- a stall at an acceptable iterate reports kAcceptable.
///   * Otherwise kStageStalled and kRestorationLocallyInfeasible both report
///     kStalled, and the cap (or an unlabelled exit) reports kMaxIter.
///
/// A simultaneous stall and cap therefore reports kStalled: the engine records
/// the stall first and both cap doors defer to a reason already in place.
///
/// Applied PER PHASE, at the point the phase's own verdict is settled -- the
/// engine keeps no per-call verdict for a later phase to inherit (M6 W5 T8.4).
/// IDEMPOTENT: resolving an already-resolved status returns it unchanged.
///
/// @param raw_verdict The phase's own verdict as the iteration loop left it.
/// @param reason      The stop reason recorded for the same phase.
/// @return The resolved status.
/// @throws std::invalid_argument if @p reason is not one of the four.
SolveStatus resolve_ipm_phase_status(SolveStatus raw_verdict, IpmStopReason reason);

/// @brief Maps an SQP verdict onto SolveStatus; the identity on its five values.
/// @param status The engine's verdict.
/// @return The shared status.
/// @throws std::invalid_argument if @p status is not one of the five.
SolveStatus to_solve_status(SqpStatus status);

} // namespace hven::solvers
