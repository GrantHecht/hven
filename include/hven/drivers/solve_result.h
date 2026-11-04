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
#include <hven/core/types.h>
#include <hven/warmstart/warm_start_data.h>

namespace hven::solvers {

class IpmSolverImpl;
class SqpSolverImpl;

namespace detail {
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
    std::optional<Index> minor_budget;
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

    /// @brief One line saying why the solve stopped, for a log or a console.
    ///        Built once per solve from the status and the engine's own stop
    ///        facts; informational, never to be parsed.
    std::string message;

    /// @brief True when the returned point is the best iterate the solve
    ///        visited rather than the last one it reached.
    ///
    /// The rule per engine. Interior-point: set when IpmOptions::return_best is
    /// on and the solve ended in any non-optimal status except an interrupt
    /// (the substitution applies to the last phase that ran), so it can ride
    /// with kMaxIter, kStalled, kWorkLimit and the other non-optimal exits.
    /// A non-finite abort (kNumericalError, kNonFiniteIterate) returns the best
    /// iterate in full -- x, multipliers and every residual column -- with this
    /// set, when one was recorded; when none was (a non-finite start, a phase's
    /// first iterate, a failed initial factorization) it returns the failed
    /// point with its multipliers cleared and this false.
    /// SQP: set only when SqpOptions::budget_mode ran out of iterations
    /// (SolveStatus::kMaxIter); its non-finite exits never substitute and leave
    /// it false. The message gains "; returned the best iterate"
    /// whenever it is set.
    bool returned_best_iterate = false;

    /// @brief SQP engine only (the interior-point engine leaves it false). True
    ///        only on the certified infeasibility exit: the restoration phase ran
    ///        to its own KKT test, that test passed, and the returned point fails
    ///        the convergence test's inf-norm feasibility reading, so
    ///        (x, lambda, z) is a subgradient certificate that no direction
    ///        reduces the constraint violation from `x`.
    ///
    /// The certificate requires the restoration point's inf-norm violation to
    /// exceed `2 * feas_tol`; a point at or below that takes the no-claim exit
    /// (kNumericalError). At `SqpOptions::restoration_proximity > 0` the driver
    /// also recomputes the certificate residual explicitly at the restored point
    /// and requires it to be <= kkt_tol.
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
    // These are NOT the quantity either engine's convergence test gated on.
    // That quantity stays on the engine's own result (IpmResult::convergence,
    // SqpResult::convergence), because a tolerance comparison must be made
    // against the number the test actually read.
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
    /// engine's own measurement -- IpmResult::timing.total stops before final
    /// reporting, SqpResult::solve_impl_seconds starts after setup.
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
    /// detail::engine_rows (detail/drivers/result_access.h).
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
    friend class IpmSolverImpl;
    friend class SqpSolverImpl;
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
    /// (a converged iterate still reports kOptimal).
    kStop = 1,
};

/// @brief What an attached iteration callback asks its engine to PREPARE.
///
/// A bit mask, declared at attachment. The three vector views and the three
/// declared diagnostics cost O(n) of engine work per event -- an expansion out
/// of the engine's own space, a units map, and one pass over every declared
/// coordinate and constraint row -- prepared whether or not the consumer reads
/// them. The consumer declares what it reads, and an engine prepares nothing
/// else. On the interior-point engine this removes most of the attached cost; on
/// the SQP engine, whose events are per major and few, it is carried for one
/// surface across both engines.
///
/// set_iteration_callback() defaults to @ref kAll on both engines.
enum class IterationPayload : unsigned {
    /// The fields that cost only a copy, and that EVERY mask delivers:
    /// `iteration`, `phase`/`depth`, `f`, `step_norm`, `radius`/`mu` and
    /// `elapsed_seconds`. As a mask on its own it asks for nothing else.
    kScalars = 0,
    /// The three vector views -- `x`, `lambda`, `z`. Without this bit all
    /// three arrive EMPTY (size 0).
    kVectors = 1,
    /// The three declared diagnostics -- `stationarity`, `feasibility`,
    /// `complementarity`. Without this bit all three arrive
    /// NaN, which under this header's own rule reads as NOT MEASURED; under a
    /// mask that omits them it means NOT REQUESTED, which is the same absence.
    kDiagnostics = 2,
    /// Everything: the shape every consumer got before the mask existed.
    kAll = kVectors | kDiagnostics,
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

/// @brief Which iterations a callback is shown.
///
/// Only the SQP engine nests solves (its restoration phase runs a feasibility
/// sub-solve); the interior-point engine's events are all top-level, so the
/// scope is accepted there and changes nothing.
enum class CallbackScope {
    /// Only the caller's own solve: every event has `depth` 0 on the SQP. The
    /// restoration sub-solves run without any callback machinery.
    kTopLevel = 0,
    /// Also the restoration sub-solves' own rows, stamped with their `depth`
    /// (>= 1). Those rows are in the sub-problem's space; see
    /// IterationEvent::depth.
    kIncludeRestoration = 1,
};

/// @brief One iteration, as both engines report it to a caller's callback.
///
/// What arrives depends on the payload mask the callback was attached with
/// (@ref IterationPayload). Under the default @ref
/// IterationPayload::kAll every field below is prepared. Under a narrower mask
/// the three vector views arrive EMPTY and/or the three diagnostics arrive NaN,
/// and the engine does not do the work that would have filled them. The
/// scalars are delivered under every mask.
///
/// HOW MANY EVENTS. One event per iteration row. The SQP's row 0 is the start
/// point, so on a plain exit its default stream (@ref CallbackScope::kTopLevel)
/// holds `iterations + 1` events, `SolveResult::iterations` being the number
/// of steps taken. The interior-point engine's rows are its iterations: the
/// number of events equals its iteration count.
///
/// AT `depth` 0 -- which is every interior-point event and every top-level SQP
/// major -- EVERY field is in DECLARED space and CALLER units, exactly as
/// SolveResult is: the interior-point engine's reduced primal space is
/// expanded, its internal fixing rows are sliced out of the equality block, and
/// a scaled SQP solve's multipliers and prices carry no factor of the engine's.
/// In particular `lambda` is in the problem's ROW SPACE, the form of
/// SolveResult::lambda, and the final depth-0 event's `lambda` equals the
/// result's.
///
/// At `depth` 1 the space is the restoration sub-problem's, not the caller's.
/// A depth-1 event is the feasibility
/// sub-solve's OWN row: `x` is that problem's variable vector (whose width need
/// not be the caller's `n`), `f` is its feasibility objective, and the three
/// diagnostics are measured on it. Nothing is mapped back -- see @ref depth.
///
/// THE THREE VECTOR VIEWS ARE BORROWED AND VALID FOR THE CALL ONLY. They alias
/// storage the engine owns and reuses; a callback that needs them afterwards
/// copies them. Copying the EVENT does not deepen the views.
///
/// UNMEASURED IS NaN, never zero -- the three diagnostics (`stationarity`,
/// `feasibility`, `complementarity`) follow SolveResult's own rule (this
/// header's note), so an event taken where no finite evaluation of the point
/// exists reports NaN in all three rather than a residual of zero.
///
/// The three optionals say which engine produced the event rather than
/// carrying a sentinel: `phase` and `mu` are the interior-point engine's,
/// `depth` and `radius` the SQP's.
struct IterationEvent {
    /// @brief The iteration index this event describes. SQP: the major's index,
    ///        which is also this row's index in SqpResult::history and the
    ///        `major` field of its `sqp.major` trace line. INTERIOR-POINT: the
    ///        per-phase iterate index, which is `ipm.iter`'s own.
    Index iteration = 0;
    /// @brief INTERIOR-POINT ONLY: the index of the phase this iterate belongs
    ///        to, in IpmResult::phases. nullopt on the SQP.
    std::optional<Index> phase;
    /// @brief SQP ONLY: the restoration nesting level -- 0 in the caller's own
    ///        solve, 1 inside a restoration sub-solve. nullopt on the
    ///        interior-point engine. Depth >= 2 is unreachable: a restoration
    ///        sub-solve refuses to nest another.
    ///
    /// A NON-ZERO DEPTH CHANGES THE SPACE THE EVENT IS IN. The restoration
    /// sub-solve runs the FEASIBILITY problem -- a different model, in its own
    /// variables, with its own objective -- and the forwarder that carries its
    /// rows out stamps this field and maps nothing else, save that on an SQP
    /// solve whose column scaling fired its first n entries of `x` and `z` (the
    /// caller's variables) are in caller units, as at depth 0. So a caller that
    /// compares an event against the caller's own model, or against
    /// SqpResult's fields, must filter on `depth == 0`; the design's rule is
    /// that depth-1 rows are the sub-solve's own.
    std::optional<Index> depth;

