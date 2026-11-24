// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#include <hven/sqp/sqp_options.h>

namespace hven::detail {

// Trust-region update constants of the SQP driver. Source: Conn, Gould &
// Toint, "Trust-Region Methods" (MPS-SIAM, 2000), Algorithm BTR and Table
// 6.1.1 -- eta_2, gamma_2, gamma_1 respectively. [KLV] states the RULE ("if
// the trust region is active at d then increase the radius", Algorithm 3) but
// no values, so these are ported rather than transcribed. See sqp_solver.h's
// RADIUS MANAGEMENT note for the full radius lifecycle.

/// rho = actual/predicted objective decrease at or above which an accepted step
/// counts as "very successful" and earns a larger radius.
inline constexpr double kTrGrowThreshold = 0.75;
/// The expansion factor, applied only when the radius was ACTIVE at the step
/// and capped at SqpOptions::trust_region_max.
inline constexpr double kTrGrowFactor = 2.0;
/// The contraction factor, applied on every kReject verdict. COUPLED to
/// funnel.h's kRestoreMinRejections -- see RADIUS MANAGEMENT.
inline constexpr double kTrShrinkFactor = 0.5;
/// The trust-region floor exit's noise test (RF-1 design section 3.4): a rejected step whose
/// predicted objective decrease is at most `kNoiseFloorFactor * eps * max(1, |f|)` promised
/// nothing the objective's rounding can resolve.
inline constexpr double kNoiseFloorFactor = 100.0;

/// Whether an ACCEPTED step grows the radius under `rule`. `delta_finite` and `tr_binding` are the
/// radius being finite and active at the step; `failure_step` marks a failed subproblem's own step,
/// which never grows it. `pred_df`/`actual_df` are the predicted and actual objective decreases;
/// `feasibility_step` marks an accepted h-type step. Only the ratio test reads `pred_df`.
inline bool trust_region_grows(SqpTrustRegionGrowth rule, bool failure_step, bool tr_binding,
                               bool delta_finite, double pred_df, double actual_df,
                               bool feasibility_step) {
    if (failure_step || !tr_binding || !delta_finite) {
        return false;
    }
    const bool ratio = pred_df > 0.0 && actual_df >= kTrGrowThreshold * pred_df;
    switch (rule) {
    case SqpTrustRegionGrowth::kRatio:
        return ratio;
    case SqpTrustRegionGrowth::kRatioOrFeasibilityStep:
        return ratio || feasibility_step;
    case SqpTrustRegionGrowth::kAnyAccepted:
        return true;
    case SqpTrustRegionGrowth::kStepTypeRatio:
        return ratio;
    }
    return ratio;
}

// ---- I19-2a: the growth and shrink levers (SqpAdvancedOptions::trust_region_*) ----

/// Bits of a growth refusal (SqpIterationRow::tr_growth_refusal): each lever that refused a
/// growth the rule granted. 0 when nothing was refused.
inline constexpr Index kTrRefusalViolationRatio = 1;  ///< kStepTypeRatio's h-type test
inline constexpr Index kTrRefusalSoc = 2;             ///< trust_region_growth_after_soc
inline constexpr Index kTrRefusalMemory = 4;          ///< trust_region_rejection_memory
inline constexpr Index kTrRefusalRatioCeiling = 8;    ///< trust_region_ratio_ceiling
inline constexpr Index kTrRefusalCapPins = 16;        ///< trust_region_growth_on_cap_pins
inline constexpr Index kTrRefusalFunnelHeadroom = 32; ///< trust_region_funnel_headroom

/// The funnel-headroom guard's factor on the funnel width (KLV's beta).
inline constexpr double kTrHeadroomFraction = 0.99;

/// What the growth decision reads of one accepted major. Violations are engine-unit l1.
struct TrustRegionGrowthInputs {
    bool failure_step = false;
    bool radius_finite = true;
    /// The merged flag (radius or caller-step-limit pin held the step): growth eligibility.
    bool region_limited = false;
    /// The radius's own pin held the step.
    bool radius_binding = false;
    /// The verdict was kAcceptFeasibilityStep.
    bool feasibility_step = false;
    bool soc_applied = false;
    double pred_df = 0.0;
    double actual_df = 0.0;
    double h_old = 0.0;
    /// At the accepted point: the SOC point when SOC was applied.
    double h_new = 0.0;
    /// At the uncorrected trial x + p.
    double h_trial = 0.0;
    /// The model's linearization at the uncorrected step; nullopt when it was not computed.
    std::optional<double> h_lin;
    /// The funnel width after the judgement; +inf without a funnel.
    double width = std::numeric_limits<double>::infinity();
    /// min(2 radius, trust_region_max).
    double grown_radius = 0.0;
    /// TrustRegionRejectionMemory::cap().
    double rejection_cap = std::numeric_limits<double>::infinity();
};

struct TrustRegionGrowthDecision {
    bool grows = false;
    /// The kTrRefusal* bits of every lever that refused.
    Index refusal = 0;
};

/// The growth decision. The rule's own grant is read first (kStepTypeRatio reads kRatio's
/// grant for the base and the violation ratio for an h-type candidate); a major neither grants
/// carries no refusal bit. At default levers this is trust_region_grows on the merged flag.
inline TrustRegionGrowthDecision trust_region_growth_decision(const SqpAdvancedOptions &a,
                                                              const TrustRegionGrowthInputs &in) {
    if (in.failure_step || !in.radius_finite || !in.region_limited) {
        return {};
    }
    const bool f_ratio_ok = in.pred_df > 0.0 && in.actual_df >= kTrGrowThreshold * in.pred_df;
    bool base = f_ratio_ok;
    if (a.trust_region_growth == SqpTrustRegionGrowth::kRatioOrFeasibilityStep) {
        base = f_ratio_ok || in.feasibility_step;
    } else if (a.trust_region_growth == SqpTrustRegionGrowth::kAnyAccepted) {
        base = true;
    }
    const bool h_branch =
        a.trust_region_growth == SqpTrustRegionGrowth::kStepTypeRatio && in.feasibility_step;
    bool candidate = base;
    double h_ratio = 0.0;
    if (h_branch) {
        const double pred_dh = in.h_lin ? in.h_old - *in.h_lin : 0.0;
        const double noise =
            kNoiseFloorFactor * std::numeric_limits<double>::epsilon() * std::max(1.0, in.h_old);
        candidate = in.h_lin.has_value() && pred_dh > noise &&
                    in.h_old - in.h_new >= kTrGrowThreshold * pred_dh;
        h_ratio = pred_dh > noise ? (in.h_old - in.h_new) / pred_dh : 0.0;
    }
    if (!base && !candidate) {
        return {};
    }
    Index refusal = 0;
    if (h_branch && !candidate) {
        refusal |= kTrRefusalViolationRatio;
    }
    if (!a.trust_region_growth_on_cap_pins && !in.radius_binding) {
        refusal |= kTrRefusalCapPins;
    }
    if (!a.trust_region_growth_after_soc && in.soc_applied) {
        refusal |= kTrRefusalSoc;
    }
    if (candidate) {
        // The ratio that granted the candidate; none for a grant made without one.
        std::optional<double> granted_by;
        switch (a.trust_region_growth) {
        case SqpTrustRegionGrowth::kRatio:
            granted_by = in.actual_df / in.pred_df;
            break;
        case SqpTrustRegionGrowth::kRatioOrFeasibilityStep:
            if (!in.feasibility_step) {
                granted_by = in.actual_df / in.pred_df;
            }
            break;
        case SqpTrustRegionGrowth::kAnyAccepted:
            break;
        case SqpTrustRegionGrowth::kStepTypeRatio:
            granted_by = h_branch ? h_ratio : in.actual_df / in.pred_df;
            break;
        }
        if (granted_by && *granted_by > a.trust_region_ratio_ceiling) {
            refusal |= kTrRefusalRatioCeiling;
        }
    }
    if (a.trust_region_funnel_headroom && std::isfinite(in.width)) {
        const double lin = in.h_lin.value_or(0.0);
        if (lin + 4.0 * std::max(0.0, in.h_trial - lin) > kTrHeadroomFraction * in.width) {
            refusal |= kTrRefusalFunnelHeadroom;
        }
    }
    if (a.trust_region_rejection_memory > 0 && in.grown_radius >= in.rejection_cap) {
        refusal |= kTrRefusalMemory;
    }
    return {candidate && refusal == 0, refusal};
}

/// trust_region_rejection_memory's state: the smallest radius a grown step was rejected at, and
/// the acceptances since the last recorded rejection.
class TrustRegionRejectionMemory {
  public:
    double cap() const { return cap_; }
    Index clean_acceptances() const { return clean_; }
    /// A rejection of a trial solved at `radius`; recorded only when that radius came from a
    /// growth.
    void record_rejection(double radius, bool radius_from_growth) {
        if (!radius_from_growth) {
            return;
        }
        cap_ = std::min(cap_, radius);
        clean_ = 0;
    }
    /// An acceptance, after its growth decision: the `window`-th clears the cap.
    void after_acceptance(Index window) {
        if (++clean_ >= window) {
            clear();
        }
    }
    void clear() {
        cap_ = std::numeric_limits<double>::infinity();
        clean_ = 0;
    }

