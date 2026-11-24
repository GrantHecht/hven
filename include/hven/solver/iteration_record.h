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
    /// The main QP's status, before the ladder; under kIpqpFallback the interior-point tier's
    /// status at its escape.
    QpStatus main_status = QpStatus::kOptimal;
    QpStatus rung_status = QpStatus::kOptimal; ///< The final rung's status.
    double rho = 0.0;                          ///< The final rung's penalty.
    Index rungs = 0;                           ///< Rungs climbed above the first.
    bool closed = false;
    bool reduced = false;
    bool promises_f = false;
    double slack_l1 = 0.0; ///< l1 of the final rung's violations; 0 unless it is kOptimal.
    bool exhausted = false;
    /// The first rung's penalty of the surviving ladder, as the placement rule resolved it (`rho`
    /// is the final rung's); after a certified fallback's floor retry, the floor. Report-only.
    double rho_0 = 0.0;
    /// The ladder's own work -- QP minor iterations and factorizations summed over every rung it
    /// solved (a certified fallback's floor retry included). The row's `qp_minor_iterations` /
    /// `qp_factorizations` are those of the rung that supplied the step only.
    Index minor_iterations = 0;
    Index factorizations = 0;
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

/// The growth levers (SqpAdvancedOptions::trust_region_growth_*, trust_region_rejection_memory,
/// trust_region_growth_after_soc) that refused a growth the trust-region rule had granted, as a bit
/// set (SqpIterationRow::trust_region_growth_refusal_reasons).
enum class SqpTrustRegionGrowthRefusal : unsigned {
    kNone = 0,
    kViolationRatio = 1,  ///< kRatioByStepType's violation-ratio test on an h-type step
    kAfterSoc = 2,        ///< trust_region_growth_after_soc
    kRejectionMemory = 4, ///< trust_region_rejection_memory
    kRatioCeiling = 8,    ///< trust_region_growth_ratio_max
    kStepLimitPins = 16,  ///< trust_region_growth_on_step_limit
    kFunnelHeadroom = 32, ///< trust_region_growth_funnel_guard
};

constexpr SqpTrustRegionGrowthRefusal operator|(SqpTrustRegionGrowthRefusal a,
                                                SqpTrustRegionGrowthRefusal b) {
    return static_cast<SqpTrustRegionGrowthRefusal>(static_cast<unsigned>(a) |
                                                    static_cast<unsigned>(b));
}
constexpr SqpTrustRegionGrowthRefusal &operator|=(SqpTrustRegionGrowthRefusal &a,
                                                  SqpTrustRegionGrowthRefusal b) {
    return a = a | b;
}

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
    /// The trust RADIUS limited the step (a variable held by the radius's own pin). A
    /// `caller_step_limit` pin is `cap_binding`, not this; radius OR cap is `tr_binding ||
    /// cap_binding`.
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

    // --- Step telemetry: REPORT-ONLY. No decision reads any of it. ---

    /// A `caller_step_limit` pin was active on the step (see `tr_binding`).
    bool cap_binding = false;
    /// The model's predicted objective decrease for the QP step, the number the switching test
    /// and the radius-growth test read (caller objective scale). Present where `switching_ratio`
    /// is (the row's QP solved to kOptimal).
    std::optional<double> pred_df;
    /// Objective and l1 constraint violation at the trial point x + p (caller scale); present
    /// where `pred_df` is.
    std::optional<double> f_trial;
    std::optional<double> h_trial;
    /// The same two at the second-order-correction point; present only when a SOC re-solve
    /// solved and its point was evaluated.
    std::optional<double> soc_f;
    std::optional<double> soc_h;
    /// The l1 violation of the QP's own linearized rows at the step p, in `h_trial`'s units; null
    /// when the QP's rows are not the model's rows (equality-basis reduction) or p is not finite.
    std::optional<double> h_lin;
    /// QP minor iterations and factorizations charged since the previous row, EXCLUDING a
    /// restoration phase's folded work: the main QP, any cap retry, every ladder rung and the SOC
    /// re-solve of this row.
    Index qp_minor_iterations_total = 0;
    Index qp_factorizations_total = 0;
    /// The change in the solve's running `factorizations` since the previous row of this solve
    /// (the record's `factorizations` is the running total).
    Index factorizations_delta = 0;
    /// Seconds since this solve's entry on a monotonic clock; informational, never asserted. A
    /// depth-1 row counts from the nested solve's own entry.
    double t_s = 0.0;
    /// The SqpAdvancedOptions::trust_region_growth_* levers that refused this accepted step's
    /// growth (a bit set; kNone when no candidate growth was refused, and on every row that was
    /// not accepted).
    SqpTrustRegionGrowthRefusal trust_region_growth_refusal_reasons =
        SqpTrustRegionGrowthRefusal::kNone;
};

/// @brief One iteration row: the shared core plus exactly one engine block.
struct IterationRecord {
    Index iteration = 0;
    bool recovering = false;
    IterationTier tier = IterationTier::kBarrier;
    std::optional<double> f;
    std::optional<double> f_recovery;
    /// The three measures in the CALLER's units (the objective scale and, on an SQP solve,
    /// the row/column scaling mapped back). Schema keys `stationarity_caller`,
    /// `feasibility_caller`, `complementarity_caller`.
    double stationarity = 0.0;
    double feasibility = 0.0;
    double complementarity = 0.0;
    /// The three measures EXACTLY AS THE CONVERGENCE TEST COMPARES THEM with
    /// `optimality_tol` / `feasibility_tol` (the scaled problem's units; equal to the caller
    /// values on an unscaled solve). The console columns `stat feas comp` and the schema
    /// keys `stationarity`, `feasibility`, `complementarity` print these.
    double tested_stationarity = 0.0;
    double tested_feasibility = 0.0;
    double tested_complementarity = 0.0;
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