    /// @brief Objective value at the event's point, on the caller's scale.
    double f = std::numeric_limits<double>::quiet_NaN();
    /// @brief The three SHARED diagnostics of SolveResult, at the event's
    ///        point: `feasibility` is the larger of the equality and inequality
    ///        residuals, NaN if either is.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief The step that reached (SQP: that was taken from) this point.
    ///        SQP: SqpIterate::step_norm of the row. INTERIOR-POINT: the
    ///        inf-norm of the primal block of the committed alpha * DXSL, 0 at
    ///        the first iterate of a phase (nothing was stepped yet).
    double step_norm = std::numeric_limits<double>::quiet_NaN();

    /// @brief SQP ONLY: the trust-region radius at this major.
    std::optional<double> radius;
    /// @brief INTERIOR-POINT ONLY: the barrier parameter this iterate was
    ///        evaluated under.
    std::optional<double> mu;

    /// @brief The point, declared width. BORROWED -- see this struct's note.
    Eigen::Ref<const Vec> x;
    /// @brief Row multipliers at @ref x, in the problem's row space -- the
    ///        definition of SolveResult::lambda. BORROWED. At `depth` >= 1 it
    ///        is the restoration sub-problem's own [lambda_e; lambda_i]
    ///        concatenation instead.
    Eigen::Ref<const Vec> lambda;
    /// @brief Bound multipliers at @ref x, z = z_lower - z_upper, declared
    ///        width. BORROWED.
    Eigen::Ref<const Vec> z;

    /// @brief Seconds since this solve's public entry.
    ///
    /// INFORMATIONAL, NEVER ASSERTED, on SolveResult::wall_seconds's own
    /// footing (CLAUDE.md section 7).
    double elapsed_seconds = 0.0;
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

} // namespace hven::solvers
