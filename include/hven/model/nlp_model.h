// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// nlp_model.h — the driver-facing NLP:
//
//     min   f(x)
//     s.t.  cE(x)  = 0
//           cI(x) <= 0
//           l <= x <= u
//
// NlpModel is a pure interface.
//
// MULTIPLIER SIGN CONVENTION -- IDENTICAL to qp_problem.h's stationarity
// condition, with cE/cI/Je/Ji standing in for that header's be/bi/Ae/Ai:
//
//     grad(f)(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,  lambda_i >= 0
//
// where Je = eval_jac_e(x), Ji = eval_jac_i(x), grad(f) = eval_grad(x). z is
// the bound multiplier: >= 0 when a variable sits at its active lower bound,
// <= 0 at an active upper bound, 0 when free -- again exactly qp_problem.h's
// convention.
//
// EXACT LAGRANGIAN HESSIAN. eval_hess(x, obj_scale, lambda_e, lambda_i)
// returns the upper triangle only (SpMatRM -- same storage convention as
// qp_problem.h::H) of
//
//     obj_scale * hess(f)(x)
//       + sum_i lambda_e(i) * hess(cE_i)(x) + sum_j lambda_i(j) * hess(cI_j)(x)
//
// i.e. the Hessian, with respect to x, of
// L(x, lambda) = obj_scale*f(x) + lambda_e^T cE(x) + lambda_i^T cI(x) -- the
// SAME Lagrangian sign convention as the stationarity condition above
// (+lambda_e^T cE, +lambda_i^T cI). obj_scale exists for driver-side scaling;
// callers that do not scale pass obj_scale = 1.0.
//
// STRUCTURAL PATTERN INVARIANCE -- A BINDING PRECONDITION ON EVERY
// IMPLEMENTER. eval_hess, eval_jac_e and eval_jac_i must each return the SAME
// SPARSITY PATTERN at every x: the set of (row, col) entries a model emits is
// a property of the MODEL, not of the point it is evaluated at -- AND, FOR
// eval_hess, NOT OF THE ARGUMENTS IT IS CALLED WITH: the pattern must be
// independent of `obj_scale` and of the `lambda_e`/`lambda_i` VALUES as well.
// Concretely, an implementation may NOT skip constraint j's Hessian block
// because `lambda_i(j) == 0.0`, nor drop the objective block at
// `obj_scale == 0.0` -- both must be emitted as structural zeros; an entry
// whose value happens to be 0.0 at some particular x must still be emitted,
// as a structural zero (Eigen's setFromTriplets preserves explicit zeros).
// The warm-start machinery keys its cached-factorization structural hash
// (warm_start.h's WarmStart::structure_hash, over qp_engine.h's
// detail::structural_hash) on these patterns alone, so a pattern that shifts
// with x or with the arguments silently forfeits hot starts or matches a hash
// whose symbolic factorization no longer describes the matrix -- neither
// failure detectable from inside the engine, which is why it is stated here
// as a requirement on the model instead.

#include <Eigen/SparseCore>

#include <hven/qp/qp_types.h>

namespace hven::solvers {

// The driver-facing NLP. IMPLEMENTER PRECONDITION: eval_hess/eval_jac_e/
// eval_jac_i return an x-INDEPENDENT sparsity pattern, and eval_hess's is
// additionally independent of `obj_scale` and of the lambda_e/lambda_i
// VALUES it is called with -- see this header's STRUCTURAL PATTERN
// INVARIANCE note.
class NlpModel {
  public:
    virtual ~NlpModel() = default;

    /// @brief Number of variables.
    virtual Index n() const = 0;
    /// @brief Number of equality constraints.
    virtual Index me() const = 0;
    /// @brief Number of inequality constraints.
    virtual Index mi() const = 0;

    /// @brief Objective value at x.
    virtual double eval_f(const Vec &x) const = 0;
    /// @brief Objective gradient at x, size n().
    virtual Vec eval_grad(const Vec &x) const = 0;
    /// @brief Equality residuals at x, size me().
    virtual Vec eval_ce(const Vec &x) const = 0;
    /// @brief Inequality residuals at x, size mi().
    virtual Vec eval_ci(const Vec &x) const = 0;

    // The matrix returns below must be compressed: one contiguous value array
    // in canonical order, with sorted, duplicate-free inner indices -- what
    // makeCompressed() leaves, and what setFromTriplets() produces. A consumer
    // pairing stored value k with the kth entry of a pattern it recorded
    // earlier reads a different element in uncompressed storage. An
    // uncompressed return is refused by name at nlp_require_claimed_pattern
    // (detail/model/nlp_adapter.h).

    /// @brief Exact Lagrangian Hessian at x, upper triangle only (see this
    ///        header's EXACT LAGRANGIAN HESSIAN note).
    virtual SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                              const Vec &lambda_i) const = 0;

    /// @brief Equality Jacobian at x, me() by n().
    virtual Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const = 0;
    /// @brief Inequality Jacobian at x, mi() by n().
    virtual Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const = 0;

    // The in-place forms of the six evaluations above, under the same opt-in
    // contract eval_values carries: each has a default implementation that
    // calls the by-value counterpart and assigns into the caller's storage,
    // so no model has to write one and no existing model changes. An override
    // exists only to be cheaper than that baseline, never to return a
    // different number; it must leave the caller's destination holding exactly
    // what the by-value counterpart would have returned -- for the two matrix
    // forms, COMPRESSED per the note above. Callers own the destination and
    // may reuse it across calls.

