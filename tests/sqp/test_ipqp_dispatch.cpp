// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_ipqp_dispatch.cpp -- M6 W1 task 6: the driver's kIpm dispatch, the
// section 2.3 routing chain, and the tier's registration as the third producer
// of exported inequality face prices.
//
// WHAT IS PINNED HERE, and how each pin is kept from passing vacuously:
//
//   (A7)  STRUCTURAL INERTNESS AT THE DEFAULT. At kWalk and at kSsn no
//         IpqpEngine is constructed, no analysis runs and every ipqp_* counter
//         is at its DEFAULT -- zero for the counts, +inf for the two
//         min-folded alpha fields. Non-vacuity: the same model at kIpm moves
//         them.
//   (2.3) THE ROUTING TABLE, one pin per row, each on a fixture MEASURED to
//         take that row (the measurements are in this task's report). The two
//         arithmetic identities the table implies are asserted on EVERY solve
//         in this file through `assert_ipqp_routing_partition`, so a route
//         that started double-counting would fail everywhere rather than in
//         one place.
//   (6.1) RETIREMENT AND ITS RESET, at the DRIVER scale: after K consecutive
//         escapes the remaining majors consult nothing and charge nothing.
//         (`IpqpEscapeLadder`'s own three rules are unit-pinned in task 5's
//         test_ipqp_certification.cpp; what is new here is the driver
//         honouring them.)
//   (A9)  THE SYMBOLIC-ANALYSIS PIN, driver half: plan section 7 notes (a) and
//         (k) -- exactly one of {1 analyze, 1 verify} per TIER ENTRY, analyses
//         hoisted across majors. Mutation non-vacuity through the
//         `ipqp_hoist_symbolic` kill switch.
//   (R6)  THE EXPORT BOUNDARY WITH THREE PRODUCERS: a kIpm solve's prices
//         reach `SqpDriver::finish`'s single sign sweep like every other
//         mode's. (The sweep's own semantics stay pinned where they were, on
//         hand-built vectors in test_sqp_driver.cpp.)
//   Equivalence: kIpm and kWalk agree on the ANSWER across the HS battery --
//         the tier is a different route to the same optimum, not a different
//         problem.

#include <cmath>
#include <limits>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>

#include "support/hs_problems.h"
#include "support/ipqp_test_support.h"

namespace hven::solvers {
namespace {

using hven::solvers::test_support::assert_ipqp_escape_census_sums;
using hven::solvers::test_support::assert_ipqp_routing_partition;
using hven::solvers::test_support::hs_numbers;
using hven::solvers::test_support::HsProblem;
using hven::solvers::test_support::make_hs;

// How many subproblems the tier was CONSULTED on, for the routing-partition
// helper: plan section 7 note (k)'s discipline is exactly one of {1 analyze,
// 1 verify} per TIER ENTRY, so their sum IS the entry count. Derived at the
// call sites rather than inside the helper so the two invariants stay
// separately diagnosable -- if this relation ever breaks, A9's own pin is
// what fails first.
Index tier_entries(const IpqpCounters &c) {
    return c.ipqp_symbolic_analyses + c.ipqp_pattern_verifies;
}

SqpOptions ipm_options() {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    o.max_iter = 60;
    return o;
}

SqpOptions walk_options() {
    SqpOptions o;
    o.max_iter = 60;
    return o;
}

// EVERY ipqp_* FIELD, read as one object, so "structurally zero" is a
// statement about the struct rather than about the fields a test happened to
// name. Compared against a DEFAULT-CONSTRUCTED IpqpCounters rather than
// against literal zeros: the two alpha fields default to +infinity (a 0.0
// default would defeat their own min-fold), and hard-coding that here would
// duplicate a decision solver_counters.h already made.
::testing::AssertionResult every_ipqp_counter_is_at_its_default(const IpqpCounters &c) {
    // THE FIELD LIST BELOW IS HAND-WRITTEN, so it needs a guard that fails
    // when the struct grows: 32 `Index` fields and 5 `double` fields, all
    // 8 bytes, no padding (T4b: +`ipqp_iters_ladder_armed_no_advance`, and
    // `ipqp_rho_flaps` RENAMED not removed; T4b fix round 1:
    // +`ipqp_pivot_reroute_primal`, +`ipqp_pivot_reroute_dual_fallback`). A W2
    // field added without a line here would otherwise drop silently out of
    // A7's coverage -- which is the one assertion that says the shipped
    // default touches none of them.
    static_assert(sizeof(IpqpCounters) == 37 * 8,
                  "IpqpCounters changed size: add the new field to "
                  "every_ipqp_counter_is_at_its_default below (A7's coverage is this list) and "
                  "update this assertion.");
    const IpqpCounters d;
    std::vector<std::string> moved;
    auto check = [&](const char *name, double got, double want) {
        // Bitwise on purpose: +inf == +inf is true and that is the intent, but
        // a NaN that crept into a min-fold would compare unequal to itself and
        // must be reported, not silently accepted.
        if (!(got == want)) {
            moved.push_back(fmt::format("{} = {} (default {})", name, got, want));
        }
    };
    check("ipqp_iters", static_cast<double>(c.ipqp_iters), static_cast<double>(d.ipqp_iters));
    check("ipqp_factorizations", static_cast<double>(c.ipqp_factorizations),
          static_cast<double>(d.ipqp_factorizations));
    check("ipqp_symbolic_analyses", static_cast<double>(c.ipqp_symbolic_analyses),
          static_cast<double>(d.ipqp_symbolic_analyses));
    check("ipqp_solves", static_cast<double>(c.ipqp_solves), static_cast<double>(d.ipqp_solves));
    check("ipqp_pattern_verifies", static_cast<double>(c.ipqp_pattern_verifies),
          static_cast<double>(d.ipqp_pattern_verifies));
    check("ipqp_rho_demanded_max", c.ipqp_rho_demanded_max, d.ipqp_rho_demanded_max);
    check("ipqp_rho_demanded_last", c.ipqp_rho_demanded_last, d.ipqp_rho_demanded_last);
    check("ipqp_inertia_retries", static_cast<double>(c.ipqp_inertia_retries),
          static_cast<double>(d.ipqp_inertia_retries));
    check("ipqp_iters_at_elevated_rho", static_cast<double>(c.ipqp_iters_at_elevated_rho),
          static_cast<double>(d.ipqp_iters_at_elevated_rho));
    check("ipqp_ladder_reclimbs", static_cast<double>(c.ipqp_ladder_reclimbs),
          static_cast<double>(d.ipqp_ladder_reclimbs));
    check("ipqp_iters_ladder_armed_no_advance",
          static_cast<double>(c.ipqp_iters_ladder_armed_no_advance),
          static_cast<double>(d.ipqp_iters_ladder_armed_no_advance));
    check("ipqp_pivot_reroute_primal", static_cast<double>(c.ipqp_pivot_reroute_primal),
          static_cast<double>(d.ipqp_pivot_reroute_primal));
    check("ipqp_pivot_reroute_dual_fallback",
          static_cast<double>(c.ipqp_pivot_reroute_dual_fallback),
          static_cast<double>(d.ipqp_pivot_reroute_dual_fallback));
    check("ipqp_final_inertia_read", static_cast<double>(c.ipqp_final_inertia_read),
          static_cast<double>(d.ipqp_final_inertia_read));
    check("ipqp_reg_decreases", static_cast<double>(c.ipqp_reg_decreases),
          static_cast<double>(d.ipqp_reg_decreases));
    check("ipqp_reg_increases", static_cast<double>(c.ipqp_reg_increases),
          static_cast<double>(d.ipqp_reg_increases));
    check("ipqp_prox_center_updates", static_cast<double>(c.ipqp_prox_center_updates),
          static_cast<double>(d.ipqp_prox_center_updates));
    check("ipqp_restart_repairs", static_cast<double>(c.ipqp_restart_repairs),
          static_cast<double>(d.ipqp_restart_repairs));
    check("ipqp_restart_shift_max", c.ipqp_restart_shift_max, d.ipqp_restart_shift_max);
    check("ipqp_mu_adopted", static_cast<double>(c.ipqp_mu_adopted),
          static_cast<double>(d.ipqp_mu_adopted));
    check("ipqp_warm_restart_abandoned", static_cast<double>(c.ipqp_warm_restart_abandoned),
          static_cast<double>(d.ipqp_warm_restart_abandoned));
    check("ipqp_declined_pinned", static_cast<double>(c.ipqp_declined_pinned),
          static_cast<double>(d.ipqp_declined_pinned));
    check("ipqp_tier_retired_after", static_cast<double>(c.ipqp_tier_retired_after),
          static_cast<double>(d.ipqp_tier_retired_after));
    check("ipqp_face_uncertain", static_cast<double>(c.ipqp_face_uncertain),
          static_cast<double>(d.ipqp_face_uncertain));
    check("ipqp_refine_accepted", static_cast<double>(c.ipqp_refine_accepted),
          static_cast<double>(d.ipqp_refine_accepted));
    check("ipqp_refine_refused", static_cast<double>(c.ipqp_refine_refused),
          static_cast<double>(d.ipqp_refine_refused));
    check("ipqp_to_refine", static_cast<double>(c.ipqp_to_refine),
          static_cast<double>(d.ipqp_to_refine));
    check("ipqp_to_ssn", static_cast<double>(c.ipqp_to_ssn), static_cast<double>(d.ipqp_to_ssn));
    check("ipqp_to_walk", static_cast<double>(c.ipqp_to_walk), static_cast<double>(d.ipqp_to_walk));
    check("ipqp_escapes", static_cast<double>(c.ipqp_escapes), static_cast<double>(d.ipqp_escapes));
    check("ipqp_escape_budget", static_cast<double>(c.ipqp_escape_budget),
          static_cast<double>(d.ipqp_escape_budget));
    check("ipqp_escape_stall", static_cast<double>(c.ipqp_escape_stall),
          static_cast<double>(d.ipqp_escape_stall));
    check("ipqp_escape_indefinite", static_cast<double>(c.ipqp_escape_indefinite),
          static_cast<double>(d.ipqp_escape_indefinite));
    check("ipqp_escape_numerical", static_cast<double>(c.ipqp_escape_numerical),
          static_cast<double>(d.ipqp_escape_numerical));
    check("ipqp_escape_infeasible_suspect", static_cast<double>(c.ipqp_escape_infeasible_suspect),
          static_cast<double>(d.ipqp_escape_infeasible_suspect));
    check("ipqp_alpha_p_min", c.ipqp_alpha_p_min, d.ipqp_alpha_p_min);
    check("ipqp_alpha_d_min", c.ipqp_alpha_d_min, d.ipqp_alpha_d_min);
    if (!moved.empty()) {
        return ::testing::AssertionFailure() << "IpqpCounters is not at its default: "
                                             << fmt::format("{}", fmt::join(moved, "; "));
    }
    return ::testing::AssertionSuccess();
}

// A subproblem the tier DECLINES pre-solve, by construction rather than by
// luck: variable 0's declared bounds are EQUAL, so the effective box has a
// zero-width pair at index 0 whatever the radius is, which is exactly the
// domain gate's rule (IpqpBounds' zero-width note). The walk pins that
// variable and eliminates it, which is what the note says the walk does best.
class PinnedVariableModel : public NlpModel {
  public:
    // `pin` false widens variable 0's box and nothing else, so the SAME model
    // exercises the declining and the non-declining case -- the mutation the
    // decline pin needs to be non-vacuous.
    explicit PinnedVariableModel(bool pin) : pin_(pin) {}

    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * ((x(0) - 1.0) * (x(0) - 1.0) + (x(1) - 2.0) * (x(1) - 2.0));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << x(0) - 1.0, x(1) - 2.0;
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        // x0 + x1 - 3 <= 0, in the tree's own "ci(x) <= 0" convention.
        Vec c(1);
        c << x(0) + x(1) - 3.0;
        return c;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return pin_ ? pinned_lower() : free_lower(); }
    const Vec &upper() const override { return pin_ ? pinned_upper() : free_upper(); }
    Vec start_point() const override { return Vec::Constant(2, 0.25); }

