// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The interior-point engine's options, as one value.
///
/// `IpmOptions` is an aggregate: write the fields you care about and hand the
/// whole value to the `IpmSolver` constructor or to `set_options()`, which
/// validate it. Either the whole value is accepted or the previous one stays in
/// force.
///
///     IpmOptions o = ipm_preset("funnel");
///     o.common.max_iterations = 200;
///     o.common.verbosity = Verbosity::kIterations;
///     IpmSolver solver(o);
///
/// Two tiers. The top-level fields, `common`, `termination` and `linear` are
/// the user tier: documented and stable between break windows. `advanced` is the
/// research tier (the globalization dispatch, line-search and inertia-correction
/// internals): its fields MAY CHANGE IN ANY MILESTONE, without a break window.

#include <iosfwd>
#include <limits>
#include <optional>
#include <string_view>
#include <vector>

#include <hven/core/types.h>
#include <hven/drivers/common_options.h>
#include <hven/drivers/ipm_enums.h>
#include <hven/linear/dense_policy.h>

namespace hven {

/// @brief The KKT factorization's settings.
struct IpmLinearOptions {
    /// Default `kNestedDissection`.
    KktOrdering ordering = KktOrdering::kNestedDissection;
    /// Default `kTwoByTwo`. Pardiso only.
    KktPivoting pivoting = KktPivoting::kTwoByTwo;
    /// Weighted matching. Default true. Pardiso only.
    bool matching = true;
    /// Symmetric scaling. Default false: it helps on some problem classes and
    /// degrades convergence on others. Pardiso only.
    bool scaling = false;
    /// Pivot perturbation exponent: a pivot below 10^-k times the matrix norm is
    /// perturbed. Default 8. Must be >= 0.
    int pivot_perturbation = 8;
    /// Iterative refinement steps per solve. Default 0. Must be >= 0, and 0 on
    /// Accelerate, which performs no refinement.
    int refinement_steps = 0;
    /// Default `kAdaptive`. Pardiso only.
    KktSolveParallelism parallel_solve = KktSolveParallelism::kAdaptive;
    /// Conditional numerical reproducibility: pins the backend to
    /// `common.threads` CNR threads, making the factorization bit-reproducible
    /// across thread counts. Read when the factorization is configured. Default
    /// false. Pardiso only.
    bool conditional_reproducibility = false;
    /// Force a fresh symbolic analysis on every solve instead of once per
    /// solver. Numeric refactorizations are unaffected. Default false.
    bool force_analysis = false;
#ifdef USE_ACCELERATE_SPARSE
    /// Accelerate's zero (drop) tolerance. Default 1e-4 * epsilon. Must be
    /// finite and > 0.
    double accelerate_zero_tolerance = 1e-4 * std::numeric_limits<double>::epsilon();
#endif
    /// When the dense factorization arm is used. RESERVED: the arm is not wired
    /// yet, and `validate()` accepts only `kNever` (the default).
    hven::linear::DensePolicy dense_factorization = hven::linear::DensePolicy::kNever;
    /// The dimension at or below which `DensePolicy::kAuto` takes the dense arm;
    /// empty uses the library's rule. RESERVED: `validate()` accepts only the
    /// empty value until the arm is wired.
    std::optional<Index> dense_max_dimension;
};

/// @brief The termination tests beyond the shared tolerances.
///
/// The convergence tier reads `common.optimality_tol` (KKT and, unless
/// `complementarity_tol` is set, barrier) and `common.feasibility_tol` (equality
/// and inequality). Each tier's tolerances must not exceed the next tier's.
struct IpmTerminationOptions {
    /// The barrier (complementarity) convergence tolerance. Empty (the default)
    /// uses `common.optimality_tol`. An engaged value must be finite and > 0.
    std::optional<double> complementarity_tol;

    /// Consecutive iterates that must all sit inside the acceptable tolerances
    /// before a solve reports `kAcceptable`. Default 50. Must be >= 1.
    Index acceptable_iterations = 50;
    /// Acceptable-level KKT tolerance. Default 1e-2.
    double acceptable_kkt_tol = 1.0e-2;
    /// Acceptable-level equality tolerance. Default 1e-3.
    double acceptable_equality_tol = 1.0e-3;
    /// Acceptable-level inequality tolerance. Default 1e-3.
    double acceptable_inequality_tol = 1.0e-3;
    /// Acceptable-level barrier tolerance. Default 1e-3.
    double acceptable_barrier_tol = 1.0e-3;

