// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_counters.cpp -- M6 W1 task 2: the IP-PMM tier's counters surface. Pins default
// construction, the fold rule for every field class (sum / max / min / overwrite), and the
// five-way escape-census invariant's reusable helper. See `.superpowers/w1-t2-report.md`.

#include <limits>

#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>
#include <hven/drivers/sqp_driver.h>

#include "support/ipqp_test_support.h"

namespace hven::solvers {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

// ===========================================================================
// ZERO-INIT.
// ===========================================================================

TEST(IpqpCounters, DefaultConstructedIsZeroInitialized) {
    IpqpCounters c;
    EXPECT_EQ(c.ipqp_iters, 0);
    EXPECT_EQ(c.ipqp_factorizations, 0);
    EXPECT_EQ(c.ipqp_symbolic_analyses, 0);
    EXPECT_EQ(c.ipqp_solves, 0);
    EXPECT_EQ(c.ipqp_pattern_verifies, 0);
    EXPECT_EQ(c.ipqp_rho_demanded_max, 0.0);
    EXPECT_EQ(c.ipqp_rho_demanded_last, 0.0);
    EXPECT_EQ(c.ipqp_inertia_retries, 0);
    EXPECT_EQ(c.ipqp_iters_at_elevated_rho, 0);
    EXPECT_EQ(c.ipqp_ladder_reclimbs, 0);
    EXPECT_EQ(c.ipqp_iters_ladder_armed_no_advance, 0);
    EXPECT_EQ(c.ipqp_pivot_reroute_primal, 0);
    EXPECT_EQ(c.ipqp_pivot_reroute_dual_fallback, 0);
    EXPECT_EQ(c.ipqp_final_inertia_read, 0);
    EXPECT_EQ(c.ipqp_reg_decreases, 0);
    EXPECT_EQ(c.ipqp_reg_increases, 0);
    EXPECT_EQ(c.ipqp_prox_center_updates, 0);
    EXPECT_EQ(c.ipqp_restart_repairs, 0);
    EXPECT_EQ(c.ipqp_restart_shift_max, 0.0);
    EXPECT_EQ(c.ipqp_mu_adopted, 0);
    EXPECT_EQ(c.ipqp_warm_restart_abandoned, 0);
    EXPECT_EQ(c.ipqp_declined_pinned, 0);
    EXPECT_EQ(c.ipqp_tier_retired_after, 0);
    EXPECT_EQ(c.ipqp_face_uncertain, 0);
    EXPECT_EQ(c.ipqp_refine_accepted, 0);
    EXPECT_EQ(c.ipqp_refine_refused, 0);
    EXPECT_EQ(c.ipqp_to_ssn, 0);
    EXPECT_EQ(c.ipqp_to_walk, 0);
    EXPECT_EQ(c.ipqp_escapes, 0);
    EXPECT_EQ(c.ipqp_escape_budget, 0);
    EXPECT_EQ(c.ipqp_escape_stall, 0);
    EXPECT_EQ(c.ipqp_escape_indefinite, 0);
    EXPECT_EQ(c.ipqp_escape_numerical, 0);
    EXPECT_EQ(c.ipqp_escape_infeasible_suspect, 0);
    // NOT zero: the min-fold identity for a (0, 1]-valued quantity is
    // +infinity, not 0.0 -- see the field's own doc comment.
    EXPECT_EQ(c.ipqp_alpha_p_min, kInf);
    EXPECT_EQ(c.ipqp_alpha_d_min, kInf);
}

TEST(IpqpCounters, SqpCountersCarriesAZeroInitializedIpqpFieldBesideSsn) {
    SqpCounters c;
    EXPECT_EQ(c.ipqp.ipqp_iters, 0);
    EXPECT_EQ(c.ipqp.ipqp_alpha_p_min, kInf);
    // ssn is the sibling field this one was added beside; both start
    // zero-initialized regardless of qp_mode (structural zero, no dispatch
    // has run at all on a freshly constructed SqpCounters).
    EXPECT_EQ(c.ssn.ssn_iters, 0);
}

// ===========================================================================
// THE ESCAPE-CENSUS INVARIANT, as the reusable helper in tests/sqp/support/ipqp_test_support.h.
// ===========================================================================

TEST(IpqpCounters, EscapeCensusHelperAcceptsAConsistentCensus) {
    IpqpCounters c;
    c.ipqp_escape_budget = 2;
    c.ipqp_escape_stall = 1;
    c.ipqp_escape_indefinite = 0;
    c.ipqp_escape_numerical = 3;
    c.ipqp_escape_infeasible_suspect = 1;
    c.ipqp_escapes = 7; // 2+1+0+3+1
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(c));
}

