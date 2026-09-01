// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/ipqp_test_support.h -- test-support only, NOT part of the public library
// surface. Shared IPQP counter predicates; AssertionResult, not a direct assert, so the checker
// stays mutation-pinnable by test_ipqp_counters.cpp. Requirement: `.superpowers/w1-t2-report.md`.

#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>

namespace hven::solvers::test_support {

/// Asserts the five-way escape census sums to `ipqp_escapes` (plan section 7
/// note b: escape-COUNT only -- the dropped stall-reason sub-counters play no
/// part in this invariant).
inline ::testing::AssertionResult assert_ipqp_escape_census_sums(const IpqpCounters &c) {
    const Index sum = c.ipqp_escape_budget + c.ipqp_escape_stall + c.ipqp_escape_indefinite +
                      c.ipqp_escape_numerical + c.ipqp_escape_infeasible_suspect;
    if (sum != c.ipqp_escapes) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_escape_census_sums: census sum " << sum
               << " (budget=" << c.ipqp_escape_budget << " stall=" << c.ipqp_escape_stall
               << " indefinite=" << c.ipqp_escape_indefinite
               << " numerical=" << c.ipqp_escape_numerical
               << " infeasible_suspect=" << c.ipqp_escape_infeasible_suspect << ") != ipqp_escapes "
               << c.ipqp_escapes;
    }
    return ::testing::AssertionSuccess();
}

/// Asserts the section 2.3 ROUTING PARTITION IS CLOSED: every subproblem the
/// routing chain disposed of went to exactly one FIRST destination, and the
/// three `ipqp_to_*` counters say which.
///
/// @param c            the solve's folded IPQP counters.
/// @param tier_entries subproblems that ENTERED the tier (neither retired-past nor declined);
///        the counters cannot express it, so the caller supplies it. Driver fixtures derive it
///        as `ipqp_symbolic_analyses + ipqp_pattern_verifies` -- plan section 7 note (k).
///
/// FOUR IDENTITIES, and the first is the closed one:
///
///   `to_refine + escape_indefinite + (to_walk - declined_pinned)`
///       `== tier_entries`
///       -- one FIRST destination each: the tier-3 refinement, the SSN warm grade
///          (saddle-suspect), or the cold walk. A decline also lands in `to_walk`
///          without the tier having run, so it is subtracted.
///   `to_refine == refine_accepted + refine_refused`
///       -- the refinement's two outcomes, so `to_refine` counts hand-offs.
///   `to_ssn == refine_refused + escape_indefinite`
///       -- item 4's TWO feeders; a refusal reaches SSN SECOND, so `to_ssn` is not in the sum.
///   `to_walk >= declined_pinned`.
///
/// The closed form needs `to_refine`: the two-term version is FALSE on the converged-`kBudget`
/// row. Derivation: `.superpowers/w1-t6-report.md` section R2.
inline ::testing::AssertionResult assert_ipqp_routing_partition(const IpqpCounters &c,
                                                                Index tier_entries) {
    if (c.ipqp_to_refine != c.ipqp_refine_accepted + c.ipqp_refine_refused) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_refine " << c.ipqp_to_refine
               << " != refine_accepted " << c.ipqp_refine_accepted << " + refine_refused "
               << c.ipqp_refine_refused;
    }
    if (c.ipqp_to_ssn != c.ipqp_refine_refused + c.ipqp_escape_indefinite) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_ssn " << c.ipqp_to_ssn
               << " != refine_refused " << c.ipqp_refine_refused << " + escape_indefinite "
               << c.ipqp_escape_indefinite;
    }
    if (c.ipqp_to_walk < c.ipqp_declined_pinned) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_walk " << c.ipqp_to_walk
               << " is below ipqp_declined_pinned " << c.ipqp_declined_pinned
               << ", but every decline routes to the walk";
    }
    const Index first_destinations =
        c.ipqp_to_refine + c.ipqp_escape_indefinite + (c.ipqp_to_walk - c.ipqp_declined_pinned);
    if (first_destinations != tier_entries) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: first destinations " << first_destinations
               << " (to_refine=" << c.ipqp_to_refine
               << " escape_indefinite=" << c.ipqp_escape_indefinite << " to_walk=" << c.ipqp_to_walk
               << " declined_pinned=" << c.ipqp_declined_pinned << ") != tier entries "
               << tier_entries;
    }
    return ::testing::AssertionSuccess();
}

} // namespace hven::solvers::test_support
