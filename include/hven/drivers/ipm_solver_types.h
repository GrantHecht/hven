// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The interior-point engine's options AS A VALUE (M6 W5 T8.3).
//
// This header carries what used to be InteriorPointSolver::Settings -- the same
// 65 knobs, in the same declaration order, with the trailing underscores gone --
// plus the mode enums the struct is written in terms of, which used to be nested
// inside the solver class and are now namespace-scope here so that a caller can
// name an option without including the solver. InteriorPointSolver keeps member
// type aliases for all eight, so `InteriorPointSolver::BarrierModes::LOQO` still
// names the same type it always did.
//
// Two fields LEFT the struct: qp_threads_ became CommonOptions::threads and
// print_level_ became CommonOptions::print_level, both reached through
// `common`. Their defaults are unchanged (HVEN_DEFAULT_QP_THREADS and 0), so
// nothing behaves differently -- T8.3 moves the fields and stops there.
//
// `common` is the LAST member, not the first, deliberately: every field ahead
// of the two that moved keeps the byte offset it had inside Settings, which is
// what lets most of the engine's compiled bodies stay byte-identical across
// this task.
//
// The ~50 validated set_*() methods, the four strto_*() parsers and the six
// string-taking setter overloads that used to guard these fields are GONE. A
// caller writes fields on a value and hands the whole value over:
//
//     IpmOptions o = ipm_preset("funnel");
//     o.max_iters = 200;
//     o.common.print_level = 3;
//     solver.set_options(std::move(o));
//
// validate() below is the one place a field's invariant lives now, and it runs
// at construction, at set_options() and again at run_phase_sequence() entry.

#include <string_view>

#include <Eigen/Core>

#include "hven/detail/drivers/interior_point_solver_fwd.h"
#include "hven/drivers/common_options.h"
#include "hven/model/non_linear_program.h"

#ifdef USE_ACCELERATE_SPARSE
#include <limits>
#endif

namespace hven::solvers {

/// Barrier-mode selector (IpmOptions::opt_bar_mode/soe_bar_mode).
enum class BarrierModes { PROBE, LOQO };
/// Line-search-mode selector (IpmOptions::opt_ls_mode/soe_ls_mode).
enum class LineSearchModes { AUGLANG, LANG, L1, NOLS };
/// @brief Algorithm mode of one solve phase.
enum class AlgorithmModes { OPT, OPTNO, SOE, INIT };

/// @brief QP factorization algorithm variant.
enum class QPAlgModes {
    Classic = 0,
    TwoLevel = 1,
};

/// QP fill-reducing ordering.
enum class QPOrderingModes { MINDEG = 0, METIS = 2, PARMETIS = 3 };
/// Criterion used to score iterates when return_best is on.
enum class BestCriteriaModes { ECONS, ICONS, KKT, OBJ };

/// @brief QP pivot strategy code passed through to the sparse backend.
enum class QPPivotModes {
    OneByOne = 0,
    TwoByTwo = 1,
    E4 = 4,
    E6 = 6,
    E8 = 8,
    E13 = 13,
};
/// @brief Primal-dual step computation strategy for the QP subproblem.
enum class PDStepStrategies { PrimSlackEq_Iq, AllMinimum, PrimSlack_EqIq, MaxEq };

/// @brief Every user-configurable interior-point parameter, as one value.
///
/// An aggregate: write the fields you care about and hand the whole struct to
/// the constructor or to set_options(), which validates it. There is no
/// per-field setter and no partially-applied state -- either the whole value is
/// accepted or the previous one stays in force.
struct IpmOptions {
    // --- Iteration limits ---
    /// @brief Main iteration cap per phase. Default 500.
    int max_iters = 500;
    /// @brief Classic backtracking ladder cap per rejected trial. Default 2.
    int max_ls_iters = 2;
    /// Number of consecutive trailing iterates that must ALL sit inside the
    /// acceptable tolerances before converge_check() reports
    /// ConvergenceFlags::ACCEPTABLE. Raising it makes ACCEPTABLE harder to
    /// reach, not easier. Default 50. Must be > 0.
    int max_acc_iters = 50;
    /// @brief Refactorization attempt cap. Default 15.
    int max_refac = 15;
    /// Maximum second-order corrections attempted after a first-trial
    /// rejection (Wächter & Biegler 2006, §2.4). Default 0 = off: the
    /// solver behaves exactly as it does without SOC. Set > 0 to opt in;
    /// the recommended enable value is 4 (kSocRecommendedMaxCorrections
    /// in globalization/soc.h).
    int max_soc = 0;

