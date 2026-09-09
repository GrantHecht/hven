// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// THE RESULT CORE both engines report through (M6 W5 T8.4).
//
// Three things live here and nothing else does:
//
//   SolveBudget   -- the per-call work ceiling, one aggregate for both engines.
//   SolveResult   -- the base every engine result derives from: the returned
//                    point, its prices, the objective, the four SHARED
//                    diagnostics, the constraint residuals, the iteration
//                    count, the wall clock and the warm-start snapshot.
//   compute_declared_diagnostics -- the ONE definition of those four
//                    diagnostics, over the DECLARED problem, in the CALLER's
//                    units, from an evaluation the engine already holds.
//
// WHY A BASE STRUCT AND NOT AN INTERFACE. SolveResult is non-polymorphic: no
// virtuals, no destructor of its own, never deleted through a base pointer.
// Slicing an IpmResult or an SqpResult down to it is a value-preserving copy of
// exactly the fields declared here, which is the point -- a consumer that wants
// "the answer" writes one function against SolveResult and both engines feed
// it. Everything an engine measures about ITSELF stays on that engine's own
// result under that engine's own names.
//
// WHY THE DIAGNOSTICS ARE COMPUTED, NOT COPIED. Each engine's convergence test
// gates on quantities in ITS OWN space: the interior-point engine's kkt_inf is
// a z-form dual residual carrying the objective scale, the SQP's stationarity
// is a reduced/projected measure at its own multipliers. Neither is the other,
// and neither is what a caller comparing two solves of the same declared
// problem needs. compute_declared_diagnostics is that comparable quantity, and
// it is computed ONCE, here, so that the two engines cannot drift apart on what
// "stationarity" means.
//
// WHY FROM A STASHED EVALUATION. This function evaluates NOTHING. Every model
// quantity arrives as an argument, taken from the evaluation the engine already
// performed at the point it is about to return. A fresh evaluation here would
// move evals_full on every solve and break the identity proofs T8 rests on, so
// the rule is absolute: where no finite evaluation of the returned point
// exists, every diagnostic is NaN -- UNMEASURED -- and ce/ci are empty. Absent
// is never zero-filled.

#include <functional>
#include <limits>
#include <optional>
#include <vector>

#include <hven/core/types.h>
#include <hven/drivers/solve_status.h>
#include <hven/warmstart/warm_start_data.h>

namespace hven::solvers {

class InteriorPointSolver;
class SqpDriver;

/// @brief The work ceiling for one solve call, on both engines.
///
/// Both members default to 0, which means "no budget of the caller's own": the
/// engine's own options decide, exactly as they did before this type existed.
struct SolveBudget {
    /// The MINOR-iteration budget. SQP: today's probe budget with its
    /// accounting unchanged -- it charges SSN and IPQP work, not only walk
    /// minors -- and a solve that exhausts it reports kMaxIter at the current
    /// point (kBudgetExhausted belongs to the separate budget-BEST exit under
    /// SqpOptions::budget_mode). INTERIOR-POINT: IGNORED, and documented so;
    /// that engine has no minor loop to budget.
    Index minor_budget = 0;

    /// The MAJOR-iteration ceiling. When non-zero it gives an EFFECTIVE cap of
    /// min(max_iterations, the engine option) -- so a caller can only tighten
    /// the engine's own limit, never loosen it. SQP: the effective cap governs
    /// the exit conjunction, the restoration refusal and the restoration
    /// sub-driver's budget alike, so restoration cannot spend past the caller's
    /// ceiling. INTERIOR-POINT: it caps EACH PHASE at min(max_iterations,
    /// IpmOptions::max_iters).
    Index max_iterations = 0;
};

/// @brief The four SHARED diagnostics of a returned point, over the declared
///        problem, in the caller's units.
///
/// Every member is NaN when the quantity was not measured.
struct DeclaredDiagnostics {
    /// inf-norm of grad f + Je^T lambda_e + Ji^T lambda_i - z over the declared
    /// coordinates that were measured.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm of the declared equality residuals ce.
    double feasibility_e = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm of the positive part of the declared inequality residuals and
    /// of the declared bound violations.
    double feasibility_i = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm over the declared inequality products lambda_i o ci and the
    /// CANONICAL bound products max(z,0) o (x - l) and max(-z,0) o (u - x).
    double complementarity = std::numeric_limits<double>::quiet_NaN();
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

