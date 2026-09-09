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

#include <array>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/drivers/interior_point_solver_fwd.h"
#include "hven/detail/interior/eval_error_log.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/drivers/common_options.h"
#include "hven/drivers/solve_result.h"
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

/// @brief One phase of an interior-point solve, as `IpmOptions::phases` names it.
///
/// Declaration ORDER is contractual: it is what `static_cast<int>` and any
/// packed diagnostic column print.
enum class IpmPhase {
    /// The optimality phase -- `AlgorithmModes::OPT` at the optimality barrier
    /// and line-search modes. This is what the old `optimize()` entry ran.
    kOptimize = 0,
    /// The feasibility ("solve the equations") phase -- `IpmOptions::soe_mode`
    /// at the SOE barrier and line-search modes. This is what the old `solve()`
    /// entry ran.
    kSolve = 1,
};

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
    /// SolveStatus::kAcceptable. Raising it makes kAcceptable harder to
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
    /// under best_criteria), EXCEPT on an interrupted exit, which returns the
    /// point the callback was shown (M6 W5 T8.6). A stop is a caller's decision
    /// about a point it has just been handed, and substituting a different
    /// iterate for it would make the event a lie. Default false.
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

    /// THE PHASE SEQUENCE this solver runs, in order (M6 W5 T8.4).
    ///
    /// It REPLACES the five phase-named entry points. The five they were are
    /// documented instances of one rule, not five behaviours:
    ///
    ///   optimize()             -> {kOptimize}                    (the default)
    ///   solve()                -> {kSolve}
    ///   solve_optimize()       -> {kSolve, kOptimize}
    ///   optimize_solve()       -> {kOptimize, kSolve}
    ///   solve_optimize_solve() -> {kSolve, kOptimize, kSolve}
    ///
    /// THE RULE, for ANY sequence: phases run in order; a kSolve that FOLLOWS a
    /// kOptimize runs only if that optimize phase did not report kOptimal --
    /// today's conditional trailing solve, generalised, with the condition read
    /// off the IMMEDIATELY PRECEDING phase; a kOptimize is never conditional,
    /// which is why solve_optimize's second phase always ran; the first
    /// optimality phase installs a seeded multiplier value; every phase
    /// re-initialises under the same inter-phase rules as before; and a phase
    /// verdict of kDiverging or worse short-circuits the rest of the sequence.
    ///
    /// The EMPTY sequence is refused by validate(): a solve that runs no phase
    /// has nothing to report and is far more likely a caller's mistake than an
    /// intention.
    std::vector<IpmPhase> phases{IpmPhase::kOptimize};
};

/// @brief What one phase of a solve did.
///
/// One entry per phase in `IpmOptions::phases`, in that order, including the
/// phases that did NOT run -- `ran` is how a caller tells them apart, and a
/// skipped phase's other fields keep their defaults.
struct IpmPhaseReport {
    /// Which phase this is; equal to `IpmOptions::phases[i]` for entry i.
    IpmPhase phase = IpmPhase::kOptimize;
    /// THIS PHASE's verdict, resolved against its own stop reason. Per phase by
    /// construction: the engine resets the verdict at every phase start, so a
    /// later phase can no longer report an earlier one's answer (the pre-M6-W5
    /// defect design §2.3 registered for this task).
    SolveStatus status = SolveStatus::kNumericalError;
    /// Iterations this phase took; 0 when it did not run.
    Index iterations = 0;
    /// Wall-clock seconds inside this phase's `alg_impl`. INFORMATIONAL.
    double phase_seconds = 0.0;
    /// Which door this phase left its iteration loop by; `kNone` when the
    /// verdict is the whole explanation. Same value the solve-level
    /// `last_stop_reason()` reports for the LAST phase that ran.
    IpmStopReason stop_reason = IpmStopReason::kNone;
    /// False for a conditional phase the sequence skipped.
    bool ran = false;
};

/// @brief What one interior-point solve produced.
///
/// The base carries the answer in DECLARED space and CALLER units; everything
/// added here is this engine's own -- its measurements, its timings, its
/// per-phase account and its factorization snapshot.
///
/// PER CALL, all of it. This is a value returned by `solve()`, not an
/// accumulator a caller reads later: the engine holds no result between calls
/// and there is no `result()` accessor any more.
struct IpmResult : SolveResult {

