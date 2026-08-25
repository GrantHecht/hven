// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <Eigen/Core>

#include <stdexcept>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/solver_context.h"
// InteriorPointSolver::SolveResult requires the complete InteriorPointSolver class; see
// acceptance_strategy.h's include note for why this is a plain, non-circular
// include (interior_point_solver.h does not include this directory back).
#include "hven/drivers/interior_point_solver.h"

namespace hven::solvers {

/// Near-feasible entry-guard factor, adapted from the unscaled
/// 1e-1 * constr_viol_tol member of Ipopt's scaled/unscaled entry-guard pair —
/// see proximal_restoration.h for the single-measure adaptation disclosure.
/// Defined here rather than beside one strategy because the guard is shared:
/// both concrete strategies test against it in entry_permitted(), and so does
/// the feasibility-stage stall seam, through near_feasible() below.
inline constexpr double kNearFeasibleGuardFactor = 0.1;

/// @brief Feasibility-restoration mode-switch interface.
///
/// A RestorationStrategy caches NO live solver state beyond what defines its
/// own mode (for the proximal switch: the primal snapshot / frozen proximal
/// coefficient / per-coordinate scaling captured at entry). Everything else is
/// an explicit per-call parameter or reached through a SolverContext passed to
/// the call.
class RestorationStrategy {
  public:
    virtual ~RestorationStrategy() = default;

    /// @brief Enter restoration mode.
    /// @param reference The (theta, f) pair of the point restoration was
    ///   entered from — the eventual exit test compares later trials against it
    ///   via reference().
    /// @param primals Snapshotted (copied, not referenced) as the restoration
    ///   mode's defining center point.
    /// @param mu Barrier parameter live at entry, used to derive whatever
    ///   mode-specific state is frozen at switch time (for the proximal switch:
    ///   the coefficient zeta, set ONCE here and never re-derived from live mu).
    virtual void enter_restoration(const ProgressMeasures &reference,
                                    const Eigen::Ref<const Eigen::VectorXd> &primals,
                                    double mu) = 0;

    /// @brief Leave restoration mode. This call only deactivates the mode;
    /// the solver-side multiplier reset around a nested-restoration exit ships
    /// at the call site — deliberately kept out of this interface because it is
    /// solver state, not restoration-strategy state.
    virtual void exit_restoration() = 0;

    /// Whether restoration mode is currently active.
    virtual bool is_active() const = 0;

    /// Full clear: deactivates, drops the entry snapshot, zeroes the per-phase
    /// counters. mu-event / phase-change hook, matching the other globalization
    /// interfaces.
    virtual void reset() = 0;

    // --- Evaluation surface the solver seam consumes while active ---

    /// The proximal term's contribution to the objective at primals.
    virtual double proximal_objective(const Eigen::Ref<const Eigen::VectorXd> &primals) const = 0;

    /// Accumulates the proximal term's gradient into grad_out (ADDED, not
    /// overwritten — the caller's objective gradient is already there).
    virtual void add_proximal_gradient(const Eigen::Ref<const Eigen::VectorXd> &primals,
                                        Eigen::Ref<Eigen::VectorXd> grad_out) const = 0;

    /// The proximal term's (diagonal) contribution to the primal Hessian block.
    virtual const Eigen::VectorXd &proximal_diagonal() const = 0;

    /// Is this violation already at the near-feasible floor, where a real
    /// restoration episode is not warranted? Non-virtual: the rule does not
    /// vary by strategy. Used by both concrete entry_permitted()
    /// implementations, and by the feasibility-stage stall seam to tell an
    /// endgame (constraints at their floor while the barrier residual still
    /// grinds down) from a genuine stall.
    bool near_feasible(double constraint_violation, const SolverContext &ctx) const {
        return constraint_violation <= kNearFeasibleGuardFactor * ctx.settings_.econ_tol_;
    }

    /// @brief Entry-permission test: may the solver enter restoration right now?
    /// False refuses entry — either the point is already near-feasible or this
    /// phase's restoration budget (ctx.settings_.max_feas_rest_) is exhausted.
    /// Virtual with a shared default body: both shipped strategies use exactly
    /// this guard + budget test and do not override it; test doubles override
    /// it directly for controllability.
    virtual bool entry_permitted(double constraint_violation, const SolverContext &ctx) const {
        if (near_feasible(constraint_violation, ctx)) {
            return false;
        }
        if (entries_ >= ctx.settings_.max_feas_rest_) {
            return false;
        }
        return true;
    }

    /// The (theta, f) pair restoration was entered from.
    virtual const ProgressMeasures &reference() const = 0;

    /// Increments the per-phase iterations-in-mode counter; called once per
    /// iteration while restoration is active, one call per iteration outcome.
    virtual void note_iteration() = 0;

    /// Writes this strategy's diagnostic state into result. Same write-only
    /// contract and last-phase-wins semantics as the other globalization
    /// components' hooks. Non-virtual: both shipped strategies report the
    /// identical counter pair. When restoration_mode_ == off no strategy is
    /// constructed, so this is never reached on that path and the
    /// corresponding SolveResult fields keep their -1 sentinel.
    void append_diagnostics(InteriorPointSolver::SolveResult &result) const {
        result.last_feas_rest_entries_ = entries_;
        result.last_feas_rest_iters_ = iterations_in_mode_;
    }