    /// @brief The returned primal point, in the caller's declared space.
    Vec x;
    /// @brief Equality multipliers at @ref x, against L = f + lambda_e^T ce +
    ///        lambda_i^T ci - z. DECLARED rows only.
    Vec lambda_e;
    /// @brief Inequality multipliers at @ref x, same convention, declared rows.
    Vec lambda_i;
    /// @brief Bound multipliers at @ref x: z = z_lower - z_upper, so an entry is
    ///        >= 0 at an active lower bound and <= 0 at an active upper bound.
    ///        DECLARED width -- an eliminated coordinate has an entry here (0
    ///        under MakeParameter, -lambda_fix under MakeConstraint).
    Vec z;

    /// @brief Objective value at @ref x, on the caller's scale. NaN when
    ///        nothing was evaluated there.
    double f = std::numeric_limits<double>::quiet_NaN();

    // --- The four shared diagnostics ---
    //
    // Defined once, by compute_declared_diagnostics below, over the DECLARED
    // problem in the CALLER's units, from the evaluation the engine already
    // held at the returned point. NaN means UNMEASURED and nothing else: the
    // engines never zero-fill a diagnostic they did not take.
    //
    // These are NOT the quantity either engine's convergence test gated on.
    // That quantity stays on the engine's own result under its own name
    // (IpmResult::kkt_inf and friends; SqpResult::sqp_stationarity and
    // friends), because a tolerance comparison must be made against the number
    // the test actually read.
    /// @brief inf-norm dual residual over the measured declared coordinates.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm equality residual.
    double feasibility_e = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm inequality + bound violation (positive part).
    double feasibility_i = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm complementarity, canonical bound split.
    double complementarity = std::numeric_limits<double>::quiet_NaN();

    /// @brief Declared equality residuals at @ref x. EMPTY when unmeasured --
    ///        never a zero vector, which would read as a feasible point.
    Vec ce;
    /// @brief Declared inequality residuals at @ref x, empty when unmeasured.
    Vec ci;

    /// @brief Iterations taken. SQP: the top-level solve's major count, with
    ///        nested restoration majors NOT added (they are in `counters`).
    ///        INTERIOR-POINT: the sum over the phases that ran.
    Index iterations = 0;

    /// @brief Wall-clock seconds from the public solve entry -- after argument
    ///        validation -- to its return, seam and warm setup, final reporting
    ///        and the export snapshot INCLUDED.
    ///
    /// INFORMATIONAL, NEVER ASSERTED: counters, not timings, are this project's
    /// currency of correctness (CLAUDE.md §7). This is deliberately NOT either
    /// engine's older measurement -- IpmResult::total_time stops before final
    /// reporting, SqpResult::solve_impl_seconds starts after setup -- and both
    /// of those survive under their own names.
    double wall_seconds = 0.0;

    /// @brief The solve's exit state as warm-start currency, snapshotted at
    ///        exit while the model and workspace were still valid.
    ///
    /// A SNAPSHOT: later edits to this result's public fields do not reach it.
    ///
    /// @return The captured value, or nullopt when the solve produced no usable
    ///         point. Per engine: the interior-point engine returns nullopt
    ///         only on a numerical-error exit taken before the first iterate;
    ///         the SQP engine NEVER returns nullopt -- every one of its exits,
    ///         the non-finite start included, carries a finite point, zero
    ///         multipliers and a valid stamp.
    std::optional<WarmStartData> export_warm_start() const { return export_snapshot_; }

  protected:
    /// The snapshot @ref export_warm_start hands out; written by the engine at
    /// solve exit and by nothing else.
    std::optional<WarmStartData> export_snapshot_;

