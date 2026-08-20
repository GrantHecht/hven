// tests/sqp/test_funnel.cpp — Task 5: the KLV funnel acceptance test, in
// ISOLATION (no driver, no NLP, no QP). Every fixture is a scripted
// StepContext sequence whose expected verdict is HAND-DERIVED from the
// transcribed rules, and every derivation cites the equation it comes from:
//
//   [KLV] D. Kiessling, S. Leyffer, C. Vanaret, "A Unified Funnel Restoration
//         SQP Algorithm", arXiv:2409.09208, Math. Program. (2025).
//
// Equation map (see include/hven/detail/globalization/sqp/globalization.h for the full
// source-mapping note):
//   Eq. (8)  h(x̂) <= τ                     funnel condition (membership)
//   Eq. (9)  τ⁰ = max(τ̄, κ̄·h⁰)             initialization
//   Eq. (10) Δm_f(d) >= δ·(h)²              switching condition (f-type)
//   Eq. (11) Δf >= σ·Δm_f(d)                Armijo sufficient decrease
//   Eq. (12) h(x̂) <= β·τ                    funnel sufficient decrease (h-type)
//   Eq. (13) τ⁺ = (1−κ)·h(x̂) + κ·τ          width shrink on an h-type accept
//   Thm. 1   τ⁺ <= θ·τ, θ = 1−(1−β)(1−κ)    width contraction factor
//   Table 1  τ̄=100, κ̄=1.25, δ=0.999, σ=1e-4, β=0.99, κ=0.5
//
// Batteries:
//   FunnelConstants.*     -- every constant against KLV Table 1 / its stated
//                            admissible range. This is the port-discipline
//                            gate: if a constant drifts, this fails first.
//   FunnelInit.*          -- Eq. (9), including the τ̄/κ̄ crossover, and the
//                            uninitialized/invalid-argument contract.
//   FunnelHType.*         -- Eq. (12) acceptance and the Eq. (13) shrink, to
//                            the exact arithmetic, plus the Eq. (12) boundary.
//   FunnelFType.*         -- Eq. (10) + Eq. (11), the width left UNCHANGED
//                            (Thm. 1's f-type branch), and both boundaries.
//   FunnelMembership.*    -- Eq. (8): a trial above the width is rejected
//                            before either type test runs.
//   FunnelRestore.*       -- the infeasible-stationary signature, one
//                            near-miss per conjunct (all must stay kReject),
//                            and the healthy-overshoot regression that
//                            conjunct (e) exists to prevent.
//   FunnelInvariant.*     -- funnel-width monotonicity across scripted mixed
//                            sequences (Thm. 1), and reset() clearing state.
//   FunnelFullStep.*      -- Phase-4 Task 5's full-step mode: what it accepts,
//                            what it still refuses (a non-finite trial), and
//                            that resume_from_restoration is its only exit.
//                            NOT a KLV battery -- see globalization.h's THE
//                            FULL-STEP MODE note for its own source, [KD].
//   FunnelMeasure.*       -- constraint_violation_l1 == the KLV h.

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/drivers/sqp_driver.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;

namespace {

// A StepContext that is deliberately inert: h_old == h_new == 0, no predicted
// decrease, no objective change. Fixtures below set only the fields their
// derivation depends on, so an unset field can never quietly carry a verdict.
// `rejections` is StepContext::rejections_at_iterate. It defaults to 0 -- the
// first trial at a fresh iterate -- so every fixture that does NOT mention it
// is asserting behaviour at the start of a shrink sequence, which is where the
// restoration signature must stay silent (conjunct (e)).
StepContext ctx(double f_old, double f_new, double h_old, double h_new, double pred_df,
                Index rejections = 0) {
    StepContext c;
    c.f_old = f_old;
    c.f_new = f_new;
    c.h_old = h_old;
    c.h_new = h_new;
    c.pred_df = pred_df;
    c.rejections_at_iterate = rejections;
    return c;
}

// The same, at a point where the driver has already shrunk the radius enough
// times for Lemma 5's "for Delta_TR sufficiently small" hypothesis to have
// evidence behind it -- i.e. where the restoration signature is permitted to
// fire at all.
StepContext stalled_ctx(double f_old, double f_new, double h_old, double h_new, double pred_df) {
    return ctx(f_old, f_new, h_old, h_new, pred_df, kRestoreMinRejections);
}

// A funnel initialized at h⁰ = 800, so (Eq. 9) τ = max(100, 1.25·800) = 1000.
// Every fixture that wants a wide, round funnel starts here.
FunnelStrategy funnel_at_1000() {
    FunnelStrategy s;
    s.reset(800.0);
    return s;
}

} // namespace

// =============================================================================
// FunnelConstants — KLV Table 1 and the admissible ranges stated with each
// equation. The reviewer's constant-tracing gate, executable.
// =============================================================================

TEST(FunnelConstants, MatchKlvTableOne) {
    EXPECT_DOUBLE_EQ(kFunnelTauBar, 100.0);      // τ̄,  Table 1 (Eq. 9)
    EXPECT_DOUBLE_EQ(kFunnelKappaBar, 1.25);     // κ̄,  Table 1 (Eq. 9)
    EXPECT_DOUBLE_EQ(kFunnelDelta, 0.999);       // δ,  Table 1 (Eq. 10)
    EXPECT_DOUBLE_EQ(kFunnelSigma, 1.0e-4);      // σ,  Table 1 (Eq. 11)
    EXPECT_DOUBLE_EQ(detail::kFunnelBeta, 0.99); // β,  Table 1 (Eq. 12)
    EXPECT_DOUBLE_EQ(detail::kFunnelKappa, 0.5); // κ,  Table 1 (Eq. 13)
    EXPECT_DOUBLE_EQ(kFunnelFeasEps, 1.0e-6);    // ε,  Sec. 5.1 (infeasible-stationary test)
}

TEST(FunnelConstants, LieInTheirStatedRanges) {
    EXPECT_GT(kFunnelTauBar, 0.0);   // "τ̄ > 0" — text under Eq. (9)
    EXPECT_GT(kFunnelKappaBar, 1.0); // "κ̄ > 1" — text under Eq. (9)
    EXPECT_GT(kFunnelDelta, 0.0);    // "δ ∈ (0,1)" — Eq. (10)
    EXPECT_LT(kFunnelDelta, 1.0);
    EXPECT_GT(kFunnelSigma, 0.0); // "σ ∈ (0,1)" — Eq. (11)
    EXPECT_LT(kFunnelSigma, 1.0);
    EXPECT_GT(detail::kFunnelBeta, 0.0); // "β ∈ (0,1)" — Eq. (12)
    EXPECT_LT(detail::kFunnelBeta, 1.0);
    EXPECT_GT(detail::kFunnelKappa, 0.0); // "κ ∈ (0,1)" — Eq. (13)
    EXPECT_LT(detail::kFunnelKappa, 1.0);
}