    // -------------------------------------------------------------------------
    // Nested restoration surface.
    //
    // A second family of restoration strategies solves an l1 elastic
    // reformulation of the feasibility problem (min rho*sum(n+p) + proximal)
    // with the elastic slack pairs (n,p) and their bound multipliers condensed
    // out of the KKT system analytically. That machinery has no counterpart in
    // the proximal mode-switch above, so it lives behind is_nested(): the
    // solver seam consults these methods ONLY when is_nested() reports true.
    //
    // The default bodies below therefore throw — reaching one on a strategy
    // that is not nested marks a wiring bug, not a recoverable condition.
    // Concrete proximal-switch strategies inherit them untouched; the nested l1
    // strategy overrides every one.
    // -------------------------------------------------------------------------

    /// Whether this strategy uses the nested elastic-condensation surface below.
    virtual bool is_nested() const { return false; }

    /// @brief Enter the nested phase from the given entry point.
    /// @param eq_residuals,iq_residuals Constraint residual values at entry
    ///   (equality h(x); inequality g(x)+s).
    /// @param outer_mu Live outer barrier parameter, one input to entry_mu().
    virtual void enter_nested(const ProgressMeasures &reference,
                              const Eigen::Ref<const Eigen::VectorXd> &primals,
                              const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                              const Eigen::Ref<const Eigen::VectorXd> &iq_residuals,
                              double outer_mu) {
        (void)reference;
        (void)primals;
        (void)eq_residuals;
        (void)iq_residuals;
        (void)outer_mu;
        throw std::logic_error(
            "RestorationStrategy::enter_nested called on a strategy that does not "
            "implement the nested restoration surface");
    }

    /// The restoration barrier parameter computed at entry (also the phase's
    /// starting barrier parameter).
    virtual double entry_mu() const {
        throw std::logic_error(
            "RestorationStrategy::entry_mu called on a strategy that does not "
            "implement the nested restoration surface");
    }

    /// Per-row diagonal pivots landing in the KKT constraint-row slots —
    /// POSITIVE vectors (the solver seam negates them into the (y,y)
    /// diagonal entries).
    virtual const Eigen::VectorXd &e_pivots() const {
        throw std::logic_error(
            "RestorationStrategy::e_pivots called on a strategy that does not "
            "implement the nested restoration surface");
    }
    virtual const Eigen::VectorXd &i_pivots() const {
        throw std::logic_error(
            "RestorationStrategy::i_pivots called on a strategy that does not "
            "implement the nested restoration surface");
    }

    /// @brief Aggregates the elastic bound-variable complementarity products
    /// (n*z_n and p*z_p over every equality- and inequality-channel row) into
    /// sum/min/max/count over ONLY those pairs. All outputs are reinitialized
    /// by the call: when no elastic rows exist count is zero and min/max stay
    /// at their infinite sentinels (the caller must guard on count).
    ///
    /// These pairs are part of the restoration barrier subproblem exactly as
    /// the original slack/multiplier pairs are part of the outer one; the
    /// barrier machinery MUST see them, or a late-entered phase (original
    /// complementarity already at solve tolerance) drives the barrier parameter
    /// to its floor while the elastic pairs are still at restoration scale.
    virtual void nested_complementarity(double &sum, double &min_comp, double &max_comp,
                                        int &count) const {
        (void)sum;
        (void)min_comp;
        (void)max_comp;
        (void)count;
        throw std::logic_error(
            "RestorationStrategy::nested_complementarity called on a strategy that "
            "does not implement the nested restoration surface");
    }

    /// Condensed constraint-row right-hand sides (r-tilde), given the live
    /// barrier parameter and the CURRENT residual values and multipliers.
    virtual void condensed_residuals(double mu,
                                     const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                                     const Eigen::Ref<const Eigen::VectorXd> &iq_residuals,
                                     const Eigen::Ref<const Eigen::VectorXd> &eq_lmults,
                                     const Eigen::Ref<const Eigen::VectorXd> &iq_lmults,
                                     Eigen::Ref<Eigen::VectorXd> eq_rtilde_out,
                                     Eigen::Ref<Eigen::VectorXd> iq_rtilde_out) const {
        (void)mu;
        (void)eq_residuals;
        (void)iq_residuals;
        (void)eq_lmults;
        (void)iq_lmults;
        (void)eq_rtilde_out;
        (void)iq_rtilde_out;
        throw std::logic_error(
            "RestorationStrategy::condensed_residuals called on a strategy that does "
            "not implement the nested restoration surface");
    }

