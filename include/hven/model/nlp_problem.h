// =============================================================================
// New file in Tycho, carried into hven (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt)
// =============================================================================

#pragma once

#include <string>

#include <Eigen/Core>

#include "hven/detail/interior/typedefs/eigen_types.h"

namespace hven::solvers {

/// Solver-neutral sparse NLP in the Ipopt TNLP shape:
///
///   min  f(x)   s.t.  g_lower <= g(x) <= g_upper,  x_lower <= x <= x_upper
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
/// Where the callbacks are first called. Setup does not only read the
/// structures: NLPSolver's transcription, before any solve iterate exists,
/// calls eval_jac once and eval_hess once at a point it chooses -- the origin
/// projected onto the declared variable bounds. Their values are discarded;
/// what setup takes from them is the sparsity pattern, which is the only way
/// the solver can learn it once the problem has been converted, and which the
/// structures alone cannot supply after duplicate entries have been summed.
/// eval_f, eval_grad_f and eval_g are not called at that point.
///
/// So eval_jac and eval_hess must be defined at that point, not only at
/// iterates the caller supplies. A callback that divides by a coordinate, or
/// takes a root or a logarithm of one, may not be -- the projected origin can
/// sit on a singularity a solve would never visit. Two ways out: make those
/// two callbacks total (they need only produce finite numbers there, since the
/// values are thrown away), or declare variable bounds that exclude the
/// singularity, which moves the projected point with them.
///
/// Migration note, bounds. The bounds declared here reach the solver verbatim:
/// no value is rescaled, clipped, or reinterpreted on the way in or on the way
/// back out. Unboundedness on a side is written -inf / +inf, and every finite
/// value is a real bound, however large. This is where hven parts company with
/// Ipopt, whose `nlp_lower_bound_inf` / `nlp_upper_bound_inf` cutoff (default
/// magnitude 1e19) makes any bound past the cutoff mean "unbounded". A problem
/// carried over from Ipopt that spells unboundedness as +/-1e20 therefore
/// changes meaning here: those rows and variables arrive as genuine bounds at
/// +/-1e20 rather than free, and an equality row declared at +/-1e20 on both
/// sides becomes an equality at 1e20 rather than a dropped free row. Rewrite
/// such declarations to use the infinities.
class NLPProblem {
  public:
    virtual ~NLPProblem() = default;

    // --- Sizing (required) ---
    virtual int num_vars() const = 0;
    virtual int num_cons() const = 0;          // 0 = unconstrained is legal
    virtual int num_jac_nonzeros() const = 0;  // must be 0 when num_cons() == 0
    virtual int num_hess_nonzeros() const = 0; // lower triangle of the Lagrangian Hessian

    // --- Bounds (required; -inf / +inf = unbounded on that side, every finite
    //     value a real bound -- see this class's migration note) ---
    virtual void bounds(Eigen::Ref<Eigen::VectorXd> x_lower, Eigen::Ref<Eigen::VectorXd> x_upper,
                        Eigen::Ref<Eigen::VectorXd> g_lower,
                        Eigen::Ref<Eigen::VectorXd> g_upper) const = 0;

    // --- Evaluation (required; pure) ---
    virtual void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const = 0;
    virtual void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                             Eigen::Ref<Eigen::VectorXd> grad) const = 0;
    virtual void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const = 0;

    // --- Sparsity structures (required; queried once; 0-based) ---
    virtual void jac_structure(Eigen::Ref<Eigen::VectorXi> rows,
                               Eigen::Ref<Eigen::VectorXi> cols) const = 0;
    virtual void hess_structure(Eigen::Ref<Eigen::VectorXi> rows,
                                Eigen::Ref<Eigen::VectorXi> cols) const = 0;

    // --- Derivative values (required; same slot ordering as the structures) ---
    virtual void eval_jac(ConstEigenRef<Eigen::VectorXd> x,
                          Eigen::Ref<Eigen::VectorXd> vals) const = 0;
    virtual void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                           ConstEigenRef<Eigen::VectorXd> lambda,
                           Eigen::Ref<Eigen::VectorXd> vals) const = 0;

    // --- Warm start (the primal guess is a solve argument; multipliers are
    //     optional). Return true and fill lambda to seed the solver's
    //     constraint multipliers; the default leaves the solver's own
    //     initialization untouched. Seeds apply to the first
    //     optimality-mode phase of a solve sequence; a sequence with no
    //     such phase (a bare solve()) validates and then discards them. ---
    virtual bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const {
        (void)lambda;
        return false;
    }

    virtual std::string name() const { return "NLPProblem"; }
};

} // namespace hven::solvers
