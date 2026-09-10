// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

#pragma once
#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include <fmt/color.h>
#include <fmt/core.h>

#include "hven/detail/drivers/interior_point_solver_fwd.h"
#include "hven/detail/interior/bound_set.h"
#include "hven/detail/interior/eval_error_log.h"
#include "hven/detail/interior/iterate_info.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/detail/interior/kkt_vector.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/solve_status.h"
#include "hven/model/non_linear_program.h"
#include "hven/warmstart/seeding.h"
#include "hven/warmstart/warm_start_data.h"

#ifdef USE_ACCELERATE_SPARSE
#include <limits>
#endif

// Forward declarations of the gtest-generated test-fixture classes and the test
// harnesses befriended below (global namespace, per gtest's TEST() expansion).
class RecoveryDispatchGate_FunnelSelectionConstructsFunnelAcceptance_Test;
class RecoveryDispatchGate_FilterSelectionConstructsFilterAcceptance_Test;
class RecoveryDispatchGate_MonitoredSelectionConstructsMonitoredGovernor_Test;
class RecoveryDispatchGate_MeritPenaltyRuleSelectionReachesTheStrategy_Test;
class FeasibilitySwitch_ProximalSwitchConstructsRestorationAndWrapsRecovery_Test;
class FeasibilitySwitch_OffModeConstructsNoRestoration_Test;
class FeasibilitySwitch_FilterSeedsRestorationConstraintTol_Test;
class NestedSeamHarness;
class NestedSeamIneqHarness;
class NestedLifecycleHarness;
class DivergencePersistenceHarness;
class SocGenericHarness;
class InertiaRegularizationSolve_ClassicDegeneracyLatchTracksSingularity_Test;
class InertiaRegularizationSolve_ActiveBoundCurvatureNeverTripsSingularitySignal_Test;
class InertiaRegularizationSolve_NarrowBoxCurvatureNeverTripsSingularitySignal_Test;
class NativeBoundsHarness;

namespace hven::solvers {

// Pull root-namespace Eigen type aliases into hven::solvers so that InteriorPointSolver
// member declarations (EigenRef<VectorXd>, ConstEigenRef<VectorXd>, …) resolve
// without full qualification inside this namespace.
using hven::ConstEigenRef;
using hven::EigenRef;

// The interior-point polish hand-off (hven/warmstart/ipm_polish_extension.h),
// forward-declared rather than included: it appears here only as a `const &`
// parameter of one private method, and including it would pull the SQP
// crossover header and hven/qp/qp_types.h behind it into every consumer of
// this header. src/drivers/interior_point_solver.cpp takes the include.
struct IpmPolishData;

/// @brief Number of consecutive trailing iterates that must ALL exceed a
///        divergence threshold before converge_check() declares DIVERGING on a
///        finite (but large) residual.
///
/// A non-finite residual remains an immediate hard abort, so this window governs
/// only the finite-overshoot case, where a single blown-up iterate can be a
/// recoverable transient.
/// @see docs/notes/2026-09-header-prose-archive.md §interior_point_solver.h
inline constexpr int kDivergencePersistIters = 3;

// The five globalization components below are held through unique_ptr members
// whose concrete types are complete only in interior_point_solver.cpp, which is
// why the constructors and the destructor are defined out-of-line there. The
// acceptance_strategy.h header includes THIS header, so it must not be included
// back. RestorationStrategy is the one that is not always constructed: it stays
// null unless restoration_mode_ != off.
class AcceptanceStrategy;
class GlobalizationMechanism;
class BarrierGovernor;
class RecoveryChain;
class RestorationStrategy;
struct ProgressMeasures;
struct FeasibilityStallDetector;

/// @brief Forward-declared, not included: `drivers/trace.h` pulls types this
/// driver does not use, and only `attach_trace`'s parameter and one member
/// pointer name this type here. The .cpp takes the include.
class TraceSink;

/// @brief Forward-declared for the same reason (M6 W5 T8.7b): the only place
/// this header names the event is `emit_message`'s parameter, which is a
/// reference, and the nine call sites that build one are all in the .cpp.
struct IpmMessageTraceEvent;

/// The two composition sinks (M6 W5 T8.7, `drivers/console_trace_sink.h`),
/// forward-declared for the same reason and held through `unique_ptr`s whose
/// deleters are instantiated in the .cpp -- which is where the destructor is,
/// so an incomplete type here is fine.
class ConsoleTraceSink;
class FanOutTraceSink;

/// @brief Forward-declared: only `attach_ledger`'s parameter names it here.
class Ledger;

/// Primal-dual interior-point solver for continuous NLPs, driving a phase
/// sequence over barrier/line-search modes with pluggable step acceptance, a
/// barrier governor, a post-rejection recovery chain and optional
/// feasibility-restoration mode switches.
class InteriorPointSolver {
  public:
    // --- Mode enums (declared at namespace scope in drivers/ipm_solver_types.h) ---
    // These eight were NESTED here until M6 W5 T8.3 moved them out beside
    // IpmOptions, so a caller can name an option's value without including the
    // solver. The aliases keep every InteriorPointSolver::<Enum> spelling in the
    // tree naming the same type it always did. The four strto_*() parsers that
    // sat here went with the string-taking setters: a caller builds an
    // IpmOptions value and names the enumerator.
    using BarrierModes = hven::solvers::BarrierModes;
    using LineSearchModes = hven::solvers::LineSearchModes;
    using AlgorithmModes = hven::solvers::AlgorithmModes;
    using QPAlgModes = hven::solvers::QPAlgModes;
    using QPOrderingModes = hven::solvers::QPOrderingModes;
    using BestCriteriaModes = hven::solvers::BestCriteriaModes;
    using QPPivotModes = hven::solvers::QPPivotModes;
    using PDStepStrategies = hven::solvers::PDStepStrategies;

    // The options struct that used to live here as InteriorPointSolver::Settings
    // is now hven::solvers::IpmOptions in drivers/ipm_solver_types.h: the same
    // 65 knobs in the same order, trailing underscores gone, with qp_threads_
    // and print_level_ moved into CommonOptions and reached as
    // options().common.threads / options().common.print_level. Read it through
    // options(), replace it through set_options().

    // The nested struct SolveResult that lived here is GONE (M6 W5 T8.4). Its
    // replacement is hven::solvers::IpmResult in drivers/ipm_solver_types.h --
    // the same fields under the same names with the trailing underscores gone,
    // on top of the shared hven::solvers::SolveResult base that carries the
    // answer itself (status, x, lambda_e, lambda_i, z, f, the four shared
    // declared diagnostics, ce/ci, iterations, wall_seconds and the warm-start
    // snapshot) in DECLARED space and CALLER units.
    //
    // It is RETURNED BY VALUE from solve(), not accumulated on the solver: the
    // result()/kkt_analysis_count()/kkt_factor_counters()/eval_error_log()
    // accessors are gone with it, and every question they answered is answered
    // by a field on the returned value. reset_accumulators() is retired -- a
    // fresh IpmResult is default-constructed at every solve entry, and its
    // default member initializers ARE the sentinels the reset used to write.

    /// @brief Shorthand for Eigen::VectorXd, used by this class's callback
    ///        signatures and entry points.
    using VectorXd = Eigen::VectorXd;

    /// @brief Type of the per-iteration KKT hook -- THE ONE INTERIOR-POINT-ONLY
    ///        EXTENSION of the shared solver surface (M6 W5 T8.6).
    ///
    /// LABELLED AS SUCH AND NOT INVENTED FOR THE SQP ENGINE. The shared
    /// per-iteration callback both engines carry is IterationCallback
    /// (drivers/solve_result.h), installed with set_iteration_callback(); this
    /// hook is a different thing entirely -- it hands out the ASSEMBLED,
    /// NOT-YET-FACTORIZED KKT matrix of the interior-point Newton system, which
    /// is a structure the SQP engine does not have. Nothing equivalent exists
    /// there, deliberately (design section 2.5).
    ///
    /// Semantics are UNCHANGED from W5 T2's early callback; only the name is
    /// new. See docs/notes/2026-09-m6-w5-migration-guide.md.
    ///
    /// The iterate, right-hand side and KKT matrix are the SOLVER's own, in its
    /// REDUCED primal space: a bound-fixed variable is eliminated, so the primal
    /// block is narrower than the caller's initial guess and every segment offset
    /// follows the narrowed width. Map through
    /// NonLinearProgram::reduced_to_full(), or rebuild a full-space primal vector
    /// with scatter_full_x(). The returned solution is always in the caller's
    /// space.
    ///
    /// The KKT matrix is handed over by mutable reference, assembled and not yet
    /// factorized. Writing new VALUES into the entries it already carries is
    /// supported. Changing its SPARSITY PATTERN is not: from this callback's first
    /// invocation in a call, every numeric factorization in that call re-derives
    /// the buffer's pattern and compares it against the analyzed one, so a
    /// structural edit is refused by name at the first factorization that sees it.
    /// A call in which this callback never runs pays no such check.
    ///
    /// XSL, PGX and RHS are READ-ONLY borrowed views of the solver's own storage,
    /// valid for the duration of the call and showing this iteration at its
    /// EVALUATION stage: the model is evaluated, the matrix assembled, the
    /// factorization not yet run. The solver may overwrite that storage at any
    /// point after the callback returns, including before this iteration computes
    /// a step at all -- the nested-restoration pre-exit re-initialises XSL's
    /// multiplier blocks and abandons the iteration
    /// (src/drivers/interior_point_solver.cpp:2199).
    /// @see docs/notes/2026-09-header-prose-archive.md §interior_point_solver.h
    using KktHook =
        std::function<int(int, double, ConstEigenRef<VectorXd>, double, ConstEigenRef<VectorXd>,
                          ConstEigenRef<VectorXd>, Eigen::SparseMatrix<double, Eigen::RowMajor> &)>;

    // LateCallBackType WENT in M6 W5 T8.6, and IterateInfo left the CALLBACK
    // surface with it -- it remains the trace row type in drivers/trace.h,
    // which hands it out by reference as IpmIterTraceEvent::iterate exactly as
    // the IPQP's trace event types are handed out, and T8.7's console sink
    // renders from it (fix1, ruling R3: the earlier "left the public surface"
    // was wrong -- a type reachable through an installed public header on a
    // public event is on the public surface). Its replacement ON THE CALLBACK
    // is the SHARED per-iteration callback
    // both engines take -- hven::solvers::IterationCallback, over
    // IterationEvent, in drivers/solve_result.h -- installed with
    // set_iteration_callback() below. The old type handed out this engine's own
    // per-iteration record in this engine's own space; the new one hands out
    // one view of one iteration, in DECLARED space and CALLER units, that a
    // caller can write once against either engine. The migration guide has the
    // field-by-field table.

    // --- Constructors / destructor ---
    // All three are defined out-of-line in interior_point_solver.cpp: the
    // unique_ptr members with incomplete element types force even the
    // constructors' exception-cleanup paths to see the complete types.

    /// @brief Constructs a solver over `opts`.
    ///
    /// A solver binds NO program. The program is an argument of solve() and is
    /// borrowed for the duration of that call only (M6 W5 T8.4) -- the
    /// model-taking constructor, set_nlp() and release() are gone, and so is
    /// the shared_ptr this class used to hold. What DOES survive across calls
    /// is the cross-call reuse the engine earns on its own: the symbolic
    /// analysis, the pattern hash and the partition setup, keyed on the
    /// program's own identity (its structure key, its structure epoch and the
    /// fixed-variable treatment in force) rather than on a retained address.
    /// kkt_pattern_is_analyzed(model) is how a caller asks whether a given
    /// program is the one that reuse currently applies to.
    ///
    /// @param opts The options; validated here, exactly as set_options() would.
    /// @throws std::invalid_argument if validate(opts) rejects the value.
    explicit InteriorPointSolver(IpmOptions opts = {});
    /// @brief Releases the factorization and the globalization components.
    ~InteriorPointSolver();

    // Neither copyable nor movable: the kkt_sol_ factorization and the
    // unique_ptr globalization components have no defined transfer semantics.

    /// @brief Deleted: a solver is not copy-constructible.
    InteriorPointSolver(const InteriorPointSolver &) = delete;
    /// @brief Deleted: a solver is not copy-assignable.
    InteriorPointSolver &operator=(const InteriorPointSolver &) = delete;
    /// @brief Deleted: a solver is not move-constructible.
    InteriorPointSolver(InteriorPointSolver &&) = delete;
    /// @brief Deleted: a solver is not move-assignable.
    InteriorPointSolver &operator=(InteriorPointSolver &&) = delete;

    // --- Accessors ---
    /// @brief Returns the options this solver runs under.
    ///
    /// The value is READ-ONLY: there is no mutable accessor and no per-field
    /// setter. Copy it, edit the copy, hand it back through set_options() --
    /// which validates the whole value and either takes all of it or none.
    const IpmOptions &options() const noexcept { return opts_; }

