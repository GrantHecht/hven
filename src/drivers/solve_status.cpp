// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The interior-point engine's phase-status resolution. A TU rather than an
// inline body, on core/enum_names.cpp's footing: a switch over enums is
// orchestration, not a per-element hot path (CLAUDE.md §5).
//
// The vocabulary's own switches -- to_string and severity -- moved down to
// src/core/enum_names.cpp with the enums themselves in M6 W5 T8.4.

#include <hven/detail/drivers/ipm_phase_status.h>

#include <stdexcept>

#include <fmt/format.h>

namespace hven {

SolveStatus detail::resolve_ipm_phase_status(SolveStatus raw_verdict, IpmPhaseStop reason) {
    // THE VERDICT IS READ FIRST, so a stronger one the convergence check already
    // holds -- kOptimal or kAcceptable -- survives a stall recorded in the same
    // iteration. Only the engine's own "ran out of iterations with nothing
    // better to say" verdict is open to a stop reason's reinterpretation.
    if (raw_verdict != SolveStatus::kIterationLimit) {
        return raw_verdict;
    }
    switch (reason) {
    case IpmPhaseStop::kCallbackStop:
    case IpmPhaseStop::kStopRequested:
        // THE CALLER STOPPED IT (its callback or its stop token), and that outranks the cap the
        // loop would otherwise have been labelled with -- but only from kIterationLimit, so the
        // read above still lets a converged or acceptable verdict stand. "The
        // stop is a caller's decision, not a verdict" cuts both ways: it
        // renames a NOTCONVERGED exit and it overrides nothing.
        return SolveStatus::kInterrupted;
    case IpmPhaseStop::kStageStalled:
    case IpmPhaseStop::kRestorationLocallyInfeasible:
        return SolveStatus::kStalled;
    case IpmPhaseStop::kWorkLimit:
        return SolveStatus::kWorkLimit;
    case IpmPhaseStop::kTimeLimit:
        return SolveStatus::kTimeLimit;
    case IpmPhaseStop::kNonFiniteStart:
    case IpmPhaseStop::kNonFiniteIterate:
    case IpmPhaseStop::kEvaluationFailure:
        // The engine ends these three kNumericalError, which the read above
        // returns unchanged; this arm only labels a defensive kIterationLimit.
        return SolveStatus::kNumericalError;
    case IpmPhaseStop::kException:
        return SolveStatus::kInterrupted;
    case IpmPhaseStop::kIterationLimit:
    case IpmPhaseStop::kNone:
        // kNone here is the defensive default: the engine labels every
        // non-converged exit it takes, so an unlabelled one is the cap.
        return SolveStatus::kIterationLimit;
    }
    throw std::invalid_argument(fmt::format(
        "resolve_ipm_phase_status: unrecognized IpmPhaseStop ({})", static_cast<int>(reason)));
}

StopReason detail::to_stop_reason(IpmPhaseStop stop) {
    switch (stop) {
    case IpmPhaseStop::kNone:
    case IpmPhaseStop::kIterationLimit:
    case IpmPhaseStop::kWorkLimit:
    case IpmPhaseStop::kTimeLimit:
        // The status names these three (M8 AP-5 G1, the SQP's kNone rule).
        return StopReason::kNone;
    case IpmPhaseStop::kRestorationLocallyInfeasible:
        return StopReason::kRestorationLocallyInfeasible;
    case IpmPhaseStop::kStageStalled:
        return StopReason::kStageStalled;
    case IpmPhaseStop::kCallbackStop:
        return StopReason::kCallbackStop;
    case IpmPhaseStop::kStopRequested:
        return StopReason::kStopRequested;
    case IpmPhaseStop::kNonFiniteStart:
        return StopReason::kNonFiniteStart;
    case IpmPhaseStop::kNonFiniteIterate:
        return StopReason::kNonFiniteIterate;
    case IpmPhaseStop::kEvaluationFailure:
        return StopReason::kEvaluationFailure;
    case IpmPhaseStop::kException:
        return StopReason::kException;
    }
    throw std::invalid_argument(
        fmt::format("to_stop_reason: unrecognized IpmPhaseStop ({})", static_cast<int>(stop)));
}

// to_solve_status(SolveStatus) went with SolveStatus in M6 W5 T8.4: the mapping was
// the identity on the five values, and the SQP engine reports SolveStatus now.

} // namespace hven
