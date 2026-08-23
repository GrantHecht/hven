// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// aggregate_views.h — the interior-point solver's side of the Level 2 model
// contract: the candidate point's primal block and the scatter views its
// evaluations fill.
//
// Written once here because two consumers build them: InteriorPointSolver's own
// evaluation dispatch and the globalization components, which evaluate trial
// points through the same aggregate. Both slice the same compound
// [primals | slacks | eq | iq] layout and address the same published claim
// tables.

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/core/types.h"
#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/model/non_linear_program.h"

namespace hven::solvers::detail {

/// @brief The empty multiplier block a values-only candidate point carries.
///
/// Empty means "no multipliers", not "zeros"; a request that reads the
/// multipliers is refused an empty block by the assemble entry.
/// @return A reference to a shared empty vector, valid for the process lifetime.
inline const Vec &no_multipliers() {
    static const Vec empty;
    return empty;
}

/// @brief Expand a solver-space primal block into declaration space.
///
/// Every vector on the contract's surface is indexed by declared variable
/// identity, while the solver iterates in the reduced space the fixed-variable
/// treatment left. Returns a view of @p reduced itself when no variable is
/// eliminated; otherwise fills @p scratch through NonLinearProgram::
/// scatter_full_x, which produces the same buffer NonLinearProgram::
/// primal_view() builds from the same iterate.
///
/// @p scratch is caller-owned and is reused across calls. A returned view is
/// valid until the next call made against that same scratch, which is the same
/// discipline the engine's own primal_view() keeps: one evaluation is in flight
/// at a time, and the view is consumed before the call returns.
/// @param nlp      The aggregate the point will be evaluated against.
/// @param scratch  Caller-owned buffer, resized as needed.
/// @param reduced  Primal block in the solver's own space.
/// @return A declaration-space view.
inline Eigen::Ref<const Vec> declaration_primals(NonLinearProgram &nlp, Vec &scratch,
                                                 ConstEigenRef<Vec> reduced) {
    if (!nlp.is_reduced()) {
        return reduced;
    }
    scratch.resize(nlp.primal_vars_);
    nlp.scatter_full_x(reduced, scratch);
    return scratch;
}

/// @brief Build a right-hand-side scatter view over the compound solver layout.
///
/// All four arenas are supplied; a request writes only the arenas it names.
/// @param nlp           The aggregate whose claim tables address the arenas.
/// @param val           Objective out slot.
/// @param gx            Objective-gradient destination; its first
///                      @p primal_vars rows are the arena.
/// @param agxs_fx       Adjoint-gradient and residual destination, in the
///                      [primals | slacks | eq | iq] layout.
/// @param primal_vars   Solver primal width.
/// @param slack_vars    Slack count.
/// @param equal_cons    Equality row count.
/// @param inequal_cons  Inequality row count.
/// @return The scatter view.
inline RhsScatterView compound_rhs_scatter_view(NonLinearProgram &nlp, double &val,
                                                EigenRef<Vec> gx, EigenRef<Vec> agxs_fx,
                                                int primal_vars, int slack_vars, int equal_cons,
                                                int inequal_cons) {
    RhsScatterView rhs;
    rhs.objective_ = &val;
    rhs.objective_gradient_ = RhsArenaView{gx.data(), primal_vars, &nlp.objective_gradient_table()};
    rhs.constraint_adjoint_gradient_ =
        RhsArenaView{agxs_fx.data(), primal_vars, &nlp.constraint_adjoint_gradient_table()};
    rhs.equality_residuals_ = RhsArenaView{agxs_fx.data() + primal_vars + slack_vars, equal_cons,
                                           &nlp.equality_residual_table()};
    rhs.inequality_residuals_ = RhsArenaView{agxs_fx.data() + (agxs_fx.size() - inequal_cons),
                                             inequal_cons, &nlp.inequality_residual_table()};
    return rhs;
}

/// @brief Build a right-hand-side scatter view naming only the residual arenas.
///
/// For requests that produce the objective value and both residual blocks and
/// no gradient. The two gradient arenas are left empty, which the assemble
/// entry permits for arenas a request does not name.
/// @param nlp            The aggregate whose claim tables address the arenas.
/// @param val            Objective out slot.
/// @param eq_residuals   Equality-residual destination, @p equal_cons rows.
/// @param iq_residuals   Inequality-residual destination, @p inequal_cons rows.
/// @param equal_cons     Equality row count.
/// @param inequal_cons   Inequality row count.
/// @return The scatter view.
inline RhsScatterView residual_rhs_scatter_view(NonLinearProgram &nlp, double &val,
                                                double *eq_residuals, double *iq_residuals,
                                                int equal_cons, int inequal_cons) {
    RhsScatterView rhs;
    rhs.objective_ = &val;
    rhs.equality_residuals_ =
        RhsArenaView{eq_residuals, equal_cons, &nlp.equality_residual_table()};
    rhs.inequality_residuals_ =
        RhsArenaView{iq_residuals, inequal_cons, &nlp.inequality_residual_table()};
    return rhs;
}

/// @brief Build a KKT scatter view over an assembly buffer.
///
/// The assemble entry checks the view against the destination the engine bound
/// at analysis time, so @p kkt must be the matrix the current
/// NonLinearProgram::analyze_sparsity() call was run against.
/// @param nlp  The aggregate whose KKT location table addresses the buffer.
/// @param kkt  Assembly buffer.
/// @return The scatter view.
inline KktScatterView kkt_scatter_view(NonLinearProgram &nlp,
                                       Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt) {
    return KktScatterView{kkt.valuePtr(), static_cast<int>(kkt.nonZeros()),
                          &nlp.kkt_location_table()};
}

} // namespace hven::solvers::detail
