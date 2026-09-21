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

// THE COUNTER'S OWN VOCABULARY (M7 W2 M3a fix1). Ten spellings: the nine above,
// byte for byte -- a consumer must be able to compare this key against the end
// line's `status` without a translation table -- plus the `not_run` the enum
// exists for. The switch is exhaustive and has no default arm, so an enumerator
// added without a spelling fails to compile rather than reaching the throw.
const char *to_string(RestorationSubStatus status) {
    switch (status) {
    case RestorationSubStatus::kNotRun:
        return "not_run";
    case RestorationSubStatus::kOptimal:
        return "optimal";
    case RestorationSubStatus::kAcceptable:
        return "acceptable";
    case RestorationSubStatus::kMaxIter:
        return "max_iter";
    case RestorationSubStatus::kInfeasible:
        return "infeasible";
    case RestorationSubStatus::kStalled:
        return "stalled";
    case RestorationSubStatus::kDiverging:
        return "diverging";
    case RestorationSubStatus::kNumericalError:
        return "numerical_error";
    case RestorationSubStatus::kBudgetExhausted:
        return "budget_exhausted";
    case RestorationSubStatus::kInterrupted:
        return "interrupted";
    }
    throw std::invalid_argument(fmt::format(
        "to_string(RestorationSubStatus): unrecognized value ({})", static_cast<int>(status)));
}

// THE TOTAL MAP, and the reason the enum above carries all nine of SolveStatus's
// values rather than the five a feasibility sub-solve can return: an exhaustive
// switch with no default arm and no invented image. A sub-solve that returned a
// status has, by construction, RUN -- so `kNotRun` is unreachable from here.
RestorationSubStatus to_restoration_sub_status(SolveStatus status) {
    switch (status) {
    case SolveStatus::kOptimal:
        return RestorationSubStatus::kOptimal;
    case SolveStatus::kAcceptable:
        return RestorationSubStatus::kAcceptable;
    case SolveStatus::kMaxIter:
        return RestorationSubStatus::kMaxIter;
    case SolveStatus::kInfeasible:
        return RestorationSubStatus::kInfeasible;
    case SolveStatus::kStalled:
        return RestorationSubStatus::kStalled;
    case SolveStatus::kDiverging:
        return RestorationSubStatus::kDiverging;
    case SolveStatus::kNumericalError:
        return RestorationSubStatus::kNumericalError;
    case SolveStatus::kBudgetExhausted:
        return RestorationSubStatus::kBudgetExhausted;
    case SolveStatus::kInterrupted:
        return RestorationSubStatus::kInterrupted;
    }
    throw std::invalid_argument(fmt::format(
        "to_restoration_sub_status: unrecognized SolveStatus ({})", static_cast<int>(status)));
}

const char *to_string(IpmStopReason reason) {
    switch (reason) {
    case IpmStopReason::kNone:
        return "none";
    case IpmStopReason::kIterationCap:
        return "iteration_cap";
    case IpmStopReason::kRestorationLocallyInfeasible:
        return "restoration_locally_infeasible";
    case IpmStopReason::kInterrupted:
        return "interrupted";
    case IpmStopReason::kStageStalled:
        return "stage_stalled";
    }
    throw std::invalid_argument(
        fmt::format("to_string(IpmStopReason): unrecognized value ({})", static_cast<int>(reason)));
}

// THE NINE MESSAGE KINDS (M6 W5 T8.7b). Lower snake, which is both the display
// form and -- unlike to_string(StartLevel) below -- the schema's own spelling,
// so trace_writer.cpp's to_json(IpmMessageKind) forwards here rather than
// keeping a second copy that could drift.
const char *to_string(IpmMessageKind kind) {
    switch (kind) {
    case IpmMessageKind::kSolverInitialized:
        return "solver_initialized";
    case IpmMessageKind::kRankDeficiency:
        return "rank_deficiency";
    case IpmMessageKind::kFactorizationHardError:
        return "factorization_hard_error";
    case IpmMessageKind::kInertiaExhausted:
        return "inertia_exhausted";
    case IpmMessageKind::kRestorationLocallyInfeasible:
        return "restoration_locally_infeasible";
    case IpmMessageKind::kFeasibilityStall:
        return "feasibility_stall";
    case IpmMessageKind::kInterruptAtIteration:
        return "interrupt_at_iteration";
    case IpmMessageKind::kPhaseDiverged:
        return "phase_diverged";
    case IpmMessageKind::kInterruptSkippingPhases:
        return "interrupt_skipping_phases";
    }
    throw std::invalid_argument(
        fmt::format("to_string(IpmMessageKind): unrecognized value ({})", static_cast<int>(kind)));
}

const char *to_string(IpmKktFactorStatus status) {
    switch (status) {
    case IpmKktFactorStatus::kSuccess:
        return "success";
    case IpmKktFactorStatus::kNumericalIssue:
        return "numerical_issue";
    case IpmKktFactorStatus::kNoConvergence:
        return "no_convergence";
    case IpmKktFactorStatus::kInvalidInput:
        return "invalid_input";
    }
    throw std::invalid_argument(fmt::format(
        "to_string(IpmKktFactorStatus): unrecognized value ({})", static_cast<int>(status)));
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
