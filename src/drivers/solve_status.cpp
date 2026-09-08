// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The shared status vocabulary's switches. A TU rather than inline bodies, on
// the same footing as core/enum_names.cpp: display strings and mappings are
// orchestration, not a per-element hot path (CLAUDE.md §5).

#include <hven/drivers/solve_status.h>

#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers {

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

SolveStatus to_solve_status(hven::ConvergenceFlags flag, IpmStopReason reason) {
    // The verdict is read first, so a stronger one the convergence check already
    // holds survives a stall recorded in the same iteration.
    switch (flag) {
    case hven::ConvergenceFlags::CONVERGED:
        return SolveStatus::kOptimal;
    case hven::ConvergenceFlags::ACCEPTABLE:
        return SolveStatus::kAcceptable;
    case hven::ConvergenceFlags::DIVERGING:
        return SolveStatus::kDiverging;
    case hven::ConvergenceFlags::SINGULAR_KKT:
        return SolveStatus::kNumericalError;
    case hven::ConvergenceFlags::NOTCONVERGED:
        switch (reason) {
        case IpmStopReason::kStageStalled:
        case IpmStopReason::kRestorationLocallyInfeasible:
            return SolveStatus::kStalled;
        case IpmStopReason::kIterationCap:
        case IpmStopReason::kNone:
            // kNone here is the defensive default: the engine labels every
            // NOTCONVERGED exit it takes, so an unlabelled one is the cap.
            return SolveStatus::kMaxIter;
        }
        throw std::invalid_argument(fmt::format("to_solve_status: unrecognized IpmStopReason ({})",
                                                static_cast<int>(reason)));
    }
    throw std::invalid_argument(
        fmt::format("to_solve_status: unrecognized ConvergenceFlags ({})", static_cast<int>(flag)));
}

SolveStatus to_solve_status(SqpStatus status) {
    switch (status) {
    case SqpStatus::kOptimal:
        return SolveStatus::kOptimal;
    case SqpStatus::kMaxIter:
        return SolveStatus::kMaxIter;
    case SqpStatus::kInfeasible:
        return SolveStatus::kInfeasible;
    case SqpStatus::kNumericalError:
        return SolveStatus::kNumericalError;
    case SqpStatus::kBudgetExhausted:
        return SolveStatus::kBudgetExhausted;
    }
    throw std::invalid_argument(
        fmt::format("to_solve_status: unrecognized SqpStatus ({})", static_cast<int>(status)));
}

} // namespace hven::solvers