    /// Extended backtracking cap: further trials continuing the SAME
    /// classic ladder (same direction, same alpha_red divisor, same merit
    /// test) once the classic capped backtrack rejects and SOC (if
    /// enabled) is exhausted or not triggered. Default 0 = off: the solver
    /// behaves exactly as it does without extended backtracking. This cap
    /// extends the classic cap (max_ls_iters) ONLY when the recovery
    /// dispatch is active on a rejected step — max_ls_iters itself is
    /// unaffected. See ExtendedBacktrackRecovery, globalization/watchdog.h.
    int ls_extended_iters = 0;

    /// Watchdog (Chamberlain, Powell, Lemaréchal & Pedersen 1982;
    /// constants per Wächter & Biegler 2006's implementation — see
    /// globalization/watchdog.h): arms after kWatchdogShortenedIterTrigger
    /// consecutive fully-rejected iterations, then accepts up to
    /// kWatchdogTrialIterMax trial iterations under relaxed acceptance
    /// before reverting to the pre-watchdog snapshot. Default false =
    /// off: the solver behaves exactly as it does without the watchdog.
    bool watchdog = false;

    /// Per-phase feasibility-restoration entry budget: the maximum number
    /// of times restoration mode may be entered within a single phase. 0
    /// refuses restoration entirely; ignored when restoration_mode == off.
    /// validate() requires >= 0. Default 2.
    int max_feas_rest = 2;

    // --- Convergence tolerances ---
    /// @brief KKT stationarity convergence tolerance. Default 1e-6.
    double kkt_tol = 1.0e-6;
    /// @brief Equality-constraint feasibility convergence tolerance. Default 1e-6.
    double econ_tol = 1.0e-6;
    /// @brief Inequality-constraint feasibility convergence tolerance. Default 1e-6.
    double icon_tol = 1.0e-6;
    /// @brief Barrier (complementarity) convergence tolerance. Default 1e-6.
    double bar_tol = 1.0e-6;

    // --- Acceptable tolerances ---
    /// @brief Acceptable-level KKT tolerance. Default 1e-2.
    double acc_kkt_tol = 1.0e-2;
    /// @brief Acceptable-level equality-constraint tolerance. Default 1e-3.
    double acc_econ_tol = 1.0e-3;
    /// @brief Acceptable-level inequality-constraint tolerance. Default 1e-3.
    double acc_icon_tol = 1.0e-3;
    /// @brief Acceptable-level barrier tolerance. Default 1e-3.
    double acc_bar_tol = 1.0e-3;

    // --- Divergence tolerances ---
    /// @brief Divergence threshold on the KKT measure. Default 1e15.
    double div_kkt_tol = 1.0e15;
    /// @brief Divergence threshold on equality feasibility. Default 1e15.
    double div_econ_tol = 1.0e15;
    /// @brief Divergence threshold on inequality feasibility. Default 1e15.
    double div_icon_tol = 1.0e15;
    /// @brief Divergence threshold on the barrier measure. Default 1e15.
    double div_bar_tol = 1.0e15;

    // --- Algorithm modes ---
    /// @brief Phase algorithm mode. Default SOE.
    AlgorithmModes soe_mode = AlgorithmModes::SOE;
    /// @brief OPT-phase barrier mode. Default LOQO.
    BarrierModes opt_bar_mode = BarrierModes::LOQO;
    /// @brief SOE-phase barrier mode. Default LOQO.
    BarrierModes soe_bar_mode = BarrierModes::LOQO;
    /// @brief OPT-phase line-search mode. Default AUGLANG.
    LineSearchModes opt_ls_mode = LineSearchModes::AUGLANG;
    /// @brief SOE-phase line-search mode. Default NOLS.
    LineSearchModes soe_ls_mode = LineSearchModes::NOLS;
    /// Primal-dual step strategy for the QP subproblem.
    /// Default PrimSlackEq_Iq.
    PDStepStrategies pd_step_strategy = PDStepStrategies::PrimSlackEq_Iq;

