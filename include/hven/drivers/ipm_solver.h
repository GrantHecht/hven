// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The primal-dual interior-point solver.
///
/// `IpmSolver` is a thin handle over a private implementation: the engine's
/// state and machinery live behind a `std::unique_ptr` and are not part of this
/// header. A solver is movable but not copyable. Moving transfers the whole
/// engine, including its cached symbolic analysis and any attached callback,
/// trace sink or ledger. A moved-from solver may only be destroyed or
/// assigned to.

#include <functional>
#include <memory>
#include <string>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/core/solver_status.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/model/problem.h"
#include "hven/warmstart/seeding.h"
#include "hven/warmstart/warm_start_data.h"

namespace hven::solvers {

class IpmSolverImpl;
class TraceSink;
class Ledger;

namespace detail {
struct IpmSolverAccess;
} // namespace detail

/// Primal-dual interior-point solver for continuous NLPs.
///
/// A solve runs the phase sequence named by `IpmOptions::phases` over a
/// `Problem` (model/problem.h), with the barrier, line-search, step-acceptance,
/// recovery and restoration behaviour selected by `IpmOptions`. The problem is an
/// argument of `solve()`; the solver keeps nothing that refers to it between
/// calls except an identity token for the interior-point transcription the
/// Problem holds (structure key, structure epoch and the fixed-variable
/// treatment), which lets a later solve of the same problem reuse the symbolic
/// KKT analysis.
///
/// Thread safety: one solver is not safe for concurrent use. Distinct solvers
/// may run concurrently. See docs/concurrency.md.
///
/// Any other member call on a moved-from solver throws std::logic_error.
class IpmSolver {
  public:
    using VectorXd = Eigen::VectorXd;

    /// Per-iteration KKT hook. This is the one interior-point-only extension of
    /// the shared solver surface: it hands out the assembled, not yet
    /// factorized KKT matrix of the Newton system, a structure the SQP engine
    /// does not have.
    ///
    /// Arguments, in order: iteration number, barrier parameter, the iterate
    /// `XSL`, the objective scale, the primal-dual step `PGX`, the right-hand
    /// side `RHS`, and the KKT matrix. The iterate, right-hand side and matrix
    /// are in the solver's REDUCED space: a bound-fixed variable is eliminated,
    /// so the primal block is narrower than the caller's start point.
    /// `XSL`, `PGX` and `RHS` are read-only views of the solver's storage,
    /// valid only for the duration of the call.
    ///
    /// The matrix is passed by mutable reference. Writing new values into the
    /// entries it already carries is supported. Changing its sparsity pattern
    /// is not: from the hook's first invocation in a call, every numeric
    /// factorization re-derives the buffer's pattern and compares it with the
    /// analyzed one, and a structural edit is refused by name at the first
    /// factorization that sees it.
    using KktHook = std::function<int(
        int, double, const Eigen::Ref<const VectorXd> &, double, const Eigen::Ref<const VectorXd> &,
        const Eigen::Ref<const VectorXd> &, Eigen::SparseMatrix<double, Eigen::RowMajor> &)>;

    /// Constructs a solver over `opts`. The solver binds no program.
    /// @throws std::invalid_argument if `opts` fails validation.
    explicit IpmSolver(IpmOptions opts = {});
    ~IpmSolver();

    IpmSolver(const IpmSolver &) = delete;
    IpmSolver &operator=(const IpmSolver &) = delete;

    /// Transfers the engine. The source may afterwards only be destroyed or
    /// assigned to.
    IpmSolver(IpmSolver &&) noexcept;
    /// Transfers the engine, releasing the one this solver held. The source may
    /// afterwards only be destroyed or assigned to.
    IpmSolver &operator=(IpmSolver &&) noexcept;

    /// The options this solver runs under. Read-only: copy, edit the copy, and
    /// hand it back through `set_options()`.
    const IpmOptions &options() const;

    /// Replaces the whole options value. Transactional: validation runs first,
    /// and on a throw the previous options stay in force. Every field takes
    /// effect on the next solve.
    /// @throws std::invalid_argument if `o` fails validation.
    /// @throws std::logic_error if a solve is in flight (a call from inside a
    ///         callback).
    void set_options(IpmOptions o);

    /// Which exit the last phase of the most recent call left its iteration
    /// loop by. Reset at each phase start. A phase that converges on exactly
    /// its cap iteration reads `kNone`: the convergence, not the cap, ended it.
    /// `kException` after a solve an exception escaped.
    IpmStopReason last_stop_reason() const;

