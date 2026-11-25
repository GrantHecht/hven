// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The two rules both engines share for the best iterate: which exits substitute it
// (CommonOptions::return_best), and what "best" means.

#include <algorithm>
#include <cmath>
#include <limits>

#include <hven/core/solver_status.h>
#include <hven/solver/common_options.h>

namespace hven::detail {

/// @brief Whether an exit with @p status hands back the best iterate under @p mode.
///        kInfeasible (certified or not), kInterrupted and kOptimal never do.
constexpr bool return_best_applies(ReturnBest mode, SolveStatus status) {
    switch (status) {
    case SolveStatus::kIterationLimit:
    case SolveStatus::kWorkLimit:
    case SolveStatus::kTimeLimit:
        return mode != ReturnBest::kNever;
    case SolveStatus::kAcceptable:
    case SolveStatus::kStalled:
    case SolveStatus::kDiverging:
    case SolveStatus::kNumericalError:
        return mode == ReturnBest::kOnAnyNonOptimal;
    case SolveStatus::kOptimal:
    case SolveStatus::kInfeasible:
    case SolveStatus::kInterrupted:
        return false;
    }
    return false;
}

/// @brief The shared best-iterate order: feasibility first, objective second.
///
/// Violations at or below @p feasibility_tol are tied (the band); outside it the smaller
/// violation wins; tied points compare by objective (NaN counts as +inf). Strict: an equal key
/// does not replace the incumbent. A weak order, so tracking is transitive.
/// @param violation  The engine's own convergence feasibility measure at the candidate.
inline bool best_improves(double violation, double f, double best_violation, double best_f,
                          double feasibility_tol) {
    const double inf = std::numeric_limits<double>::infinity();
    const double v = std::max(violation, feasibility_tol);
    const double bv = std::max(best_violation, feasibility_tol);
    const double fe = std::isnan(f) ? inf : f;
    const double bfe = std::isnan(best_f) ? inf : best_f;
    return v < bv || (v == bv && fe < bfe);
}

} // namespace hven::detail
