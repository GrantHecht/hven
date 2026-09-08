// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Display-string switches, and the reporting order, for the solver enums whose
// headers live in core/: SolveStatus and IpmStopReason (core/solver_status.h)
// and StartLevel (core/start_level.h). Defined here so a core/ TU never links
// against a drivers/ object -- core/ledger.cpp calls to_string(SolveStatus).
// FP-arithmetic-free: no double is read, written or compared in this file.

#include <stdexcept>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>

namespace hven::solvers {

// to_string(SqpStatus) stood here until M6 W5 T8.4, returning the CAPITALISED
// display names Optimal / MaxIter / Infeasible / NumericalError /
// BudgetExhausted. SqpStatus is gone and to_string(SolveStatus)
// (src/drivers/solve_status.cpp) spells its nine in lower snake, so the
// capitalised vocabulary has exactly one consumer left: the corpus and
// crossover CSV writers, whose committed baselines and control pin those bytes.
// It lives there now, as bench-local corpus::legacy_status_string(), and its
// rename to the lower-case vocabulary is a declared re-derivation of four
// baselines and ~10 pins in its own task.

const char *to_string(SolveStatus status) {
    switch (status) {
    case SolveStatus::kOptimal:
        return "optimal";
    case SolveStatus::kAcceptable:
        return "acceptable";
    case SolveStatus::kMaxIter:
        return "max_iter";
    case SolveStatus::kInfeasible:
        return "infeasible";
    case SolveStatus::kStalled:
        return "stalled";
    case SolveStatus::kDiverging:
        return "diverging";
    case SolveStatus::kNumericalError:
        return "numerical_error";
    case SolveStatus::kBudgetExhausted:
        return "budget_exhausted";
    case SolveStatus::kInterrupted:
        return "interrupted";
    }
    throw std::invalid_argument(
        fmt::format("to_string(SolveStatus): unrecognized value ({})", static_cast<int>(status)));
}

const char *to_string(IpmStopReason reason) {
    switch (reason) {
    case IpmStopReason::kNone:
        return "none";
    case IpmStopReason::kIterationCap:
        return "iteration_cap";
    case IpmStopReason::kRestorationLocallyInfeasible:
        return "restoration_locally_infeasible";
    case IpmStopReason::kStageStalled:
        return "stage_stalled";
    }
    throw std::invalid_argument(
        fmt::format("to_string(IpmStopReason): unrecognized value ({})", static_cast<int>(reason)));
}

int severity(SolveStatus status) {
    // The documented reporting order, which is NOT the enumerator order: a
    // caller comparing two outcomes wants "how bad", not "declared where".
    switch (status) {
    case SolveStatus::kOptimal:
        return 0;
    case SolveStatus::kAcceptable:
        return 1;
    case SolveStatus::kInterrupted:
        return 2;
    case SolveStatus::kMaxIter:
        return 3;
    case SolveStatus::kBudgetExhausted:
        return 4;
    case SolveStatus::kStalled:
        return 5;
    case SolveStatus::kInfeasible:
        return 6;
    case SolveStatus::kDiverging:
        return 7;
    case SolveStatus::kNumericalError:
        return 8;
    }
    throw std::invalid_argument(
        fmt::format("severity(SolveStatus): unrecognized value ({})", static_cast<int>(status)));
}

const char *to_string(StartLevel level) {
    switch (level) {
    case StartLevel::kCold:
        return "Cold";
    case StartLevel::kSeeded:
        return "Seeded";
    case StartLevel::kWarm:
        return "Warm";
    case StartLevel::kHot:
        return "Hot";
    }
    return "Unknown";
}

} // namespace hven::solvers
