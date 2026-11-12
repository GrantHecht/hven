// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/drivers/result_message.h"

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "hven/drivers/ipm_options.h"
#include "hven/drivers/sqp_options.h"
#include "hven/drivers/sqp_solver_types.h"

namespace hven::detail {

namespace {

Index effective_iteration_cap(const CommonOptions &common, const SolveBudget &budget) {
    return budget.max_iterations ? std::min(*budget.max_iterations, common.max_iterations)
                                 : common.max_iterations;
}

std::string iteration_limit_message(Index cap) {
    return fmt::format("iteration limit reached (max_iterations = {})", cap);
}

// The best-iterate clause rides on every status the flag is set with, not only
// kIterationLimit: the interior-point return-best substitution applies to any
// non-optimal exit but an interrupt.
std::string with_best_clause(std::string message, bool returned_best_iterate) {
    if (returned_best_iterate) {
        message += "; returned the best iterate";
    }
    return message;
}

std::string factorization_limit_message(const CommonOptions &common) {
    if (common.max_factorizations) {
        return fmt::format("work limit reached: factorization ceiling (max_factorizations = {})",
                           *common.max_factorizations);
    }
    return "work limit reached: factorization ceiling";
}

std::string time_limit_message(const CommonOptions &common) {
    if (common.time_limit_seconds) {
        return fmt::format("time limit reached (time_limit_seconds = {})",
                           *common.time_limit_seconds);
    }
    return "time limit reached";
}

// "<lead>: <detail>", or the lead alone when the engine gave no detail.
std::string with_detail(const char *lead, const std::string &detail) {
    if (detail.empty()) {
        return lead;
    }
    return fmt::format("{}: {}", lead, detail);
}

std::string exception_message(const std::string &detail) {
    return with_detail("interrupted by an exception that escaped the solve", detail);
}

std::string ipm_status_message(const SolveResult &result, IpmStopReason reason,
                               const std::string &detail, const IpmOptions &options,
                               const SolveBudget &budget) {
    const CommonOptions &common = options.common;
    switch (result.status) {
    case SolveStatus::kOptimal:
        return "converged: optimality and feasibility tolerances met";
    case SolveStatus::kAcceptable:
        return "converged to an acceptable level: the acceptable tolerances were met";
    case SolveStatus::kIterationLimit:
        return iteration_limit_message(effective_iteration_cap(common, budget));
    case SolveStatus::kInfeasible:
        return "locally infeasible";
    case SolveStatus::kStalled:
        if (reason == IpmStopReason::kRestorationLocallyInfeasible) {
            return "stalled: restoration reached a locally infeasible point";
        }
        return "stalled: the iteration stopped making progress";
    case SolveStatus::kDiverging:
        return "diverging: the iterates grew without bound";
    case SolveStatus::kNumericalError:
        switch (reason) {
        case IpmStopReason::kNonFiniteStart:
        case IpmStopReason::kNonFiniteIterate:
        case IpmStopReason::kEvaluationFailure:
        case IpmStopReason::kNone:
            // kNone: empty on every exit but a factorization that left no
            // numerics (the initial one, an iteration's inertia-correction
            // ladder, or the dense step guard's re-run), whose words say what
            // failed.
            return with_detail("numerical error", detail);
        default:
            return "numerical error";
        }
    case SolveStatus::kInterrupted:
        if (reason == IpmStopReason::kException) {
            return exception_message(detail);
        }
        return "interrupted by the iteration callback";
    case SolveStatus::kWorkLimit:
        return factorization_limit_message(common);
    case SolveStatus::kTimeLimit:
        return time_limit_message(common);
    }
    return "unrecognized status";
}

std::string sqp_status_message(const SqpResult &result, SqpStopReason reason,
                               const std::string &detail, const SqpOptions &options,
                               const SolveBudget &budget) {
    const CommonOptions &common = options.common;
    switch (result.status) {
    case SolveStatus::kOptimal:
        return "converged: optimality and feasibility tolerances met";
    case SolveStatus::kAcceptable:
        return "converged to an acceptable level: the acceptable tolerances were met";
    case SolveStatus::kIterationLimit:
        return iteration_limit_message(effective_iteration_cap(common, budget));
    case SolveStatus::kInfeasible:
        if (result.infeasibility_certified) {
            return "locally infeasible (certified: restoration reached a stationary point of the "
                   "infeasibility measure)";
        }
        if (reason == SqpStopReason::kInconsistentEqualities) {
            return "locally infeasible (uncertified: the equality constraints are inconsistent)";
        }
        if (reason == SqpStopReason::kSubproblemFailure) {
            return "locally infeasible (uncertified: the QP subproblem is infeasible)";
        }
        return "locally infeasible (uncertified: no claim is made about the model)";
    case SolveStatus::kStalled:
        return "stalled: the iteration stopped making progress";
    case SolveStatus::kDiverging:
        return "diverging: the iterates grew without bound";
    case SolveStatus::kNumericalError:
        switch (reason) {
        case SqpStopReason::kNonFiniteStart:
        case SqpStopReason::kNonFiniteIterate:
            return with_detail("numerical error", detail);
        case SqpStopReason::kSubproblemFailure:
            return "numerical error: the QP subproblem failed and its retry budget was spent";
        case SqpStopReason::kRestorationRefused:
            return "numerical error: feasibility restoration was requested and could not be "
                   "serviced (a request from inside restoration, or the entry cap spent)";
        case SqpStopReason::kRestorationNonFinite:
            return "numerical error: feasibility restoration ended where the model is not finite";
        case SqpStopReason::kRestorationNoClaim:
            return "numerical error: feasibility restoration converged without a claim (the "
                   "point is neither feasible enough to resume nor infeasible enough to certify)";
        case SqpStopReason::kRestorationFailed:
            return with_detail("numerical error: the feasibility restoration sub-solve failed",
                               detail);
        default:
            return "numerical error";
        }
    case SolveStatus::kInterrupted:
        if (reason == SqpStopReason::kException) {
            return exception_message(detail);
        }
        return "interrupted by the iteration callback";
    case SolveStatus::kWorkLimit:
        if (result.counters.probe_budget_stops > 0) {
            if (budget.max_minor_iterations) {
                return fmt::format("work limit reached: minor-iteration budget (minor_budget = {})",
                                   *budget.max_minor_iterations);
            }
            return "work limit reached: minor-iteration budget";
        }
        return factorization_limit_message(common);
    case SolveStatus::kTimeLimit:
        return time_limit_message(common);
    }
    return "unrecognized status";
}

} // namespace

std::string ipm_result_message(const SolveResult &result, IpmStopReason reason,
                               const std::string &detail, const IpmOptions &options,
                               const SolveBudget &budget) {
    return with_best_clause(ipm_status_message(result, reason, detail, options, budget),
                            result.returned_best_iterate);
}

std::string sqp_result_message(const SqpResult &result, SqpStopReason reason,
                               const std::string &detail, const SqpOptions &options,
                               const SolveBudget &budget) {
    return with_best_clause(sqp_status_message(result, reason, detail, options, budget),
                            result.returned_best_iterate);
}

std::string describe_exception(const std::exception_ptr &error) noexcept {
    try {
        if (error) {
            std::rethrow_exception(error);
        }
        return std::string();
    } catch (const std::exception &e) {
        try {
            return e.what();
        } catch (...) {
            return std::string();
        }
    } catch (...) {
        try {
            return "an exception of a type not derived from std::exception";
        } catch (...) {
            return std::string();
        }
    }
}

} // namespace hven::detail
