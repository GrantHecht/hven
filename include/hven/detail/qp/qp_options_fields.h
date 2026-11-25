// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The options tables of `QpOptions` and `IpqpOptions` and their walkers.
///
/// Both structs sit under `SqpOptions` (`qp.` and `advanced.ipqp.`); see
/// `detail/core/options_registry.h` for the row grammar and the checks.

#include <string>

#include <hven/detail/core/options_registry.h>
#include <hven/qp/qp_options.h>
#include <hven/qp/qp_types.h>

// clang-format off
#define HVEN_QP_OPTIONS_FIELDS(X)                                                                  \
    X(primal_regularization, r_ge(0.0),                                                            \
      "Primal regularization magnitude on the (1,1) block of the walk's KKT matrix, centred at "   \
      "the iterate. 0 is a real setting (no regularization).")                                     \
    X(dual_regularization, r_ge(0.0),                                                              \
      "Dual regularization magnitude on the (2,2) block, so an over-determined working set "       \
      "stays solvable. 0 is a real setting.")                                                      \
    X(feasibility_tol, r_gt(0.0), "The QP's own feasibility tolerance.")                           \
    X(optimality_tol, r_gt(0.0), "The QP's own optimality tolerance.")                             \
    X(max_iterations, r_ge(1.0),                                                                   \
      "Minor-iteration cap for one QP solve. Unset derives the cap from the subproblem's size; "   \
      "an engaged value is an absolute cap.")                                                      \
    X(max_schur_border_dimension, r_ge(0.0),                                                       \
      "Largest Schur-complement border the walk admits before it refactorizes. 0 refactorizes "    \
      "on every working-set change.")                                                              \
    X(schur_condition_max, r_ge(1.0),                                                              \
      "Ceiling on the Schur border's condition estimate; beyond it the walk refactorizes.")        \
    X(working_set_algebra, r_any(),                                                                \
      "How the walk updates its factorization when the working set changes.")                      \
    X(trust_region_radius, r_ge(0.0, false),                                                       \
      "l-infinity trust-region radius about the solve's start point, fixed for the engine's "      \
      "lifetime. +inf disables it exactly.")                                                       \
    X(independent_working_set, r_any(),                                                            \
      "Keep the walk's working set linearly independent. False disables the check.")               \
    X(hessian_known_convex, r_any(),                                                               \
      "Caller assertion that H is positive semidefinite, accepted by the walk's convexity "        \
      "certificate in place of its structural test. Never verified.")

#define HVEN_IPQP_OPTIONS_FIELDS(X)                                                                \
    X(max_iterations, r_ge(1.0),                                                                   \
      "Iteration budget for the tier's Mehrotra loop. Unset derives it from the subproblem's "     \
      "size.")                                                                                     \
    X(hard_max_iterations, r_ge(1.0),                                                              \
      "The hard cap of last resort, never derived. Per attempt, not per subproblem.")              \
    X(max_factorizations, r_ge(1.0),                                                               \
      "Factorization budget for the tier, enforced before every factorization. Unset is 3 times "  \
      "the effective iteration budget.")                                                           \
    X(barrier_initial, r_gt(0.0),                                                                  \
      "The cold starting barrier parameter and the ceiling of the warm-restart mu_0 clamp. "       \
      "Must be >= barrier_min.")                                                                   \
    X(barrier_min, r_gt(0.0),                                                                      \
      "The floor of the mu_0 clamp and the tier's barrier-decay floor. Must be <= "                \
      "barrier_initial.")                                                                          \
    X(primal_regularization_initial,                                                               \
      r_text("finite, in [regularization_min, regularization_max]"),                               \
      "Initial primal regularization rho_0 of the proximal (rho, delta) schedule.")                \
    X(dual_regularization_initial, r_text("finite, in (0, regularization_max]"),                   \
      "Initial dual regularization delta_0 of the (rho, delta) schedule.")                         \
    X(regularization_min, r_gt(0.0),                                                               \
      "The floor the (rho, delta) ladder's gated decrease may never cross.")                       \
    X(regularization_max, r_text("finite, >= regularization_min"),                                 \
      "The ceiling the ladder's inertia-demanded growth may never cross.")                         \
    X(regularization_decrease, r_open(0.0, 1.0),                                                   \
      "The ladder's gated multiplicative decrease factor.")                                        \
    X(fraction_to_boundary, r_open(0.0, 1.0),                                                      \
      "Fraction-to-boundary parameter both the affine and corrector steps are clipped by.")        \
    X(face_ratio_threshold, r_open(0.0, 1.0),                                                      \
      "The ratio threshold kappa of the face-classification rule (active iff s < kappa z).")       \
    X(convergence_slack, r_ge(1.0),                                                                \
      "The barrier phase converges to this multiple of the QP layer's relative tolerances; "       \
      "the exact refinement owns the last decades.")                                               \
    X(ruiz_equilibration, r_any(), "Ruiz equilibration inside the tier, unscaled on export.")      \
    X(warm_repair, r_any(),                                                                        \
      "The warm-seed repair (positivity clamps plus a two-scalar shift) before a warm restart "    \
      "is trusted.")                                                                               \
    X(max_warm_iterations, r_ge(0.0),                                                              \
      "The warm-kill rule's iteration budget: a warm start overrunning it restarts cold once.")    \
    X(barrier_adoption_factor, r_ge(0.0),                                                          \
      "kappa_mu in the mu_0 clamp. 0 disables adoption of the payload's barrier value.")           \
    X(stall_window, r_ge(1.0), "The early-stall detector's window width, in accepted iterations.") \
    X(max_consecutive_escapes, r_ge(1.0),                                                          \
      "Consecutive escapes within one SQP solve after which the tier is retired for the "          \
      "remainder of that solve.")                                                                  \
    X(farkas_gate, r_any(),                                                                        \
      "Farkas corroboration of an infeasible-suspect escape. May arm the evidence block; never "   \
      "certifies.")                                                                                \
    X(hoist_symbolic_analysis, r_any(),                                                            \
      "Cross-major symbolic reuse. False forces a fresh symbolic pass every major.")               \
    X(require_final_inertia, r_any(),                                                              \
      "Require the final unregularized inertia read; false downgrades every certificate.")         \
    X(barrier_initial_rule, r_any(), "How the solve-entry cold start sets mu_0.")                  \
    X(barrier_residual_floor, r_any(),                                                             \
      "Raise the corrector target to a residual-proportional floor.")                              \
    X(ladder_skip_after, r_ge(0.0),                                                                \
      "The inertia ladder's zero trial is skipped once this many consecutive iterations needed "   \
      "a modification.")
// clang-format on

namespace hven::detail {

/// @brief Calls `f(const OptionMeta &, T &value)` for every field of a (const) `QpOptions`.
template <class S, class F> void visit_qp(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "QpOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_QP_OPTIONS_FIELDS(X)
#undef X
}

/// @brief The same for `IpqpOptions`.
template <class S, class F> void visit_ipqp(S &o, const OptionCtx &c, F &f) {
    constexpr std::string_view owner = "IpqpOptions";
#define X(m, r, d) f(OptionMeta{c.prefix, #m, d, (r), c.advanced, owner}, o.m);
    HVEN_IPQP_OPTIONS_FIELDS(X)
#undef X
}

} // namespace hven::detail