TEST(IpqpCounters, EscapeCensusHelperAcceptsTheVacuousAllZeroCase) {
    IpqpCounters c; // default: every escape field 0, ipqp_escapes 0
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(c));
}

TEST(IpqpCounters, EscapeCensusHelperCatchesAViolation) {
    // MUTATION CHECK: a census that under-counts (a field an accounting bug dropped) must be
    // REJECTED, not silently accepted -- the pin that proves the helper is falsifiable, per the
    // derivative_check.h precedent this file follows.
    IpqpCounters c;
    c.ipqp_escape_budget = 2;
    c.ipqp_escape_stall = 1;
    c.ipqp_escapes = 4; // 2+1 == 3, not 4: the gap a dropped increment leaves
    EXPECT_FALSE(test_support::assert_ipqp_escape_census_sums(c));
}

// ===========================================================================
// accumulate_ipqp_counters: THE FOLD RULE, ONE CLASS PER PIN.
// ===========================================================================

TEST(AccumulateIpqpCounters, RhoDemandedMaxFoldsByMaxAcrossSubproblems) {
    // Three subproblems folded small, large, medium. MUTATION CHECK: an ignore-one bug leaves
    // total at its 0.0 default (fails the first step); an assign-one (overwrite) bug leaves it
    // at the LAST value 5.0, not the true max 7.0.
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_rho_demanded_max = 3.0;
    IpqpCounters p2;
    p2.ipqp_rho_demanded_max = 7.0;
    IpqpCounters p3;
    p3.ipqp_rho_demanded_max = 5.0;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_rho_demanded_max, 3.0);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_rho_demanded_max, 7.0);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_rho_demanded_max, 7.0) << "the true max, not the last-folded value";
}

TEST(AccumulateIpqpCounters, RestartShiftMaxFoldsByMaxAcrossSubproblems) {
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_restart_shift_max = 0.02;
    IpqpCounters p2;
    p2.ipqp_restart_shift_max = 0.09;
    IpqpCounters p3;
    p3.ipqp_restart_shift_max = 0.04;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_restart_shift_max, 0.02);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_restart_shift_max, 0.09);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_restart_shift_max, 0.09) << "the true max, not the last-folded value";
}

TEST(AccumulateIpqpCounters, TierRetiredAfterFoldsByMaxNotSumOrOverwrite) {
    // Fix round 1 (Codex co-review I1): ipqp_tier_retired_after is a once-per-solve MAJOR INDEX,
    // so it folds by MAX -- not sum, not overwrite. Folding 4 then 7 rules out sum (11 is an
    // impossible major); folding a 0 third rules out overwrite. See w1-t2-report.md I1.
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_tier_retired_after = 4;
    IpqpCounters p2;
    p2.ipqp_tier_retired_after = 7;
    IpqpCounters p3;
    p3.ipqp_tier_retired_after = 0; // a later subproblem where the tier was never retired

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_tier_retired_after, 4);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_tier_retired_after, 7) << "the true max; sum would give the impossible 11";
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_tier_retired_after, 7)
        << "still 7: folding a later 0 must not overwrite the true retirement major";
}

TEST(AccumulateIpqpCounters, AlphaPMinFoldsByMinAcrossSubproblems) {
    // MUTATION CHECK: an "ignore one" bug would leave total at +infinity
    // forever; an "assign one" bug would leave total at the LAST value,
    // 0.3, not the true min, 0.05.
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_alpha_p_min = 0.5;
    IpqpCounters p2;
    p2.ipqp_alpha_p_min = 0.05;
    IpqpCounters p3;
    p3.ipqp_alpha_p_min = 0.3;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_alpha_p_min, 0.5);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_alpha_p_min, 0.05);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_alpha_p_min, 0.05) << "the true min, not the last-folded value";
}

TEST(AccumulateIpqpCounters, AlphaDMinFoldsByMinAcrossSubproblems) {
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_alpha_d_min = 0.6;
    IpqpCounters p2;
    p2.ipqp_alpha_d_min = 0.03;
    IpqpCounters p3;
    p3.ipqp_alpha_d_min = 0.2;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_alpha_d_min, 0.6);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_alpha_d_min, 0.03);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_alpha_d_min, 0.03) << "the true min, not the last-folded value";
}

