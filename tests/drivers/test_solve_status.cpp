// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The shared status vocabulary: the severity order, the interior-point mapping
// with its stop-reason split, and the SQP mapping. The LIVE pins on the stop
// reason itself are in tests/interior/ -- they need a solve.

#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <stdexcept>
#include <vector>

#include <hven/drivers/solve_status.h>

using hven::ConvergenceFlags;
using hven::solvers::IpmStopReason;
using hven::solvers::severity;
using hven::solvers::SolveStatus;
using hven::solvers::SqpStatus;
using hven::solvers::to_solve_status;
using hven::solvers::to_string;

namespace {

// The nine statuses in the documented reporting order, weakest first.
const SolveStatus kSeverityOrder[] = {
    SolveStatus::kOptimal,    SolveStatus::kAcceptable,      SolveStatus::kInterrupted,
    SolveStatus::kMaxIter,    SolveStatus::kBudgetExhausted, SolveStatus::kStalled,
    SolveStatus::kInfeasible, SolveStatus::kDiverging,       SolveStatus::kNumericalError};

} // namespace

TEST(SolveStatus, SeverityIsTotalAndOrderedAsDocumented) {
    for (int i = 1; i < 9; ++i) {
        SCOPED_TRACE(to_string(kSeverityOrder[i]));
        EXPECT_LT(severity(kSeverityOrder[i - 1]), severity(kSeverityOrder[i]));
    }
    // Total, not merely monotone on this list: nine distinct ranks over nine
    // distinct statuses, and every name distinct too.
    std::set<int> ranks;
    std::set<std::string> names;
    for (const SolveStatus s : kSeverityOrder) {
        ranks.insert(severity(s));
        names.insert(to_string(s));
    }
    EXPECT_EQ(ranks.size(), 9u);
    EXPECT_EQ(names.size(), 9u);
}

TEST(SolveStatus, IpmMappingSplitsNotConvergedByStopReason) {
    EXPECT_EQ(to_solve_status(ConvergenceFlags::NOTCONVERGED, IpmStopReason::kIterationCap),
              SolveStatus::kMaxIter);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::NOTCONVERGED, IpmStopReason::kStageStalled),
              SolveStatus::kStalled);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::NOTCONVERGED,
                              IpmStopReason::kRestorationLocallyInfeasible),
              SolveStatus::kStalled);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::NOTCONVERGED, IpmStopReason::kNone),
              SolveStatus::kMaxIter);
    // The stronger verdict survives the stall: the mapping reads the flag first.
    // This is the rule the live pins cannot reach -- ACCEPTABLE needs fifty
    // consecutive acceptable iterates coinciding with a stall.
    EXPECT_EQ(to_solve_status(ConvergenceFlags::ACCEPTABLE, IpmStopReason::kStageStalled),
              SolveStatus::kAcceptable);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::CONVERGED, IpmStopReason::kStageStalled),
              SolveStatus::kOptimal);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::SINGULAR_KKT, IpmStopReason::kNone),
              SolveStatus::kNumericalError);
    EXPECT_EQ(to_solve_status(ConvergenceFlags::DIVERGING, IpmStopReason::kNone),
              SolveStatus::kDiverging);
    // Reachability, stated: the interior-point engine reports neither of these.
    for (const ConvergenceFlags f :
         {ConvergenceFlags::CONVERGED, ConvergenceFlags::ACCEPTABLE, ConvergenceFlags::NOTCONVERGED,
          ConvergenceFlags::DIVERGING, ConvergenceFlags::SINGULAR_KKT}) {
        for (const IpmStopReason r :
             {IpmStopReason::kNone, IpmStopReason::kIterationCap,
              IpmStopReason::kRestorationLocallyInfeasible, IpmStopReason::kStageStalled}) {
            const SolveStatus s = to_solve_status(f, r);
            EXPECT_NE(s, SolveStatus::kInfeasible);
            EXPECT_NE(s, SolveStatus::kBudgetExhausted);
            EXPECT_NE(s, SolveStatus::kInterrupted);
        }
    }
}

TEST(SolveStatus, SqpMappingIsIdentityOnItsFive) {
    EXPECT_EQ(to_solve_status(SqpStatus::kOptimal), SolveStatus::kOptimal);
    EXPECT_EQ(to_solve_status(SqpStatus::kMaxIter), SolveStatus::kMaxIter);
    EXPECT_EQ(to_solve_status(SqpStatus::kInfeasible), SolveStatus::kInfeasible);
    EXPECT_EQ(to_solve_status(SqpStatus::kNumericalError), SolveStatus::kNumericalError);
    EXPECT_EQ(to_solve_status(SqpStatus::kBudgetExhausted), SolveStatus::kBudgetExhausted);
    EXPECT_STREQ(to_string(SolveStatus::kStalled), "stalled");
    EXPECT_STREQ(to_string(SolveStatus::kOptimal), "optimal");
    EXPECT_STREQ(to_string(SolveStatus::kBudgetExhausted), "budget_exhausted");
    EXPECT_STREQ(to_string(IpmStopReason::kNone), "none");
    EXPECT_STREQ(to_string(IpmStopReason::kIterationCap), "iteration_cap");
    EXPECT_STREQ(to_string(IpmStopReason::kRestorationLocallyInfeasible),
                 "restoration_locally_infeasible");
    EXPECT_STREQ(to_string(IpmStopReason::kStageStalled), "stage_stalled");
    // Out-of-range values are refused, not silently mapped: the CSV column and
    // the printed status come from these switches.
    EXPECT_THROW((void)to_string(static_cast<SolveStatus>(99)), std::invalid_argument);
    EXPECT_THROW((void)severity(static_cast<SolveStatus>(99)), std::invalid_argument);
    EXPECT_THROW((void)to_solve_status(static_cast<SqpStatus>(99)), std::invalid_argument);
}