// Thm. 1, case 1: τ⁺ <= θ·τ with θ = 1 − (1−β)(1−κ) ∈ (0,1). With Table 1's
// β = 0.99, κ = 0.5 this is θ = 1 − 0.01·0.5 = 0.995.
TEST(FunnelConstants, TheoremOneContractionFactorIsInZeroOne) {
    const double theta = 1.0 - (1.0 - detail::kFunnelBeta) * (1.0 - detail::kFunnelKappa);
    EXPECT_GT(theta, 0.0);
    EXPECT_LT(theta, 1.0);
    EXPECT_DOUBLE_EQ(theta, 0.995);
}

// =============================================================================
// FunnelInit — Eq. (9): τ⁰ = max(τ̄, κ̄·h⁰), τ̄ > 0 and κ̄ > 1.
// =============================================================================

TEST(FunnelInit, WidthIsMaxOfFloorAndScaledInitialViolation) {
    FunnelStrategy s;

    // κ̄·h⁰ = 0 < τ̄ ⇒ the τ̄ floor wins. A feasible start still gets a funnel
    // of positive width, which is exactly what "τ̄ > 0" is for.
    s.reset(0.0);
    EXPECT_DOUBLE_EQ(s.width(), kFunnelTauBar);

    // κ̄·h⁰ = 1.25·1000 = 1250 > τ̄ ⇒ the scaled term wins.
    s.reset(1000.0);
    EXPECT_DOUBLE_EQ(s.width(), 1250.0);
}

// The crossover sits at h⁰ = τ̄/κ̄ = 100/1.25 = 80.
TEST(FunnelInit, CrossoverBetweenTheFloorAndTheScaledTerm) {
    FunnelStrategy s;

    s.reset(79.0); // κ̄·h⁰ = 98.75 < 100
    EXPECT_DOUBLE_EQ(s.width(), kFunnelTauBar);

    s.reset(80.0); // κ̄·h⁰ = 100 == τ̄ (the tie)
    EXPECT_DOUBLE_EQ(s.width(), kFunnelTauBar);

    s.reset(81.0); // κ̄·h⁰ = 101.25 > 100
    EXPECT_DOUBLE_EQ(s.width(), 101.25);
}

// κ̄ > 1 exists "to ensure that the initial point is acceptable" (text under
// Eq. 9): the start point must satisfy the funnel condition (8) strictly.
TEST(FunnelInit, InitialPointIsStrictlyInsideItsOwnFunnel) {
    for (const double h0 : {0.0, 1e-9, 1.0, 80.0, 1e3, 1e7}) {
        FunnelStrategy s;
        s.reset(h0);
        EXPECT_GT(s.width(), h0) << "h0 = " << h0;
    }
}

TEST(FunnelInit, JudgeBeforeResetThrows) {
    FunnelStrategy s;
    EXPECT_FALSE(s.initialized());
    EXPECT_THROW(s.judge(ctx(1.0, 0.0, 0.0, 0.0, 1.0)), std::logic_error);
}

TEST(FunnelInit, RejectsAnUnusableInitialViolation) {
    FunnelStrategy s;
    EXPECT_THROW(s.reset(-1.0), std::invalid_argument);
    EXPECT_THROW(s.reset(std::numeric_limits<double>::quiet_NaN()), std::invalid_argument);
    EXPECT_THROW(s.reset(std::numeric_limits<double>::infinity()), std::invalid_argument);
    EXPECT_FALSE(s.initialized()); // a rejected reset leaves the object unusable, not half-set
}

// =============================================================================
// FunnelHType — the switching condition (10) FAILS, so the trial is h-type:
// accepted iff Eq. (12) holds, and then the width shrinks by Eq. (13).
// =============================================================================

// Two consecutive h-type steps, both derived by hand.
//
// Step 1: τ = 1000, h = 800, Δm_f = 0.
//   (10): 0 >= 0.999·800² = 639360  ⇒ FALSE ⇒ h-type.
//   (8) : 500 <= 1000               ⇒ inside the funnel.
//   (12): 500 <= 0.99·1000 = 990    ⇒ TRUE ⇒ accept.
//   (13): τ⁺ = 0.5·500 + 0.5·1000 = 750.
//
// Step 2: τ = 750, h = 500, Δm_f = 0.
//   (10): 0 >= 0.999·500² = 249750  ⇒ FALSE ⇒ h-type.
//   (12): 100 <= 0.99·750 = 742.5   ⇒ TRUE ⇒ accept.
//   (13): τ⁺ = 0.5·100 + 0.5·750 = 425.
TEST(FunnelHType, AcceptanceShrinksTheWidthByEquationThirteen) {
    FunnelStrategy s = funnel_at_1000();
    ASSERT_DOUBLE_EQ(s.width(), 1000.0);

    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 500.0, 0.0)), StepVerdict::kAcceptH);
    EXPECT_DOUBLE_EQ(s.width(), 750.0);

    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 500.0, 100.0, 0.0)), StepVerdict::kAcceptH);
    EXPECT_DOUBLE_EQ(s.width(), 425.0);
}

// Eq. (12) is a non-strict "<=": h = β·τ exactly is accepted, and the tiniest
// step above it is not. This pins β itself, not merely its neighbourhood.
TEST(FunnelHType, SufficientDecreaseBoundaryIsInclusive) {
    {
        FunnelStrategy s = funnel_at_1000();
        const double on_boundary = detail::kFunnelBeta * 1000.0;
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, on_boundary, 0.0)), StepVerdict::kAcceptH);
        // (13): τ⁺ = 0.5·(β·1000) + 0.5·1000.
        EXPECT_DOUBLE_EQ(s.width(), (1.0 - detail::kFunnelKappa) * on_boundary +
                                        detail::kFunnelKappa * 1000.0);
    }
    {
        FunnelStrategy s = funnel_at_1000();
        const double above =
            std::nextafter(detail::kFunnelBeta * 1000.0, std::numeric_limits<double>::infinity());
        // Still inside the funnel (Eq. 8 holds: above < 1000), so this is a
        // pure Eq. (12) failure, not a membership failure.
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, above, 0.0)), StepVerdict::kReject);
        EXPECT_DOUBLE_EQ(s.width(), 1000.0); // a rejected trial never moves the width
    }
}

