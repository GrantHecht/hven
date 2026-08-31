// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/ipqp_test_support.h — test-support only, NOT part of the
// public library surface. The IPQP tier's escape-census invariant, factored
// out so every task that produces a real IpqpCounters (T5's certification
// ladder, T6's routing chain, T9's acceptance battery) re-asserts the SAME
// check rather than re-deriving it -- task 2's own requirement, since this
// task's counters are dead code and the invariant is otherwise unexercised
// until a later task writes a real escape.
//
// Returns ::testing::AssertionResult (gtest's predicate-assertion idiom, the
// derivative_check.h convention) rather than asserting directly, so a call
// site chooses EXPECT_TRUE/ASSERT_TRUE and the checker itself stays pinnable
// by a mutation self-test (test_ipqp_counters.cpp's own
// EscapeCensusHelperCatchesAViolation) -- a helper that asserted directly
// could not be exercised that way.

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
/// @param tier_entries how many subproblems the tier was CONSULTED on, i.e.
///        entered the engine: neither retired-past nor declined by the domain
///        gate. The counters cannot express this on their own -- a retired
///        major writes nothing at all -- so the caller supplies it. Under the
///        hoisting discipline of plan section 7 note (k) it is exactly
///        `ipqp_symbolic_analyses + ipqp_pattern_verifies` (one of {analyze,
///        verify} per entry), which is how the driver-level fixtures get it;
///        naming it here rather than deriving it inside keeps the two
///        invariants from smearing into one diagnosis.
///
/// FOUR IDENTITIES, and the first is the closed one:
///
///   `to_refine + escape_indefinite + (to_walk - declined_pinned)`
///       `== tier_entries`
///       -- every subproblem the tier actually ran on left by exactly one
///          FIRST destination: the tier-3 refinement (item 3), the SSN warm
///          grade directly (item 4's saddle-suspect route), or the cold walk
///          (item 5). `declined_pinned` is subtracted because a decline also
///          lands in `to_walk` -- the counter's own settled text -- without
///          the tier having run.
///   `to_refine == refine_accepted + refine_refused`
///       -- the refinement's two outcomes, so `to_refine` counts hand-offs.
///   `to_ssn == refine_refused + escape_indefinite`
///       -- item 4's TWO feeders; a refusal reaches SSN as a SECOND
///          destination, which is why `to_ssn` is not in the closed sum.
///   `to_walk >= declined_pinned`.
///
/// WHY THE CLOSED FORM NEEDS `to_refine` AT ALL (M6 W1 task 6 fix round 1):
/// the two-term version `to_walk == escapes - escape_indefinite +
/// declined_pinned` is FALSE on the converged-`kBudget` row, which increments
/// `ipqp_escape_budget` and routes to the refinement.
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