  private:
    static const Vec &pinned_lower() {
        static const Vec v = (Vec(2) << 0.5, -5.0).finished();
        return v;
    }
    static const Vec &pinned_upper() {
        static const Vec v = (Vec(2) << 0.5, 5.0).finished();
        return v;
    }
    static const Vec &free_lower() {
        static const Vec v = (Vec(2) << -5.0, -5.0).finished();
        return v;
    }
    static const Vec &free_upper() {
        static const Vec v = (Vec(2) << 5.0, 5.0).finished();
        return v;
    }
    bool pin_;
};

// ===========================================================================
// A7 -- STRUCTURAL INERTNESS AT THE DEFAULT (spec section 8.3).
// ===========================================================================

TEST(IpqpDispatch, EveryIpqpCounterIsStructurallyZeroAtKWalkAndKSsn) {
    for (int number : {6, 26, 35}) {
        SCOPED_TRACE(fmt::format("HS{}", number));

        auto walk_p = make_hs(number);
        SqpDriver walk_driver(walk_options());
        const SqpSolution walk = walk_driver.solve(*walk_p.model);
        EXPECT_TRUE(every_ipqp_counter_is_at_its_default(walk.counters.ipqp))
            << "at kWalk the dispatch's kWalk arm is the only one entered: no IpqpEngine is "
               "constructed, so nothing can write these";

        auto ssn_p = make_hs(number);
        SqpOptions ssn_opts = walk_options();
        ssn_opts.qp_mode = QpMode::kSsn;
        SqpDriver ssn_driver(ssn_opts);
        const SqpSolution ssn = ssn_driver.solve(*ssn_p.model);
        EXPECT_TRUE(every_ipqp_counter_is_at_its_default(ssn.counters.ipqp))
            << "and kSsn is just as inert: the tier is not the SSN arm's fallback, the walk is";
    }

    // NON-VACUITY. Without this the two loops above pass on a build where the
    // kIpm arm does nothing either.
    auto ipm_p = make_hs(6);
    SqpDriver ipm_driver(ipm_options());
    const SqpSolution ipm = ipm_driver.solve(*ipm_p.model);
    EXPECT_FALSE(every_ipqp_counter_is_at_its_default(ipm.counters.ipqp))
        << "the same model at kIpm must move the counters, or the assertions above are about a "
           "tier that never runs in any mode";
    EXPECT_GT(ipm.counters.ipqp.ipqp_iters, 0);
}

// ===========================================================================
// THE MODE IS DISPATCHABLE (task 1's temporary refusal, removed).
//
// This test REPLACES tests/sqp/test_ipqp_options.cpp's
// KIpmIsNotYetDispatchableDeleteThisPinWhenTheRoutingChainLands, whose own
// name asked for its deletion when this landed.
// ===========================================================================

TEST(IpqpDispatch, KIpmValidatesAndSolves) {
    SqpOptions o;
    o.qp_mode = QpMode::kIpm;
    EXPECT_NO_THROW(validate_sqp_options(o))
        << "task 1's temporary \"not yet dispatchable\" refusal is gone";

    const HsProblem p = make_hs(6);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    EXPECT_EQ(s.status, SqpStatus::kOptimal);
    EXPECT_NEAR(s.f, p.f_star, 1e-6);
    EXPECT_GT(s.counters.ipqp.ipqp_iters, 0) << "and the tier really solved the subproblems";

    // The default is untouched by any of it.
    SqpOptions defaulted;
    EXPECT_EQ(defaulted.qp_mode, QpMode::kWalk);
}

// ===========================================================================
// THE SECTION 2.3 ROUTING TABLE, ROW BY ROW.
// ===========================================================================

// ROW 1 -- THE DOMAIN GATE DECLINES, PRE-SOLVE, TO THE WALK.
//
// A decline is NOT an escape: nothing is charged toward the K=3 tally and the
// tier never runs, so no iteration, factorization or analysis is paid either.
// The walk gets its ORDINARY seed, not the cold one an escape gets -- there is
// no failed iterate to discard.
TEST(IpqpDispatch, AZeroWidthEffectivePairDeclinesToTheWalkAndChargesNothing) {
    PinnedVariableModel pinned(true);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(pinned);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_EQ(s.status, SqpStatus::kOptimal) << "the walk answers a pinned variable exactly";
    EXPECT_GT(c.ipqp_declined_pinned, 0) << "variable 0's declared bounds are equal";
    EXPECT_EQ(c.ipqp_declined_pinned, s.counters.major_iters)
        << "every major declines: the pin is a property of the model, not of an iterate";
    EXPECT_EQ(c.ipqp_to_walk, c.ipqp_declined_pinned)
        << "a decline routes to the walk, and ipqp_to_walk's doc comment names it as its second "
           "contributor";
    EXPECT_EQ(c.ipqp_escapes, 0) << "A DECLINE IS NOT AN ESCAPE";
    EXPECT_EQ(c.ipqp_tier_retired_after, 0)
        << "so no number of declines can retire the tier, whatever K is";
    EXPECT_EQ(c.ipqp_iters, 0);
    EXPECT_EQ(c.ipqp_factorizations, 0);
    EXPECT_EQ(c.ipqp_symbolic_analyses, 0) << "the gate runs BEFORE the engine is entered at all";
    EXPECT_TRUE(assert_ipqp_routing_partition(c, tier_entries(c)));

    // NON-VACUITY: the same model with variable 0's box widened declines
    // nothing and runs the tier.
    PinnedVariableModel unpinned(false);
    SqpDriver free_driver(ipm_options());
    const SqpSolution t = free_driver.solve(unpinned);
    EXPECT_EQ(t.counters.ipqp.ipqp_declined_pinned, 0)
        << "the decline is the zero-width pair's doing, not this model's";
    EXPECT_GT(t.counters.ipqp.ipqp_iters, 0);
}

// ROWS 2-5 -- MEASURED COVERAGE OF THE WHOLE TABLE ACROSS THE HS BATTERY.
//
// One solve per problem, every solve asserted against both routing identities
// and the five-way census; and the battery as a whole asserted to have taken
// each row at least once, so no row is pinned only by an identity that holds
// vacuously at zero. WHICH problem takes which row is NOT pinned -- that is a
// property of the tier's numerics and this task's report records the measured
// table -- but that at least one does is.
TEST(IpqpDispatch, TheRoutingTableIsExercisedAndItsIdentitiesHoldOnEverySolve) {
    Index refine_accepted = 0;
    Index refine_refused = 0;
    Index to_ssn = 0;
    Index to_walk = 0;
    Index escapes = 0;
    Index retirements = 0;

    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));
        auto p = make_hs(number);
        SqpDriver driver(ipm_options());
        const SqpSolution s = driver.solve(*p.model);
        const IpqpCounters &c = s.counters.ipqp;

        EXPECT_EQ(s.status, SqpStatus::kOptimal)
            << "the routing chain's whole point: a subproblem the tier cannot finish is handed on, "
               "so the SOLVE still converges";
        // NOT COMPARED TO `f_star` HERE, deliberately: two HS members
        // (25, 33) are ones the WALK itself does not drive to the published
        // optimum from their shipped start point, so an f_star assertion in
        // this test would be asserting something about the battery rather
        // than about the routing. The comparison that belongs to this task is
        // against the WALK, and it is its own test below.
        EXPECT_TRUE(assert_ipqp_escape_census_sums(c));
        EXPECT_TRUE(assert_ipqp_routing_partition(c, tier_entries(c)));

        refine_accepted += c.ipqp_refine_accepted;
        refine_refused += c.ipqp_refine_refused;
        to_ssn += c.ipqp_to_ssn;
        to_walk += c.ipqp_to_walk;
        escapes += c.ipqp_escapes;
        retirements += (c.ipqp_tier_retired_after > 0) ? 1 : 0;
    }

    EXPECT_GT(refine_accepted, 0) << "row 2: a converged tier exit refined on its own face";
    EXPECT_GT(refine_refused, 0) << "row 3: a refusal";
    EXPECT_EQ(to_ssn, refine_refused)
        << "row 3's destination, with no kIndefinite exit on this battery to add to it";
    EXPECT_GT(escapes, 0) << "row 5: a genuine escape";
    EXPECT_EQ(to_walk, escapes) << "row 5's destination, with no decline on this battery";
    EXPECT_GT(retirements, 0) << "section 6.1 fired at least once";
}

