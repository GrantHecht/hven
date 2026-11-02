// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The sequential-quadratic-programming solver.
///
/// `SqpSolver` is a thin handle over a private implementation: the engine's
/// state and machinery live behind a `std::unique_ptr` and are not part of this
/// header. A solver is movable but not copyable. Moving transfers the whole
/// engine, including its QP engines, attached callback, trace sink and ledger.
/// A moved-from solver may only be destroyed or assigned to.

#include <memory>
#include <string>

#include <hven/core/start_level.h>
#include <hven/core/types.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/solve_status.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/problem.h>
#include <hven/warmstart/sqp_warm_start.h>
#include <hven/warmstart/warm_start_data.h>

namespace hven::solvers {

class SqpSolverImpl;
class TraceSink;
class Ledger;

namespace detail {
struct SqpSolverAccess;
} // namespace detail

/// Sequential-quadratic-programming solver for smooth NLPs.
///
/// A trust-region major loop over an NLP: each major solves a QP subproblem
/// with the engine named by `SqpOptions::qp_mode`, with an elastic
/// reformulation for infeasible subproblems, one second-order correction per
/// rejected trial, and a restoration phase when the loop cannot make progress.
///
/// Thread safety: one solver is not safe for concurrent use. Distinct solvers
/// may run concurrently. See docs/concurrency.md.
///
/// Any other member call on a moved-from solver throws std::logic_error.
class SqpSolver {
  public:
    /// Constructs a solver over `opts`.
    /// @throws std::invalid_argument if `opts` fails validation.
    explicit SqpSolver(const SqpOptions &opts);
    ~SqpSolver();

    SqpSolver(const SqpSolver &) = delete;
    SqpSolver &operator=(const SqpSolver &) = delete;

    /// Transfers the engine. The source may afterwards only be destroyed or
    /// assigned to.
    SqpSolver(SqpSolver &&) noexcept;
    /// Transfers the engine, releasing the one this solver held. The source may
    /// afterwards only be destroyed or assigned to.
    SqpSolver &operator=(SqpSolver &&) noexcept;

    /// The options this solver runs under. Read-only: copy, edit the copy, and
    /// hand it back through `set_options()`.
    const SqpOptions &options() const;

    /// Replaces the whole options value and rebuilds the QP engines.
    /// Transactional: validation and the engine construction both happen
    /// before anything is swapped in, so a throw leaves the previous options
    /// and engines in force. There is no fast path: a replacement with
    /// identical options rebuilds too. Legal between solves only.
    /// @throws std::invalid_argument if `o` fails validation.
    /// @throws std::logic_error if a solve is in flight on this solver.
    void set_options(SqpOptions o);

    /// The thread count in force on the SSN tier's live factor, or -1 when this
    /// solver has not built that tier yet. The tier engines are built lazily at
    /// first use, so a solver that has not run in that tier reads -1.
    int ssn_tier_num_threads() const;

    /// The thread count in force on the IPQP tier's live KKT factorization, or
    /// -1 when this solver has not built that tier yet.
    int ipqp_tier_num_threads() const;

    /// Solves `problem` from `x0`.
    ///
    /// The problem is borrowed for the call. Its per-solve re-read runs first
    /// (model/problem.h). `lambda` and `g` on the result are in the problem's row
    /// space.
    /// @param problem The problem; borrowed.
    /// @param x0      The start point.
    /// @throws std::invalid_argument if the problem refuses the solve: a pieces
    ///         problem, which this engine does not solve; a dimension, structure
    ///         or row-kind change; an Nlp with has_hessian() == false under
    ///         SqpHessianMode::kExact. Also on a declaration this engine cannot
    ///         lay: a crossed or NaN box, a Jacobian or Hessian whose dimensions
    ///         contradict the row and variable counts, and, per evaluation, a
    ///         stored element count or coordinate set that moves with `x`.
    SqpResult solve(Problem &problem, const Vec &x0);

    /// Solves from `x0` under a work ceiling.
    /// @param budget `minor_budget` is the probe budget: 0 is no budget, and a
    ///               positive value stops the solve at the top of the first
    ///               major that finds `qp_minor_iters >= minor_budget` without
    ///               having converged, reporting `kMaxIter` and counting the
    ///               stop in `counters.probe_budget_stops`. Convergence always
    ///               wins, and under `kSsn` the budget also charges the SSN and
    ///               refinement factorizations. A non-zero `max_iterations`
    ///               gives an effective major cap of
    ///               `min(max_iterations, SqpOptions::max_iter)` that governs
    ///               the exit conjunction, the restoration refusal and the
    ///               restoration sub-driver alike: a caller may tighten the
    ///               engine's limit, never loosen it.
    SqpResult solve(Problem &problem, const Vec &x0, SolveBudget budget);