    // --- Step-acceptance strategy (opt-in modernized merit) ---
    /// classic_merit (the default) is the fused backtracking merit line
    /// search; merit selects the modernized merit family, whose penalty rule
    /// is merit_penalty_rule (read only under merit). Both enums live in
    /// interior_point_solver_fwd.h.
    AcceptanceStrategies acceptance_strategy = AcceptanceStrategies::classic_merit;
    /// Merit penalty rule for the generic merit family; only read when
    /// acceptance_strategy == merit. Default wmno.
    MeritPenaltyRules merit_penalty_rule = MeritPenaltyRules::wmno;

    // --- Barrier-parameter governor (opt-in monitored free<->monotone) ---
    /// classic_adaptive (the default) is the PROBE/LOQO free-mode barrier
    /// update; monitored selects the free<->monotone governor, which composes
    /// a classic_adaptive delegate and so pairs with any acceptance_strategy.
    /// validate() rejects funnel or filter over classic_adaptive unless
    /// never_monotone is set. Enum in interior_point_solver_fwd.h.
    BarrierGovernors barrier_governor = BarrierGovernors::classic_adaptive;

    /// Expert escape hatch mirroring Ipopt's never-monotone-mode: explicitly
    /// accepts running funnel/filter above the classic_adaptive (free-only)
    /// barrier governor without its monotone safeguard. Default false.
    /// Contradictory when combined with barrier_governor == monitored (the
    /// monitored governor already provides the monotone fallback) —
    /// validate() rejects that combination.
    bool never_monotone = false;

    /// off (the default) constructs no RestorationStrategy at all, and every
    /// restoration branch in the solver is then dead. proximal_switch swaps
    /// the true objective for a proximal term on a ladder-exhausted rejection
    /// at a not-near-feasible point, until infeasibility is sufficiently
    /// reduced. l1_nested instead runs an l1 elastic reformulation as a
    /// condensed in-place phase reusing the outer KKT system. Both compose
    /// with every acceptance_strategy and barrier_governor and share the
    /// max_feas_rest entry budget. Enum in interior_point_solver_fwd.h.
    RestorationModes restoration_mode = RestorationModes::off;

    // --- Barrier parameters ---
    /// @brief Initial barrier parameter. Default 1e-3.
    double init_mu = 0.001;
    /// @brief Maximum barrier parameter. Default 100.
    double max_mu = 100.0;
    /// @brief Minimum barrier parameter. Default 1e-12.
    double min_mu = 1.0e-12;

    // --- Step parameters ---
    /// @brief Fraction-to-boundary factor. Default 0.99.
    double bound_fraction = 0.99;
    /// Absolute component of the interior push applied to a bounded primal
    /// variable at solve entry: the push away from a bound is
    /// bound_push * max(1, |bound|), before the two-sided cap below
    /// (Ipopt's bound_push). It is also the floor on the initial slack the
    /// INIT multiplier pass hands an inequality row: that slack is
    /// max(-c_i(x), bound_push). Must be > 0. Default 1e-3.
    double bound_push = 1.0e-3;
    /// Relative component of that same push, applied only to a TWO-SIDED
    /// variable: the push is additionally capped at
    /// bound_interval_push * (upper - lower), so a narrow interval is never
    /// pushed past its own midpoint (Ipopt's bound_frac, same default).
    /// Read only when the problem declares native variable bounds.
    /// Must lie in the open interval (0, 0.5), so the lower and upper
    /// projections of one variable cannot cross. Default 1e-2.
    double bound_interval_push = 1.0e-2;
    /// @brief Reset threshold for negative slack values. Default 1e-12.
    double neg_slack_reset = 1.0e-12;
    /// @brief Backtracking step reduction divisor. Default 2.0.
    double alpha_red = 2.0;

