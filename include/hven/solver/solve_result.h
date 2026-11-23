// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The result core both engines report through.
//
//   SolveBudget   -- the per-call work ceiling, one aggregate for both engines.
//   WorkCounters  -- what a solve spent, in model callbacks and factorizations.
//   SolveResult   -- the base every engine result derives from: the returned
//                    point, its prices, the objective, the shared diagnostics,
//                    the constraint residuals, the iteration count, the work
//                    counters, the stop message, the wall clock and the
//                    warm-start snapshot.
//
// SolveResult is non-polymorphic: no virtuals, no destructor of its own, never
// deleted through a base pointer. Slicing an IpmResult or an SqpResult down to
// it is a value-preserving copy of exactly the fields declared here, so a
// consumer that wants "the answer" writes one function against SolveResult and
// both engines feed it. Everything an engine measures about itself stays on that
// engine's own result under that engine's own names.
//
// The shared diagnostics (`stationarity`, `feasibility`, `complementarity`) are
// computed over the DECLARED problem in the CALLER's units, from the evaluation
// the engine already held at the returned point, so they are comparable across
// engines. Each engine's own convergence measure stays on its own result.
// Where no finite evaluation of the returned point exists every diagnostic is
// NaN -- unmeasured, never zero-filled.

#include <functional>
#include <limits>
#include <optional>
#include <string>

#include <hven/core/solver_status.h>
#include <hven/core/stop.h>
#include <hven/core/types.h>
#include <hven/solver/step_verdict.h>
#include <hven/warmstart/warm_start.h>

namespace hven {

namespace detail {
class IpmSolverImpl;
class SqpSolverImpl;
struct ResultAccess;
} // namespace detail

/// @brief The work ceiling for one solve call, on both engines.
///
/// "No limit" is an empty optional. An engaged member must be >= 1 (and
/// `max_iterations` at most INT_MAX); the solve refuses another value with
/// std::invalid_argument at entry. With both empty
/// (the default) the engine's own options decide.
struct SolveBudget {
    /// The major-iteration ceiling: an effective cap of
    /// min(max_iterations, `common.max_iterations`), so a budget can only tighten
    /// the options' cap. SQP: the effective cap governs the exit test, the
    /// restoration refusal and the restoration sub-solve's budget alike.
    /// INTERIOR-POINT: it caps each phase.
    std::optional<Index> max_iterations;

    /// The minor-iteration budget. SQP: the probe budget -- it charges walk
    /// minors and the SSN and IPQP tiers' work -- checked between majors; a
    /// solve that exhausts it without converging stops with
    /// `SolveStatus::kWorkLimit` at the current point and counts the stop in
    /// `SqpCounters::probe_budget_stops`. INTERIOR-POINT: ignored; that engine
    /// has no minor loop.
    std::optional<Index> max_minor_iterations;