    // The two engines fill the snapshot on a result they are about to return.
    // Protected rather than public because a result is a REPORT: a consumer
    // reads the snapshot, and nothing outside the producing engine may install
    // one.
    friend class InteriorPointSolver;
    friend class SqpDriver;
};

/// @brief What a per-iteration callback asks the engine to do next.
enum class CallbackAction {
    /// Keep solving. The engine's trajectory is unchanged -- a callback that
    /// only ever returns this cannot move a counter, a status or a residual.
    kContinue = 0,
    /// Stop as soon as the engine can do so without spending work on an answer
    /// the caller no longer wants. Both engines then report kInterrupted at the
    /// point they are standing on, with the ordinary cleanup and the ordinary
    /// trace end event -- and, on the SQP, the ordinary ledger record (the
    /// interior-point engine has no ledger of its own until W5 T8.7).
    /// See each engine's set_iteration_callback() for the
    /// exact moment the stop takes effect and for the one case that outranks it
    /// (a CONVERGED iterate still reports kOptimal -- converged beats stop).
    kStop = 1,
};

/// @brief One iteration, as both engines report it to a caller's callback.
///
/// AT `depth` 0 -- which is every interior-point event and every top-level SQP
/// major -- EVERY field is in DECLARED space and CALLER units, exactly as
/// SolveResult is: the interior-point engine's reduced primal space is
/// expanded, its internal fixing rows are sliced out of the equality block, and
/// a scaled SQP solve's multipliers and prices carry no factor of the engine's.
///
/// AT `depth` 1 THE SPACE IS THE RESTORATION SUB-PROBLEM'S, not the caller's
/// (M6 W5 T8.6 fix1, the SQP lane's M4). A depth-1 event is the feasibility
/// sub-solve's OWN row: `x` is that problem's variable vector (whose width need
/// not be the caller's `n`), `f` is its feasibility objective, and the four
/// diagnostics are measured on it. Nothing is mapped back -- see @ref depth.
///
/// THE FOUR VECTOR VIEWS ARE BORROWED AND VALID FOR THE CALL ONLY. They alias
/// storage the engine owns and reuses; a callback that needs them afterwards
/// copies them. Copying the EVENT does not deepen the views.
///
/// UNMEASURED IS NaN, never zero -- the four diagnostics follow SolveResult's
/// own rule (this header's note), so an event taken where no finite evaluation
/// of the point exists reports NaN in all four rather than a residual of zero.
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
    ///        solve, 1 inside a restoration sub-solve, and one deeper per level
    ///        below that. nullopt on the interior-point engine.
    ///
    /// A NON-ZERO DEPTH CHANGES THE SPACE THE EVENT IS IN. The restoration
    /// sub-solve runs the FEASIBILITY problem -- a different model, in its own
    /// variables, with its own objective -- and the forwarder that carries its
    /// rows out stamps this field and maps nothing else. So a caller that
    /// compares an event against the caller's own model, or against
    /// SqpResult's fields, must filter on `depth == 0`; the design's rule is
    /// that depth-1 rows are the sub-solve's own.
    std::optional<Index> depth;