    /// True when `problem`'s interior-point transcription is the one the solver's
    /// current symbolic analysis was laid against and no structural event has
    /// happened to it since. Compared by identity token, never by address. False
    /// before the first solve, for any other problem, and for a problem whose
    /// transcription a bound change re-laid.
    bool kkt_pattern_is_analyzed(const Problem &problem) const;

    /// Runs the configured phase sequence over `problem` from `x0`, optionally
    /// starting from a warm start.
    ///
    /// The problem is borrowed for the call and released on every exit, a throw
    /// included. Its per-solve re-read runs first (model/problem.h). `lambda` and
    /// `g` on the result are in the problem's row space.
    ///
    /// @param problem The problem; borrowed.
    /// @param x0     The start point, in the problem's variable space.
    /// @param warm   The warm start; read, never retained. A default-built
    ///               (empty) value means no warm start: the solve is the cold
    ///               one, counter for counter, so a loop may pass the previous
    ///               result's `warm_start` from its first call. Otherwise
    ///               identity mismatch refuses; value defects degrade. The
    ///               declaration stamp and block lengths are checked at entry
    ///               against the program this call binds and a mismatch is
    ///               `std::invalid_argument`. A seeded multiplier outside the
    ///               seeding band is clamped and applied. What is applied
    ///               depends on `IpmOptions::common.start_level`, a ceiling on
    ///               this route:
    ///                - `kCold`: the value is ignored and counted
    ///                  (`payload_ignored`); the call is bitwise the cold solve.
    ///                  Identity is still checked first.
    ///                - `kSeeded`: only the multipliers are applied. The primal
    ///                  point and the polish extension are ignored (the
    ///                  extension counted in `polish_ignored`). `x0` is the
    ///                  start.
    ///                - `kWarm`: the whole value. Its primal point becomes the
    ///                  starting point, the multipliers are applied as at
    ///                  `kSeeded`, and the `"hven.ipm.polish.v1"` extension,
    ///                  when present, seeds the bound multipliers.
    ///                - `kHot`: identical to `kWarm`. The engine has no hot
    ///                  handle.
    ///               A multipliers-only value (`WarmStart::seed` with an empty
    ///               `x`) leaves `x0` as the start and only the multipliers
    ///               travel.
    /// @param budget Work ceiling. A non-zero `max_iterations` caps each phase
    ///               at `min(budget, IpmOptions::max_iters)`: a caller may
    ///               tighten the engine's limit, never loosen it. `minor_budget`
    ///               is ignored (this engine has no minor loop).
    /// @return The result, by value.
    /// @throws std::invalid_argument if `x0` is not sized to the declared
    ///         primal width, if the options fail validation, if the problem
    ///         refuses the solve (a dimension, structure or row-kind change; an
    ///         Nlp with has_hessian() == false; an SQP-only internal form), or
    ///         if a non-empty `warm` has a non-finite entry, blocks that
    ///         disagree or are not at the declared width, a duplicated,
    ///         unreadable or mis-sized polish extension, or a declaration stamp
    ///         that is not this problem's.
    /// @throws hven::solvers::EvaluationError if the model throws one at the
    ///         start point. At a trial point it is a rejected trial, never
    ///         thrown; a non-finite value is treated exactly the same, and at
    ///         the start point ends the solve `kNumericalError` (core/errors.h
    ///         states the whole contract).
    /// @throws hven::solvers::BackendError if the linear-algebra backend fails
    ///         in a way no status can report.
    /// @throws Any other exception a user callback throws, unchanged. The
    ///         interrupted solve's result is then `last_result()`.
    IpmResult solve(Problem &problem, ConstVecRef x0, const WarmStart &warm = {},
                    SolveBudget budget = {});

    /// The result of the most recent `solve()`: the value it returned, or --
    /// when an exception escaped it -- what the interrupted solve reached,
    /// finished into the problem's row space: status `kInterrupted`, the
    /// iterations taken, the last committed iterate and its multipliers
    /// (`x`, `lambda`, `z`), the work spent and a `message` naming the
    /// exception. Its evaluation-derived fields are unmeasured (`g` empty, `f`
    /// and the shared diagnostics NaN): the exception may have arrived
    /// mid-evaluation. After ANY exception out of `solve()` -- an argument
    /// refusal included -- it describes what was reached, the start point with no
    /// iterations when nothing was. Cleared at the start of every `solve()`;
    /// default-constructed before the first solve.
    /// @throws std::logic_error on a moved-from solver.
    const IpmResult &last_result() const;

