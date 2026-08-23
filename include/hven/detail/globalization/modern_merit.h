// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <limits>
#include <stdexcept>

#include "hven/detail/drivers/interior_point_solver_fwd.h"
#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/progress_measures.h"

namespace hven::solvers {

/// WMNO Eq. (3.4)/(3.5): pred(d_z) >= rho_nu*|c|, rho in (0,1); shipped value
/// rho = 0.1 (Waltz, Morales, Nocedal & Orban, Math. Program. 107, §3.1).
inline constexpr double kWmnoRho = 0.1;
/// WMNO Eq. (3.6): nu+ = tau + 1 when nu < tau (the additive bump).
inline constexpr double kWmnoPenaltyBump = 1.0;
/// WMNO Armijo Eq. (3.9) / Algorithm 2.1 constant: eta = 1e-8.
inline constexpr double kWmnoArmijoEta = 1.0e-8;
/// Initial penalty nu_0 > 0 (WMNO require nu > 0); a neutral start that grows
/// monotonically via Eq. (3.6). Cleared to this by reset().
inline constexpr double kWmnoInitPenalty = 1.0;

/// Flexible chi-threshold Eq. (3.8): denominator (1-sigma)*|c|, sigma in (0,1)
/// — same role as WMNO's rho, matched value (Curtis & Nocedal, IMA JNA 28(4)).
inline constexpr double kFlexSigma = 0.1;
/// Flexible Armijo Eq. (3.12): 0 < eta < 1 (matched to WMNO's).
inline constexpr double kFlexArmijoEta = 1.0e-8;
/// Flexible pi_u update Eq. (3.9): pi_u+ = chi + eps, "a small constant".
inline constexpr double kFlexEpsPiU = 1.0e-8;
/// Flexible pi_l update Eq. (3.10): the additive floor eps_l.
inline constexpr double kFlexEpsPiL = 1.0e-8;
/// Flexible pi_l update Eq. (3.10): the damping factor 0.1 ("so that the value
/// for pi_l will increase only gradually").
inline constexpr double kFlexPiLDamping = 0.1;
/// Flexible initial interval (Algorithm 3.1: pi_l small, pi_u large). Cleared
/// to these by reset().
inline constexpr double kFlexInitPiL = 1.0e-8;
inline constexpr double kFlexInitPiU = 1.0e8;

/// Restoration-exit sufficient-infeasibility-decrease ratio: the trial's
/// infeasibility must fall to this fraction of the smallest infeasibility seen
/// so far to leave feasibility restoration (reference implementation option
/// sufficient_infeasibility_decrease_ratio, shipped default 0.9).
inline constexpr double kSufficientInfeasibilityDecreaseRatio = 0.9;

/// @brief Modernized merit family (WMNO single-penalty / flexible interval),
/// driven through the GENERIC AcceptanceStrategy path — a second acceptance
/// strategy alongside ClassicMeritAcceptance, which it neither edits nor
/// replaces. Opt-in via Settings::acceptance_strategy_; the default classic
/// path stays bit-identical.
///
/// Formulation. Both rules use the standard interior-point merit
///
///     phi_pi(z) = phi_mu(z) + pi*||c(z)||,
///
/// where phi_mu(z) = f(x) - mu*sum(log s_i) and ||c(z)|| is the constraint-
/// violation norm (WMNO Eq. (3.1); Curtis–Nocedal Eq. (2.1) with the SQP
/// objective adapted to the barrier objective). ProgressMeasures mapping:
///
///     current.infeasibility  = theta_c = ||c(z_k)||_1   (the ||c|| in the merit)
///     current.objective      = f(x_k) (sigma-scaled)
///     current.auxiliary      = -mu*sum(log s) at z_k    (the barrier term)
///     trial.*                = same three at z_k + alpha*d
///     => phi_mu(pt) = pt.objective + pt.auxiliary ; phi_pi(pt) = phi_mu(pt) + pi*pt.infeasibility
///     predicted_reduction.objective     = m_f = -alpha*(grad phi_mu^T d)   (>=0 descent)
///     predicted_reduction.infeasibility = m_theta = alpha*theta_c          (linearized c)
///     predicted_reduction.auxiliary     = 0 (unused)
///     objective_multiplier              = sigma; objective/aux arrive pre-scaled
///                                         so the merit uses them as-is (parity
///                                         with the classic path).
///
/// The alpha factor cancels in every penalty-threshold ratio below, so the
/// penalty update is steplength-independent (fixed once per iteration, held
/// during the backtrack), exactly as both papers intend.
///
/// Predicted merit reduction for penalty pi (WMNO Eq. (3.2) with the SQP
/// curvature term dropped — sanctioned by WMNO as "sigma always equal to 0",
/// and the Lagrangian Hessian is unavailable in a line search):
///
///     pred_pi = m_f + pi*m_theta.
///
/// Actual reduction ared_pi = phi_pi(current) - phi_pi(trial); acceptance is
/// ared_pi >= eta*pred_pi (WMNO ratio test §3.1 / Armijo Eq. (3.9); flexible
/// Armijo Eq. (3.12) — algebraically identical forms once alpha-scaled).
///
/// Penalty threshold (WMNO Eq. (3.5) with sigma=0; flexible Eq. (3.8)):
///
///     tau = (-predicted_reduction.objective)
///           / ((1-rho)*predicted_reduction.infeasibility).
///
/// Updates: WMNO nu+ = nu if nu >= tau else tau + 1 (Eq. (3.6)). Flexible:
/// pi_u+ = pi_u if pi_u >= tau else tau + eps_u (Eq. (3.9)); acceptance holds
/// iff (accept-pi) holds for pi = pi_l OR pi = pi_u (the papers' own practical
/// interval reduction); on accept, pi_l+ = pi_l if pi_l already sufficed,
/// otherwise pi_l+ = min{pi_u, pi_l + max{0.1*(r - pi_l), eps_l}} with
/// r = (phi_mu(trial) - phi_mu(current))/(theta_c - theta_t) (Eq. (3.11)),
/// computed only when theta_c - theta_t > 0. Feasible-current special case
/// (both papers): when m_theta == 0 no penalty update is performed (tau
/// undefined); the acceptance test still evaluates pred_pi = m_f.
class ModernMeritAcceptance : public AcceptanceStrategy {
  public:
    explicit ModernMeritAcceptance(MeritPenaltyRules rule) : rule_(rule) { reset(); }

