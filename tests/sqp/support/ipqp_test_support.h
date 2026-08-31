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

} // namespace hven::solvers::test_support
