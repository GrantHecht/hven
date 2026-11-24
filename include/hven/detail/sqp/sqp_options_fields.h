// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options tables of `SqpOptions` and its nested structs, and `for_each_option`.
///
/// One table per struct, one row per field, in declaration order; see
/// `detail/core/options_registry.h` for the row grammar and the checks. The `doc` strings are the
/// one-line statement the generated options reference prints; the header comments on the structs
/// carry the full contract. `QpOptions` and `IpqpOptions` are listed in
/// `detail/qp/qp_options_fields.h`.

#include <string>

#include <hven/detail/core/options_registry.h>
#include <hven/detail/qp/qp_options_fields.h>
#include <hven/detail/solver/common_options_fields.h>
#include <hven/detail/sqp/sqp_option_enum_names.h>
#include <hven/sqp/sqp_options.h>

// clang-format off
#define HVEN_SQP_LINEAR_OPTIONS_FIELDS(X)                                                          \
    X(dense_factorization, r_any(),                                                                \
      "When the dense factorization arm is used: kAuto takes it for each analyzed pattern of "     \
      "order n <= 105, the sparse backend above, so one solve can use both.")                      \
    X(max_dense_dimension, r_text("unset (reserved)"),                                             \
      "Reserved for a caller's kAuto threshold; only the empty value is accepted.")

#define HVEN_SQP_TERMINATION_OPTIONS_FIELDS(X)                                                     \
    X(acceptable_stationarity_tol, r_gt(0.0),                                                      \
      "Acceptable-level stationarity tolerance a failure exit's point is labelled against.")      \
    X(acceptable_equality_tol, r_gt(0.0), "Acceptable-level equality tolerance.")                  \
    X(acceptable_inequality_tol, r_gt(0.0),                                                        \
      "Acceptable-level inequality tolerance (bounds included).")                                  \
    X(acceptable_complementarity_tol, r_gt(0.0), "Acceptable-level complementarity tolerance.")