    /// @brief The generic acceptance test (formulation above). Pure in its
    /// ProgressMeasures arguments plus the penalty state; may mutate the
    /// penalty state per the update rules. step_length is accepted and
    /// ignored: the penalty math is steplength-independent.
    bool is_iterate_acceptable(const ProgressMeasures &current, const ProgressMeasures &trial,
                               const ProgressMeasures &predicted_reduction,
                               double objective_multiplier, double step_length) override;

    /// @brief Restoration-exit test: theta_trial <=
    /// kSufficientInfeasibilityDecreaseRatio * smallest_known_infeasibility.
    ///
    /// The reference argument is accepted and IGNORED — the rule reduces
    /// against the internal smallest-known tracker, not the entry point. While
    /// in the feasibility phase the tracker read is the STASHED (frozen
    /// optimality-phase) one — see the state-isolation note below; outside the
    /// phase it is the live tracker.
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                               const ProgressMeasures &trial) const override;

    /// @brief mu-event / phase-change hook: clears the WORKING penalty state
    /// AND the working tracker to fresh-construction values. NOT a no-op (the
    /// penalties are per-solve state). Phase-aware: during the feasibility
    /// phase a mu-event clears only the working state, preserving the stashed
    /// optimality-phase state the exit test consults; outside the phase it is
    /// the full clear and additionally drops the stash defensively. The
    /// restoration-off default path only ever sees the full clear.
    void reset() override;

    /// Selects the GENERIC driving path in compute_step.
    bool drives_classic_path() const override { return false; }

