// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <stdexcept>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/solver_context.h"
#include "hven/detail/interior/iterate_info.h"
// InteriorPointSolver::LineSearchModes (forwarded to AcceptanceStrategy::classic_line_search)
// requires the complete InteriorPointSolver class; see acceptance_strategy.h's include note.
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// @brief Owns the fraction-to-boundary + backtracking sequence for one step
/// proposal, then dispatches acceptance.
///
/// Holds no solver state; reset() is the mu-event/phase-change hook.
class GlobalizationMechanism {
  public:
    virtual ~GlobalizationMechanism() = default;

    /// @brief Computes the fraction-to-boundary step (alphap, alphad), scales
    /// DXSL by it IN PLACE, then runs the acceptance backtrack on the scaled
    /// DXSL.
    ///
    /// Callout: DXSL is mutated in place between the negate and the commit,
    /// and both the trial points (xsl + alpha*dxsl) and the eventual commit
    /// (XSL += alpha*DXSL) operate on that same already-scaled DXSL. Scaling
    /// and backtracking are deliberately fused into this one call: splitting
    /// them across an interface boundary would let a future boundary reorder
    /// those multiplications, which breaks the bit-identical-iteration-count
    /// gate.
    ///
    /// XSL/DXSL/XSL2/RHS/RHS2 are raw Eigen::VectorXd blocks viewed via
    /// hven::solvers::KKTVector internally. bfrac and the step strategy are
    /// read from ctx.settings_ (persistent settings, not per-call transients).
    /// lsmode/obj_scale/mu/prim_obj/barr_obj are forwarded verbatim to the
    /// acceptance strategy once scaling has been applied.
    ///
    /// @param alphap Out: fraction-to-boundary primal step.
    /// @param alphad Out: fraction-to-boundary dual step.
    /// @return Final backtracked alpha.
    virtual double compute_step(InteriorPointSolver::LineSearchModes lsmode, double obj_scale,
                                double mu, double prim_obj, double barr_obj, Eigen::VectorXd &XSL,
                                Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                                Eigen::VectorXd &RHS2, AcceptanceStrategy &acceptance,
                                double &alphap, double &alphad, IterateInfo &Citer,
                                const std::vector<IterateInfo> &iters, SolverContext &ctx) = 0;

    /// @brief Fraction-to-boundary primal/dual step scaling alone — the first
    /// half of compute_step without the acceptance backtrack. MUTATES DXSL in
    /// place. Exists for callers that need the scaling without a backtrack:
    /// the barrier governor's PROBE predictor consumes a DXSL already scaled
    /// to the boundary (that scaling is barrier/IPM logic independent of the
    /// globalization strategy), and SOC correction re-scales a corrected
    /// direction before re-testing acceptance. bfrac and the problem dims are
    /// read from ctx.
    ///
    /// Numerical caveat: runs only inside alg_impl's GoodStep branch, so on a
    /// non-finite DXSL the recorded per-iterate alpha_p_/alpha_d_ diagnostics
    /// stay at their 1.0 init values instead of values derived from -Inf/NaN
    /// entries; print-only exposure on diverging runs — iterates, mu, and
    /// iteration counts are unaffected (DXSL is discarded before the state
    /// commit on that path).
    virtual void max_primal_dual_step(Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL, double bfrac,
                                       double &alphap, double &alphad, const SolverContext &ctx) = 0;

    /// @brief Runs ONLY the acceptance backtrack on an already-scaled DXSL —
    /// compute_step's second half without its first. Recovery links (SOC /
    /// extended backtracking) call this to re-drive acceptance on a corrected
    /// direction so a corrected trial is tested against the SAME criteria as
    /// the ordinary step, classic or generic. Recovery links must not call
    /// AcceptanceStrategy::classic_line_search directly (it throws on generic
    /// strategies); only the mechanism can reach the generic driving path,
    /// which owns the trial-point evaluation.
    /// @throws std::logic_error Always, in the default body: a mechanism that
    ///   hosts a generic driving path must override it.
    virtual double run_acceptance_backtrack(
        InteriorPointSolver::LineSearchModes lsmode, double obj_scale, double mu, double prim_obj,
        double barr_obj, Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL, Eigen::VectorXd &XSL2,
        Eigen::VectorXd &RHS, Eigen::VectorXd &RHS2, AcceptanceStrategy &acceptance,
        IterateInfo &Citer, const std::vector<IterateInfo> &iters, SolverContext &ctx) {
        (void)lsmode;
        (void)obj_scale;
        (void)mu;
        (void)prim_obj;
        (void)barr_obj;
        (void)XSL;
        (void)DXSL;
        (void)XSL2;
        (void)RHS;
        (void)RHS2;
        (void)acceptance;
        (void)Citer;
        (void)iters;
        (void)ctx;
        throw std::logic_error("GlobalizationMechanism::run_acceptance_backtrack is only "
                               "implemented by mechanisms that host an acceptance backtrack "
                               "(BacktrackingLineSearch)");
    }

    /// @brief mu-event / phase-change hook (mirrors
    /// AcceptanceStrategy::reset()); implementations clear any persistent
    /// state here.
    virtual void reset() = 0;
};

} // namespace hven::solvers