// A TESTING BLIND SPOT, recorded rather than papered over. Eq. (13) is
// (1−κ)·h(x̂) + κ·τ; at Table 1's κ = 0.5 that expression is SYMMETRIC in its
// two arguments, so an implementation that swapped the roles of the trial
// infeasibility and the old width would compute exactly the same number and no
// black-box assertion above could detect it. What holds the roles in place is
// κ's exact value (FunnelConstants.MatchKlvTableOne) plus this test: should κ
// ever move off 0.5, this fails, and every Eq. (13) arithmetic assertion in
// the suite becomes role-sensitive at the same moment.
TEST(FunnelHType, KappaRoleIsUnobservableAtOneHalf) {
    ASSERT_DOUBLE_EQ(detail::kFunnelKappa, 0.5);

    const double h = 123.0;
    const double tau = 456.0;
    const double as_written = (1.0 - detail::kFunnelKappa) * h + detail::kFunnelKappa * tau;
    const double role_swapped = (1.0 - detail::kFunnelKappa) * tau + detail::kFunnelKappa * h;

    EXPECT_DOUBLE_EQ(as_written, role_swapped)
        << "at κ = 0.5 Eq. (13) cannot distinguish its two arguments; if this "
           "fires, κ has changed and the Eq. (13) fixtures now pin the roles";
}

// An h-type step that improves feasibility but WORSENS the objective is still
// accepted: Eq. (12) reads infeasibility alone. (KLV Sec. 2.4.2: "we also
// allow steps that increase both optimality and infeasibility" — the funnel's
// h-type branch never consults f.)
TEST(FunnelHType, ObjectiveIsNotConsultedOnTheHTypePath) {
    FunnelStrategy s = funnel_at_1000();
    EXPECT_EQ(s.judge(ctx(1.0, 1.0e6, 800.0, 500.0, 0.0)), StepVerdict::kAcceptH);
    EXPECT_DOUBLE_EQ(s.width(), 750.0);
}

// =============================================================================
// FunnelFType — the switching condition (10) HOLDS, so the trial is f-type:
// accepted iff the Armijo condition (11) holds. Thm. 1's f-type branch says
// τ⁺ = τ: the width is NOT updated.
// =============================================================================

// τ = 100 (h⁰ = 0 ⇒ Eq. 9 floor). h = 1e-3, Δm_f = 1.0.
//   (10): 1.0 >= 0.999·(1e-3)² = 9.99e-7  ⇒ TRUE ⇒ f-type.
//   (8) : 1e-3 <= 100                     ⇒ inside the funnel.
//   (11): Δf = 10 − 9 = 1 >= 1e-4·1 = 1e-4 ⇒ TRUE ⇒ accept.
TEST(FunnelFType, SwitchingPlusArmijoAcceptsAndLeavesTheWidthUntouched) {
    FunnelStrategy s;
    s.reset(0.0);
    ASSERT_DOUBLE_EQ(s.width(), 100.0);

    EXPECT_EQ(s.judge(ctx(10.0, 9.0, 1.0e-3, 1.0e-3, 1.0)), StepVerdict::kAcceptF);
    EXPECT_DOUBLE_EQ(s.width(), 100.0);
}

