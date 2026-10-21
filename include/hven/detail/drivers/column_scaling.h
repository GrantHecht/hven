// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// column_scaling.h — the SQP engine's VARIABLE (column) scaling (M7 W2 SCL).
//
// WHAT IT IS. A diagonal change of variables x = D x~, D = diag(d) > 0, computed
// once per solve from the constraint Jacobian at the start point and held fixed.
// It is the half of problem scaling that problem_scaling.h (M6 W0.2) declares a
// non-goal: that layer puts the OBJECTIVE and each ROW into units and leaves the
// variables alone; this one puts the VARIABLES into units. The two compose -- the
// engine solves the column-scaled problem below and, when the row layer is on,
// row-scales THAT.
//
// THE TRANSFORMATION, exact in floating point because every d_j is a power of two
// (the power-of-the-radix rule of LAPACK's xGEEQUB): multiplying or dividing by a
// d_j changes only an exponent, so x = d*(x/d) bit for bit, the box maps without
// rounding, and the scaled problem is the caller's problem reparametrized, not an
// approximation of it.
//
//     f~(x~)  = f(D x~)            grad f~ = D grad f
//     c~(x~)  = c(D x~)            J~      = J D          (rows are NOT touched)
//     H~      = D H D              l~ = l / d,  u~ = u / d,  x~0 = x0 / d
//
// THE MULTIPLIER MAP. The rows are not scaled, so lambda~ = lambda exactly; a bound
// price is the gradient of the Lagrangian in a coordinate, so z~ = D z, z = z~ / d.
//
// THE RULE (compute_column_scaling): RUIZ'S EQUILIBRATION (D. Ruiz, "A scaling
// algorithm to equilibrate both rows and columns norms in matrices", RAL-TR-2001-034)
// of the stacked Jacobian [Je; Ji] at the start point, in the inf-norm; ONLY THE
// COLUMN FACTORS ARE KEPT. The row factors make the column factors independent of
// each row's units (a raw 1/max_i |J_ij| column is dominated by the badly-scaled
// row); the rows' own units are the row layer's (problem_scaling.h) to set.
//
// THE GATE. A problem is column-scaled only when the spread max_j d_j / min_j d_j of
// the factors (over columns that carry data) is at least the threshold. Below it the
// solve is the unscaled solve plus the rule's one Jacobian probe (`evals_values`).
//
// OVERFLOW. Exactness holds barring overflow: a finite bound with
// |l_j| / d_j > DBL_MAX (|l_j| beyond about 1.8e308 / factor_limit) maps to an
// infinite one, i.e. that bound is dropped in x~.
//
// WHAT IS SCALED IN UNITS OF WHAT -- the contract the driver keeps
// (drivers/sqp_solver.h, SqpSolver::solve_column_scaled):
//   * The convergence test, the trust region (an l-infinity box on the step in x~,
//     i.e. |dx_j| <= d_j * radius), the funnel, SOC and restoration all run on the
//     scaled problem; restoration builds its feasibility model around it.
//   * The caller receives x, z, lambda and every terminal residual in ITS units, the
//     residuals re-measured on its own model at the returned point.
//   * A kOptimal needs, beside the scaled test, the CALLER-unit feasibility
//     <= feas_tol (drivers/sqp_solver.cpp, the convergence test).

#include <hven/core/types.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers::detail {

/// @brief The column rule's parameters. Validated where SqpOptions is validated.
struct ColumnScalingRule {
    /// Ruiz sweeps, at most. The iteration stops early once every row and column
    /// inf-norm is within `tolerance` of 1.
    Index sweeps = 10;
    /// The early-stop band on the equilibrated norms.
    double tolerance = 1e-3;
    /// Symmetric clamp: every factor in [1/factor_limit, factor_limit]. >= 1.
    double factor_limit = 1e12;
    /// The gate: the factors are installed only when their spread is >= this. 1.0
    /// installs any factors that are not all equal to 1.
    double threshold = 1e3;
};

