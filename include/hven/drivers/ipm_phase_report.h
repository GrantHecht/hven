// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief What one phase of an interior-point solve did (`IpmPhaseReport`).
///
/// A header of its own so that the trace interface can name the report without
/// including the engine's options or result types.

#include <optional>

#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/drivers/ipm_enums.h>

namespace hven {

/// @brief What one phase of a solve did.
///
/// One entry per phase in `IpmOptions::phases`, in that order, including the
/// phases that did not run: `ran` tells them apart, and a skipped phase's other
/// fields keep their defaults.
struct IpmPhaseReport {
    /// Which phase this is; equal to `IpmOptions::phases[i]` for entry i.
    IpmPhase phase = IpmPhase::kOptimize;
    /// This phase's own verdict, resolved against its own stop reason.
    SolveStatus status = SolveStatus::kNumericalError;
    /// Iterations this phase took; 0 when it did not run.
    Index iterations = 0;
    /// Wall-clock seconds inside this phase. Informational.
    double phase_seconds = 0.0;
    /// Why this phase left its iteration loop; `kNone` when the status names
    /// the cause (kOptimal, kAcceptable and the three limits among them), on
    /// SolveResult::stop_reason's rule.
    StopReason stop_reason = StopReason::kNone;
    /// False for a conditional phase the sequence skipped.
    bool ran = false;

    // This phase's own diagnostics. Each is nullopt when the selected strategy
    // does not report it, or when nothing ran in the phase to report.

    /// Final funnel width (tau); nullopt unless the acceptance strategy is the funnel.
    std::optional<double> funnel_width;
    /// Final filter size; nullopt unless the acceptance strategy is the filter.
    std::optional<Index> filter_size;
    /// Filter-reset-heuristic clears; nullopt unless the strategy is the filter.
    std::optional<Index> filter_resets;
    /// Free -> monotone handoffs; nullopt unless the governor is kMonitored.
    std::optional<Index> monotone_switches;
    /// Iterations spent in monotone mode; nullopt unless the governor is kMonitored.
    std::optional<Index> monotone_iterations;
    /// Restoration entries in this phase; nullopt when no strategy is constructed.
    std::optional<Index> restoration_entries;
    /// Iterations in restoration mode in this phase; nullopt when no strategy is constructed.
    std::optional<Index> restoration_iters;
    /// Proximal primal shift at the last factorized iteration; nullopt unless the
    /// proximal inertia mode is on.
    std::optional<double> proximal_regularization_primal;
    /// Proximal dual shift, on the same rule.
    std::optional<double> proximal_regularization_dual;
    /// The last non-Success KKT factorization status observed in this phase.
    IpmKktFactorStatus kkt_factor_status = IpmKktFactorStatus::kSuccess;
};

} // namespace hven
