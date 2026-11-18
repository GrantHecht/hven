// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_iteration_history.cpp -- M8 TR-D / TR-D': `IpmResult::history` and `SqpResult::history` are
// the same `IterationRecord` rows the trace writes, ALWAYS ON. `history` == the depth-0 `iter`
// stream, row for row (every member, by bits), on both engines across the HS battery and the
// trace battery; it exists with no sink attached and is the same with one attached; and the SQP
// row carries `factorizations` unconditionally.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include <hven/detail/model/problem_access.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/sqp/sqp_solver.h>

#include "solver/support/iteration_record_eq.h"
#include "model/support/hs_problems.h"
#include "trace/support/trace_battery.h"

namespace {

using namespace hven;
using hven::test::same_record;
using namespace hven_drivers_tests;

std::vector<IterationRecord> depth0(const CaptureSink &sink) {
    std::vector<IterationRecord> out;
    for (const CapturedRow &r : sink.rows) {
        if (r.depth == 0) {
            out.push_back(r.record);
        }
    }
    return out;
}

void expect_history_is_stream(const std::vector<IterationRecord> &history, const CaptureSink &sink,
                              bool ipm) {
    const std::vector<IterationRecord> stream = depth0(sink);
    ASSERT_EQ(history.size(), stream.size()) << "one history row per depth-0 iter line";
    for (std::size_t k = 0; k < history.size(); ++k) {
        EXPECT_TRUE(same_record(history[k], stream[k])) << "row " << k;
        EXPECT_EQ(history[k].ipm.has_value(), ipm) << "row " << k;
        EXPECT_EQ(history[k].sqp.has_value(), !ipm) << "row " << k;
    }
}

IpmOptions ipm_opts() {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

SqpOptions sqp_opts() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

TEST(IterationHistory, HsBatteryHistoryIsTheDepthZeroIterStreamOnBothEngines) {
    int ipm_cells = 0;
    int sqp_cells = 0;
    for (const int hs : test_support::hs_numbers()) {
        SCOPED_TRACE("hs" + std::to_string(hs));
        {
            const auto p = test_support::make_hs(hs);
            IpmSolver solver(ipm_opts());
            CaptureSink sink;
            solver.attach_trace(&sink);
            try {
                const IpmResult r =
                    solver.solve(detail::problem_ref(*p.model), p.model->start_point());
                expect_history_is_stream(r.history, sink, true);
                EXPECT_EQ(static_cast<Index>(r.history.size()), r.iterations);
                ++ipm_cells;
            } catch (const std::exception &) {
            }
        }
        {
            const auto p = test_support::make_hs(hs);
            SqpSolver solver(sqp_opts());
            CaptureSink sink;
            solver.attach_trace(&sink);
            try {
                const SqpResult r =
                    solver.solve(detail::problem_ref(*p.model), p.model->start_point());
                expect_history_is_stream(r.history, sink, false);
                ++sqp_cells;
            } catch (const std::exception &) {
            }
        }
    }
    EXPECT_GE(ipm_cells, 20) << "premise: the IPM battery ran";
    EXPECT_GE(sqp_cells, 20) << "premise: the SQP battery ran";
}

TEST(IterationHistory, TraceBatteryHistoryIsTheDepthZeroIterStreamOnBothEngines) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        {
            Problem problem(c.make());
            IpmSolver solver(ipm_opts());
            CaptureSink sink;
            solver.attach_trace(&sink);
            const IpmResult r = solver.solve(problem, c.start());
            ASSERT_FALSE(r.history.empty());
            expect_history_is_stream(r.history, sink, true);
        }
        {
            Problem problem(c.make());
            SqpSolver solver(sqp_opts());
            CaptureSink sink;
            solver.attach_trace(&sink);
            const SqpResult r = solver.solve(problem, c.start());
            ASSERT_FALSE(r.history.empty());
            expect_history_is_stream(r.history, sink, false);
        }
    }
}

// ALWAYS ON: with no sink and `kSilent` nothing is rendered, yet the history is there and equals
// the one an attached sink saw (a history built only when tracing is on fails here).
TEST(IterationHistory, HistoryExistsWithNoSinkAndIsTheSameWithOne) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        {
            Problem a(c.make());
            Problem b(c.make());
            IpmSolver quiet(ipm_opts());
            IpmSolver traced(ipm_opts());
            CaptureSink sink;
            traced.attach_trace(&sink);
            const IpmResult ra = quiet.solve(a, c.start());
            const IpmResult rb = traced.solve(b, c.start());
            ASSERT_FALSE(ra.history.empty()) << "ipm history with no sink";
            ASSERT_EQ(ra.history.size(), rb.history.size());
            for (std::size_t k = 0; k < ra.history.size(); ++k) {
                EXPECT_TRUE(same_record(ra.history[k], rb.history[k])) << "ipm row " << k;
            }
        }
        {
            Problem a(c.make());
            Problem b(c.make());
            SqpSolver quiet(sqp_opts());
            SqpSolver traced(sqp_opts());
            CaptureSink sink;
            traced.attach_trace(&sink);
            const SqpResult ra = quiet.solve(a, c.start());
            const SqpResult rb = traced.solve(b, c.start());
            ASSERT_FALSE(ra.history.empty()) << "sqp history with no sink";
            ASSERT_EQ(ra.history.size(), rb.history.size());
            for (std::size_t k = 0; k < ra.history.size(); ++k) {
                EXPECT_TRUE(same_record(ra.history[k], rb.history[k])) << "sqp row " << k;
            }
        }
    }
}

// TR-D': the SQP row carries `factorizations` unconditionally: the (sub-)solve's running count,
// never decreasing, ending at or below the solve's total, and moving on a solve that factorizes.
TEST(IterationHistory, SqpRowCarriesTheRunningFactorizationCount) {
    for (const BatteryCase &c : battery()) {
        SCOPED_TRACE(c.name);
        Problem problem(c.make());
        SqpSolver solver(sqp_opts());
        const SqpResult r = solver.solve(problem, c.start());
        ASSERT_GE(r.history.size(), 2u);
        Index prev = 0;
        for (const IterationRecord &row : r.history) {
            EXPECT_GE(row.factorizations, prev);
            prev = row.factorizations;
        }
        EXPECT_LE(prev, r.counters.factorizations);
        EXPECT_GT(prev, 0) << "premise: the solve factorized before its last row";
    }
}

} // namespace
