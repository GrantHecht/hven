// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// ipqp_options.cpp -- boundary validation of IpqpOptions (qp/qp_options.h).
//
// Carved out of src/sqp/sqp_options.cpp verbatim (the thrown messages keep their "SqpSolver:
// advanced.ipqp.<field>" spelling: that is the path a caller sets the field by). The same premise
// as that file's header applies and is not restated: every check is the NEGATION of the
// acceptance condition so that NaN is rejected, which relies on SAFER_FAST's
// `-fno-finite-math-only`. Nothing in this file computes with a floating-point value.

#include <cmath>
#include <optional>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/qp/qp_options.h>

namespace hven {

namespace {

template <typename E> void enum_in_range(E v, E last, const char *name) {
    const auto i = static_cast<long long>(v);
    if (i < 0 || i > static_cast<long long>(last)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: {} ({}) is not one of its enumerators", name, i));
    }
}

} // namespace

void validate(const IpqpOptions &opts) {
    // Validated UNCONDITIONALLY: an options object is a value whose qp_mode a caller may set
    // later, so a nonsensical field was already nonsensical when it was set. The `Index` fields
    // use the direct form (an Index has no NaN to guard against); every `double` field uses the
    // negation-of-acceptance idiom for the NaN-rejection reason in the file banner.
    if (opts.hard_max_iterations <= 0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.hard_max_iterations ({}) must be > 0; it is the budget of "
            "last "
            "resort and, unlike max_iterations, carries no size-derived sentinel reading",
            opts.hard_max_iterations));
    }
    if (!(opts.barrier_min > 0.0) || std::isinf(opts.barrier_min)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.barrier_min ({}) must be finite and > 0; it is the "
            "floor of the mu_0 clamp and the tier's own barrier-decay floor",
            opts.barrier_min));
    }
    if (!(opts.barrier_initial > 0.0) || std::isinf(opts.barrier_initial)) {
        throw std::invalid_argument(fmt::format("SqpSolver: advanced.ipqp.barrier_initial ({}) "
                                                "must be finite and > 0; it is the cold starting "
                                                "mu and the ceiling of the mu_0 clamp",
                                                opts.barrier_initial));
    }
    if (!(opts.barrier_min <= opts.barrier_initial)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.barrier_min ({}) must be <= "
                        "advanced.ipqp.barrier_initial ({}); the mu_0 "
                        "clamp's band [min_mu, init_mu] is otherwise inverted",
                        opts.barrier_min, opts.barrier_initial));
    }
    if (!(opts.regularization_min > 0.0) || std::isinf(opts.regularization_min)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.regularization_min ({}) must be finite and > 0; "
                        "it is the absolute "
                        "floor the (rho, delta) ladder's gated decrease may never cross",
                        opts.regularization_min));
    }
    if (!(opts.regularization_max >= opts.regularization_min) ||
        std::isinf(opts.regularization_max)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.regularization_max ({}) must be finite and >= "
                        "advanced.ipqp.regularization_min ({}); "
                        "it is the ceiling the (rho, delta) ladder's growth may never cross",
                        opts.regularization_max, opts.regularization_min));
    }
    if (!(opts.primal_regularization_initial >= opts.regularization_min) ||
        !(opts.primal_regularization_initial <= opts.regularization_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.primal_regularization_initial ({}) must lie in "
            "[regularization_min ({}), regularization_max "
            "({})]; the starting value of a quantity the ladder only ever moves within that "
            "band must itself start inside it",
            opts.primal_regularization_initial, opts.regularization_min, opts.regularization_max));
    }
    if (!(opts.dual_regularization_initial > 0.0) ||
        !(opts.dual_regularization_initial <= opts.regularization_max)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.dual_regularization_initial ({}) must lie in (0, "
            "regularization_max ({})]; unlike "
            "primal_regularization_initial it is NOT tied to regularization_min -- spec section "
            "2.2's monotone "
            "floor is stated for rho only",
            opts.dual_regularization_initial, opts.regularization_max));
    }
    if (!(opts.regularization_decrease > 0.0) || !(opts.regularization_decrease < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.regularization_decrease ({}) must lie strictly in (0, 1); it "
            "is a "
            "gated multiplicative decrease -- 0 would collapse the regularization in one "
            "gated step and >= 1 would never decrease it",
            opts.regularization_decrease));
    }
    if (!(opts.fraction_to_boundary > 0.0) || !(opts.fraction_to_boundary < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.fraction_to_boundary ({}) must lie strictly in (0, 1); it is "
            "the "
            "fraction-to-boundary parameter -- 0 permits no step and >= 1 permits stepping "
            "onto or past a bound",
            opts.fraction_to_boundary));
    }
    if (!(opts.face_ratio_threshold > 0.0) || !(opts.face_ratio_threshold < 1.0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.face_ratio_threshold ({}) must lie strictly in (0, 1); a "
            "non-positive "
            "ratio-rule threshold can never classify a row active, and at kappa >= 1 the active "
            "test (s_j < kappa*z_j) and the inactive test (z_j < kappa*s_j) can no longer "
            "partition: at kappa == 1 an exact tie (s_j == z_j) satisfies NEITHER strict "
            "inequality (both tests false, so every tie becomes UNCERTAIN with no genuine "
            "margin), and above 1 the two tests can both fire on the same pair (a contradictory "
            "active-AND-inactive verdict)",
            opts.face_ratio_threshold));
    }
    if (!(opts.convergence_slack >= 1.0) || std::isinf(opts.convergence_slack)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.convergence_slack ({}) must be finite and >= 1; "
                        "a slack below "
                        "1 would ask the barrier phase for more accuracy than tier 3's own finish, "
                        "inverting the two-tier division of labor",
                        opts.convergence_slack));
    }
    if (opts.max_warm_iterations < 0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.max_warm_iterations ({}) must be >= 0; 0 is legal (every "
            "warm "
            "restart is killed on its first iteration) but a negative budget is not a count",
            opts.max_warm_iterations));
    }
    if (!(opts.barrier_adoption_factor >= 0.0) || std::isinf(opts.barrier_adoption_factor)) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.barrier_adoption_factor ({}) must be finite and "
                        ">= 0; 0 is a legal, "
                        "deliberate reading (adoption disabled), a negative one is not",
                        opts.barrier_adoption_factor));
    }
    if (opts.stall_window <= 0) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.stall_window ({}) must be > 0; a zero-width window can "
            "never accumulate the whole-window evidence the early-stall test is built on",
            opts.stall_window));
    }
    if (opts.max_consecutive_escapes <= 0) {
        throw std::invalid_argument(
            fmt::format("SqpSolver: advanced.ipqp.max_consecutive_escapes ({}) must be > 0; "
                        "retiring \"after zero "
                        "consecutive escapes\" is not a count, it disables the tier outright",
                        opts.max_consecutive_escapes));
    }
    // A2 (M7-37): the two shipped values only; any out-of-range cast is refused by name.
    // `mu_residual_floor` is a bool: it has no refusable value and therefore no predicate.
    enum_in_range(opts.barrier_initial_rule, IpqpBarrierInitialRule::kComplementarity,
                  "advanced.ipqp.barrier_initial_rule");
    // The two size-derived budgets: empty derives them; an engaged value is a count.
    if (opts.max_iterations && *opts.max_iterations < 1) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.max_iterations ({}) must be empty (size-derived) or >= 1",
            *opts.max_iterations));
    }
    if (opts.max_factorizations && *opts.max_factorizations < 1) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.max_factorizations ({}) must be empty (derived) or >= 1",
            *opts.max_factorizations));
    }
    // A3 (M7-38): 0 is legal (skip whenever a memory exists); a count cannot be negative.
    if (!(opts.ladder_skip_after >= 0)) {
        throw std::invalid_argument(fmt::format(
            "SqpSolver: advanced.ipqp.ladder_skip_after ({}) must be >= 0; it counts consecutive "
            "modified iterations, and 0 already means \"skip the zero trial whenever the ladder "
            "has a memory\"",
            opts.ladder_skip_after));
    }
}

} // namespace hven
