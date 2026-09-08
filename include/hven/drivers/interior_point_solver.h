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
#include <functional>
#include <initializer_list>
#include <memory>
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
#include "hven/drivers/solve_status.h"
#include "hven/model/non_linear_program.h"
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

    /// @brief Accumulated outputs of the most recent solve/optimize call.
    ///
    /// Reset per call by reset_accumulators(); the timing and iteration
    /// counters accumulate across the phases of one call.
    struct SolveResult {
        // --- Solve outcome ---
        /// @brief Iterations taken by the most recent call.
        int iter_num_ = 0;
        /// @brief Objective value at the returned point, on the CALLER's scale:
        ///        f(x), never Settings::obj_scale_ * f(x).
        ///
        /// The scale is divided back out once, at the end of the call, from this
        /// field and the multiplier blocks below; the scale divided out is the one
        /// the call RAN at, captured at its entry. A call that threw part-way
        /// through leaves the SCALED value standing -- reading a result after a
        /// throw was never contractual.
        double obj_val_ = 0;
        /// @brief Convergence verdict of the most recent call.
        ConvergenceFlags converge_flag_ = ConvergenceFlags::NOTCONVERGED;

        // --- Solution ---
        /// @brief Returned primal variables, in the caller's (full) space.
        Eigen::VectorXd primals_;

        /// @brief Which Settings::fixed_variable_treatment_ this call actually
        ///        ran under.
        ///
        /// Always equals the Settings field's value at the time the call ran --
        /// configure_variable_treatment runs the requested treatment or throws,
        /// never substitutes. Overwritten unconditionally at the start of every
        /// call. Recorded here because it decides eq_lmults_'s shape.
        FixedVariableTreatments fixed_variable_treatment_ = FixedVariableTreatments::MakeParameter;

        // --- Multipliers and constraints ---
        /// @brief Equality-constraint multipliers at the returned point, on the
        ///        CALLER's scale, against L = f + lambda_e^T cE + lambda_i^T cI - z.
        ///
        /// Sized equal_cons_: the declared equality rows, plus -- only under
        /// fixed_variable_treatment_ == MakeConstraint -- one internal fixing row
        /// per bound-fixed variable, appended after the declared rows. eq_cons_
        /// below shares that shape.
        Eigen::VectorXd eq_lmults_;
        /// @brief Inequality-constraint multipliers at the returned point, on
        ///        the caller's scale (see eq_lmults_).
        Eigen::VectorXd iq_lmults_;
        /// @brief Equality-constraint residuals at the returned point.
        Eigen::VectorXd eq_cons_;
        /// @brief Inequality-constraint residuals at the returned point.
        Eigen::VectorXd iq_cons_;
        /// @brief Variable-bound multipliers at the returned point, on the
        ///        caller's scale: z = z_lower - z_upper, so a component is >= 0 at
        ///        an active lower bound, <= 0 at an active upper bound, 0 when free.
        ///
        /// Dense over the SOLVER's reduced primal space (size primal_vars_) --
        /// unlike primals_, NOT expanded to the caller's full space, so an
        /// eliminated variable has no entry here. Empty when the problem has no
        /// finite variable bounds, and reset everywhere bounds_ goes null.
        /// Included in the return_best_ snapshot, so a non-converged return_best_
        /// exit reports this from the same iterate as the rest of SolveResult.
        Eigen::VectorXd bound_lmults_;

        // --- Terminal KKT residuals ---
        //
        // The four scalars converge_check() gates on, with IterateInfo's
        // definitions, so each is directly comparable against its matching
        // Settings tolerance. kkt_inf_ and barr_inf_ carry Settings::obj_scale_;
        // econ_inf_ and icon_inf_ carry no scale, and nothing here is unscaled on
        // the way out. On a restoration-active exit they describe the restoration
        // subproblem, not the NLP. Last phase wins; reset_accumulators() sets them
        // to NaN, and NaN means UNMEASURED, never "zero residual".
        /// @brief inf-norm dual infeasibility (z-form) at the reported iterate.
        double kkt_inf_ = std::numeric_limits<double>::quiet_NaN();
        /// @brief Complementarity (barrier) error at the reported iterate.
        double barr_inf_ = std::numeric_limits<double>::quiet_NaN();
        /// @brief inf-norm equality-constraint residual; 0 when equal_cons_ == 0.
        double econ_inf_ = std::numeric_limits<double>::quiet_NaN();
        /// @brief inf-norm inequality-constraint residual; 0 when inequal_cons_ == 0.
        double icon_inf_ = std::numeric_limits<double>::quiet_NaN();

        // --- Timing (seconds) ---
        /// @brief Total wall-clock time of the most recent call.
        ///
        /// INFORMATIONAL, NEVER ASSERTED -- counters are this project's currency
        /// of correctness, not timings. Measured on std::chrono::steady_clock.
        double total_time_ = 0;
        /// @brief Setup/preprocessing time.
        double pre_time_ = 0;
        /// @brief NLP evaluation callback time.
        double func_time_ = 0;
        /// @brief KKT assembly/factorization/solve time.
        double kkt_time_ = 0;
        /// @brief Printing time.
        double print_time_ = 0;
        /// @brief Solver-initialization time (measured before the main timer starts).
        double solver_init_time_ = 0;

        /// Derived timing — total wall-clock minus all categorized components.
        /// Excludes solver_init_time_ (measured before the main timer starts).
        /// Captures: callback time, step application, convergence checks, etc.
        double misc_time() const {
            return total_time_ - pre_time_ - kkt_time_ - func_time_ - print_time_;
        }

        // --- Factorization stats ---
        /// @brief Memory reported by the last factorization.
        int factor_mem_ = 0;
        /// @brief Flops reported by the last factorization.
        int factor_flops_ = 0;

        /// Number of second-order correction back-substitutions performed
        /// across the whole solve (one per correction attempt; each costs a
        /// single constraint evaluation + one back-substitution on the live
        /// factorization). Always 0 when SOC is off (max_soc_ == 0). Reset per
        /// solve alongside the other accumulators.
        int soc_steps_taken_ = 0;

        /// Number of times the watchdog armed across the whole solve
        /// (Chamberlain, Powell, Lemaréchal & Pedersen 1982; constants per
        /// Wächter & Biegler 2006's implementation, globalization/watchdog.h).
        /// Always 0 when the watchdog is off (watchdog_ == false). Reset per
        /// solve alongside the other accumulators.
        int watchdog_activations_ = 0;

        /// Per-rejection recovery-chain outcome depth, indexed by the
        /// kRecoveryDepth* constants in globalization/recovery_chain.h: [0] SOC,
        /// [1] extended backtracking, [2] watchdog, [3] unresolved (the only
        /// bucket that increments when all three are off), [4] restoration
        /// (increments only when restoration_mode_ != off). Counts REJECTIONS,
        /// not interventions. Reset per solve.
        std::array<int, 5> recovery_depth_histogram_{};

        /// Final funnel width (tau) reported by FunnelAcceptance at the end of
        /// the most recent solve's LAST PHASE. Sentinel -1.0 when the selected
        /// acceptance strategy does not report it, and when no acceptance test ran
        /// in that phase. A multi-phase call reports only the last phase's value.
        /// Reset per solve, not by the per-phase AcceptanceStrategy::reset().
        double last_funnel_width_ = -1.0;

        /// Final filter size (number of stored (θ, φ) pairs, Filter::size())
        /// reported by FilterAcceptance::append_diagnostics()
        /// (globalization/filter_acceptance.h) at the end of the most recent
        /// solve's LAST PHASE. Sentinel -1 when the selected acceptance
        /// strategy is not filter. Same last-phase-only semantics as
        /// last_funnel_width_ above.
        int last_filter_size_ = -1;

        /// Total filter-reset-heuristic clears (FilterAcceptance::filter_resets())
        /// at the end of the most recent solve's LAST PHASE. Sentinel -1 when the
        /// acceptance strategy is not filter. PER-PHASE, and under
        /// barrier_governor_ == monitored per barrier SUBPROBLEM: each mu-event
        /// also clears the counter, so this reports resets since the last mu-event
        /// of the last phase.
        int last_filter_resets_ = -1;

        /// Number of free -> monotone handoffs during the most recent solve's
        /// LAST PHASE, reported by MonitoredBarrierGovernor. Sentinel -1 when the
        /// selected barrier_governor_ is not monitored. Per-phase, like
        /// last_filter_resets_ above.
        int last_monotone_switches_ = -1;

        /// Number of iterations spent in monotone mode during the most recent
        /// solve's LAST PHASE, reported by MonitoredBarrierGovernor::
        /// append_diagnostics(). Sentinel -1 when the selected
        /// barrier_governor_ is not monitored. Same per-phase semantics as
        /// last_monotone_switches_.
        int last_monotone_iters_ = -1;

        /// Number of times feasibility restoration was entered during the most
        /// recent solve's LAST PHASE, reported by RestorationStrategy. WRITE-ONLY
        /// diagnostics: no algorithm code reads it back. Sentinel -1 when no
        /// restoration strategy is constructed. Counting is identical across both
        /// modes -- entries_ increments once per entry call, iterations_in_mode_
        /// once per note_iteration() while active.
        int last_feas_rest_entries_ = -1;

        /// Number of iterations spent in restoration mode during the most
        /// recent solve's LAST PHASE, reported by RestorationStrategy::
        /// append_diagnostics(). WRITE-ONLY diagnostics field. Sentinel -1
        /// when no restoration strategy is constructed. Same per-phase
        /// semantics as last_feas_rest_entries_ (including the nested-mode
        /// counting note above).
        int last_feas_rest_iters_ = -1;

        /// Proximal primal-dual regularization shifts applied at the LAST
        /// FACTORIZED ITERATION of the most recent solve's last phase.
        /// last_prox_reg_primal_ is the persistent primal base shift rho_k added
        /// to the Hessian diagonal there; last_prox_reg_dual_ is the
        /// barrier-scaled dual shift delta_c subtracted from the constraint-row
        /// diagonals (0.0 when suppressed inside a nested l1 phase). Sentinel -1.0
        /// for BOTH when inertia_mode_ != proximal_regularization, and when a
        /// mode-on phase converged before its first factorization.
        double last_prox_reg_primal_ = -1.0;
        /// The dual half of the pair documented just above: the barrier-scaled
        /// dual shift δ_c, on the same sentinel and last-phase-wins rules.
        double last_prox_reg_dual_ = -1.0;

        /// Message of the most recent trial-evaluation exception absorbed by
        /// the acceptance machinery during the most recent solve call (all
        /// phases). Empty when every evaluation succeeded. A populated value
        /// means the solver rejected un-evaluable trial steps and continued —
        /// to full recovery, to a graceful ACCEPTABLE-level exit at an
        /// already-acceptable iterate, or into feasibility restoration. When
        /// none of those paths existed, the solve threw the latched message
        /// wrapped in solver context instead.
        std::string last_eval_exception_;

        /// The last non-Success status observed from kkt_sol_.info() by
        /// factor_impl() within the CURRENT phase (alg_impl resets it on
        /// entry, so print_exit_stats reports per-phase status). Purely
        /// observational (surfaced by print_exit_stats()); feeds no
        /// control-flow decision in factor_impl.
        Eigen::ComputationInfo last_kkt_info_ = Eigen::Success;

        /// @brief Resets the accumulated timing/iteration counters, the
        ///        convergence flag, last_kkt_info_, the four terminal KKT
        ///        residuals (to NaN), the SOC/watchdog/recovery counters and
        ///        every last_* diagnostic.
        ///
        /// primals_,
        /// obj_val_ and fixed_variable_treatment_ are overwritten unconditionally
        /// per phase or per call instead; the four constraint-indexed blocks are
        /// emptied at solve entry and then written only when the current problem
        /// has rows for them; bound_lmults_ is overwritten when the solve has
        /// finite variable bounds. factor_mem_/factor_flops_ hold the LAST
        /// factorization's stats and are not accumulated across phases.
        void reset_accumulators() {
            converge_flag_ = ConvergenceFlags::NOTCONVERGED;
            total_time_ = 0;
            pre_time_ = 0;
            func_time_ = 0;
            kkt_time_ = 0;
            print_time_ = 0;
            solver_init_time_ = 0;
            iter_num_ = 0;
            kkt_inf_ = std::numeric_limits<double>::quiet_NaN();
            barr_inf_ = std::numeric_limits<double>::quiet_NaN();
            econ_inf_ = std::numeric_limits<double>::quiet_NaN();
            icon_inf_ = std::numeric_limits<double>::quiet_NaN();
            last_kkt_info_ = Eigen::Success;
            soc_steps_taken_ = 0;
            watchdog_activations_ = 0;
            recovery_depth_histogram_.fill(0);
            last_funnel_width_ = -1.0;
            last_filter_size_ = -1;
            last_filter_resets_ = -1;
            last_monotone_switches_ = -1;
            last_monotone_iters_ = -1;
            last_feas_rest_entries_ = -1;
            last_feas_rest_iters_ = -1;
            last_prox_reg_primal_ = -1.0;
            last_prox_reg_dual_ = -1.0;
            last_eval_exception_.clear();
        }
    };

    /// @brief Shorthand for Eigen::VectorXd, used by this class's callback
    ///        signatures and entry points.
    using VectorXd = Eigen::VectorXd;

    /// @brief Type of the per-iteration early callback.
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
    using EarlyCallBackType =
        std::function<int(int, double, ConstEigenRef<VectorXd>, double, ConstEigenRef<VectorXd>,
                          ConstEigenRef<VectorXd>, Eigen::SparseMatrix<double, Eigen::RowMajor> &)>;

    /// Type of the per-iteration late callback (same variable-space caveat --
    /// see EarlyCallBackType's note).
    using LateCallBackType =
        std::function<int(const IterateInfo &, ConstEigenRef<VectorXd>, ConstEigenRef<VectorXd>)>;

    // --- Constructors / destructor ---
    // All three are defined out-of-line in interior_point_solver.cpp: the
    // unique_ptr members with incomplete element types force even the
    // constructors' exception-cleanup paths to see the complete types.

    /// @brief Constructs a solver over `opts` with no program attached;
    ///        set_nlp() must run before any entry point.
    /// @param opts The options; validated here, exactly as set_options() would.
    /// @throws std::invalid_argument if validate(opts) rejects the value.
    explicit InteriorPointSolver(IpmOptions opts = {});
    /// @brief Constructs a solver over `np` and runs QP parameter setup, as
    ///        set_nlp() does.
    /// @param np The program to solve.
    InteriorPointSolver(std::shared_ptr<NonLinearProgram> np);
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
    /// TRANSCRIPTION-TIME FIELDS. Most fields are read inside the solve, so a
    /// replacement between two solves simply takes effect on the next one. Two
    /// are not: fixed_variable_treatment and bound_relax_factor are consumed
    /// when the program is TRANSCRIBED (set_nlp() -> configure_variable_treatment(),
    /// which decides the solver's variable space), and cnr_mode is consumed at
    /// set_qp_params() time. Changing any of the three after the program has
    /// been attached is REFUSED by name rather than silently ignored: re-attach
    /// the program (set_nlp()) after the replacement, or build the solver with
    /// the options you want. The refusal is the ONE rule -- this method never
    /// silently re-transcribes behind the caller.
    ///
    /// @param o The replacement options.
    /// @throws std::invalid_argument if validate(o) rejects the value, or if a
    ///         transcription-time field differs from the one in force while a
    ///         program is attached.
    /// @throws std::logic_error if a solve is in flight (a replacement from
    ///         inside a callback).
    void set_options(IpmOptions o);
    /// @brief Returns the accumulated outputs of the most recent solve/optimize call.
    const SolveResult &result() const { return result_; }

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
    /// What it does NOT claim is agreement with result().converge_flag_. That
    /// field's lifetime is the CALL, not the phase: a multi-phase call whose
    /// later phase leaves without assigning it reports the EARLIER phase's
    /// verdict, and this reason then describes the later phase correctly beside
    /// a stale verdict. That verdict lifetime is a pre-existing engine property,
    /// registered for T8.4's per-phase results; to_solve_status() reads the
    /// verdict first, so the pairing it produces is only as good as the verdict.
    ///
    /// @return The stop reason recorded by the last phase that ran.
    IpmStopReason last_stop_reason() const noexcept { return last_stop_reason_; }
    /// @brief Returns the log of absorbed NLP evaluation errors.
    const EvalErrorLog &eval_error_log() const { return eval_error_log_; }

    /// @brief How many times this solver has laid and analyzed the KKT sparsity
    ///        pattern, over this object's LIFETIME.
    ///
    /// Not a SolveResult field, which is reset per call: this answers a
    /// cross-call question. Moves once per set_nlp() and once per solve entry
    /// that finds the structures re-laid since the last analysis; release()
    /// returns it to zero.
    ///
    /// @return The lifetime analysis count.
    Index kkt_analysis_count() const noexcept { return kkt_analysis_count_; }

    /// @brief The KKT factor's linear-layer call counters, including the
    ///        pattern-guard count that shows how many factorizations
    ///        re-verified the sparsity pattern.
    const KktFactorization::Counters &kkt_factor_counters() const { return kkt_sol_.counters(); }

    /// @brief True when the assembly buffer's sparsity pattern is the one this
    ///        solver's current analysis was laid against.
    ///
    /// The model owns the answer and this only asks it: an epoch equal to the
    /// one recorded at the last analysis means no structural event has
    /// happened since, and the pattern in the buffer is therefore the analyzed
    /// one. False before the first analysis, and false over the whole span
    /// between a re-lay and the analysis that answers it -- a span a caller can
    /// open by renegotiating the partition count, or re-transcribing, between
    /// two solves. The next solve entry closes it.
    bool kkt_pattern_is_analyzed() const;

    // --- NLP management ---
    /// Sets (or replaces) the program this solver works on; also runs QP
    /// parameter setup.
    void set_nlp(std::shared_ptr<NonLinearProgram> np);
    /// @brief Releases the current program.
    void release();

    // --- Entry points ---
    /// @brief Runs the OPTIMIZE phase sequence from `x`.
    Eigen::VectorXd optimize(const Eigen::VectorXd &x);
    /// @brief Runs the SOLVE phase sequence from `x`.
    Eigen::VectorXd solve(const Eigen::VectorXd &x);
    /// @brief Runs SOLVE then OPTIMIZE from `x`. Both phases always run.
    Eigen::VectorXd solve_optimize(const Eigen::VectorXd &x);
    /// Runs OPTIMIZE then SOLVE from `x`. The trailing SOLVE is conditional:
    /// it is skipped when OPTIMIZE reported ConvergenceFlags::CONVERGED.
    Eigen::VectorXd optimize_solve(const Eigen::VectorXd &x);
    /// Runs SOLVE, OPTIMIZE, then SOLVE from `x`. The trailing SOLVE is
    /// conditional: it is skipped when OPTIMIZE reported
    /// ConvergenceFlags::CONVERGED.
    Eigen::VectorXd solve_optimize_solve(const Eigen::VectorXd &x);

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

    // --- Callback methods ---
    /// Installs the per-iteration early callback. The vectors and matrix it
    /// receives are in the SOLVER's variable space, which is narrower than the
    /// caller's on a problem with bound-fixed variables -- see the space note
    /// on EarlyCallBackType above.
    void set_early_callback(const EarlyCallBackType &f) {
        this->early_callback_enabled_ = true;
        this->early_callback_ = f;
    }
    /// @brief Disables the early callback.
    void disable_early_callback() { this->early_callback_enabled_ = false; }
    /// Installs the per-iteration late callback. Same variable-space caveat as
    /// set_early_callback -- see the note on LateCallBackType above.
    void set_late_callback(const LateCallBackType &f) {
        this->late_callback_enabled_ = true;
        this->late_callback_ = f;
    }
    /// @brief Disables the late callback.
    void disable_late_callback() { this->late_callback_enabled_ = false; }

    // --- Machine trace (schema v0) ---
    /// @brief Attaches a trace sink; `nullptr` (the default) is off.
    ///
    /// The sink is borrowed and must outlive every solve made while it is
    /// attached. Every emit site null-checks, and an unattached solve builds no
    /// event and does no census.
    ///
    /// @param sink The sink to attach, or `nullptr` to detach. Borrowed, not
    ///             owned.
    void attach_trace(TraceSink *sink);

    // --- Constraint-multiplier seeding ---
    /// Floor applied to seeded inequality multipliers when they are installed:
    /// the slack-complementarity update divides by these values, so a seed at
    /// or below zero would put the very first iterate outside the interior the
    /// method is defined on.
    static constexpr double kSeededIqMultFloor = 1.0e-8;

    /// Ceiling applied to every seeded multiplier (both signs for equality
    /// rows; the upper end for inequality rows, alongside kSeededIqMultFloor's
    /// lower end). Parity with Ipopt's own seeded-multiplier ceiling
    /// (warm_start_mult_init_max, default 1e6) and with this class's own
    /// bound-multiplier seeding precedent (kBoundMultInitCap = 1e3 in
    /// push_initial_point_interior, bound_set.h).
    static constexpr double kSeededMultInitMax = 1.0e6;

    /// Staged constraint-multiplier seeds. Consumed -- moved into run-local state
    /// and mults_staged_ cleared -- at the very start of the NEXT
    /// run_phase_sequence() call, before anything in that call can throw and
    /// leave this armed. validate_staged_multipliers() then rejects a mis-sized or
    /// non-finite seed once the row counts are final for the call. Applied at most
    /// once per call, to whichever XSL is current when the phase loop reaches the
    /// first OPT/OPTNO-mode phase -- never at all when the sequence has none.
    Eigen::VectorXd staged_eq_mults_;
    /// @brief The inequality half of the staged seed, under the same contract.
    Eigen::VectorXd staged_iq_mults_;
    /// True while a staged seed is waiting to be applied; cleared once applied
    /// and by clear_initial_multipliers().
    bool mults_staged_ = false;

    /// Stages equality/inequality multiplier seeds for the next solve call
    /// (see the staged_* field contract above).
    ///
    /// The seeds are the CALLER's multipliers, on the convention SolveResult
    /// reports in: Settings::obj_scale_ is multiplied in when they are
    /// installed, so a seed taken from an earlier SolveResult means the same
    /// thing whatever the scale is.
    ///
    /// PRECEDENCE against a staged warm start: the warm start wins. Any
    /// stage_warm_start() CALL, accepted or refused, clears a seed standing at
    /// that moment, and a seed staged after a warm start is discarded
    /// unapplied at solve entry -- see stage_warm_start().
    void set_initial_multipliers(const Eigen::VectorXd &eq_mults, const Eigen::VectorXd &iq_mults) {
        this->staged_eq_mults_ = eq_mults;
        this->staged_iq_mults_ = iq_mults;
        this->mults_staged_ = true;
    }
    /// @brief Discards any staged multiplier seeds.
    void clear_initial_multipliers() {
        this->staged_eq_mults_.resize(0);
        this->staged_iq_mults_.resize(0);
        this->mults_staged_ = false;
    }

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

    /// The staged warm start, valid only while warm_staged_ is true. Consumed
    /// -- moved into run-local state and warm_staged_ cleared -- at the very
    /// start of the NEXT run_phase_sequence() call, on the same terms as the
    /// multiplier seed above.
    WarmStartData staged_warm_;
    /// True while a staged warm start is waiting to be applied.
    bool warm_staged_ = false;

    /// @brief The warm-start value of the last completed solve, in DECLARED
    ///        space.
    ///
    /// Blocks, all at declared dimensions: `primal_` is the returned primal
    /// vector, an eliminated variable carrying the value the treatment holds it
    /// at; `eq_lmults_` is the USER's equality rows only, so MakeConstraint's
    /// internal fixing rows are dropped; `iq_lmults_` is the inequality block as
    /// reported; `bound_lmults_` is result().bound_lmults_ mapped out of the
    /// solver's reduced space, an exact zero at every eliminated variable.
    /// SIGN: z = z_lower - z_upper, verbatim from SolveResult::bound_lmults_.
    ///
    /// The stamp is the bound program's DECLARATION key as of that solve's
    /// COMPLETION, not as of this call, and not the layout/epoch key.
    ///
    /// EXTENSIONS: exactly one, `"hven.ipm.polish.v1"`, and only when the solve
    /// had a non-empty variable-bound set AND did not end on a
    /// restoration-active exit. It carries the invertible (z_lower, z_upper) pair
    /// at declared width, the inequality values cI(x), and the barrier parameter
    /// the solve ended at, all on the caller's objective scale. Without it the
    /// core-only value is the whole hand-off.
    ///
    /// @return The captured value, by copy.
    /// @throws std::logic_error if no solve has completed on this instance --
    ///         never an empty payload, which would stage cleanly and then
    ///         silently cold-start.
    WarmStartData export_warm_start() const;

    /// @brief Stages a warm start for the NEXT solve on this instance.
    /// @param data The value to stage, in DECLARED space; taken by const
    ///             reference and copied.
    ///
    /// One-shot and loud: the value applies to the next run_phase_sequence() call
    /// and is consumed by it, applied or refused; it survives any re-bind or
    /// re-lay in between; and a stamp mismatch at that call REFUSES rather than
    /// silently cold-starting. A stamp mismatch means a DIFFERENT PROBLEM was
    /// transcribed -- different declared dimensions, or a different declared bound
    /// STRUCTURE -- not a different treatment or layout; and a match promises
    /// neither the pieces' row structure nor the bound VALUES.
    ///
    /// CHECKED HERE: every block's length against the declared dimensions, and
    /// finiteness. The stamp is compared at solve entry instead.
    ///
    /// CLEARS FIRST: this call, whether it succeeds or refuses, first drops any
    /// warm start AND any multiplier seed staged before it; a seed staged after a
    /// warm start is discarded unapplied at solve entry.
    ///
    /// WHAT IS APPLIED: `primal_` becomes the starting point, mapped declared ->
    /// reduced and then pushed into the interior like any starting point;
    /// `eq_lmults_`/`iq_lmults_` go through the same staged-seed path
    /// set_initial_multipliers() feeds, with the same clamps and scale handling.
    /// `bound_lmults_` is validated and carried but NOT installed -- the signed
    /// core block does not invert into the (z_lower, z_upper) pair the barrier
    /// state needs. That pair travels in the `"hven.ipm.polish.v1"` extension,
    /// which this engine consumes, seeding the bound multipliers after the
    /// interior push under the same [kSeededIqMultFloor, kSeededMultInitMax]
    /// clamps. The payload's barrier parameter is NOT consumed: the schedule is a
    /// Settings decision the caller owns.
    ///
    /// @throws std::runtime_error if no NLP has been set.
    /// @throws std::invalid_argument if any block's length is not the matching
    ///         declared dimension (naming the block, the length held and the
    ///         length declared), if any block holds a non-finite value, if the
    ///         value carries the polish tag more than once, or if a payload under
    ///         that tag is malformed or is not at the declared widths (naming the
    ///         tag). An unknown tag is ignored -- a capability downgrade, not an
    ///         error. Every one of these refusals leaves this instance with
    ///         nothing staged.
    void stage_warm_start(const WarmStartData &data);

    /// @brief Discards any staged warm start.
    void clear_staged_warm_start() {
        this->staged_warm_ = WarmStartData{};
        this->warm_staged_ = false;
    }

    // --- Printing ---
    /// @brief Prints the console output banner ruler.
    static void print_header() { fmt::print(fmt::fg(fmt::color::white), "{0:=^{1}}\n", "", 65); }

  private:
    // Test access: these unit tests verify which concrete acceptance strategy
    // the settings dispatch constructs; befriended narrowly instead of
    // exposing a public rebuild hook.
    friend class ::RecoveryDispatchGate_FunnelSelectionConstructsFunnelAcceptance_Test;
    friend class ::RecoveryDispatchGate_FilterSelectionConstructsFilterAcceptance_Test;
    friend class ::RecoveryDispatchGate_MonitoredSelectionConstructsMonitoredGovernor_Test;
    friend class ::RecoveryDispatchGate_MeritPenaltyRuleSelectionReachesTheStrategy_Test;

    IpmOptions opts_;
    SolveResult result_;
    EvalErrorLog eval_error_log_;
    std::shared_ptr<NonLinearProgram> nlp_;

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
    // collects its diagnostics into SolveResult's last_feas_rest_* fields.
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
    EarlyCallBackType early_callback_;
    bool early_callback_enabled_ = false;
    LateCallBackType late_callback_;
    bool late_callback_enabled_ = false;

    /// The attached trace sink, or null. Never owned; see attach_trace().
    TraceSink *trace_ = nullptr;

    /// The 0-based index of the phase `alg_impl` is currently running, written
    /// by run_phase_sequence() before each call and read only by the `ipm.iter`
    /// emit sites. A member rather than an alg_impl parameter because it is
    /// instrumentation: the algorithm itself has no use for it.
    Index trace_phase_ = 0;

    // Is a public entry point on THIS object currently inside its solve? Set by
    // an RAII guard at run_phase_sequence()'s entry -- the ONE place all five
    // public entry points funnel through exactly once -- and cleared on every
    // exit, a throw included. Read by set_options(), which refuses to replace
    // the options a solve is running under. A nested restoration phase builds a
    // DISTINCT solver, so it never re-enters this object's guard.
    bool solve_in_flight_ = false;

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
        bool conditional_ = false; // skip if converge_flag_ == CONVERGED
                                   // (still runs on ACCEPTABLE / NOTCONVERGED;
                                   // DIVERGING breaks the loop before reaching this)
    };

    Eigen::VectorXd run_phase_sequence(const Eigen::VectorXd &x,
                                       std::initializer_list<PhaseStep> steps);

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
    void validate_warm_start_blocks(const WarmStartData &data, const char *entry) const;

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
    ConvergenceFlags converge_check(std::vector<IterateInfo> &iters);

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

    // --- Printing methods ---
    static void print_banner();
    void print_settings();
    void print_stats();
    void print_last_iterate(const std::vector<IterateInfo> &iters);
    void print_beginning(std::string_view msg) const;
    void print_finished(std::string_view msg) const;
    void print_exit_stats(ConvergenceFlags ExitCode, const IterateInfo &last, int iternum,
                          double tottime, double nlptime, double qptime, double printtime);
    void print_timing_summary();
    static fmt::text_style calculate_color(double val, double targ, double acc);
};

} // namespace hven::solvers
