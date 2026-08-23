// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <stdexcept>
#include <vector>

#include <Eigen/Core>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/interior/iterate_info.h"
// The complete InteriorPointSolver class is required here (classic_line_search
// takes its nested LineSearchModes by value, and a nested enum cannot be
// forward-declared independently of its enclosing class). This include is
// deliberately one-directional: interior_point_solver.h does not include back
// into this directory.
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// @brief Relative infeasibility-reduction floor for leaving feasibility
/// restoration: a trial's constraint violation must fall to
/// kKappaResto * theta_ref to be eligible to exit (consumers may apply a
/// tolerance floor on top). Ipopt option "required_infeasibility_reduction",
/// shipped default 0.9. Shared by the strategies whose exit test uses it;
/// both compile into the same translation unit, so exactly one definition
/// must exist.
inline constexpr double kKappaResto = 0.9;

/// @brief Decides whether a trial step is accepted.
///
/// An instance holds no solver-owned state (iterate vectors, barriers,
/// histories): those are passed per call or reached through SolverContext.
/// Strategy-internal state is permitted but must be cleared in reset().
class AcceptanceStrategy {
  public:
    virtual ~AcceptanceStrategy() = default;

    /// @brief Acceptance test for the generic driving path: is the trial
    /// point's (theta, f) pair acceptable relative to the current iterate?
    /// Driven whenever a non-classic strategy is selected.
    /// @param step_length Trial alpha in (0, 1], the live ladder value from
    ///   the backtracking loop that produced @p trial.
    /// @param predicted_reduction Stays alpha-scaled; recover the raw
    ///   directional derivative as predicted_reduction.objective / step_length.
    virtual bool is_iterate_acceptable(const ProgressMeasures &current,
                                        const ProgressMeasures &trial,
                                        const ProgressMeasures &predicted_reduction,
                                        double objective_multiplier, double step_length) = 0;

    /// @brief Restoration-exit test: has infeasibility been reduced enough,
    /// relative to @p reference (the point restoration was entered from), to
    /// leave restoration mode? Driven once per restoration mode by the exit
    /// test call sites; near-feasible exits are plain threshold tests and do
    /// not consult the strategy.
    virtual bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                                        const ProgressMeasures &trial) const = 0;

    /// @brief mu-event / phase-change hook: called whenever the solver starts
    /// a new phase or resets the barrier parameter. Stateful strategies clear
    /// their persistent state here. One deliberate exception: while a strategy
    /// is in the feasibility phase, reset() may let stashed optimality-phase
    /// state survive, because the restoration-exit test reduces against it.
    virtual void reset() = 0;

    /// @brief Selects which driving path GlobalizationMechanism::compute_step
    /// uses.
    /// @retval true Fused classic path: compute_step forwards straight to
    ///   classic_line_search, whose own backtracking loop hosts the merit test.
    /// @retval false Generic path: compute_step runs the loop itself (trial
    ///   evaluation -> ProgressMeasures -> is_iterate_acceptable -> backtrack)
    ///   and never calls classic_line_search.
    /// Pure virtual on purpose: a defaulted answer would let a new strategy
    /// silently inherit the classic path and hit classic_line_search's
    /// throwing default at solve time instead of failing to compile.
    virtual bool drives_classic_path() const = 0;

    /// @brief Mode-switch notification, called when the solver enters
    /// feasibility restoration; the argument is the ProgressMeasures at the
    /// switch point (the entry point on the way in).
    /// Default no-op: strategies whose acceptance state survives the objective
    /// swap need no action.
    virtual void notify_switch_to_feasibility(const ProgressMeasures &) {}

    /// @brief Mode-switch notification, called when the solver leaves
    /// feasibility restoration; the argument is the ProgressMeasures at the
    /// switch point (the exit point on the way out).
    /// Default no-op.
    virtual void notify_switch_to_optimality(const ProgressMeasures &) {}

    /// @brief Writes this strategy's diagnostic state (if any) into
    /// @p result. Called once per phase, right after that phase's solve
    /// returns, so a multi-phase solve ends up with the last phase's values.
    ///
    /// Invariant: WRITE-ONLY by design. This hook never reads @p result or any
    /// other solver state, so it cannot influence control flow; a strategy
    /// that changed behavior based on prior diagnostics would need a real
    /// feedback path. The default body is a no-op.
    virtual void append_diagnostics(InteriorPointSolver::SolveResult &result) const {
        (void)result;
    }

    /// @brief Classic fused entry point: loop and merit test in one call.
    /// Operates on raw Eigen::VectorXd blocks (no named-segment views); a
    /// future implementation reconstructs any view types internally if
    /// needed. Only the classic-path implementation overrides this; generic
    /// strategies never call it.
    ///
    /// Deliberately NOT split into separate "step" and "accept" calls: the
    /// per-variant loops host the merit test as their own exit condition, and
    /// splitting them would re-derive trial-point evaluation at a new seam,
    /// risking reordering of floating-point operations downstream gates pin.
    /// Returns the accepted step-length alpha.
    /// @throws std::logic_error Always, in the default body: reaching it means
    ///   a non-classic strategy was driven through the classic path.
    virtual double classic_line_search(InteriorPointSolver::LineSearchModes lsmode,
                                       double obj_scale, double mu, double prim_obj,
                                       double barr_obj, Eigen::VectorXd &XSL, Eigen::VectorXd &DXSL,
                                       Eigen::VectorXd &XSL2, Eigen::VectorXd &RHS,
                                       Eigen::VectorXd &RHS2, IterateInfo &Citer,
                                       const std::vector<IterateInfo> &iters) {
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
        (void)Citer;
        (void)iters;
        throw std::logic_error(
            "classic_line_search is only implemented by ClassicMeritAcceptance");
    }
};

} // namespace hven::solvers
