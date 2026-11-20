// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// iteration_record.h -- the trace-v1 iteration row, shared by both engines
// (docs/trace-schema-v1.md, `iter`). A row is the point plus the step taken
// FROM it; `std::nullopt` is a field the emitting site did not measure.

#include <optional>

#include <hven/core/types.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/step_verdict.h>

namespace hven {

/// @brief Which tier produced the step leaving the row's point.
enum class QpTraceMode { kIpqp, kWalk, kSsn };

/// @brief How an elastic ladder was entered.
enum class ElasticEntry {
    kVerifiedInfeasible,    ///< The main QP was verified infeasible in box and trust region.
    kWorkingSetTermination, ///< The main QP's working set ended numerically at a violated iterate.
    kRefusedCertificate,    ///< The main QP's infeasibility certificate was refused.
    kIpqpFallback,          ///< The certified fallback's rung A (kIpqp escape).
};

/// @brief One elastic ladder's outcome, beside (never in place of) the main QP's status.
struct ElasticLadderRow {
    ElasticEntry entry = ElasticEntry::kVerifiedInfeasible;
    QpStatus main_status = QpStatus::kOptimal; ///< The main QP's status, before the ladder.
    QpStatus rung_status = QpStatus::kOptimal; ///< The final rung's status.
    double rho = 0.0;                          ///< The final rung's penalty.
    Index rungs = 0;                           ///< Rungs climbed above the first.
    bool closed = false;
    bool reduced = false;
    bool promises_f = false;
    double slack_l1 = 0.0; ///< l1 of the final rung's violations; 0 unless it is kOptimal.
    bool exhausted = false;
};

enum class IterationTier { kBarrier, kIpqp, kSsn, kWalk, kElastic, kRestoration };

/// @brief The interior-point block of an iteration row.
struct IpmIterationRow {
    Index phase = 0;
    double mu = 0.0;
    std::optional<double> alpha_primal;
    std::optional<double> alpha_dual;
    std::optional<double> alpha_scale;
    std::optional<Index> line_search_trials;
    std::optional<double> regularization_primal;
    std::optional<double> regularization_primal_total;
    std::optional<double> regularization_dual;
    std::optional<double> proximal_primal;
    std::optional<Index> inertia_corrections;
    std::optional<Index> perturbed_pivots;
    std::optional<double> barrier_objective;
    double feasibility_eq = 0.0;
    double feasibility_ineq = 0.0;
    std::optional<double> merit;
    double merit_penalty_1 = 0.0;
    double merit_penalty_2 = 0.0;
    double max_multiplier_eq = 0.0;
    double max_multiplier_ineq = 0.0;
    std::optional<Index> first_rejected_trial;
    std::optional<double> theta_first_rejected;
    Index eval_exceptions = 0;
};

/// @brief The SQP block of an iteration row.
struct SqpIterationRow {
    Index depth = 0;
    QpTraceMode mode = QpTraceMode::kWalk;
    std::optional<StepVerdict> verdict;
    double trust_radius = 0.0;
    double violation_l1 = 0.0;
    std::optional<double> regularization_dual;
    bool qp_solved = false;
    std::optional<QpStatus> qp_status;
    std::optional<Index> qp_minor_iterations;
    std::optional<Index> qp_factorizations;
    bool tr_binding = false;
    bool elastic = false;
    bool elastic_clamped = false;
    bool restoration_seed_used = false;
    std::optional<double> ipqp_least_infeasible_primal;
    std::optional<bool> ipqp_farkas_corroborated;
    Index active_set_size = 0;
    Index active_set_delta = 0;
    Index weak_active_rows = 0;
    Index near_active_rows = 0;
    Index active_rows = 0;
    Index active_lower_sides = 0;
    Index active_upper_sides = 0;
    /// GL-0: pred_df / (delta * h^2) at the iterate, KLV Eq. (10); >= 1 selects an f-type trial.
    /// Present only when the row's QP status is kOptimal (the point the strategy was shown);
    /// nullopt otherwise, so a judged qp_failure_step row is null.
    std::optional<double> switching_ratio;
    /// The elastic ladder's outcome; present on a row whose major ran one.
    std::optional<ElasticLadderRow> ladder;
};

/// @brief One iteration row: the shared core plus exactly one engine block.
struct IterationRecord {
    Index iteration = 0;
    bool recovering = false;
    IterationTier tier = IterationTier::kBarrier;
    std::optional<double> f;
    std::optional<double> f_recovery;
    double stationarity = 0.0;
    double feasibility = 0.0;
    double complementarity = 0.0;
    std::optional<double> step_norm;
    std::optional<bool> accepted;
    bool soc = false;
    bool watchdog = false;
    bool rolled_back = false;
    Index factorizations = 0;
    std::optional<IpmIterationRow> ipm;
    std::optional<SqpIterationRow> sqp;
};

} // namespace hven
