// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <cassert>
#include <utility>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/globalization_mechanism.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"
#include "hven/detail/interior/kkt_vector.h"
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// @brief The classic globalization mechanism: fraction-to-boundary primal/dual
/// step-length scaling followed by an AcceptanceStrategy backtrack on the
/// scaled search direction.
///
/// Holds no solver-owned state (GlobalizationMechanism's ownership rule); the
/// only exceptions are scratch backing storages, which exist to avoid
/// per-backtrack heap allocation and are not solver state and not cleared by
/// reset().
class BacktrackingLineSearch : public GlobalizationMechanism {
  public:
    BacktrackingLineSearch() = default;

    /// Fused fraction-to-boundary scaling + acceptance backtrack — see
    /// GlobalizationMechanism::compute_step for why the two halves are fused.
    double compute_step(InteriorPointSolver::LineSearchModes lsmode, double obj_scale, double mu,
                        double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                        Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                        Eigen::VectorXd &RHS2, AcceptanceStrategy &acceptance, double &alphap,
                        double &alphad, IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                        SolverContext &ctx) override;

    /// mu-event / phase-change hook — no-op: classic backtracking carries no
    /// persistent decision state across iterations.
    void reset() override {}

    /// Fraction-to-boundary primal/dual step. Public so the PROBE barrier
    /// block's predictor call site can drive it directly on the predictor DXSL;
    /// builds the KKTVector view over the raw XSL/DXSL blocks internally and
    /// MUTATES DXSL in place. bfrac / dims / step strategy are read through ctx.
    void max_primal_dual_step(Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL, double bfrac,
                              double &alphap, double &alphad, const SolverContext &ctx) override;

    /// Acceptance backtrack only (no fraction-to-boundary scaling), dispatching
    /// classic-vs-generic per the strategy; compute_step's tail delegates here
    /// so the dispatch lives in one place. On the classic path this forwards to
    /// acceptance.classic_line_search; on the generic path it runs
    /// generic_line_search.
    double run_acceptance_backtrack(InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                                    double mu, double prim_obj, double barr_obj,
                                    Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL,
                                    Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                                    Eigen::VectorXd &RHS2, AcceptanceStrategy &acceptance,
                                    IterateInfo &Citer, const std::vector<IterateInfo> &iters,
                                    SolverContext &ctx) override;

  private:
    /// @brief Creates a KKTVector view over a VectorXd using the context's
    /// dimensions. The view type is shared with the solver and the sibling
    /// components, so the moved step body keeps its named-segment accessors
    /// unchanged.
    static KKTVector kkt_view(Eigen::VectorXd &v, const SolverContext &ctx) {
        return KKTVector(v, ctx.primal_vars_, ctx.slack_vars_, ctx.equal_cons_, ctx.inequal_cons_);
    }

    /// @brief Scalar fraction-to-boundary step for one block of
    /// strictly-positive quantities and their step.
    /// @param count Block length, passed explicitly: the variable-bound
    ///   distances/multipliers share this kernel but their length is the bound
    ///   set's, not the constraint count's. The slack-block call sites pass
    ///   ctx.inequal_cons_, keeping their trip count unchanged.
    double max_step_to_boundary(Eigen::Ref<Eigen::VectorXd> SLI, Eigen::Ref<Eigen::VectorXd> dSLI,
                                double bfrac, int count) const;

    /// @brief Generic driving path (taken when the strategy reports
    /// drives_classic_path() == false). Same backtracking ladder as the
    /// classic path, but the accept/reject verdict comes from
    /// AcceptanceStrategy::is_iterate_acceptable on a ProgressMeasures triple
    /// built from the trial point. Stores the same accepted_ /
    /// first-rejection signals as the classic path so the recovery chain
    /// composes.
    double generic_line_search(InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                               double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                               Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                               Eigen::VectorXd &RHS2, AcceptanceStrategy &acceptance,
                               IterateInfo &Citer, SolverContext &ctx);

    /// Nested-restoration trial-path scratch (dead unless a nested restoration
    /// strategy is active): backs the elastic residual shift added to a
    /// trial's raw residuals in the generic acceptance loop, without
    /// per-backtrack heap allocation.
    Eigen::VectorXd resto_eq_shift_scratch_;
    Eigen::VectorXd resto_iq_shift_scratch_;

    /// Variable-bound fraction-to-boundary scratch (dead unless the problem
    /// declares variable bounds): backs the primal leg's gather (bound
    /// distances/directional derivatives are not contiguous KKT blocks) used
    /// for the lower side and reused for the upper, without a per-iteration
    /// heap allocation.
    Eigen::VectorXd bound_dist_scratch_;
    Eigen::VectorXd bound_dir_scratch_;
};

} // namespace hven::solvers
