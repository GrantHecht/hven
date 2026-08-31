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

/// Asserts the section 2.3 ROUTING PARTITION: every subproblem the routing
/// chain disposed of went to exactly one successor, and the two `ipqp_to_*`
/// counters say which.
///
/// The two identities are the routing table read as arithmetic (M6 W1 task 6):
///
///   `ipqp_to_ssn  == ipqp_refine_refused + ipqp_escape_indefinite`
///       -- section 2.3's TWO routes into the SSN warm grade, item 4's own
///          pair: a refused refinement, and a saddle-suspect exit.
///   `ipqp_to_walk == (ipqp_escapes - ipqp_escape_indefinite)
///                    + ipqp_declined_pinned`
///       -- item 5's genuine escapes MINUS the indefinite ones (which went to
///          SSN instead), PLUS the domain gate's pre-solve declines, which
///          `ipqp_to_walk`'s own doc comment names as its second contributor.
///
/// A subproblem the tier-3 refinement ACCEPTED is in neither: it was not
/// routed onward at all, and `ipqp_refine_accepted` counts it.
inline ::testing::AssertionResult assert_ipqp_routing_partition(const IpqpCounters &c) {
    const Index to_ssn = c.ipqp_refine_refused + c.ipqp_escape_indefinite;
    if (c.ipqp_to_ssn != to_ssn) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_ssn " << c.ipqp_to_ssn
               << " != refine_refused " << c.ipqp_refine_refused << " + escape_indefinite "
               << c.ipqp_escape_indefinite;
    }
    const Index to_walk = c.ipqp_escapes - c.ipqp_escape_indefinite + c.ipqp_declined_pinned;
    if (c.ipqp_to_walk != to_walk) {
        return ::testing::AssertionFailure()
               << "assert_ipqp_routing_partition: ipqp_to_walk " << c.ipqp_to_walk
               << " != (escapes " << c.ipqp_escapes << " - escape_indefinite "
               << c.ipqp_escape_indefinite << ") + declined_pinned " << c.ipqp_declined_pinned;
    }
    return ::testing::AssertionSuccess();
}

} // namespace hven::solvers::test_support