    /// @brief Objective value at the event's point, on the caller's scale.
    double f = std::numeric_limits<double>::quiet_NaN();
    /// @brief The four SHARED diagnostics of SolveResult, at the event's point.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    double feasibility_e = std::numeric_limits<double>::quiet_NaN();
    double feasibility_i = std::numeric_limits<double>::quiet_NaN();
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
    /// @brief Equality multipliers at @ref x, declared rows. BORROWED.
    Eigen::Ref<const Vec> lambda_e;
    /// @brief Inequality multipliers at @ref x, declared rows. BORROWED.
    Eigen::Ref<const Vec> lambda_i;
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

/// @brief Computes the four shared diagnostics over the DECLARED problem.
///
/// Evaluates nothing: every model quantity arrives as an argument, from the
/// evaluation the caller already holds at @p x.
///
/// One pass over the declared coordinates and one over each constraint block.
/// The bound products use the CANONICAL SPLIT -- max(z,0) * (x - l) and
/// max(-z,0) * (u - x) -- because a signed z cannot recover two separate prices
/// at a two-sided bound whose lower and upper multipliers are both positive.
/// The engine's own two-price complementarity, where it has one, stays on the
/// engine's own result.
///
/// Infinite bounds contribute nothing: a product of a zero price and an
/// infinite distance is not a measurement.
///
/// @param x                     The returned point, declared width n.
/// @param lambda_e              Declared equality multipliers, me.
/// @param lambda_i              Declared inequality multipliers, mi.
/// @param z                     Bound multipliers, n, signed z = zL - zU.
/// @param grad                  Objective gradient at @p x, n.
/// @param Je                    Declared equality Jacobian, me x n.
/// @param Ji                    Declared inequality Jacobian, mi x n.
/// @param ce                    Declared equality residuals at @p x, me.
/// @param ci                    Declared inequality residuals at @p x, mi;
///                              the feasible set is ci <= 0.
/// @param lower                 Declared lower bounds, n; -inf where absent.
/// @param upper                 Declared upper bounds, n; +inf where absent.
/// @param excluded_coordinates  Coordinates whose stationarity was NOT measured
///                              -- the interior-point engine's MakeParameter
///                              eliminations, whose reduced gradient has no row
///                              at all. May be empty. Indices outside [0, n)
///                              are ignored.
/// @return The four diagnostics.
/// @throws std::invalid_argument if the block sizes do not agree.
DeclaredDiagnostics compute_declared_diagnostics(const Vec &x, const Vec &lambda_e,
                                                 const Vec &lambda_i, const Vec &z, const Vec &grad,
                                                 const SpMatRM &Je, const SpMatRM &Ji,
                                                 const Vec &ce, const Vec &ci, const Vec &lower,
                                                 const Vec &upper,
                                                 const std::vector<Index> &excluded_coordinates);

/// @brief The same four diagnostics, from a Lagrangian gradient the caller
///        already holds.
///
/// THE ARITHMETIC IS THE SAME ARITHMETIC -- the overload above forms
/// grad f + Je^T lambda_e + Ji^T lambda_i and calls straight through to this
/// one. Two front doors exist because the two engines hold different things at
/// the point they report:
///
///   * The SQP's SqpKkt carries `grad_lag` outright, so forming it again from
///     the pieces would be arithmetic done twice with a chance of disagreeing.
///   * The INTERIOR-POINT engine has no separated gradient or Jacobian AT ALL
///     at the returned iterate: the model's derivatives are scattered directly
///     into one compound KKT buffer, fused there with the Hessian, the barrier
///     diagonals and any inertia perturbation, and left at the LAST EVALUATED
///     iterate rather than the reported one. What that engine does hold, in
///     its right-hand side at the returned iterate, is exactly this vector. A
///     signature demanding (grad, Je, Ji) would force it to evaluate the model
///     again -- which would move `evals_full` on every solve and break the very
///     identity proofs this task is held to.
///
/// @param x                     The returned point, declared width n.
/// @param lambda_i              Declared inequality multipliers, mi. (The
///                              equality multipliers are already inside
///                              @p grad_lag; the diagnostics never read them
///                              again.)
/// @param z                     Bound multipliers, n, signed z = zL - zU.
/// @param grad_lag              grad f + Je^T lambda_e + Ji^T lambda_i at @p x,
///                              declared width n, WITHOUT the bound term -- it
///                              is subtracted here, per coordinate, so that the
///                              exclusion set can be applied.
/// @param ce                    Declared equality residuals at @p x.
/// @param ci                    Declared inequality residuals at @p x; the
///                              feasible set is ci <= 0.
/// @param lower                 Declared lower bounds, n; -inf where absent.
/// @param upper                 Declared upper bounds, n; +inf where absent.
/// @param excluded_coordinates  Coordinates whose stationarity was NOT
///                              measured; see the overload above.
/// @return The four diagnostics.
/// @throws std::invalid_argument if the block sizes do not agree.
DeclaredDiagnostics
compute_declared_diagnostics_from_grad_lag(const Vec &x, const Vec &lambda_i, const Vec &z,
                                           const Vec &grad_lag, const Vec &ce, const Vec &ci,
                                           const Vec &lower, const Vec &upper,
                                           const std::vector<Index> &excluded_coordinates);

} // namespace hven::solvers