    /// @brief Replaces the whole options value.
    ///
    /// TRANSACTIONAL: validate(o) runs first, and a throw there leaves the
    /// previous options in force and the solver usable. Legal BETWEEN solves
    /// only.
    ///
    /// EVERY FIELD TAKES EFFECT ON THE NEXT SOLVE, and T8.3's twelve-field
    /// refusal is GONE with the thing it protected against (M6 W5 T8.4).
    ///
    /// The refusal existed because twelve fields -- qp_ord, qp_pivot_perturb,
    /// qp_ref_steps, qp_matching, qp_scaling, qp_pivot_strategy, qp_alg,
    /// qp_par_solve, qp_print, cnr_mode, and on Accelerate builds
    /// accel_pivot_tolerance and accel_zero_tolerance -- were read exactly once,
    /// inside set_qp_params(), which ran from set_nlp(). Changing one while a
    /// program was attached would have been SILENTLY INERT until the next
    /// set_nlp(), so this method refused it by name.
    ///
    /// There is no attachment any more. The program is an argument of solve()
    /// and set_qp_params() runs from solve(), on the entry that BINDS a program
    /// this solver has not analysed yet -- so there is nothing for a changed
    /// field to be inert against: it is read the next time transcription
    /// happens. What survives across solves of the SAME program is the
    /// analysis, and set_options() marks it stale for exactly these twelve, so
    /// the next solve re-transcribes under the new value rather than silently
    /// keeping the old one. Nothing is refused and nothing is silently
    /// ignored.
    ///
    /// @param o The replacement options.
    /// @throws std::invalid_argument if validate(o) rejects the value.
    /// @throws std::logic_error if a solve is in flight (a replacement from
    ///         inside a callback).
    void set_options(IpmOptions o);

    /// @brief Which door the last phase of the most recent call left its
    ///        iteration loop by.
    ///
    /// PHASE-LOCAL and exact: reset to kNone at each phase start, written only
    /// from that phase's own loop, and never derived from a per-CALL field. The
    /// loop has five exits and the label covers each of them: the restoration
    /// locally-infeasible break (kRestorationLocallyInfeasible), the
    /// converge-check early exit (kNone -- that phase's verdict is the whole
    /// explanation), the terminal conjunction (kStageStalled if the stall fired
    /// this iteration, kIterationCap if this was the cap iteration AND the
    /// phase's own local verdict is NOTCONVERGED, kNone otherwise), exhaustion
    /// after one of the loop's `continue`s bypassed the conjunction on the cap
    /// iteration (kIterationCap), and an exception, which unwinds without
    /// producing a result at all.
    ///
    /// So the label never contradicts the verdict it is reported beside. A phase
    /// that CONVERGES on exactly its cap iteration reads kNone: the cap did not
    /// stop it, the convergence did. Only a phase that ran out of iterations
    /// with nothing better to say reads kIterationCap.
    ///
    /// A stall coinciding with the cap reports kStageStalled: the stall is
    /// recorded first and both cap doors defer to a reason already in place.
    ///
    /// THE VERDICT IT IS REPORTED BESIDE IS NOW PER PHASE TOO (M6 W5 T8.4).
    /// The engine resets its verdict at every phase start, so the stale-verdict
    /// defect design §2.3 registered for this task -- a later phase leaving
    /// without assigning a verdict and reporting an earlier phase's -- is gone
    /// by construction. `IpmResult::phases` carries both per phase; this
    /// accessor reports the LAST RAN phase's reason and is kept because a
    /// caller mid-callback has no result to read yet.
    ///
    /// @return The stop reason recorded by the last phase that ran.
    IpmStopReason last_stop_reason() const noexcept { return last_stop_reason_; }

    // eval_error_log(), kkt_analysis_count() and kkt_factor_counters() are GONE
    // (M6 W5 T8.4). Each answered a question about a solve after the fact, from
    // a solver that had gone on living; the answers are now SNAPSHOT FIELDS on
    // the value solve() returns -- IpmResult::eval_error_log,
    // ::kkt_analyses_total, ::kkt_analyses_this_call and
    // ::kkt_factor_counters -- so they describe the solve that produced them
    // and cannot go stale behind a caller's back.

    /// @brief True when @p model is the program this solver's current analysis
    ///        was laid against, and no structural event has happened to it
    ///        since.
    ///
    /// TAKES THE MODEL (M6 W5 T8.4, design §2.2). The no-argument form
    /// dereferenced a retained program; with the program borrowed only for the
    /// duration of a solve there is nothing to dereference between calls, so
    /// the caller names the program it is asking about. What is compared is the
    /// LIFETIME-SAFE IDENTITY TOKEN -- the program's structure key, its
    /// structure epoch and the fixed-variable treatment in force -- never an
    /// address, so a program that merely happens to sit where an earlier one
    /// did cannot answer true.
    ///
    /// False before this solver's first solve, false for any program other than
    /// the last one it analysed, and false over the whole span between a re-lay
    /// and the analysis that answers it -- a span a caller can open by
    /// renegotiating the partition count, or re-transcribing, between two
    /// solves. The next solve of that program closes it.
    ///
    /// @param model The program to ask about.
    /// @return Whether a solve of @p model would reuse the current analysis.
    bool kkt_pattern_is_analyzed(const NonLinearProgram &model) const;

    // --- The entry point ---
    //
    // ONE, taking the program (M6 W5 T8.4). The five phase-named entries --
    // optimize(), solve(), solve_optimize(), optimize_solve() and
    // solve_optimize_solve() -- were five instances of one rule; the rule is
    // now IpmOptions::phases and the sequence is an option like any other. The
    // table from each old entry to its phase sequence is in
    // docs/notes/2026-09-m6-w5-migration-guide.md, and on IpmOptions::phases
    // itself.
    //
    // set_nlp() and release() went with them: a solve BORROWS the program for
    // the duration of the call and holds no pointer to it afterwards.

    /// @brief Runs the configured phase sequence over @p model from @p x0.
    ///
    /// The program is BORROWED for this call: the solver binds it on entry and
    /// releases it on every exit, a throw included. It keeps no pointer to it,
    /// so the caller may destroy it the moment this returns.
    ///
    /// @param model  The program to solve; borrowed for the call.
    /// @param x0     The starting point, in @p model's DECLARED variable space.
    /// @param budget The caller's work ceiling. `max_iterations`, when non-zero,
    ///               caps EACH PHASE at min(budget, IpmOptions::max_iters) -- a
    ///               caller may tighten this engine's limit, never loosen it.
    ///               `minor_budget` is IGNORED: this engine has no minor loop.
    /// @return The result, by value.
    /// @throws std::invalid_argument if @p x0 is not sized to @p model's
    ///         declared primal width, or if validate(options()) rejects.
    IpmResult solve(NonLinearProgram &model, const Eigen::VectorXd &x0, SolveBudget budget = {});

    /// @brief Runs the configured phase sequence over @p model from @p x0,
    ///        starting from the shared warm-start PAYLOAD @p warm.
    ///
    /// THE PAYLOAD ROUTE, one protocol on both engines (design 2.4). @p warm is
    /// the declared-space currency `WarmStartData` -- the same value the SQP
    /// accepts through its own payload overload, and the same value
    /// `SolveResult::export_warm_start()` hands back. It is an ARGUMENT, not
    /// staged state: it applies to this call and nothing outlives it.
    ///
    /// THE RULE. Identity mismatch REFUSES; pattern and value defects DEGRADE.
    /// Identity is the declaration stamp plus the block lengths, both checked
    /// at solve entry against the program this call binds, both
    /// `std::invalid_argument`. Value defects -- a seeded multiplier outside
    /// [kSeededIqMultFloor, kSeededMultInitMax] -- are clamped and applied, as
    /// they always were.
    ///
    /// WHAT IS APPLIED depends on `IpmOptions::common.start_level`, which is a
    /// CEILING on this route and has four rungs (design 2.6):
    ///
    ///   kCold   the payload is IGNORED ENTIRELY and COUNTED
    ///           (`IpmResult::payload_ignored == 1`). The call is then bitwise
    ///           the solve it would have been with no payload at all -- the
    ///           lengths and the stamp are still checked first, so a foreign
    ///           payload is still refused rather than quietly discarded.
    ///   kSeeded only the MULTIPLIERS are applied (`eq_lmults_`/`iq_lmults_`,
    ///           through the same install site, clamps and scale handling the
    ///           removed set_initial_multipliers() fed); `primal_` and the
    ///           polish extension are ignored, the extension counted
    ///           (`IpmResult::polish_ignored`). `x0` is the start.
    ///   kWarm   the WHOLE payload: `primal_` becomes the starting point,
    ///           mapped declared -> reduced and pushed into the interior like
    ///           any starting point; the multipliers as at kSeeded; the
    ///           `"hven.ipm.polish.v1"` extension, when present, seeds the
    ///           bound multipliers after the push.
    ///   kHot    IDENTICAL to kWarm. This engine has no hot handle to offer or
    ///           adopt -- a KKT factorization is reused across calls through
    ///           the program's own analysis identity, never through a payload
    ///           -- so the top rung is documented as equal to kWarm rather
    ///           than refused.
    ///
    /// THE MULTIPLIERS-ONLY SEED. A payload whose `primal_` is EMPTY is the
    /// seed form: `x0` is the start and the multipliers are what travel. Its
    /// hand-over rule is `primal_` and `bound_lmults_` EITHER both empty OR
    /// both at the declared primal width; `eq_lmults_`/`iq_lmults_` must always
    /// match the declared row counts. `bound_lmults_` is never installed on
    /// either form -- the signed core block does not invert into the
    /// (z_lower, z_upper) pair the barrier state needs -- and a polish
    /// extension on a seed is IGNORED and counted, its bound duals being
    /// stated at a point this call is not standing on.
    ///
    /// @param model  The program to solve; borrowed for the call.
    /// @param x0     The starting point, in @p model's DECLARED variable space.
    ///               Used whenever the resolved treatment does not install
    ///               `warm.primal_` -- which is every rung below kWarm, and the
    ///               multipliers-only form at every rung.
    /// @param warm   The payload; read, never retained.
    /// @param budget As the cold overload above.
    /// @return The result, by value.
    /// @throws std::invalid_argument if @p x0 is mis-sized, if validate(options())
    ///         rejects, if any block of @p warm holds a non-finite value, if the
    ///         core blocks disagree with each other (`primal_` and
    ///         `bound_lmults_` neither both empty nor one length), if any block
    ///         is not at the declared width, if the polish extension is
    ///         duplicated, unreadable or not at the core's widths, or if the
    ///         declaration stamp is not this program's.
    IpmResult solve(NonLinearProgram &model, const Eigen::VectorXd &x0, const WarmStartData &warm,
                    SolveBudget budget = {});

    // The ~50 validated set_*() methods that lived here, the four static
    // strto_*() parsers above them and the six string-taking setter overloads
    // among them were REMOVED in M6 W5 T8.3. Their replacement is one value:
    // read options(), write the fields, hand it back through set_options(),
    // which runs the same checks over the whole struct. The setter -> field
    // table for every removed method is in
    // docs/notes/2026-09-m6-w5-migration-guide.md.

    // apply_preset() was REMOVED in M6 W5 T8.3: the preset is now the free
    // function hven::solvers::ipm_preset(name), which RETURNS a full IpmOptions
    // value rather than mutating a solver's settings in place. See
    // drivers/ipm_solver_types.h.

