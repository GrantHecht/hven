// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The solve-outcome vocabulary: QpStatus, the verdict on ONE QP subproblem, and
// SolveStatus, the verdict on a WHOLE solve on EITHER engine -- with the stop
// reason the interior-point engine records beside it, both display-string
// helpers and the reporting order.
//
// SolveStatus LIVES HERE, at the bottom tier, and not in drivers/: core/
// ledger.h reports a status, and a core header may depend on no tier above it
// (CLAUDE.md section 2's map, pinned by CoreLayering). This is where SqpStatus
// lived before M6 W5 T8.4 removed it, and for the same reason. What stays in
// drivers/solve_status.h is resolve_ipm_phase_status(), a statement about ONE
// engine's exits rather than about the vocabulary.

namespace hven::solvers {

/// @brief The verdict on ONE QP subproblem solve.
enum class QpStatus {
    kOptimal = 0,
    kMaxIter = 1,
    kInfeasible = 2,
    kNumericalError = 3,
};

/// @brief The verdict on a whole solve, reported by both engines.
///
/// Reachability differs per engine: the interior-point engine reports neither
/// kInfeasible nor kBudgetExhausted, and the SQP engine reports none of
/// kAcceptable, kStalled and kDiverging. BOTH report kInterrupted as of M6 W5
/// T8.6 -- it is what a per-iteration callback returning CallbackAction::kStop
/// gets, at the point the engine was standing on.
///
/// THE SQP ENGINE'S OWN NOTES, moved here from the removed SqpStatus (M6 W5
/// T8.4) because they describe values this enum now carries:
///
/// Verdict on a whole SQP solve. Deliberately mirrors QpStatus's shape, but
/// the correspondence is not literal: no subproblem status propagates into
/// kInfeasible at all, and kBudgetExhausted has no QP counterpart.
///
/// kInfeasible has TWO shapes, and only one field tells them apart:
/// - Certified: the restoration phase reached a stationary point of the
/// infeasibility measure h at which h is still above feas_tol (the
/// Byrd-Curtis-Nocedal rapid-detection conclusion). No direction reduces
/// the constraint violation from the returned point, so SqpResult::x is
/// a LOCAL certificate that the NLP is infeasible there, and lambda_e /
/// lambda_i are its subgradient certificate -- see SqpResult's own note
/// for the certificate and sqp_driver.h's RESTORATION PHASE for what
/// "stationary" is measured as.
/// - Not certified: restoration could not run to a verdict -- its own
/// subproblems stalled, or the one-restoration-per-solve cap was already
/// spent -- so the driver still reports kInfeasible because it holds a
/// restoration request it cannot service. Then the status means "could not
/// make progress", a statement about THIS solve and NOT about the model.
///
/// THE ONLY FIELD DISTINGUISHING THE TWO SHAPES IS
/// SqpResult::infeasibility_certified. counters.restoration_iters does NOT
/// distinguish them: it is > 0 on all three exits (certified, nested-stuck,
/// cap) because a violated entry always spends at least one major restoring.
/// Nor does the last history row: its StepVerdict depends on which of the
/// THREE REQUEST SOURCES raised the restoration (the funnel's signature and
/// the elastic tier's exhaustion leave kRestore; the radius floor leaves
/// kReject, told apart by tr_radius sitting at the floor), not on which exit
/// was reached -- sqp_driver.h's RESTORATION PHASE has the decision table.
/// tests/sqp/test_sqp_restoration.cpp shows how to re-derive the certificate from
/// the model if a caller wants to check the driver's claim.
///
/// kBudgetExhausted is a THIRD shape of "ran out of majors", distinct from
/// kMaxIter: what a max_iter exhaustion of the MAIN optimality loop reports
/// when SqpOptions::budget_mode is on, paired with a DIFFERENT
/// (x, lambda_e, lambda_i, z, f) than kMaxIter would report for the identical
/// run -- the best iterate by the funnel's own ordering rather than the last
/// one reached. See SqpOptions::budget_mode's own note for the full contract
/// and sqp_driver.h's BUDGETED MODE note for the mechanics. It is NEVER
/// reported when budget_mode is false (every max_iter exhaustion is kMaxIter)
/// and NEVER reported by the restoration phase's own budget exhaustion (that
/// stays kMaxIter regardless of budget_mode).
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
    /// The per-iteration callback returned CallbackAction::kStop (M6 W5 T8.6).
    /// A CALLER'S DECISION, not a verdict about the problem -- which is why it
    /// is a stop reason and why resolve_ipm_phase_status still lets a verdict
    /// the convergence check already holds win over it.
    kInterrupted = 4,
};

