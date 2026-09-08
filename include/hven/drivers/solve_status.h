// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's phase-status resolution.
//
// The VOCABULARY itself -- SolveStatus, IpmStopReason, their display strings
// and the severity order -- moved down to core/solver_status.h in M6 W5 T8.4,
// because core/ledger.h reports a status and a core header may depend on no
// tier above it. This header is what is left: one function, and it is a
// statement about ONE engine's exits rather than about the vocabulary.
//
// It stays a public header of its own rather than folding into the engine's,
// so a caller resolving a verdict it obtained elsewhere needs neither engine.

#include <hven/core/solver_status.h>

namespace hven::solvers {

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

// to_solve_status(SolveStatus) was REMOVED in M6 W5 T8.4 with SolveStatus itself.
// The mapping it made was the identity on the five values, so there is nothing
// left to map: the SQP engine reports SolveStatus directly.

} // namespace hven::solvers