    // --- The shared per-iteration callback (M6 W5 T8.6) ---
    /// @brief Installs the per-iteration callback both engines take.
    ///
    /// WHEN IT FIRES: once per `ipm.iter` trace row -- one event per iterate,
    /// at the TOP of the iteration that iterate starts, after its residuals
    /// have been measured and BEFORE its KKT matrix is factorized. So the event
    /// describes the COMMITTED point of the previous step, with the diagnostics
    /// of that point (design section 2.7 behaviour change (3)); the last event
    /// of a phase describes the point that phase RETURNS -- on every exit,
    /// including the ones that leave from below this dispatch: the
    /// convergence-check early exit and the restoration-locally-infeasible door
    /// each fire a TERMINAL event of their own, after any `return_best`
    /// substitution has chosen the point, so the event's `x` is the point the
    /// phase hands back (M6 W5 T8.6 fix1; before it, that door fired no event
    /// at all and the caller never saw the iterate it got).
    ///
    /// ONE EXIT IS THE EXCEPTION, and it is stated rather than changed: a
    /// `return_best` solve that ends at the BOTTOM-of-loop terminal conjunction
    /// and SUBSTITUTES hands back the best iterate, which an EARLIER event
    /// described -- every iterate gets one -- rather than the last. That exit
    /// fires no terminal event of its own, because the event for its row
    /// already fired at the top of that iteration and "one event per `ipm.iter`
    /// row" is the identity that placement buys.
    ///
    /// NESTED RESTORATION IS SILENT. Under a nested l1 restoration the
    /// feasibility subproblem is solved by a DISTINCT InteriorPointSolver, which
    /// carries no callback of its own: its iterations fire nothing, and a kStop
    /// is honoured only once control is back in this solver's loop. The SQP
    /// engine forwards into its restoration sub-solve and this one does not;
    /// forwarding here is registered as a T8-close disposition item rather than
    /// done in T8.6.
    ///
    /// WHAT kStop DOES: the solve ends at the point the event just described --
    /// pre-factorization, so no work is spent on an answer the caller no longer
    /// wants -- and reports kInterrupted (IpmStopReason::kInterrupted). A stop
    /// returned at a phase's LAST event is read too: the phase's own verdict
    /// stands (a CONVERGED iterate still reports kOptimal -- converged beats
    /// stop), and the remaining phases do not run, reporting `ran == false`.
    ///
    /// The callback may throw; the exception propagates out of solve(), no
    /// solve-end trace event is emitted, and this solver stays usable.
    ///
    /// SETTING OR CLEARING FROM INSIDE THE CALLBACK IS SAFE (M6 W5 T8.6 fix1).
    /// Either call made while the callback is on the stack is DEFERRED: the
    /// stored std::function is left alone until the invocation returns, and the
    /// change is applied at that point (and, failing that -- a callback that
    /// left by throwing -- at the next solve entry). So a callback may disarm
    /// itself and go on touching its own captures. The same rule holds for
    /// set_kkt_hook()/clear_kkt_hook() called from inside the hook.
    ///
    /// @param cb The callback. An empty std::function is the same as clearing.
    void set_iteration_callback(IterationCallback cb) {
        if (this->callback_in_flight_ || this->solve_in_flight_) {
            this->pending_callback_ = std::move(cb);
            // WHICH SAFE POINT APPLIES IT (M6 W5 T8.7b, the lane's Q5).
            //
            // FROM INSIDE A CALLER-SUPPLIED INVOCATION -- the iteration
            // callback or the KKT hook -- the parked value is applied the
            // statement after that invocation returns. That is T8.6's contract
            // and it is unchanged: a callback may still arm a hook mid-solve
            // and see it fire in this solve, which
            // `StructureEpochGating.AnEarlyCallbackArmedFromInsideTheLate
            // CallbackVerifiesFromThatHandOutOn` pins.
            //
            // FROM ANYWHERE ELSE WHILE A SOLVE RUNS -- and since T8.7b that
            // means A SINK METHOD, which now fires from inside a factorization
            // -- the value is applied at the NEXT SOLVE'S ENTRY, so the solve
            // in progress is bitwise the solve it would have been. Before this
            // task such a call took the DIRECT branch below, mid-iteration.
            //
            // ONE SLOT PER SETTER, AND THE LAST WRITE WINS (M6 W5 T8.7b fix1,
            // the lane's M3). A sink-origin park followed IN THE SAME SOLVE by
            // an invocation-origin call to this same setter overwrites both the
            // value and the flag: the callback's value lands when that
            // invocation returns and the sink's is gone. That is the rule a
            // single `std::optional` gives and it is the rule we want -- the
            // most recent request from the caller is the one that takes effect,
            // whoever made it -- rather than a queue that would replay a stale
            // value after a newer one. `IpmDeferral.TheLastWriteWinsWhenASink
            // AndACallbackBothSetInOneSolve` pins the order.
            this->pending_callback_at_entry_ =
                !(this->callback_in_flight_ || this->kkt_hook_in_flight_);
            return;
        }
        // A DIRECT CALL SUPERSEDES ANY PENDING DEFERRAL (M6 W5 T8.7, the SQP
        // lane's M9). A callback that parked a deferral and then THREW never
        // reached the apply point, so the optional is still engaged; without
        // this reset the next solve entry would apply that stale value OVER the
        // callback just installed here.
        this->pending_callback_.reset();
        this->pending_callback_at_entry_ = false;
        this->iteration_callback_ = std::move(cb);
    }
    /// @brief Removes the per-iteration callback.
    ///
    /// Deferred to the safe point when called from INSIDE the callback; see
    /// set_iteration_callback(), whose ONE SLOT this shares -- a park made here
    /// overwrites one made there in the same solve, and the last write wins.
    void clear_iteration_callback() {
        if (this->callback_in_flight_ || this->solve_in_flight_) {
            this->pending_callback_ = IterationCallback{};
            this->pending_callback_at_entry_ =
                !(this->callback_in_flight_ || this->kkt_hook_in_flight_);
            return;
        }
        // See set_iteration_callback(): a direct call supersedes a deferral.
        this->pending_callback_.reset();
        this->pending_callback_at_entry_ = false;
        this->iteration_callback_ = nullptr;
    }

    // --- The interior-point-only KKT hook (M6 W5 T8.6; W5 T2 semantics) ---
    /// Installs the per-iteration KKT hook. The vectors and matrix it receives
    /// are in the SOLVER's variable space, which is narrower than the caller's
    /// on a problem with bound-fixed variables -- see the space note on KktHook
    /// above. THIS IS THE ONE INTERIOR-POINT-ONLY EXTENSION; the SQP engine has
    /// nothing equivalent, by design.
    ///
    /// SETTING FROM INSIDE THE HOOK IS SAFE (M6 W5 T8.6 fix1): the replacement
    /// is DEFERRED to the statement after the running hook returns, so the
    /// callable is never destroyed during its own invocation.
    void set_kkt_hook(const KktHook &f) {
        if (this->kkt_hook_in_flight_ || this->solve_in_flight_) {
            this->pending_kkt_hook_ = f;
            // See set_iteration_callback() for the two safe points. A hook
            // installed from a SINK METHOD is the case T8.7b closes: it used
            // to take the direct branch below and flip `kkt_hook_enabled_`
            // mid-iteration, from inside a factorization. (The pattern
            // VERIFICATION is not the exposure: the hand-out site arms
            // `verify_kkt_pattern_for_solve_` unconditionally beside the
            // hand-out itself, so a hook armed at any moment still verifies
            // from its first hand-out on -- M6 W5 T2. What the deferral buys
            // is that a solve's own behaviour cannot be changed by a sink
            // watching it.)
            //
            // ONE SLOT, LAST WRITE WINS, exactly as on the callback pair above
            // (M6 W5 T8.7b fix1, the lane's M3): this setter and
            // clear_kkt_hook() share one `std::optional`, so a sink-origin park
            // followed by a hook-origin call in the same solve keeps only the
            // second, applied when that hook returns.
            this->pending_kkt_hook_at_entry_ =
                !(this->callback_in_flight_ || this->kkt_hook_in_flight_);
            return;
        }
        // See set_iteration_callback(): a direct call supersedes a deferral.
        this->pending_kkt_hook_.reset();
        this->pending_kkt_hook_at_entry_ = false;
        this->kkt_hook_enabled_ = true;
        this->kkt_hook_ = f;
    }
    /// @brief Removes the KKT hook.
    ///
    /// UNLIKE the disable_early_callback() it replaces, this CLEARS the stored
    /// hook rather than only disarming it: "clear" is what the name says and
    /// what clear_iteration_callback() does, and nothing in the tree re-armed a
    /// disabled hook by calling the setter with no argument.
    ///
    /// CLEARING FROM INSIDE THE HOOK IS SAFE (M6 W5 T8.6 fix1). That is what
    /// disable_early_callback() -- which only flipped a flag -- used to make
    /// safe for free, and what assigning nullptr to the std::function would
    /// otherwise have broken: the callable's storage, its captures included,
    /// would be destroyed during its own invocation. The clear is DEFERRED to
    /// the statement after the hook returns. It shares set_kkt_hook()'s ONE
    /// deferral slot: last write wins.
    void clear_kkt_hook() {
        if (this->kkt_hook_in_flight_ || this->solve_in_flight_) {
            this->pending_kkt_hook_ = KktHook{};
            this->pending_kkt_hook_at_entry_ =
                !(this->callback_in_flight_ || this->kkt_hook_in_flight_);
            return;
        }
        // See set_iteration_callback(): a direct call supersedes a deferral.
        this->pending_kkt_hook_.reset();
        this->pending_kkt_hook_at_entry_ = false;
        this->kkt_hook_enabled_ = false;
        this->kkt_hook_ = nullptr;
    }

    // --- Machine trace (schema v0) ---
    /// @brief Attaches a trace sink; `nullptr` (the default) is off.
    ///
    /// The sink is borrowed and must outlive every solve made while it is
    /// attached. Every emit site null-checks, and an unattached solve builds no
    /// event and does no census.
    ///
    /// @param sink The sink to attach, or `nullptr` to detach. Borrowed, not
    ///             owned.
    ///
    /// THE SINK A SOLVE ACTUALLY WRITES TO IS COMPOSED AT SOLVE ENTRY (M6 W5
    /// T8.7). When `common.print_level` says printing is on, the solver builds
    /// its own `ConsoleTraceSink` and fans out over BOTH -- this sink first,
    /// the console second -- so attaching a console never displaces a caller's
    /// sink, and a caller's stream is byte-identical with printing on and off
    /// (its `ipm.solve.end` line excepted: every one of that line's seven
    /// fields is wall-clock, and two runs of one solve differ there by
    /// construction, console or no console).
    ///
    /// @throws std::logic_error if a solve is in flight on this solver (M6 W5
    ///         T8.7 fix1, the lane's M1 and astra's I1). The composition above
    ///         is fixed for the solve, so a call made from inside an iteration
    ///         callback, a KKT hook or a sink's own method could not take
    ///         effect in it: before this refusal such a call replaced the
    ///         composed fan-out for the rest of the solve -- silencing the
    ///         console mid-table -- and a sink that detached itself from inside
    ///         `on_ipm_iter` left the restoration door's very next emit
    ///         dereferencing a null. `set_options` refuses on the same rule and
    ///         for the same reason, as does `SqpDriver::attach_trace`.
    void attach_trace(TraceSink *sink);

    // --- Instrumentation ledger ---
    /// @brief Attaches a ledger; one `IpmSolveRecord` is written per public
    ///        solve() call that RETURNS.
    ///
    /// The SQP driver's `attach_ledger` in shape and in every rule (see
    /// `drivers/sqp_driver.h`): the ledger is borrowed and must outlive every
    /// solve made while it is attached, the label is
    /// `"{label_prefix}_{n}"` with `n` a per-solver counter this call RESETS to
    /// 0, and the counter advances only when a record is actually written -- so
    /// a solve that leaves by an exception neither records nor consumes a
    /// number.
    ///
    /// TWO THINGS THE SQP DOES THAT THIS DOES NOT. There is no `"_qp"`
    /// forwarding to a subordinate engine (this solver owns no engine object
    /// with its own ledger), and nothing is re-forwarded on `set_options`
    /// (nothing is rebuilt). The engine's nested feasibility restoration runs
    /// INSIDE the solve rather than as a separate solver and writes no record
    /// of its own -- exactly as the SQP's restoration sub-driver receives
    /// `attach_trace` but no `attach_ledger`.
    ///
    /// @param ledger       The ledger to write to, or `nullptr` to detach.
    ///                     Borrowed, not owned.
    /// @param label_prefix The prefix every record's label is built from.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    // --- Constraint-multiplier seeding ---
    //
    // THE TWO VALUES MOVED (M6 W5 T8.5) to `warmstart/seeding.h`, beside the
    // SQP's own seeded-dual band -- three policies, three derivations, one
    // header, values DELIBERATELY NOT UNIFIED (design 2.4). The two names
    // below are ALIASES at the old spellings, kept so nothing that already
    // says `InteriorPointSolver::kSeededIqMultFloor` had to move with the
    // constant; T8.10 rewrites those call sites and drops the aliases.
    /// @brief Alias of hven::solvers::kSeededIqMultFloor (warmstart/seeding.h).
    static constexpr double kSeededIqMultFloor = hven::solvers::kSeededIqMultFloor;
    /// @brief Alias of hven::solvers::kSeededMultInitMax (warmstart/seeding.h).
    static constexpr double kSeededMultInitMax = hven::solvers::kSeededMultInitMax;

    // set_initial_multipliers() / clear_initial_multipliers() and the three
    // staged_*_mults_ / mults_staged_ members they armed were REMOVED in
    // M6 W5 T8.5. The replacement is the MULTIPLIERS-ONLY SEED on the payload
    // route: a WarmStartData whose `primal_` is EMPTY carries multipliers and
    // nothing else, and `solve(model, x0, warm, budget)` applies them through
    // exactly the install site the staged seed used, with the same clamps and
    // the same objective-scale handling. `x0` is then the start, which is what
    // the old two-call shape meant anyway. The seed table is in
    // docs/notes/2026-09-m6-w5-migration-guide.md.

    // --- Warm-start currency ---
    /// The warm-start value captured at the end of the last COMPLETED solve,
    /// valid only while solve_completed_ is true. Built at that point rather
    /// than at export so the stamp and the blocks describe the same solve: a
    /// re-lay between the solve and the export must not stamp these blocks
    /// with a key they were never taken under.
    WarmStartData completed_warm_;
    /// True once a run_phase_sequence() call has RETURNED on this instance. A
    /// call that threw part-way through never reaches the capture and so does
    /// not arm this; convergence is NOT required (a caller reads the verdict
    /// from result().converge_flag_). Never cleared by set_nlp(): the captured
    /// value carries its own stamp, and re-binding is exactly the case that
    /// stamp exists to refuse.
    bool solve_completed_ = false;

    // stage_warm_start() / clear_staged_warm_start() and the staged_warm_ /
    // warm_staged_ pair they armed were REMOVED in M6 W5 T8.5, together with
    // this class's own export_warm_start(). Their replacements:
    //
    //   stage_warm_start(p); solve(m, x0)  ->  solve(m, x0, p)
    //   clear_staged_warm_start()          ->  (nothing to clear: a payload
    //                                           lives exactly as long as the
    //                                           call it is an argument to)
    //   solver.export_warm_start()         ->  result.export_warm_start()
    //                                           (drivers/solve_result.h; an
    //                                           optional, empty when the solve
    //                                           captured nothing)
    //
    // The payload overload is declared beside the cold solve() above. The
    // members below are the CAPTURE side, which is internal state feeding
    // IpmResult::export_snapshot_ and is unchanged.

