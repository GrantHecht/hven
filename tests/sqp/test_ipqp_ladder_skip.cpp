// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_ipqp_ladder_skip.cpp -- M7 W2 A3's IPQP lever
// `IpqpOptions::ipqp_ladder_skip_after` (formerly `detail::kIpqpLadderSkipAfter`) and its fire
// counter `ipqp_ladder_zero_trial_skips`. Design:
// docs/notes/2026-10-m7-w2-a3-ipqp-levers-design.md.

#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/ipqp_engine.h>
#include <hven/drivers/sqp_solver_types.h>

#include "support/indefinite_fixtures.h"

namespace hven::solvers {
namespace {

constexpr Index kNever = 1000000; // above any attempt's iteration count (hard cap 60)

QpOptions tight_opts() {
    QpOptions o;
    o.tr_radius = std::numeric_limits<double>::infinity();
    return o;
}

IpqpOptions with_skip(Index k) {
    IpqpOptions o;
    o.ipqp_ladder_skip_after = k;
    return o;
}

IpqpResult solve(const QpProblem &qp, const IpqpOptions &o) {
    IpqpEngine tier(tight_opts());
    return tier.solve(qp, nullptr, o, SolveOverrides{});
}

struct Fixture {
    const char *name;
    QpProblem qp;
};

/// The seven T4b gate-9 fixtures -- the population `--ladder` measures.
std::vector<Fixture> ladder_fixtures() {
    return {
        {"hs_indefinite_equality", test_support::indefinite_equality_qp()},
        {"hs_indefinite_equality_and_row", test_support::indefinite_equality_and_row_qp()},
        {"hs_two_negative_eigenvalue_row", test_support::two_negative_eigenvalue_row_qp()},
        {"indefinite_box_family_c0", test_support::indefinite_box_qp(0.0)},
        {"indefinite_box_family_c1", test_support::indefinite_box_qp(1.0)},
        {"equality_null_space", test_support::equality_null_space_indefinite_qp(-2.0)},
        {"saddle", test_support::saddle_qp()},
    };
}

/// A strictly convex box QP: the ladder never arms.
QpProblem convex_box_qp() {
    QpProblem qp = test_support::indefinite_box_qp(0.5);
    qp.H = test_support::fixture_sparse(2, 2, {{0, 0, 1.0}, {1, 1, 2.0}});
    return qp;
}

// ---------------------------------------------------------------------------
// The A0 surface: default, validation, dump
// ---------------------------------------------------------------------------

TEST(IpqpLadderSkipOptions, DefaultIsThree) { EXPECT_EQ(IpqpOptions{}.ipqp_ladder_skip_after, 3); }

TEST(IpqpLadderSkipOptions, ValidateRefusesANegativeCountByName) {
    SqpOptions o;
    for (const Index ok : {Index{0}, Index{1}, Index{3}, kNever}) {
        o.ipqp.ipqp_ladder_skip_after = ok;
        EXPECT_NO_THROW(validate(o)) << ok;
    }
    for (const Index bad : {Index{-1}, std::numeric_limits<Index>::min()}) {
        o.ipqp.ipqp_ladder_skip_after = bad;
        try {
            validate(o);
            ADD_FAILURE() << "validate accepted ipqp_ladder_skip_after " << bad;
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("ipqp.ipqp_ladder_skip_after"), std::string::npos)
                << e.what();
        }
    }
    // The tier validates through the same owner.
    EXPECT_THROW(solve(test_support::saddle_qp(), with_skip(-1)), std::invalid_argument);
}

TEST(IpqpLadderSkipOptions, TheDumpCarriesTheLine) {
    SqpOptions o;
    std::ostringstream def;
    dump_options(o, def);
    EXPECT_NE(def.str().find("ipqp.ipqp_ladder_skip_after = 3\n"), std::string::npos);
    o.ipqp.ipqp_ladder_skip_after = 1;
    std::ostringstream one;
    dump_options(o, one);
    EXPECT_NE(one.str().find("ipqp.ipqp_ladder_skip_after = 1\n"), std::string::npos);
}

// ---------------------------------------------------------------------------
// The rule and its fire counter
// ---------------------------------------------------------------------------

// Structurally 0 on a convex subproblem: no memory ever exists.
TEST(IpqpLadderSkip, TheCounterIsZeroOnAConvexSubproblemAtEveryValue) {
    for (const Index k : {Index{0}, Index{1}, Index{3}, kNever}) {
        const IpqpResult r = solve(convex_box_qp(), with_skip(k));
        EXPECT_EQ(r.status, QpStatus::kOptimal) << k;
        EXPECT_EQ(r.counters.ipqp_iters_at_elevated_rho, 0) << k;
        EXPECT_EQ(r.counters.ipqp_ladder_zero_trial_skips, 0) << k;
    }
}

// "Never" offers IC-1's zero trial on every iteration: the counter is 0, and every modified
// iteration paid at least one refused factorization first.
TEST(IpqpLadderSkip, NeverSkippingCountsNothingAndPaysTheZeroTrialEveryModifiedIteration) {
    for (const Fixture &f : ladder_fixtures()) {
        SCOPED_TRACE(f.name);
        const IpqpResult r = solve(f.qp, with_skip(kNever));
        EXPECT_EQ(r.counters.ipqp_ladder_zero_trial_skips, 0);
        EXPECT_GE(r.counters.ipqp_inertia_retries, r.counters.ipqp_iters_at_elevated_rho);
    }
}

// The shipped value FIRES on the gate fixtures (G4's non-vacuity), and a skipped iteration is a
// modified one: the counter never exceeds `ipqp_iters_at_elevated_rho`.
TEST(IpqpLadderSkip, TheShippedValueFiresOnTheLadderFixtures) {
    Index fired = 0;
    for (const Fixture &f : ladder_fixtures()) {
        SCOPED_TRACE(f.name);
        const IpqpResult r = solve(f.qp, IpqpOptions{});
        EXPECT_LE(r.counters.ipqp_ladder_zero_trial_skips, r.counters.ipqp_iters_at_elevated_rho);
        fired += r.counters.ipqp_ladder_zero_trial_skips;
    }
    EXPECT_GT(fired, 0);
}

// THE LATCH (design page section 2): once fired the rule holds for the rest of the attempt, so on
// a fixture whose trajectory the value does not move before the licence -- 26 of 27 iterations
// modified at 1, 2 and 3 -- the count is the iterations after the threshold is first met, one
// fewer per step. Measured pins, identical in Debug and Release (MKL, Linux x86-64, the A3 round;
// gate 7's fixture is NOT used: its Debug and Release trajectories differ); a mis-timed licence
// (`>` for `>=`, a hard-coded 3) moves them.
TEST(IpqpLadderSkip, TheCountTracksTheThresholdOnTheTwoNegativeEigenvalueRow) {
    const QpProblem qp = test_support::two_negative_eigenvalue_row_qp();
    const IpqpResult r1 = solve(qp, with_skip(1));
    const IpqpResult r2 = solve(qp, with_skip(2));
    const IpqpResult r3 = solve(qp, with_skip(3));
#ifdef USE_ACCELERATE_SPARSE
    // Exact trajectory pins are MKL-scoped (T4b F5); the ordering is the backend-neutral part.
    RecordProperty("a3_skip_counts_accelerate", "UNOBSERVED -- the exact counts are MKL-only");
    EXPECT_GE(r1.counters.ipqp_ladder_zero_trial_skips, r3.counters.ipqp_ladder_zero_trial_skips);
    return;
#endif
    for (const IpqpResult *r : {&r1, &r2, &r3}) {
        ASSERT_EQ(r->status, QpStatus::kOptimal);
        ASSERT_EQ(r->counters.ipqp_iters, 27);
        ASSERT_EQ(r->counters.ipqp_iters_at_elevated_rho, 26);
    }
    EXPECT_EQ(r1.counters.ipqp_ladder_zero_trial_skips, 25);
    EXPECT_EQ(r2.counters.ipqp_ladder_zero_trial_skips, 24);
    EXPECT_EQ(r3.counters.ipqp_ladder_zero_trial_skips, 23);
    // Each skipped zero trial is one refused factorization not paid.
    EXPECT_EQ(r1.counters.ipqp_factorizations, 36);
    EXPECT_EQ(r2.counters.ipqp_factorizations, 37);
    EXPECT_EQ(r3.counters.ipqp_factorizations, 38);
}

// The default is the pre-A3 constant: an explicit 3 and a default-constructed IpqpOptions run
// the same solve.
TEST(IpqpLadderSkip, AnExplicitThreeIsTheDefault) {
    for (const Fixture &f : ladder_fixtures()) {
        SCOPED_TRACE(f.name);
        const IpqpResult a = solve(f.qp, IpqpOptions{});
        const IpqpResult b = solve(f.qp, with_skip(3));
        EXPECT_EQ(a.status, b.status);
        EXPECT_EQ(a.counters.ipqp_iters, b.counters.ipqp_iters);
        EXPECT_EQ(a.counters.ipqp_factorizations, b.counters.ipqp_factorizations);
        EXPECT_EQ(a.counters.ipqp_ladder_zero_trial_skips, b.counters.ipqp_ladder_zero_trial_skips);
        EXPECT_TRUE((a.x.array() == b.x.array()).all());
    }
}

} // namespace
} // namespace hven::solvers
