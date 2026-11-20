// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options tables of `IpmOptions` and its nested structs, and `for_each_option`.
///
/// One table per struct, one row per field, in declaration order; see
/// `detail/core/options_registry.h` for the row grammar and the checks. The `doc` strings are the
/// one-line statement the generated options reference prints; the header comments on the structs
/// carry the full contract.

#include <string>

#include <hven/detail/core/options_registry.h>
#include <hven/detail/ipm/ipm_option_enum_names.h>
#include <hven/detail/solver/common_options_fields.h>
#include <hven/ipm/ipm_options.h>

// clang-format off
#define HVEN_IPM_LINEAR_OPTIONS_FIELDS(X)                                                          \
    X(ordering, r_any(), "Fill-reducing ordering of the KKT factorization.")                       \
    X(pivot_strategy, r_any(), "Pivoting strategy. Pardiso only.")                                 \
    X(weighted_matching, r_any(), "Weighted matching. Pardiso only.")                              \
    X(matrix_scaling, r_any(),                                                                     \
      "Symmetric scaling: helps on some problem classes, degrades convergence on others. "         \
      "Pardiso only.")                                                                             \
    X(pivot_perturbation_exponent, r_ge(0.0),                                                      \
      "Pivot perturbation exponent: a pivot below 10^-k times the matrix norm is perturbed.")      \
    X(max_refinement_steps, r_ge(0.0),                                                             \
      "Iterative refinement steps per solve. Must be 0 on Accelerate, which performs none.")       \
    X(solve_parallelism, r_any(), "Parallelism of the solve phase. Pardiso only.")                 \
    X(conditional_reproducibility, r_any(),                                                        \
      "Conditional numerical reproducibility: bit-reproducible factorization across thread "       \
      "counts. Pardiso only; requires dense_factorization = kNever.")                              \
    X(force_analysis, r_any(),                                                                     \
      "Force a fresh symbolic analysis on every solve instead of once per solver.")                \
    X(dense_factorization, r_any(),                                                                \
      "When the dense factorization arm is used: kAuto takes it for a KKT pattern of order "       \
      "n_kkt <= 167, the sparse backend above.")                                                   \
    X(accelerate_zero_tol, r_gt(0.0),                                                              \
      "Accelerate's zero (drop) tolerance. Ignored on MKL, validated on every backend.")           \
    X(max_dense_dimension, r_text("unset (reserved)"),                                             \
      "Reserved for a caller's kAuto threshold; only the empty value is accepted.")

#define HVEN_IPM_TERMINATION_OPTIONS_FIELDS(X)                                                     \
    X(complementarity_tol, r_gt(0.0),                                                              \
      "The barrier (complementarity) convergence tolerance. Unset uses common.optimality_tol.")    \
    X(acceptable_iterations, r_ge(1.0),                                                            \
      "Consecutive iterates that must sit inside the acceptable tolerances before kAcceptable.")   \
    X(acceptable_stationarity_tol, r_gt(0.0),                                                      \
      "Acceptable-level stationarity (dual infeasibility) tolerance.")                             \
    X(acceptable_equality_tol, r_gt(0.0), "Acceptable-level equality tolerance.")                  \
    X(acceptable_inequality_tol, r_gt(0.0), "Acceptable-level inequality tolerance.")              \
    X(acceptable_complementarity_tol, r_gt(0.0), "Acceptable-level complementarity tolerance.")    \
    X(divergence_stationarity_tol, r_gt(0.0), "Divergence threshold on stationarity.")             \
    X(divergence_equality_tol, r_gt(0.0), "Divergence threshold on the equality residual.")        \
    X(divergence_inequality_tol, r_gt(0.0), "Divergence threshold on the inequality residual.")    \
    X(divergence_complementarity_tol, r_gt(0.0), "Divergence threshold on complementarity.")