#define HVEN_SQP_ADVANCED_OPTIONS_FIELDS(X, G)                                                     \
    X(second_order_correction, r_any(),                                                            \
      "Second-order correction: a rejected trial whose violation increased gets one hot-started "  \
      "re-solve before the radius shrinks (the Maratos-effect defense).")                          \
    X(adaptive_dual_regularization, r_any(),                                                       \
      "Schedule the QP's dual regularization down with the KKT residual instead of leaving it "    \
      "at the engine's fixed value.")                                                              \
    X(make_strategy, r_text("empty (the funnel) or a factory returning non-null"),                 \
      "Factory for the globalization strategy, called once per solve. Empty means the funnel.")    \
    X(warm_full_step, r_any(),                                                                     \
      "The full-step-first warm rule: a warm solve begins in the funnel's full-step mode with a "  \
      "watchdog.")                                                                                 \
    X(elastic_ladder_early_exit, r_any(),                                                          \
      "Stop the elastic penalty ladder once an escalation leaves the solution where the "          \
      "previous rung left it.")                                                                    \
    X(crash_basis, r_any(),                                                                        \
      "Seed a cold solve's first QP working set from the activity geometry at x0.")                \
    X(elastic_rung_engine, r_any(), "Which engine solves the elastic ladder's rungs.")             \
    X(ipqp_reseed_after_disproval, r_any(),                                                        \
      "After a disproval, offer the fallback's certified dual blocks to the tier's next entry.")   \
    G(ipqp, ipqp, true)                                                                            \
    X(qp_dump_dir, r_text("empty (off) or a directory path; required when qp_dump_policy is on"),  \
      "Directory the walk's QP dump files are written to. Empty is off.")                          \
    X(qp_dump_policy, r_any(), "Which walk solves are dumped.")                                    \
    X(ssn_proximal_carry, r_any(),                                                                 \
      "Read the proximal carry off an ingested SqpWarmStart for the first SSN subproblem.")        \
    X(ssn_certify_from_face, r_any(),                                                              \
      "Read the certifying exit's second-order evidence off the face-EQP factorization, saving "   \
      "one factorization.")                                                                        \
    X(ssn_sigma_rule, r_any(), "The SSN regularization (sigma) rule.")                             \
    X(ssn_hint_rule, r_any(), "How the hinted first SSN step is protected.")                       \
    X(ssn_infeasibility_rule, r_any(),                                                             \
      "What turns an SSN infeasibility suspicion into an exit.")                                   \
    X(max_consecutive_qp_failures, r_ge(1.0),                                                      \
      "Subproblem failures in a row the driver routes at one iterate before the next one ends "    \
      "the solve.")                                                                                \
    X(qp_iteration_limit_retry, r_any(),                                                           \
      "Re-solve a subproblem that spent its whole minor budget, once, at twice the cap.")          \
    X(qp_failure_step, r_any(),                                                                    \
      "Judge a failed walk subproblem's usable iterate as a trial step.")                          \
    X(restoration_proximity, r_ge(0.0),                                                            \
      "Restoration proximity weight zeta on 1/2 zeta ||D (x - x_k)||^2. 0 is off. "                \
      "Experimental. Refused with kLbfgsDamped when > 0.")                                         \
    X(equality_basis, r_any(),                                                                     \
      "Split the equality rows into an independent set and a dependent set; every QP carries "     \
      "only the independent set.")                                                                 \
    X(elastic_witness_start, r_any(), "Start each elastic walk rung at the feasible witness point.") \
    X(elastic_last_optimal_rung, r_any(),                                                          \
      "A ladder rung that fails above a solved rung reports the solved rung instead.")             \
    X(restoration_resume_reduction, r_closed_open(0.0, 1.0),                                       \
      "Sufficient-reduction resume fraction of the entry violation. 0 is off.")                    \
    X(restoration_stall_factorizations, r_ge(0.0),                                                 \
      "The restoration phase's stall window: the phase ends when its best violation has not "     \
      "fallen by 10% over this many factorizations and at least five passes; a window of 0 "      \
      "means after five passes.")                                                                 \
    X(lbfgs_update_on_reject, r_any(),                                                             \
      "Under kLbfgsDamped, also offer the pair of a rejected trial.")                              \
    X(lbfgs_nonlinear_only, r_any(),                                                               \
      "Under kLbfgsDamped, let B act on the nonlinear variables only.")                            \
    X(max_lbfgs_full_memory_dimension, r_ge(0.0),                                                  \
      "Under kLbfgsDamped, a full-memory damped BFGS replaces the L-BFGS window at or below "      \
      "this dimension. 0 is never.")                                                               \
    X(restoration_curvature, r_any(), "The curvature the restoration phase's QPs carry.")          \
    X(max_restoration_entries, r_ge(0.0),                                                          \
      "Restoration entries allowed per solve. 0 refuses restoration.")                             \
    X(trust_region_growth, r_any(),                                                                \
      "When an accepted step earns the trust-region radius a larger value.")                       \
    X(trust_region_growth_after_soc, r_any(),                                                      \
      "Let an acceptance through second-order correction grow the trust-region radius.")          \
    X(trust_region_rejection_memory, r_ge(0.0),                                                    \
      "Refuse a growth reaching the smallest radius a grown step was rejected at until this many " \
      "acceptances pass. 0 is off.")                                                               \
    X(trust_region_ratio_ceiling, r_gt(1.0, false),                                                \
      "Refuse a ratio-granted growth whose ratio exceeds this. +inf is off.")                      \
    X(trust_region_shrink_to_step, r_any(),                                                        \
      "On a rejection whose step the radius did not hold, shrink to half the step's inf-norm.")   \
    X(trust_region_growth_on_cap_pins, r_any(),                                                    \
      "Count a step held only by caller step-limit pins as growth-eligible.")                      \
    X(trust_region_funnel_headroom, r_any(),                                                       \
      "Refuse a growth whose predicted violation at twice the radius leaves the funnel width.")    \
    X(ssn_farkas_residual_tol, r_open(0.0, 1.0),                                                   \
      "SSN Farkas certificate's relative-residual threshold (kFarkasGated only).")                 \
    X(ssn_uncertain_tol, r_closed_open(0.0, 1.0),                                                  \
      "SSN uncertain-set entering threshold on |alpha - beta|. 0 disables the set.")               \
    X(ssn_lm_sigma_c, r_gt(0.0),                                                                   \
      "SSN Levenberg-Marquardt sizing coefficient c (residual sigma rules only).")                 \
    X(ssn_lm_sigma_exponent, r_closed(1.0, 2.0),                                                   \
      "SSN Levenberg-Marquardt sizing exponent p in c * min(r, r^p).")                             \
    X(restoration_elastic_seed, r_any(),                                                           \
      "Offer the elastic ladder's step as a restoration candidate at the ladder-unusable site.")   \
    X(elastic_rho0_headroom_rungs, r_closed(1.0, 6.0),                                             \
      "Rungs of the penalty ladder the evidence-placed first rung leaves below the ceiling.")      \
    X(adaptive_dual_regularization_max, r_closed(1e-12, 1.0),                                      \
      "Ceiling of the adaptive dual-regularization schedule.")