// Switching holds, Armijo fails ⇒ kReject. Eq. (11) is checked against σ·Δm_f,
// so a decrease that is merely positive is NOT enough.
TEST(FunnelFType, ArmijoFailureRejectsWithoutFallingBackToTheHTypeBranch) {
    FunnelStrategy s;
    s.reset(0.0);

    // Δf = 1e-5 < σ·Δm_f = 1e-4. Note h_new = 1e-3 <= β·τ = 99 would have
    // passed the h-type test (12) comfortably — Algorithm 2 nests the two
    // branches as if/else-if, so an f-type trial never gets that second look.
    EXPECT_EQ(s.judge(ctx(1.0, 1.0 - 1.0e-5, 1.0e-3, 1.0e-3, 1.0)), StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(s.width(), 100.0);

    // No decrease at all, and an outright increase.
    EXPECT_EQ(s.judge(ctx(1.0, 1.0, 1.0e-3, 1.0e-3, 1.0)), StepVerdict::kReject);
    EXPECT_EQ(s.judge(ctx(1.0, 2.0, 1.0e-3, 1.0e-3, 1.0)), StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(s.width(), 100.0);
}

// Both of Eq. (10) and Eq. (11) are non-strict ">=" — pin each boundary.
TEST(FunnelFType, SwitchingAndArmijoBoundariesAreInclusive) {
    // h IS A POWER OF TWO (2^-10 ~ 1e-3), and that is load-bearing under the
    // unified flag regime (U0, -ffast-math): with h = 1e-3 the boundary value
    // δ·h·h rounds differently depending on multiplication order, which
    // fast-math leaves to the compiler -- the value this test computed sat one
    // ulp below the threshold the strategy computed, and the "exactly on the
    // boundary" case silently became the "one ulp below" case. Scaling by a
    // power of two is EXACT, so δ·h·h is the same double under every
    // association and the boundary construction is flag-independent.
    const double h = 0x1p-10;
    const double on_switching = kFunnelDelta * h * h; // Eq. (10) with equality

    { // Δm_f exactly on the switching boundary ⇒ f-type (and Armijo passes).
        FunnelStrategy s;
        s.reset(0.0);
        EXPECT_EQ(s.judge(ctx(10.0, 9.0, h, h, on_switching)), StepVerdict::kAcceptF);
        EXPECT_DOUBLE_EQ(s.width(), 100.0); // f-type ⇒ width unchanged
    }
    { // A hair BELOW the switching boundary ⇒ h-type instead, and (12)
        // accepts (h = 1e-3 <= 0.99·100), so the width MOVES. Same context but
        // for one ulp of Δm_f: this is what makes δ observable.
        FunnelStrategy s;
        s.reset(0.0);
        const double below = std::nextafter(on_switching, -std::numeric_limits<double>::infinity());
        EXPECT_EQ(s.judge(ctx(10.0, 9.0, h, h, below)), StepVerdict::kAcceptH);
        EXPECT_DOUBLE_EQ(s.width(),
                         (1.0 - detail::kFunnelKappa) * h + detail::kFunnelKappa * 100.0);
    }
    { // Δf straddling σ·Δm_f. Written with a 1% margin rather than an ulp:
        // f_old − f_new is a subtraction, so an exactly-on-the-boundary f_new
        // cannot be constructed without rounding. σ's exact value is pinned in
        // FunnelConstants; what this pins is that Eq. (11) is tested against
        // σ·Δm_f and not against 0 (or some other multiple).
        const double pred = 1.0;
        const double f_old = 1.0;
        {
            FunnelStrategy s;
            s.reset(0.0);
            EXPECT_EQ(s.judge(ctx(f_old, f_old - 1.01 * kFunnelSigma * pred, h, h, pred)),
                      StepVerdict::kAcceptF);
        }
        {
            FunnelStrategy s;
            s.reset(0.0);
            EXPECT_EQ(s.judge(ctx(f_old, f_old - 0.99 * kFunnelSigma * pred, h, h, pred)),
                      StepVerdict::kReject);
        }
    }
    { // The one boundary that IS exact in floating point: at a feasible point
        // with no predicted decrease, Eq. (10) holds with equality (0 >= 0), so
        // the trial is f-type, and Eq. (11) then demands only Δf >= 0.
        FunnelStrategy s;
        s.reset(0.0);
        EXPECT_EQ(s.judge(ctx(1.0, 1.0, 0.0, 0.0, 0.0)), StepVerdict::kAcceptF);
        EXPECT_DOUBLE_EQ(s.width(), 100.0);
        EXPECT_EQ(s.judge(ctx(1.0, std::nextafter(1.0, 2.0), 0.0, 0.0, 0.0)), StepVerdict::kReject);
    }
}

// The switching condition scales with (h)², so the SAME predicted decrease is
// f-type near feasibility and h-type far from it. This is the rule that makes
// the method "recognize the two scenarios" (KLV Sec. 2.4).
TEST(FunnelFType, SwitchingConditionScalesWithTheSquareOfInfeasibility) {
    // Δm_f = 1.0. (10) holds iff 1.0 >= 0.999·h², i.e. h <= 1.0005... .
    FunnelStrategy near;
    near.reset(0.0);
    EXPECT_EQ(near.judge(ctx(10.0, 9.0, 1.0, 1.0, 1.0)), StepVerdict::kAcceptF);

    // Same Δm_f, h = 10 ⇒ 1.0 >= 99.9 is false ⇒ h-type, and (12) accepts.
    FunnelStrategy far;
    far.reset(0.0);
    EXPECT_EQ(far.judge(ctx(10.0, 9.0, 10.0, 10.0, 1.0)), StepVerdict::kAcceptH);
    EXPECT_LT(far.width(), 100.0);
}

// Eq. (10) is δ·(h⁽ᵏ⁾)² — the infeasibility of the CURRENT iterate, not of the
// trial. Every other f-type fixture happens to use h_old == h_new (an f-type
// step near feasibility barely moves h), which leaves the argument unpinned:
// swapping h_old for h_new there changes nothing. These two cases straddle the
// switching threshold in opposite directions, so the swap flips the verdict.
TEST(FunnelFType, SwitchingConditionReadsTheCurrentIterateNotTheTrial) {
    // τ = 100. Δm_f = 5. δ·h_old² = 0.999, δ·h_new² = 99.9.
    //   as written (h_old): 5 >= 0.999  ⇒ f-type, Δf = 1 >= 5e-4 ⇒ kAcceptF.
    //   with h_new:         5 >= 99.9   ⇒ h-type, 10 <= 99      ⇒ kAcceptH.
    {
        FunnelStrategy s;
        s.reset(0.0);
        EXPECT_EQ(s.judge(ctx(10.0, 9.0, 1.0, 10.0, 5.0)), StepVerdict::kAcceptF);
        EXPECT_DOUBLE_EQ(s.width(), 100.0) << "an f-type accept must not move the width";
    }
    // The mirror image: h_old and h_new exchanged, same Δm_f.
    //   as written (h_old): 5 >= 99.9   ⇒ h-type, 1 <= 99       ⇒ kAcceptH,
    //                                     τ⁺ = 0.5·1 + 0.5·100 = 50.5.
    //   with h_new:         5 >= 0.999  ⇒ f-type                ⇒ kAcceptF.
    {
        FunnelStrategy s;
        s.reset(0.0);
        EXPECT_EQ(s.judge(ctx(10.0, 9.0, 10.0, 1.0, 5.0)), StepVerdict::kAcceptH);
        EXPECT_DOUBLE_EQ(s.width(), 50.5);
    }
}

// =============================================================================
// FunnelMembership — Eq. (8): the funnel condition is NECESSARY. A trial above
// the width is rejected before either type test runs.
// =============================================================================

// τ = 1000; h_new = 1500 > τ. Δm_f = 5 > 0, so this is NOT the restoration
// signature (see FunnelRestore) — it is a plain funnel-incompatible step.
TEST(FunnelMembership, ViolationAboveTheWidthIsRejected) {
    FunnelStrategy s = funnel_at_1000();
    EXPECT_EQ(s.judge(ctx(10.0, 1.0, 800.0, 1500.0, 5.0)), StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0);
}

// Membership outranks BOTH type tests: a trial that would have sailed through
// the Armijo test (huge objective decrease, switching condition satisfied) is
// still rejected for leaving the funnel.
TEST(FunnelMembership, OutranksAnOtherwisePerfectFTypeStep) {
    FunnelStrategy s = funnel_at_1000();
    // Δm_f = 1e12 >= 0.999·800² ⇒ (10) holds; Δf = 1e12 ⇒ (11) holds.
    // But h_new = 1500 > τ = 1000 ⇒ (8) fails.
    EXPECT_EQ(s.judge(ctx(1.0e12, 0.0, 800.0, 1500.0, 1.0e12)), StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0);
}

// Eq. (8) is "<=": exactly at the width is inside.
TEST(FunnelMembership, BoundaryIsInclusive) {
    FunnelStrategy s = funnel_at_1000();
    // h_new = τ = 1000 exactly: passes (8), fails (12) (1000 > 990) ⇒ kReject,
    // but for the Eq. (12) reason, and the width still does not move.
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 1000.0, 0.0)), StepVerdict::kReject);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0);

    const double above = std::nextafter(1000.0, std::numeric_limits<double>::infinity());
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 800.0, above, 0.0)), StepVerdict::kRestore)
        << "one ulp above the width, with Δm_f = 0, h not improving and the radius "
           "already shrunk, is the infeasible-stationary signature";
    // ... but only once conjunct (e) has evidence. At a fresh iterate the very
    // same trial is a plain overshoot.
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, above, 0.0)), StepVerdict::kReject);
}

// =============================================================================
// FunnelRestore — the infeasible-stationary signature. All four conjuncts must
// hold; each near-miss below drops exactly one and must fall back to kReject.
// =============================================================================

