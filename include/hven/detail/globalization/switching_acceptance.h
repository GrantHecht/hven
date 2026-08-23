// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include "hven/detail/globalization/acceptance_strategy.h"
#include "hven/detail/globalization/progress_measures.h"

namespace hven::solvers {

/// Eq. (19) switching-condition exponents and threshold; Ipopt
/// IpFilterLSAcceptor.cpp defaults: s_phi_ = 2.3, s_theta_ = 1.1, delta_ = 1.0.
inline constexpr double kSwitchingDelta = 1.0;
inline constexpr double kSwitchingSPhi = 2.3;
inline constexpr double kSwitchingSTheta = 1.1;

/// Eq. (20) Armijo-on-phi constant; Ipopt IpFilterLSAcceptor.cpp default
/// eta_phi_ = 1e-8.
inline constexpr double kArmijoEtaPhi = 1.0e-8;

/// theta_min / theta_max factors (Eq. (21) and the paragraph preceding
/// Eq. (19)); Ipopt defaults theta_min_fact_ = 1e-4, theta_max_fact_ = 1e4.
inline constexpr double kThetaMinFact = 1.0e-4;
inline constexpr double kThetaMaxFact = 1.0e4;

/// Why the template method rejected a trial, handed to the subclass's
/// notify_trial_rejected() hook.
enum class RejectionCause {
    kCeiling,      
    kMembership,   
    kArmijo,       
    kHTypeProgress 
};

/// @brief Shared Wächter–Biegler switching-condition skeleton (template
/// method) used by BOTH the filter and funnel strategies; never instantiated
/// directly — each concrete subclass supplies its bound-tracking data
/// structure through the protected hooks.
///
/// Driven through the GENERIC AcceptanceStrategy path: compute_step runs the
/// loop, is_iterate_acceptable renders the verdict. Formulation follows
/// Wächter & Biegler, Math. Program. 106(1):25–57 (2006), with Ipopt-reference
/// default constants (cited per constant below).
///
/// ProgressMeasures mapping (same convention as modern_merit.h):
///   current/trial .infeasibility = θ at z_k / z_k + α·d
///   φ(pt) = pt.objective + pt.auxiliary            (barrier objective)
///   predicted_reduction.objective = m_f = −α·∇φ_kᵀd_k   (α-scaled model
///     reduction, positive for descent; raw directional derivative recovers
///     as m_f / step_length)
///   step_length = α_k ∈ (0, 1]
///
/// Bounds: θ_min = kThetaMinFact·max(1,θ₀) and θ_max = kThetaMaxFact·max(1,θ₀),
/// fixed once per phase from θ₀ — the infeasibility of the first current
/// iterate seen after reset() — via a lazy initialization, not an argument.
///
/// ACCEPTANCE ORDER (every trial passes through it in order):
///   1. θ_max ceiling (Eq. 21): θ(trial) > θ_max ⇒ reject (kCeiling).
///   2. STRATEGY MEMBERSHIP, every trial (F-type included):
///      is_trial_acceptable_to_strategy(); failure ⇒ reject (kMembership).
///      To reproduce reference implementations' rejection ATTRIBUTION exactly
///      despite checking membership first, a membership rejection here
///      SPECULATIVELY evaluates the type-appropriate T1 test (side-effect-free)
///      and hands the result to notify_trial_rejected() — see the hook.
///   3. Switching condition (Eq. 19), tested only when θ_k ≤ θ_min AND the
///      step is a descent direction for φ:
///          α_k ·(m_f/α_k)^{s_φ} > δ ·θ_k^{s_θ}.
///      F-TYPE (holds): accept iff Armijo holds (Eq. 20,
///      φ(trial) ≤ φ(current) − η_φ·m_f); else reject (kArmijo).
///      H-TYPE (fails): accept iff is_h_type_progress_acceptable(); else
///      reject (kHTypeProgress).
///   4. On any accept: register_accepted_step(current, trial, h_type).
///   5. On any rejection: notify_trial_rejected(cause, t1_result).
///
/// Deliberate deviation callout: the minimal-step / restoration trigger
/// (WB Eq. (23), α_min) is deliberately NOT implemented here — recovery
/// dispatch already owns "the line search cannot make progress" through
/// FeasibilitySwitchRecovery, which converts a ladder-exhausted kAcceptAsIs
/// into the restoration switch. A second α_min-based trigger would be
/// redundant with that dispatch, not a missing prerequisite. Until one is
/// added (if ever), a persistently failing trial simply keeps backtracking to
/// the generic ladder's floor and the recovery dispatch above.
///
/// Ownership: no SolverContext, no NLP evaluation — pure function of its
/// ProgressMeasures arguments plus its own θ_min/θ_max state (and whatever
/// state a concrete subclass adds).
class SwitchingAcceptance : public AcceptanceStrategy {
  public:
    /// The switching-condition template method (see the class formulation).
    bool is_iterate_acceptable(const ProgressMeasures &current, const ProgressMeasures &trial,
                               const ProgressMeasures &predicted_reduction,
                               double objective_multiplier, double step_length) override;