    /// The proximal objective/gradient/Hessian-diagonal pieces of the nested
    /// reformulation, evaluated with the LIVE barrier parameter (eta recomputed
    /// from mu on every call, unlike the frozen-zeta proximal-switch trio).
    virtual double nested_objective(double mu,
                                    const Eigen::Ref<const Eigen::VectorXd> &primals) const {
        (void)mu;
        (void)primals;
        throw std::logic_error(
            "RestorationStrategy::nested_objective called on a strategy that does not "
            "implement the nested restoration surface");
    }
    virtual void add_nested_gradient(double mu,
                                     const Eigen::Ref<const Eigen::VectorXd> &primals,
                                     Eigen::Ref<Eigen::VectorXd> grad_out) const {
        (void)mu;
        (void)primals;
        (void)grad_out;
        throw std::logic_error(
            "RestorationStrategy::add_nested_gradient called on a strategy that does "
            "not implement the nested restoration surface");
    }
    virtual void nested_primal_diagonal(double mu, Eigen::Ref<Eigen::VectorXd> diag_out) const {
        (void)mu;
        (void)diag_out;
        throw std::logic_error(
            "RestorationStrategy::nested_primal_diagonal called on a strategy that does "
            "not implement the nested restoration surface");
    }

    /// Recovers the elastic slack / bound-multiplier steps from the constraint
    /// multiplier steps (delta-y) produced by the condensed KKT solve.
    virtual void recover_elastic_steps(double mu,
                                       const Eigen::Ref<const Eigen::VectorXd> &eq_lmults,
                                       const Eigen::Ref<const Eigen::VectorXd> &iq_lmults,
                                       const Eigen::Ref<const Eigen::VectorXd> &eq_dy,
                                       const Eigen::Ref<const Eigen::VectorXd> &iq_dy) {
        (void)mu;
        (void)eq_lmults;
        (void)iq_lmults;
        (void)eq_dy;
        (void)iq_dy;
        throw std::logic_error(
            "RestorationStrategy::recover_elastic_steps called on a strategy that does "
            "not implement the nested restoration surface");
    }

    /// @brief Second-level elastic re-centering fallback: when the in-phase
    /// line search exhausts the recovery ladder, re-solve the separable elastic
    /// subproblem in closed form holding x and s FIXED (the same per-row
    /// quadratic the entry initializer uses, at LIVE mu and CURRENT raw
    /// residuals) and adopt the re-centered pairs as the live elastic state
    /// (n,p from the closed form; z_n=mu/n, z_p=mu/p).
    ///
    /// Deliberate deviation note: the condensed representation re-centers z
    /// ALONGSIDE n,p rather than keeping stale multipliers (the classic
    /// reference implementation leaves z untouched because its z are real
    /// variables the next Newton step updates — here they are not).
    virtual void recenter_elastics(double mu,
                                   const Eigen::Ref<const Eigen::VectorXd> &eq_residuals,
                                   const Eigen::Ref<const Eigen::VectorXd> &iq_residuals) {
        (void)mu;
        (void)eq_residuals;
        (void)iq_residuals;
        throw std::logic_error(
            "RestorationStrategy::recenter_elastics called on a strategy that does "
            "not implement the nested restoration surface");
    }

    /// Fraction-to-boundary caps for the recovered elastic steps: primal cap
    /// from the slacks (n,p), dual cap from their bound multipliers (z_n,z_p).
    virtual double primal_boundary_alpha(double tau) const {
        (void)tau;
        throw std::logic_error(
            "RestorationStrategy::primal_boundary_alpha called on a strategy that does "
            "not implement the nested restoration surface");
    }
    virtual double dual_boundary_alpha(double tau) const {
        (void)tau;
        throw std::logic_error(
            "RestorationStrategy::dual_boundary_alpha called on a strategy that does "
            "not implement the nested restoration surface");
    }

    /// Commits the recovered elastic steps at the accepted step fractions.
    virtual void apply_elastic_step(double alpha_primal, double alpha_dual) {
        (void)alpha_primal;
        (void)alpha_dual;
        throw std::logic_error(
            "RestorationStrategy::apply_elastic_step called on a strategy that does not "
            "implement the nested restoration surface");
    }

    /// Trial-path measures at step fraction alpha, for acceptance during the phase.
    virtual double trial_objective(double mu, double alpha,
                                   const Eigen::Ref<const Eigen::VectorXd> &trial_primals) const {
        (void)mu;
        (void)alpha;
        (void)trial_primals;
        throw std::logic_error(
            "RestorationStrategy::trial_objective called on a strategy that does not "
            "implement the nested restoration surface");
    }

    /// shift = (n + alpha*dn) - (p + alpha*dp), added to the raw constraint
    /// residuals during trial evaluation in-phase.
    virtual void trial_residual_shift(double alpha, Eigen::Ref<Eigen::VectorXd> eq_shift_out,
                                      Eigen::Ref<Eigen::VectorXd> iq_shift_out) const {
        (void)alpha;
        (void)eq_shift_out;
        (void)iq_shift_out;
        throw std::logic_error(
            "RestorationStrategy::trial_residual_shift called on a strategy that does not "
            "implement the nested restoration surface");
    }

  protected:
    /// Per-phase diagnostics (write-only via append_diagnostics()) and the
    /// entry-permission budget counter entry_permitted() reads. Both concrete
    /// strategies increment entries_ in their enter hooks and clear both in
    /// reset().
    int entries_ = 0;
    int iterations_in_mode_ = 0;
};

} // namespace hven::solvers