TEST(AccumulateIpqpCounters, RhoDemandedLastIsOverwrittenByTheMostRecentSubproblem) {
    // MUTATION CHECK: values chosen so overwrite (2.0), max-fold (9.0) and
    // sum (16.0) all disagree -- only the true "latest wins" rule matches.
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_rho_demanded_last = 5.0;
    IpqpCounters p2;
    p2.ipqp_rho_demanded_last = 9.0;
    IpqpCounters p3;
    p3.ipqp_rho_demanded_last = 2.0;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_rho_demanded_last, 5.0);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_rho_demanded_last, 9.0);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_rho_demanded_last, 2.0)
        << "overwritten by the most recent subproblem, not max-folded or summed";
}

TEST(AccumulateIpqpCounters, FinalInertiaReadIsOverwrittenByTheMostRecentSubproblem) {
    // Same shape as the rho_demanded_last pin above, on the categorical
    // 0/1/2 outcome field.
    IpqpCounters total;
    IpqpCounters p1;
    p1.ipqp_final_inertia_read = 0;
    IpqpCounters p2;
    p2.ipqp_final_inertia_read = 2;
    IpqpCounters p3;
    p3.ipqp_final_inertia_read = 1;

    accumulate_ipqp_counters(total, p1);
    EXPECT_EQ(total.ipqp_final_inertia_read, 0);
    accumulate_ipqp_counters(total, p2);
    EXPECT_EQ(total.ipqp_final_inertia_read, 2);
    accumulate_ipqp_counters(total, p3);
    EXPECT_EQ(total.ipqp_final_inertia_read, 1)
        << "overwritten by the most recent subproblem, not max-folded or summed";
}

