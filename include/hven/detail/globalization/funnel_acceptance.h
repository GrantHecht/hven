// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <limits>
#include <stdexcept>

#include "hven/detail/globalization/progress_measures.h"
#include "hven/detail/globalization/switching_acceptance.h"

namespace hven::solvers {

/// (init) absolute floor tau-bar (KLV Eq. (9); reference option "funnel_ubd" = 1.0).
inline constexpr double kFunnelInitialUpperBound = 1.0;
/// (init) multiplier kappa-bar > 1 on theta_0 (KLV Eq. (9); option "funnel_fact" = 1.5).
inline constexpr double kFunnelInfeasibilityFactor = 1.5;
/// Margin beta in (0,1) for the sufficient-reduction test and the update floor
/// (KLV Eq. (12); option "funnel_beta" = 0.9999).
inline constexpr double kFunnelBeta = 0.9999;
/// Convex-combination coefficient kappa in (0,1) in the width update
/// (KLV Eq. (13) / update strategy 1; option "funnel_kappa" = 0.5).
inline constexpr double kFunnelKappa = 0.5;

/// @brief Concrete scalar-funnel strategy on the switching skeleton: replaces
/// the filter's SET of (theta, f) pairs with ONE scalar — an upper bound on the
/// constraint violation, the funnel width tau, monotonically tightened as
/// accepted iterates stay inside the funnel. The base gates EVERY trial
/// (F-type included) on funnel membership (theta_trial <= tau), so at strategy
/// level every accepted iterate lies inside the funnel; each accepted H-type
/// step then tightens tau. This class supplies ONLY the funnel-specific
/// membership + H-type verdicts and the width bookkeeping through the base's
/// hooks; theta_min/theta_max ceiling, switching condition, and F-type Armijo
/// live in SwitchingAcceptance. Opt-in via Settings::acceptance_strategy_ ==
/// funnel; the default classic path stays bit-identical.
///
/// ProgressMeasures mapping: current/trial .infeasibility = h(x) at z_k /
/// z_k + alpha*d; objective/auxiliary participate only in the base's F-type
/// Armijo test — the funnel's own rules read infeasibility alone.
///
/// Formulation:
///  (1) Width init from theta_0 (first current infeasibility after a reset(),
///      captured lazily by the base): tau = max(tau_bar, kappa_bar * theta_0).
///  (2) Verdicts:
///      (2a) MEMBERSHIP, every trial: theta_trial <= tau.
///      (2b) H-type sufficient reduction: theta_trial <= beta*tau. Since
///           beta < 1, (2b) implies (2a), so an accepted H-type step is
///           strictly inside the funnel.
///  (3) Width update on an ACCEPTED H-type step only (an F-type accept leaves
///      tau untouched):
///          if theta_trial <= theta_current:
///              tau+ = max(beta*tau, kappa*theta_cur + (1-kappa)*theta_trial)
///          else:
///              tau+ = beta*tau,
///      floored at beta*tau so a single step never over-tightens. Under the
///      membership gate every accepted iterate satisfies theta_current <= tau,
///      so tau+ < tau unconditionally at strategy level — monotonically
///      non-increasing width.
///
///      ONE residual edge, disclosed: when the backtracking ladder exhausts,
///      the solver's recovery fallback may accept a strategy-REJECTED trial;
///      such an iterate can lie OUTSIDE the funnel (theta_current >> tau), and
///      a later accepted H-type step then reads that oversized theta_current in
///      the convex-combination branch, which can transiently RE-WIDEN tau
///      (e.g. tau=1.5, theta_cur=100, theta_trial=0.5 -> tau+=50.25). This is
///      unreachable through the strategy itself and the pinning test drives
///      register_accepted_step() directly with an out-of-funnel current to hold
///      the edge fixed.
///
/// Feasibility-restoration hooks: the reference implementation runs a SEPARATE
/// strategy instance per phase, structurally freezing the optimality width
/// during restoration. This engine drives one object across both phases, so:
///  - Entry stashes the optimality width, re-arms the lazy theta_0 init via a
///    reset(), sets the flag, and reinitializes a fresh feasibility width
///    (lazily derived from feasibility-phase theta_0 on first use). (The
///    reference entry hook is an empty no-op ONLY because its optimality
///    instance is untouched by the other phase; here the stash IS the
///    single-instance equivalent of that no-op — without it feasibility accepts
///    would tighten the very width the exit consults.)
///  - Exit test: funnel-membership against the STASHED width while in the
///    phase (live width outside it), AND theta_trial <= beta*theta_ref
///    (reusing margin beta).
///  - Exit restores the stashed width directly (NOT via reset(), which would
///    re-derive and wipe it), then re-bases tau <- kappa*tau + (1-kappa)*theta_exit
///    toward the reduced exit infeasibility. No extra guard beyond the
///    reference's: the +inf-width edge is FP-inert (kappa*inf + finite = inf —
///    no NaN; the funnel simply stays wide).
///
/// Disclosed divergences from the sources:
///  - KLV Eq. (13) states the update as a combination of the OLD WIDTH with the
///    trial; that formula is the reference's NON-default update_strategy = 2.
///    This follows the shipped default (strategy 1, above); both share the same
///    width-decrease guarantee.
///  - The optional acceptable-wrt-current-iterate refinement (funnel_gamma /
///    funnel_require_acceptance_wrt_current_iterate) is off by default upstream
///    and not implemented here; its constant is omitted to avoid dead state.
class FunnelAcceptance final : public SwitchingAcceptance {
  public:
    /// Current funnel width tau (diagnostics + unit tests). Before the first
    /// acceptance call after a reset() this is the uninitialized sentinel
    /// (+inf); it is derived from theta_0 on the first call.
    double funnel_width() const { return width_; }