    /// Installs the per-iteration callback that both engines accept. An empty
    /// function clears it.
    ///
    /// The callback fires once per `ipm.iter` trace row: at the top of the
    /// iteration that iterate starts, after its residuals are measured and
    /// before its KKT matrix is factorized. The last event of a phase describes
    /// the point that phase returns, including on the convergence-check and
    /// restoration-locally-infeasible exits.
    ///
    /// Returning `CallbackAction::kStop` ends the solve at the event's point,
    /// before factorization, with `kInterrupted`. A stop returned at a phase's
    /// last event is read too: a converged iterate still reports `kOptimal`,
    /// and the remaining phases do not run.
    ///
    /// Nested l1 restoration is silent: its iterations fire nothing, and a stop
    /// is honoured once control is back in the outer loop.
    ///
    /// The callback may throw. The exception propagates out of `solve()`, no
    /// solve-end trace event is emitted, and the solver stays usable.
    ///
    /// Setting or clearing from inside the callback (or the KKT hook) is safe:
    /// the change is deferred until the invocation returns, or failing that to
    /// the next solve entry. One slot serves each setter and the last write
    /// wins.
    ///
    /// @param payload Which parts of `IterationEvent` the consumer reads; the
    ///        engine prepares nothing else. Under `IterationPayload::kScalars`
    ///        the event carries `iteration`, `phase`, `f`, `step_norm`, `mu`
    ///        and `elapsed_seconds`; `x`, `lambda` and `z` are empty views and
    ///        the three declared diagnostics are NaN, meaning "not requested".
    ///        The engine then skips the O(n) work that fills them. The mask is
    ///        stored with the callable, parked through the same in-flight
    ///        guard, and reset to `kAll` by `clear_iteration_callback()` and by
    ///        passing an empty callable.
    /// @param scope This engine has no nested solves, so every event is
    ///        top-level and the scope is accepted with no effect.
    ///
    /// The stream has one event per iteration row, so the number of events
    /// equals the iteration count. `IterationEvent::lambda` is in the
    /// problem's row space, as `SolveResult::lambda` is, and costs a compose
    /// per event only under a payload that includes `kVectors`.
    void set_iteration_callback(IterationCallback cb,
                                IterationPayload payload = IterationPayload::kAll,
                                CallbackScope scope = CallbackScope::kTopLevel);

    /// The payload mask currently in force.
    IterationPayload iteration_payload() const;

    /// The callback scope currently in force (`kTopLevel` when none is set).
    CallbackScope iteration_scope() const;

    /// Removes the callback and resets the payload mask to `kAll` and the scope
    /// to `kTopLevel`. Deferred
    /// like `set_iteration_callback()` when called from inside a callback.
    void clear_iteration_callback();

    /// Installs the per-iteration KKT hook (see `KktHook`). Setting from inside
    /// the hook is safe: the replacement is deferred until the hook returns.
    void set_kkt_hook(const KktHook &f);

    /// Removes the KKT hook. Deferred like `set_kkt_hook()` when called from
    /// inside a hook.
    void clear_kkt_hook();

    /// Attaches a trace sink; `nullptr` detaches. The sink is borrowed and must
    /// outlive every solve made while it is attached.
    ///
    /// When `common.print_level` enables printing, the solver composes its own
    /// console sink behind the caller's, so attaching a console never displaces
    /// a caller's sink and the caller's stream is the same with printing on
    /// and off.
    /// @throws std::logic_error if a solve is in flight. The composition is
    ///         fixed for the solve, so a call from inside a callback, a hook or
    ///         a sink could not take effect in it.
    void attach_trace(TraceSink *sink);

    /// Attaches a ledger; `nullptr` detaches. One `IpmSolveRecord` is written
    /// per `solve()` call that returns, labelled `"{label_prefix}_{n}"` with
    /// `n` a per-solver counter this call resets to 0. A solve that leaves by
    /// an exception neither records nor consumes a number. The ledger is
    /// borrowed and must outlive every solve made while it is attached.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

  private:
    friend struct detail::IpmSolverAccess;

    IpmSolverImpl &impl();
    const IpmSolverImpl &impl() const;

    std::unique_ptr<IpmSolverImpl> impl_;
};

} // namespace hven::solvers
