// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <Eigen/Core>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/restoration.h"
#include "hven/detail/globalization/solver_context.h"

namespace hven::solvers {

/// Proximal weight for zeta (item 1 below); the reference implementation's
/// "resto_proximity_weight" shipped default 1.0.
inline constexpr double kRestoProximityWeight = 1.0;

// The entry-guard factor and the near_feasible() test built on it live in
// restoration.h — shared by both concrete strategies and the feasibility-stage
// stall seam.

/// Failure-classification threshold for a restoration STALL (distinct from the
/// ENTRY guard, and three orders of magnitude looser). When the restoration
/// subproblem converges/stalls, the outcome is classified by comparing true
/// primal infeasibility against this multiple of the tolerance: at or below it
/// the restoration is treated as having reached a (near-)feasible point — a
/// soft, recoverable outcome; only above it is the problem declared locally
/// infeasible. Using the (much tighter) entry-guard factor here would
/// misclassify every stall with violation in (0.1*tol, 1e2*tol] — points the
/// reference treats as feasible-enough to continue from — as local
/// infeasibility on feasible problems.
inline constexpr double kRestoFailureFeasibilityFactor = 1.0e2;

/// @brief Concrete proximal feasibility mode-switch: keeps the SAME barrier
/// algorithm running on globalization failure, swapping the true objective for
/// a proximal term pulling the primal variables back toward the point
/// restoration was entered from.
///
/// Formulation:
///
/// (1) Proximal coefficient zeta = kRestoProximityWeight * sqrt(mu), set ONCE
///     at the moment restoration is entered — NOT re-derived from mu on
///     subsequent iterations while restoration remains active. enter_restoration()
///     is the single call site: it freezes zeta_ from the mu live at that
///     instant; only a fresh enter_restoration() episode or reset() reassigns it.
/// (2) Per-coordinate scaling and proximal term, with x_R the primal snapshot
///     at entry:
///         d_i = min(1, 1/|x_R_i|)                    (scaling)
///         P(x) = (zeta/2) * sum_i d_i^2 (x_i - x_R_i)^2
///         dP/dx_i = zeta * d_i^2 * (x_i - x_R_i)
///     A coordinate with |x_R_i| < 1 is unscaled (d_i = 1); |x_R_i| > 1 shrinks
///     in proportion to the center's magnitude so the proximal curvature stays
///     bounded near large centers; |x_R_i| == 0 lands in the first branch
///     without special-casing division by zero. proximal_diagonal() returns
///     zeta*d_i^2 directly (the primal-diagonal Hessian block of P), so both
///     the objective and gradient are expressed in terms of it.
/// (3) Near-feasible entry guard: refuses entry at an almost-feasible point —
///     constraint violation <= kNearFeasibleGuardFactor * econ_tol_ (see
///     restoration.h). Single-measure ADAPTATION of the reference's two-test
///     (scaled AND unscaled) guard: with one violation measure and one
///     tolerance available, the unscaled member of the pair is matched.
///     Disclosed consequence: the guard can refuse entry slightly earlier or
///     later than a dual test would where the two tests would disagree.
/// (4) Budget guard: entry_permitted() also refuses once this phase's entry
///     count reaches ctx.settings_.max_feas_rest_; 0 means exhausted before
///     the first entry (always refused). Guards (3) and (4) are independent —
///     either refusing is enough.
class ProximalSwitchRestoration final : public RestorationStrategy {
  public:
    void enter_restoration(const ProgressMeasures &reference,
                            const Eigen::Ref<const Eigen::VectorXd> &primals,
                            double mu) override;

    void exit_restoration() override { active_ = false; }

    bool is_active() const override { return active_; }

    void reset() override;

    double proximal_objective(const Eigen::Ref<const Eigen::VectorXd> &primals) const override;

    void add_proximal_gradient(const Eigen::Ref<const Eigen::VectorXd> &primals,
                                Eigen::Ref<Eigen::VectorXd> grad_out) const override;

    const Eigen::VectorXd &proximal_diagonal() const override { return diagonal_; }

    // entry_permitted() (virtual, shared default body) and append_diagnostics()
    // (non-virtual) are both inherited unoverridden — both read/write only the
    // per-phase counters shared with the base (see restoration.h).

    const ProgressMeasures &reference() const override { return reference_; }

    void note_iteration() override { ++iterations_in_mode_; }

    /// Test/diagnostic observers.
    double zeta() const { return zeta_; }
    const Eigen::VectorXd &scaling() const { return d_; }
    const Eigen::VectorXd &snapshot() const { return x_r_; }
    int entries() const { return entries_; }
    int iterations_in_mode() const { return iterations_in_mode_; }

  private:
    bool active_ = false;
    ProgressMeasures reference_;

    /// Entry snapshot — the mode's defining state. x_r_ is the primal center;
    /// d_ the per-coordinate scaling; diagonal_ = zeta*d_i^2 cached so the
    /// objective/gradient never recompute it.
    Eigen::VectorXd x_r_;
    Eigen::VectorXd d_;
    Eigen::VectorXd diagonal_;
    double zeta_ = 0.0;
};

} // namespace hven::solvers
