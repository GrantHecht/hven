// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <string>

#include <hven/core/types.h>

namespace hven::solvers {

/// @brief The sizes an Nlp declares.
struct NlpDimensions {
    Index n = 0;                 ///< variables; at least 1
    Index m = 0;                 ///< constraint rows; 0 is legal
    Index jacobian_nonzeros = 0; ///< entries of jacobian_structure(); 0 when m == 0
    Index hessian_nonzeros = 0;  ///< entries of hessian_structure(), lower triangle
};

/// @brief The callback NLP, in the shape of Ipopt's TNLP:
///
///        min f(x)   s.t.   g_lower <= g(x) <= g_upper,   x_lower <= x <= x_upper
///
/// The one public problem contract. Wrap an instance in a Problem
/// (model/problem.h) and hand the Problem to IpmSolver or SqpSolver.
///
/// Conventions.
/// - The Lagrangian is L = obj_factor * f(x) + lambda^T g(x), with one multiplier
///   per row, in this problem's row order (Ipopt's sign convention).
/// - eval_hessian fills the LOWER triangle (row >= col) of the Lagrangian Hessian.
/// - Structures are (row, col) pairs, 0-based. Duplicate pairs are legal; their
///   values are summed.
/// - A bound of -kInfinity / +kInfinity is absent. Every finite value is a real bound.
/// - A row with g_lower == g_upper is an equality; a row with two infinite bounds is
///   free and is not passed to an engine.
///
/// What may change between solves. Every solve re-reads dimensions(), both
/// structures and bounds(), and clears the engines' per-iterate caches, so values and
/// bounds may change between solves (a parameter is a member of the subclass). Within
/// one solve the callbacks must be pure: the same x gives the same values. The
/// dimensions, the structures and the kind of each row (equality, upper-bounded,
/// lower-bounded, range, free) may not change; a solve that finds them changed is
/// refused by name.
///
/// Where the callbacks are first called. The interior-point transcription evaluates
/// eval_jacobian and eval_hessian once, at the origin projected onto the variable
/// bounds, and keeps only the sparsity pattern; both must be defined there.
///
/// Evaluation failure. A callback that cannot evaluate at a point throws. The
/// interior-point engine treats a throw at a trial point as a rejected trial; the
/// SQP engine propagates it.
///
/// Thread safety. Problems that share one Nlp may solve concurrently on separate
/// threads only if the Nlp's const members are safe to call concurrently.
class Nlp {
  public:
    virtual ~Nlp() = default;

    /// @brief The problem's sizes. Re-queried at every solve.
    virtual NlpDimensions dimensions() const = 0;

    /// @brief The variable and row bounds. Read at every solve.
    /// @param x_lower n entries.
    /// @param x_upper n entries.
    /// @param g_lower m entries.
    /// @param g_upper m entries.
    virtual void bounds(VecRef x_lower, VecRef x_upper, VecRef g_lower, VecRef g_upper) const = 0;

    /// @brief The objective at @p x.
    virtual double eval_f(ConstVecRef x) const = 0;

    /// @brief The objective gradient at @p x, n entries.
    virtual void eval_grad_f(ConstVecRef x, VecRef grad) const = 0;

    /// @brief The row values at @p x, m entries. The default serves m == 0 and
    ///        throws std::logic_error when m > 0.
    virtual void eval_g(ConstVecRef x, VecRef g) const;

    /// @brief The Jacobian structure, jacobian_nonzeros entries each. The default
    ///        serves m == 0 and throws std::logic_error otherwise.
    virtual void jacobian_structure(IndexVecRef rows, IndexVecRef cols) const;

    /// @brief The Jacobian values at @p x, in jacobian_structure() order. The
    ///        default serves m == 0 and throws std::logic_error otherwise.
    virtual void eval_jacobian(ConstVecRef x, VecRef values) const;

    /// @brief Whether eval_hessian is provided. A problem without one may be solved
    ///        by a quasi-Newton SQP; the interior-point engine and an exact-Hessian
    ///        SQP refuse it by name. Quasi-Newton never reads the Hessian, restoration
    ///        included: eval_hessian() is not called.
    virtual bool has_hessian() const { return true; }

    /// @brief The Lagrangian-Hessian structure, lower triangle, hessian_nonzeros
    ///        entries each. The default serves hessian_nonzeros == 0 and throws
    ///        std::logic_error otherwise.
    virtual void hessian_structure(IndexVecRef rows, IndexVecRef cols) const;

    /// @brief The Lagrangian-Hessian values at @p x, in hessian_structure() order.
    /// @param obj_factor The objective's weight in the Lagrangian.
    /// @param lambda One multiplier per row.
    /// The default serves hessian_nonzeros == 0 and throws std::logic_error
    /// otherwise.
    virtual void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                              VecRef values) const;

    /// @brief The problem's name, used in diagnostics.
    virtual std::string name() const { return "nlp"; }
};

} // namespace hven::solvers
