// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The solve-outcome vocabulary: QpStatus, the verdict on ONE QP subproblem, and
// SolveStatus, the verdict on a WHOLE solve on EITHER engine -- with the stop
// reason both engines record beside it, the display-string helpers and the
// reporting order.
//
// SolveStatus lives here, at the bottom tier, and not in drivers/: core/
// ledger.h reports a status, and a core header may depend on no tier above it
// (pinned by CoreLayering). resolve_ipm_phase_status(), a statement about one
// engine's exits rather than about the vocabulary, is in
// detail/drivers/ipm_phase_status.h.

#include <type_traits>

namespace hven {

/// @brief The verdict on ONE QP subproblem solve.
enum class QpStatus {
    kOptimal = 0,
    kIterationLimit = 1,
    kInfeasible = 2,
    kNumericalError = 3,
};

/// @brief The verdict on a whole solve, reported by both engines.
///
/// Reachability differs per engine: the interior-point engine never reports
/// kInfeasible, and the SQP engine reports none of kAcceptable, kStalled and
/// kDiverging. Both report kInterrupted -- what a per-iteration callback
/// returning CallbackAction::kStop gets, at the point the engine was standing on
/// -- and both report kWorkLimit and kTimeLimit.
///
/// kInfeasible has two shapes on the SQP engine, told apart only by
/// SolveResult::infeasibility_certified:
/// - Certified: the restoration phase reached a stationary point of the
///   infeasibility measure h at which h is still above feasibility_tol (the
///   Byrd-Curtis-Nocedal rapid-detection conclusion). No direction reduces the
///   constraint violation from the returned point, so `x` is a local
///   certificate that the problem is infeasible there.
/// - Not certified: the sub-solve converged with h above feasibility_tol but the
///   certificate gate refused it (restoration_proximity > 0), or the
///   inconsistent-equalities exit. No claim is made about the model.
/// An unserviceable restoration request (nested, or refused by the entry rule)
/// is kNumericalError, never kInfeasible.
///
/// A solve that exhausts its major-iteration cap reports kIterationLimit;
/// SolveResult::returned_best_iterate says whether the point returned is the
/// best iterate (SqpOptions::return_best_on_max_iterations) rather than the last one reached.
///
/// A non-finite evaluation the solve cannot recover from is kNumericalError. The
/// interior-point engine with IpmOptions::return_best on returns its best
/// iterate in full on that exit (returned_best_iterate set) when it recorded
/// one, and otherwise the failed point with its multipliers cleared
/// (returned_best_iterate false); the SQP engine returns the failed point with
/// its multipliers cleared.
enum class SolveStatus {
    kOptimal = 0,
    kAcceptable = 1,
    kIterationLimit = 2,
    kInfeasible = 3,
    kStalled = 4,
    kDiverging = 5,
    kNumericalError = 6,
    // 7 is retired; the remaining values keep their numbers.
    kInterrupted = 8,
    /// A deterministic work ceiling was reached: `CommonOptions::max_factorizations`
    /// on either engine, or the SQP engine's `SolveBudget::max_minor_iterations`. Read at
    /// iteration boundaries; the point returned is the iterate the solve stood on.
    kWorkLimit = 9,
    /// `CommonOptions::time_limit_seconds` was reached. Wall-clock, therefore NOT
    /// reproducible: the same solve can stop at a different iteration on another
    /// run. Read at iteration boundaries, like kWorkLimit.
    kTimeLimit = 10,
};

/// @brief A restoration SUB-SOLVE's own terminal status AS A COUNTER, with an
///        explicit "never ran" zero.
///
/// SolveStatus's ten values plus kNotRun, which is the DEFAULT and the whole
/// reason this enum exists. `SqpCounters::restoration_sub_status` is a field on
/// a record that most solves never write; a bare SolveStatus defaulting to the
/// enum's zero, `kOptimal`, would PRINT `optimal` on a never-restored solve's
/// trace -- a legal success value doing duty as an absence, so a reader who
/// takes this key without `restoration_entries` would read "the feasibility
/// sub-solve converged" off a solve that never ran one. Here the zero says so
/// itself.
///
/// THE OTHER ELEVEN ARE SolveStatus's, ONE FOR ONE, under the same names and the
/// same spellings (`to_string` below is `to_string(SolveStatus)`'s alphabet
/// plus `not_run`), and `to_restoration_sub_status` is the TOTAL map from the
/// one enum to the other. It is total -- every SolveStatus has an image here --
/// rather than carrying only the five a feasibility sub-solve can actually
/// return, because a partial map would need a default arm or an invented
/// image for the ones it omitted, and both are how a silent mistranslation
/// gets in. WHICH ARE REACHABLE is stated where it belongs, on the
/// counter's own note in `core/solver_counters.h`: kOptimal, kInfeasible,
/// kIterationLimit, kInterrupted and kNumericalError, plus kWorkLimit and kTimeLimit
/// when a limit is set. The remaining four are here for
/// the map's totality and have never been observed on this field.
enum class RestorationSubStatus : std::underlying_type_t<SolveStatus> {
    /// NO restoration sub-solve was measured on this solve. The default, and
    /// the only value that is not a reading. Equivalent to
    /// `restoration_entries == 0`, which stays the record-wide absence reading
    /// for the four `double` absolutes beside it -- they have no spare value to
    /// spend on a sentinel and this enum does.
    kNotRun = 0,
    kOptimal = 1,
    kAcceptable = 2,
    kIterationLimit = 3,
    kInfeasible = 4,
    kStalled = 5,
    kDiverging = 6,
    kNumericalError = 7,
    // 8 is retired; the remaining values keep their numbers.
    kInterrupted = 9,
    kWorkLimit = 10,
    kTimeLimit = 11,
};

/// @brief Why a solve ended, beside its SolveStatus -- one vocabulary for both
///        engines, read from `SolveResult::stop_reason` (and per phase from
///        `IpmPhaseReport::stop_reason`).
///
/// kNone means the status names the cause on its own: kOptimal, kAcceptable,
/// kIterationLimit, kWorkLimit, kTimeLimit and a certified kInfeasible. Every
/// other value names which of the engine's exits produced the status beside it.
/// As with SolveStatus, each engine reaches a subset; the engines column below
/// says which.
enum class StopReason {
    /// The status names the cause. Both engines.
    kNone = 0,
    /// The per-iteration callback returned CallbackAction::kStop (on the SQP, at
    /// the top level or inside a restoration sub-solve). Verdict kInterrupted. A
    /// CALLER'S DECISION, not a verdict about the problem: a converged or
    /// acceptable verdict reached at the same point outranks it. Both engines.
    kCallbackStop = 1,
    /// The `SolveBudget::stop` token's source requested the stop, and the solve
    /// honoured it at an iteration boundary. Verdict kInterrupted; a converged or
    /// acceptable verdict reached at the same point outranks it. Both engines.
    kStopRequested = 2,
    /// An exception escaped the solve (a user callback's own, or one raised
    /// inside hven). Verdict kInterrupted; the partial result is the solver's
    /// `last_result()`. Both engines.
    kException = 3,
    /// The model returned a non-finite value (objective, gradient, constraint or
    /// derivative entry) at the solve's start point. Verdict kNumericalError,
    /// reached before anything is factorized. The returned point is the start
    /// point, its multipliers cleared. Both engines.
    kNonFiniteStart = 4,
    /// The model returned a non-finite value, or threw EvaluationError, at an
    /// iterate a step had reached, and the step could not be rolled back.
    /// Verdict kNumericalError. The returned point is that iterate -- the one
    /// whose evaluation failed -- with its multipliers cleared; the residual
    /// fields report what its evaluation measured, NaN where it measured
    /// nothing. Both engines.
    kNonFiniteIterate = 5,
    /// Every trial evaluation of an iteration failed (EvaluationError or a
    /// non-finite value), the iterate was not acceptable and feasibility
    /// restoration could not take over. Verdict kNumericalError. Interior-point.
    kEvaluationFailure = 6,
    /// A stage of the interior-point iteration stopped making progress. Verdict
    /// kStalled. Interior-point.
    kStageStalled = 7,
    /// Feasibility restoration converged to a point that is still infeasible.
    /// Verdict kStalled. Interior-point.
    kRestorationLocallyInfeasible = 8,
    /// The QP subproblem failed and its retry budget was spent. Verdict
    /// kNumericalError, or kInfeasible (uncertified) for an infeasible
    /// subproblem. SQP.
    kSubproblemFailure = 9,
    /// The equality constraints were judged inconsistent (the equality-basis
    /// exit). Verdict kInfeasible, uncertified. SQP.
    kInconsistentEqualities = 10,
    /// Feasibility restoration was requested and could not be serviced: the
    /// request came from inside a restoration sub-solve, or the per-solve entry
    /// cap was spent. Verdict kNumericalError. SQP.
    kRestorationRefused = 11,
    /// Restoration ended at a point where the model is not finite. Verdict
    /// kNumericalError. SQP.
    kRestorationNonFinite = 12,
    /// The restoration sub-solve converged without a claim: the point is
    /// neither feasible enough to resume from nor infeasible enough to certify.
    /// Verdict kNumericalError. SQP.
    kRestorationNoClaim = 13,
    /// The restoration sub-solve itself ended kNumericalError. Verdict
    /// kNumericalError. SQP.
    kRestorationFailed = 14,
};

/// @brief WHICH DIAGNOSTIC MESSAGE the interior-point engine emitted (schema
///        `ipm.message`).
///
/// These are events, not prints: the console renders them at the tier
/// `CommonOptions::print_level` selects, and a caller's own sink receives the
/// fact instead of losing it to stdout.
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
    /// A stop was honoured at an iteration: the per-iteration callback
    /// returned `kStop`, or (M8 AP-5 G1) the `SolveBudget::stop` token was
    /// requested -- `SolveResult::stop_reason` tells them apart. `iter` is the
    /// row it stopped on.
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
/// A NAMED VOCABULARY, NOT THE BACKEND'S RAW INTEGER: the value
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
/// @throws std::invalid_argument if @p status is not one of the ten.
const char *to_string(SolveStatus status);

/// @brief Maps a restoration sub-status to its lower-case snake name.
/// @param status The sub-status.
/// @return A static string, never null. `not_run` for @c kNotRun; otherwise
///         EXACTLY what to_string(SolveStatus) spells for the same name.
/// @throws std::invalid_argument if @p status is not one of the eleven.
const char *to_string(RestorationSubStatus status);

/// @brief The TOTAL map from a sub-solve's status to the counter's vocabulary.
///
/// Every SolveStatus has an image; kNotRun is the one value this map never
/// produces, because it means "no sub-solve ran" and the caller of this
/// function is holding one that did.
///
/// @param status The status the restoration sub-solve returned.
/// @return Its counterpart, name for name.
/// @throws std::invalid_argument if @p status is not one of the ten.
RestorationSubStatus to_restoration_sub_status(SolveStatus status);

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

/// @brief Maps a stop reason to its lower-case snake name (the trace's
///        `stop_reason` spelling).
/// @param reason The stop reason.
/// @return A static string, never null.
/// @throws std::invalid_argument if @p reason is not one of the fifteen.
const char *to_string(StopReason reason);

/// @brief The reporting order over all ten statuses, weakest verdict first.
///
/// kOptimal < kAcceptable < kInterrupted < kIterationLimit < kWorkLimit < kTimeLimit <
/// kStalled < kInfeasible < kDiverging < kNumericalError. A
/// reporting order only.
///
/// @param status The status.
/// @return Its rank, 0 through 9; distinct for every status.
/// @throws std::invalid_argument if @p status is not one of the ten.
int severity(SolveStatus status);

} // namespace hven