// THE SECTION 6.3 EVIDENCE ARRIVES AT THE W2 HOOK, END TO END (fix round 3).
//
// The hook's SIGNATURE is pinned on hand-built inputs below; what this pins is
// that a real `kInfeasibleSuspect` exit's own evidence -- not a
// default-constructed block, and not one rebuilt at the call site -- is what
// reaches it. HS10 produces such an exit (measured), and the two headline
// scalars land on that major's history row.
TEST(IpqpDispatch, AnInfeasibleSuspectExitsEvidenceReachesTheW2Hook) {
    auto p = make_hs(10);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(c.ipqp_escape_infeasible_suspect, 0)
        << "the fixture must produce a section 6.3 escape, or nothing is tested";

    Index rows_with_evidence = 0;
    for (const SqpIterate &row : s.history) {
        if (row.ipqp_least_infeasible_primal > 0.0 || row.ipqp_farkas_corroborated) {
            ++rows_with_evidence;
            EXPECT_GT(row.ipqp_least_infeasible_primal, 0.0)
                << "the least-infeasible point's own primal residual is positive on a solve that "
                   "suspected infeasibility -- a zero here would be the default block";
        }
    }
    EXPECT_GT(rows_with_evidence, 0)
        << "the escaped major's evidence reached the hook and was recorded. A call site that "
           "substituted a default-constructed IpqpInfeasibilityEvidence would leave every row at "
           "0/false and fail here, which is the whole reason these two fields exist";
    EXPECT_LE(rows_with_evidence, c.ipqp_escapes)
        << "and only escaped majors carry it -- the hook is the escape branch's single entry";
}

// ROW 5 -- THE ESCAPED SUBPROBLEM'S ANSWER IS THE WALK'S, AND ITS COST IS
// STILL PAID.
//
// The two halves of "the iterate is discarded" that ARE observable from
// outside: the solve's answer no longer depends on the tier (it converges
// through the walk), and the tier's factorizations are CHARGED rather than
// lost -- the same "vanishing work" class the kSsn hand-off charges at its own
// site, and the reason `charge_ipqp_subproblem_cost` exists.
//
// WHAT IS NOT OBSERVABLE HERE, STATED RATHER THAN IMPLIED: that the hand-off
// walk is COLD rather than seeded. Both spellings reach the same optimum (the
// walk is exact), and the driver publishes no per-major seeding column to tell
// them apart, so the coldness rests on `certified_feasibility_fallback` being
// the escape branch's single entry and its body being the one-argument
// `engine_.solve(qp, overrides)`. This task's report records that as a
// coverage gap rather than claiming a pin it does not have.
TEST(IpqpDispatch, AnEscapedSubproblemIsAnsweredByTheWalkAndItsCostIsStillCharged) {
    // HS10: measured to escape on EVERY major of a cold solve, so the whole
    // solve is the escape route and no refinement cost is mixed in.
    //
    // THE ROW MOVED FROM HS24 TO HS10 AT T4b, and the reason is the change
    // rather than a fixture preference: HS24's subproblems used to escape
    // because the ladder froze on them (the pre-T4b monotone floor), and with
    // the freeze gone the tier now FINISHES every one of them -- six entries,
    // five refinements accepted, zero escapes. A test whose premise is "the
    // fixture must escape" has to move to a fixture that still does. HS10
    // escapes on all three of its entries (two section 6.3
    // infeasible-suspect, one numerical), routes all three to the walk and
    // none to SSN, which is exactly the shape this row asserts.
    auto p = make_hs(10);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(c.ipqp_escapes, 0) << "the fixture must escape, or nothing is tested";
    ASSERT_EQ(c.ipqp_refine_accepted, 0) << "and it must escape on every major, or the arithmetic "
                                            "below is about two routes rather than one";
    EXPECT_EQ(c.ipqp_to_walk, c.ipqp_escapes) << "every escape routed to the walk";
    EXPECT_EQ(c.ipqp_to_ssn, 0) << "and none of them to SSN -- no kIndefinite exit here";
    EXPECT_EQ(s.status, SqpStatus::kOptimal)
        << "the walk answers what the tier could not, which is what the last branch is for";
    EXPECT_GT(s.counters.factorizations, c.ipqp_factorizations)
        << "the escaped attempt's factorizations are CHARGED (they were spent), and the walk's own "
           "are charged on top -- a strict inequality is what says neither set was lost";
}

// ===========================================================================
// SECTION 6.1 -- RETIREMENT, AT THE DRIVER SCALE.
// ===========================================================================

// After retirement the remaining majors go to the default engine WITHOUT
// consulting the tier and WITHOUT charging anything. Read off the analysis
// bookkeeping, which counts TIER ENTRIES exactly (plan section 7 note k):
// entries stop at the retiring major while majors keep going.
TEST(IpqpDispatch, RetirementStopsEveryLaterMajorFromConsultingTheTier) {
    auto p = make_hs(10); // measured: three consecutive escapes, retires at major 3
    SqpOptions o = ipm_options();
    SqpDriver driver(o);
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(c.ipqp_tier_retired_after, 0) << "the fixture must retire, or nothing is tested";
    EXPECT_EQ(c.ipqp_escapes, o.ipqp.ipqp_retire_after)
        << "exactly K escapes: the (K+1)-th major never entered the tier, so it could not escape";
    const Index entries = c.ipqp_symbolic_analyses + c.ipqp_pattern_verifies;
    EXPECT_EQ(entries, c.ipqp_tier_retired_after)
        << "one entry per major up to and including the retiring one, and none after";
    EXPECT_LT(entries, s.counters.major_iters)
        << "the solve kept going on the default engine, which is what retirement means";
    EXPECT_EQ(s.status, SqpStatus::kOptimal);
}

// A SUCCESS RESETS THE TALLY, observed at the driver scale rather than on the
// ladder object (task 5 pins the object's own three rules).
//
// HS15 escapes TWICE over its seven majors. With K LOWERED TO 2 the tier
// retires if and only if those two escapes were CONSECUTIVE -- and it does
// NOT retire, which is only possible if a successful subproblem between them
// reset the tally. That is exactly section 6.1's "any success resets the
// count", read off a solve rather than off the counter that implements it.
TEST(IpqpDispatch, ASuccessfulSubproblemResetsTheConsecutiveEscapeTally) {
    auto p = make_hs(15);
    SqpOptions o = ipm_options();
    o.ipqp.ipqp_retire_after = 2;
    SqpDriver driver(o);
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GE(c.ipqp_escapes, 2) << "the fixture must escape at least K times, or nothing is "
                                    "tested -- a solve with one escape cannot show a reset";
    EXPECT_EQ(c.ipqp_tier_retired_after, 0)
        << "K = 2 escapes were reached in TOTAL but never CONSECUTIVELY: the success between them "
           "reset the tally (spec 6.1, and IpqpEscapeLadder's own second rule)";
    EXPECT_GT(c.ipqp_refine_accepted, 0) << "and there really was a success to do the resetting";
}