TEST(AccumulateIpqpCounters, EveryOtherIndexFieldSumsAcrossSubproblems) {
    // Every field below gets a DISTINCT (a, b) pair, so a cross-field
    // aliasing bug (summing into the wrong member) is caught by a mismatch
    // rather than an accidental pass.
    IpqpCounters a;
    a.ipqp_iters = 3;
    a.ipqp_factorizations = 4;
    a.ipqp_symbolic_analyses = 1;
    a.ipqp_solves = 6;
    a.ipqp_pattern_verifies = 2;
    a.ipqp_inertia_retries = 5;
    a.ipqp_iters_at_elevated_rho = 8;
    a.ipqp_ladder_reclimbs = 1;
    a.ipqp_iters_ladder_armed_no_advance = 6;
    a.ipqp_pivot_reroute_primal = 3;
    a.ipqp_pivot_reroute_dual_fallback = 1;
    a.ipqp_reg_decreases = 9;
    a.ipqp_reg_increases = 2;
    a.ipqp_prox_center_updates = 11;
    a.ipqp_restart_repairs = 3;
    a.ipqp_mu_adopted = 1;
    a.ipqp_warm_restart_abandoned = 0;
    a.ipqp_declined_pinned = 7;
    // ipqp_tier_retired_after deliberately NOT set here: it is max-folded, not summed (fix round
    // 1), so TierRetiredAfterFoldsByMaxNotSumOrOverwrite above discriminates it rather than this
    // sum-only fixture.
    a.ipqp_face_uncertain = 12;
    a.ipqp_refine_accepted = 4;
    a.ipqp_refine_refused = 1;
    a.ipqp_to_ssn = 2;
    a.ipqp_to_walk = 1;
    a.ipqp_escapes = 3;
    a.ipqp_escape_budget = 1;
    a.ipqp_escape_stall = 1;
    a.ipqp_escape_indefinite = 0;
    a.ipqp_escape_numerical = 1;
    a.ipqp_escape_infeasible_suspect = 0;
    a.ipqp_read_kept_tight_sides = 2;
    a.ipqp_read_barrier_noise_sides = 1;

    IpqpCounters b;
    b.ipqp_iters = 10;
    b.ipqp_factorizations = 13;
    b.ipqp_symbolic_analyses = 0;
    b.ipqp_solves = 20;
    b.ipqp_pattern_verifies = 0;
    b.ipqp_inertia_retries = 2;
    b.ipqp_iters_at_elevated_rho = 1;
    b.ipqp_ladder_reclimbs = 4;
    b.ipqp_iters_ladder_armed_no_advance = 2;
    b.ipqp_pivot_reroute_primal = 5;
    b.ipqp_pivot_reroute_dual_fallback = 2;
    b.ipqp_reg_decreases = 1;
    b.ipqp_reg_increases = 6;
    b.ipqp_prox_center_updates = 5;
    b.ipqp_restart_repairs = 2;
    b.ipqp_mu_adopted = 0;
    b.ipqp_warm_restart_abandoned = 1;
    b.ipqp_declined_pinned = 2;
    b.ipqp_face_uncertain = 6;
    b.ipqp_refine_accepted = 3;
    b.ipqp_refine_refused = 0;
    b.ipqp_to_ssn = 1;
    b.ipqp_to_walk = 3;
    b.ipqp_escapes = 4;
    b.ipqp_escape_budget = 0;
    b.ipqp_escape_stall = 2;
    b.ipqp_escape_indefinite = 1;
    b.ipqp_escape_numerical = 0;
    b.ipqp_escape_infeasible_suspect = 1;
    b.ipqp_read_kept_tight_sides = 5;
    b.ipqp_read_barrier_noise_sides = 3;

    IpqpCounters total;
    accumulate_ipqp_counters(total, a);
    accumulate_ipqp_counters(total, b);

    EXPECT_EQ(total.ipqp_iters, 13);
    EXPECT_EQ(total.ipqp_factorizations, 17);
    EXPECT_EQ(total.ipqp_symbolic_analyses, 1);
    EXPECT_EQ(total.ipqp_solves, 26);
    EXPECT_EQ(total.ipqp_pattern_verifies, 2);
    EXPECT_EQ(total.ipqp_inertia_retries, 7);
    EXPECT_EQ(total.ipqp_iters_at_elevated_rho, 9);
    EXPECT_EQ(total.ipqp_ladder_reclimbs, 5);
    EXPECT_EQ(total.ipqp_iters_ladder_armed_no_advance, 8);
    EXPECT_EQ(total.ipqp_pivot_reroute_primal, 8);
    EXPECT_EQ(total.ipqp_pivot_reroute_dual_fallback, 3);
    EXPECT_EQ(total.ipqp_reg_decreases, 10);
    EXPECT_EQ(total.ipqp_reg_increases, 8);
    EXPECT_EQ(total.ipqp_prox_center_updates, 16);
    EXPECT_EQ(total.ipqp_restart_repairs, 5);
    EXPECT_EQ(total.ipqp_mu_adopted, 1);
    EXPECT_EQ(total.ipqp_warm_restart_abandoned, 1);
    EXPECT_EQ(total.ipqp_declined_pinned, 9);
    EXPECT_EQ(total.ipqp_face_uncertain, 18);
    EXPECT_EQ(total.ipqp_refine_accepted, 7);
    EXPECT_EQ(total.ipqp_refine_refused, 1);
    EXPECT_EQ(total.ipqp_to_ssn, 3);
    EXPECT_EQ(total.ipqp_to_walk, 4);
    EXPECT_EQ(total.ipqp_escapes, 7);
    EXPECT_EQ(total.ipqp_escape_budget, 1);
    EXPECT_EQ(total.ipqp_escape_stall, 3);
    EXPECT_EQ(total.ipqp_escape_indefinite, 1);
    EXPECT_EQ(total.ipqp_escape_numerical, 1);
    EXPECT_EQ(total.ipqp_escape_infeasible_suspect, 1);
    EXPECT_EQ(total.ipqp_read_kept_tight_sides, 7);
    EXPECT_EQ(total.ipqp_read_barrier_noise_sides, 4);

    // The census invariant holds on the folded total too, using the same
    // reusable helper live solves will call later.
    EXPECT_TRUE(test_support::assert_ipqp_escape_census_sums(total));
}

TEST(AccumulateIpqpCounters, TotalStartingFromANonzeroBaseFoldsCorrectly) {
    // A second call site (finish() folding a restoration sub-solve's IpqpCounters onto an
    // already-populated total, mirroring accumulate_ssn_counters' restoration fold) starts from
    // a NONZERO total, pinned separately so a fold that only works from zero cannot pass.
    IpqpCounters total;
    total.ipqp_iters = 100;
    total.ipqp_rho_demanded_max = 4.0;
    total.ipqp_alpha_p_min = 0.2;
    total.ipqp_rho_demanded_last = 1.0;

    IpqpCounters one;
    one.ipqp_iters = 7;
    one.ipqp_rho_demanded_max = 6.0; // above the running total: must raise it
    one.ipqp_alpha_p_min = 0.5;      // above the running total: must NOT lower it
    one.ipqp_rho_demanded_last = 8.0;

    accumulate_ipqp_counters(total, one);
    EXPECT_EQ(total.ipqp_iters, 107);
    EXPECT_EQ(total.ipqp_rho_demanded_max, 6.0);
    EXPECT_EQ(total.ipqp_alpha_p_min, 0.2) << "0.2 is still smaller than one's 0.5";
    EXPECT_EQ(total.ipqp_rho_demanded_last, 8.0);
}

} // namespace
} // namespace hven::solvers
