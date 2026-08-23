// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// funnel.cpp -- FunnelStrategy's state machine (KLV Algorithm 2, optimality
// phase), carved out of the class body in
// include/hven/detail/globalization/sqp/globalization.h.
//
// M3 PHASE-C T4, and the FIRST carve of the T-series that moves FLOATING-POINT
// arithmetic across a translation-unit boundary. Both halves of how it is
// argued follow from that.
//
// ---------------------------------------------------------------------------
// WHY IT COSTS NOTHING: THE CALLS WERE ALREADY OUT-OF-LINE, AND THAT WAS READ
// OUT OF THE OBJECT FILES, NOT ASSUMED.
//
// The plan's premise is that FunnelStrategy is "already virtual-dispatched
// through GlobalizationStrategy, so out-of-lining costs nothing the vtable was
// not already costing". At the base commit, in every object of a clean Release
// build:
//
//   * `reset` and `judge` -- the two the driver's major loop calls -- were
//     referenced from exactly ONE place, the FunnelStrategy vtable
//     (a `.data.rel.ro._ZTVN4hven7solvers14FunnelStrategyE` relocation). Not a
//     single call site held a direct relocation to either, so every call
//     really did go through the vtable slot and this carve cannot remove an
//     inlining that never happened.
//   * `resume_from_restoration` held exactly one DIRECT (R_X86_64_PLT32)
//     reference per driver-carrying object, from `SqpDriver::solve_impl`: the
//     concrete-typed `dynamic_cast<FunnelStrategy *>` warm-ingest re-base in
//     sqp_driver.h. The class is `final`, so clang devirtualized that one site
//     -- and then emitted a CALL rather than an inline expansion. The carve
//     therefore changes the callee's address from a local weak symbol to a
//     library symbol and changes nothing else about that site, which in any
//     case runs at most once per solve.
//   * the four members that ARE inlined at every use -- `begin_full_step`,
//     `width`, `initialized`, `in_full_step` -- emit no symbol in any object in
//     the build, which is what full inlining looks like. They therefore STAY
//     INLINE IN THE HEADER. Moving them is the one thing that would falsify
//     the premise, so it is deliberately not done, and the header says so.
//
// ---------------------------------------------------------------------------
// THE FP, AND WHAT CARRIES THE PROOF.
//
// Unlike the printing/options carves that preceded it, this file COMPUTES with
// doubles: Eq. (9)'s max(tau_bar, kappa_bar*h0), Eq. (10)'s delta*h_old*h_old,
// Eq. (11)'s sigma*pred_df, Eq. (12)'s beta*width_, and Eq. (13)'s convex
// combination. Under -ffast-math those are re-associable and contractible, so a
// boundary that put different flags on the two sides could change a verdict and
// therefore an iterate.
//
// Since M3 phase-C U0 there is ONE uniform flag regime: this TU, the header it
// was carved from, and every consumer compile with the same options, so these
// expressions are compiled exactly as they were compiled inline. That claim
// comes with its falsifier: the proof of this carve is BIT-IDENTITY -- the
// 57-cell walk census and the 17-cell bench must reproduce every asserted
// counter column byte-for-byte across it. Any counter movement whatsoever means
// the arithmetic moved, and the carve is reverted rather than re-derived.
//
// ---------------------------------------------------------------------------
// The class contract, the KLV transcription, the six recorded divergences from
// the interior-point port, and every "why" behind the constants live in
// globalization.h and stay there. This file carries the executable form and the
// equation-by-equation comments that belong to the code itself.

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

    // NON-FINITE TRIALS ARE NOT SILENTLY CLEAN -- sqp_driver.h's discipline,
    // and here it is a wrong-ANSWER guard rather than a robustness nicety:
    // every comparison against NaN is false, so an unguarded NaN h_new
    // would pass Eq. (8) (`h_new > tau` false), fail Eq. (10) (`>=` false),
    // and then pass Eq. (12) (`<=` false -> ... ) only by accident of which
    // way each test is written. Rejecting up front is the only defensible
    // reading: nothing has been measured, so nothing can be accepted. The
    // driver then shrinks the radius, exactly as for any other rejection.
    if (!std::isfinite(ctx.f_old) || !std::isfinite(ctx.f_new) || !std::isfinite(ctx.h_old) ||
        !std::isfinite(ctx.h_new) || !std::isfinite(ctx.pred_df)) {
        return StepVerdict::kReject;
    }

    // THE FULL-STEP MODE (Task 5). Deliberately AFTER the non-finite guard
    // above and BEFORE every one of Algorithm 2's tests -- see the class's
    // THE FULL-STEP MODE note for both placements. The width is not
    // touched, so the mode is exactly "Algorithm 2 is not consulted", not
    // "Algorithm 2 is consulted with different constants".
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
    // h_old*h_old overflows to +inf for h_old > ~1.34e154. Benign, and
    // benign in the safe direction: pred_df is finite (guarded above), so
    // `pred_df >= inf` is false, the trial is classified h-type, and Eq.
    // (12) then judges it against the finite width -- which is the correct
    // treatment of an iterate that infeasible anyway.
    if (ctx.pred_df >= kFunnelDelta * ctx.h_old * ctx.h_old) {
        // f-TYPE. Eq. (11): the Armijo-type sufficient decrease on f.
        // Accepted or not, the width does not move (KLV Thm. 1, Eq. (17)).
        const double actual_df = ctx.f_old - ctx.f_new; // delta_f, Eq. (7a)
        return actual_df >= kFunnelSigma * ctx.pred_df ? StepVerdict::kAcceptF
                                                       : StepVerdict::kReject;
    }

    // h-TYPE. Eq. (12): the funnel sufficient decrease condition.
    if (ctx.h_new <= detail::kFunnelBeta * width_) {
        // Eq. (13). Written in the paper's own term order so the arithmetic
        // is the arithmetic the tests hand-derive.
        //
        // A TESTING BLIND SPOT, acknowledged rather than papered over: at
        // Table 1's kappa = 0.5 this expression is SYMMETRIC in its two
        // arguments, so (1-kappa)*width + kappa*h_new computes the same
        // number and no black-box test can distinguish the roles of h_new
        // and tau here. What pins the roles is (i) kappa's exact value,
        // asserted in FunnelConstants.MatchKlvTableOne, and (ii) the fact
        // that the two orderings coincide identically at 0.5, asserted in
        // FunnelHType.KappaRoleIsUnobservableAtOneHalf so that the blind
        // spot is recorded in the suite. Should kappa ever move off 0.5,
        // that test fails and the existing Eq. (13) arithmetic assertions
        // become role-sensitive automatically.
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