// ===========================================================================
// A9 (DRIVER HALF) -- THE SYMBOLIC-ANALYSIS PIN.
// ===========================================================================

// Plan section 7 notes (a) and (k), as amended: the discipline is "exactly one
// of {1 analyze (the entry that lays the pattern), 1 verify (a re-entry on a
// laid pattern)} per TIER ENTRY", and the analysis is hoisted across majors
// while the model's structure epoch holds. `compute()` analyses without
// verifying and `kAssumeAnalyzed` leaves the verify count alone, which is why
// the claim is the SUM rather than `pattern_verifies == analyses`.
TEST(IpqpDispatch, OneAnalysisPerSolveAndOneVerifyPerLaterTierEntry) {
    // HS26: measured 17 majors, every one of them a tier entry, none declined
    // and none retired -- the clean case the claim is written for.
    auto p = make_hs(26);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(s.counters.major_iters, 1) << "one major cannot show a hoist";
    ASSERT_EQ(c.ipqp_declined_pinned, 0);
    ASSERT_EQ(c.ipqp_tier_retired_after, 0);
    EXPECT_EQ(c.ipqp_symbolic_analyses, 1) << "ONE analysis for the whole solve (spec 4.1)";
    EXPECT_EQ(c.ipqp_pattern_verifies, s.counters.major_iters - 1)
        << "and one verify on every LATER entry -- the one-time O(nnz) payment per entry, never a "
           "second analysis";
    EXPECT_EQ(c.ipqp_symbolic_analyses + c.ipqp_pattern_verifies, s.counters.major_iters)
        << "exactly one of {analyze, verify} per tier entry";

    // MUTATION NON-VACUITY, through the kill switch the spec provides for
    // exactly this purpose: with hoisting off every entry re-analyses, so the
    // two numbers swap places.
    auto q = make_hs(26);
    SqpOptions off = ipm_options();
    off.ipqp.ipqp_hoist_symbolic = false;
    SqpDriver off_driver(off);
    const SqpSolution t = off_driver.solve(*q.model);
    EXPECT_EQ(t.counters.ipqp.ipqp_symbolic_analyses, t.counters.major_iters)
        << "ipqp_hoist_symbolic = false forces a fresh symbolic pass every entry";
    EXPECT_EQ(t.counters.ipqp.ipqp_pattern_verifies, 0)
        << "and an entry that ANALYSED does not also verify -- compute() does not verify";
    EXPECT_GT(t.counters.ipqp.ipqp_symbolic_analyses, c.ipqp_symbolic_analyses)
        << "the hoist is doing real work, not describing a build where every entry analysed anyway";
}

// THE HOIST KEY IS PER SQP SOLVE, so a SECOND solve on the same driver
// analyses again (settler ruling, fix round 1, C4). "One symbolic analysis per
// SQP solve" is spec 4.1's own executable claim and A9's; making the key a
// per-solve local is what makes it true BY CONSTRUCTION rather than by an
// epoch comparison two different aggregates could satisfy by coincidence.
//
// THE ENGINE'S CACHE WOULD PHYSICALLY ALLOW THE REUSE -- it holds the analysis
// on the instance -- and taking it is a real optimisation. It is deliberately
// NOT taken in W1: it needs an identity a numeric epoch cannot supply (the
// DeclarationKey stamp), and it is registered as a T7/W3 continuation item.
TEST(IpqpDispatch, EverySqpSolveAnalysesOnceOnItsOwnAccount) {
    auto p = make_hs(26);
    SqpDriver driver(ipm_options());
    const SqpSolution first = driver.solve(*p.model);
    ASSERT_EQ(first.counters.ipqp.ipqp_symbolic_analyses, 1);

    const SqpSolution second = driver.solve(*p.model);
    const IpqpCounters &c = second.counters.ipqp;
    ASSERT_GT(second.counters.major_iters, 0);
    EXPECT_EQ(c.ipqp_symbolic_analyses, 1)
        << "the key is a per-solve local, so this solve's FIRST tier entry analyses on its own "
           "account rather than inheriting the previous solve's";
    EXPECT_EQ(c.ipqp_pattern_verifies, second.counters.major_iters - 1)
        << "and every LATER entry of it verifies";
    EXPECT_EQ(c.ipqp_symbolic_analyses + c.ipqp_pattern_verifies, second.counters.major_iters)
        << "exactly one of {analyze, verify} per tier entry, in this solve as in the first";
}

// ===========================================================================
// R6 -- THE EXPORT BOUNDARY, WITH THE TIER AS THE THIRD PRODUCER.
// ===========================================================================

// WHAT THE TIER PRODUCES, STATED PRECISELY (fix round 1, CM2 -- the first
// round's narrative overclaimed). The tier's RAW barrier prices never reach
// `finish()`: every route replaces them. What the tier produces is the FACE --
// its section 2.3 ratio-rule classification -- and that face is what
// `refine_on_face` is handed under kIpm, so the tier decides which rows that
// producer prices and therefore which prices are exported. On a refusal the
// subproblem goes to the SSN warm grade and the prices become SSN's; on an
// escape they become the walk's. So the third-producer pin measures the
// CERTIFIED/DOWNGRADED refine-accepted path, which is the only one where the
// tier's own decision reaches the export.
//
// Either way the result reaches SqpDriver::finish's single sign sweep -- there
// is no second export boundary for a third producer to have missed, which is
// the whole content of the registration. The MUTATION PARTNER (a kIpm solve
// that really does produce a negative price for the sweep to repair) lives in
// tests/sqp/test_scale_smoke.cpp, at the F7 weight class where such prices
// actually occur.
TEST(IpqpDispatch, NoNegativeFacePriceEscapesUnderKIpmEither) {
    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));
        auto p = make_hs(number);
        SqpDriver driver(ipm_options());
        const SqpSolution s = driver.solve(*p.model);

        if (s.lambda_i.size() > 0) {
            EXPECT_GE(s.lambda_i.minCoeff(), 0.0)
                << "no negative price may escape in SqpSolution::lambda_i";
        }
        if (s.warm_start.lambda_i.size() > 0) {
            EXPECT_GE(s.warm_start.lambda_i.minCoeff(), 0.0)
                << "nor in WarmStart::lambda_i, the half that reaches the currency";
        }
        // NOTHING ON THIS BATTERY PRICES NEGATIVE under any kernel (the same
        // claim test_sqp_driver.cpp's clean-solve pin makes for the other
        // two), so the sweep must be idle here: a sweep that fired would be
        // repairing something this population does not produce.
        EXPECT_EQ(s.counters.ssn.ssn_sign_swept, 0);
        EXPECT_DOUBLE_EQ(s.counters.ssn.ssn_sign_sweep_max, 0.0);
    }
}

// THE TIER'S EXPORTED BOUND PRICES PRICE THE QP's OWN BOUNDS AND NOTHING ELSE.
//
// FOUND BY THIS TASK, AND IT IS NOT COSMETIC. Under a FINITE radius every
// variable has finite EFFECTIVE bounds, so the barrier carries a (zl, zu) pair
// at every index whatever the caller's box says. Exporting `zl - zu`
// unconditionally prices bounds the QP does not have, and `SsnEngine::solve`
// REFUSES such a start outright ("there is no row for that multiplier") -- so
// before the fix the section 2.3 item 4 route THREW on the first HS problem
// that took it. The residue at an absent bound is a trust-region dual, and TR
// duals are internal (qp_problem.h), so it is dropped rather than exported.
TEST(IpqpDispatch, TheSsnWarmGradeRouteIsReachableAndPricesOnlyRealBounds) {
    // HS3 takes the refusal route on three of its four majors (measured).
    auto p = make_hs(3);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    ASSERT_GT(s.counters.ipqp.ipqp_to_ssn, 0)
        << "the fixture must reach the SSN warm grade, or nothing is tested";
    EXPECT_EQ(s.status, SqpStatus::kOptimal);
    EXPECT_GT(s.counters.ssn.ssn_iters, 0)
        << "and the SSN tier really ran -- ipqp_to_ssn is a routing count, this is the work";
    EXPECT_NEAR(s.f, p.f_star, 1e-6);
}

// ===========================================================================
// THE FIX-ROUND-1 SETTLER RULINGS (plan section 7 note q).
// ===========================================================================

// An NLP whose QP subproblem carries T4's diag(2, -1) SADDLE Hessian: convex
// in x0, concave in x1, both variables inside a real box so the tier has an
// interior to start from. Used for the kIndefinite route.
class SaddleBoxModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return x(0) * x(0) - x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * x(0), -2.0 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * obj_scale;
        h.insert(1, 1) = -2.0 * obj_scale;
        h.makeCompressed();
        return h;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -2.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, 2.0);
        return u;
    }
    Vec start_point() const override { return Vec::Constant(2, 0.5); }
};

