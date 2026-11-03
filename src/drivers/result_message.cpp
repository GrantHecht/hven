// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/drivers/result_message.h"

#include <algorithm>
#include <string>

#include <fmt/format.h>

#include "hven/drivers/ipm_options.h"
#include "hven/drivers/sqp_options.h"
#include "hven/drivers/sqp_solver_types.h"

namespace hven::solvers::detail {

namespace {

Index effective_iteration_cap(const CommonOptions &common, const SolveBudget &budget) {
    return budget.max_iterations ? std::min(*budget.max_iterations, common.max_iterations)
                                 : common.max_iterations;
}

std::string iteration_limit_message(Index cap, bool best) {
    return fmt::format("iteration limit reached (max_iterations = {}){}", cap,
                       best ? "; returned the best iterate" : "");
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

} // namespace

std::string ipm_result_message(const SolveResult &result, IpmStopReason reason,
                               const IpmOptions &options, const SolveBudget &budget) {
    const CommonOptions &common = options.common;
    switch (result.status) {
    case SolveStatus::kOptimal:
        return "converged: optimality and feasibility tolerances met";
    case SolveStatus::kAcceptable:
        return "converged to an acceptable level: the acceptable tolerances were met";
    case SolveStatus::kMaxIter:
        return iteration_limit_message(effective_iteration_cap(common, budget),
                                       result.returned_best_iterate);
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
        return "numerical error";
    case SolveStatus::kInterrupted:
        return "interrupted by the iteration callback";
    case SolveStatus::kWorkLimit:
        return factorization_limit_message(common);
    case SolveStatus::kTimeLimit:
        return time_limit_message(common);
    }
    return "unrecognized status";
}

std::string sqp_result_message(const SqpResult &result, const SqpOptions &options,
                               const SolveBudget &budget) {
    const CommonOptions &common = options.common;
    switch (result.status) {
    case SolveStatus::kOptimal:
        return "converged: optimality and feasibility tolerances met";
    case SolveStatus::kAcceptable:
        return "converged to an acceptable level: the acceptable tolerances were met";
    case SolveStatus::kMaxIter:
        return iteration_limit_message(effective_iteration_cap(common, budget),
                                       result.returned_best_iterate);
    case SolveStatus::kInfeasible:
        if (result.infeasibility_certified) {
            return "locally infeasible (certified: restoration reached a stationary point of the "
                   "infeasibility measure)";
        }
        return "locally infeasible (uncertified: no claim is made about the model)";
    case SolveStatus::kStalled:
        return "stalled: the iteration stopped making progress";
    case SolveStatus::kDiverging:
        return "diverging: the iterates grew without bound";
    case SolveStatus::kNumericalError:
        return "numerical error";
    case SolveStatus::kInterrupted:
        return "interrupted by the iteration callback";
    case SolveStatus::kWorkLimit:
        if (result.counters.probe_budget_stops > 0) {
            if (budget.minor_budget) {
                return fmt::format("work limit reached: minor-iteration budget (minor_budget = {})",
                                   *budget.minor_budget);
            }
            return "work limit reached: minor-iteration budget";
        }
        return factorization_limit_message(common);
    case SolveStatus::kTimeLimit:
        return time_limit_message(common);
    }
    return "unrecognized status";
}

} // namespace hven::solvers::detail