/// @brief WHICH DIAGNOSTIC MESSAGE the interior-point engine emitted (schema
///        `ipm.message`, M6 W5 T8.7b).
///
/// Until this task these were nine `fmt::print` calls inside the solver, guarded
/// by `CommonOptions::print_level`. They are events now: the console renders
/// them at the tier the guard used to apply, and a caller's own sink receives
/// the fact instead of losing it to stdout.
///
/// EIGHT WARNINGS AND ONE NOTICE. The first is not a warning at all --
/// `ensure_solver_initialized()` runs INSIDE the solve, after `ipm.solve.begin`,
/// and reports the process-global initialization on the first solve that pays
/// for it. It is a message kind so that "the transcript is the console's alone"
/// is true on EVERY solve, the process's first included.
///
/// THE PAYLOAD IS PER KIND and the unused slots are absences, not zeros; the
/// table lives on `IpmMessageTraceEvent` and in `docs/trace-schema-v0.md` §4.
enum class IpmMessageKind {
    /// The process-global solver initialization ran and took `a` MILLISECONDS.
    /// At most once per process. The console keeps the old `> 0.5 ms` rule.
    kSolverInitialized = 0,
    /// The KKT factorization's inertia read does not account for the whole
    /// matrix, so the system may be rank deficient. No payload.
    kRankDeficiency = 1,
    /// A factorization reported a HARD error -- `k` is the backend's own info
    /// code. `NumericalIssue` is normal while probing the inertia ladder and
    /// does NOT produce this message; it is recorded on the result instead.
    kFactorizationHardError = 2,
    /// The inertia-correction ladder exhausted its attempts. SIX integers:
    /// `k` attempts, the observed `p`/`n`/`z`, and the expected
    /// `expected_p`/`expected_n` (`z` is expected 0).
    kInertiaExhausted = 3,
    /// A feasibility restoration converged to a point that is still infeasible:
    /// `a` is the infeasibility reached, `b` the threshold it was judged
    /// against.
    kRestorationLocallyInfeasible = 4,
    /// The feasibility phase stalled with its restoration budget exhausted:
    /// `a` is the infeasibility now, `b` the infeasibility at the last
    /// restoration entry.
    kFeasibilityStall = 5,
    /// The per-iteration callback returned `kStop`; `iter` is the row it
    /// stopped on.
    kInterruptAtIteration = 6,
    /// A phase reached kDiverging or worse, so the remaining phases are
    /// skipped. No payload.
    kPhaseDiverged = 7,
    /// A stop was honoured between phases, so the remaining phases are skipped.
    /// No payload.
    kInterruptSkippingPhases = 8,
};

/// @brief The last non-Success factorization status the interior-point engine
///        observed during one solve (schema `ipm.phase.exit`'s `last_kkt_info`).
///
/// A NAMED VOCABULARY, NOT THE BACKEND'S RAW INTEGER (M6 W5 T8.7b): the value
/// the engine records is an `Eigen::ComputationInfo`, and writing that integer
/// onto the trace would make `ipm.phase.exit` the schema's only raw-integer
/// enum. The mapping is the identity on the four names; `kNoConvergence` has
/// never been observed from either backend and is here for completeness rather
/// than from a reading.
enum class IpmKktFactorStatus {
    kSuccess = 0,
    kNumericalIssue = 1,
    kNoConvergence = 2,
    kInvalidInput = 3,
};

/// @brief Maps a status to its lower-case display name.
/// @param status The status.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p status is not one of the nine.
const char *to_string(SolveStatus status);

/// @brief Maps a message kind to its lower-case snake name.
/// @param kind The kind.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p kind is not one of the nine.
const char *to_string(IpmMessageKind kind);

/// @brief Maps a factorization status to its lower-case snake name.
/// @param status The status.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p status is not one of the four.
const char *to_string(IpmKktFactorStatus status);

/// @brief Maps a stop reason to its lower-case display name.
/// @param reason The stop reason.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p reason is not one of the five.
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

// hven::solvers::SqpStatus -- the SQP engine's own five-value verdict enum --
// was REMOVED in M6 W5 T8.4. Its five enumerators ARE the first five of
// SolveStatus above, under the same names, so the substitution was textual.
// Its display strings were the CAPITALISED Optimal / MaxIter / Infeasible /
// NumericalError / BudgetExhausted; to_string(SolveStatus) spells all nine in
// lower snake, and the capitalised vocabulary survives only where committed
// artifacts are pinned to those bytes -- bench-local, as
// corpus::legacy_status_string.

} // namespace hven::solvers
