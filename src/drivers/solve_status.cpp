// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The interior-point engine's phase-status resolution. A TU rather than an
// inline body, on core/enum_names.cpp's footing: a switch over enums is
// orchestration, not a per-element hot path (CLAUDE.md §5).
//
// The vocabulary's own switches -- to_string and severity -- moved down to
// src/core/enum_names.cpp with the enums themselves in M6 W5 T8.4.

#include <hven/drivers/solve_status.h>

#include <stdexcept>

#include <fmt/format.h>

namespace hven::solvers {

SolveStatus resolve_ipm_phase_status(SolveStatus raw_verdict, IpmStopReason reason) {
    // THE VERDICT IS READ FIRST, so a stronger one the convergence check already
    // holds -- kOptimal or kAcceptable -- survives a stall recorded in the same
    // iteration. Only the engine's own "ran out of iterations with nothing
    // better to say" verdict is open to a stop reason's reinterpretation.
    if (raw_verdict != SolveStatus::kMaxIter) {
        return raw_verdict;
    }
    switch (reason) {
    case IpmStopReason::kInterrupted:
        // THE CALLER STOPPED IT, and that outranks the cap the loop would
        // otherwise have been labelled with -- but only from kMaxIter, so the
        // read above still lets a converged or acceptable verdict stand. "The
        // stop is a caller's decision, not a verdict" cuts both ways: it
        // renames a NOTCONVERGED exit and it overrides nothing.
        return SolveStatus::kInterrupted;
    case IpmStopReason::kStageStalled:
    case IpmStopReason::kRestorationLocallyInfeasible:
        return SolveStatus::kStalled;
    case IpmStopReason::kIterationCap:
    case IpmStopReason::kNone:
        // kNone here is the defensive default: the engine labels every
        // non-converged exit it takes, so an unlabelled one is the cap.
        return SolveStatus::kMaxIter;
    }
    throw std::invalid_argument(fmt::format(
        "resolve_ipm_phase_status: unrecognized IpmStopReason ({})", static_cast<int>(reason)));
}

// to_solve_status(SolveStatus) went with SolveStatus in M6 W5 T8.4: the mapping was
// the identity on the five values, and the SQP engine reports SolveStatus now.

} // namespace hven::solvers