    // --- Printing ---
    //
    // THE CONSOLE TABLE LEFT THIS CLASS IN M6 W5 T8.7. `print_header()`,
    // `print_banner()`, `print_stats()`, `print_last_iterate()`,
    // `print_timing_summary()` and `calculate_color()` are gone; what they
    // wrote is written by `ConsoleTraceSink` (`drivers/console_trace_sink.h`)
    // from the events this solver emits, and the solver attaches one of those
    // itself when `common.print_level` says printing is on. `print_settings()`
    // was declared and defined with NO caller and is simply deleted.
    //
    // AND M6 W5 T8.7b TOOK THE REST. The per-phase Beginning/Finished lines,
    // the KKT-analysis block, the per-PHASE exit statistics and the nine
    // messages are `ipm.phase.begin`/`.end`, `ipm.kkt_analysis`,
    // `ipm.phase.exit` and `ipm.message`; `interior_point_solver_print.cpp` is
    // gone. `grep fmt::print src/drivers/interior_point_solver.cpp` finds
    // nothing in the solve path -- the console is a SINK now, in full, and a
    // caller's own sink receives every fact the transcript carries.

  private:
    // Test access: these unit tests verify which concrete acceptance strategy
    // the settings dispatch constructs; befriended narrowly instead of
    // exposing a public rebuild hook.
    friend class ::RecoveryDispatchGate_FunnelSelectionConstructsFunnelAcceptance_Test;
    friend class ::RecoveryDispatchGate_FilterSelectionConstructsFilterAcceptance_Test;
    friend class ::RecoveryDispatchGate_MonitoredSelectionConstructsMonitoredGovernor_Test;
    friend class ::RecoveryDispatchGate_MeritPenaltyRuleSelectionReachesTheStrategy_Test;

    IpmOptions opts_;
    // The result under construction for the call in flight; MOVED OUT at the
    // exit seam and returned by value. Not a cross-call accumulator any more:
    // solve() default-constructs it at entry, which is what retired
    // reset_accumulators() (a default IpmResult already carries every sentinel
    // that reset wrote).
    //
    // ITS SHAPES ARE THE ENGINE'S WHILE THE SOLVE RUNS and the CALLER's only at
    // the exit seam: lambda_e/ce carry the internal fixing rows in their tail
    // until run_phase_sequence splits them off into internal_fixed_*, z is
    // dense over the REDUCED primal space until the reinsertion seam scatters
    // it to declared width, and f/lambda_* carry the objective scale until
    // unscale_reported_outputs divides it out.
    IpmResult result_;
    EvalErrorLog eval_error_log_;
    // BORROWED, NOT OWNED (M6 W5 T8.4). Non-null only for the duration of one
    // solve() call: bound after the argument checks and nulled on every exit,
    // a throw included, by a scope guard. Never dereferenced between calls --
    // the cross-call reuse this solver earns is keyed on the identity token
    // below, never on this pointer.
    NonLinearProgram *nlp_ = nullptr;

    // Classic merit line-search acceptance, held through the AcceptanceStrategy
    // interface. Rebuilt by rebuild_globalization_components(); never null once
    // run_phase_sequence has run it once, which every entry point guarantees.
    std::unique_ptr<AcceptanceStrategy> acceptance_;

    // Step-length globalization mechanism (fraction-to-boundary + backtracking),
    // on acceptance_'s lifetime discipline.
    std::unique_ptr<GlobalizationMechanism> mechanism_;

    // Barrier-parameter governor, on acceptance_'s lifetime discipline.
    std::unique_ptr<BarrierGovernor> governor_;

    // Post-rejection recovery chain, on acceptance_'s lifetime discipline. With
    // max_soc_ == 0, ls_extended_iters_ == 0 and watchdog_ == false it is a plain
    // NoopRecovery, which always returns kAcceptAsIs and is stateless; opt in to
    // any subset through the corresponding Settings fields.
    std::unique_ptr<RecoveryChain> recovery_;

    // Optional feasibility-restoration mode-switch. NOT always constructed:
    // rebuild_globalization_components() leaves it null unless restoration_mode_
    // != off, and every restoration branch in the solver guards on it being
    // non-null. run_phase_sequence() resets it at each phase boundary and
    // collects its diagnostics into IpmResult's last_feas_rest_* fields.
    std::unique_ptr<RestorationStrategy> restoration_;

    // Degeneracy latch (Ipopt hess_degenerate_/jac_degenerate_ adaptation,
    // simplified to sticky-per-phase): set by factor_impl when the on-demand
    // dual regularization first engages; later classic base attempts pre-apply
    // δ_c instead of re-discovering the singularity. Reset at each alg_impl
    // phase init. See inertia_regularization.h for the δ_c reference.
    bool dc_latched_ = false;

    // (Re)builds acceptance_/mechanism_/governor_/recovery_ from the current
    // Settings, once per run_phase_sequence() -- after the variable-treatment
    // configuration and before the first phase -- and NOT from set_nlp(), so a
    // construction-time knob takes effect on the next solve without a
    // re-transcription, like every other Settings field.
    void rebuild_globalization_components();

    // QP parameter setup — called automatically by set_nlp()
    void set_qp_params();

    /// Transcribes the bound program: drops the previous program's state,
    /// re-reads the dimensions, hands the backend-configuration options to the
    /// factor, and lays + schedules the KKT sparsity analysis. What set_nlp()
    /// used to do; runs from the solve entry now. PRECONDITION: nlp_ is bound.
    void transcribe_bound_program();

    /// Whether the analysis in hand is the one @p model's current structure was
    /// laid against -- the identity token of design §2.2, WITHOUT the
    /// option-staleness conjunct the public query adds.
    bool analysis_matches(const NonLinearProgram &model) const;

    // Re-reads the problem dimensions from the NLP into the members below.
    // Called by set_nlp, and again at solve entry whenever the fixed-variable
    // configuration changed the size of the problem the solver factorizes.
    void refresh_nlp_dimensions();

    /// @brief Divides Settings::obj_scale_ back out of the reported objective
    ///        value and the three multiplier blocks, once per solve call.
    ///
    /// What leaves this class is the CALLER's problem: nlp_model.h's stationarity
    /// convention is stated at a unit scale. A unit scale returns untouched.
    void unscale_reported_outputs();

    /// @brief Lays the KKT sparsity pattern into the assembly buffer, records
    ///        the structure epoch it was laid against, and schedules the
    ///        symbolic analysis over it.
    ///
    /// The one place this solver analyzes, so the epoch record and the
    /// analysis cannot drift apart: every path that re-lays the pattern goes
    /// through here, and none of them writes analyzed_structure_epoch_ itself.
    void analyze_kkt_sparsity();

    /// @brief The pattern-guard mode a numeric factorization runs under right
    ///        now: kAssumeAnalyzed while kkt_pattern_is_analyzed() holds and this
    ///        call is not verifying throughout, kVerify otherwise.
    ///
    /// One epoch read per factorization in place of one full-KKT pattern hash.
    ///
    /// @return kAssumeAnalyzed or kVerify.
    KktFactorization::PatternCheck kkt_pattern_check() const;

    /// @brief Empties the four constraint-indexed result blocks -- the equality
    ///        and inequality multipliers and residuals.
    ///
    /// Called wherever result_.bound_lmults_ is cleared. alg_impl writes each pair
    /// only when the current problem has rows for it, so without this a reused
    /// solver would keep an earlier call's block standing -- and the
    /// objective-scale seam would divide it a second time. Emptied rather than
    /// resized: a block with no rows behind it is empty.
    void clear_reported_constraint_blocks();

    // --- Problem dimensions ---
    // primal_vars_ is the SOLVER's primal width: the NLP's variable count minus
    // the variables the fixed-variable treatment eliminated. Every vector this
    // solver sizes, every KKTVector segment and the KKT matrix are in that
    // space. full_primal_vars_ is the problem's own count -- the width of an
    // initial guess and of the returned solution, and the only one a caller
    // ever sees. They are equal unless a variable is bound-fixed.
    int full_primal_vars_ = 0;
    int primal_vars_ = 0;
    int slack_vars_ = 0;
    int equal_cons_ = 0;
    int inequal_cons_ = 0;
    int kkt_dim_ = 0;

    // --- Reusable per-iteration scratch buffers (avoid per-call heap allocation) ---
    // complementarity()/barrier_hessian() are invoked serially from alg_impl's
    // single-threaded control loop. Sized to inequal_cons_/slack_vars_, resized in
    // place, which is a no-op once the size matches.
    mutable Eigen::VectorXd stli_scratch_; ///< @internal complementarity() S*LI buffer.
    Eigen::VectorXd hp_scratch_;           ///< @internal barrier_hessian() LI/S buffer.

    // alg_impl's return_best_ path copies the full XSL/RHS iterate on every
    // improving iteration; hoisted so repeated alg_impl calls reuse the store.
    Eigen::VectorXd best_xsl_scratch_; ///< @internal alg_impl() return_best_ XSL snapshot.
    Eigen::VectorXd best_rhs_scratch_; ///< @internal alg_impl() return_best_ RHS snapshot.
    // bound_duals_ has no XSL/RHS-carried counterpart, so return_best_ needs its
    // own snapshot of it, taken and restored alongside the two above -- otherwise
    // a non-converged return_best_ exit would report bound_lmults_ from the LAST
    // iterate beside the rest from the BEST one.
    BoundDualState best_bound_duals_scratch_; ///< @internal return_best_ bound_duals_ snapshot.

    // Nested feasibility-restoration eval-seam scratch, all dead unless a nested
    // restoration strategy is active, following the *_scratch_
    // no-per-call-allocation discipline. resto_pdiag_scratch_ holds the proximal
    // Hessian diagonal eta(mu)*D_R^2; resto_epiv_/ipiv_scratch_ hold the NEGATED
    // constraint-row pivots; resto_ec_/ic_scratch_ copy the raw constraint
    // residuals out before the condensed r-tilde overwrites the RHS in place.
    Eigen::VectorXd resto_pdiag_scratch_;
    Eigen::VectorXd resto_epiv_scratch_;
    Eigen::VectorXd resto_ipiv_scratch_;
    Eigen::VectorXd resto_ec_scratch_;
    Eigen::VectorXd resto_ic_scratch_;

    // Nested feasibility-restoration lifecycle state, all dead unless a nested
    // strategy is active. stashed_mu_ holds the outer barrier parameter captured
    // at entry; resto_first_iter_ guards the first phase iteration;
    // resto_theta_orig_prev_ carries the previous iteration's original-problem
    // infeasibility for the kappa_resto ratchet, seeded at entry and ratcheted
    // each iteration. A mu-event reset() mid-phase does NOT touch this state --
    // only the phase-boundary reset clears it.
    double stashed_mu_ = 0.0;
    bool resto_first_iter_ = false;
    double resto_theta_orig_prev_ = 0.0;
    Eigen::VectorXd resto_dz_scratch_;
    // The bound families' re-centring steps at that same return, the two sides
    // separate because they index different lists. Deliberately NOT
    // bound_duals_.dz_*, which is the ITERATE's Newton direction. Empty unless a
    // nested restoration phase returns on a problem with variable bounds.
    Eigen::VectorXd resto_bound_dz_lower_scratch_;
    Eigen::VectorXd resto_bound_dz_upper_scratch_;

    // --- Native primal variable bounds (all inert on a problem without any) ---
    //
    // bounds_ points at the NonLinearProgram's classification of the finite
    // variable bounds this solve keeps barrier terms for, in the solver's REDUCED
    // index space. Set ONLY on the configuration success path and ONLY when the
    // set is non-empty; cleared before the configuration attempt and by
    // set_nlp()/release(). Null means "this solve has no variable-bound barrier
    // terms", which every bound branch here and in the globalization components
    // tests.
    const BoundSet *bounds_ = nullptr;
    // The bound multipliers and their Newton step, index-aligned to bounds_'s two
    // lists. Iterate state; sized by the interior push at solve entry and empty
    // whenever bounds_ is null.
    //
    // TWO EVENTS write z, and only two: the iterate commit
    // (apply_bound_dual_step) moves it along dz once per committed iterate with
    // the kappa_Sigma clip against the new x, and the nested restoration return
    // (exit_feasibility_restoration_nested) RE-ANCHORS it on the stashed outer
    // barrier parameter, applying no dz and moving no x.
    BoundDualState bound_duals_;

    // Bound scratch, following the *_scratch_ no-per-call-allocation discipline
    // (resize-on-assign is a no-op once dims are stable across a solve); all
    // three stay empty on a problem without variable bounds.
    //
    // bound_sigma_scratch_ backs the primal-diagonal base vector that carries
    // the condensed bound curvature into the KKT assembly.
    // bound_resid_scratch_ backs the z-form dual-infeasibility block the
    // convergence account reads (mutable: the residual accessor is const).
    // bound_grad_scratch_ holds the primal RHS block as it stands BEFORE the
    // mu-form Newton terms are installed, so the restore after the step is an
    // exact copy back rather than an add-then-subtract round trip.
    Eigen::VectorXd bound_sigma_scratch_;
    mutable Eigen::VectorXd bound_resid_scratch_;
    Eigen::VectorXd bound_grad_scratch_;

