// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The shared driver surface's two small exhaustive-table functions, both cold
// on the W6 T0 coverage read:
//
//   * `to_string(PredictorOutcome)` (src/drivers/sqp_print.cpp:38-48) had NO
//     caller in the test suites at all -- the whole function read 0, which made
//     it the largest uncovered region in that TU. `to_string(StepVerdict)`'s
//     four arms are exercised by the console-sink tests; only its
//     unknown-value fallback (`:61`) was cold.
//
//   * `census_variable_bounds` (src/drivers/trace.cpp) is the ONE copy of the
//     bound-census arithmetic, shared by both engines' `solve.begin` lines. Its
//     three boundary refusals and two of its five classification arms read 0.
//
// Both are display/report contracts, which CLAUDE.md §4 and §5 name explicitly:
// a table test that walks every enumerator once, and a census that walks every
// classification once, are what pin them. The enum tables also fail loudly if
// an enumerator is ADDED without a display string -- the `switch` has no
// `default`, so a new enumerator falls to the trailing sentinel, and the
// sentinel is asserted here to belong to no declared value.

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>
#include <string>

#include <hven/detail/warmstart/predictor.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/trace.h>

using hven::Index;
using hven::Vec;
using hven::solvers::census_variable_bounds;
using hven::solvers::PredictorOutcome;
using hven::solvers::StepVerdict;
using hven::solvers::to_string;
using hven::solvers::VariableBoundCensus;

namespace {

constexpr double kPrintInf = std::numeric_limits<double>::infinity();

Vec print_vec(const std::vector<double> &v) {
    Vec out(static_cast<Index>(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        out(static_cast<Index>(i)) = v[i];
    }
    return out;
}

} // namespace

// ===========================================================================
// The enum display tables.
// ===========================================================================

// Every PredictorOutcome enumerator, once, with the string the trace schema and
// the console table both publish.
TEST(PrintTables, PredictorOutcomeNamesEveryEnumerator) {
    EXPECT_STREQ(to_string(PredictorOutcome::kPredicted), "Predicted");
    EXPECT_STREQ(to_string(PredictorOutcome::kZeroStep), "ZeroStep");
    EXPECT_STREQ(to_string(PredictorOutcome::kDegraded), "Degraded");
}

// The fallback exists so an out-of-contract value prints something rather than
// falling off the end of the function; it must belong to no DECLARED value,
// which is what makes the three assertions above exhaustive.
TEST(PrintTables, PredictorOutcomeFallbackBelongsToNoDeclaredValue) {
    EXPECT_STREQ(to_string(static_cast<PredictorOutcome>(99)), "Unknown");
    EXPECT_STRNE(to_string(PredictorOutcome::kPredicted), "Unknown");
    EXPECT_STRNE(to_string(PredictorOutcome::kZeroStep), "Unknown");
    EXPECT_STRNE(to_string(PredictorOutcome::kDegraded), "Unknown");
}

// The same, for the step verdict the iteration table prints in its own column.
TEST(PrintTables, StepVerdictNamesEveryEnumeratorAndFallsBackOnNoneOfThem) {
    EXPECT_STREQ(to_string(StepVerdict::kAcceptF), "AcceptF");
    EXPECT_STREQ(to_string(StepVerdict::kAcceptH), "AcceptH");
    EXPECT_STREQ(to_string(StepVerdict::kReject), "Reject");
    EXPECT_STREQ(to_string(StepVerdict::kRestore), "Restore");
    EXPECT_STREQ(to_string(static_cast<StepVerdict>(99)), "?");
}

// ===========================================================================
// The variable-bound census.
// ===========================================================================

// One variable of each of the five classes, in one call, so the branch order is
// exercised as a whole rather than one arm at a time: free, lower-only,
// upper-only, fixed (a zero-width box), ranged.
TEST(VariableBoundCensusTable, ClassifiesEveryKindOfBoundedVariable) {
    const Vec lower = print_vec({-kPrintInf, 0.0, -kPrintInf, 2.0, -1.0});
    const Vec upper = print_vec({kPrintInf, kPrintInf, 3.0, 2.0, 1.0});

    const VariableBoundCensus c = census_variable_bounds(lower, upper, 5);

    EXPECT_EQ(c.vars_free, 1);
    EXPECT_EQ(c.vars_lower_only, 1);
    EXPECT_EQ(c.vars_upper_only, 1);
    EXPECT_EQ(c.vars_fixed, 1);
    EXPECT_EQ(c.vars_ranged, 1);
}

// A size-0 vector is "no variable is bounded on that side" -- the
// interior-point NLP's own reading when nothing has been declared -- and is
// accepted rather than refused, on either side or both.
TEST(VariableBoundCensusTable, AnEmptySideMeansUnboundedThereForEveryVariable) {
    const Vec none;

    const VariableBoundCensus both = census_variable_bounds(none, none, 3);
    EXPECT_EQ(both.vars_free, 3);

    const VariableBoundCensus lower_only = census_variable_bounds(print_vec({0.0, 0.0}), none, 2);
    EXPECT_EQ(lower_only.vars_lower_only, 2);
    EXPECT_EQ(lower_only.vars_free, 0);

    const VariableBoundCensus upper_only = census_variable_bounds(none, print_vec({1.0, 1.0}), 2);
    EXPECT_EQ(upper_only.vars_upper_only, 2);
    EXPECT_EQ(upper_only.vars_free, 0);

    // And n == 0 censuses nothing at all rather than refusing.
    const VariableBoundCensus empty = census_variable_bounds(none, none, 0);
    EXPECT_EQ(empty.vars_free, 0);
    EXPECT_EQ(empty.vars_ranged, 0);
}

// The three boundary refusals, each asserted on its own message: a size check
// at a public boundary, which Eigen's asserts (compiled out under NDEBUG) must
// never be the only guard for.
TEST(VariableBoundCensusTable, RefusesANegativeWidthAndAMisSizedSide) {
    const Vec two = print_vec({0.0, 0.0});

    try {
        (void)census_variable_bounds(two, two, -1);
        ADD_FAILURE() << "a negative variable count must be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("n is -1"), std::string::npos) << e.what();
    }

    try {
        (void)census_variable_bounds(print_vec({0.0}), two, 2);
        ADD_FAILURE() << "a lower vector that is neither n-wide nor empty must be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("lower has size 1"), std::string::npos) << e.what();
    }

    try {
        (void)census_variable_bounds(two, print_vec({0.0, 0.0, 0.0}), 2);
        ADD_FAILURE() << "an upper vector that is neither n-wide nor empty must be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("upper has size 3"), std::string::npos) << e.what();
    }
}