    /// Divergence thresholds, one per measure. Default 1e15 each.
    double divergence_kkt_tol = 1.0e15;
    double divergence_equality_tol = 1.0e15;
    double divergence_inequality_tol = 1.0e15;
    double divergence_barrier_tol = 1.0e15;
};

/// @brief The interior-point engine's research tier.
///
/// THESE FIELDS MAY CHANGE IN ANY MILESTONE, outside the break windows that
/// govern the rest of the options. Use `ipm_preset()` for the supported
/// combinations of the first nine.
struct IpmAdvancedOptions {
    /// Default `kFilter`, which `validate()` requires to run above
    /// `BarrierGovernorMode::kMonitored` unless `never_monotone` is set.
    AcceptanceMode acceptance_mode = AcceptanceMode::kFilter;
    /// Read only under `AcceptanceMode::kMerit`. Default `kSinglePenalty`.
    MeritPenaltyRule merit_penalty_rule = MeritPenaltyRule::kSinglePenalty;
    /// Default `kMonitored`.
    BarrierGovernorMode barrier_governor_mode = BarrierGovernorMode::kMonitored;
    /// Accept a funnel or filter test over `kClassicAdaptive` without the
    /// monotone safeguard. Contradicts `kMonitored`, which `validate()` refuses.
    /// Default false.
    bool never_monotone = false;
    /// Default `kOff`.
    RestorationMode restoration_mode = RestorationMode::kOff;
    /// Default `kClassic`. Assumes the exact model Hessian.
    InertiaMode inertia_mode = InertiaMode::kClassic;
    /// Second-order corrections after a first-trial rejection. Default 0 (off).
    /// Must be >= 0.
    Index max_second_order_corrections = 0;
    /// Further backtracking trials on the same direction once the classic
    /// ladder rejects and SOC is exhausted. Default 0 (off). Must be >= 0.
    Index max_extended_backtracks = 0;
    /// The watchdog (Chamberlain, Powell, Lemarechal and Pedersen 1982).
    /// Default false.
    bool watchdog = false;

    /// Restoration entries allowed per phase. Default 2. Must be >= 0; 0
    /// refuses restoration.
    Index max_restoration_entries = 2;
    /// Classic backtracking trials per rejected step. Default 2. Must be >= 0.
    Index max_line_search_backtracks = 2;
    /// The divisor applied to the step on each backtrack. Default 2. Must be
    /// finite and > 1.
    double backtrack_divisor = 2.0;

    /// What an `IpmPhase::kSolve` phase iterates on. Default `kSolveEquations`.
    SolvePhaseMode solve_phase_mode = SolvePhaseMode::kSolveEquations;
    /// Barrier update in the optimality phase. Default `kLoqo`.
    BarrierMode optimize_barrier_mode = BarrierMode::kLoqo;
    /// Barrier update in the feasibility phase. Default `kLoqo`.
    BarrierMode solve_barrier_mode = BarrierMode::kLoqo;
    /// Merit function in the optimality phase. Default `kAugmentedLagrangian`.
    LineSearchMode optimize_line_search_mode = LineSearchMode::kAugmentedLagrangian;
    /// Merit function in the feasibility phase. Default `kNone`.
    LineSearchMode solve_line_search_mode = LineSearchMode::kNone;
    /// Default `kEqualityMultipliersPrimal`.
    PrimalDualStepStrategy primal_dual_step_strategy =
        PrimalDualStepStrategy::kEqualityMultipliersPrimal;

    /// The inertia correction's first Hessian shift. Default 1e-5. Must be
    /// finite and > 0.
    double inertia_correction_initial = 1.0e-5;
    /// Its growth factor. Default 8. Must be finite and > 1.
    double inertia_correction_increase = 8.0;
    /// Its decay factor between iterations. Default 0.333333. Must be in (0, 1).
    double inertia_correction_decrease = 0.333333;
    /// Correction attempts per factorization. Default 15. Must be >= 0; 0
    /// disables the ladder.
    Index max_inertia_correction_attempts = 15;
    /// Skip the unperturbed factorization attempt, past iteration 6 and on 3 of
    /// every 4 iterations, while the last four iterations all needed Hessian
    /// perturbation. Saves a factorization known to fail on a persistently
    /// near-singular problem. Default true.
    bool skip_unperturbed_attempts = true;
    /// Reset value for a negative slack. Default 1e-12. Must be finite and > 0.
    double negative_slack_reset = 1.0e-12;
};

/// @brief Every user-configurable interior-point parameter, as one value.
struct IpmOptions {
    /// The shared options, at this engine's defaults: `max_iterations` 1000 (per
    /// phase) and `threads` HVEN_DEFAULT_QP_THREADS (must be >= 1).
    CommonOptions common{.max_iterations = 1000, .threads = HVEN_DEFAULT_QP_THREADS};

    /// The phase sequence, in order. Default `{kOptimize}`.
    ///
    /// Phases run in order; a `kSolve` that follows a `kOptimize` runs only if
    /// that phase did not report `kOptimal`; a `kOptimize` always runs; every
    /// phase re-initializes from the previous phase's point; a verdict of
    /// `kDiverging` or worse, an interrupt, or a work or time limit ends the
    /// sequence. Must not be empty.
    std::vector<IpmPhase> phases{IpmPhase::kOptimize};