    /// @brief Restoration-exit test, driven once a feasibility-restoration
    /// strategy is active (one call site per restoration mode). The base body
    /// throws rather than fabricate an answer; concrete subclasses override it
    /// with a real body.
    /// @throws std::logic_error Always, in this base.
    bool is_infeasibility_sufficiently_reduced(const ProgressMeasures &reference,
                                               const ProgressMeasures &trial) const override;

    /// mu-event / phase-change hook: re-arm the lazy theta_min/theta_max init
    /// and defer to the subclass's own bound-tracking reset.
    void reset() override;

    /// Selects the GENERIC driving path in compute_step.
    bool drives_classic_path() const override { return false; }

    /// Bound accessors (diagnostics + unit tests).
    double theta_min() const { return theta_min_; }
    double theta_max() const { return theta_max_; }

  protected:
    /// Abstract base: only a concrete filter/funnel subclass may construct.
    SwitchingAcceptance() = default;

    /// Called once, lazily, at the first post-reset() acceptance test (i.e.
    /// phase start); lets the filter clear its list / the funnel set its
    /// initial width from theta_0.
    virtual void initialize_bounds(double theta_0) = 0;

    /// The reset() companion: undoes initialize_bounds() so the next
    /// acceptance test re-arms the lazy init.
    virtual void reset_bounds() = 0;

    /// MEMBERSHIP verdict, run for EVERY trial (order step 2).
    virtual bool is_trial_acceptable_to_strategy(const ProgressMeasures &current,
                                                 const ProgressMeasures &trial) = 0;

    /// H-TYPE sufficient-progress verdict, run only on the H-type path.
    virtual bool is_h_type_progress_acceptable(const ProgressMeasures &current,
                                               const ProgressMeasures &trial) = 0;

    /// Bookkeeping on any accept; h_type is false for an F-type accept.
    virtual void register_accepted_step(const ProgressMeasures &current,
                                        const ProgressMeasures &trial, bool h_type) = 0;

    /// @brief Bookkeeping on any rejection; default no-op. The second
    /// argument carries the SPECULATIVE T1 result evaluated in the membership-
    /// reject branch and is meaningful only when cause == kMembership; for
    /// kArmijo/kHTypeProgress it is trivially false (T1 failed by definition
    /// to reach those branches), and for kCeiling it is false and MUST be
    /// ignored (the reference implementation leaves its attribution flag
    /// untouched on a ceiling rejection).
    virtual void notify_trial_rejected(RejectionCause cause, bool trial_passed_progress_test) {
        (void)cause;
        (void)trial_passed_progress_test;
    }

  private:
    /// Eq. (19), factored out so the membership-reject branch can evaluate it
    /// speculatively without duplicating the arithmetic the template method
    /// also runs at step 3.
    bool compute_switching_holds(const ProgressMeasures &current,
                                 const ProgressMeasures &predicted_reduction,
                                 double step_length) const;

    /// Eq. (20), factored out for the same reason (used at step 3 AND,
    /// speculatively, in the membership-reject branch's T1 evaluation).
    bool armijo_holds(const ProgressMeasures &current, const ProgressMeasures &trial,
                      const ProgressMeasures &predicted_reduction) const;

    /// Lazy-init flag; re-armed by reset().
    bool bounds_initialized_ = false; 
    double theta_min_ = 0.0;
    double theta_max_ = 0.0;
};

} // namespace hven::solvers
