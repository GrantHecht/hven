// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// iteration_record_eq.h -- test support: bit-exact equality of two `IterationRecord`s over EVERY
// member (a double compares by its bits, so a NaN equals itself and -0.0 differs from 0.0), and
// the same predicate over the optional blocks. A new member stops the build in the arity pins of
// trace_writer.cpp until it is added here.

#include <bit>
#include <cstdint>
#include <optional>
#include <type_traits>

#include <hven/drivers/iteration_record.h>

namespace hven::test {

template <class T> bool same_value(const T &a, const T &b) {
    if constexpr (std::is_floating_point_v<T>) {
        return std::bit_cast<std::uint64_t>(static_cast<double>(a)) ==
               std::bit_cast<std::uint64_t>(static_cast<double>(b));
    } else {
        return a == b;
    }
}

template <class T> bool same_value(const std::optional<T> &a, const std::optional<T> &b) {
    return a.has_value() == b.has_value() && (!a || same_value(*a, *b));
}

inline bool same_block(const IpmIterationRow &a, const IpmIterationRow &b) {
    return same_value(a.phase, b.phase) && same_value(a.mu, b.mu) &&
           same_value(a.alpha_primal, b.alpha_primal) && same_value(a.alpha_dual, b.alpha_dual) &&
           same_value(a.alpha_scale, b.alpha_scale) &&
           same_value(a.line_search_trials, b.line_search_trials) &&
           same_value(a.regularization_primal, b.regularization_primal) &&
           same_value(a.regularization_primal_total, b.regularization_primal_total) &&
           same_value(a.regularization_dual, b.regularization_dual) &&
           same_value(a.proximal_primal, b.proximal_primal) &&
           same_value(a.inertia_corrections, b.inertia_corrections) &&
           same_value(a.perturbed_pivots, b.perturbed_pivots) &&
           same_value(a.barrier_objective, b.barrier_objective) &&
           same_value(a.feasibility_eq, b.feasibility_eq) &&
           same_value(a.feasibility_ineq, b.feasibility_ineq) && same_value(a.merit, b.merit) &&
           same_value(a.merit_penalty_1, b.merit_penalty_1) &&
           same_value(a.merit_penalty_2, b.merit_penalty_2) &&
           same_value(a.max_multiplier_eq, b.max_multiplier_eq) &&
           same_value(a.max_multiplier_ineq, b.max_multiplier_ineq) &&
           same_value(a.first_rejected_trial, b.first_rejected_trial) &&
           same_value(a.theta_first_rejected, b.theta_first_rejected) &&
           same_value(a.eval_exceptions, b.eval_exceptions);
}

inline bool same_block(const SqpIterationRow &a, const SqpIterationRow &b) {
    return same_value(a.depth, b.depth) && a.mode == b.mode && a.verdict == b.verdict &&
           same_value(a.trust_radius, b.trust_radius) &&
           same_value(a.violation_l1, b.violation_l1) &&
           same_value(a.regularization_dual, b.regularization_dual) && a.qp_solved == b.qp_solved &&
           a.qp_status == b.qp_status && a.qp_minor_iterations == b.qp_minor_iterations &&
           a.qp_factorizations == b.qp_factorizations && a.tr_binding == b.tr_binding &&
           a.elastic == b.elastic && a.elastic_clamped == b.elastic_clamped &&
           a.restoration_seed_used == b.restoration_seed_used &&
           same_value(a.ipqp_least_infeasible_primal, b.ipqp_least_infeasible_primal) &&
           a.ipqp_farkas_corroborated == b.ipqp_farkas_corroborated &&
           a.active_set_size == b.active_set_size && a.active_set_delta == b.active_set_delta &&
           a.weak_active_rows == b.weak_active_rows && a.near_active_rows == b.near_active_rows &&
           a.active_rows == b.active_rows && a.active_lower_sides == b.active_lower_sides &&
           a.active_upper_sides == b.active_upper_sides;
}

template <class Block>
bool same_block(const std::optional<Block> &a, const std::optional<Block> &b) {
    return a.has_value() == b.has_value() && (!a || same_block(*a, *b));
}

/// Bit-exact equality over every member of the record and its engine block.
inline bool same_record(const IterationRecord &a, const IterationRecord &b) {
    return same_value(a.iteration, b.iteration) && a.recovering == b.recovering &&
           a.tier == b.tier && same_value(a.f, b.f) && same_value(a.f_recovery, b.f_recovery) &&
           same_value(a.stationarity, b.stationarity) && same_value(a.feasibility, b.feasibility) &&
           same_value(a.complementarity, b.complementarity) &&
           same_value(a.step_norm, b.step_norm) && a.accepted == b.accepted && a.soc == b.soc &&
           a.watchdog == b.watchdog && a.rolled_back == b.rolled_back &&
           a.factorizations == b.factorizations && same_block(a.ipm, b.ipm) &&
           same_block(a.sqp, b.sqp);
}

} // namespace hven::test