    /// A cooperative stop request for THIS solve, from a StopSource the caller
    /// owns (core/stop.h). Default-constructed: no source, `stop_possible()`
    /// false, and the solve pays one null test per iteration. When the source's
    /// `request_stop()` is called -- from any thread -- the solve ends at its
    /// next iteration boundary with SolveStatus::kInterrupted and
    /// StopReason::kStopRequested, returning the current iterate (the point of
    /// the last iteration event), never the best one (SolveResult::best_iterate
    /// reports that; CallbackAction::kStopReturnBest is a callback's). A
    /// converged or acceptable verdict reached at the same boundary outranks
    /// it. INTERIOR-POINT: polled at the top of each
    /// iteration, before its factorization, and between inertia-correction
    /// attempts (the iteration is abandoned); a stop ends the phase sequence. SQP:
    /// polled at the exit test before each subproblem, right after each accepted
    /// step's row is recorded, so the step is not taken, and inside the
    /// subproblem's minor loop and its elastic re-solves (the subproblem is
    /// abandoned and the row for the current iterate recorded). A source
    /// already stopped stops the solve at its first boundary: a token is per
    /// solve. A stop landing inside feasibility restoration returns the point
    /// restoration was entered from (SolveResult::returned_pre_restoration_point);
    /// the per-engine mechanics are in docs/migration-0.2.md.
    StopToken stop;
};

/// @brief Refuses an engaged SolveBudget member below 1, and a `max_iterations`
///        above INT_MAX. Both engines run it at solve entry.
/// @throws std::invalid_argument naming the member and its value.
void validate(const SolveBudget &budget);

/// @brief What one solve spent, counted per solve.
///
/// The evaluation counts are model callbacks, counted at the boundary the
/// solver calls through, and they are PER SOLVE (reset at solve entry).
///   * An Nlp problem (Problem::from_nlp): every call to the user's Nlp
///     eval_f / eval_grad_f / eval_g / eval_jacobian / eval_hessian made on
///     behalf of this solve, by either engine, counts once. Structure, bounds
///     and dimension queries are not counted. A value an engine re-uses from
///     its own cache is not a call.
///   * A pieces or program problem on the interior-point engine: counted per
///     evaluation pass, by the quantities that pass forms (objective, gradient,
///     constraints, Jacobian, Hessian).
/// `factorizations` is the number of KKT factorizations the solve performed:
/// the interior-point engine's factorizations over the call, the SQP engine's
/// `SqpCounters::factorizations`.
///
/// `factorizations` equals `SqpCounters::factorizations` by design. `hessian_evals`
/// does NOT always equal `SqpCounters::eval_hess_calls`: this one counts calls that
/// reach the caller's model, while `eval_hess_calls` also counts the evaluations the
/// restoration wrapper and the pattern walk make on the engine's side. When the two
/// differ, `SqpCounters::eval_hess_calls` (core/solver_counters.h) is the engine's
/// own measured count.
struct WorkCounters {
    Index objective_evals = 0;
    Index gradient_evals = 0;
    Index constraint_evals = 0;
    Index jacobian_evals = 0;
    Index hessian_evals = 0;
    Index factorizations = 0;
};

/// @brief The trial evaluations a solve rejected because the model could not be
///        evaluated there, on both engines.
///
/// One count per rejected evaluation: the model threw EvaluationError
/// (core/errors.h), or returned a non-finite objective value, constraint value or
/// derivative entry. Each one was treated as a failed trial -- the step was
/// rejected or backed off, never accepted -- so a nonzero count says the
/// trajectory met points the model cannot evaluate, not that the answer is wrong.
struct EvaluationErrors {
    /// Number of rejected evaluations.
    Index count = 0;
    /// What the most recent one said: the EvaluationError's message, or a line
    /// naming the non-finite quantity. Empty when `count` is 0.
    std::string last_message;
};

/// @brief Interior-point facts of one iterate (IterationEvent::ipm). Delivered
///        under every payload mask. The step fields describe the step that
///        REACHED the iterate (the event fires before the iterate factorizes):
///        NaN/0 at a phase's first iterate, where nothing was stepped yet.
struct IpmIterationInfo {
    /// Index into IpmResult::phases.
    Index phase = 0;
    /// Barrier parameter the iterate was evaluated under, caller's scale.
    double mu = std::numeric_limits<double>::quiet_NaN();
    double alpha_primal = std::numeric_limits<double>::quiet_NaN();
    double alpha_dual = std::numeric_limits<double>::quiet_NaN();
    /// Primal inertia-correction delta of the step's factorization; 0 if none.
    double regularization_primal = 0.0;
    /// Dual (constraint-row) shift of that factorization; 0 if none.
    double regularization_dual = 0.0;
    /// Inertia-correction attempts of that factorization; -1 when its Newton
    /// direction came back non-finite (no ladder ran).
    Index inertia_corrections = 0;
    Index line_search_trials = 0;
};

/// @brief SQP facts of one major (IterationEvent::sqp). Delivered under every
///        payload mask. They describe the subproblem solved AT the major's point
///        (the event fires once it is solved), as `step_norm` does on this engine.
struct SqpIterationInfo {
    /// 0 in the caller's solve, 1 inside a restoration sub-solve.
    Index depth = 0;
    /// The l-infinity trust-region radius the subproblem was solved at.
    double trust_radius = std::numeric_limits<double>::quiet_NaN();
    /// h(x), the funnel's l1 constraint violation at the point.
    double violation_l1 = std::numeric_limits<double>::quiet_NaN();
    /// The subproblem's dual regularization (SqpIterationRow::regularization_dual).
    double regularization_dual = 0.0;
    /// False on a row whose subproblem was not solved; the fields below are
    /// then not meaningful (the two binding flags read false).
    bool qp_solved = false;
    QpStatus qp_status = QpStatus::kOptimal;
    Index qp_minor_iterations = 0;
    /// Active rows plus active lower and upper bound sides.
    Index active_set_size = 0;
    StepVerdict verdict = StepVerdict::kReject;
    /// An elastic solve supplied this major (SqpIterationRow::elastic).
    bool elastic = false;
    /// The trust RADIUS limited the step (SqpIterationRow::tr_binding).
    bool tr_binding = false;
    /// A `caller_step_limit` pin limited the step (SqpIterationRow::cap_binding).
    bool cap_binding = false;
};

/// @brief One iterate, owned: the copy of an IterationEvent a consumer may keep.
///
/// The event's field names with owning storage. Where the source carried no
/// vector it is empty, where it carried no diagnostic it is NaN. Never aliases
/// engine storage, so another thread may read it.
struct IterationSnapshot {
    Index iteration = 0;
    Index event_index = 0;
    bool recovering = false;
    double f = std::numeric_limits<double>::quiet_NaN();
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    double step_norm = std::numeric_limits<double>::quiet_NaN();
    double elapsed_seconds = std::numeric_limits<double>::quiet_NaN();
    Vec x;
    Vec lambda;
    Vec z;
    Vec g;
    Vec row_violation;
    Vec z_lower;
    Vec z_upper;
    /// Exactly one of the two is engaged.
    std::optional<IpmIterationInfo> ipm;
    std::optional<SqpIterationInfo> sqp;
};

/// @brief The result core both engines return through.
///
/// EVERY field is in DECLARED space and CALLER units: the vectors are sized to
/// the problem the caller transcribed (an interior-point elimination is
/// expanded back, an internal fixing row is not reported here), and no
/// objective scaling factor survives to this boundary.
///
/// NON-POLYMORPHIC by construction -- see this header's own note. Copy it,
/// slice it, store it; never delete a derived result through a pointer to this.
struct SolveResult {
    /// @brief How the solve ended.
    SolveStatus status = SolveStatus::kNumericalError;

