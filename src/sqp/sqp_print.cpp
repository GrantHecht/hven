// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_print.cpp -- the SQP engine's display-string helpers and its iteration
// -table renderer, carved out of the headers that declare them.
//
// CLAUDE.md section 5 names printing explicitly as code that
// belongs in a .cpp translation unit: none of it is a per-element hot path,
// none of it depends on inlining through a template parameter, and every one
// of these functions runs O(1) times per solve or once per report row. As a
// header `inline` each definition was parsed and code-generated in every TU
// that included the header, and the linker discarded all but one copy.
//
// THE WHOLE SET IS FP-ARITHMETIC-FREE: switch-to-`const char *` functions, and one
// renderer that hands already-computed doubles to `fmt::format`. `fmt` READS
// a double and formats its value; it does not compute with it, so no
// floating-point expression in this file can be re-associated, contracted or
// otherwise re-shaped by the codegen flags.
//
// WHAT LIVES WHERE. The display strings for `SolveStatus` and `StartLevel` are
// defined in `src/core/enum_names.cpp`, so no `core/` object resolves a symbol
// out of a `drivers/` one (an edge pointing up CLAUDE.md section 2's tier
// order). What stays here points DOWNWARD and is not an inversion:
// `StepVerdict` and `SqpResult` are `drivers/` types, and
// `PredictorOutcome` is a `detail/warmstart/` one.

#include <string>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>
#include <hven/detail/sqp/predictor.h>
#include <hven/sqp/sqp_solver.h>

using namespace hven::detail;

namespace hven {

const char *detail::to_string(PredictorOutcome outcome) {
    switch (outcome) {
    case PredictorOutcome::kPredicted:
        return "Predicted";
    case PredictorOutcome::kZeroStep:
        return "ZeroStep";
    case PredictorOutcome::kDegraded:
        return "Degraded";
    }
    return "Unknown";
}

const char *to_string(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptObjectiveStep:
        return "AcceptF";
    case StepVerdict::kAcceptFeasibilityStep:
        return "AcceptH";
    case StepVerdict::kReject:
        return "Reject";
    case StepVerdict::kRestore:
        return "Restore";
    }
    return "?";
}

} // namespace hven
