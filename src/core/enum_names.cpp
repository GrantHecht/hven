// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Display-string switches for SqpStatus (core/solver_status.h) and StartLevel
// (core/start_level.h), the two solver enums whose headers live in core/.
// Defined here so a core/ TU never links against a drivers/ object.
// FP-arithmetic-free: no double is read, written or compared in this file.

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>

namespace hven::solvers {

const char *to_string(SqpStatus status) {
    switch (status) {
    case SqpStatus::kOptimal:
        return "Optimal";
    case SqpStatus::kMaxIter:
        return "MaxIter";
    case SqpStatus::kInfeasible:
        return "Infeasible";
    case SqpStatus::kNumericalError:
        return "NumericalError";
    case SqpStatus::kBudgetExhausted:
        return "BudgetExhausted";
    }
    return "Unknown";
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