    // --- The warm-start PAYLOAD's own two counters (M6 W5 T8.5) ---
    //
    // Both are 0 on every solve that was handed no payload -- the cold
    // overload, and any native-route caller -- and 0 or 1 otherwise, there
    // being one payload per call. They exist because the alternative to
    // counting a discarded payload is discarding it silently: a caller who set
    // `common.start_level` in one place and attached a payload in another has
    // no other way to find out which of the two won.

    /// @brief 1 when this call was handed a payload and IGNORED IT ENTIRELY
    ///        because `IpmOptions::common.start_level` was kCold; 0 otherwise.
    ///
    /// The payload's block lengths and its declaration stamp were still checked
    /// -- a foreign payload is refused at every rung -- and then nothing of it
    /// was applied. Apart from this field the call is the solve it would have
    /// been with no payload at all.
    int payload_ignored = 0;

    /// @brief 1 when this call was handed a payload carrying the
    ///        `"hven.ipm.polish.v1"` extension and did NOT consume it;
    ///        0 otherwise.
    ///
    /// Two ways to earn it, one reason. At a ceiling of kSeeded only the
    /// multipliers are applied; and on the MULTIPLIERS-ONLY seed form (an empty
    /// `primal_`) there is no point to apply at any ceiling. Either way this
    /// solve starts at `x0`, while the extension's (z_lower, z_upper) pair and
    /// inequality values are stated at the EXPORTER's point -- seeding barrier
    /// state from them would describe somewhere this solve is not.
    ///
    /// 0 at a kCold ceiling: nothing of the payload was read there, and
    /// `payload_ignored` is that call's answer.
    int polish_ignored = 0;

    /// @brief Which IpmOptions::fixed_variable_treatment this call actually
    ///        ran under.
    ///
    /// Always equals the option's value at the time the call ran --
    /// configure_variable_treatment runs the requested treatment or throws,
    /// never substitutes. Overwritten unconditionally at the start of every
    /// call. Recorded here because it decides lambda_e's shape.
    FixedVariableTreatments fixed_variable_treatment = FixedVariableTreatments::MakeParameter;

    // --- The treatment's own rows, reported rather than dropped ---
    //
    // Under MakeConstraint a bound-fixed variable stays in the problem and is
    // held by an INTERNAL equality row `x - value = 0` appended after the
    // declared rows. Those rows are the treatment's, not the declaration's, so
    // they are taken OFF the base's ce/lambda_e -- whose contract is the
    // DECLARED problem -- and reported here instead. Empty under every other
    // treatment, and empty under MakeConstraint on a problem with no
    // bound-fixed variables.
    //
    // The fixing row's multiplier is also what prices that variable's box in
    // declared space: the base's z carries -lambda_fix at that coordinate, the
    // sign the stationarity convention grad f + J'lambda - z = 0 forces.
    /// @brief Residuals of the internal fixing rows, one per bound-fixed
    ///        variable, in the order of
    ///        NonLinearProgram::fixed_variable_indices().
    Vec internal_fixed_ce;
    /// @brief Multipliers of those same rows, same order.
    Vec internal_fixed_lambda_e;

    // --- The engine's OWN terminal measurements ---
    //
    // NOT the base's four shared diagnostics: these are what this engine's
    // convergence test gated on, in ITS space, and a tolerance comparison must
    // be made against the number the test actually read.
    //
    // --- Terminal KKT residuals ---
    //
    // The four scalars converge_check() gates on, with IterateInfo's
    // definitions, so each is directly comparable against its matching
    // IpmOptions tolerance. kkt_inf and barr_inf carry IpmOptions::obj_scale;
    // econ_inf and icon_inf carry no scale, and nothing here is unscaled on
    // the way out. On a restoration-active exit they describe the restoration
    // subproblem, not the NLP. Last phase wins; the per-call reset sets them
    // to NaN, and NaN means UNMEASURED, never "zero residual".
    /// @brief inf-norm dual infeasibility (z-form) at the reported iterate.
    double kkt_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief Complementarity (barrier) error at the reported iterate.
    double barr_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm equality-constraint residual; 0 when equal_cons == 0.
    double econ_inf = std::numeric_limits<double>::quiet_NaN();
    /// @brief inf-norm inequality-constraint residual; 0 when inequal_cons == 0.
    double icon_inf = std::numeric_limits<double>::quiet_NaN();

