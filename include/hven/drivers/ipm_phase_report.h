// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief What one phase of an interior-point solve did (`IpmPhaseReport`).
///
/// A header of its own so that the trace interface can name the report without
/// including the engine's options or result types.

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
    /// Why this phase left its iteration loop; `kNone` when the verdict is the
    /// whole explanation.
    IpmStopReason stop_reason = IpmStopReason::kNone;
    /// False for a conditional phase the sequence skipped.
    bool ran = false;
};

} // namespace hven
