// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// restoration.h -- the restoration phase's l1 feasibility-problem model
// wrapper. The RESTORATION PHASE note ("the header note"), build_subproblem,
// and qp_failure_is_retryable live in drivers/sqp_driver.h; the elastic tier
// compared against is detail/globalization/sqp/elastic.h's. Every member
// function of RestorationModel declared here is defined in
// src/globalization/sqp/soc_elastic_restoration.cpp (with soc.h's and
// elastic.h's definitions). One consequence worth stating where the class is:
// `n()` is the class's KEY FUNCTION, so the vtable/typeinfo are emitted in that
// TU alone rather than weakly in every TU touching the class.
//
// NOT SELF-CONTAINED BY DESIGN: `NlpEval` is defined in drivers/sqp_driver.h,
// which includes this header after NlpEval's definition and before the driver
// class, so no header cycle exists (this header never includes sqp_driver.h).
// Any other includer must have NlpEval complete first.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/types.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers {

/// The fraction of tr_init the trust region restarts at when the main loop
/// resumes from restoration: one order of magnitude is the conservative reading
/// of "start again", and the growth rule earns it back (see sqp_driver.h's
/// WHAT RESUMING DOES).
inline constexpr double kRestoreRadiusFactor = 0.1;

/// "Effectively infinite" upper bound on a slack, matching qp_problem.h's own
/// stated convention (+/-1e20) rather than a true +inf, so every bound
/// arithmetic in the engine and in build_subproblem (upper - x) stays in
/// ordinary floating point.
inline constexpr double kRestorationSlackBound = 1e20;

/// @brief The l1 FEASIBILITY PROBLEM of the restoration phase, as an NlpModel
/// wrapping the caller's own model:
///
///     min   sum_i sigmaE_i (sp_i + sm_i) + sum_j sigmaI_j si_j
///     s.t.  cE(x) + sigmaE.*(sp - sm)  = 0
///           cI(x) - sigmaI.*si        <= 0
///           l <= x <= u,   sp, sm, si >= 0
///
/// in y = (x, sp, sm, si), with n + 2*me + mi variables and the SAME me and mi
/// (the slacks add COLUMNS, never rows — the same shape choice as the elastic
/// tier). Minimizing this minimizes h(x) = ||cE(x)||_1 +
/// sum_j max(0, cI_j(x)) EXACTLY (sqp_driver.h's EXACTNESS paragraph is the
/// argument this class is built to satisfy).
///
/// THE SIGMA SCALING: sigmaE_i = max(1, ||grad cE_i(x_entry)||inf), sigmaI_j
/// likewise, computed ONCE at construction (the wrapper's variables must have
/// constant units) from the Jacobians already in hand at entry — scale the
/// constructed COLUMN to the row it joins, which is why the slack columns are
/// +/-sigma rather than +/-1. Because the objective coefficient carries the
/// SAME sigma, the objective is still the actual violation and the multiplier
/// certificate is unchanged.
///
/// max(1, .) rather than the row norm itself: a column must never be scaled UP,
/// or a small Jacobian row would shrink the column entry and inflate the slack
/// variable, trading this conditioning problem for its mirror image.
///
/// LIFETIME: holds a REFERENCE to the wrapped model, which must outlive it; the
/// driver constructs one on the stack for the duration of one restoration.
class RestorationModel final : public NlpModel {
  public:
    /// @param ev Must be eval_nlp(model, x_entry) -- the entry point's
    /// evaluation, already in hand, from which both the sigmas and the start
    /// point's slacks are read without a single extra model call.
    RestorationModel(const NlpModel &model, const Vec &x_entry, const NlpEval &ev);

    Index n() const override;
    Index me() const override;
    Index mi() const override;

    /// The original variables of an augmented point -- what the driver takes
    /// back out of a restoration solve.
    Vec original_x(const Vec &y) const;
    const Vec &slack_scale_e() const;
    const Vec &slack_scale_i() const;

    double eval_f(const Vec &y) const override;

    /// CONSTANT gradient: the objective is LINEAR with no x-block at all. That
    /// zero x-block is what makes a stationary point of this problem a
    /// stationary point of h rather than of some trade-off against f.
    Vec eval_grad(const Vec &) const override;

    Vec eval_ce(const Vec &y) const override;

    Vec eval_ci(const Vec &y) const override;

    /// obj_scale IS DELIBERATELY IGNORED HERE, and that is exact rather than a
    /// shortcut: hess = obj_scale*hess(f) + sum lambda*hess(c), and hess(f) is
    /// IDENTICALLY ZERO for a linear objective. What remains is the wrapped
    /// model's own constraint Hessian -- model_.eval_hess(x, 0.0, .), the SAME
    /// call with the objective switched off. The slack block contributes
    /// nothing (slacks enter linearly), so the augmented Hessian is the
    /// original's entries in an n_-square frame with no new nonzeros -- the
    /// elastic tier's H-extension, satisfying QpProblem::validate's upper-
    /// triangle rule for the same reason (no entry moves).
    SpMatRM eval_hess(const Vec &y, double, const Vec &lambda_e,
                      const Vec &lambda_i) const override;

    /// [ Je(x) | diag(sigmaE) | -diag(sigmaE) | 0 ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &y) const override;

    /// [ Ji(x) | 0 | 0 | -diag(sigmaI) ]
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &y) const override;

    const Vec &lower() const override;
    const Vec &upper() const override;
    Vec start_point() const override;

  private:
    const NlpModel &model_;
    Index nx_ = 0, me_ = 0, mi_ = 0, n_ = 0;
    Vec sigma_e_, sigma_i_;
    Vec lower_, upper_, start_;
};

} // namespace hven::solvers