    /// The acceptable and divergence tiers and the complementarity override.
    IpmTerminationOptions termination;

    /// Initial barrier parameter. Default 1e-3. Must lie in
    /// [`barrier_min`, `barrier_max`].
    double barrier_initial = 1.0e-3;
    /// Default 100. Must be finite and > 0.
    double barrier_max = 100.0;
    /// Default 1e-12. Must be finite, > 0 and <= `barrier_max`.
    double barrier_min = 1.0e-12;

    /// Fraction-to-boundary factor. Default 0.99. Must be in (0, 1).
    double fraction_to_boundary = 0.99;
    /// Absolute interior push of a bounded variable at solve entry:
    /// `bound_push * max(1, |bound|)`; also the floor on an inequality row's
    /// initial slack. Default 1e-3. Must be finite and > 0.
    double bound_push = 1.0e-3;
    /// Relative push of a two-sided variable, capped at
    /// `bound_interval_push * (upper - lower)`. Default 1e-2. Must be in
    /// (0, 0.5).
    double bound_interval_push = 1.0e-2;

    /// Default `kMakeParameter`. Read once per solve.
    FixedVariableTreatment fixed_variable_treatment = FixedVariableTreatment::kMakeParameter;
    /// Every finite variable bound is moved outward by this factor times
    /// max(1, |bound|) before the solve (Ipopt's bound_relax_factor); 0 records
    /// bounds verbatim. Read once per solve. Default 1e-8. Must be in
    /// [0, 1e-2].
    double bound_relax_factor = 1.0e-8;

    /// Objective scale factor applied at evaluation. Default 1. Must be finite
    /// and > 0.
    double objective_scale = 1.0;

    /// Return the best-scoring iterate seen instead of the last, scored by
    /// `best_iterate_criterion`, except on an interrupted exit, which returns the point
    /// the callback was shown. Default false.
    bool return_best = false;
    /// Default `kEquality`.
    BestIterateCriterion best_iterate_criterion = BestIterateCriterion::kEquality;

    /// Wide console table layout. Default false.
    bool wide_console = false;

    /// The KKT factorization.
    IpmLinearOptions linear;

    /// The research tier. May change in any milestone.
    IpmAdvancedOptions advanced;
};

/// @brief The barrier (complementarity) tolerance @p o's termination test reads:
///        `termination.complementarity_tol` when set, else `common.optimality_tol`.
inline double effective_complementarity_tol(const IpmOptions &o) noexcept {
    return o.termination.complementarity_tol.value_or(o.common.optimality_tol);
}

/// @brief Validates a whole IpmOptions value.
///
/// Every per-field condition stated on the fields above, the cross-field
/// orderings (`barrier_min <= barrier_initial <= barrier_max`; each convergence
/// tolerance <= its acceptable tolerance <= its divergence tolerance), the two
/// governor guards (`kFunnel`/`kFilter` over `kClassicAdaptive` without
/// `never_monotone`; `never_monotone` with `kMonitored`), every enum in range,
/// and the reserved dense-arm values.
///
/// @throws std::invalid_argument naming the first violated setting.
void validate(const IpmOptions &o);

/// @brief Returns a default IpmOptions with a named globalization preset applied.
///
/// Assigns exactly the first nine `advanced` fields (acceptance_mode through
/// watchdog) and leaves every other field at its default.
///
/// @throws std::invalid_argument, listing every valid name, if `name` is unknown.
IpmOptions ipm_preset(std::string_view name);

/// @brief Writes every field of @p o to @p os, one `name = value` line each, in
///        declaration order.
///
/// The format, fixed so that a diff of two dumps is a diff of two configurations:
///  * one `name = value` line per field, `\n`-terminated, no header or footer;
///  * declaration order; a nested struct is flattened with its member name as
///    the prefix (`common.threads`, `linear.ordering`, `advanced.watchdog`);
///  * an enum prints its enumerator name (`kLoqo`);
///  * a double prints at its shortest round-trip form (`1e-06`);
///  * a bool prints `true` / `false`;
///  * an optional prints its value, or `unset` when empty;
///  * `phases` prints as `[kOptimize, kSolve]` on one line.
///
/// Deterministic and side-effect-free: the caller's stream state (flags,
/// precision, width, locale) reaches no byte of the output and is not modified.
/// It does not validate.
void dump_options(const IpmOptions &o, std::ostream &os);

/// @brief Returns @p base prepared for a worker context: one backend thread and
///        silent output. Every other field is carried through untouched.
IpmOptions ipm_worker_options(IpmOptions base);

} // namespace hven