// τ = 1000, h_old = 800 > ε, h_new = 1500 > τ (Eq. 8 fails) and >= h_old, and
// Δm_f = 0 (the QP model offers no objective decrease at all).
TEST(FunnelRestore, InfeasibleStationarySignatureRequestsRestoration) {
    FunnelStrategy s = funnel_at_1000();
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 800.0, 1500.0, 0.0)), StepVerdict::kRestore);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0) << "a restoration request must not move the width";

    // A negative predicted decrease (a model that predicts an INCREASE) is the
    // same signature a fortiori.
    FunnelStrategy t = funnel_at_1000();
    EXPECT_EQ(t.judge(stalled_ctx(10.0, 10.0, 800.0, 1500.0, -1.0)), StepVerdict::kRestore);
}

TEST(FunnelRestore, NearMissModelStillPredictsDecrease) {
    // Drops "Δm_f <= 0": the model still offers objective progress, so the
    // right response is to shrink the trust region and retry, not to restore.
    FunnelStrategy s = funnel_at_1000();
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 800.0, 1500.0, 1.0e-12)), StepVerdict::kReject);
}

TEST(FunnelRestore, NearMissCurrentPointIsEssentiallyFeasible) {
    // Drops "h_old > ε": at a feasible point a funnel-incompatible step is an
    // overshoot, not an infeasible stationary point (KLV Sec. 5.1's
    // infeasible-stationary test requires ‖c(x*)‖ > ε).
    FunnelStrategy s;
    s.reset(0.0); // τ = 100
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 1.0e-9, 200.0, 0.0)), StepVerdict::kReject);

    // Exactly at ε is NOT "> ε".
    FunnelStrategy t;
    t.reset(0.0);
    EXPECT_EQ(t.judge(stalled_ctx(10.0, 10.0, kFunnelFeasEps, 200.0, 0.0)), StepVerdict::kReject);
}

TEST(FunnelRestore, NearMissStepStillImprovesFeasibility) {
    // Drops "h_new >= h_old": the step DOES reduce infeasibility, it merely
    // has not reduced it into the funnel yet. Reachable only from a current
    // iterate that is itself outside the funnel.
    FunnelStrategy s;
    s.reset(0.0); // τ = 100
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 500.0, 200.0, 0.0)), StepVerdict::kReject);
}

// REGRESSION. Conjunct (e) exists because conjuncts (a)-(d) alone fire on a
// PERFECTLY HEALTHY problem at its very first trial. Worked instance:
//
//     min x   s.t.  x² − 1 = 0,     x0 = 0.01
//
//   h_old = |0.01² − 1|          = 0.9999
//   τ     = max(100, 1.25·0.9999) = 100                          (Eq. 9)
//   QP    : 2·x0·p = −c ⇒ 0.02·p = 0.9999 ⇒ p = 49.995
//   pred_df = −(½·W·p² + g·p) = −(0 + 1·49.995) = −49.995   (W = 0 at λ = 0)
//   x_new = 50.005,  h_new = |50.005² − 1| = 2499.500025
//
// (a) 2499.5 > 100 ✓  (b) 0.9999 > 1e-6 ✓  (c) −49.995 ≤ 0 ✓  (d) h_new ≥ h_old ✓
//
// Nothing is wrong here: pred_df < 0 because the linearization Je p = −cE
// drags the step away from p = 0 and the objective pays for it — the normal
// state of affairs at an infeasible iterate. The paper's response is kReject
// plus a radius shrink, and its own Lemma 3 guarantees that shrink succeeds
// (a step is funnel-acceptable once Δ² ≤ 2βτ/(mnM), here Δ ≲ 9.9).
TEST(FunnelRestore, HealthyOvershootIsRejectedNotRestored) {
    const double h_old = 0.9999;
    const double p = 49.995;
    const double x_new = 0.01 + p;
    const double h_new = x_new * x_new - 1.0; // 2499.500025
    const double pred_df = -p;

    FunnelStrategy s;
    s.reset(h_old);
    ASSERT_DOUBLE_EQ(s.width(), kFunnelTauBar);

    // Trial #1 at a fresh iterate: conjuncts (a)-(d) all hold, (e) does not.
    EXPECT_EQ(s.judge(ctx(0.01, x_new, h_old, h_new, pred_df)), StepVerdict::kReject);

    // Still kReject while the driver is shrinking, right up to the threshold.
    for (Index r = 1; r < kRestoreMinRejections; ++r) {
        EXPECT_EQ(s.judge(ctx(0.01, x_new, h_old, h_new, pred_df, r)), StepVerdict::kReject)
            << "rejections_at_iterate = " << r;
    }

    // Once the radius has demonstrably been shrunk kRestoreMinRejections times
    // without the trial escaping the funnel, the SAME context is a dead end.
    EXPECT_EQ(s.judge(ctx(0.01, x_new, h_old, h_new, pred_df, kRestoreMinRejections)),
              StepVerdict::kRestore);
    EXPECT_DOUBLE_EQ(s.width(), kFunnelTauBar);
}

TEST(FunnelRestore, NearMissRadiusHasNotBeenShrunkYet) {
    // Drops conjunct (e) alone from the otherwise-complete signature.
    FunnelStrategy s = funnel_at_1000();
    for (Index r = 0; r < kRestoreMinRejections; ++r) {
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 1500.0, 0.0, r)), StepVerdict::kReject)
            << "rejections_at_iterate = " << r;
    }
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 1500.0, 0.0, kRestoreMinRejections)),
              StepVerdict::kRestore);
}

TEST(FunnelRestore, NearMissTrialIsInsideTheFunnel) {
    // Drops "h_new > τ": a stationary model at an infeasible point whose trial
    // still lies inside the funnel is judged by the ordinary h-type rule.
    FunnelStrategy s = funnel_at_1000();
    // h_new = 900 <= τ = 1000, and 900 <= β·τ = 990 ⇒ Eq. (12) accepts.
    EXPECT_EQ(s.judge(stalled_ctx(10.0, 10.0, 800.0, 900.0, 0.0)), StepVerdict::kAcceptH);
}

// =============================================================================
// FunnelInvariant — Thm. 1's monotonicity, and reset().
// =============================================================================

