// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's phase-status resolution: the engine's own record
// of which door a phase left its loop by, how it resolves a phase's raw verdict,
// and how that record maps onto the public StopReason (core/solver_status.h).

#include <hven/core/solver_status.h>

namespace hven::detail {

/// @brief Which door the interior-point engine's current phase left its
///        iteration loop by. The ENGINE'S OWN record, finer than the public
///        StopReason (M8 AP-5 G1): the three limit doors are kept apart here
///        because resolve_ipm_phase_status needs them, and to_stop_reason folds
///        them into StopReason::kNone, since the status already names them.
///
/// kNone means the phase did not end NOTCONVERGED: the verdict itself says what
/// happened. Recorded per phase and reset at each phase start.
enum class IpmPhaseStop {
    kNone = 0,
    kIterationLimit,
    kRestorationLocallyInfeasible,
    kStageStalled,
    /// The per-iteration callback returned CallbackAction::kStop.
    kCallbackStop,
    /// The `SolveBudget::stop` token was found requested at an iteration
    /// boundary.
    kStopRequested,
    /// `CommonOptions::max_factorizations` was reached at an iteration boundary.
    kWorkLimit,
    /// `CommonOptions::time_limit_seconds` was reached at an iteration boundary.
    kTimeLimit,
    kNonFiniteStart,
    kNonFiniteIterate,
    kEvaluationFailure,
    kException,
};

/// @brief Resolves ONE interior-point phase's raw verdict against the stop
///        recorded for the same phase.
///
/// The interior-point engine's iteration loop reaches its cap, its stall exit
/// and its locally-infeasible restoration return with the same "not converged"
/// verdict, and only the recorded stop tells the three apart. This is where
/// that split lives, and the rule is:
///
///   * A verdict the convergence check ALREADY HOLDS wins over the stop
///     -- a stall at an acceptable iterate reports kAcceptable.
///   * Otherwise either stop (callback or token) reports kInterrupted,
///     kStageStalled and kRestorationLocallyInfeasible both report kStalled,
///     kWorkLimit and kTimeLimit report the status of the same name, and the
///     cap (or an unlabelled exit) reports kIterationLimit.
///
/// A simultaneous stall and cap therefore reports kStalled: the engine records
/// the stall first and both cap doors defer to a stop already in place.
///
/// Applied per phase, at the point the phase's own verdict is settled -- the
/// engine keeps no per-call verdict for a later phase to inherit. Idempotent:
/// resolving an already-resolved status returns it unchanged.
///
/// @param raw_verdict The phase's own verdict as the iteration loop left it.
/// @param stop        The stop recorded for the same phase.
/// @return The resolved status.
/// @throws std::invalid_argument if @p stop is not one of the twelve.
SolveStatus resolve_ipm_phase_status(SolveStatus raw_verdict, IpmPhaseStop stop);

/// @brief The public StopReason for a recorded phase stop. The three limit doors
///        (kIterationLimit, kWorkLimit, kTimeLimit) map to StopReason::kNone:
///        the status names them. Every other value maps to its namesake.
///        A converged status (kOptimal, kAcceptable) maps to kNone whatever the
///        recorded stop (a stall at an acceptable iterate resolves kAcceptable).
/// @param stop     The stop recorded for the phase.
/// @param resolved The phase's resolved status.
/// @throws std::invalid_argument if @p stop is not one of the twelve.
StopReason to_stop_reason(IpmPhaseStop stop, SolveStatus resolved);

} // namespace hven::detail
