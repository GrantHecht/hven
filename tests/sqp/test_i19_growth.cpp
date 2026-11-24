// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_i19_growth.cpp -- M8 I19-2a: the trust-region growth and shrink levers at the helpers
// (include/hven/detail/sqp/globalization/trust_region.h): the growth decision with its refusal
// bitmask, the rejection memory and the rejection shrink. The levers' wiring into the driver is
// in test_i19_radius_cycle.cpp.

#include <cmath>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include <hven/detail/sqp/globalization/trust_region.h>

namespace hven {
namespace {

using detail::kTrGrowThreshold;
using detail::kTrRefusalCapPins;
using detail::kTrRefusalFunnelHeadroom;
using detail::kTrRefusalMemory;
using detail::kTrRefusalRatioCeiling;
using detail::kTrRefusalSoc;
using detail::kTrRefusalViolationRatio;
using detail::TrustRegionGrowthDecision;
using detail::TrustRegionGrowthInputs;
using detail::TrustRegionRejectionMemory;
using detail::TrustRegionShrink;

constexpr double kInf = std::numeric_limits<double>::infinity();

// A radius-bound f-type acceptance with objective ratio 1 at radius 1: grows under every rule.
TrustRegionGrowthInputs f_step() {
    TrustRegionGrowthInputs in;
    in.region_limited = true;
    in.radius_binding = true;
    in.pred_df = 1.0;
    in.actual_df = 1.0;
    in.h_old = 0.0;
    in.h_new = 0.0;
    in.h_trial = 0.0;
    in.h_lin = 0.0;
    in.grown_radius = 2.0;
    return in;
}

// The issue's iteration 5 (free TA / cap 1.0): an h-type acceptance with objective ratio 5.06
// whose l1 violation rose 0.210 -> 0.605; the QP's linearization would have closed it.
TrustRegionGrowthInputs issue_iteration_5() {
    TrustRegionGrowthInputs in = f_step();
    in.feasibility_step = true;
    in.pred_df = 4.83e-4;
    in.actual_df = 2.45e-3;
    in.h_old = 0.210;
    in.h_new = 0.605;
    in.h_trial = 0.605;
    in.h_lin = 0.0;
    return in;
}

TrustRegionGrowthDecision decide(const SqpAdvancedOptions &a, const TrustRegionGrowthInputs &in) {
    return detail::trust_region_growth_decision(a, in);
}

SqpAdvancedOptions with_rule(SqpTrustRegionGrowth g) {
    SqpAdvancedOptions a;
    a.trust_region_growth = g;
    return a;
}

// ---- G1: kStepTypeRatio -------------------------------------------------------------------------

TEST(I19GrowthRule, StepTypeRatioReadsTheViolationRatioOnAnHTypeStep) {
    const TrustRegionGrowthInputs in = issue_iteration_5();
    const TrustRegionGrowthDecision base = decide(SqpAdvancedOptions{}, in);
    EXPECT_TRUE(base.grows) << "kRatio grows on the objective ratio 5.06";
    EXPECT_EQ(base.refusal, 0);
    const TrustRegionGrowthDecision g1 =
        decide(with_rule(SqpTrustRegionGrowth::kStepTypeRatio), in);
    EXPECT_FALSE(g1.grows) << "the violation rose: (h_old - h_new) / (h_old - h_lin) < 0";
    EXPECT_EQ(g1.refusal, kTrRefusalViolationRatio);
}

TEST(I19GrowthRule, StepTypeRatioGrowsAnHTypeStepThatDeliversItsPredictedViolationDecrease) {
    TrustRegionGrowthInputs in = issue_iteration_5();
    in.actual_df = 0.0; // the objective ratio fails: kRatio refuses
    in.h_old = 1.0;
    in.h_lin = 0.5;   // predicted 0.5
    in.h_new = 0.625; // actual 0.375 = 0.75 * 0.5 exactly
    EXPECT_FALSE(decide(SqpAdvancedOptions{}, in).grows);
    const auto g1 = with_rule(SqpTrustRegionGrowth::kStepTypeRatio);
    const TrustRegionGrowthDecision d = decide(g1, in);
    EXPECT_TRUE(d.grows) << "a violation ratio of exactly 0.75 grows";
    EXPECT_EQ(d.refusal, 0) << "a growth the base rule did not grant is no refusal";
    in.h_new = 0.625 + 1e-9;
    EXPECT_FALSE(decide(g1, in).grows) << "just below 0.75 does not";
}

TEST(I19GrowthRule, StepTypeRatioKeepsTheObjectiveRatioOnAnFTypeStep) {
    const auto g1 = with_rule(SqpTrustRegionGrowth::kStepTypeRatio);
    TrustRegionGrowthInputs in = f_step();
    in.h_old = 1.0;
    in.h_new = 3.0; // the violation rose, but the step is f-type
    EXPECT_TRUE(decide(g1, in).grows);
    in.actual_df = kTrGrowThreshold - 1e-9;
    EXPECT_FALSE(decide(g1, in).grows);
    EXPECT_EQ(decide(g1, in).refusal, 0) << "the base rule refused: no lever is consulted";
}

TEST(I19GrowthRule, StepTypeRatioRefusesWithoutALinearization) {
    TrustRegionGrowthInputs in = issue_iteration_5();
    in.h_new = 0.0; // the violation closed
    in.h_lin = std::nullopt;
    const TrustRegionGrowthDecision d = decide(with_rule(SqpTrustRegionGrowth::kStepTypeRatio), in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalViolationRatio);
}

TEST(I19GrowthRule, StepTypeRatioRefusesANonPositivePredictedViolationDecrease) {
    const auto g1 = with_rule(SqpTrustRegionGrowth::kStepTypeRatio);
    TrustRegionGrowthInputs in = issue_iteration_5();
    in.h_new = 0.0;
    in.h_lin = in.h_old; // predicted decrease 0
    EXPECT_FALSE(decide(g1, in).grows);
    in.h_lin = in.h_old + 0.1; // negative
    EXPECT_FALSE(decide(g1, in).grows);
    // At the noise floor: a predicted decrease the violation's rounding cannot resolve.
    in.h_old = 1.0;
    in.h_lin = 1.0 - 10.0 * std::numeric_limits<double>::epsilon();
    in.h_new = 0.0;
    EXPECT_FALSE(decide(g1, in).grows);
}

// ---- G2: no growth after SOC --------------------------------------------------------------------

TEST(I19GrowthRule, NoGrowthAfterSocWhenTheLeverIsOff) {
    TrustRegionGrowthInputs in = f_step();
    in.soc_applied = true;
    EXPECT_TRUE(decide(SqpAdvancedOptions{}, in).grows) << "default: SOC acceptances grow";
    SqpAdvancedOptions a;
    a.trust_region_growth_after_soc = false;
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalSoc);
    in.soc_applied = false;
    EXPECT_TRUE(decide(a, in).grows) << "a direct acceptance is untouched";
}

// ---- G4: ratio ceiling --------------------------------------------------------------------------

TEST(I19GrowthRule, RatioCeilingRefusesARatioAboveItAndNotAtIt) {
    SqpAdvancedOptions a;
    a.trust_region_ratio_ceiling = 3.0;
    TrustRegionGrowthInputs in = f_step();
    in.actual_df = 3.0; // ratio exactly 3
    EXPECT_TRUE(decide(a, in).grows);
    in.actual_df = 3.0 + 1e-9;
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalRatioCeiling);
    EXPECT_TRUE(decide(SqpAdvancedOptions{}, in).grows) << "+inf is off";
}

