// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// nlp_model_in_place.h — the optional companion surface to NlpModel: the same
// evaluations, handed back as references into storage the model keeps.
//
// NlpModel's evaluators return whole objects by value. That is the right shape
// for a contract -- it says nothing about who owns what, and every model can
// implement it -- but it costs one allocation per call however the caller
// stores the result, which a consumer evaluating once per iteration pays on
// its hot path. A model that also derives from the class below evaluates into
// members it owns and returns a reference to them, so such a consumer, once
// warm, allocates nothing.

#include "hven/core/types.h"

namespace hven::solvers {

/// @brief Optional companion to NlpModel: evaluations that land in storage the
///        model owns.
///
/// Deriving from this is optional and adds no obligation to NlpModel. A model
/// that does not derive from it is evaluated through NlpModel's by-value
/// methods and produces the same numbers; a consumer that wants the reference
/// form asks for it and falls back when it is absent.
///
/// Each method must return exactly what its NlpModel counterpart returns at
/// the same arguments. The returned reference stays valid until the next call
/// on this model that could refresh that storage, so a caller needing the
/// value to outlive that must copy it.
class NlpModelInPlace {
  public:
    virtual ~NlpModelInPlace() = default;

    /// @brief NlpModel::eval_grad, by reference.
    /// @param x The iterate.
    /// @return The gradient, size n().
    virtual const Vec &grad_in_place(const Vec &x) const = 0;

    /// @brief NlpModel::eval_ce, by reference.
    /// @param x The iterate.
    /// @return The equality residuals, size me().
    virtual const Vec &ce_in_place(const Vec &x) const = 0;

    /// @brief NlpModel::eval_ci, by reference.
    /// @param x The iterate.
    /// @return The inequality residuals, size mi().
    virtual const Vec &ci_in_place(const Vec &x) const = 0;

    /// @brief NlpModel::eval_jac_e, by reference.
    /// @param x The iterate.
    /// @return The equality Jacobian, me() by n().
    virtual const SpMatRM &jac_e_in_place(const Vec &x) const = 0;

    /// @brief NlpModel::eval_jac_i, by reference.
    /// @param x The iterate.
    /// @return The inequality Jacobian, mi() by n().
    virtual const SpMatRM &jac_i_in_place(const Vec &x) const = 0;

    /// @brief NlpModel::eval_hess, by reference.
    /// @param x The iterate.
    /// @param obj_scale The objective scale.
    /// @param lambda_e Equality multipliers, size me().
    /// @param lambda_i Inequality multipliers, size mi().
    /// @return The Lagrangian Hessian, upper triangle, n() by n().
    virtual const SpMatRM &hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                         const Vec &lambda_i) const = 0;
};

} // namespace hven::solvers