    /// Reports the current width into SolveResult::last_funnel_width_, or the
    /// -1.0 sentinel if uninitialized (the pathological case of a phase that
    /// never calls is_iterate_acceptable, e.g. converges at the initial iterate).
    void append_diagnostics(InteriorPointSolver::SolveResult &result) const override;

    /// Restoration-exit test: membership (against the stashed width while in
    /// the feasibility phase) AND theta_trial <= beta*theta_ref.
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                               const ProgressMeasures &trial) const override;

    /// Entry hook: stash the optimality width, set the flag, reinitialize fresh.
    void notify_switch_to_feasibility(const ProgressMeasures &current_progress) override;

    /// Exit hook: restore the stashed width, then re-base tau.
    void notify_switch_to_optimality(const ProgressMeasures &current_progress) override;

    /// Restoration diagnostics (unit tests).
    bool in_feasibility_phase() const { return in_feasibility_phase_; }
    double stashed_funnel_width() const { return stashed_width_; }

  protected:
    /// Width init: tau = max(tau_bar, kappa_bar*theta_0).
    void initialize_bounds(double theta_0) override;

    /// Restores the uninitialized sentinel so the next theta_0 re-derives the
    /// width. Phase-aware (mirroring the filter): inside the feasibility phase
    /// a mu-event clears only the working width, preserving the stash and flag;
    /// outside the phase it also drops the stash.
    void reset_bounds() override;

    /// Membership: within the funnel (theta_trial <= tau) — every trial.
    bool is_trial_acceptable_to_strategy(const ProgressMeasures &current,
                                         const ProgressMeasures &trial) override;

    /// H-type sufficient reduction: theta_trial <= beta*tau.
    bool is_h_type_progress_acceptable(const ProgressMeasures &current,
                                       const ProgressMeasures &trial) override;

    /// Shrinks the width on an accepted H-type step (update strategy 1); an
    /// F-type accept leaves the width untouched.
    void register_accepted_step(const ProgressMeasures &current, const ProgressMeasures &trial,
                                bool h_type) override;

  private:
    /// Reference convex_combination(a, b, coeff) = coeff*a + (1-coeff)*b.
    static double convex_combination(double a, double b, double coefficient) {
        return coefficient * a + (1.0 - coefficient) * b;
    }

    /// Scalar funnel width tau; +inf until the first theta_0 capture
    /// (uninitialized sentinel).
    double width_ = std::numeric_limits<double>::infinity();

    /// PRESERVED optimality-phase width, stashed at entry, restored at exit;
    /// the exit test's membership half consults it while in the phase.
    double stashed_width_ = std::numeric_limits<double>::infinity();

    /// Set at entry, cleared at exit; makes reset_bounds() phase-aware.
    bool in_feasibility_phase_ = false;
};

} // namespace hven::solvers