TEST(I19GrowthRule, RatioCeilingLeavesARatioFreeGrantAlone) {
    SqpAdvancedOptions a = with_rule(SqpTrustRegionGrowth::kAnyAccepted);
    a.trust_region_ratio_ceiling = 3.0;
    TrustRegionGrowthInputs in = f_step();
    in.actual_df = 10.0;
    EXPECT_TRUE(decide(a, in).grows);
}

TEST(I19GrowthRule, RatioCeilingLeavesTheFeasibilityBranchOfRatioOrFeasibilityStepAlone) {
    SqpAdvancedOptions a = with_rule(SqpTrustRegionGrowth::kRatioOrFeasibilityStep);
    a.trust_region_ratio_ceiling = 3.0;
    TrustRegionGrowthInputs in = f_step();
    in.actual_df = 10.0; // the objective ratio grants too, and is above the ceiling
    in.feasibility_step = true;
    EXPECT_TRUE(decide(a, in).grows) << "the feasibility branch grants without a ratio";
    in.feasibility_step = false;
    EXPECT_FALSE(decide(a, in).grows) << "an f-type step is ratio-granted";
}

TEST(I19GrowthRule, RatioCeilingReadsTheViolationRatioUnderStepTypeRatio) {
    SqpAdvancedOptions a = with_rule(SqpTrustRegionGrowth::kStepTypeRatio);
    a.trust_region_ratio_ceiling = 3.0;
    TrustRegionGrowthInputs in = issue_iteration_5();
    in.actual_df = 0.5 * in.pred_df; // objective ratio 0.5: not the ratio that grants
    in.h_old = 1.0;
    in.h_lin = 0.9;  // predicted 0.1
    in.h_new = 0.55; // actual 0.45: violation ratio 4.5 > 3
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalRatioCeiling);
    in.h_new = 0.85;                 // violation ratio 1.5
    in.actual_df = 5.0 * in.pred_df; // objective ratio 5: not read on an h-type step
    EXPECT_TRUE(decide(a, in).grows);
}