    // declaration_primals_scratch_ backs the declaration-space primal block the
    // contract's evaluation entry is handed. Stays empty when no variable is
    // eliminated, where the iterate is viewed directly. Shared with the
    // globalization components through SolverContext.
    Eigen::VectorXd declaration_primals_scratch_;

    // One-shot guard for the second-level elastic re-centering fallback (nested
    // l1 restoration only, disclosure (f) in l1_restoration.h). Set true when
    // an in-phase ladder-exhausted rejection re-centers the elastic pairs
    // instead of taking the failed step; a second consecutive ladder
    // exhaustion while set falls through to accept-as-is (no re-center loop).
    // Cleared on any accepted step and re-armed at each phase entry /
    // phase-boundary reset. Dead unless a nested restoration phase is active.
    bool resto_recentered_ = false;

    // --- KKT solver ---
    // The assembly buffer, the sparse symmetric factor driving it, and the
    // evidence projection the inertia machinery reads (kkt_factorization.h).
    KktFactorization kkt_sol_;
    bool qp_analyzed_ = false;

    // The structure epoch the KKT sparsity analysis was laid against, and whether
    // there has been one at all. An EPOCH rather than the treatment call's own
    // report, because every structural event -- a partition renegotiation, a
    // re-transcription, a declaration adoption replaying identical bounds --
    // re-lays without moving treatment, relax factor or bounds revision, so the
    // treatment call reports no change while the table it left names no
    // destination. Reset with the analysis it describes.
    StructureEpoch analyzed_structure_epoch_;
    bool has_analyzed_structure_epoch_ = false;

    // Lifetime count of KKT sparsity analyses; see kkt_analysis_count().
    Index kkt_analysis_count_ = 0;

    // THE LIFETIME-SAFE IDENTITY TOKEN (M6 W5 T8.4, design §2.2). Which program
    // the analysis above belongs to, recorded as the program's own structural
    // identity rather than as its address -- the whole point being that the
    // address is gone between calls and a new program may reuse it. Read by
    // kkt_pattern_is_analyzed(model) and by the solve entry deciding whether a
    // bound program needs transcribing.
    ModelStructureKey analyzed_structure_key_;
    bool has_analyzed_structure_key_ = false;
    // The value-array address this solver's own assembly buffer had when the
    // analysis was laid, compared against the program's own
    // bound_kkt_destination(). It is what closes the token's one hole: a
    // structure key and an epoch identify a STRUCTURE, and two distinct
    // programs of identical structure both start their epoch counters at 0, so
    // the pair alone cannot tell "the program I analysed" from "a different
    // program that looks like it" -- whose location tables are all -1 and would
    // scatter through an unlaid table. This is an address, but not a RETAINED
    // MODEL address: it names a buffer this solver owns, it is never
    // dereferenced, and the program's side of the comparison is a value the
    // program captured itself and clears on every re-lay.
    const double *analyzed_kkt_values_ = nullptr;
    // The treatment the analysis above was laid under: a treatment switch
    // changes the problem's dimensions, so the same key at the same epoch under
    // a different treatment is NOT the analysed program.
    FixedVariableTreatments analyzed_treatment_ = FixedVariableTreatments::MakeParameter;
    // Set by set_options() when one of the twelve transcription-time fields
    // moved: the next solve re-runs set_qp_params() and re-analyses even for
    // the program the token names. Cleared by that solve.
    bool qp_params_dirty_ = false;

    // THE ONE INPUT the shared declared diagnostics need that only a phase
    // holds: grad f*obj_scale + Je'lambda_e + Ji'lambda_i at the RETURNED
    // iterate, in the SOLVER's reduced primal space, captured at the alg_impl
    // tail beside result_.ce from the same right-hand side and therefore at the
    // same iterate (the return_best_ substitution replaces XSL and RHS
    // together, so the pair stays matched). Empty when no phase ran.
    Eigen::VectorXd exit_grad_lag_;

    // THE PROVENANCE OF THAT RIGHT-HAND SIDE (M6 W5 T8.4 fix1). A captured
    // vector is not a measurement until three things are true of the
    // EVALUATION that produced it, and not one of them can be read off the
    // vector's own size or off the restoration flag the exit happens to carry
    // -- which is exactly what the T8.4 guard tried to do:
    //
    //   (a) OBJECTIVE-BEARING. The feasibility phases evaluate through
    //       eval_soe/eval_kkt_no at objective scale 0.0 and the SOE arm then
    //       ZEROES both primal blocks (see eval_nlp), and while restoration is
    //       active the eval seam substitutes phi_prox / phi_l1 for the declared
    //       objective. A right-hand side from either carries no declared
    //       objective gradient at all, so grad f + J'lambda is NOT what it
    //       holds and a declared stationarity read off it measures something
    //       else. Its other three diagnostics are untouched: they read ce, ci,
    //       x, the box, lambda_i and z, all of which a feasibility phase does
    //       evaluate.
    //   (b) AT THE RETURNED POINT. The restoration-return `continue` paths
    //       re-anchor multipliers into XSL and go round the loop again; when
    //       the loop is then EXHAUSTED (exit door 2), the last right-hand side
    //       belongs to the iterate BEFORE that re-anchoring, not to the one
    //       being returned.
    //   (c) RESTORATION INACTIVE at the evaluation. Nested restoration
    //       replaces the constraint rows with the CONDENSED residuals r-tilde
    //       (eval_nlp's nested arm), so ce/ci are not the declared residuals
    //       either -- which is why an exit failing (c) EMPTIES them rather
    //       than copying them, per drivers/solve_result.h's "absent is never
    //       zero-filled".
    //
    // Recorded where the evaluation happens (cur_eval_prov_), carried through
    // the return_best_ substitution by best_eval_prov_ -- the substitution
    // swaps XSL and RHS, so it must swap their provenance too -- and read
    // ONCE, by the result assembly, out of exit_eval_prov_.
    struct ExitEvalProvenance {
        /// An evaluation produced a right-hand side at all.
        bool has_eval = false;
        /// (a): the evaluation carried the DECLARED objective's gradient.
        bool objective_bearing = false;
        /// (c): feasibility restoration was active AT the evaluation.
        bool restoration_active = false;
        /// (b): the right-hand side still describes the iterate being returned.
        bool at_returned_point = false;
        /// The return_best_ path replaced the pair this describes.
        bool substituted = false;
    };

    /// The provenance of the evaluation the CURRENT phase last took.
    ExitEvalProvenance cur_eval_prov_;
    /// The provenance stored beside best_xsl_scratch_/best_rhs_scratch_.
    ExitEvalProvenance best_eval_prov_;
    /// The provenance of exit_grad_lag_ -- the one the result assembly reads.
    ExitEvalProvenance exit_eval_prov_;

    /// The OBJECTIVE of the iterate stored in best_xsl_scratch_, snapshotted
    /// beside it (M6 W5 T8.4 fix1). Without it a return_best_ exit reported
    /// `f` from iters.back() -- the LAST iterate -- beside x, multipliers and
    /// residuals from the BEST one, so the objective did not belong to the
    /// point being returned. Read only on the substituted path.
    double best_prim_obj_scratch_ = 0.0;

    /// THE PROCESS-UNIQUE ID OF THIS SOLVER'S CURRENT ANALYSIS (M6 W5 T8.4
    /// fix1), issued at each lay and NEVER reused -- not by this solver, not by
    /// another, not after this object dies. 0 until this solver has analysed.
    ///
    /// It replaces the analysis-identity conjunct that compared a captured
    /// value-array ADDRESS. That address answers "did I lay this program's
    /// tables" only while no OTHER solver can be handed the same address, and
    /// one can: solver A dies while the program it analysed lives on, solver B
    /// is constructed and its assembly buffer lands on the freed allocation --
    /// at which point A's old program passes every conjunct while its retained
    /// analyzed_kkt_matrix_ still names A's destroyed matrix, which the
    /// provider would go on to use.
    ///
    /// PER ANALYSIS RATHER THAN PER CONSTRUCTION, which is where this departs
    /// from the fix as first stated. A per-construction id cannot distinguish
    /// WHICH of two programs this solver analysed LAST -- both would carry it
    /// -- and one analysis is all a solver has; the query
    /// (kkt_pattern_is_analyzed) is required to answer false for the program
    /// displaced by a later one. A per-analysis id is that answer and is
    /// lifetime-safe for the same reason: the counter only ever goes up.
    std::uint64_t analyzed_owner_id_ = 0;

    // THE PER-PHASE ITERATION CEILING IN FORCE, which alg_impl reads in place
    // of opts_.max_iters (M6 W5 T8.4). Written once per call by the solve
    // entry: min(SolveBudget::max_iterations, opts_.max_iters) when the caller
    // named a budget, and opts_.max_iters otherwise -- so a caller can tighten
    // this engine's own limit and never loosen it. Equal to opts_.max_iters on
    // every call that names no budget, which is what makes the whole feature
    // trajectory-neutral by construction.
    int effective_max_iters_ = 0;

    // Whether every factorization of the CURRENT call re-derives the assembly
    // buffer's pattern instead of taking the structure epoch's word for it.
    // Set once at run_phase_sequence() entry and held for the whole call, so a
    // callback that disables itself part-way cannot hand the rest of the solve
    // back the skip after it has already had the matrix.
    bool verify_kkt_pattern_for_solve_ = false;

    // The objective scale THIS call runs under, taken once at
    // run_phase_sequence() entry from the just-validated Settings and read by
    // everything downstream: the entry initialization, the multiplier seed,
    // every phase, and the unscaling of what the call reports. A setting
    // written while a solve is in flight therefore takes effect on the NEXT
    // call rather than splitting one call between two scales.
    double solve_obj_scale_ = 1.0;

    // The barrier parameter the last phase of this solve ended at, on the CALLER's
    // objective scale. Written once per phase beside the rest of result_'s
    // per-phase fields, so a multi-phase call ends with the LAST phase's value.
    // Read by exactly one thing: the polish extension's `mu_`.
    double solve_exit_mu_ = 0.0;

    // Did the last phase of this solve end with feasibility restoration still
    // active? Written once per phase beside solve_exit_mu_, same last-phase-wins
    // semantics. Read by exactly one thing: the capture, which SUPPRESSES the
    // polish extension when it is true -- every block that extension carries is
    // restoration-space on such an exit, and its contract states those blocks as
    // cI(x) and as non-negative prices at a barrier level. The CORE blocks make no
    // such claim. Keyed on the EXIT, not on the values: return_best_ can
    // substitute a clean optimality-mode iterate on these exits and the
    // suppression still applies.
    bool solve_exit_restoration_active_ = false;

    // --- Callbacks ---
    /// The interior-point-only KKT hook and its arming flag; see set_kkt_hook.
    KktHook kkt_hook_;
    bool kkt_hook_enabled_ = false;

    /// THE IN-FLIGHT GUARDS AND THE DEFERRED CHANGES (M6 W5 T8.6 fix1, the SQP
    /// lane's M1). Each flag is true for exactly the duration of one invocation
    /// of the callable beside it; a set_/clear_ made in that window parks its
    /// new value in the optional instead of assigning the std::function that is
    /// running -- which would destroy the callable's storage, and its captures,
    /// underneath itself. An ENGAGED optional holding an EMPTY function is a
    /// deferred clear; engaged and non-empty is a deferred replacement;
    /// disengaged is "nothing pending".
    bool callback_in_flight_ = false;
    bool kkt_hook_in_flight_ = false;
    std::optional<IterationCallback> pending_callback_;
    std::optional<KktHook> pending_kkt_hook_;
    // WHICH OF THE TWO SAFE POINTS the parked value belongs to (M6 W5 T8.7b).
    // False: the statement after the running callback/hook invocation returns
    // (T8.6's contract, unchanged). True: the next solve's ENTRY, which is
    // where a value parked by a SINK method -- or by anything else that
    // reaches a setter mid-solve with no invocation on the stack -- is
    // applied, so the solve in progress is unchanged bitwise.
    bool pending_callback_at_entry_ = false;
    bool pending_kkt_hook_at_entry_ = false;
    /// The SHARED per-iteration callback (M6 W5 T8.6). No separate arming flag:
    /// an installed std::function is armed and an empty one is not, which is
    /// the whole of clear_iteration_callback().
    IterationCallback iteration_callback_;

    /// When this solve's public entry was taken, for IterationEvent::
    /// elapsed_seconds. Written by every public solve() overload at the same
    /// statement that starts the shared wall clock, so the two measure from one
    /// instant. INFORMATIONAL, never asserted (CLAUDE.md section 7).
    std::chrono::steady_clock::time_point entry_time_{};

    /// The sink the CALLER attached, or null. Never owned; see attach_trace().
    TraceSink *user_trace_ = nullptr;