#define HVEN_SQP_OPTIONS_FIELDS(X, G)                                                              \
    G(common, common, false)                                                                       \
    G(termination, sqp_termination, false)                                                         \
    X(trust_region_initial, r_gt(0.0, false),                                                      \
      "The l-infinity trust-region radius the first subproblem is given. +inf means no trust "     \
      "region.")                                                                                   \
    X(trust_region_max, r_text("> 0; >= trust_region_initial unless that is +inf"),                \
      "Ceiling the growth rule expands the radius toward; the value a +inf initial radius's "      \
      "first shrink lands on.")                                                                    \
    X(trust_region_min, r_gt(0.0),                                                                 \
      "The radius floor: a shrink below it raises a restoration request. Must be <= "              \
      "trust_region_initial and trust_region_max.")                                                \
    X(caller_step_limit, r_ge(0.0),                                                                \
      "Caller-unit step limit r: every QP's variable box is narrowed to |dx_j| <= r * (1 + "       \
      "|x_j|). 0 is off.")                                                                         \
    G(qp, qp, false)                                                                               \
    X(qp_mode, r_any(), "Which QP kernel the driver's subproblems go through.")                    \
    X(row_scaling, r_any(),                                                                        \
      "The problem-scaling layer: solve a diagonally rescaled problem, results mapped back to "    \
      "caller units.")                                                                             \
    X(scaling_max_gradient, r_gt(0.0),                                                             \
      "The inf-norm the scaled objective gradient and each scaled Jacobian row are aimed at.")     \
    X(scaling_factor_max, r_ge(1.0),                                                               \
      "Symmetric clamp on every row-layer factor, to [1/limit, limit].")                           \
    X(row_scaling_power_of_two, r_any(), "Round each row factor to the nearest power of two.")     \
    X(column_scaling, r_any(), "The column (variable) layer of problem scaling.")                  \
    X(column_scaling_threshold, r_ge(1.0),                                                         \
      "kAuto's gate: the column factors' max/min spread at which a problem counts as badly "       \
      "scaled.")                                                                                   \
    X(column_scaling_factor_max, r_ge(1.0),                                                        \
      "Symmetric clamp on the column factors under kAlways.")                                      \
    X(project_start_point, r_any(),                                                                \
      "Project the start point onto the declared box, or take it verbatim.")                       \
    X(hessian_mode, r_any(), "The curvature source of the subproblems.")                           \
    X(lbfgs_memory, r_ge(1.0), "The L-BFGS window length m under kLbfgsDamped.")                   \
    G(linear, sqp_linear, false)                                                                   \
    G(advanced, sqp_advanced, true)
// clang-format on

namespace hven::detail {

template <class S, class F> void visit_sqp_linear(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "SqpLinearOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_SQP_LINEAR_OPTIONS_FIELDS(X)
#undef X
}

template <class S, class F> void visit_sqp_termination(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "SqpTerminationOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_SQP_TERMINATION_OPTIONS_FIELDS(X)
#undef X
}

template <class S, class F> void visit_sqp_advanced(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "SqpAdvancedOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
#define G(m, t, adv)                                                                               \
    {                                                                                              \
        const std::string p = std::string(c.prefix) + #m + ".";                                    \
        visit_##t(o.m, OptionCtx{p, c.engine, c.advanced || (adv)}, f);                            \
    }
    HVEN_SQP_ADVANCED_OPTIONS_FIELDS(X, G)
#undef G
#undef X
}

/// @brief Calls `f(const OptionMeta &, T &value)` for every field of @p o, in declaration order, a
///        nested struct flattened under its member name (`common.`, `qp.`, `linear.`,
///        `advanced.`, `advanced.ipqp.`).
///
/// Typed access, for the dump, the metadata and the bindings; there is no string-to-field setter.
/// @p o may be const.
template <class S, class F>
    requires std::is_same_v<std::remove_const_t<S>, hven::SqpOptions>
void for_each_option(S &o, F &&f) {
    constexpr std::string_view owner = "SqpOptions";
    const OptionCtx root{"", OptionEngine::kSqp, false};
#define X(m, r, d) f(OptionMeta{root.prefix, #m, d, (r), root.advanced, owner}, o.m);
#define G(m, t, adv)                                                                               \
    {                                                                                              \
        const std::string p = std::string(root.prefix) + #m + ".";                                 \
        visit_##t(o.m, OptionCtx{p, root.engine, root.advanced || (adv)}, f);                      \
    }
    HVEN_SQP_OPTIONS_FIELDS(X, G)
#undef G
#undef X
}

} // namespace hven::detail