// The property: across ANY sequence of judgements, the width is monotonically
// non-increasing (KLV, text under Eq. (9): "the funnel ... whose width is
// monotonically non-increasing; that is, τ⁽ᵏ⁺¹⁾ <= τ⁽ᵏ⁾ for all k >= 0"), it
// moves ONLY on a kAcceptH, and each such move contracts by at least the
// factor θ = 1 − (1−β)(1−κ) from Thm. 1 case 1.
TEST(FunnelInvariant, WidthIsMonotoneAndContractsOnlyOnHTypeAccepts) {
    const double theta = 1.0 - (1.0 - detail::kFunnelBeta) * (1.0 - detail::kFunnelKappa);

    // A scripted mixed sequence: h-type accepts, f-type accepts, rejections
    // for each of the three reasons, and a restoration request.
    const std::vector<StepContext> script = {
        ctx(10.0, 10.0, 800.0, 500.0, 0.0),         // h-type accept
        ctx(10.0, 10.0, 500.0, 900.0, 5.0),         // membership reject (900 > τ=750)
        ctx(10.0, 10.0, 500.0, 400.0, 0.0),         // h-type accept
        ctx(10.0, 9.0, 1.0e-4, 1.0e-4, 1.0),        // f-type accept
        ctx(1.0, 1.0, 1.0e-4, 1.0e-4, 1.0),         // f-type Armijo reject
        ctx(10.0, 10.0, 1.0e-4, 1.0e-4, 0.0),       // h-type accept (tiny h)
        stalled_ctx(10.0, 10.0, 400.0, 1.0e6, 0.0), // restore
        ctx(10.0, 10.0, 1.0e-4, 1.0e-5, 0.0),       // h-type accept
        ctx(10.0, 9.0, 1.0e-6, 1.0e-6, 1.0),        // f-type accept
    };

    FunnelStrategy s = funnel_at_1000();
    double previous = s.width();
    for (std::size_t i = 0; i < script.size(); ++i) {
        const StepVerdict v = s.judge(script[i]);
        const double now = s.width();

        EXPECT_LE(now, previous) << "step " << i << ": the funnel width widened";

        if (v == StepVerdict::kAcceptH) {
            EXPECT_LT(now, previous) << "step " << i << ": an h-type accept must shrink it";
            EXPECT_LE(now, theta * previous)
                << "step " << i << ": Thm. 1's contraction factor was not met";
            EXPECT_DOUBLE_EQ(now, (1.0 - detail::kFunnelKappa) * script[i].h_new +
                                      detail::kFunnelKappa * previous)
                << "step " << i << ": Eq. (13) arithmetic";
        } else {
            EXPECT_DOUBLE_EQ(now, previous)
                << "step " << i << ": only an h-type accept may move the width";
        }
        previous = now;
    }
    EXPECT_LT(s.width(), 1000.0);
}

// The same invariant driven by a deterministic pseudo-random script, so the
// property is exercised over many orderings rather than one hand-picked path.
TEST(FunnelInvariant, WidthIsMonotoneOverManyScriptedSequences) {
    const double theta = 1.0 - (1.0 - detail::kFunnelBeta) * (1.0 - detail::kFunnelKappa); // Thm. 1
    std::uint64_t seed = 0x9e3779b97f4a7c15ULL;
    auto next = [&seed]() {
        seed ^= seed << 13;
        seed ^= seed >> 7;
        seed ^= seed << 17;
        return static_cast<double>(seed >> 11) / static_cast<double>(1ULL << 53);
    };

    int f_accepts = 0;
    int h_accepts = 0;

    for (int trial = 0; trial < 200; ++trial) {
        FunnelStrategy s;
        s.reset(next() * 1000.0);
        double previous = s.width();
        for (int step = 0; step < 40; ++step) {
            // A quarter of the draws sit in an F-TYPE BAND: h_old in [0, 8]
            // keeps δ·h_old² <= 63.9, inside the pred_df draw's range, so the
            // switching condition (10) actually fires there. Without this band
            // h_old ~ U[0,1200] makes δ·h_old² astronomically larger than any
            // pred_df drawn and every single case degenerates to h-type.
            const bool f_band = next() < 0.25;
            StepContext c;
            c.h_old = f_band ? next() * 8.0 : next() * 1200.0;
            c.h_new = next() * 1200.0;
            c.f_old = next() * 10.0;
            c.f_new = next() * 10.0;
            c.pred_df = (next() - 0.25) * 100.0;
            c.tr_active = next() > 0.5;
            c.rejections_at_iterate = static_cast<Index>(next() * 6.0);

            const StepVerdict v = s.judge(c);
            EXPECT_LE(s.width(), previous);
            if (v == StepVerdict::kAcceptH) {
                ++h_accepts;
                EXPECT_LT(s.width(), previous);
                EXPECT_LE(s.width(), theta * previous);
                EXPECT_DOUBLE_EQ(s.width(), (1.0 - detail::kFunnelKappa) * c.h_new +
                                                detail::kFunnelKappa * previous)
                    << "Eq. (13) arithmetic";
            } else {
                EXPECT_DOUBLE_EQ(s.width(), previous);
                if (v == StepVerdict::kAcceptF) {
                    ++f_accepts;
                }
            }
            previous = s.width();
        }
    }

    // The band must actually exercise the f-type path, or the assertions above
    // are only ever reached on one branch.
    EXPECT_GT(f_accepts, 0) << "the f-type band produced no f-type accepts";
    EXPECT_GT(h_accepts, 0);
}

TEST(FunnelInvariant, ResetClearsState) {
    FunnelStrategy s = funnel_at_1000();
    ASSERT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 500.0, 0.0)), StepVerdict::kAcceptH);
    ASSERT_DOUBLE_EQ(s.width(), 750.0);

    // A new solve re-derives the width from ITS OWN h⁰ (Eq. 9) — including
    // WIDENING relative to the previous solve's final width. The monotonicity
    // invariant is per-solve, exactly as Thm. 1 states it.
    s.reset(1000.0);
    EXPECT_TRUE(s.initialized());
    EXPECT_DOUBLE_EQ(s.width(), 1250.0);

    // And the funnel behaves like a freshly constructed one at that width.
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 1000.0, 600.0, 0.0)), StepVerdict::kAcceptH);
    EXPECT_DOUBLE_EQ(s.width(),
                     (1.0 - detail::kFunnelKappa) * 600.0 + detail::kFunnelKappa * 1250.0);
}

// =============================================================================
// FunnelResume (Task 9) — the switch BACK from a restoration phase.
//
// KLV Algorithm 2 re-bases the width on exit from restoration by the ORDINARY
// Eq. (13) update at the restored point. It does NOT re-initialize by Eq. (9),
// and the difference is not cosmetic: tau_bar = 100 is an absolute FLOOR, so
// re-initializing a funnel that has tightened would widen it back out and
// forfeit the monotonicity KLV Thm. 1 case 1 sums over.
// =============================================================================