    /// THE EFFECTIVE SINK FOR THE SOLVE IN FLIGHT, or null (M6 W5 T8.7).
    /// Composed at `run_phase_sequence` entry -- the caller's sink alone, the
    /// console alone, or a fan-out over both -- and every emit site in this
    /// class reads THIS, so a caller's stream is byte-identical whether or not
    /// a console is attached beside it. Never owned; points at `user_trace_`,
    /// at `console_` or at `fanout_`, all of which outlive the solve.
    TraceSink *trace_ = nullptr;

    /// This solver's OWN console, built at solve entry when
    /// `common.print_level` is below 3 and destroyed with the solver. Held
    /// through a pointer rather than by value so the class stays default-
    /// constructible against an incomplete `ConsoleTraceSink`, and because
    /// neither sink type is movable.
    std::unique_ptr<ConsoleTraceSink> console_;
    /// The fan-out over `user_trace_` and `console_`, built at solve entry when
    /// both exist. Points INTO this object, which is neither copyable nor
    /// movable, so the pointers it holds are stable.
    std::unique_ptr<FanOutTraceSink> fanout_;

    // --- Instrumentation ledger (M6 W5 T8.7) ---
    /// The attached ledger, or null. Never owned; see attach_ledger().
    Ledger *ledger_ = nullptr;
    /// The prefix every record's label is built from; see attach_ledger().
    std::string label_prefix_;
    /// The number of records this solver has written since the ledger was
    /// attached. Reset by attach_ledger(), advanced ONLY when a record is
    /// actually written.
    Index solve_counter_ = 0;

    /// Numeric factorizations performed by KKT engines this solver has RETIRED
    /// (M6 W5 T8.7 fix1, the lane's M3). `set_qp_params()` -- the one site that
    /// calls `KktFactorization::reconfigure()`, and so the one site that
    /// REPLACES the linear engine -- adds the outgoing engine's count here
    /// before the replacement. Never reset while the solver lives.
    ///
    /// WHY IT IS HERE AND NOT ON `KktFactorization`: that class is a member of
    /// `IpqpEngine`, so a field on it moves every member offset in every QP
    /// kernel object, and those objects are required to stay byte-identical.
    /// The accumulator belongs to whoever triggers the replacement, and that is
    /// this class.
    Index retired_factorizations_ = 0;

    /// @brief Numeric factorizations this solver's KKT engines have performed
    ///        since it was constructed -- MONOTONE, unlike
    ///        `kkt_sol_.counters().factorize_count`, which restarts at zero
    ///        every time `set_qp_params()` replaces the engine.
    ///
    /// The ledger's per-call `factorizations` is the difference of two readings
    /// of this, taken at the two public solve() entries and in record_solve().
    Index lifetime_factorize_count() const {
        return retired_factorizations_ + kkt_sol_.counters().factorize_count;
    }

    /// The 0-based index of the phase `alg_impl` is currently running, written
    /// by run_phase_sequence() before each call and read only by the `ipm.iter`
    /// emit sites. A member rather than an alg_impl parameter because it is
    /// instrumentation: the algorithm itself has no use for it.
    Index trace_phase_ = 0;

    /// The iteration `alg_impl` is currently on, written at the top of its loop
    /// and read by `factor_impl`'s `ipm.message` emits (M6 W5 T8.7b).
    ///
    /// A MEMBER FOR THE SAME REASON `trace_phase_` IS ONE, and for a second:
    /// `factor_impl` does not see the loop counter at all, and threading an
    /// instrumentation-only parameter through its eight-argument signature
    /// would put it in the algorithm's way. `-1` outside a loop iteration,
    /// which the message serializer writes as `null`.
    Index trace_iter_ = -1;

    /// WHAT `alg_impl` LEAVES BEHIND for the `ipm.phase.exit` event (M6 W5
    /// T8.7b), filled at its tail exactly where `print_exit_stats()` was called
    /// and read by `run_phase_sequence` one statement after `alg_impl` returns.
    ///
    /// A COPY OF THE ROW, not a reference: `iters` is an `alg_impl` local and
    /// is destroyed by the time the emit runs. `IterateInfo` is 28 scalars.
    ///
    /// WRITTEN UNCONDITIONALLY, sink or no sink: it is four doubles, two
    /// integers, a bool and one record copy per PHASE -- not per iteration --
    /// and making it conditional would put a second predicate between the
    /// algorithm and its own bookkeeping for no measurable saving.
    /// THE ROW'S INDEX IS NOT KEPT (M6 W5 T8.7b fix1, the lane's M4): the loop
    /// stamps `Citer.iter_ = i` and pushes one row per iteration, so the index
    /// IS `row.iter_` and a second member would be the same number twice.
    struct PhaseExitScratch {
        IterateInfo row;               ///< The row the phase returns.
        bool best_substituted = false; ///< Did `return_best` substitute it?
        double total_s = 0.0;          ///< alg_impl's `Runtimer`.
        double func_s = 0.0;
        double kkt_s = 0.0;
        double print_s = 0.0;
    };
    PhaseExitScratch phase_exit_{};

    // Is a public entry point on THIS object currently inside its solve? Set by
    // an RAII guard at run_phase_sequence()'s entry -- the ONE place all five
    // public entry points funnel through exactly once -- and cleared on every
    // exit, a throw included. Read by set_options() and by attach_trace(), both
    // of which refuse to replace what a running solve is using.
    //
    // NO RESTORATION MODE BUILDS A SECOND SOLVER (M6 W5 T8.7 fix1, astra's
    // Minor; an earlier sentence here said one did). All three modes run IN
    // PLACE on this object: `off` builds no strategy at all, `proximal_switch`
    // builds a `ProximalSwitchRestoration` and `l1_nested` a
    // `NestedL1Restoration`, and both of those are STRATEGY objects that reuse
    // the outer barrier algorithm's own KKT system (see
    // `detail/globalization/l1_restoration.h`: "an in-place phase reusing the
    // outer barrier algorithm's KKT system rather than a separate nested solver
    // instance"). "Nested" in `l1_nested` names the nested PHASE, not a nested
    // solver. A restoration phase therefore never re-enters this guard because
    // it never re-enters a public entry point -- not because it runs on some
    // other object.
    bool solve_in_flight_ = false;

    /// Has the per-iteration callback asked this call to stop? Set by
    /// fire_iteration_event when the callback returns kStop, cleared once per
    /// call at run_phase_sequence's entry (NOT per phase -- a stop in phase 0
    /// has to reach the phase loop that decides whether phase 1 runs).
    ///
    /// PLACED HERE, BEFORE last_stop_reason_, deliberately (M6 W5 T8.6): that
    /// member is the LAST one on purpose, so that appending to this class moves
    /// no existing offset. This bool goes beside solve_in_flight_ instead, in
    /// the padding that flag's own word already had -- so the tail layout is
    /// unchanged and last_stop_reason_ stays last. sizeof() still moves (the
    /// std::function member above), which is named in this task's P-SYM set.
    bool interrupt_requested_ = false;

    /// Which door the current phase left its loop by; see last_stop_reason().
    /// Written by run_phase_sequence (the per-phase reset) and by alg_impl's two
    /// abnormal exits and two cap doors, read by nothing inside the engine
    /// except those stores' own kNone guards. LAST data member on purpose:
    /// appending it moves no existing member's offset.
    IpmStopReason last_stop_reason_ = IpmStopReason::kNone;

    // KKTVector — the compound-KKT segment view — lives in
    // detail/interior/kkt_vector.h as hven::solvers::KKTVector, shared with
    // the globalization components (which are non-member, non-friend).

    /// @brief Create a KKTVector view over a VectorXd using this solver's dimensions.
    KKTVector kkt_view(Eigen::VectorXd &v) {
        return KKTVector(v, primal_vars_, slack_vars_, equal_cons_, inequal_cons_);
    }
    /// @brief The read-only view over const storage, same dimensions. Chosen by
    ///        overload resolution, so a member that only reads its vector can
    ///        take it by `const Eigen::VectorXd &` and still say `kkt_view(v)`.
    ConstKKTVector kkt_view(const Eigen::VectorXd &v) const {
        return ConstKKTVector(v, primal_vars_, slack_vars_, equal_cons_, inequal_cons_);
    }
    /// @brief Refused for a TEMPORARY. The const overload above binds an rvalue
    ///        and ConstKKTVector holds a reference, so the view would outlive its
    ///        storage. Const-qualified so the refusal also covers a call from a
    ///        const member function.
    ConstKKTVector kkt_view(const Eigen::VectorXd &&) const = delete;

    // --- Phase sequence ---
    // Describes one phase in a multi-phase solve strategy. run_phase_sequence
    // executes steps in order, skipping conditional steps when an earlier phase
    // already converged, and re-initializing the KKT system between phases.
    struct PhaseStep {
        AlgorithmModes alg_mode_;
        BarrierModes bar_mode_;
        LineSearchModes ls_mode_;
        const char *label_;
        bool conditional_ = false; // skip if the PRECEDING phase reported
                                   // kOptimal (still runs on kAcceptable /
                                   // kMaxIter; kDiverging and worse break the
                                   // loop before reaching this)
    };

    /// Builds the PhaseStep list IpmOptions::phases names, applying design
    /// §2.2's conditional rule: a kSolve that FOLLOWS a kOptimize is
    /// conditional, everything else is unconditional.
    std::vector<PhaseStep> phase_steps() const;

    // `payload` is the warm-start currency the three-argument public overload
    // was handed, or nullptr on the cold one. Borrowed for the call: this
    // function copies what it needs and retains nothing (M6 W5 T8.5, which
    // replaced the staged_warm_/warm_staged_ pair with this parameter).
    IpmResult run_phase_sequence(NonLinearProgram &model, const Eigen::VectorXd &x,
                                 const std::vector<PhaseStep> &steps, SolveBudget budget,
                                 const WarmStartData *payload);

    // --- Core algorithm (defined in interior_point_solver.cpp) ---
    Eigen::VectorXd alg_impl(AlgorithmModes algmode, BarrierModes barmode, LineSearchModes lsmode,
                             double obj_scale, double MuI, Eigen::Ref<Eigen::VectorXd> xsl);

    Eigen::VectorXd init_impl(const Eigen::VectorXd &x, double Mu, bool docompute);

    // Rejects a mis-sized or non-finite staged seed: eq_mults must be sized to
    // either the problem's user-facing equality row count or the post-treatment
    // count (which additionally counts one internal fixing row per fixed variable
    // under MakeConstraint), iq_mults to inequal_cons_, and every entry must be
    // finite. Called once, from run_phase_sequence, once those counts are final
    // for the call -- before the entry init_impl and before any phase runs.
    void validate_staged_multipliers(const Eigen::VectorXd &eq_mults,
                                     const Eigen::VectorXd &iq_mults);

    // Installs an already-validated eq_mults/iq_mults (see
    // validate_staged_multipliers, always called first) into XSL's
    // multiplier block, clamping every value to +/-kSeededMultInitMax
    // (inequality entries additionally floored at kSeededIqMultFloor). Called
    // from run_phase_sequence, at most once per call -- see the call site
    // there for which init_impl call it follows.
    void apply_staged_multipliers(Eigen::VectorXd &XSL, const Eigen::VectorXd &eq_mults,
                                  const Eigen::VectorXd &iq_mults);

    // Rejects a warm-start value whose blocks are not at the DECLARED dimensions
    // -- primal_ and bound_lmults_ at the primal variable count, eq_lmults_ at the
    // USER equality row count (never the post-treatment count: the currency is
    // declared-space), iq_lmults_ at the inequality row count -- or which holds a
    // non-finite value. Every dimension it reads is treatment-invariant, which is
    // what makes this checkable at staging while the stamp is not. Read off the
    // program, not this solver's cached copies. `entry` names the public entry.
    /// The AGAINST-THE-PROBLEM half: every block at the declared dimensions.
    /// Needs a bound program, so it runs at solve entry (M6 W5 T8.4).
    void validate_warm_start_dimensions(const WarmStartData &data, const char *entry) const;
    /// The PAYLOAD'S-OWN half: every block a real number. Needs no program, so
    /// it runs at the staging call, where a caller can still act on it.
    void validate_warm_start_finiteness(const WarmStartData &data, const char *entry) const;

    // Captures completed_warm_ from result_ and the bound program, and arms
    // solve_completed_. Defensive but not fatal: a failed internal-consistency
    // check skips the capture and leaves solve_completed_ false rather than
    // throwing one line before a completed solve's return. Called once, at the end
    // of run_phase_sequence, after the reinsertion seam and after the
    // objective-scale seam. One structural-key read per solve, memoized per lay.
    void capture_completed_warm_start();

    // Builds the "hven.ipm.polish.v1" extension for the value being captured: the
    // (z_lower, z_upper) pair scattered out of the solver's reduced space into
    // declared coordinates, the inequality values, and the barrier parameter the
    // solve ended at. Returns false, writing nothing, if any internal-consistency
    // check fails, on capture_completed_warm_start's DEFENSIVE-BUT-NOT-FATAL
    // terms. The objective scale is divided out here, on a copy: the pair is read
    // live out of bound_duals_, which must not be mutated by a side product of the
    // solve.
    // PRECONDITION: bounds_ != nullptr AND solve_exit_restoration_active_ == false.
    bool build_polish_extension(const Eigen::VectorXd &iq_values, WarmExtension &out) const;

