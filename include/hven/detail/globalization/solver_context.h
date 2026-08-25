// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <Eigen/Core>
#include <Eigen/Sparse>

// SolverContext exposes InteriorPointSolver::Settings by reference, which is only
// visible once the InteriorPointSolver class itself has been parsed (Settings is a
// nested struct). The include is deliberately one-directional:
// interior_point_solver.h does not include back into this directory, which keeps
// every header below standalone-compilable without a circular-include trick.
#include "hven/detail/interior/bound_set.h"
#include "hven/detail/interior/eval_error_log.h"
#include "hven/detail/interior/kkt_factorization.h"
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

// Forward declaration only: SolverContext carries a non-owning pointer to the
// active RestorationStrategy (complete type in globalization/restoration.h,
// which includes THIS header — a plain include here would be circular). The
// pointer is null whenever feasibility restoration is off (the default), which
// is exactly when every restoration branch that consults it is provably dead.
class RestorationStrategy;

/// @brief The sparse KKT factorization type, matching InteriorPointSolver's
/// own member declaration. Components never choose or construct this type;
/// they only drive solves/refactors through SolverContext::kkt_solver_, which
/// InteriorPointSolver still owns.
using KktSolverType = KktFactorization;

/// @brief References-only view into the live InteriorPointSolver instance.
///
/// Owns nothing: every member is a reference or non-owning pointer into the
/// live InteriorPointSolver, and a SolverContext must not outlive the solver it
/// was built from. Most call sites construct one fresh (as a temporary) for
/// the duration of a single call. One exception holds its copy as a private
/// member instead of re-threading it through every call — that copy is rebuilt
/// at every solve entry and obeys the same lifetime bound. Components hold no
/// other channel to the solver's persistent state besides this context plus
/// the explicit per-iteration parameters threaded through each interface
/// method.
struct SolverContext {
    /// Raw non-owning pointer to the NLP under solve; the solver retains the
    /// owning shared_ptr. Read by trial-point evaluation and barrier-Hessian
    /// call sites.
    NonLinearProgram *nlp_;

    /// Reference to the live KKT factorization object (solver retains
    /// ownership): read (solve) by the PROBE predictor, written
    /// (factor/refactor) by inertia-ladder dispatch.
    KktSolverType &kkt_solver_;

    /// One const reference to the full settings bundle rather than per-field
    /// plumbing: every live call site already reads it as ctx.settings_.<field>,
    /// and each component consumes a disjoint subset.
    const InteriorPointSolver::Settings &settings_;

    /// References (not copies) into the solver's dimension members — fixed
    /// for the lifetime of a solve, but always observed live, never snapshotted.
    const int &primal_vars_;
    const int &slack_vars_;
    const int &equal_cons_;
    const int &inequal_cons_;
    const int &kkt_dim_;

    /// Complementarity scratch: read+write by the barrier governor's
    /// complementarity evaluation, backed by the SAME solver-owned buffer to
    /// avoid per-call heap allocation. The only scratch buffer reached through
    /// the context.
    Eigen::VectorXd &stli_scratch_;

    /// Trial-point evaluation scratch in declaration space (the solver iterates
    /// in the reduced space): the same solver-owned buffer its own evaluation
    /// dispatch uses — one evaluation in flight at a time, consumed before the
    /// building call returns. Stays empty when no variable is eliminated.
    Eigen::VectorXd &declaration_primals_scratch_;

    /// Non-owning pointer to the active RestorationStrategy, nullptr when
    /// restoration is off (the default). Consulted by the trial-point
    /// evaluators (proximal objective term while restoration is active) and by
    /// the feasibility-switch entry test. Defaulted nullptr keeps existing
    /// braced-init call sites compiling; those sites, and the whole default
    /// solve path, stay restoration-free and bit-identical.
    const RestorationStrategy *restoration_ = nullptr;

    /// Non-owning pointer to the solver's trial-evaluation exception log;
    /// recording sites null-guard, nullptr in isolation. Defaulted nullptr
    /// keeps bare braced-init constructions (unit tests) compiling.
    EvalErrorLog *eval_errors_ = nullptr;

    /// Native primal variable bounds (NLP-owned classification, reduced index
    /// space) and their matching multiplier state. Set only on the
    /// configuration success path and only when the set is non-empty, so a
    /// problem with no variable bounds leaves both null and every component's
    /// bound branch is provably unreachable — which is what makes assembly on
    /// such a problem byte-identical. Every reader null-guards.
    const BoundSet *bounds_ = nullptr;

    /// Multiplier state paired with bounds_; see above.
    BoundDualState *bound_duals_ = nullptr;
};

} // namespace hven::solvers