    /// @brief Which of the engine's exits produced @ref status. kNone when the
    ///        status names the cause on its own (kOptimal, kAcceptable,
    ///        kIterationLimit, kWorkLimit, kTimeLimit, a certified kInfeasible).
    ///        The interior-point engine reports its last phase's reason
    ///        (IpmPhaseReport::stop_reason). After a solve an exception escaped,
    ///        the solver's `last_result()` carries kException.
    StopReason stop_reason = StopReason::kNone;

    /// @brief One line saying why the solve stopped, for a log or a console.
    ///        Built once per solve from the status and the engine's own stop
    ///        facts; informational, never to be parsed.
    std::string message;

    /// @brief True when the returned point is the best iterate the solve
    ///        visited rather than the last one it reached.
    ///
    /// Set when `common.return_best` applied to the exit (see ReturnBest for which statuses),
    /// on both engines: kOnLimit on kIterationLimit, kWorkLimit and kTimeLimit,
    /// kOnAnyNonOptimal on every other non-optimal status except kInfeasible (certified or not)
    /// and kInterrupted. A non-finite abort returns the best iterate in full -- x, multipliers
    /// and every residual column -- with this set, when one was recorded; when none was (a
    /// non-finite start, a phase's first iterate, a failed initial factorization) it returns
    /// the failed point with its multipliers cleared and this false. The interior-point engine
    /// substitutes in the last phase that ran. The message gains "; returned the best iterate"
    /// whenever it is set. Also set, on both engines, by a callback's
    /// CallbackAction::kStopReturnBest that found a recorded best iterate (with
    /// kInterrupted).
    bool returned_best_iterate = false;

    /// @brief True when the returned point is the one feasibility restoration was
    ///        entered from, because a stop (a callback kStop on a `recovering`
    ///        event, or a requested SolveBudget::stop) cut restoration short. Only
    ///        with SolveStatus::kInterrupted; the message gains
    ///        "; returned the point before feasibility restoration".
    bool returned_pre_restoration_point = false;