    /// Warm-start ingest of this engine's native object.
    ///
    /// The native route is the SQP-only entry and the only route to `kWarm` and
    /// `kHot`. It never throws on a stale or foreign value: an unusable hash
    /// resolves `kSeeded` and still contributes its values, and a failed
    /// ingest gate degrades to `kCold`.
    /// @param x0 The start point; ignored whenever `warm` resolves above
    ///           `kCold`, where the solve starts from `warm.x`.
    /// @throws std::invalid_argument as the cold overload, checked before `warm`
    ///         is looked at.
    SqpResult solve(Problem &problem, const Vec &x0, const SqpWarmStart &warm,
                    SolveBudget budget = {});

    /// Warm-start ingest of the shared payload.
    ///
    /// The payload is the declared-space currency that both engines accept,
    /// that `SolveResult::export_warm_start()` hands back and that travels
    /// between engines and across a serialization boundary. Identity mismatch
    /// refuses; pattern and value defects degrade. Identity is the block
    /// lengths and the declaration stamp, checked at solve entry in every
    /// `SqpOptions::qp_mode`. A payload never saw a model, so the level it
    /// resolves to is capped at `kSeeded`. An inequality multiplier a shade
    /// negative is clamped and counted; a badly negative one degrades the
    /// object to `kCold`. A non-finite entry in a payload's core blocks is
    /// refused, not degraded.
    ///
    /// A payload whose `primal_` is empty is a multipliers-only seed: `x0` is
    /// the start, the object resolves `kSeeded`, and `primal_`/`bound_lmults_`
    /// must be both empty or both at the declared primal width. A polish
    /// extension on a seed is ignored and counted in
    /// `SqpCounters::polish_ignored`. `z` is never ingested from any seed on
    /// this engine.
    ///
    /// A braced third argument (`solve(problem, x0, {})`) is ambiguous between
    /// `SolveBudget`, `WarmStartData` and `SqpWarmStart`; spell the type.
    ///
    /// @param x0 The start point, used whenever `warm` does not supply one.
    /// @throws std::invalid_argument as the cold overload (checked first); if
    ///         any core block of `warm` holds a non-finite value; if `primal_`
    ///         and `bound_lmults_` are neither both empty nor one length; if the
    ///         `"hven.ipm.polish.v1"` extension is duplicated, unreadable,
    ///         mis-sized or holds a non-finite or negative bound dual; if any
    ///         block is not at the problem's declared dimensions; or if the
    ///         declaration stamp is not this problem's.
    SqpResult solve(Problem &problem, const Vec &x0, const WarmStartData &warm,
                    SolveBudget budget = {});

    /// The warm-start value of the last completed solve, in declared space and
    /// by copy. `primal_` is `SqpResult::x`, `eq_lmults_` its `lambda_e`,
    /// `iq_lmults_` its `lambda_i` and `bound_lmults_` its `z`. The stamp is the
    /// assembly's declaration key as of that solve's completion. This engine
    /// produces no extensions.
    /// @throws std::logic_error if no solve has completed on this solver.
    WarmStartData export_warm_start() const;

    /// Installs the per-iteration callback that both engines accept, with the
    /// full payload (`IterationPayload::kAll`). An empty function clears it.
    ///
    /// The callback fires once per history row: after the `sqp.major` trace
    /// line and before the row is pushed, so the trace stream,
    /// `SqpResult::history` and the callback are the same rows in the same
    /// order and caller units, and the event count equals `history.size()`.
    /// The event describes the row being pushed. `IterationEvent::depth` is the
    /// restoration nesting: 0 in this solve, 1 inside a restoration sub-solve,
    /// whose events reach the same callback.
    ///
    /// Returning `CallbackAction::kStop` latches a stop that is honoured before
    /// the next subproblem is built. The solve then takes its ordinary
    /// terminal exit at the current iterate and reports `kInterrupted`.
    /// Precedence at that exit is converged, then interrupted, then
    /// probe-exhausted, then the iteration cap: a stop on a converged row still
    /// reports `kOptimal`. A stop returned on row `k` is honoured at row
    /// `k + 1`'s exit, so the solve returns after that row. A stop returned
    /// inside a restoration sub-solve latches on this solver too. A stop on
    /// the public call's own terminal row is a no-op.
    ///
    /// The callback may throw. The exception propagates out of `solve()`, no
    /// `sqp.solve.end` event and no ledger record are emitted for the abandoned
    /// solve, and the solver stays usable.
    ///
    /// Setting or clearing from inside the callback is safe: the change is
    /// deferred until the invocation returns, or failing that to the next
    /// solve entry. One slot serves each setter and the last write wins.
    void set_iteration_callback(IterationCallback cb);