#define HVEN_IPM_ADVANCED_OPTIONS_FIELDS(X)                                                        \
    X(acceptance_mode, r_any(),                                                                    \
      "Step-acceptance test. kFunnel and kFilter require the monitored barrier governor unless "   \
      "never_monotone is set.")                                                                    \
    X(merit_penalty_rule, r_any(), "Penalty rule; read only under kMerit.")                        \
    X(barrier_governor_mode, r_any(), "Barrier-parameter governor.")                               \
    X(never_monotone, r_any(),                                                                     \
      "Accept a funnel or filter test over kClassicAdaptive without the monotone safeguard. "      \
      "Contradicts kMonitored.")                                                                   \
    X(restoration_mode, r_any(), "Restoration strategy.")                                          \
    X(inertia_mode, r_any(), "Inertia-correction strategy; kClassic assumes the exact Hessian.")   \
    X(max_second_order_corrections, r_ge(0.0),                                                     \
      "Second-order corrections after a first-trial rejection. 0 is off.")                         \
    X(max_extended_backtracks, r_ge(0.0),                                                          \
      "Further backtracking trials once the ladder rejects and SOC is exhausted. 0 is off.")       \
    X(watchdog, r_any(), "The watchdog (Chamberlain, Powell, Lemarechal and Pedersen 1982).")      \
    X(max_restoration_entries, r_ge(0.0),                                                          \
      "Restoration entries allowed per phase. 0 refuses restoration.")                             \
    X(max_line_search_backtracks, r_ge(0.0), "Classic backtracking trials per rejected step.")     \
    X(backtrack_divisor, r_gt(1.0), "The divisor applied to the step on each backtrack.")          \
    X(solve_phase_mode, r_any(), "What a kSolve phase iterates on.")                               \
    X(optimize_barrier_mode, r_any(), "Barrier update in the optimality phase.")                   \
    X(solve_barrier_mode, r_any(), "Barrier update in the feasibility phase.")                     \
    X(optimize_line_search_mode, r_any(), "Merit function in the optimality phase.")               \
    X(solve_line_search_mode, r_any(), "Merit function in the feasibility phase.")                 \
    X(step_length_rule, r_any(), "How the step length is shared between primal and multipliers.")  \
    X(inertia_correction_initial, r_gt(0.0), "The inertia correction's first Hessian shift.")      \
    X(inertia_correction_increase, r_gt(1.0), "The inertia correction's growth factor.")           \
    X(inertia_correction_decrease, r_open(0.0, 1.0),                                               \
      "The inertia correction's decay factor between iterations.")                                 \
    X(max_inertia_correction_attempts, r_ge(0.0),                                                  \
      "Correction attempts per factorization. 0 disables the ladder.")                             \
    X(skip_unperturbed_attempts, r_any(),                                                          \
      "Skip the unperturbed factorization attempt while the last four iterations all needed "      \
      "Hessian perturbation.")                                                                     \
    X(negative_slack_reset, r_gt(0.0), "Reset value for a negative slack.")

#define HVEN_IPM_OPTIONS_FIELDS(X, G)                                                              \
    G(common, common, false)                                                                       \
    X(phases, r_text("non-empty; each of kOptimize, kSolve"),                                      \
      "The phase sequence, in order. A kSolve after a kOptimize runs only if that phase was not "  \
      "kOptimal.")                                                                                 \
    G(termination, ipm_termination, false)                                                         \
    X(barrier_initial, r_gt(0.0),                                                                  \
      "Initial barrier parameter. Must lie in [barrier_min, barrier_max].")                        \
    X(barrier_max, r_gt(0.0), "Largest barrier parameter.")                                        \
    X(barrier_min, r_gt(0.0), "Smallest barrier parameter. Must be <= barrier_max.")               \
    X(fraction_to_boundary, r_open(0.0, 1.0), "Fraction-to-boundary factor.")                      \
    X(bound_push, r_gt(0.0),                                                                       \
      "Absolute interior push of a bounded variable at solve entry, bound_push * "                 \
      "max(1, |bound|); also the floor on an inequality row's initial slack.")                     \
    X(bound_interval_push, r_open(0.0, 0.5),                                                       \
      "Relative push of a two-sided variable, capped at bound_interval_push * (upper - lower).")   \
    X(fixed_variable_treatment, r_any(), "How a variable with equal bounds is treated.")           \
    X(bound_relax_factor, r_closed(0.0, 0.01),                                                     \
      "Every finite variable bound moves outward by this factor times max(1, |bound|) before "     \
      "the solve. 0 records bounds verbatim; kRelaxBounds requires > 0.")                          \
    X(objective_scale, r_gt(0.0), "Objective scale factor applied at evaluation.")                 \
    G(linear, ipm_linear, false)                                                                   \
    G(advanced, ipm_advanced, true)
// clang-format on

namespace hven::detail {

template <class S, class F> void visit_ipm_linear(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "IpmLinearOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_IPM_LINEAR_OPTIONS_FIELDS(X)
#undef X
}

template <class S, class F> void visit_ipm_termination(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "IpmTerminationOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_IPM_TERMINATION_OPTIONS_FIELDS(X)
#undef X
}

template <class S, class F> void visit_ipm_advanced(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "IpmAdvancedOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_IPM_ADVANCED_OPTIONS_FIELDS(X)
#undef X
}

/// @brief Calls `f(const OptionMeta &, T &value)` for every field of @p o, in declaration order, a
///        nested struct flattened under its member name (`common.`, `linear.`, `advanced.`).
///
/// Typed access, for the dump, the metadata and the bindings; there is no string-to-field setter.
/// @p o may be const.
template <class S, class F>
    requires std::is_same_v<std::remove_const_t<S>, hven::IpmOptions>
void for_each_option(S &o, F &&f) {
    constexpr std::string_view owner = "IpmOptions";
    const OptionCtx root{"", OptionEngine::kIpm, false};
#define X(m, r, d) f(OptionMeta{root.prefix, #m, d, (r), root.advanced, owner}, o.m);
#define G(m, t, adv)                                                                               \
    {                                                                                              \
        const std::string p = std::string(root.prefix) + #m + ".";                                 \
        visit_##t(o.m, OptionCtx{p, root.engine, root.advanced || (adv)}, f);                      \
    }
    HVEN_IPM_OPTIONS_FIELDS(X, G)
#undef G
#undef X
}

} // namespace hven::detail