    /// @brief The best iterate the solve visited, when best tracking was armed,
    ///        recorded an iterate, and the solve did not end kOptimal or
    ///        kAcceptable; else nullopt.
    ///
    /// Armed whenever a return-best option is on, an iteration callback is
    /// installed or SolveBudget::stop can fire. Never affects the trajectory.
    /// Best by the shared ordering (see ReturnBest; interior-point: last phase that ran), over the
    /// caller's own iterates, never a feasibility-restoration point. Every vector is filled;
    /// `iteration`, `step_norm` and `ipm`/`sqp` are those of the iterate's own event.
    /// @ref returned_best_iterate says whether the primary point is this one.
    /// Invariant, both engines: returned_best_iterate implies best_iterate is set.
    /// It is set whenever the best was substituted for the primary point
    /// (whatever the status, kAcceptable included), and otherwise only on an exit
    /// that is neither kOptimal nor kAcceptable.
    std::optional<IterationSnapshot> best_iterate;

    /// @brief SQP engine only (the interior-point engine leaves it false). True
    ///        only on the certified infeasibility exit: the restoration phase ran
    ///        to its own KKT test, that test passed, and the returned point fails
    ///        the convergence test's inf-norm feasibility reading, so
    ///        (x, lambda, z) is a subgradient certificate that no direction
    ///        reduces the constraint violation from `x`.
    ///
    /// The certificate requires the restoration point's inf-norm violation to
    /// exceed `2 * feasibility_tol`; a point at or below that takes the no-claim exit
    /// (kNumericalError). At `SqpOptions::restoration_proximity > 0` the driver
    /// also recomputes the certificate residual explicitly at the restored point
    /// and requires it to be <= kkt_tol.
    ///
    /// Never set on an interrupted exit: a stop landing on the restoration phase's
    /// converged row leaves that phase's verdict alone (converged beats stop), so the
    /// exit stays kInfeasible and certified.
    ///
    /// False on every other exit, including other kInfeasible ones, and it is not
    /// derivable from the status or the counters: it is the only field that tells
    /// a certified kInfeasible from an uncertified one. A one-way guarantee: true
    /// means the certificate holds; false means no claim is made about the model.
    bool infeasibility_certified = false;

    /// @brief The returned primal point, in the caller's declared space.
    Vec x;
    /// @brief Row multipliers at @ref x, one per row of the problem's own row
    ///        space (model/problem.h): for an Nlp problem one per Nlp row, in
    ///        Ipopt's sign convention (L = f + lambda^T g - z^T x); for a pieces
    ///        problem [equality rows | inequality rows], with L = f +
    ///        lambda^T [c_E; c_I] - z^T x. Empty when unmeasured.
    Vec lambda;
    /// @brief Bound multipliers at @ref x: z = z_lower - z_upper, so an entry is
    ///        >= 0 at an active lower bound and <= 0 at an active upper bound.
    ///        DECLARED width -- an eliminated coordinate has an entry here (0
    ///        under MakeParameter, -lambda_fix under MakeConstraint).
    ///        Interior-point engine: an inactive bound's entry is small but not
    ///        zero (of the order of the final barrier parameter divided by the
    ///        distance to the bound). It is the value the engine computed and is
    ///        reported as such, never zeroed.
    Vec z;

    /// @brief Objective value at @ref x, on the caller's scale. NaN when
    ///        nothing was evaluated there.
    double f = std::numeric_limits<double>::quiet_NaN();

    // --- The shared diagnostics ---
    //
    // Over the DECLARED problem in the CALLER's units, from the evaluation the
    // engine already held at the returned point. NaN means UNMEASURED and
    // nothing else: the engines never zero-fill a diagnostic they did not take.
    //
    // These are the SHARED measures: the same three numbers, computed the same
    // way, whichever engine produced the result. They are NOT the quantity either
    // engine's convergence test gated on.
    // That quantity stays on the engine's own result (IpmResult::convergence,
    // SqpResult::convergence), which is engine-specific, because a tolerance comparison must be
    // made against the number the test actually read.
    /// @brief inf-norm dual residual over the measured declared coordinates.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(inf-norm equality residual, inf-norm inequality and bound
    ///        violation (positive part)); NaN if either part is unmeasured.
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm complementarity, canonical bound split.
    double complementarity = std::numeric_limits<double>::quiet_NaN();