    /// How a primal variable whose declared lower and upper bounds are equal
    /// is handed to the solver. MakeParameter (the default) eliminates it, so
    /// the factorized system is one row and column narrower per fixed
    /// variable; MakeConstraint keeps it and appends one internal equality row
    /// per fixed variable after every declared row, so the system is wider;
    /// RelaxBounds keeps it two-sided with its bounds pushed apart by
    /// bound_relax_factor. All three reach the same solution on a well-posed
    /// problem. Closed-set enum, declared in non_linear_program.h.
    FixedVariableTreatments fixed_variable_treatment = FixedVariableTreatments::MakeParameter;

    /// Widening applied to every finite variable bound before the
    /// classifier records it: the bound is moved outward by this factor
    /// times max(1, |bound|), so the box the barrier terms divide by is
    /// never exactly the declared one (Ipopt's bound_relax_factor, same
    /// default). Also what separates the bounds of a fixed variable under
    /// the relax_bounds treatment, which therefore requires it positive.
    /// Zero records every declared bound verbatim.
    ///
    /// Both this and fixed_variable_treatment are read once per solve, at
    /// the NLP's classification pass; changing either between two solves on
    /// one solver re-classifies.
    double bound_relax_factor = kDefaultBoundRelaxFactor;

    // --- Hessian perturbation ---
    /// @brief Initial Hessian perturbation delta. Default 1e-5.
    double delta_h = 1.0e-5;
    /// @brief Perturbation growth multiplier. Default 8.0.
    double incr_h = 8.0;
    /// @brief Perturbation decay multiplier. Default 0.333333.
    double decr_h = 0.333333;

    /// KKT inertia-correction mode. classic (the default) runs the on-demand
    /// inertia ladder under the full inertia condition and engages the dual
    /// shift -delta_c at most once per phase (then latched); an exhausted
    /// ladder fails the step. proximal_regularization bakes a decaying primal
    /// base shift rho_k and an always-on barrier-scaled -delta_c into the base
    /// matrix each iteration, with the same ladder escalating on top; delta_c
    /// is suppressed while a nested l1 restoration phase is active. Closed-set
    /// enum, declared in interior_point_solver_fwd.h.
    InertiaModes inertia_mode = InertiaModes::classic;

    // --- QP solver ---
    /// @brief QP factorization algorithm variant. Default Classic.
    QPAlgModes qp_alg = QPAlgModes::Classic;
    /// @brief QP fill-reducing ordering. Default METIS.
    QPOrderingModes qp_ord = QPOrderingModes::METIS;
    /// @brief QP pivot strategy. Default TwoByTwo.
    QPPivotModes qp_pivot_strategy = QPPivotModes::TwoByTwo;
    /// @brief MKL Pardiso weighted matching (iparm[12]) flag, 0/1. ON by default.
    int qp_matching = 1;
    /// MKL Pardiso MPS scaling (iparm[10]) flag, 0/1. OFF by default: it
    /// helps on some problem classes and deterministically degrades
    /// convergence on others.
    /// @see docs/notes/2026-09-header-prose-archive.md §interior_point_solver.h
    int qp_scaling = 0;
    /// @brief Pivot perturbation level handed to the backend. Default 8.
    int qp_pivot_perturb = 8;
    /// @brief Iterative-refinement steps handed to the backend. Default 0.
    int qp_ref_steps = 0;
    /// @brief Parallel-solve flag handed to the backend. Default 0.
    int qp_par_solve = 0;
    /// @brief Backend-side QP printout toggle. Default false.
    bool qp_print = false;
#ifdef USE_ACCELERATE_SPARSE
    /// @brief Apple Accelerate sparse pivot tolerance. Default 0.01.
    double accel_pivot_tolerance = 0.01;
    /// Apple Accelerate sparse zero (drop) tolerance.
    /// Default 1e-4 * epsilon.
    double accel_zero_tolerance = 1e-4 * std::numeric_limits<double>::epsilon();
#endif