    // --- Timing (seconds) ---
    /// @brief Total wall-clock time of the most recent call.
    ///
    /// INFORMATIONAL, NEVER ASSERTED -- counters are this project's currency
    /// of correctness, not timings. Measured on std::chrono::steady_clock.
    double total_time = 0;
    /// @brief Setup/preprocessing time.
    double pre_time = 0;
    /// @brief NLP evaluation callback time.
    double func_time = 0;
    /// @brief KKT assembly/factorization/solve time.
    double kkt_time = 0;
    /// @brief Printing time.
    double print_time = 0;
    /// @brief Solver-initialization time (measured before the main timer starts).
    double solver_init_time = 0;

    /// Derived timing — total wall-clock minus all categorized components.
    /// Excludes solver_init_time (measured before the main timer starts).
    /// Captures: callback time, step application, convergence checks, etc.
    double misc_time() const { return total_time - pre_time - kkt_time - func_time - print_time; }

    // --- Factorization stats ---
    /// @brief Memory reported by the last factorization.
    int factor_mem = 0;
    /// @brief Flops reported by the last factorization.
    int factor_flops = 0;

    /// Number of second-order correction back-substitutions performed
    /// across the whole solve (one per correction attempt; each costs a
    /// single constraint evaluation + one back-substitution on the live
    /// factorization). Always 0 when SOC is off (max_soc == 0). Reset per
    /// solve alongside the other accumulators.
    int soc_steps_taken = 0;

    /// Number of times the watchdog armed across the whole solve
    /// (Chamberlain, Powell, Lemaréchal & Pedersen 1982; constants per
    /// Wächter & Biegler 2006's implementation, globalization/watchdog.h).
    /// Always 0 when the watchdog is off (watchdog == false). Reset per
    /// solve alongside the other accumulators.
    int watchdog_activations = 0;

    /// Per-rejection recovery-chain outcome depth, indexed by the
    /// kRecoveryDepth* constants in globalization/recovery_chain.h: [0] SOC,
    /// [1] extended backtracking, [2] watchdog, [3] unresolved (the only
    /// bucket that increments when all three are off), [4] restoration
    /// (increments only when restoration_mode != off). Counts REJECTIONS,
    /// not interventions. Reset per solve.
    std::array<int, 5> recovery_depth_histogram{};

    /// Final funnel width (tau) reported by FunnelAcceptance at the end of
    /// the most recent solve's LAST PHASE. Sentinel -1.0 when the selected
    /// acceptance strategy does not report it, and when no acceptance test ran
    /// in that phase. A multi-phase call reports only the last phase's value.
    /// Reset per solve, not by the per-phase AcceptanceStrategy::reset().
    double last_funnel_width = -1.0;

    /// Final filter size (number of stored (θ, φ) pairs, Filter::size())
    /// reported by FilterAcceptance::append_diagnostics()
    /// (globalization/filter_acceptance.h) at the end of the most recent
    /// solve's LAST PHASE. Sentinel -1 when the selected acceptance
    /// strategy is not filter. Same last-phase-only semantics as
    /// last_funnel_width above.
    int last_filter_size = -1;

    /// Total filter-reset-heuristic clears (FilterAcceptance::filter_resets())
    /// at the end of the most recent solve's LAST PHASE. Sentinel -1 when the
    /// acceptance strategy is not filter. PER-PHASE, and under
    /// barrier_governor == monitored per barrier SUBPROBLEM: each mu-event
    /// also clears the counter, so this reports resets since the last mu-event
    /// of the last phase.
    int last_filter_resets = -1;

    /// Number of free -> monotone handoffs during the most recent solve's
    /// LAST PHASE, reported by MonitoredBarrierGovernor. Sentinel -1 when the
    /// selected barrier_governor is not monitored. Per-phase, like
    /// last_filter_resets above.
    int last_monotone_switches = -1;