    /// @brief Row values at @ref x in the same row space as @ref lambda: g(x) for an
    ///        Nlp problem, [c_E(x); c_I(x)] for a pieces problem, mapped from the
    ///        engine's own measurement (no callback is made to report it). A free
    ///        Nlp row, which no engine measures, reads NaN (unmeasured); it is
    ///        never evaluated. EMPTY when the engine's row blocks are unmeasured --
    ///        never a zero vector, which would read as a feasible point.
    Vec g;

    /// @brief Iterations taken. SQP: the top-level solve's major count, with
    ///        nested restoration majors NOT added (they are in `counters`).
    ///        INTERIOR-POINT: the sum over the phases that ran.
    Index iterations = 0;

    /// @brief What the solve spent; see WorkCounters.
    WorkCounters work;

    /// @brief The trial evaluations this solve rejected; see EvaluationErrors.
    EvaluationErrors evaluation_errors;

    /// @brief Wall-clock seconds from the public solve entry -- after argument
    ///        validation -- to its return, seam and warm setup, final reporting
    ///        and the warm-start snapshot INCLUDED.
    ///
    /// INFORMATIONAL, NEVER ASSERTED: counters, not timings, are this project's
    /// currency of correctness (CLAUDE.md §7). This is deliberately NOT either
    /// engine's own measurement -- IpmResult::timing.total_seconds stops before final
    /// reporting, SqpResult::timing.engine_seconds starts after setup.
    double wall_seconds = 0.0;

    /// @brief The solve's exit state as warm-start currency, snapshotted at
    ///        exit while the model and workspace were still valid.
    ///
    /// A SNAPSHOT: later edits to this result's public fields do not reach it.
    /// EMPTY (`warm_start.empty()`) when the solve produced no usable point,
    /// which a solve accepts as "no warm start". Per engine: the interior-point
    /// engine leaves it empty only on a numerical-error exit taken before the
    /// first iterate; the SQP engine NEVER does -- every one of its exits, the
    /// non-finite start included, carries a finite point, zero multipliers and a
    /// valid stamp.
    WarmStart warm_start;

  private:
    /// The engine's own row blocks at @ref x: equality and inequality multipliers
    /// and residuals in the engine's internal split, and the two parts of
    /// @ref feasibility. The solve entry maps them into
    /// @ref lambda and @ref g; library internals and tests read them through
    /// detail::engine_rows (detail/solver/result_access.h).
    struct EngineRows {
        Vec lambda_e;
        Vec lambda_i;
        Vec ce;
        Vec ci;
        /// The two parts of @ref feasibility: the equality residual and the
        /// inequality-plus-bound violation, NaN when unmeasured.
        double feasibility_e = std::numeric_limits<double>::quiet_NaN();
        double feasibility_i = std::numeric_limits<double>::quiet_NaN();
    };
    EngineRows engine_rows_;