TEST(FunnelResume, RebasesByEqThirteenAndNotByEqNine) {
    FunnelStrategy s = funnel_at_1000();
    // Tighten it well below tau_bar first, so the two rules disagree loudly:
    // an h-type accept at h = 4 takes tau from 1000 to 502.
    ASSERT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 4.0, 0.0)), StepVerdict::kAcceptH);
    ASSERT_DOUBLE_EQ(s.width(), 502.0);
    ASSERT_EQ(s.judge(ctx(10.0, 10.0, 4.0, 1.0, 0.0)), StepVerdict::kAcceptH);
    ASSERT_DOUBLE_EQ(s.width(), 251.5);
    ASSERT_EQ(s.judge(ctx(10.0, 10.0, 1.0, 0.5, 0.0)), StepVerdict::kAcceptH);
    const double before = s.width();
    ASSERT_DOUBLE_EQ(before, 126.0);

    // A restoration lands at h = 1e-9. Eq. (13): tau_+ = 0.5*1e-9 + 0.5*126.
    s.resume_from_restoration(1.0e-9);
    EXPECT_DOUBLE_EQ(s.width(),
                     (1.0 - detail::kFunnelKappa) * 1.0e-9 + detail::kFunnelKappa * before);
    EXPECT_LT(s.width(), before) << "the width must not grow on the way back";
    // Eq. (9) would have produced max(100, 1.25e-9) = 100 here, which is a
    // DIFFERENT number and, from a tighter funnel, a wider one.
    EXPECT_NE(s.width(), std::max(kFunnelTauBar, kFunnelKappaBar * 1.0e-9));
}

TEST(FunnelResume, WideningIsOnlyPossibleAboveTheCurrentWidth) {
    // The transcription is unconditional (Eq. (13) is not a clamped rule), so
    // the monotonicity of the resume comes from the CALLER's exit condition
    // h_restored <= tau -- which sqp_driver.h's h < feas_tol supplies with room
    // to spare. Pinned in both directions so the dependency is explicit rather
    // than assumed.
    {
        FunnelStrategy s = funnel_at_1000();
        s.resume_from_restoration(1.0); // h << tau
        EXPECT_DOUBLE_EQ(s.width(), 0.5 * 1.0 + 0.5 * 1000.0);
        EXPECT_LT(s.width(), 1000.0);
    }
    {
        FunnelStrategy s = funnel_at_1000();
        s.resume_from_restoration(4000.0); // h > tau: the caller broke its side
        EXPECT_DOUBLE_EQ(s.width(), 0.5 * 4000.0 + 0.5 * 1000.0);
        EXPECT_GT(s.width(), 1000.0);
    }
}

TEST(FunnelResume, RejectsGarbageAndUninitializedUse) {
    {
        FunnelStrategy s; // never reset
        EXPECT_THROW(s.resume_from_restoration(1.0), std::logic_error);
    }
    FunnelStrategy s = funnel_at_1000();
    EXPECT_THROW(s.resume_from_restoration(-1.0), std::invalid_argument);
    EXPECT_THROW(s.resume_from_restoration(std::numeric_limits<double>::quiet_NaN()),
                 std::invalid_argument);
    EXPECT_THROW(s.resume_from_restoration(std::numeric_limits<double>::infinity()),
                 std::invalid_argument);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0) << "a rejected call must not have moved the width";
}

// THE INTERFACE DEFAULT, which is what a caller-supplied strategy that does
// not override the hook gets: reset(h). Pinned because it is a documented
// fallback rather than an accident, and because it is the ONE path on which
// the width can legitimately widen.
TEST(FunnelResume, DefaultImplementationIsAReset) {
    class Minimal final : public GlobalizationStrategy {
      public:
        void reset(double h0) override { last_reset = h0; }
        StepVerdict judge(const StepContext &) override { return StepVerdict::kAcceptF; }
        double last_reset = -1.0;
    };
    Minimal m;
    m.resume_from_restoration(3.5);
    EXPECT_DOUBLE_EQ(m.last_reset, 3.5);
}

// tr_active is carried in StepContext for the DRIVER's radius-update rule
// (KLV Algorithm 3: "if trust region is active at d then increase radius") and
// is deliberately not read by the acceptance test (Algorithm 2 never mentions
// it). Pinned so that a later change cannot quietly fold it into a verdict.
TEST(FunnelInvariant, VerdictDoesNotDependOnTrActive) {
    const std::vector<StepContext> cases = {
        ctx(10.0, 10.0, 800.0, 500.0, 0.0),          // h-type accept
        ctx(10.0, 9.0, 1.0e-3, 1.0e-3, 1.0),         // f-type accept
        ctx(10.0, 10.0, 800.0, 1500.0, 5.0),         // membership reject
        stalled_ctx(10.0, 10.0, 800.0, 1500.0, 0.0), // restore
    };
    for (const StepContext &c : cases) {
        StepContext on = c;
        on.tr_active = true;
        StepContext off = c;
        off.tr_active = false;

        FunnelStrategy a = funnel_at_1000();
        FunnelStrategy b = funnel_at_1000();
        EXPECT_EQ(a.judge(on), b.judge(off));
        EXPECT_DOUBLE_EQ(a.width(), b.width());
    }
}

// A non-finite trial is rejected rather than silently compared: every
// comparison against NaN is false, so an unguarded implementation would fall
// through Eq. (8) and Eq. (10) into the h-type branch and could ACCEPT it
// (this is sqp_driver.h's NON-FINITE ITERATES discipline, applied here).
TEST(FunnelInvariant, NonFiniteTrialIsRejected) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double bad : {nan, inf}) {
        FunnelStrategy s = funnel_at_1000();
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, bad, 0.0)), StepVerdict::kReject);
        EXPECT_EQ(s.judge(ctx(10.0, bad, 800.0, 500.0, 1.0)), StepVerdict::kReject);
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, bad, 500.0, 1.0)), StepVerdict::kReject);
        EXPECT_EQ(s.judge(ctx(bad, 10.0, 800.0, 500.0, 1.0)), StepVerdict::kReject);
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 500.0, bad)), StepVerdict::kReject);
        EXPECT_DOUBLE_EQ(s.width(), 1000.0);
    }
}

// The base-class handle is the interface Task 6 will hold.
TEST(FunnelInvariant, DrivesThroughTheGlobalizationStrategyInterface) {
    FunnelStrategy concrete;
    GlobalizationStrategy &s = concrete;
    s.reset(800.0);
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, 500.0, 0.0)), StepVerdict::kAcceptH);
    EXPECT_DOUBLE_EQ(concrete.width(), 750.0);
}

