// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <string>

#include <Eigen/Core>

#include "hven/detail/interior/typedefs/eigen_types.h"

namespace hven::solvers {

/// @brief Solver-neutral sparse NLP in the Ipopt TNLP shape:
///
///        min  f(x)   s.t.  g_lower <= g(x) <= g_upper,  x_lower <= x <= x_upper
///
/// Subclass this (in C++ or Python) and hand it to NLPSolver. Conventions are
/// Ipopt's, verbatim: the Lagrangian is L = obj_factor*f + lambda^T g; eval_hess
/// fills the LOWER TRIANGLE of grad^2 L (row >= col); lambda is in THIS
/// problem's own row space in every signature here. Rows with
/// g_lower == g_upper are equalities; +/-infinity means unbounded on that side;
/// rows with two finite, unequal bounds are handled (internally split);
/// rows unbounded on both sides are dropped.
///
/// Structures are queried once at setup and must not change afterwards.
/// Evaluation callbacks must be pure (same x -> same values): results are
/// cached per iterate. Duplicate (row, col) entries in a structure are legal;
/// their values are summed.
///
/// Where the callbacks are first called: NLPSolver's transcription, before any
/// solve iterate exists, calls eval_jac once and eval_hess once at a point it
/// chooses -- the origin projected onto the declared variable bounds. Their
/// values are discarded; what setup takes from them is the sparsity pattern,
/// which is the only way the solver can learn it once the problem has been
/// converted. eval_f, eval_grad_f and eval_g are not called at that point.
/// So eval_jac and eval_hess must be defined at that point, not only at
/// iterates the caller supplies -- the projected origin can sit on a
/// singularity a solve would never visit. Make those two callbacks total
/// there, or declare variable bounds that exclude the singularity.
///
/// Bounds reach the solver verbatim: no value is rescaled, clipped, or
/// reinterpreted on the way in or on the way back out; unboundedness on a side
/// is written -inf / +inf, and every finite value is a real bound, however
/// large. This is where hven parts company with Ipopt, whose
/// `nlp_lower_bound_inf` / `nlp_upper_bound_inf` cutoff (default magnitude
/// 1e19) makes any bound past the cutoff mean "unbounded": a problem carried
/// over from Ipopt that spells unboundedness as +/-1e20 arrives here as
/// genuine bounds at +/-1e20 rather than free. One route does apply a cutoff:
/// the SSN/QP engine treats a bound of magnitude kSsnInfBound (1e20) or larger
/// as absent, so a genuine bound in [1e20, inf) is honored by the
/// interior-point route and not by that one.
class NLPProblem {
  public:
    virtual ~NLPProblem() = default;

    /// @brief Number of variables.
    virtual int num_vars() const = 0;

    /// @brief Number of constraints; 0 (unconstrained) is legal.
    virtual int num_cons() const = 0;

    /// @brief Number of Jacobian nonzeros; must be 0 when num_cons() == 0.
    virtual int num_jac_nonzeros() const = 0;

    /// @brief Number of Hessian nonzeros, lower triangle of the Lagrangian
    ///        Hessian.
    virtual int num_hess_nonzeros() const = 0;

    /// @brief Declared variable and constraint bounds. -inf / +inf means
    ///        unbounded on that side; every finite value is a real bound.
    virtual void bounds(Eigen::Ref<Eigen::VectorXd> x_lower, Eigen::Ref<Eigen::VectorXd> x_upper,
                        Eigen::Ref<Eigen::VectorXd> g_lower,
                        Eigen::Ref<Eigen::VectorXd> g_upper) const = 0;

    /// @brief Objective value at x. Must be pure.
    virtual void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const = 0;

    /// @brief Objective gradient at x. Must be pure.
    virtual void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                             Eigen::Ref<Eigen::VectorXd> grad) const = 0;

    /// @brief Constraint values at x. Must be pure.
    virtual void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const = 0;

    /// @brief Jacobian sparsity pattern as (row, col) triplets, 0-based;
    ///        queried once at setup. Duplicates legal, their values summed.
    virtual void jac_structure(Eigen::Ref<Eigen::VectorXi> rows,
                               Eigen::Ref<Eigen::VectorXi> cols) const = 0;

    /// @brief Hessian sparsity pattern as (row, col) triplets, lower triangle,
    ///        0-based; queried once at setup. Duplicates legal, summed.
    virtual void hess_structure(Eigen::Ref<Eigen::VectorXi> rows,
                                Eigen::Ref<Eigen::VectorXi> cols) const = 0;

    /// @brief Jacobian values, same slot ordering as jac_structure().
    virtual void eval_jac(ConstEigenRef<Eigen::VectorXd> x,
                          Eigen::Ref<Eigen::VectorXd> vals) const = 0;

    /// @brief Lagrangian-Hessian values, same slot ordering as hess_structure().
    virtual void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                           ConstEigenRef<Eigen::VectorXd> lambda,
                           Eigen::Ref<Eigen::VectorXd> vals) const = 0;

    /// @brief Optional multiplier seed for the first optimality-mode phase of a
    ///        solve sequence (the primal guess is a solve argument). Return
    ///        true and fill @p lambda to seed the solver's constraint
    ///        multipliers; the default leaves the solver's own initialization
    ///        untouched. A sequence with no such phase (a bare solve())
    ///        validates and then discards the seeds.
    virtual bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const {
        (void)lambda;
        return false;
    }

    /// @brief Problem name, used in diagnostics.
    virtual std::string name() const { return "NLPProblem"; }
};

} // namespace hven::solvers
