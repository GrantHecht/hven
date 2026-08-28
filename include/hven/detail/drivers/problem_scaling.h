// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// problem_scaling.h — the SQP engine's problem-scaling layer (M6 W0.2).
//
// WHAT IT IS. A diagonal change of units applied to the OBJECTIVE and to each
// CONSTRAINT ROW, computed once from the derivatives at the start point and
// held fixed for the solve. The declared problem is never touched: the caller's
// model is read verbatim, the transformation is applied where the engine reads
// it (detail/drivers/aggregate_eval_seam.h), and every exported quantity is
// mapped back to the caller's units at the driver's one export boundary.
//
// THE TRANSFORMATION.
//
//     min  sf * f(x)   s.t.  se_j * cE_j(x) = 0,  si_j * cI_j(x) <= 0
//
// with the bounds and the variables THEMSELVES untouched -- there is no column
// scaling here, deliberately (see WHAT IS NOT SCALED). Because the variables do
// not move, x, the box and the l-infinity trust radius are the same objects in
// both spaces, and only the objective and the rows carry units.
//
// THE MULTIPLIER MAP, which is the whole of the export contract. Writing L for
// the caller's Lagrangian and L~ for the scaled one,
//
//     sf * L = sf*f + sum_j (sf*lambda_j) * c_j
//            = L~   = sf*f + sum_j lambda~_j * (s_j * c_j)
//
// so lambda~_j * s_j = sf * lambda_j, i.e.
//
//     ENGINE -> CALLER:  lambda_j = s_j * lambda~_j / sf,   z = z~ / sf
//     CALLER -> ENGINE:  lambda~_j = sf * lambda_j / s_j,   z~ = sf * z
//
// and f = f~ / sf. The bound duals carry only sf because no bound was scaled.
//
// THE HESSIAN, the one step that is not a plain multiply. A provider computes
// `w*hess(f) + sum_j mu_j*hess(c_j)` from an objective weight and a multiplier
// block, both in ITS OWN units. To obtain hess(L~) the consumer therefore asks
// for `w = obj_scale * sf` and `mu_j = s_j * lambda~_j` -- the engine's
// multipliers mapped back to the provider's units by the CALLER->ENGINE map
// above, read backwards. That composition is exact at every objective weight
// including the restoration weight 0.0, where hess(f) drops out of both sides.
//
// WHAT IS NOT SCALED: the VARIABLES. Column scaling is a declared non-goal of
// this layer, not an oversight -- the reasoning is in the W0.2 report.
//
// THE TWO NEIGHBOURING `obj_scale` SURFACES, neither of which this is and
// neither of which it changes. `SqpDriver`'s per-subproblem `obj_scale`
// (drivers/sqp_driver.h) is the Lagrangian's objective WEIGHT -- 1.0 normally,
// 0.0 to drop the objective during restoration -- and it multiplies ON TOP of
// `obj` here, exactly as THE HESSIAN above spells out.
// `InteriorPointSolver::Settings::obj_scale_` is the OTHER engine's
// caller-supplied problem scale; this layer does not reach the IPM.

#include <hven/core/types.h>

namespace hven::solvers::detail {

/// @brief The rule that turns derivatives at the start point into factors.
///
/// Both fields are validated where the caller's own options are validated
/// (drivers/sqp_options.cpp), never here.
struct ScalingRule {
    /// The inf-norm each scaled gradient and each scaled Jacobian row is aimed
    /// at. Follows IPOPT's `nlp_scaling_max_gradient` in name and in default.
    double max_gradient = 100.0;
    /// Symmetric clamp: every factor is confined to [1/factor_limit,
    /// factor_limit]. Must be >= 1.
    double factor_limit = 1e12;
};

/// @brief One solve's factors: strictly positive, finite, and fixed once set.
///
/// A default-constructed value is the IDENTITY and reports `active == false`;
/// that is exactly the state a solve runs in when scaling is off, and every
/// apply site is a no-op against it.
struct ProblemScaling {
    /// True once `compute_problem_scaling` has installed factors. False means
    /// the identity, and means the arithmetic below is never entered.
    bool active = false;
    /// `sf` above. Strictly positive and finite; 1.0 when inactive.
    double obj = 1.0;
    /// `se`, one factor per equality row. Empty when inactive.
    Vec eq_rows;
    /// `si`, one factor per inequality row. Empty when inactive.
    Vec ineq_rows;

    /// @brief Largest row factor over BOTH blocks, or 1.0 if there are no rows.
    double row_max() const noexcept;
    /// @brief Smallest row factor over BOTH blocks, or 1.0 if there are no rows.
    double row_min() const noexcept;
};

/// @brief Computes the factors of THE RULE from the start point's derivatives.
///
/// Each factor is `clamp(rule.max_gradient / norm, 1/limit, limit)` where
/// `norm` is the inf-norm of the objective gradient or of that Jacobian row. A
/// norm that is zero or non-finite yields the factor 1.0 rather than the
/// clamp's ceiling -- a structurally empty or a broken row must not be
/// AMPLIFIED by twelve orders on the strength of having no data in it.
///
/// THE OBJECTIVE RULE IS TWO-SIDED; THE ROW RULE IS NOT. That asymmetry is
/// deliberate and it is the whole of the departure from IPOPT, whose
/// `min(1, g_max/||g||inf)` is one-sided for both.
///
///   * The OBJECTIVE may be scaled UP, because a gradient that is too small is
///     a CORRECTNESS problem rather than a conditioning one: it certifies a KKT
///     point that is not one. HS25 is the case in hand -- kOptimal in zero
///     majors at f = 32.8 against a true f* = 0, on a 1e-8 gradient below any
///     sane kkt_tol -- and only amplification refuses that certificate. The
///     cost is that a start point which is genuinely near-stationary has its
///     gradient amplified too; `factor_limit` bounds the damage and every
///     factor is reported on the solution, so the choice is visible.
///   * A ROW is never scaled up. Amplifying a row whose Jacobian is small at
///     the start point -- because the row is nearly inactive there, not because
///     its units are wrong -- multiplies its residual by up to `factor_limit`
///     and manufactures infeasibility out of a row that has none. Equilibration
///     removes the SPREAD between rows, and shrinking the large ones achieves
///     that without inventing a residual. A Jacobian that is already well
///     scaled is therefore left EXACTLY alone, factor 1.0 in every row, which
///     also makes "scaling changed nothing here" an observable fact rather than
///     a uniform factor hiding in the multipliers.
///
/// @param grad  objective gradient at the start point.
/// @param Je    equality Jacobian at the start point; may have zero rows.
/// @param Ji    inequality Jacobian at the start point; may have zero rows.
/// @param rule  the rule's two parameters, already validated.
/// @return factors with `active == true`, sized to Je/Ji's row counts.
ProblemScaling compute_problem_scaling(const Vec &grad, const SpMatRM &Je, const SpMatRM &Ji,
                                       const ScalingRule &rule);

} // namespace hven::solvers::detail
