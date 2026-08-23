// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// FunnelStrategy's state machine -- KLV Algorithm 2, optimality phase. The
// class contract, the KLV transcription and every "why" behind the constants
// live in globalization.h.

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/detail/globalization/sqp/globalization.h>

namespace hven::solvers {

void FunnelStrategy::reset(double h0) {
    if (!(h0 >= 0.0) || !std::isfinite(h0)) { // the !(>=) form also catches NaN
        throw std::invalid_argument(
            fmt::format("FunnelStrategy::reset: h0 ({}) must be finite and >= 0 (it is the "
                        "constraint violation at the start point)",
                        h0));
    }
    width_ = std::max(kFunnelTauBar, kFunnelKappaBar * h0);
    initialized_ = true;
}

StepVerdict FunnelStrategy::judge(const StepContext &ctx) {
    if (!initialized_) {
        throw std::logic_error("FunnelStrategy::judge: called before reset(h0); the funnel "
                               "width is undefined until a solve has been started");
    }

    // Wrong-answer guard, not a robustness nicety: every comparison against a
    // NaN is false, so an unguarded NaN would be classified by accident of
    // how each test is written (the guard rejects infinities too). Nothing
    // has been measured here, so nothing is accepted; the driver shrinks the
    // radius as for any other rejection.
    if (!std::isfinite(ctx.f_old) || !std::isfinite(ctx.f_new) || !std::isfinite(ctx.h_old) ||
        !std::isfinite(ctx.h_new) || !std::isfinite(ctx.pred_df)) {
        return StepVerdict::kReject;
    }

    // Full-step mode: placed deliberately AFTER the non-finite guard and
    // BEFORE every one of Algorithm 2's tests -- see the class's FULL-STEP
    // MODE note for both placements. The width is not touched, so the mode is
    // exactly "Algorithm 2 is not consulted", not "consulted with different
    // constants".
    if (full_step_) {
        return StepVerdict::kAcceptF;
    }

    // Eq. (8) -- the funnel condition, necessary for ANY acceptance.
    if (!(ctx.h_new <= width_)) {
        // The restoration signature; see the class note for all five
        // conjuncts and for why (e) is not optional.
        const bool infeasible_stationary =
            ctx.h_old > kFunnelFeasEps &&                       // (b)
            ctx.pred_df <= 0.0 &&                               // (c) weak on its own
            ctx.h_new >= ctx.h_old &&                           // (d)
            ctx.rejections_at_iterate >= kRestoreMinRejections; // (e) Lemma 5's hypothesis
        return infeasible_stationary ? StepVerdict::kRestore : StepVerdict::kReject;
    }

    // Eq. (10) -- the switching condition selects the iteration type. The
    // argument is h_old (h^(k), the CURRENT iterate), not h_new: Eq. (10)
    // asks whether the model promises enough decrease to justify ignoring
    // the infeasibility WE ARE AT.
    //
    // h_old*h_old overflows to +inf for h_old > ~1.34e154. Benign, and in the
    // safe direction: pred_df is finite (guarded above), so `pred_df >= inf`
    // is false, the trial is classified h-type, and Eq. (12) then judges it
    // against the finite width.
    if (ctx.pred_df >= kFunnelDelta * ctx.h_old * ctx.h_old) {
        // f-TYPE. Eq. (11): the Armijo-type sufficient decrease on f.
        // Accepted or not, the width does not move (KLV Thm. 1, Eq. (17)).
        const double actual_df = ctx.f_old - ctx.f_new; // delta_f, Eq. (7a)
        return actual_df >= kFunnelSigma * ctx.pred_df ? StepVerdict::kAcceptF
                                                       : StepVerdict::kReject;
    }

    // h-TYPE. Eq. (12): the funnel sufficient decrease condition.
    if (ctx.h_new <= detail::kFunnelBeta * width_) {
        // Eq. (13), written in the paper's own term order so the arithmetic is
        // the arithmetic the tests hand-derive. A testing blind spot,
        // recorded rather than papered over: at Table 1's kappa = 0.5 this
        // expression is SYMMETRIC in its two arguments, so no black-box test
        // can distinguish the roles of h_new and tau here. The roles are
        // pinned by (i) kappa's exact value (FunnelConstants.MatchKlvTableOne)
        // and (ii) the symmetry being asserted explicitly
        // (FunnelHType.KappaRoleIsUnobservableAtOneHalf); moving kappa off 0.5
        // makes the existing Eq. (13) arithmetic assertions role-sensitive
        // automatically.
        width_ = (1.0 - detail::kFunnelKappa) * ctx.h_new + detail::kFunnelKappa * width_;
        return StepVerdict::kAcceptH;
    }

    return StepVerdict::kReject;
}

void FunnelStrategy::resume_from_restoration(double h_restored) {
    if (!initialized_) {
        throw std::logic_error("FunnelStrategy::resume_from_restoration: called before "
                               "reset(h0); there is no funnel width to re-base");
    }
    if (!(h_restored >= 0.0) || !std::isfinite(h_restored)) { // the !(>=) form catches NaN
        throw std::invalid_argument(
            fmt::format("FunnelStrategy::resume_from_restoration: h_restored ({}) must be "
                        "finite and >= 0 (it is the constraint violation at the restored "
                        "point)",
                        h_restored));
    }
    width_ = (1.0 - detail::kFunnelKappa) * h_restored + detail::kFunnelKappa * width_;
    full_step_ = false;
}

} // namespace hven::solvers