    /// As above, and declares which parts of `IterationEvent` the consumer
    /// reads; the solver prepares nothing else.
    ///
    /// Under `IterationPayload::kScalars` the event carries `iteration`,
    /// `depth`, `f`, `step_norm`, `radius` and `elapsed_seconds`; `x`,
    /// `lambda_e`, `lambda_i` and `z` are empty views and the four declared
    /// diagnostics are NaN, meaning "not requested".
    ///
    /// The mask is stored with the callable, parked through the same in-flight
    /// guard, reset to `kAll` by `clear_iteration_callback()` and by passing an
    /// empty callable, and forwarded into a restoration sub-solve with the
    /// callback itself, so a depth-1 event carries the same payload as a
    /// depth-0 one. A mask changed from inside a callback takes effect at the
    /// next event, at any depth.
    void set_iteration_callback(IterationCallback cb, IterationPayload payload);

    /// The payload mask in force, or `IterationPayload::kAll` when no callback
    /// is installed. A value parked from inside a callback is not visible until
    /// the safe point applies it.
    IterationPayload iteration_payload() const;

    /// Removes the callback and resets the payload mask to `kAll`. Deferred
    /// like `set_iteration_callback()` when called from inside a callback.
    void clear_iteration_callback();

    /// Attaches a trace sink; `nullptr` detaches. The sink is borrowed and must
    /// outlive every solve made while it is attached.
    ///
    /// When `common.print_level` enables printing, the solver composes its own
    /// console sink behind the caller's, so attaching a console never displaces
    /// a caller's sink and the caller's stream is the same with printing on and
    /// off. The restoration phase's internal sub-solver never prints. The sink
    /// is also forwarded to the internal IPQP engine.
    /// @throws std::logic_error if a solve is in flight. The composition is
    ///         fixed for the solve, so a call from inside a callback or a sink
    ///         could not take effect in it.
    void attach_trace(TraceSink *sink);

    /// Attaches a ledger; `nullptr` detaches. One `SqpSolveRecord` is written
    /// per public `solve()` call, labelled `"{label_prefix}_{n}"`, and the same
    /// ledger is forwarded to the solver's own QP engine under
    /// `"{label_prefix}_qp"`. The restoration phase's nested solver is never
    /// given it, so a solve that restores still writes one record. The ledger
    /// is borrowed and must outlive every solve made while it is attached.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

  private:
    friend struct detail::SqpSolverAccess;

    SqpSolverImpl &impl();
    const SqpSolverImpl &impl() const;

    std::unique_ptr<SqpSolverImpl> impl_;
};

// Defined in src/drivers/sqp_print.cpp alongside format_iteration_table.
/// @brief Human-readable name of a step verdict, for the iteration printer.
/// @param v The verdict.
/// @return A static string; nothing in the driver branches on it.
const char *to_string(StepVerdict v);

/// @brief Renders `sol.history` as an fmt-based iteration table, one row per
///        SqpIterate.
/// @param sol A finished solve.
/// @return The table, ready to print.
std::string format_iteration_table(const SqpResult &sol);

// The five pieces `format_iteration_table` is assembled from. Concatenating
// them over a finished solve reproduces `format_iteration_table` exactly, so
// a caller that renders incrementally gets the same text.

/// @brief The table's header line plus its rule, both newline-terminated.
std::string sqp_iteration_table_head();

/// @brief One table row, newline-terminated. A row with `qp_solved == false`
///        renders "-" in the verdict and the two QP columns -- the solve's
///        stopped-AT row, or a non-finite step rollback row (`rolled_back`: the
///        WD column then reads "r"; "*" is a watchdog restore). Rollback rows
///        may sit mid-history and a solve may carry several.
/// @param row The history row.
std::string sqp_iteration_table_row(const SqpIterate &row);

/// @brief The trailer's `Status` line, with its leading blank line.
/// @param status The solve's verdict.
std::string sqp_iteration_table_status_line(SolveStatus status);

/// @brief The trailer's `Start Level` line.
/// @param level `SqpCounters::start_level_used` -- the RESOLVED level.
std::string sqp_iteration_table_start_level_line(StartLevel level);

/// @brief The trailer's `Scaling` line; "Scaling: off" when inactive.
/// @param active              `SqpScalingReport::active`.
/// @param obj                 The objective factor.
/// @param row_min             The smallest constraint-row factor.
/// @param row_max             The largest constraint-row factor.
/// @param scaled_kkt_residual The residual the convergence test read.
std::string sqp_iteration_table_scaling_line(bool active, double obj, double row_min,
                                             double row_max, double scaled_kkt_residual);

} // namespace hven::solvers