  private:
    double cap_ = std::numeric_limits<double>::infinity();
    Index clean_ = 0;
};

/// The radius after a judged rejection, and whether it crosses the floor.
struct TrustRegionShrink {
    double radius = 0.0;
    bool to_step = false;
    bool hits_floor = false;
};

/// `shrink_to_step` (trust_region_shrink_to_step) on a step the radius did not hold, not a failed
/// subproblem's: min(0.5 * min(delta, window), 0.5 * step_inf), floored on that value. Otherwise
/// today's halving, with +inf landing on `radius_max` and never on the floor.
inline TrustRegionShrink trust_region_rejection_shrink(bool shrink_to_step, bool radius_binding,
                                                       bool failure_step, double delta,
                                                       double window, double step_inf, double floor,
                                                       double radius_max) {
    if (shrink_to_step && !radius_binding && !failure_step) {
        const double r =
            std::min(kTrShrinkFactor * std::min(delta, window), kTrShrinkFactor * step_inf);
        return {r, true, r < floor};
    }
    if (!std::isfinite(delta)) {
        return {radius_max, false, false};
    }
    return {delta * kTrShrinkFactor, false, delta * kTrShrinkFactor < floor};
}

/// Relative size below which an L-BFGS pair's step counts as zero, ||s||inf <= kZeroStepScale *
/// max(1, ||x||inf) (LbfgsDamped's zero_step_scale): a numerical-zero threshold, far tighter
/// than any convergence tolerance. The driver's own zero-step short-circuit (KLV Algorithm 2's
/// ||d|| = 0) is not this constant: it reads qp_no_motion_tol (qp_problem.h), the QP kernels'
/// no-motion tolerance (docs/migration-0.2.md, "Zero-step short-circuit").
inline constexpr double kZeroStepScale = 1e-12;

/// THE FAILED SUBPROBLEM'S STEP, its second gate (M7 W2 R2,
/// SqpOptions::qp_failure_step): a step whose model predicts no decrease is
/// still judged when its trial violation is at most this fraction of the
/// iterate's -- a real infeasibility reduction, not a funnel-width one.
inline constexpr double kFailureStepViolationFactor = 0.5;

} // namespace hven::detail