// =============================================================================
// FunnelFullStep — the Phase-4 Task 5 mode, at the STRATEGY level. The driver
// half (engagement, the watchdog, the counters) is
// tests/test_warm_start.cpp's; these four are the properties this class alone
// is responsible for.
// =============================================================================

// Trials Algorithm 2 would REJECT for three different reasons -- outside the
// funnel (Eq. 8), f-type failing Armijo (Eq. 11), h-type failing sufficient
// decrease (Eq. 12) -- are all accepted under the mode, and the width does not
// move for any of them.
TEST(FunnelFullStep, AcceptsWhatTheFunnelWouldRejectWithoutMovingTheWidth) {
    FunnelStrategy gated = funnel_at_1000();
    const StepContext outside = ctx(10.0, 10.0, 800.0, 5000.0, 0.0); // Eq. (8) fails
    const StepContext armijo = ctx(10.0, 99.0, 0.0, 1.0, 50.0);      // f-type, Eq. (11) fails
    const StepContext hsteep = ctx(10.0, 10.0, 800.0, 999.0, -1.0);  // h-type, Eq. (12) fails
    ASSERT_EQ(gated.judge(outside), StepVerdict::kReject);
    ASSERT_EQ(gated.judge(armijo), StepVerdict::kReject);
    ASSERT_EQ(gated.judge(hsteep), StepVerdict::kReject);

    FunnelStrategy s = funnel_at_1000();
    s.begin_full_step();
    EXPECT_TRUE(s.in_full_step());
    EXPECT_EQ(s.judge(outside), StepVerdict::kAcceptF);
    EXPECT_EQ(s.judge(armijo), StepVerdict::kAcceptF);
    EXPECT_EQ(s.judge(hsteep), StepVerdict::kAcceptF);
    EXPECT_DOUBLE_EQ(s.width(), 1000.0) << "no verdict the mode returns may move tau";
}

// THE MODE IS NOT A LICENCE TO STEP ONTO A NaN: the non-finite guard runs
// AHEAD of it, so an unmeasurable trial is still rejected. Without this the
// driver would accept its way onto a point it cannot evaluate.
TEST(FunnelFullStep, StillRejectsANonFiniteTrial) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double bad : {nan, inf}) {
        FunnelStrategy s = funnel_at_1000();
        s.begin_full_step();
        EXPECT_EQ(s.judge(ctx(10.0, 10.0, 800.0, bad, 0.0)), StepVerdict::kReject);
        EXPECT_EQ(s.judge(ctx(10.0, bad, 800.0, 500.0, 1.0)), StepVerdict::kReject);
        EXPECT_TRUE(s.in_full_step()) << "a rejected trial does not end the mode";
    }
}

// resume_from_restoration is the ONLY exit, and it does BOTH things: clears
// the mode and re-bases the width by Eq. (13) (never Eq. (9)).
TEST(FunnelFullStep, ResumeFromRestorationClearsTheModeAndRebases) {
    FunnelStrategy s = funnel_at_1000();
    s.begin_full_step();
    ASSERT_TRUE(s.in_full_step());

    s.resume_from_restoration(200.0);
    EXPECT_FALSE(s.in_full_step());
    // Eq. (13): (1 - 0.5) * 200 + 0.5 * 1000 = 600.
    EXPECT_DOUBLE_EQ(s.width(), 600.0);
    // And Algorithm 2 is back in charge: a trial outside the (re-based) funnel
    // is rejected again.
    EXPECT_EQ(s.judge(ctx(10.0, 10.0, 500.0, 5000.0, 0.0)), StepVerdict::kReject);
}

// The mode is a state of a STARTED solve: arming it before reset() would leave
// its eventual exit with no width to re-base, so it throws instead (T5/T6 --
// the diagnostic lives only in the exception).
TEST(FunnelFullStep, ArmingBeforeResetThrows) {
    FunnelStrategy s;
    EXPECT_THROW(s.begin_full_step(), std::logic_error);
    EXPECT_FALSE(s.in_full_step());
}

// =============================================================================
// FunnelMeasure — h(x) as the funnel consumes it (sqp_driver.h).
// =============================================================================

// KLV Sec. 2.4.1 defines h(x) = ‖c(x)‖₁ for an NCO whose only inequalities are
// bounds that "are always feasible throughout SQP iterations". This engine's NLP
// carries general inequalities cI(x) <= 0, so the ℓ1 violation generalizes to
// ‖cE‖₁ + Σⱼ max(0, cIⱼ) and reduces to the paper's h when mi() == 0.
TEST(FunnelMeasure, IsTheL1ConstraintViolation) {
    NlpEval ev;
    ev.ce = Vec(3);
    ev.ce << 1.0, -2.0, 0.5; // ‖cE‖₁ = 3.5
    ev.ci = Vec(4);
    ev.ci << 3.0, -1.0, 0.0, 0.25; // Σ max(0, cI) = 3.25 (a satisfied row adds nothing)

    EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), 6.75);
}

TEST(FunnelMeasure, IsZeroAtAFeasiblePointAndOnEmptyBlocks) {
    NlpEval ev;
    ev.ce = Vec(2);
    ev.ce << 0.0, 0.0;
    ev.ci = Vec(2);
    ev.ci << -1.0, -1.0e9;
    EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), 0.0);

    NlpEval empty; // an unconstrained model: h ≡ 0
    empty.ce = Vec(0);
    empty.ci = Vec(0);
    EXPECT_DOUBLE_EQ(constraint_violation_l1(empty), 0.0);
}

TEST(FunnelMeasure, ReducesToTheL1NormWhenThereAreNoInequalities) {
    NlpEval ev;
    ev.ce = Vec(3);
    ev.ce << -4.0, 1.0, 2.0;
    ev.ci = Vec(0);
    EXPECT_DOUBLE_EQ(constraint_violation_l1(ev), ev.ce.lpNorm<1>());
}

// A non-finite constraint value must reach the funnel, which rejects the trial
// (FunnelInvariant.NonFiniteTrialIsRejected). std::max(0.0, NaN) == 0.0 would
// have dropped it on the inequality path, so both blocks are checked.
TEST(FunnelMeasure, PropagatesNonFiniteConstraintValues) {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();

    for (const double bad : {nan, inf}) {
        NlpEval eq;
        eq.ce = Vec(1);
        eq.ce << bad;
        eq.ci = Vec(0);
        EXPECT_FALSE(std::isfinite(constraint_violation_l1(eq)));

        NlpEval ineq;
        ineq.ce = Vec(0);
        ineq.ci = Vec(2);
        ineq.ci << -1.0, bad;
        EXPECT_FALSE(std::isfinite(constraint_violation_l1(ineq)));
    }
}