    // Rejects a staged value whose "hven.ipm.polish.v1" payload cannot be read, is
    // not at the declared widths, holds a non-finite entry, or holds a NEGATIVE
    // entry in either bound-dual block -- the refusal names the tag and, for the
    // last two, the block (and for a negative, the coordinate and its value). A
    // value carrying no such extension is accepted silently; a FOREIGN tag is
    // ignored entirely. The decoded value is DISCARDED: the bytes are the one
    // source of truth and are decoded again at application. `entry` names the
    // public entry in the refusal.
    void validate_staged_polish(const WarmStartData &data, const char *entry) const;

    // Installs a staged polish hand-off's bound multipliers over the fresh seed
    // push_initial_point_interior just wrote: declared -> reduced by the bound
    // set's own index lists, clamped into [kSeededIqMultFloor,
    // kSeededMultInitMax], and multiplied by this call's objective scale. Called
    // once per solve, after the push and before any evaluation. The clamp is a
    // MAGNITUDE guard: validate_staged_polish has already refused a negative.
    // PRECONDITION: bounds_ != nullptr.
    void apply_polish_bound_duals(const IpmPolishData &polish);

    // --- Line search ---
    // The classic merit line search lives in ClassicMeritAcceptance; the
    // fraction-to-boundary step-length lives in BacktrackingLineSearch.
    // alg_impl drives both through mechanism_->compute_step (which fuses the
    // step scaling and acceptance backtrack).

    // --- KKT factorization (defined in interior_point_solver.cpp) ---
    // `finalpert` is the last perturbation DELTA applied via Perturb(), which is
    // what alg_impl's Hpert0 warm start consumes. `cumpert` is display-only: the
    // running SUM of every delta applied during this call, read by nothing else.
    // `base_prox` is the proximal base shift rho_k, read only when inertia_mode_
    // == proximal_regularization. `dual_shift` is the delta_c magnitude AVAILABLE
    // to this call in both modes -- applied up-front by the proximal branch, and
    // on demand at the singularity signal (or up-front once dc_latched_) by the
    // classic one; 0.0 means suppressed. `exhausted` is set, never cleared, when
    // the ladder runs out of attempts with inertia still wrong.
    int factor_impl(bool docompute, bool ZFac, double ipurt, double incpurt0, double incpurt,
                    double &finalpert, double &cumpert, double base_prox, double dual_shift,
                    bool &exhausted);

    bool claim_kkt_analysis();

    void ensure_solver_initialized();

    // --- Barrier math helpers (defined in interior_point_solver.cpp) ---
    void apply_reset_slacks(Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> FXI) const;
    // max_step_to_boundary is now a private helper of BacktrackingLineSearch.
    // The complementarity account mu is driven by and barr_inf_ reports: the
    // inequality slack/multiplier pairs, plus -- when the problem declares
    // variable bounds -- the bound pairs (x-l)*z_L and (u-x)*z_U, which is why
    // it takes the primal block `X` alongside the slack and multiplier blocks.
    void complementarity(Eigen::Ref<Eigen::VectorXd> X, Eigen::Ref<Eigen::VectorXd> S,
                         Eigen::Ref<Eigen::VectorXd> LI, double &avgcomp, double &mincomp,
                         double &maxcomp) const;

    // How many pairs complementarity() reduced into its aggregates, given the
    // slack block length it was handed: the slack/multiplier pairs plus one per
    // finite variable bound. This is the weight the union average carries, and the
    // base_count any further fold-in has to re-weight against.
    int complementarity_pair_count(int slack_count) const;
    // Folds an active nested restoration phase's elastic complementarity pairs
    // into complementarity()'s aggregates. base_count is the number of original
    // pairs already reduced into avgcomp. A pure no-op off that path. Only ever
    // combines separately-computed aggregates, so complementarity()'s reduction
    // ordering is preserved.
    void augment_complementarity_nested(double &avgcomp, double &mincomp, double &maxcomp,
                                        int base_count) const;
    void barrier_hessian(Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                         Eigen::Ref<Eigen::VectorXd> S, Eigen::Ref<Eigen::VectorXd> LI, double mu);
    // --- Native variable-bound helpers (defined in interior_point_solver.cpp) ---
    // Every one of these is a no-op when bounds_ is null.

    // Projects `x` into the strict interior of the recorded bounds and seeds the
    // bound multipliers there. Per bounded variable the push away from a bound is
    // bound_push_ * max(1, |bound|), capped at bound_interval_push_ * (upper -
    // lower) when two-sided; the lower push runs first, and below one half the two
    // can never cross. A guess at or outside a bound is projected, never rejected.
    // The multipliers are then seeded at min(kBoundMultInitCap, mu0 / distance).
    // Runs once per solve, at entry, after the reduced gather and before any
    // evaluation.
    void push_initial_point_interior(EigenRef<Eigen::VectorXd> x, double mu0);

    // Bound-multiplier Newton step from the UNSCALED primal step `dx`:
    // dz_L = mu*(X-L)^-1 e - z_L - Sigma_L*dx and, with the sign the upper-bound
    // row carries, dz_U = mu*(U-X)^-1 e - z_U + Sigma_U*dx. Must be called on the
    // raw KKT solution, before the fraction-to-boundary rule scales it.
    void compute_bound_dual_direction(ConstEigenRef<Eigen::VectorXd> x,
                                      ConstEigenRef<Eigen::VectorXd> dx, double mu);

    // Commits the bound multipliers for an accepted iterate: z += alphad*dz, then
    // the kappa_sigma safeguard clamps each into [mu_clip/(kKappaSigma*d),
    // kKappaSigma*mu_clip/d] for the distance d at the NEW x. Exactly one call per
    // committed iterate; `xsl_new` is the already-committed iterate.
    //
    // `monotone_mu` selects which barrier parameter the clamp is taken at: under a
    // MONOTONE schedule the parameter itself, under a FREE-mu schedule the average
    // complementarity at the new point capped at kFreeModeClipMuCap -- a free-mode
    // parameter is a proposal for the NEXT step, and the safeguard needs a
    // description of where the iterate sits.
    void apply_bound_dual_step(double alphad, KKTVector &xsl_new, double mu, bool monotone_mu);

    // Accumulates the condensed bound curvature onto a primal-diagonal base
    // vector. The two callers that already have a base vector in hand call this
    // directly; install_primal_diags_with_sigma() is the scalar-base form, which
    // collapses to a plain set_primal_diags(base) when there are no bounds.
    void add_bound_sigma(ConstEigenRef<Eigen::VectorXd> x, EigenRef<Eigen::VectorXd> diag) const;
    void install_primal_diags_with_sigma(ConstEigenRef<Eigen::VectorXd> x, double base);

    // The dual infeasibility whose infinity norm is the solver's kkt_inf_. Off the
    // bound path this is the base block's norm; with bounds it is that block plus
    // the z-FORM terms (-z_L + z_U), built in scratch. Deliberately NOT accumulated
    // into the RHS, whose primal block carries the mu-form instead.
    //
    // `prim_base` is the BASE-form primal stationarity block for the point being
    // measured, passed explicitly because the live RHS's block is staged in the
    // mu-form for part of each iteration.
    double dual_infeasibility_inf(ConstEigenRef<Eigen::VectorXd> prim_base) const;

    // The barrier-parameter update runs through governor_->update_barrier().
    // complementarity() STAYS here — it is still called from the evaluate
    // stage (its maxcomp output feeds converge_check's barr_inf_).

    // --- NLP eval dispatch methods (defined in interior_point_solver.cpp) ---
    // The four wrappers below differ only in which evaluation request they name.
    // They reach the NLP through the aggregate contract's assemble() entry; the
    // request constants pair with the evaluation shapes documented in the
    // mapping table of model/candidate_point.h.