/// @brief One solve's column factors.
///
/// Default-constructed it is the IDENTITY and reports `active == false`.
struct ColumnScaling {
    /// True when the gate fired and `cols` is installed.
    bool active = false;
    /// d, one power-of-two factor per variable; empty when inactive.
    Vec cols;
    /// max d / min d over the columns that carry Jacobian data, of the factors the
    /// rule COMPUTED -- reported whether or not the gate fired. 1.0 when no column
    /// carries data.
    double spread = 1.0;
    /// The smallest / largest computed factor over the columns that carry data; 1.0
    /// when none does.
    double col_min = 1.0;
    double col_max = 1.0;
};

/// @brief Computes the column factors of THE RULE from the start point's Jacobians.
///
/// A column with no finite nonzero entry in either block keeps the factor 1 (no
/// data, no unit) and is left out of the spread. Rows with no finite nonzero entry
/// are skipped. Non-finite entries are skipped, never propagated. Each factor is
/// rounded to the nearest power of two, then clamped.
///
/// @param n     the variable count.
/// @param Je    equality Jacobian at the start point, me x n (me may be 0).
/// @param Ji    inequality Jacobian at the start point, mi x n (mi may be 0).
/// @param rule  the parameters, already validated.
/// @return the factors; `active` iff spread >= rule.threshold and some factor != 1.
/// @throws std::invalid_argument if a block's column count is not n, a 0-row block
///         included.
ColumnScaling compute_column_scaling(Index n, const SpMatRM &Je, const SpMatRM &Ji,
                                     const ColumnScalingRule &rule);

/// @brief The caller's model seen in the scaled variables x~ = x / d.
///
/// Borrows the model (which must outlive it) and the factors. Every evaluation maps
/// x~ to x = d * x~ (exact; see the banner), calls the model, and scales what comes
/// back: the gradient by d, each Jacobian column by its d, each Hessian entry (i, j)
/// by d_i d_j -- so every returned pattern is the model's own, values only rescaled.
/// Values (f, cE, cI) are the model's, untouched.
class ColumnScaledModel final : public NlpModel {
  public:
    /// @throws std::invalid_argument if d's size is not model.n() or any d_j is not
    ///         finite and > 0.
    ColumnScaledModel(const NlpModel &model, Vec d);

    Index n() const override { return model_.n(); }
    Index me() const override { return model_.me(); }
    Index mi() const override { return model_.mi(); }

    double eval_f(const Vec &x) const override;
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    Vec eval_grad(const Vec &x) const override;
    Vec eval_ce(const Vec &x) const override;
    Vec eval_ci(const Vec &x) const override;
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override;
    SpMatRM eval_jac_e(const Vec &x) const override;
    SpMatRM eval_jac_i(const Vec &x) const override;
    HVEN_SUPPRESS_DEPRECATED_END

    void eval_grad_in_place(const Vec &x, Vec &out) const override;
    void eval_ce_in_place(const Vec &x, Vec &out) const override;
    void eval_ci_in_place(const Vec &x, Vec &out) const override;
    void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const override;
    void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const override;
    void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                            const Vec &lambda_i, SpMatRM &out) const override;

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    /// The model's own start point, mapped: start_point() / d.
    Vec start_point() const override;
    void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const override;

    /// @brief The factors.
    const Vec &factors() const noexcept { return d_; }
    /// @brief x = d * x~, the caller's point of a scaled one.
    Vec to_caller(const Vec &x_scaled) const;
    /// @brief x~ = x / d, the scaled point of a caller's one.
    Vec to_scaled(const Vec &x) const;

  private:
    void scale_columns(SpMatRM &jac) const;
    void scale_symmetric(SpMatRM &hess) const;

    const NlpModel &model_;
    Vec d_;
    Vec lower_;
    Vec upper_;
};

} // namespace hven::solvers::detail