// ROW 4 END TO END -- the saddle-suspect route, through the driver, with a
// mutation partner (fix round 1, C7a).
//
// WHY THE LADDER CEILING IS LOWERED. `kIndefinite` means a reading WAS taken
// and DISAGREED (plan note h), which needs the monotone ladder to run out of
// room: at the shipped `ipqp_reg_max = 1e6` the ladder lifts `rho` until the
// inertia signature is right and the subproblem then stops on its BUDGET
// instead (that is the Q-O2 frozen-fixed-point behaviour T5 diagnosed, and it
// is why no HS member reaches this row). Capping the ceiling is the tier's own
// documented lever for exactly this: the ladder reaches `ipqp_reg_max` with
// the reading still wrong, which IS the escape's definition.
TEST(IpqpDispatch, ASaddleSuspectExitRoutesToTheSsnWarmGradeAndNotToTheWalk) {
    SaddleBoxModel model;
    SqpOptions o = ipm_options();
    o.ipqp.ipqp_reg_max = 1.0e-2;
    o.ipqp.ipqp_rho_init = 1.0e-4;
    o.ipqp.ipqp_delta_init = 1.0e-4;
    SqpDriver driver(o);
    const SqpSolution s = driver.solve(model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(c.ipqp_escape_indefinite, 0)
        << "the fixture must produce a saddle-suspect exit, or nothing is tested";
    EXPECT_EQ(c.ipqp_escapes, c.ipqp_escape_indefinite) << "and only that kind, on this fixture";
    // THE MUTATION PARTNER: redirecting kIndefinite to the walk with the other
    // escapes moves BOTH of these, in opposite directions.
    EXPECT_EQ(c.ipqp_to_ssn, c.ipqp_escape_indefinite)
        << "section 2.3 item 4: a saddle-suspect exit goes to the SSN warm grade DIRECTLY, with "
           "no refinement attempt -- SSN's bulk flip changes the whole implied active set at once, "
           "which is what a saddle-suspect face needs";
    EXPECT_EQ(c.ipqp_to_walk, 0) << "and NOT to the walk, which would only re-derive the face";
    EXPECT_EQ(c.ipqp_to_refine, c.ipqp_refine_accepted + c.ipqp_refine_refused);
    EXPECT_TRUE(assert_ipqp_routing_partition(c, tier_entries(c)));
    EXPECT_TRUE(assert_ipqp_escape_census_sums(c));
    // `ssn_iters` IS NOT ASSERTED HERE, and the omission is deliberate: on a
    // two-variable saddle the SSN kernel can reach its own exit without taking
    // a Newton step, so a zero there is a legitimate outcome of the route
    // rather than evidence the route was not taken. The route's own counters
    // above are what this fixture pins; that the SSN tier does real work on
    // this path is pinned on HS3 below.
    EXPECT_EQ(s.status, SqpStatus::kOptimal);
}

// T7 -- THE CROSS-MAJOR CARRY (spec 5.1 flow (b)) IS LIVE, AND IT PAYS. The
// instrument is the warm budget's documented extreme: at 0 every warm entry is
// killed before it can take a step, so the same solve runs the COLD trajectory
// on every major and the difference between the two runs is exactly what the
// carry bought.
TEST(IpqpDispatch, TheTierCarriesItsStateAcrossTheMajorsOfOneSolve) {
    auto warm_p = make_hs(77);
    SqpDriver warm_driver(ipm_options());
    const SqpSolution warm = warm_driver.solve(*warm_p.model);
    const IpqpCounters &wc = warm.counters.ipqp;

    auto cold_p = make_hs(77);
    SqpOptions cold_opts = ipm_options();
    cold_opts.ipqp.ipqp_warm_iter_budget = 0;
    SqpDriver cold_driver(cold_opts);
    const SqpSolution cold = cold_driver.solve(*cold_p.model);
    const IpqpCounters &cc = cold.counters.ipqp;

    ASSERT_EQ(warm.status, SqpStatus::kOptimal);
    ASSERT_EQ(cold.status, SqpStatus::kOptimal);
    ASSERT_GT(tier_entries(wc), 1) << "the fixture must enter the tier more than once";
    ASSERT_EQ(tier_entries(wc), tier_entries(cc)) << "same route, same number of tier entries";

    // EVERY ENTRY AFTER THE FIRST STARTED WARM -- the kill can only fire on a
    // warm one, and the only entries it cannot reach are the first (cold) and
    // any whose seed already met the target before the budget was tested.
    EXPECT_GT(cc.ipqp_warm_restart_abandoned, 0);
    EXPECT_LE(cc.ipqp_warm_restart_abandoned, tier_entries(cc) - 1);
    EXPECT_EQ(wc.ipqp_warm_restart_abandoned, 0) << "at the shipped budget nothing is abandoned";

    // AND THE CARRY IS WORTH HAVING: strictly fewer barrier iterations and
    // strictly fewer factorizations for the same answer.
    EXPECT_LT(wc.ipqp_iters, cc.ipqp_iters);
    EXPECT_LT(wc.ipqp_factorizations, cc.ipqp_factorizations);
}

// FIX ROUND 1, RULING R3: the carry is dropped after EVERY genuine escape,
// `kIndefinite` included -- a point the tier classified as a saddle suspect
// does not seed the next major. The instrument is the warm budget's 0 extreme,
// which fires the kill on every entry that started WARM: if the carry
// survived a saddle-suspect exit, later entries would be warm and the counter
// would move.
TEST(IpqpDispatch, AnIndefiniteEscapeDropsTheCarry) {
    SaddleBoxModel model;
    SqpOptions o = ipm_options();
    o.ipqp.ipqp_reg_max = 1.0e-2;
    o.ipqp.ipqp_rho_init = 1.0e-4;
    o.ipqp.ipqp_delta_init = 1.0e-4;
    o.ipqp.ipqp_warm_iter_budget = 0;
    SqpDriver driver(o);
    const SqpSolution s = driver.solve(model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_GT(c.ipqp_escape_indefinite, 0) << "the fixture must produce saddle-suspect exits";
    ASSERT_EQ(c.ipqp_escapes, c.ipqp_escape_indefinite) << "and only that kind";
    ASSERT_GT(tier_entries(c), 1) << "and must enter the tier more than once";
    EXPECT_EQ(c.ipqp_warm_restart_abandoned, 0)
        << "every entry after an indefinite escape started COLD, so no warm attempt existed to "
           "abandon";

    // NON-VACUITY: the same instrument on a fixture that does NOT escape sees
    // the carry survive and the kill fire.
    auto p = make_hs(77);
    SqpOptions k = ipm_options();
    k.ipqp.ipqp_warm_iter_budget = 0;
    SqpDriver hs(k);
    const SqpSolution t = hs.solve(*p.model);
    ASSERT_EQ(t.counters.ipqp.ipqp_escapes, 0);
    EXPECT_GT(t.counters.ipqp.ipqp_warm_restart_abandoned, 0);
}

// ... AND IT IS SCOPED TO ONE SQP SOLVE. The engine outlives the solve, so a
// second solve on the same driver must start its first major cold; otherwise
// two solves of one model would not be two solves of one model.
TEST(IpqpDispatch, TheCarryDoesNotLeakBetweenSolvesOnOneDriver) {
    auto p = make_hs(77);
    SqpOptions o = ipm_options();
    SqpDriver driver(o);
    const SqpSolution first = driver.solve(*p.model);
    const SqpSolution second = driver.solve(*p.model);
    ASSERT_EQ(first.status, SqpStatus::kOptimal);
    ASSERT_EQ(second.status, SqpStatus::kOptimal);
    EXPECT_EQ(second.counters.ipqp.ipqp_iters, first.counters.ipqp.ipqp_iters);
    EXPECT_EQ(second.counters.ipqp.ipqp_factorizations, first.counters.ipqp.ipqp_factorizations);
    EXPECT_EQ(second.counters.ipqp.ipqp_restart_repairs, first.counters.ipqp.ipqp_restart_repairs);
}

// RULING 1 (decision 2, REVERSED IN PART): a converged `kBudget` exit whose
// section 2.2 item 4 certification read the factorization budget refused is a
// SUCCESS for the section 6.1 ladder -- no K charge, and it RESETS the tally.
// The CENSUS still counts it as `ipqp_escape_budget`: the two answer different
// questions, and this test is the one place both answers are read at once.
//
// THREE CONSECUTIVE OF THEM, which is what the ruling actually says, and the
// fixture is built so that "three" and "consecutive" are both readable off
// solve-level counters (fix round 3 -- the first spelling asserted only
// `>= 1`). HS77 at a factorization cap of 7, capped at THREE MAJORS: all three
// majors escape `kBudget`, all three route to the refinement, none routes to
// the walk. Since the solve has exactly three majors, "all three" IS
// "consecutive", with no per-major channel needed to say so.
//
// AND THE PREMISE IS DERIVED, NOT ASSUMED. `ipqp_final_inertia_read` is a
// per-subproblem STATUS that the fold overwrites, so it cannot be read per
// major from a solve total. It does not need to be: an ESCAPED exit reaches
// `ipqp_to_refine` through exactly one branch of `ipqp_exit_is_a_usable_step`,
// and that branch requires `kBudget` AND `read == 3` AND
// `certificate_downgraded` AND residuals that met the target. So
// `escape_budget == 3` together with `to_refine == 3` and `to_walk == 0` on a
// three-major solve says all three had `read == 3` -- there is no other way
// for those three numbers to hold at once. `refine_accepted == 3` is asserted
// beside it, so all three were accepted as the step.
TEST(IpqpDispatch, ThreeConsecutiveConvergedBudgetExitsDoNotRetireTheTier) {
    auto p = make_hs(77);
    SqpOptions o = ipm_options();
    o.max_iter = 3;
    // Tight enough that the item 4 read is refused on a converged solve, loose
    // enough that the solve still converges -- the row's whole premise.
    o.ipqp.ipqp_max_factorizations = 7;
    // T7: THE FIXTURE IS HELD COLD ON EVERY MAJOR, through the documented
    // extreme of the warm budget (0 = every warm restart is killed on its
    // first iteration, before it can take a step). This row is about the
    // section 6.1 ladder, not about warm-start quality; left warm, the carry
    // converges majors 2-3 inside the cap and the premise -- three consecutive
    // converged BUDGET exits -- disappears.
    o.ipqp.ipqp_warm_iter_budget = 0;
    SqpDriver driver(o);
    const SqpSolution s = driver.solve(*p.model);
    const IpqpCounters &c = s.counters.ipqp;

    ASSERT_EQ(s.counters.major_iters, 3) << "three majors, so 'all' and 'consecutive' coincide";
    ASSERT_EQ(c.ipqp_escapes, 3);
    ASSERT_EQ(c.ipqp_escape_budget, 3) << "and every one of them is a BUDGET escape";
    ASSERT_EQ(c.ipqp_to_refine, 3)
        << "each routed to the tier-3 refinement, which an escaped exit can only do through the "
           "converged-budget branch of the usability gate -- so each had read == 3, a downgraded "
           "certificate and residuals that met the target";
    ASSERT_EQ(c.ipqp_refine_accepted, 3) << "and each was ACCEPTED as the step";
    ASSERT_EQ(c.ipqp_to_walk, 0) << "none took the cold walk an iteration-cap budget exit takes";

    EXPECT_EQ(c.ipqp_tier_retired_after, 0)
        << "THREE CONSECUTIVE 2c EXITS DO NOT RETIRE THE TIER at the shipped K = 3: each is a "
           "ladder SUCCESS that RESETS the tally, so the tally never reaches 1, let alone K "
           "(settler ruling, fix round 1)";
    EXPECT_TRUE(assert_ipqp_escape_census_sums(c))
        << "the census is unchanged: it says what stopped the tier, not whether the tier is suited";
    EXPECT_TRUE(assert_ipqp_routing_partition(c, tier_entries(c)));

    // AND AT K = 1, where a single charge would fire immediately. This is the
    // sharper reading of "no charge": not merely "fewer than three", but none.
    auto q = make_hs(77);
    SqpOptions k1 = o;
    k1.ipqp.ipqp_retire_after = 1;
    SqpDriver k1_driver(k1);
    const SqpSolution t = k1_driver.solve(*q.model);
    ASSERT_EQ(t.counters.ipqp.ipqp_escape_budget, 3) << "the same three exits";
    ASSERT_EQ(t.counters.ipqp.ipqp_to_refine, 3);
    EXPECT_EQ(t.counters.ipqp.ipqp_tier_retired_after, 0)
        << "at ipqp_retire_after = 1 a single CHARGED escape retires the tier at the major it "
           "happened on; the tier is still live after three, which is only possible if none of "
           "them was charged";

    // THE MUTATION PARTNER, on the SAME problem: three consecutive GENUINE
    // budget escapes -- the ITERATION cap rather than the factorization cap,
    // so the solves do not converge and the exits are not 2c -- ARE charged
    // and DO retire the tier at major 3.
    auto r = make_hs(77);
    SqpOptions genuine = ipm_options();
    genuine.max_iter = 6;
    genuine.ipqp.ipqp_hard_iter_cap = 2; // stops mid-descent, every major
    SqpDriver genuine_driver(genuine);
    const SqpSolution g = genuine_driver.solve(*r.model);
    const IpqpCounters &gc = g.counters.ipqp;
    ASSERT_EQ(gc.ipqp_escape_budget, 3) << "three budget escapes here too";
    EXPECT_EQ(gc.ipqp_to_refine, 0) << "but NOT converged, so none reaches the refinement";
    EXPECT_EQ(gc.ipqp_to_walk, 3) << "each takes the cold walk instead";
    EXPECT_EQ(gc.ipqp_tier_retired_after, 3)
        << "and the third one retires the tier -- which is what says the K = 3 charge is live and "
           "the pin above is about the 2c exemption rather than about a ladder that never fires";
    EXPECT_TRUE(assert_ipqp_routing_partition(gc, tier_entries(gc)));
}

// RULING 2 (decision 3, REVERSED): a usable SSN warm-grade exit is refined on
// its own face, exactly as the kSsn arm refines every certifying SSN exit.
// PARITY WITH kSsn IS THE RULE, so the counters are the SSN tier's own pair.
TEST(IpqpDispatch, AUsableSsnWarmGradeExitIsRefinedOnItsOwnFace) {
    // HS3 routes to the SSN warm grade on three of its four majors (measured),
    // and every one of those exits is usable.
    auto p = make_hs(3);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);
    const SsnCounters &ssn = s.counters.ssn;

    ASSERT_GT(s.counters.ipqp.ipqp_to_ssn, 0)
        << "the fixture must reach the SSN warm grade, or nothing is tested";
    EXPECT_EQ(ssn.ssn_refinements + ssn.ssn_refine_refused, s.counters.ipqp.ipqp_to_ssn)
        << "EVERY usable SSN exit on this route is handed to tier 3 and reports exactly one of "
           "accepted/refused -- the kSsn arm's own discipline, applied to the same kernel reached "
           "through the kIpm chain (settler ruling, fix round 1)";
    EXPECT_GT(ssn.ssn_iters, 0) << "and the SSN tier really did the work";
    // THE GRADE IS (x, lambda) IN THE TIER'S WINDOW (settler ruling, fix
    // round 2), and this solve is its live mutation partner. The route calls
    // `assert_ssn_warm_grade_window` before every SSN hand-off, which THROWS
    // unless the start it is about to pass carries the tier's iterate AND the
    // tier's clamp-centred box centre -- so a build that dropped either half,
    // or that let `SsnEngine` centre the window on `start.x` the historical
    // way, fails HERE rather than silently gating tier 3 against the wrong
    // window. `SsnStart::box_center`'s own behaviour is pinned directly in
    // test_ssn_engine.cpp (honoured, disengaged-is-bit-identical, validated).
    EXPECT_EQ(s.status, SqpStatus::kOptimal)
        << "the warm-grade window guard did not fire on any of this fixture's hand-offs";
    // `ssn_refine_factorizations` is NOT asserted positive: `refine_on_face`
    // refuses an empty or rank-deficient face on its own PRE-SCREEN, before
    // anything is factorized, and all three of this fixture's refusals are
    // that kind. The charge site is the kSsn arm's line for line; what this
    // pin holds is that every usable exit reaches tier 3, which is the parity
    // the ruling asked for.
    EXPECT_EQ(s.status, SqpStatus::kOptimal);
}

// RULING 3 (decision 8, NARROWED): `adaptive_mu` is off only for the
// subproblems the TIER solves. A model whose every major DECLINES is solved
// entirely by the ordinary walk under kIpm, so it must be bit-identical to the
// same model at kWalk -- a lever the caller never touched may not be disabled
// by a mode selection whose tier never ran.
TEST(IpqpDispatch, ASolveTheTierNeverRunsIsTheWalkWithTheCallersOwnLevers) {
    SqpOptions walk_opts = walk_options();
    ASSERT_TRUE(walk_opts.adaptive_mu) << "the schedule is ON by default -- that is the premise";

    PinnedVariableModel walk_model(true);
    SqpDriver walk_driver(walk_opts);
    const SqpSolution walk = walk_driver.solve(walk_model);

    PinnedVariableModel ipm_model(true);
    SqpDriver ipm_driver(ipm_options());
    const SqpSolution ipm = ipm_driver.solve(ipm_model);

    ASSERT_GT(ipm.counters.ipqp.ipqp_declined_pinned, 0) << "every major declines, by construction";
    EXPECT_EQ(ipm.counters.ipqp.ipqp_declined_pinned, ipm.counters.major_iters);
    EXPECT_EQ(ipm.status, walk.status);
    EXPECT_EQ(ipm.counters.major_iters, walk.counters.major_iters)
        << "the same walk, run the same number of times";
    EXPECT_EQ(ipm.counters.qp_minor_iters, walk.counters.qp_minor_iters)
        << "AND WITH THE SAME LEVERS: a suppressed adaptive-mu schedule changes the walk's "
           "regularization and with it its minor count (settler ruling, fix round 1)";
    EXPECT_EQ(ipm.counters.factorizations, walk.counters.factorizations);
    EXPECT_DOUBLE_EQ(ipm.f, walk.f);
    ASSERT_EQ(ipm.x.size(), walk.x.size());
    EXPECT_EQ(ipm.x, walk.x) << "bit-identical, not merely close";
    ASSERT_FALSE(ipm.history.empty());
    ASSERT_EQ(ipm.history.size(), walk.history.size());
    for (std::size_t k = 0; k < ipm.history.size(); ++k) {
        EXPECT_DOUBLE_EQ(ipm.history[k].mu, walk.history[k].mu)
            << "row " << k
            << ": SqpIterate::mu reports what the kernel that solved the row used, "
               "and the kernel here is the walk in both modes";
    }
}

// RULING 4 (decision 7b, REVERSED): the SSN warm grade participates in the
// proximal carry, because `warm_start.h` scopes `prox_sigma` to "the maximum
// over any SSN subproblem" and one reached through this chain is one.
TEST(IpqpDispatch, TheSsnWarmGradeExportsTheProximalCarry) {
    // HS43 routes to the SSN warm grade once, and that subproblem's ladder
    // raises sigma (measured) -- which is exactly the evidence the carry
    // exists to transmit.
    auto p = make_hs(43);
    SqpDriver driver(ipm_options());
    const SqpSolution s = driver.solve(*p.model);

    ASSERT_GT(s.counters.ipqp.ipqp_to_ssn, 0)
        << "the fixture must reach the SSN warm grade, or nothing is tested";
    ASSERT_GT(s.counters.ssn.ssn_prox_updates, 0)
        << "and that subproblem's proximal ladder must have moved, or there is no level to carry";
    EXPECT_GT(s.warm_start.prox_sigma, 0.0)
        << "the level reaches the exported currency. Before the fix-round-1 ruling this path was "
           "excluded from the carry and this read 0, so a continuation loop alternating kSsn and "
           "kIpm re-climbed the ladder with no diagnostic";
}

// THE CLOSED ROUTING PARTITION, PINNED ON HAND-BUILT COUNTERS -- the two rows
// that falsified the first round's two-term identity, neither of which the HS
// battery reaches at the shipped budgets.
TEST(IpqpDispatch, TheRoutingPartitionHelperHoldsOnTheRowsFixturesDoNotReach) {
    // Three entries: one clean refine-accept, one converged-budget exit routed
    // to the refinement (an escape that does NOT reach the walk), and one
    // saddle-suspect exit routed to SSN (an escape that does not either).
    IpqpCounters c;
    c.ipqp_refine_accepted = 2;
    c.ipqp_refine_refused = 0;
    c.ipqp_to_refine = 2;
    c.ipqp_escape_indefinite = 1;
    c.ipqp_to_ssn = 1;
    c.ipqp_to_walk = 0;
    c.ipqp_escapes = 2;
    c.ipqp_escape_budget = 1;
    EXPECT_TRUE(assert_ipqp_routing_partition(c, 3));
    EXPECT_TRUE(assert_ipqp_escape_census_sums(c));

    // A DECLINE lands in `ipqp_to_walk` without the tier having run, so it is
    // subtracted from the first-destination sum rather than counted in it.
    IpqpCounters d = c;
    d.ipqp_declined_pinned = 4;
    d.ipqp_to_walk = 4;
    EXPECT_TRUE(assert_ipqp_routing_partition(d, 3));

    // AND THE HELPER REALLY FAILS. One unrouted entry, one destination
    // double-counted, and one `to_refine` that disagrees with its outcomes.
    EXPECT_FALSE(assert_ipqp_routing_partition(c, 4));
    IpqpCounters lost = c;
    lost.ipqp_to_walk = 1;
    EXPECT_FALSE(assert_ipqp_routing_partition(lost, 3));
    IpqpCounters mismatched = c;
    mismatched.ipqp_to_refine = 3;
    EXPECT_FALSE(assert_ipqp_routing_partition(mismatched, 3));
}

// THE W2 HOOK CARRIES SECTION 6.3's EVIDENCE (fix round 1, C2). A FREE
// function for the reason the other seams are: it can be called without a
// driver, so the seam's shape -- what W2 will find in its hands -- is pinnable
// rather than merely readable.
TEST(IpqpDispatch, TheFeasibilityHookTakesTheEvidenceAndIsTheColdWalkToday) {
    QpProblem qp;
    qp.H.resize(1, 1);
    qp.H.insert(0, 0) = 1.0;
    qp.H.makeCompressed();
    qp.g = (Vec(1) << -2.0).finished();
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    qp.Ai.resize(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, -10.0);
    qp.upper = Vec::Constant(1, 10.0);

    QpOptions qopts;
    QpEngine engine(qopts);
    SolveOverrides overrides;

    // A POPULATED evidence block: the least-infeasible point and its
    // corroboration, which is what makes W2's elastic reformulation possible.
    IpqpInfeasibilityEvidence evidence;
    evidence.fired = true;
    evidence.least_infeasible_x = (Vec(1) << 0.25).finished();
    evidence.least_infeasible_primal = 0.125;
    evidence.farkas_corroborated = true;

    NlpEval ev;
    SqpIterate row;
    const QpSolution taken =
        certified_feasibility_fallback(engine, qp, ev, nullptr, evidence, overrides, row);
    EXPECT_EQ(taken.status, QpStatus::kOptimal);
    ASSERT_EQ(taken.x.size(), 1);
    EXPECT_NEAR(taken.x(0), 2.0, 1e-9) << "the unconstrained minimum of 0.5 x^2 - 2x";

    // W1's BODY IS THE COLD WALK, and "cold" is what this second call says: a
    // seed is accepted by the signature and ignored by the body, so the answer
    // is the same one an unseeded call gives.
    QpSolution seed;
    seed.x = (Vec(1) << -9.0).finished();
    seed.bound_state.assign(1, BoundState::kFree);
    SqpIterate seeded_row;
    const QpSolution seeded =
        certified_feasibility_fallback(engine, qp, ev, &seed, evidence, overrides, seeded_row);
    EXPECT_EQ(seeded.x, taken.x);

    // AND THE EVIDENCE IS RECORDED, not merely accepted (fix round 3): W1's
    // body acts on none of it, so without these two the whole payload would be
    // unobservable and a caller passing a default-constructed block would be
    // indistinguishable from one passing the real thing.
    EXPECT_DOUBLE_EQ(row.ipqp_least_infeasible_primal, evidence.least_infeasible_primal);
    EXPECT_EQ(row.ipqp_farkas_corroborated, evidence.farkas_corroborated);

    // THE MUTATION PARTNER, spelled out: a DEFAULT block records zero/false,
    // so the call site's argument is what these two report.
    SqpIterate default_row;
    const IpqpInfeasibilityEvidence unset;
    certified_feasibility_fallback(engine, qp, ev, nullptr, unset, overrides, default_row);
    EXPECT_DOUBLE_EQ(default_row.ipqp_least_infeasible_primal, 0.0);
    EXPECT_FALSE(default_row.ipqp_farkas_corroborated);
    EXPECT_NE(row.ipqp_least_infeasible_primal, default_row.ipqp_least_infeasible_primal)
        << "the fixture's evidence must differ from a default one, or the pin cannot see the "
           "substitution it exists to catch";
}

// ===========================================================================
// THE SEAM FUNCTIONS, PINNED ON HAND-BUILT RESULTS.
//
// The routing chain reads exactly two things off an IpqpResult -- "is this a
// step I may use" and "what does it look like in the QP layer's currency" --
// and both are free functions precisely so they can be pinned without a
// driver, on results a solve would take work to produce.
// ===========================================================================

// A minimal well-formed result: converged, certified, two variables, one row.
IpqpResult make_finished_result() {
    IpqpResult r;
    r.status = QpStatus::kOptimal;
    r.escape_reason = IpqpEscape::kNone;
    r.x = (Vec(2) << 0.25, 0.75).finished();
    r.lambda_e = Vec(0);
    r.lambda_i = (Vec(1) << 2.0).finished();
    r.z = (Vec(2) << 1.5, 0.0).finished();
    r.bound_state = {BoundState::kAtLower, BoundState::kFree};
    r.ineq_active = {true};
    r.tr_active = {false, true};
    r.counters.ipqp_factorizations = 7;
    r.counters.ipqp_symbolic_analyses = 1;
    return r;
}

// `QpSolution::tr_active`'s contract, reproduced (qp_problem.h:102): the flag
// travels to `refine_on_face`, which reads it as PART OF THE FACE and pins
// those variables at the value the tier stopped at. Dropping it here would
// silently hand tier 3 a different face than the one the tier solved.
TEST(IpqpDispatch, TheQpSolutionMappingCarriesTheWholeFaceIncludingTrActive) {
    const IpqpResult r = make_finished_result();
    const QpSolution qs = ipqp_result_to_qp_solution(r);

    EXPECT_EQ(qs.status, QpStatus::kOptimal)
        << "forced, exactly as the SSN mapping forces it: the caller has already judged the exit "
           "usable, and the tier's own status is not this currency";
    EXPECT_EQ(qs.x, r.x);
    EXPECT_EQ(qs.lambda_i, r.lambda_i);
    EXPECT_EQ(qs.z, r.z);
    EXPECT_EQ(qs.bound_state, r.bound_state);
    EXPECT_EQ(qs.ineq_active, r.ineq_active);
    EXPECT_EQ(qs.tr_active, r.tr_active) << "QpSolution::tr_active's contract verbatim";
    EXPECT_EQ(qs.counters.factorizations, r.counters.ipqp_factorizations)
        << "the two fields that mean the same physical thing in every kernel";
    EXPECT_EQ(qs.counters.symbolic_analyses, r.counters.ipqp_symbolic_analyses);
}

// THE USABILITY GATE, ROW BY ROW -- including the routing hazard task 5 raised
// (Claude co-review I-4), which is the one row where an ESCAPE still leaves a
// converged iterate in hand.
TEST(IpqpDispatch, TheUsabilityGateAcceptsADowngradedCertificateAndARefusedFinalRead) {
    QpOptions qopts;
    IpqpOptions iopts;
    // Residuals that MEET the barrier phase's target, so the only thing under
    // test below is the escape/downgrade bookkeeping.
    IpqpResiduals met;
    met.stationarity = 1e-12;
    met.primal_eq = 1e-12;
    met.primal_iq = 1e-12;
    met.complementarity = 1e-12;
    ASSERT_TRUE(ipqp_residuals_meet_target(met, qopts, iopts)) << "the fixture's own premise";

    // (a) converged and certified.
    IpqpResult clean = make_finished_result();
    clean.residuals = met;
    EXPECT_TRUE(ipqp_exit_is_a_usable_step(clean, qopts, iopts));

    // (b) DOWNGRADED WITHOUT AN ESCAPE -- `ipqp_require_final_inertia = false`
    // (read == 3) or a mid-solve evidence failure (read == 0). Plan section 7
    // note (j): a downgrade is not an escape, and the point converged.
    IpqpResult downgraded = clean;
    downgraded.status = QpStatus::kNumericalError; // task 5's status ruling
    downgraded.certificate_downgraded = true;
    downgraded.counters.ipqp_final_inertia_read = 3;
    EXPECT_TRUE(ipqp_exit_is_a_usable_step(downgraded, qopts, iopts))
        << "the gate reads escape_reason and NEVER status -- a downgraded certificate is a usable "
           "converged point";

    IpqpResult evidence_failed = clean;
    evidence_failed.status = QpStatus::kNumericalError;
    evidence_failed.certificate_downgraded = true;
    evidence_failed.inertia_evidence_failed = true;
    evidence_failed.counters.ipqp_final_inertia_read = 0;
    EXPECT_TRUE(ipqp_exit_is_a_usable_step(evidence_failed, qopts, iopts))
        << "the fifth way to a downgrade, whose final read is a perfectly good 0";

    // (c) THE HAZARD: kBudget with the final read REFUSED on a converged
    // iterate. Routed as a downgraded certificate, not as budget exhaustion.
    IpqpResult refused_read = clean;
    refused_read.status = QpStatus::kNumericalError;
    refused_read.escape_reason = IpqpEscape::kBudget;
    refused_read.certificate_downgraded = true;
    refused_read.counters.ipqp_final_inertia_read = 3;
    EXPECT_TRUE(ipqp_exit_is_a_usable_step(refused_read, qopts, iopts));

    // ... AND ITS THREE NEIGHBOURS, each of which must be REFUSED, because
    // each is what the hazard would be confused with.
    IpqpResult cap = refused_read;
    cap.residuals.stationarity = 1.0; // the iteration cap stopped it mid-descent
    EXPECT_FALSE(ipqp_exit_is_a_usable_step(cap, qopts, iopts))
        << "budget at the ITERATION cap: the residuals did not meet the target, so this is a "
           "genuine escape and the iterate is discarded";

    IpqpResult attempted = refused_read;
    attempted.counters.ipqp_final_inertia_read = 2; // attempted and unusable
    EXPECT_FALSE(ipqp_exit_is_a_usable_step(attempted, qopts, iopts))
        << "read == 2 is the attempted-and-unusable reading (plan note h), a numerical class -- "
           "only the read that NEVER HAPPENED (3) is the refused-certificate row";

    IpqpResult not_downgraded = refused_read;
    not_downgraded.certificate_downgraded = false;
    EXPECT_FALSE(ipqp_exit_is_a_usable_step(not_downgraded, qopts, iopts))
        << "a kBudget exit whose certificate was NOT downgraded is not the refused-read row";

    // (d) THE GENUINE ESCAPES, all refused however good the residuals look.
    for (IpqpEscape reason : {IpqpEscape::kStall, IpqpEscape::kNumerical,
                              IpqpEscape::kInfeasibleSuspect, IpqpEscape::kIndefinite}) {
        IpqpResult escaped = clean;
        escaped.escape_reason = reason;
        EXPECT_FALSE(ipqp_exit_is_a_usable_step(escaped, qopts, iopts))
            << "escape reason " << static_cast<int>(reason);
    }

    // (e) A DECLINE, and a non-finite point that reported no escape.
    IpqpResult declined;
    declined.declined_pinned = true;
    EXPECT_FALSE(ipqp_exit_is_a_usable_step(declined, qopts, iopts));

    IpqpResult nonfinite = clean;
    nonfinite.x(0) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(ipqp_exit_is_a_usable_step(nonfinite, qopts, iopts))
        << "the defensive arm: kNone with a point nothing downstream could use";
}

// ===========================================================================
// EQUIVALENCE -- kIpm IS A DIFFERENT ROUTE TO THE SAME ANSWER.
// ===========================================================================

TEST(IpqpDispatch, KIpmAndKWalkAgreeOnTheAnswerAcrossTheHsBattery) {
    for (int number : hs_numbers()) {
        SCOPED_TRACE(fmt::format("HS{}", number));

        auto walk_p = make_hs(number);
        SqpDriver walk_driver(walk_options());
        const SqpSolution walk = walk_driver.solve(*walk_p.model);
        ASSERT_EQ(walk.status, SqpStatus::kOptimal);

        auto ipm_p = make_hs(number);
        SqpDriver ipm_driver(ipm_options());
        const SqpSolution ipm = ipm_driver.solve(*ipm_p.model);
        ASSERT_EQ(ipm.status, SqpStatus::kOptimal);

        // HS33 IS A DECLARED EXCEPTION, AND IT IS THE TIER WINNING (T4b).
        // HS33 is nonconvex with more than one KKT point, and the two kernels
        // now stop at different ones: the walk at a local minimizer with
        // `f = -4`, the tier at `f = sqrt(2) - 6 = -4.5857864376269...`, which
        // is HS33'S PUBLISHED OPTIMUM. Before T4b both arms returned `-4`,
        // because the tier's indefinite subproblems froze and were handed to
        // the walk. Asserting agreement here would now be asserting that the
        // tier must give up its better answer, so the row is pinned as the
        // two VALUES it actually reaches, and the equivalence claim is stated
        // for what it is: the two modes solve the same NLP to the same
        // STATUS, and agree on the answer wherever the NLP has one answer.
        if (number == 33) {
            EXPECT_NEAR(ipm.f, std::sqrt(2.0) - 6.0, 1e-6)
                << "the kIpm arm reaches HS33's published optimum";
            EXPECT_NEAR(walk.f, -4.0, 1e-6) << "and the walk arm a different KKT point";
            EXPECT_LT(ipm.f, walk.f) << "the tier's point is the better of the two, which is why "
                                        "this row is an exception rather than a regression";
            continue;
        }

        EXPECT_NEAR(ipm.f, walk.f, 1e-6 * std::max(1.0, std::abs(walk.f)))
            << "the two modes solve the same NLP and must agree on its value";
        ASSERT_EQ(ipm.x.size(), walk.x.size());
        // A LOOSER BAND ON x THAN ON f, deliberately: f is flat at an optimum,
        // so two kernels can agree on it to 1e-6 while stopping at points a
        // few 1e-5 apart, and HS problems with a flat valley (HS25 is the
        // family's own example) do exactly that. What is asserted is that they
        // stopped at the SAME optimum, not that they stopped at the same
        // iterate.
        EXPECT_LT((ipm.x - walk.x).template lpNorm<Eigen::Infinity>(),
                  1e-4 * std::max(1.0, walk.x.template lpNorm<Eigen::Infinity>()))
            << "and on where it is";
    }
}

} // namespace
} // namespace hven::solvers
