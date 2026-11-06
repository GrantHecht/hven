// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

///////////////////////////////////////////////////////////////////////////////
// AcceptanceStrategy::retract_last_acceptance() (M8 B5 riders, R1).
//
// The interior-point engine's non-finite step rollback rejects a step AFTER the
// strategy accepted it. Whatever that accept registered must be given back, or
// the restored iterate is judged against state its own rejected successor
// wrote -- for the funnel, a width tightened for a step that was never kept,
// which can leave the current iterate outside its own funnel.
//
// Pinned per strategy: the retraction restores exactly the pre-accept state;
// it is armed only by an accept and disarmed by the next acceptance test, a
// reset() and a phase switch; and a disarmed retraction is a no-op.
///////////////////////////////////////////////////////////////////////////////

#include "progress_measures_test_utils.h"

#include "hven/detail/globalization/filter_acceptance.h"
#include "hven/detail/globalization/funnel_acceptance.h"
#include "hven/detail/globalization/modern_merit.h"

#include <gtest/gtest.h>

#include <limits>

namespace {

using hven::IpmMeritPenaltyRule;
using hven::detail::FilterAcceptance;
using hven::detail::FunnelAcceptance;
using hven::detail::ModernMeritAcceptance;
using TychoTest::pm;

// An H-type test (m_f = 0: the switching condition never holds).
bool RetractHType(hven::detail::AcceptanceStrategy &a, double theta_current, double theta_trial) {
    return a.is_iterate_acceptable(pm(theta_current), pm(theta_trial), pm(0.0), 1.0, 1.0);
}

TEST(AcceptanceRetraction, FunnelRetractRestoresTheWidthAnHTypeAcceptTightened) {
    FunnelAcceptance a;
    ASSERT_FALSE(RetractHType(a, 4.0, 10.0)); // primes tau = 6.0, rejects
    ASSERT_DOUBLE_EQ(a.funnel_width(), 6.0);
    ASSERT_TRUE(RetractHType(a, 4.0, 2.0));
    ASSERT_LT(a.funnel_width(), 6.0) << "the accept tightened the width";
    a.retract_last_acceptance();
    EXPECT_DOUBLE_EQ(a.funnel_width(), 6.0);
    // The current iterate (theta 4.0) is inside its own funnel again.
    EXPECT_LE(4.0, a.funnel_width());
    // Spent: a second retraction is a no-op.
    a.retract_last_acceptance();
    EXPECT_DOUBLE_EQ(a.funnel_width(), 6.0);
}

TEST(AcceptanceRetraction, FunnelRetractIsDisarmedByTheNextTestAndByReset) {
    FunnelAcceptance a;
    ASSERT_FALSE(RetractHType(a, 4.0, 10.0)); // tau = 6.0
    ASSERT_TRUE(RetractHType(a, 4.0, 2.0));
    const double after_accept = a.funnel_width();
    // A rejected test after the accept: the accept is no longer the last thing
    // the strategy saw, so there is nothing to retract.
    ASSERT_FALSE(RetractHType(a, 2.0, 100.0));
    a.retract_last_acceptance();
    EXPECT_DOUBLE_EQ(a.funnel_width(), after_accept);

    ASSERT_TRUE(RetractHType(a, 2.0, 1.0));
    a.reset();
    a.retract_last_acceptance();
    EXPECT_EQ(a.funnel_width(), std::numeric_limits<double>::infinity())
        << "reset() disarms: the retraction must not write a pre-reset width back";
}

TEST(AcceptanceRetraction, FunnelRetractIsDisarmedByAPhaseSwitch) {
    FunnelAcceptance a;
    ASSERT_FALSE(RetractHType(a, 4.0, 10.0)); // tau = 6.0
    a.notify_switch_to_feasibility(pm(4.0));
    ASSERT_FALSE(RetractHType(a, 4.0, 10.0)); // the feasibility width, tau = 6.0
    ASSERT_TRUE(RetractHType(a, 4.0, 2.0));
    a.notify_switch_to_optimality(pm(2.0));
    const double restored = a.funnel_width();
    a.retract_last_acceptance();
    EXPECT_DOUBLE_EQ(a.funnel_width(), restored)
        << "a feasibility-phase accept must not be retracted into the optimality width";
}

TEST(AcceptanceRetraction, FilterRetractRemovesTheEntryAnHTypeAcceptAugmented) {
    FilterAcceptance a;
    ASSERT_TRUE(RetractHType(a, 2.0, 1.0));
    ASSERT_EQ(a.filter_size(), 1u) << "an H-type accept augments with the current pair";
    a.retract_last_acceptance();
    EXPECT_EQ(a.filter_size(), 0u);
    a.retract_last_acceptance();
    EXPECT_EQ(a.filter_size(), 0u);

    // Kept entries survive a retraction of a LATER accept.
    ASSERT_TRUE(RetractHType(a, 2.0, 1.0));
    ASSERT_TRUE(RetractHType(a, 1.0, 0.5));
    ASSERT_EQ(a.filter_size(), 2u);
    const auto first = a.filter_entry(0);
    a.retract_last_acceptance();
    ASSERT_EQ(a.filter_size(), 1u);
    EXPECT_EQ(a.filter_entry(0), first);
}

TEST(AcceptanceRetraction, FilterRetractIsDisarmedByTheNextTest) {
    FilterAcceptance a;
    ASSERT_TRUE(RetractHType(a, 2.0, 1.0));
    ASSERT_FALSE(RetractHType(a, 2.0, 1.0e6)) << "above theta_max: a ceiling rejection";
    a.retract_last_acceptance();
    EXPECT_EQ(a.filter_size(), 1u);
}

TEST(AcceptanceRetraction, MeritRetractRestoresTheSmallestKnownInfeasibility) {
    ModernMeritAcceptance a(IpmMeritPenaltyRule::kSinglePenalty);
    // Armijo on the l1 merit with nu = 1: 2.0 - 1.0 >= eta * pred.
    ASSERT_TRUE(a.is_iterate_acceptable(pm(2.0), pm(1.0), pm(0.0, 0.0), 1.0, 1.0));
    ASSERT_DOUBLE_EQ(a.smallest_known_infeasibility(), 1.0);
    a.retract_last_acceptance();
    EXPECT_EQ(a.smallest_known_infeasibility(), std::numeric_limits<double>::infinity());

    ASSERT_TRUE(a.is_iterate_acceptable(pm(2.0), pm(1.0), pm(0.0, 0.0), 1.0, 1.0));
    a.reset();
    a.retract_last_acceptance();
    EXPECT_EQ(a.smallest_known_infeasibility(), std::numeric_limits<double>::infinity());
    ASSERT_TRUE(a.is_iterate_acceptable(pm(2.0), pm(1.0), pm(0.0, 0.0), 1.0, 1.0));
    ASSERT_FALSE(a.is_iterate_acceptable(pm(1.0), pm(5.0), pm(0.0, 0.0), 1.0, 1.0));
    a.retract_last_acceptance();
    EXPECT_DOUBLE_EQ(a.smallest_known_infeasibility(), 1.0)
        << "disarmed by the rejected test after the accept";
}

} // namespace