    // --- Objective ---
    /// @brief Objective scale factor applied at evaluation. Default 1.0.
    ///        Finite and strictly positive; validate() enforces it.
    double obj_scale = 1.0;

    // --- Output/behavior ---
    /// @brief Wide console layout for tables. Default false.
    bool wide_console = false;
    /// Conditional Numerical Reproducibility mode. When true the sparse
    /// backend is pinned to common.threads CNR threads (opts.cnr_threads),
    /// which makes the factorization bit-reproducible across thread counts;
    /// false leaves cnr_threads at 0 (off). Read once, at set_qp_params()
    /// transcribe time -- a later common.threads change does not move it (see
    /// the refresh-cadence note on KktFactorization::set_num_threads).
    /// Default false.
    bool cnr_mode = false;
    /// Zero-perturbation-attempt cycling heuristic. When true, past
    /// iteration 6 and on 3 of every 4 iterations, the unperturbed
    /// factorization attempt is skipped if the last four iterations all
    /// needed Hessian perturbation -- saving a factorization known to fail
    /// on a persistently near-singular problem, while the remaining 1 in 4
    /// iterations re-probes for recovered inertia. Default true.
    bool fast_factor_alg = true;
    /// Force a fresh symbolic sparsity analysis on every solve. The
    /// analysis normally runs once per solver instance and is latched
    /// (claim_kkt_analysis()); this defeats that latch. Numeric
    /// refactorizations are unaffected -- they never consult this field.
    /// Default false.
    bool force_qp_analysis = false;
    /// Return the best-scoring iterate seen instead of the last (scored
    /// under best_criteria). Default false.
    bool return_best = false;
    /// @brief Scoring criterion for the return_best path. Default ECONS.
    BestCriteriaModes best_criteria = BestCriteriaModes::ECONS;
    /// The options both engines share, defaulted to the INTERIOR-POINT engine's
    /// historical values: threads = HVEN_DEFAULT_QP_THREADS (what
    /// Settings::qp_threads_ defaulted to) and print_level = 0 (full output,
    /// what Settings::print_level_ defaulted to). start_level is CARRIED BUT
    /// UNREAD by this engine in T8.3; T8.5 is where its warm-start entry starts
    /// consulting it.
    ///
    /// LAST, not first: see this header's banner.
    CommonOptions common{/*threads=*/HVEN_DEFAULT_QP_THREADS, /*print_level=*/0,
                         /*start_level=*/StartLevel::kWarm};
};

/// @brief Validates a whole IpmOptions value, throwing std::invalid_argument on
///        the first violation.
///
/// Checks every per-field condition the removed set_*() methods checked, the
/// cross-field ordering invariants (min_mu <= init_mu <= max_mu, convergence
/// tols <= their respective acceptable tols <= their respective divergence
/// tols), the two combination guards (acceptance_strategy funnel or filter with
/// barrier_governor == classic_adaptive and !never_monotone; never_monotone with
/// barrier_governor == monitored), and the two fields that moved into `common`
/// (threads must be positive, print_level non-negative -- the same conditions
/// qp_threads_ and print_level_ faced).
///
/// This is the body of what was InteriorPointSolver::Settings::validate(), with
/// the two `common` checks added and every message unchanged.
///
/// @throws std::invalid_argument naming the first violated setting or the
///         rejected combination.
void validate(const IpmOptions &o);

/// @brief Returns a default IpmOptions with a named globalization preset
///        applied.
///
/// Assigns exactly nine fields (acceptance_strategy, merit_penalty_rule,
/// barrier_governor, never_monotone, restoration_mode, inertia_mode, max_soc,
/// ls_extended_iters, watchdog) on top of a default-constructed value and
/// leaves every other field at its default. The preset table lives in
/// detail/drivers/interior_point_solver_presets.h.
///
/// This replaces InteriorPointSolver::apply_preset(), which mutated the
/// solver's own settings in place; the free function returns a full value the
/// caller can edit further before handing it over.
///
/// @param name A name from the preset table.
/// @throws std::invalid_argument, listing every valid name, if `name` is not in
///         the table.
IpmOptions ipm_preset(std::string_view name);

} // namespace hven::solvers