// ---- G6: cap pins -------------------------------------------------------------------------------

TEST(I19GrowthRule, CapOnlyStepIsNotEligibleWhenCapPinsDoNotGrow) {
    TrustRegionGrowthInputs in = f_step();
    in.radius_binding = false; // region_limited stays: a cap pin held the step
    EXPECT_TRUE(decide(SqpAdvancedOptions{}, in).grows) << "default: the merged flag";
    SqpAdvancedOptions a;
    a.trust_region_growth_on_cap_pins = false;
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalCapPins);
}

TEST(I19GrowthRule, RadiusBoundStepStaysEligible) {
    SqpAdvancedOptions a;
    a.trust_region_growth_on_cap_pins = false;
    TrustRegionGrowthInputs in = f_step();
    EXPECT_TRUE(decide(a, in).grows) << "a radius-bound step, with or without cap pins beside";
}

// ---- G7: funnel headroom ------------------------------------------------------------------------

TEST(I19GrowthRule, FunnelHeadroomRefusesAGrowthWhosePredictedViolationLeavesTheWidth) {
    SqpAdvancedOptions a;
    a.trust_region_funnel_headroom = true;
    TrustRegionGrowthInputs in = f_step();
    in.width = 100.0;
    in.h_lin = 1.0;
    in.h_trial = 1.0 + 98.0 / 4.0; // 1 + 4 * 24.5 = 99 = 0.99 * 100: inside
    EXPECT_TRUE(decide(a, in).grows);
    in.h_trial += 1e-9;
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalFunnelHeadroom);
    EXPECT_TRUE(decide(SqpAdvancedOptions{}, in).grows) << "off by default";
}

TEST(I19GrowthRule, FunnelHeadroomReadsTheUncorrectedTrialAfterSoc) {
    SqpAdvancedOptions a;
    a.trust_region_funnel_headroom = true;
    TrustRegionGrowthInputs in = f_step();
    in.soc_applied = true;
    in.width = 100.0;
    in.h_lin = 0.0;
    in.h_new = 0.1;    // the SOC point
    in.h_trial = 30.0; // the uncorrected trial: 4 * 30 = 120 > 99
    EXPECT_FALSE(decide(a, in).grows);
    in.h_trial = 20.0; // 80 <= 99
    in.h_new = 30.0;   // the corrected point is not read
    EXPECT_TRUE(decide(a, in).grows);
}

TEST(I19GrowthRule, FunnelHeadroomWithoutAWidthIsInert) {
    SqpAdvancedOptions a;
    a.trust_region_funnel_headroom = true;
    TrustRegionGrowthInputs in = f_step();
    in.width = kInf;
    in.h_trial = 1e300;
    EXPECT_TRUE(decide(a, in).grows);
}

// ---- G3: rejection memory -----------------------------------------------------------------------

TEST(I19GrowthRule, MemoryRefusesAGrowthReachingTheRememberedRadius) {
    SqpAdvancedOptions a;
    a.trust_region_rejection_memory = 3;
    TrustRegionGrowthInputs in = f_step();
    in.rejection_cap = 2.0;
    in.grown_radius = 2.0; // 2 delta == cap: the canonical cycle
    const TrustRegionGrowthDecision d = decide(a, in);
    EXPECT_FALSE(d.grows);
    EXPECT_EQ(d.refusal, kTrRefusalMemory);
    in.grown_radius = 2.0 - 1e-12;
    EXPECT_TRUE(decide(a, in).grows);
    in.grown_radius = 2.0;
    EXPECT_TRUE(decide(SqpAdvancedOptions{}, in).grows) << "0 is off";
}