    /// Restoration entry hook: stash ALL persistent state, set the flag, and
    /// reinitialize the working state fresh so the feasibility phase runs its
    /// own penalties/tracker without contaminating the frozen copy.
    void notify_switch_to_feasibility(const ProgressMeasures &current_progress) override;

    /// Restoration exit hook: restore the stash and clear the flag.
    void notify_switch_to_optimality(const ProgressMeasures &current_progress) override;

    /// Penalty-state accessors (diagnostics + unit tests).
    double wmno_penalty() const { return nu_; }
    double flex_pi_l() const { return pi_l_; }
    double flex_pi_u() const { return pi_u_; }
    double smallest_known_infeasibility() const { return smallest_known_infeasibility_; }

    /// Restoration-state accessors (diagnostics + unit tests).
    bool in_feasibility_phase() const { return in_feasibility_phase_; }
    double stashed_wmno_penalty() const { return stashed_nu_; }
    double stashed_flex_pi_l() const { return stashed_pi_l_; }
    double stashed_flex_pi_u() const { return stashed_pi_u_; }
    double stashed_smallest_known_infeasibility() const {
        return stashed_smallest_known_infeasibility_;
    }

  private:
    /// Shared merit primitive (pure arithmetic on ProgressMeasures).
    static double merit(const ProgressMeasures &pt, double penalty) {
        return pt.objective + pt.auxiliary + penalty * pt.infeasibility;
    }

    /// (accept-pi): ared_pi >= eta*pred_pi; pred_pi = m_f + pi*m_theta.
    static bool armijo(const ProgressMeasures &current, const ProgressMeasures &trial,
                       const ProgressMeasures &pred, double penalty, double eta) {
        const double ared = merit(current, penalty) - merit(trial, penalty);
        const double pred_pi = pred.objective + penalty * pred.infeasibility;
        return ared >= eta * pred_pi;
    }

    /// The two rule implementations behind is_iterate_acceptable.
    bool accept_wmno(const ProgressMeasures &current, const ProgressMeasures &trial,
                     const ProgressMeasures &pred);
    bool accept_flexible(const ProgressMeasures &current, const ProgressMeasures &trial,
                         const ProgressMeasures &pred);

    MeritPenaltyRules rule_;

    /// Per-solve penalty state (reset() clears to the kInit* constants).
    double nu_ = kWmnoInitPenalty;    ///< WMNO single penalty ν.
    double pi_l_ = kFlexInitPiL;      ///< Flexible lower penalty π_l.
    double pi_u_ = kFlexInitPiU;      ///< Flexible upper penalty π_u.

    /// Restoration-exit tracker (+inf-initialized; min()-updated ONLY in the
    /// accept branch; cleared by reset()). FP-inert on the default path: it
    /// writes a member on every accept but is never read unless the
    /// restoration-exit test runs.
    double smallest_known_infeasibility_ = std::numeric_limits<double>::infinity();

    // Feasibility-restoration state isolation. The reference implementation
    // constructs a SEPARATE strategy instance per phase, structurally freezing
    // all optimality-phase merit state while restoration runs; this engine
    // drives ONE object across both phases, so the optimality-phase state is
    // stashed at entry and restored at exit to get the same behavior — and the
    // exit test reduces against the STASHED tracker while in the phase (the
    // live tracker is updated by every feasibility-phase accept, which would
    // make the exit ratio unsatisfiable for any positive theta). Retained edge
    // (also the reference's behavior): entering restoration before any
    // optimality-phase accept leaves the stashed tracker at +inf, so the exit
    // test passes at first check.

    /// Preserved optimality-phase state: stashed at entry, restored at exit.
    double stashed_nu_ = kWmnoInitPenalty;
    double stashed_pi_l_ = kFlexInitPiL;
    double stashed_pi_u_ = kFlexInitPiU;
    double stashed_smallest_known_infeasibility_ = std::numeric_limits<double>::infinity();

    /// Set at entry, cleared at exit; makes reset() phase-aware (a mid-phase
    /// mu-event must preserve the stash and this flag).
    bool in_feasibility_phase_ = false;
};

} // namespace hven::solvers