    /// Number of iterations spent in monotone mode during the most recent
    /// solve's LAST PHASE, reported by MonitoredBarrierGovernor::
    /// append_diagnostics(). Sentinel -1 when the selected
    /// barrier_governor is not monitored. Same per-phase semantics as
    /// last_monotone_switches.
    int last_monotone_iters = -1;

    /// Number of times feasibility restoration was entered during the most
    /// recent solve's LAST PHASE, reported by RestorationStrategy. WRITE-ONLY
    /// diagnostics: no algorithm code reads it back. Sentinel -1 when no
    /// restoration strategy is constructed. Counting is identical across both
    /// modes -- entries_ increments once per entry call, iterations_in_mode_
    /// once per note_iteration() while active.
    int last_feas_rest_entries = -1;

    /// Number of iterations spent in restoration mode during the most
    /// recent solve's LAST PHASE, reported by RestorationStrategy::
    /// append_diagnostics(). WRITE-ONLY diagnostics field. Sentinel -1
    /// when no restoration strategy is constructed. Same per-phase
    /// semantics as last_feas_rest_entries (including the nested-mode
    /// counting note above).
    int last_feas_rest_iters = -1;

    /// Proximal primal-dual regularization shifts applied at the LAST
    /// FACTORIZED ITERATION of the most recent solve's last phase.
    /// last_prox_reg_primal is the persistent primal base shift rho_k added
    /// to the Hessian diagonal there; last_prox_reg_dual is the
    /// barrier-scaled dual shift delta_c subtracted from the constraint-row
    /// diagonals (0.0 when suppressed inside a nested l1 phase). Sentinel -1.0
    /// for BOTH when inertia_mode != proximal_regularization, and when a
    /// mode-on phase converged before its first factorization.
    double last_prox_reg_primal = -1.0;
    /// The dual half of the pair documented just above: the barrier-scaled
    /// dual shift δ_c, on the same sentinel and last-phase-wins rules.
    double last_prox_reg_dual = -1.0;

    /// Message of the most recent trial-evaluation exception absorbed by
    /// the acceptance machinery during the most recent solve call (all
    /// phases). Empty when every evaluation succeeded. A populated value
    /// means the solver rejected un-evaluable trial steps and continued —
    /// to full recovery, to a graceful ACCEPTABLE-level exit at an
    /// already-acceptable iterate, or into feasibility restoration. When
    /// none of those paths existed, the solve threw the latched message
    /// wrapped in solver context instead.
    std::string last_eval_exception;

    /// The last non-Success status observed from kkt_sol_.info() by
    /// factor_impl() within the CURRENT phase (alg_impl resets it on
    /// entry, so print_exit_stats reports per-phase status). Purely
    /// observational (surfaced by print_exit_stats()); feeds no
    /// control-flow decision in factor_impl.
    Eigen::ComputationInfo last_kkt_info = Eigen::Success;
    // --- The per-phase account (M6 W5 T8.4) ---
    /// @brief One entry per phase in `IpmOptions::phases`, in that order.
    ///
    /// The base's `status` is the LAST RAN phase's status and the base's
    /// `iterations` is the sum over the phases that ran.
    std::vector<IpmPhaseReport> phases;

    // --- The factorization SNAPSHOT (M6 W5 T8.4) ---
    //
    // These answer cross-call questions that used to be asked through
    // accessors on the solver. They are SNAPSHOTS taken as this result was
    // built, so they describe the solve that produced them and cannot go stale
    // behind a caller's back.
    /// @brief How many times the producing solver had laid and analyzed the KKT
    ///        sparsity pattern over its LIFETIME, as of this result.
    Index kkt_analyses_total = 0;
    /// @brief How many of those happened during THIS call.
    Index kkt_analyses_this_call = 0;
    /// @brief The KKT factor's linear-layer call counters as of this result,
    ///        including the pattern-guard count.
    KktFactorization::Counters kkt_factor_counters;
    /// @brief The log of NLP evaluation errors absorbed during this call.
    EvalErrorLog eval_error_log;
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