    /// @brief eval_grad into caller-owned storage.
    /// @param x The iterate.
    /// @param out Receives the gradient, size n().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_grad_in_place(const Vec &x, Vec &out) const { out = this->eval_grad(x); }

    /// @brief eval_ce into caller-owned storage.
    /// @param x The iterate.
    /// @param out Receives the equality residuals, size me().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_ce_in_place(const Vec &x, Vec &out) const { out = this->eval_ce(x); }

    /// @brief eval_ci into caller-owned storage.
    /// @param x The iterate.
    /// @param out Receives the inequality residuals, size mi().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_ci_in_place(const Vec &x, Vec &out) const { out = this->eval_ci(x); }

    /// @brief eval_jac_e into caller-owned storage.
    /// @param x The iterate.
    /// @param out Receives the equality Jacobian, me() by n().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const {
        out = this->eval_jac_e(x);
    }

    /// @brief eval_jac_i into caller-owned storage.
    /// @param x The iterate.
    /// @param out Receives the inequality Jacobian, mi() by n().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const {
        out = this->eval_jac_i(x);
    }

    /// @brief eval_hess into caller-owned storage.
    /// @param x The iterate.
    /// @param obj_scale The objective scale.
    /// @param lambda_e Equality multipliers, size me().
    /// @param lambda_i Inequality multipliers, size mi().
    /// @param out Receives the Lagrangian Hessian, upper triangle, n() by n().
    /// An override returns exactly what the by-value counterpart returns; the
    /// default delegates to it.
    virtual void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                    const Vec &lambda_i, SpMatRM &out) const {
        out = this->eval_hess(x, obj_scale, lambda_e, lambda_i);
    }

    /// @brief Variable lower bounds, size n(). Unboundedness on a side is
    ///        written -inf / +inf; every finite value is a real bound, however
    ///        large -- nothing in this layer treats a finite magnitude as a
    ///        stand-in for infinity, so a bound of 1e20 constrains the
    ///        variable to 1e20.
    virtual const Vec &lower() const = 0;
    /// @brief Variable upper bounds, size n(); same convention as lower().
    virtual const Vec &upper() const = 0;

    /// @brief The model's suggested starting point, size n().
    virtual Vec start_point() const = 0;

    /// @brief VALUES ONLY -- f(x), cE(x), cI(x): no gradient, no Jacobians,
    ///        for call sites that judge or hash a point without derivatives.
    ///
    /// THE DEFAULT IMPLEMENTATION IS THE OPT-IN CONTRACT: it calls this same
    /// model's own eval_f/eval_ce/eval_ci (respecting the me()==0/mi()==0
    /// skips) and nothing else, so every existing model keeps working,
    /// unmodified. A model override exists ONLY to go faster than that
    /// baseline -- e.g. by sharing sub-expressions across f/cE/cI, or by a
    /// cheaper closed form entirely -- never to return a DIFFERENT number:
    /// whatever this returns must be identical to calling
    /// eval_f(x)/eval_ce(x)/eval_ci(x) directly under value-preserving
    /// compilation; under the library's Release flag regime (fast-math),
    /// agreement is to reassociation residue -- the same last-bit freedom the
    /// compiler already has within any one evaluation. eval_nlp rests its
    /// f/ce/ci fields on the same invariant. tests/sqp/support/scale_problems.h's
    /// F7CollocationChain is the one model in this tree that overrides rather
    /// than taking the default.
    virtual void eval_values(const Vec &x, double &f, Vec &cE, Vec &cI) const {
        f = eval_f(x);
        cE = me() > 0 ? eval_ce(x) : Vec(0);
        cI = mi() > 0 ? eval_ci(x) : Vec(0);
    }
};

/// @brief A model carrying a PARAMETER VECTOR p alongside its variables x: the
///        same NLP, re-posed at a new p by set_parameters().
///
/// The parameters are MODEL STATE, not an argument threaded through every
/// eval_*: every NlpModel method reports the model AT THE CURRENT p, so a solve
/// sees one fixed p from start to finish. set_parameters is therefore non-const
/// and is the ONLY way p changes; a caller that must not disturb an in-flight
/// solve holds the model by const reference, which makes that statically true.
/// parameters() returns the current p, so a caller can save/restore it around
/// a probe.
///
/// PRECONDITIONS ON EVERY IMPLEMENTER, both binding:
///
///   1. STRUCTURAL PATTERN INVARIANCE IN p AS WELL AS IN x: set_parameters may
///      change the VALUES eval_hess/eval_jac_e/eval_jac_i report and must
///      never change their structure -- nor n()/me()/mi(), which are pattern
///      by another name. This is what lets a warm start taken at p be fed
///      into a solve at p + dp at all: the warm-start structural hash keys the
///      cached K0 factorization on pattern alone (see this header's STRUCTURAL
///      PATTERN INVARIANCE note).
///
///   2. set_parameters(p) with p.size() != parameter_dim() is a caller error
///      and must throw std::invalid_argument -- never a silent resize, never a
///      diagnostic that is not also the thrown message.
class ParametricNlpModel : public NlpModel {
  public:
    /// @brief Dimension of the parameter vector.
    virtual Index parameter_dim() const = 0;
    /// @brief Re-poses the NLP at @p p.
    /// @throws std::invalid_argument if p.size() != parameter_dim().
    virtual void set_parameters(const Vec &p) = 0;
    /// @brief The current parameter vector, size parameter_dim().
    virtual Vec parameters() const = 0;
};

} // namespace hven::solvers