    // The two engines fill the private row blocks of a result they are about to
    // return.
    friend class detail::IpmSolverImpl;
    friend class detail::SqpSolverImpl;
    friend struct detail::ResultAccess;
};

/// @brief What a per-iteration callback asks the engine to do next.
enum class CallbackAction {
    /// Keep solving. The engine's trajectory is unchanged -- a callback that
    /// only ever returns this cannot move a counter, a status or a residual.
    kContinue = 0,
    /// Stop as soon as the engine can do so without spending work on an answer
    /// the caller no longer wants. Both engines then report kInterrupted at the
    /// point they are standing on, with the ordinary cleanup, trace end event
    /// and ledger record. See each engine's set_iteration_callback() for the
    /// exact moment the stop takes effect and for the one case that outranks it
    /// (a converged iterate still reports kOptimal). The result's stop_reason
    /// is StopReason::kCallbackStop.
    kStop = 1,
    /// As kStop, and the returned primary point is SolveResult::best_iterate
    /// (SolveResult::returned_best_iterate is then true). When no best iterate
    /// was recorded the point is kStop's and returned_best_iterate is false.
    kStopReturnBest = 2,
};

/// @brief What an attached iteration callback asks its engine to PREPARE.
///
/// A bit mask, declared at attachment. Every bit costs O(n + m) of engine work
/// per event, prepared whether or not the consumer reads it, so the consumer
/// declares what it reads and an engine prepares nothing else.
///
/// set_iteration_callback() defaults to `kVectors | kDiagnostics` on both
/// engines; @ref kAll adds the constraint and bound-multiplier views.
enum class IterationPayload : unsigned {
    /// The fields that cost only a copy, and that EVERY mask delivers:
    /// `iteration`, `recovering`, `f`, `step_norm`, `elapsed_seconds` and the
    /// engine block (`ipm`/`sqp`). As a mask on its own it asks for nothing else.
    kScalars = 0,
    /// `x`, `lambda`, `z`. Without this bit all three arrive EMPTY (size 0).
    kVectors = 1,
    /// `stationarity`, `feasibility`, `complementarity`. Without this bit all
    /// three arrive NaN (not requested reads as not measured).
    kDiagnostics = 2,
    /// `g` and `row_violation`, in `lambda`'s row space. Without it both are EMPTY.
    kConstraints = 4,
    /// `z_lower`, `z_upper`, declared width. Without it both are EMPTY.
    kBoundMultipliers = 8,
    /// Every bit.
    kAll = kVectors | kDiagnostics | kConstraints | kBoundMultipliers,
};

/// @brief The union of two payload selections.
constexpr IterationPayload operator|(IterationPayload a, IterationPayload b) noexcept {
    return static_cast<IterationPayload>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

/// @brief True when @p mask asks for every bit of @p part.
///
/// `payload_requests(m, IterationPayload::kScalars)` is true for every mask:
/// the scalars are not a bit and are always delivered.
constexpr bool payload_requests(IterationPayload mask, IterationPayload part) noexcept {
    return (static_cast<unsigned>(mask) & static_cast<unsigned>(part)) ==
           static_cast<unsigned>(part);
}

/// @brief set_iteration_callback()'s default mask on both engines, and the mask
///        in force when no callback is installed: `kVectors | kDiagnostics`.
inline constexpr IterationPayload kDefaultIterationPayload =
    IterationPayload::kVectors | IterationPayload::kDiagnostics;

/// @brief Which iterations a callback is shown.
///
/// Only the SQP engine nests solves (its restoration phase runs a feasibility
/// sub-solve); the interior-point engine's events are all top-level, so the
/// scope is accepted there and changes nothing.
enum class CallbackScope {
    /// Only the caller's own solve: every SQP event has `sqp->depth` 0. The
    /// restoration sub-solves run without any callback machinery.
    kTopLevel = 0,
    /// Also the restoration sub-solves' own rows, stamped `sqp->depth` 1 and
    /// `recovering`. Those rows are in the sub-problem's space; see
    /// IterationEvent.
    kIncludeRestoration = 1,
};

/// @brief One iteration, as both engines report it to a caller's callback.
///
/// What arrives depends on the payload mask the callback was attached with
/// (@ref IterationPayload): a view or diagnostic outside the mask arrives EMPTY
/// or NaN, and the engine does not do the work that would have filled it. The
/// scalars and the engine block are delivered under every mask.
///
/// HOW MANY EVENTS. One event per iteration row. The SQP's row 0 is the start
/// point, so on a plain exit its default stream (@ref CallbackScope::kTopLevel)
/// holds `iterations + 1` events, `SolveResult::iterations` being the number
/// of steps taken. The interior-point engine's rows are its iterations: the
/// number of events equals its iteration count.
///
/// SPACE. Every interior-point event and every SQP depth-0 event is in DECLARED
/// space and CALLER units, exactly as SolveResult is; `lambda`, `g` and
/// `row_violation` are in the problem's row space, and the final depth-0 event's
/// `lambda` equals the result's. An SQP depth-1 event (`sqp->depth == 1`) is
/// the restoration sub-solve's OWN row, in that sub-problem's variables, rows
/// and objective (elastic variables included); nothing is mapped back, save
/// that on a column-scaled solve the first n entries of `x`, `z`, `z_lower` and
/// `z_upper` are in caller units.
///
/// ON A `recovering` INTERIOR-POINT EVENT `f` and the diagnostics are NaN and
/// `g`, `row_violation`, `z_lower`, `z_upper` are EMPTY: no caller-space evaluation
/// of a restoration iterate exists.
///
/// THE VIEWS ARE BORROWED AND VALID FOR THE CALL ONLY. They alias storage the
/// engine owns and reuses; snapshot() copies them. Copying the EVENT does not
/// deepen the views.
///
/// UNMEASURED IS NaN, never zero (this header's note).
struct IterationEvent {
    /// @brief SQP: the major's index, also its row in SqpResult::history.
    ///        INTERIOR-POINT: the per-phase iterate index (`ipm.iter`'s own).
    Index iteration = 0;
    /// @brief The 0-based count of events delivered before this one in the same
    ///        solve: 0 on the first event, then 1, 2, ... without a gap or a
    ///        repeat. Unlike `iteration` (a per-phase index on the interior-point
    ///        engine, a per-solve major index on the SQP that depth-1 rows
    ///        repeat), it is monotone across interior-point phases, across
    ///        restoration, and across SQP depth-1 rows, so it is the one key that
    ///        orders every event of a solve; `(ipm->phase, iteration)` is the key
    ///        for a row within its phase. On a solve that delivered every event
    ///        the last one's `event_index + 1` is the number of events.
    Index event_index = 0;
    /// @brief The point is inside feasibility restoration. INTERIOR-POINT:
    ///        restoration is in place and its rows are delivered under either
    ///        CallbackScope with this set. SQP: exactly the depth-1 rows (only
    ///        under kIncludeRestoration). On both, a stop (callback kStop or
    ///        token) that lands in restoration returns the point restoration was
    ///        entered from, not the event's point.
    bool recovering = false;
    /// @brief Objective value at the event's point, on the caller's scale.
    double f = std::numeric_limits<double>::quiet_NaN();
    /// @brief The three SHARED diagnostics of SolveResult at the point.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief Inf-norm of a primal step. INTERIOR-POINT: the step that REACHED
    ///        this point (0 at a phase's first iterate). SQP: the step taken FROM
    ///        it (IterationRecord::step_norm), so the event, the history row and the
    ///        trace stay one row. Each engine's `ipm`/`sqp` block has the same
    ///        timing.
    double step_norm = std::numeric_limits<double>::quiet_NaN();
    /// @brief Seconds since this solve's public entry. Informational, never
    ///        asserted (CLAUDE.md section 7).
    double elapsed_seconds = std::numeric_limits<double>::quiet_NaN();

    /// @brief kVectors. The point, declared width.
    Eigen::Ref<const Vec> x;
    /// @brief kVectors. Row multipliers, the definition of SolveResult::lambda.
    Eigen::Ref<const Vec> lambda;
    /// @brief kVectors. Bound multipliers z = z_lower - z_upper, declared width.
    Eigen::Ref<const Vec> z;
    /// @brief kConstraints. Row values at `x`, the definition of SolveResult::g
    ///        (a free Nlp row reads NaN).
    Eigen::Ref<const Vec> g;
    /// @brief kConstraints. Row violation at `x`: >= 0, 0 on a free row, and NaN
    ///        where the row value is NaN (a NaN residual propagates, never 0).
    Eigen::Ref<const Vec> row_violation;
    /// @brief kBoundMultipliers. Declared width, both >= 0, z_lower - z_upper ==
    ///        z. INTERIOR-POINT: the engine's per-side multipliers (an
    ///        eliminated or fixing-row coordinate takes the sign split of its z).
    ///        SQP: the sign split of z.
    Eigen::Ref<const Vec> z_lower;
    Eigen::Ref<const Vec> z_upper;

    /// @brief Exactly one is engaged: `ipm` on the interior-point engine,
    ///        `sqp` on the SQP engine.
    std::optional<IpmIterationInfo> ipm;
    std::optional<SqpIterationInfo> sqp;
};

/// @brief The per-iteration callback both engines take.
///
/// Installed with set_iteration_callback() and removed with
/// clear_iteration_callback(); both engines have both.
///
/// AN EXCEPTION THROWN FROM IT PROPAGATES OUT OF solve(). The solve is
/// abandoned exactly as any other throw out of the iteration loop abandons it:
/// no solve-end trace event, no ledger record for the abandoned solve, and the
/// solver USABLE for the next call.
using IterationCallback = std::function<CallbackAction(const IterationEvent &)>;

/// @brief A deep copy of @p event: O(size of what its payload mask delivered).
IterationSnapshot snapshot(const IterationEvent &event);

} // namespace hven