TEST(I19GrowthRule, MemoryRecordsOnlyARadiusThatCameFromAGrowth) {
    TrustRegionRejectionMemory m;
    EXPECT_EQ(m.cap(), kInf);
    m.record_rejection(4.0, /*radius_from_growth=*/false);
    EXPECT_EQ(m.cap(), kInf);
    m.record_rejection(4.0, true);
    EXPECT_EQ(m.cap(), 4.0);
    m.record_rejection(8.0, true);
    EXPECT_EQ(m.cap(), 4.0) << "the cap only falls";
    m.record_rejection(2.0, true);
    EXPECT_EQ(m.cap(), 2.0);
}

TEST(I19GrowthRule, MemoryClearsAfterNCleanAcceptancesAndTheNthCannotGrow) {
    SqpAdvancedOptions a;
    a.trust_region_rejection_memory = 3;
    TrustRegionRejectionMemory m;
    m.record_rejection(2.0, true);
    TrustRegionGrowthInputs in = f_step();
    in.grown_radius = 2.0;
    for (int k = 1; k <= 3; ++k) {
        in.rejection_cap = m.cap();
        EXPECT_FALSE(decide(a, in).grows) << "acceptance " << k;
        m.after_acceptance(a.trust_region_rejection_memory);
    }
    EXPECT_EQ(m.cap(), kInf);
    EXPECT_EQ(m.clean_acceptances(), 0);
    in.rejection_cap = m.cap();
    EXPECT_TRUE(decide(a, in).grows);
}

TEST(I19GrowthRule, ARejectionRestartsTheCleanCount) {
    TrustRegionRejectionMemory m;
    m.record_rejection(2.0, true);
    m.after_acceptance(3);
    m.after_acceptance(3);
    EXPECT_EQ(m.clean_acceptances(), 2);
    m.record_rejection(4.0, true);
    EXPECT_EQ(m.clean_acceptances(), 0);
    m.after_acceptance(3);
    m.after_acceptance(3);
    EXPECT_EQ(m.cap(), 2.0) << "two acceptances after the restart do not clear";
    m.record_rejection(4.0, false);
    EXPECT_EQ(m.clean_acceptances(), 2) << "an unrecorded rejection restarts nothing";
    m.after_acceptance(3);
    EXPECT_EQ(m.cap(), kInf);
}

TEST(I19GrowthRule, MemoryClearForgetsTheCapAndTheCount) {
    TrustRegionRejectionMemory m;
    m.record_rejection(2.0, true);
    m.after_acceptance(3);
    m.clear();
    EXPECT_EQ(m.cap(), kInf);
    EXPECT_EQ(m.clean_acceptances(), 0);
}

// ---- G5: shrink to the step ---------------------------------------------------------------------

TrustRegionShrink shrink(bool lever, bool radius_binding, bool failure_step, double delta,
                         double window, double step_inf, double floor = 1e-10,
                         double radius_max = 1e10) {
    return detail::trust_region_rejection_shrink(lever, radius_binding, failure_step, delta, window,
                                                 step_inf, floor, radius_max);
}

TEST(I19GrowthRule, ShrinkToStepHalvesTheStepWhenTheRadiusDidNotHoldIt) {
    const TrustRegionShrink s = shrink(true, false, false, 16.0, kInf, 3.0);
    EXPECT_EQ(s.radius, 1.5);
    EXPECT_TRUE(s.to_step);
    EXPECT_FALSE(s.hits_floor);
    // Never above half the effective window: a step that leaked past the window still halves it.
    EXPECT_EQ(shrink(true, false, false, 4.0, kInf, 5.0).radius, 2.0);
    EXPECT_EQ(shrink(true, false, false, 16.0, 2.0, 3.0).radius, 1.0)
        << "the effective window is min(delta, qp.trust_region_radius)";
    // At delta = +inf: half the step.
    EXPECT_EQ(shrink(true, false, false, kInf, kInf, 3.0).radius, 1.5);
}

TEST(I19GrowthRule, ShrinkToStepKeepsHalvingARadiusBoundStep) {
    const TrustRegionShrink s = shrink(true, true, false, 4.0, kInf, 0.999 * 4.0);
    EXPECT_EQ(s.radius, 2.0);
    EXPECT_FALSE(s.to_step);
}

TEST(I19GrowthRule, ShrinkToStepLeavesAFailedSubproblemsStepAlone) {
    const TrustRegionShrink s = shrink(true, false, true, 4.0, kInf, 1e-9);
    EXPECT_EQ(s.radius, 2.0);
    EXPECT_FALSE(s.to_step);
    EXPECT_FALSE(s.hits_floor);
}

