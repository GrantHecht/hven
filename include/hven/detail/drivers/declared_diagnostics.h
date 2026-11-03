// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The ONE definition of the four shared diagnostics (SolveResult::stationarity,
// feasibility, complementarity and the two feasibility parts that make
// `feasibility`), over the DECLARED problem, in the CALLER's units.
//
// Each engine's convergence test gates on quantities in ITS OWN space; these
// are the comparable quantities, computed once so the engines cannot drift
// apart on what "stationarity" means.
//
// These functions EVALUATE NOTHING. Every model quantity arrives as an
// argument, taken from the evaluation the engine already performed at the point
// it is about to return -- a fresh evaluation would move the evaluation
// counters on every solve. Where no finite evaluation of the returned point
// exists, every diagnostic is NaN (unmeasured), never zero-filled.

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <hven/core/types.h>

namespace hven::solvers::detail {

/// @brief max(feasibility_e, feasibility_i), NaN if either is NaN.
inline double combine_feasibility(double feasibility_e, double feasibility_i) noexcept {
    if (std::isnan(feasibility_e) || std::isnan(feasibility_i)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return std::max(feasibility_e, feasibility_i);
}

/// @brief The four diagnostics of a returned point, over the declared
///        problem, in the caller's units.
///
/// Every member is NaN when the quantity was not measured.
struct DeclaredDiagnostics {
    /// inf-norm of grad f + Je^T lambda_e + Ji^T lambda_i - z over the declared
    /// coordinates that were measured.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm of the declared equality residuals ce.
    double feasibility_e = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm of the positive part of the declared inequality residuals and
    /// of the declared bound violations.
    double feasibility_i = std::numeric_limits<double>::quiet_NaN();
    /// inf-norm over the declared inequality products lambda_i o ci and the
    /// CANONICAL bound products max(z,0) o (x - l) and max(-z,0) o (u - x).
    double complementarity = std::numeric_limits<double>::quiet_NaN();
};

/// @brief Computes the four shared diagnostics over the DECLARED problem.
///
/// Evaluates nothing: every model quantity arrives as an argument, from the
/// evaluation the caller already holds at @p x.
///
/// One pass over the declared coordinates and one over each constraint block.
/// The bound products use the CANONICAL SPLIT -- max(z,0) * (x - l) and
/// max(-z,0) * (u - x) -- because a signed z cannot recover two separate prices
/// at a two-sided bound whose lower and upper multipliers are both positive.
/// The engine's own two-price complementarity, where it has one, stays on the
/// engine's own result.
///
/// Infinite bounds contribute nothing: a product of a zero price and an
/// infinite distance is not a measurement.
///
/// @param x                     The returned point, declared width n.
/// @param lambda_e              Declared equality multipliers, me.
/// @param lambda_i              Declared inequality multipliers, mi.
/// @param z                     Bound multipliers, n, signed z = zL - zU.
/// @param grad                  Objective gradient at @p x, n.
/// @param Je                    Declared equality Jacobian, me x n.
/// @param Ji                    Declared inequality Jacobian, mi x n.
/// @param ce                    Declared equality residuals at @p x, me.
/// @param ci                    Declared inequality residuals at @p x, mi;
///                              the feasible set is ci <= 0.
/// @param lower                 Declared lower bounds, n; -inf where absent.
/// @param upper                 Declared upper bounds, n; +inf where absent.
/// @param excluded_coordinates  Coordinates whose stationarity was NOT measured
///                              -- the interior-point engine's MakeParameter
///                              eliminations, whose reduced gradient has no row
///                              at all. May be empty. Indices outside [0, n)
///                              are ignored.
/// @return The four diagnostics.
/// @throws std::invalid_argument if the block sizes do not agree.
DeclaredDiagnostics compute_declared_diagnostics(const Vec &x, const Vec &lambda_e,
                                                 const Vec &lambda_i, const Vec &z, const Vec &grad,
                                                 const SpMatRM &Je, const SpMatRM &Ji,
                                                 const Vec &ce, const Vec &ci, const Vec &lower,
                                                 const Vec &upper,
                                                 const std::vector<Index> &excluded_coordinates);

/// @brief The same four diagnostics, from a Lagrangian gradient the caller
///        already holds.
///
/// THE ARITHMETIC IS THE SAME ARITHMETIC -- the overload above forms
/// grad f + Je^T lambda_e + Ji^T lambda_i and calls straight through to this
/// one. Two front doors exist because the two engines hold different things at
/// the point they report:
///
///   * The SQP's SqpKkt carries `grad_lag` outright, so forming it again from
///     the pieces would be arithmetic done twice with a chance of disagreeing.
///   * The INTERIOR-POINT engine has no separated gradient or Jacobian AT ALL
///     at the returned iterate: the model's derivatives are scattered directly
///     into one compound KKT buffer, fused there with the Hessian, the barrier
///     diagonals and any inertia perturbation, and left at the LAST EVALUATED
///     iterate rather than the reported one. What that engine does hold, in
///     its right-hand side at the returned iterate, is exactly this vector. A
///     signature demanding (grad, Je, Ji) would force it to evaluate the model
///     again -- which would move `evals_full` on every solve and break the very
///     identity proofs this task is held to.
///
/// @param x                     The returned point, declared width n.
/// @param lambda_i              Declared inequality multipliers, mi. (The
///                              equality multipliers are already inside
///                              @p grad_lag; the diagnostics never read them
///                              again.)
/// @param z                     Bound multipliers, n, signed z = zL - zU.
/// @param grad_lag              grad f + Je^T lambda_e + Ji^T lambda_i at @p x,
///                              declared width n, WITHOUT the bound term -- it
///                              is subtracted here, per coordinate, so that the
///                              exclusion set can be applied.
/// @param ce                    Declared equality residuals at @p x.
/// @param ci                    Declared inequality residuals at @p x; the
///                              feasible set is ci <= 0.
/// @param lower                 Declared lower bounds, n; -inf where absent.
/// @param upper                 Declared upper bounds, n; +inf where absent.
/// @param excluded_coordinates  Coordinates whose stationarity was NOT
///                              measured; see the overload above.
/// @return The four diagnostics.
/// @throws std::invalid_argument if the block sizes do not agree.
DeclaredDiagnostics
compute_declared_diagnostics_from_grad_lag(const Vec &x, const Vec &lambda_i, const Vec &z,
                                           const Vec &grad_lag, const Vec &ce, const Vec &ci,
                                           const Vec &lower, const Vec &upper,
                                           const std::vector<Index> &excluded_coordinates);

} // namespace hven::solvers::detail