    /// @brief Evaluate the NLP at the current iterate through the aggregate
    ///        contract, then scatter the solver's own KKT coefficients.
    ///
    /// Builds the candidate point and the scatter views from the compound
    /// [primals | slacks | eq | iq] layout, calls NlpAggregate::assemble(), and
    /// — for a request naming KKT-bearing output — follows it with
    /// NonLinearProgram::fill_solver_coeffs(). The slack Jacobian, the primal
    /// and slack diagonals and the constraint-row pivots are consumer-owned
    /// coefficients that assemble() never writes. The two steps are sequenced
    /// rather than overlapped because they share destinations; see the body.
    /// Callers set those coefficients before this call and reset them after, as
    /// they always did.
    /// @param request    One of the eight request shapes of the mapping table.
    /// @param obj_scale  Objective scale factor applied by the objective pieces.
    /// @param XSL        Current iterate, [primals | slacks | eq | iq].
    /// @param val        Objective accumulator; written only if the request
    ///                   names the objective value.
    /// @param GX         Objective-gradient destination; its first
    ///                   primal_vars_ rows are the arena.
    /// @param AGXS_FX    Adjoint-gradient and residual destination, in the same
    ///                   compound layout as @p XSL.
    /// @param KKTmat     Assembly buffer; must be the matrix the current
    ///                   sparsity analysis was run against.
    void assemble_dispatch(EvalRequest request, double obj_scale, ConstEigenRef<VectorXd> XSL,
                           double &val, EigenRef<VectorXd> GX, EigenRef<VectorXd> AGXS_FX,
                           Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    /// @brief Accumulate the objective value at a point, with no derivative or
    ///        constraint work.
    ///
    /// Issues the objective-value request of the mapping table. The caller owns
    /// the accumulator and zeroes it.
    /// @param obj_scale  Objective scale factor.
    /// @param primals    Primal block in the solver's own space.
    /// @param val        Accumulator the objective value is added into.
    void assemble_objective(double obj_scale, ConstEigenRef<VectorXd> primals, double &val);

    void eval_kkt(double obj_scale, ConstEigenRef<VectorXd> XSL, double &val, EigenRef<VectorXd> GX,
                  EigenRef<VectorXd> AGXS_FX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void eval_kkt_no(double obj_scale, ConstEigenRef<VectorXd> XSL, double &val,
                     EigenRef<VectorXd> GX, EigenRef<VectorXd> AGXS_FX,
                     Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void eval_aug(double obj_scale, ConstEigenRef<VectorXd> XSL, double &val, EigenRef<VectorXd> GX,
                  EigenRef<VectorXd> AGXS_FX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);
    void eval_soe(double obj_scale, ConstEigenRef<VectorXd> XSL, double &val, EigenRef<VectorXd> GX,
                  EigenRef<VectorXd> AGXS_FX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat);

    // `mu` is the live phase barrier parameter. It is consulted only by the
    // nested feasibility-restoration branch (which recomputes its proximity
    // weight, pivots, and condensed residuals from the live μ every evaluation);
    // every other mode ignores it, so the default and proximal-switch paths are
    // unaffected by its value.
    void eval_nlp(AlgorithmModes algmode, double obj_scale, ConstEigenRef<VectorXd> XSL,
                  double &val, EigenRef<VectorXd> GX, EigenRef<VectorXd> AGXS_FX,
                  Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat, double mu);

    // --- Feasibility-restoration exit measures (defined in interior_point_solver.cpp) ---
    // Shared by every restoration exit and teardown site. While restoration is
    // active the loop's own prim_obj_ is the restoration objective, which is not
    // comparable with the optimality filter/funnel's accumulated pairs; this
    // helper re-evaluates the TRUE objective once at the live primals so every
    // exit site hands notify_switch_to_optimality a measures triple in the scale
    // of the filter or funnel it is augmenting into.
    ProgressMeasures build_restoration_exit_measures(double obj_scale, double infeasibility,
                                                     ConstEigenRef<VectorXd> primals,
                                                     double barr_obj);

    // --- Feasibility-restoration lifecycle (defined in interior_point_solver.cpp) ---
    // Shared entry orchestration for the kSwitchToFeasibility case: build the
    // (theta, f) entry measures, then dispatch on the strategy family. The
    // proximal switch takes enter_restoration; the nested l1 phase takes
    // enter_nested, and additionally stashes the outer mu, sets mu <- entry_mu(),
    // resets the governor and applies the entry multiplier init (equality
    // multipliers <- 0, slack and bound multipliers clamped to min(rho, current)).
    // Both then notify the acceptance strategy and reset the recovery chain.
    // Passed the raw XSL/RHS blocks so it is drivable from a test harness. RHS is
    // READ ONLY; `mu` is updated in place.
    void enter_feasibility_restoration(Eigen::VectorXd &XSL, const Eigen::VectorXd &RHS,
                                       double prim_obj, double barr_obj, double &mu);

    // The restoration-entry dispatch, in the ONE order every entry site uses:
    // record `theta` as the stall detector's handback yardstick, enter
    // restoration, then re-arm the stall window. Ordering matters because
    // enter_feasibility_restoration mutates XSL (the entry multiplier init) and
    // `mu`; RHS is const, so this order cannot be reasoned about through it.
    // `theta` stays a parameter: each site already has the constraint violation
    // it needs in hand, and computing it here instead would add a reduction at
    // two of them.
    void dispatch_restoration_entry(Eigen::VectorXd &XSL, const Eigen::VectorXd &RHS,
                                    double prim_obj, double barr_obj, double &mu, double theta,
                                    FeasibilityStallDetector &feas_stall);

    // The restoration EXIT protocol, in the one order every exit site must use:
    // optionally restore the stashed outer mu and reset the governor (only a
    // nested phase needs it), exit_restoration(), notify the acceptance strategy
    // of the switch back to optimality, reset the recovery chain.
    //
    // The order is load-bearing. exit_restoration() flips is_active() false, so
    // any mu or governor work belonging to the phase must precede it; `measures`
    // must come from build_restoration_exit_measures(); and the recovery-chain
    // reset runs last and exactly once per transition.
    void leave_restoration(const ProgressMeasures &measures, bool restore_stashed_mu, double &mu);

    // The nested phase's multiplier re-entry sequence, shared by the kappa_resto
    // ratchet exit and the near-feasible stall exit, in strict order: (1) keep the
    // phase's final x/s; (2) slack-multiplier Newton complementarity step under
    // the STASHED outer mu, damped by the dual fraction-to-boundary rule; (3) if
    // max|z| over ALL inequality multipliers exceeds kBoundMultResetThreshold,
    // reset every inequality multiplier to 1; (4) equality multipliers <- 0; (5)
    // restore the stashed mu, reset the governor, exit_restoration, notify the
    // acceptance strategy with true-objective exit measures, reset the recovery
    // chain. `theta_orig` is carried into the exit measures; `mu` is restored in
    // place.
    //
    // SCOPE: steps (2) and (3) reach EVERY bound-multiplier family -- the
    // inequality (slack) multipliers and, when the problem declares variable
    // bounds, both sides of those -- under ONE shared dual fraction-to-boundary
    // damping and one reset-threshold max over all of them.
    void exit_feasibility_restoration_nested(Eigen::VectorXd &XSL, double obj_scale,
                                             double theta_orig, double barr_obj, double &mu);

    // Per-iteration κ_resto ratchet test for the nested phase: the current
    // original-problem infeasibility must fall to at most max(kKappaResto ·
    // previous-iteration infeasibility, econ_tol_) (Ipopt RestoConvCheck's
    // orig_inf_pr_max, single-tolerance floor). Reads resto_theta_orig_prev_
    // (seeded at entry, ratcheted each phase iteration). Defined in interior_point_solver.cpp so
    // the kKappaResto constant (globalization/acceptance_strategy.h) stays out of
    // this header's include set.
    bool resto_ratchet_passes(double theta_orig) const;

    // L1 norm over a KKT vector's constraint block -- the constraint violation the
    // restoration entry guards, the proximal exit test and the stall detector all
    // measure. One home for the reduction. Takes the READ-ONLY view: it reduces
    // and never writes, and a KKTVector converts implicitly.
    double constraint_violation_l1(const ConstKKTVector &v) const;

    // Original-problem infeasibility (∞-norm) for an active NESTED restoration
    // phase, taken from the raw equality/inequality residuals the eval seam saves
    // each active iteration (the RHS constraint rows carry the condensed r̃ by
    // then, so they are not a valid source). Two separate Eigen reductions,
    // deliberately not fused. Meaningless off the nested path — every caller is
    // inside a nested-active branch.
    double original_infeasibility_inf() const;

    // Second-level elastic re-centering fallback for the nested l1 phase. Invoked
    // from alg_impl's kAcceptAsIs case when an in-phase line search exhausts the
    // recovery ladder: re-centers the elastic pairs in closed form at the current
    // phase mu from the raw residuals in resto_ec_/ic_scratch_, INSTEAD of taking
    // the failed step. One-shot per consecutive-failure run -- true and consumes
    // the budget on the first call, false while the flag is still set. Re-armed on
    // any accepted step and at each phase entry.
    bool try_recenter_elastics(double mu);

    // Primal-dual system error at barrier parameter `mu`: the infinity norm of the
    // full KKT residual -- primal stationarity, primal infeasibility (equality and
    // slack-completed inequality), and the complementarity deviation max|s*z - mu|
    // -- as one scalar. Read-only; the caller passes vectors populated the way the
    // main loop populates the current iterate's RHS. Used only by the nested soft
    // feasibility pre-stage.
    //
    // The stationarity term is the z-FORM dual infeasibility, and `prim_base`
    // carries the measured point's BASE primal block for the same reason
    // dual_infeasibility_inf takes one: the comparison this feeds comes from two
    // points, and both must be measured in the same form.
    double primal_dual_error(KKTVector &xsl, KKTVector &rhs,
                             ConstEigenRef<Eigen::VectorXd> prim_base, double mu) const;

    // Nested soft feasibility pre-stage trial. Forms the full
    // fraction-to-boundary trial point XSL + DXSL, evaluates the original problem
    // there into the caller-supplied scratch, and returns whether its primal-dual
    // error is at most kSoftRestoPdErrorReductionFactor times the current point's.
    // True means the soft step is accepted and alg_impl stays in the pre-stage;
    // false means it escalates to the full restoration switch.
    bool try_soft_feasibility_step(AlgorithmModes algmode, double obj_scale, double mu,
                                   Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL,
                                   Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                                   Eigen::VectorXd &RHS2, Eigen::VectorXd &GX);

    // --- Convergence and stepping ---
    // The residual formulas shared by the pre-factorization early convergence
    // check and the post-line-search fill_iter_info() call live here ONCE, so
    // neither site can drift. fill_residual_info() sets every IterateInfo field
    // derivable from rhs/xsl alone, and deliberately NOT barr_obj_/mu_ (settled
    // only once the barrier update runs) or p_pivots_ (only real once this
    // iteration's factorization has run).
    void fill_residual_info(KKTVector &xsl, KKTVector &rhs, double pobj, IterateInfo &iter) const;
    void fill_iter_info(KKTVector &xsl, KKTVector &rhs, double pobj, double bobj, double mu,
                        IterateInfo &iter) const;
    SolveStatus converge_check(std::vector<IterateInfo> &iters);

    /// @brief Builds ONE IterationEvent from the live iterate and hands it to
    ///        the installed per-iteration callback (M6 W5 T8.6).
    ///
    /// EVERY FIELD IS BUILT IN DECLARED SPACE AND CALLER UNITS, by the same
    /// arithmetic the declared-space seam in run_phase_sequence applies to the
    /// result: the reduced primal space is scattered back, the internal fixing
    /// rows come off the equality block and reappear in the bound price as
    /// `z = -lambda_fix`, the objective scale this call runs at is divided out,
    /// and the four shared diagnostics come from compute_declared_diagnostics_
    /// from_grad_lag through the `grad_lag` door -- no evaluation, so an armed
    /// callback moves no evaluation counter.
    ///
    /// THE PROVENANCE RULE IS THE RESULT'S (T8.4 fix1): an event taken while
    /// feasibility restoration is active reports all four diagnostics NaN with
    /// the two constraint blocks EMPTY, and an event from a feasibility phase
    /// (SOE / OPTNO) reports `stationarity` NaN alone -- exactly what the
    /// result would report at that point.
    ///
    /// The four vector views the callback sees alias LOCALS of this function,
    /// which is what makes "valid for the call only" true by construction.
    ///
    /// A NO-OP when no callback is installed, and it builds nothing then.
    ///
    /// @param iter       The iterate record whose residual half has been filled.
    /// @param XSL        The live compound iterate.
    /// @param RHS        The live right-hand side, at that iterate.
    /// @param mu         The barrier parameter this iterate was evaluated under,
    ///                   at the SOLVER's objective scale (divided out here).
    /// @param step_norm  Inf-norm of the primal block of the committed
    ///                   alpha * DXSL that reached this iterate; 0 at the first
    ///                   iterate of a phase.
    void fire_iteration_event(const IterateInfo &iter, const Eigen::VectorXd &XSL,
                              const Eigen::VectorXd &RHS, double mu, double step_norm,
                              double prim_obj);

    /// @brief The four shared diagnostics, from a REDUCED-space Lagrangian
    ///        gradient -- the one arithmetic the result and the event share.
    ///
    /// FACTORED OUT AT M6 W5 T8.6 fix1 (astra item 2). T8.4's declared-space
    /// seam in run_phase_sequence and T8.6's event builder had two copies of
    /// this conversion; a caller comparing an event against the result it
    /// eventually gets must not find two answers to one question, and two
    /// copies drift. The result path's numbers are unchanged by the move --
    /// bitwise, which the interior leg's 41 rows and U0's `ipm` arm prove.
    ///
    /// @param x_declared  The point, DECLARED width.
    /// @param lambda_i    Inequality prices, declared rows, caller scale.
    /// @param z_declared  Bound prices, declared width, the fixing rows'
    ///                    -lambda_fix already folded in.
    /// @param grad_lag_reduced  obj_scale * (grad f + J'lambda) in the SOLVER's
    ///                    reduced primal space, `primal_vars_` long.
    /// @param fixed_lambda  The internal fixing rows' multipliers at CALLER
    ///                    scale, or an empty vector when there are none.
    /// @param ce_declared The declared equality residuals, or empty.
    /// @param ci_declared The declared inequality residuals, or empty.
    /// @param scale       The objective scale this call ran at.
    /// @param objective_bearing  Provenance clause (a): false when the
    ///                    evaluation carried no declared objective gradient, in
    ///                    which case STATIONARITY ALONE comes back NaN.
    /// @return The four diagnostics.
    DeclaredDiagnostics declared_diagnostics_from_reduced_grad_lag(
        const Eigen::VectorXd &x_declared, const Eigen::VectorXd &lambda_i,
        const Eigen::VectorXd &z_declared, const Eigen::VectorXd &grad_lag_reduced,
        const Eigen::VectorXd &fixed_lambda, const Eigen::VectorXd &ce_declared,
        const Eigen::VectorXd &ci_declared, double scale, bool objective_bearing) const;

    /// @brief Applies a set/clear of the iteration callback deferred while it
    ///        was on the stack; see set_iteration_callback().
    /// Applies a parked callback if this is the safe point it belongs to.
    /// @param at_solve_entry True at run_phase_sequence's entry, which applies
    ///        every parked value; false after a callback invocation returns,
    ///        which applies only what that invocation itself parked.
    void apply_pending_iteration_callback(bool at_solve_entry = false);
    /// @brief Applies a set/clear of the KKT hook deferred while it was on the
    ///        stack; see set_kkt_hook().
    /// @see apply_pending_iteration_callback().
    void apply_pending_kkt_hook(bool at_solve_entry = false);

    /// Emits one `ipm.message` if a sink is attached (M6 W5 T8.7b).
    ///
    /// THE MEMBER IS READ INTO A LOCAL ONCE, as at every other emit site since
    /// M6 W5 T8.7 fix1: nothing a sink does between the check and the call can
    /// change which sink this event reaches. Each caller fills the slots its
    /// KIND uses and leaves the rest at their absence sentinels.
    void emit_message(const IpmMessageTraceEvent &event);

    // Best-iterate bookkeeping for the return_best_ path (off by default). Scores
    // `iter` under best_criteria_ and, when it ties or beats the incumbent (or is
    // the phase's first iterate), snapshots XSL/RHS into best_xsl_scratch_/
    // best_rhs_scratch_, snapshots bound_duals_ into best_bound_duals_scratch_
    // (the z pair has no XSL/RHS-carried counterpart), and records the
    // criterion value and iteration index. The return_best_ / restoration-
    // active guard stays at the call sites, which differ in why they are
    // reached; only the scoring and snapshot live here.
    void track_best_iterate(const IterateInfo &iter, int i, const VectorXd &XSL,
                            const VectorXd &RHS, double &BestCriteriaVal, int &BestIter);

    /// The next never-reused analysis-owner id. Monotonic and process-wide;
    /// see analyzed_owner_id_.
    static std::uint64_t next_owner_id();

    /// THE ONE LEDGER WRITE (M6 W5 T8.7), called from both public solve()
    /// overloads after the entry clock has stopped and `wall_seconds` has been
    /// stamped -- the SQP driver's `record_solve` funnel in shape. Does nothing
    /// when no ledger is attached; a solve that leaves by an exception never
    /// reaches it, so it neither records nor consumes a label number.
    ///
    /// @param result                 The result this call is about to return.
    /// @param factorize_count_at_entry `lifetime_factorize_count()` read at the
    ///        top of this call; the record's `factorizations` is the
    ///        difference, so a reused solver does not charge this record for a
    ///        previous call's work. That accessor and not
    ///        `kkt_sol_.counters().factorize_count` (M6 W5 T8.7 fix1): the
    ///        latter counts per ENGINE INSTANCE and restarts at zero when
    ///        set_qp_params() replaces the engine, which a call on a DIFFERENT
    ///        program does -- and a difference taken across such a call is
    ///        negative.
    void record_solve(const IpmResult &result, Index factorize_count_at_entry);

    // --- Printing methods ---
    //
    // NONE LEFT (M6 W5 T8.7b). `print_beginning`, `print_finished` and
    // `print_exit_stats` were the last three, and
    // `src/drivers/interior_point_solver_print.cpp` -- the file that held them
    // -- is deleted. What they wrote is written by `ConsoleTraceSink` from
    // `ipm.phase.begin` / `ipm.phase.end` / `ipm.kkt_analysis` /
    // `ipm.phase.exit`. There is no `fmt::print` anywhere in this engine's
    // solve path.
};

} // namespace hven::solvers