TEST(I19GrowthRule, ShrinkToStepFloorTestReadsTheShrunkRadius) {
    // 0.5 * delta is above the floor; half the step is below it.
    const TrustRegionShrink s = shrink(true, false, false, 1.0, kInf, 1e-10, 1e-8);
    EXPECT_TRUE(s.to_step);
    EXPECT_TRUE(s.hits_floor);
    EXPECT_FALSE(shrink(false, false, false, 1.0, kInf, 1e-10, 1e-8).hits_floor);
}

TEST(I19GrowthRule, ShrinkOffIsTodaysArithmetic) {
    for (const double delta : {1e-10, 3e-10, 1.0, 7.5, 1e10, kInf}) {
        for (const bool binding : {false, true}) {
            const TrustRegionShrink s = shrink(false, binding, false, delta, kInf, 0.25 * delta);
            const double legacy = std::isfinite(delta) ? delta * detail::kTrShrinkFactor : 1e10;
            EXPECT_EQ(s.radius, legacy) << delta;
            EXPECT_EQ(s.hits_floor, std::isfinite(delta) && delta * detail::kTrShrinkFactor < 1e-10)
                << delta;
            EXPECT_FALSE(s.to_step);
        }
    }
}

// ---- default equivalence and attribution --------------------------------------------------------

TEST(I19GrowthRule, DefaultLeversEqualTodaysRuleOnAGrid) {
    const double vals[] = {
        -1.0, 0.0, 0.5, kTrGrowThreshold, 1.0, 4.0, std::numeric_limits<double>::quiet_NaN()};
    for (const auto rule :
         {SqpTrustRegionGrowth::kRatio, SqpTrustRegionGrowth::kRatioOrFeasibilityStep,
          SqpTrustRegionGrowth::kAnyAccepted}) {
        const SqpAdvancedOptions a = with_rule(rule);
        for (int bits = 0; bits < 64; ++bits) {
            for (const double pred : vals) {
                for (const double act : vals) {
                    TrustRegionGrowthInputs in;
                    in.failure_step = bits & 1;
                    in.radius_finite = bits & 2;
                    in.radius_binding = bits & 4;
                    in.region_limited = (bits & 8) || in.radius_binding;
                    in.feasibility_step = bits & 16;
                    in.soc_applied = bits & 32;
                    in.pred_df = pred;
                    in.actual_df = act;
                    in.h_old = 1.0;
                    in.h_new = 2.0;
                    in.h_trial = 1e6;
                    in.h_lin = std::nullopt;
                    in.width = 1.0;
                    in.grown_radius = 2.0;
                    in.rejection_cap = 1.0;
                    const bool today = detail::trust_region_grows(
                        rule, in.failure_step, in.region_limited, in.radius_finite, pred, act,
                        in.feasibility_step);
                    const TrustRegionGrowthDecision d = decide(a, in);
                    EXPECT_EQ(d.grows, today) << bits << " " << pred << " " << act;
                    EXPECT_EQ(d.refusal, 0);
                }
            }
        }
    }
}

TEST(I19GrowthRule, RefusalsAreReadAfterTheBaseRuleAndEveryRefusingLeverIsRecorded) {
    SqpAdvancedOptions a;
    a.trust_region_growth_after_soc = false;
    a.trust_region_growth_on_cap_pins = false;
    a.trust_region_rejection_memory = 3;
    a.trust_region_ratio_ceiling = 3.0;
    a.trust_region_funnel_headroom = true;
    TrustRegionGrowthInputs in = f_step();
    in.radius_binding = false;
    in.soc_applied = true;
    in.actual_df = 5.0;
    in.width = 1.0;
    in.h_trial = 10.0;
    in.rejection_cap = 2.0;
    const TrustRegionGrowthDecision all = decide(a, in);
    EXPECT_FALSE(all.grows);
    EXPECT_EQ(all.refusal, kTrRefusalCapPins | kTrRefusalSoc | kTrRefusalRatioCeiling |
                               kTrRefusalFunnelHeadroom | kTrRefusalMemory);
    // The base rule refuses first: a growth it never granted carries no lever's bit.
    in.actual_df = 0.1;
    const TrustRegionGrowthDecision none = decide(a, in);
    EXPECT_FALSE(none.grows);
    EXPECT_EQ(none.refusal, 0);
    // So does ineligibility and the guards.
    in.actual_df = 5.0;
    in.region_limited = false;
    EXPECT_EQ(decide(a, in).refusal, 0);
    in.region_limited = true;
    in.failure_step = true;
    EXPECT_EQ(decide(a, in).refusal, 0);
}

} // namespace
} // namespace hven
