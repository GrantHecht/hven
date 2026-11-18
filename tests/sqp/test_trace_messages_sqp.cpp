// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_trace_messages_sqp.cpp -- M8 TR-B: the SQP's restoration episodes that RESUME the solve
// are written as `restoration_exited{s: resumed}`, one per `restoration_entered`.

#include <gtest/gtest.h>

#include <string_view>

#include <hven/detail/model/problem_access.h>
#include <hven/sqp/sqp_solver.h>
#include <hven/trace/trace.h>

#include "support/hs_problems.h"

namespace hven {
namespace {

using ::hven::test_support::hs_numbers;
using ::hven::test_support::make_hs;

struct Tally final : TraceSink {
    Index entered = 0;
    Index exited = 0;
    Index resumed = 0;
    Index resumed_notices = 0;
    void on_message(const MessageTraceEvent &e) override {
        if (e.kind == MessageKind::kRestorationEntered) {
            ++entered;
        } else if (e.kind == MessageKind::kRestorationExited) {
            ++exited;
            if (e.s && *e.s == "resumed") {
                ++resumed;
                resumed_notices += e.severity == MessageSeverity::kNotice ? 1 : 0;
            }
        }
    }
};

TEST(TraceMessagesSqpResume, AResumedRestorationIsANoticeAndEveryEntryHasAnExit) {
    Index resumed = 0;
    for (const int hs : hs_numbers()) {
        for (const double scale : {1.0, -3.0, 10.0, 50.0}) {
            SCOPED_TRACE("hs" + std::to_string(hs) + " scale " + std::to_string(scale));
            const auto p = make_hs(hs);
            SqpOptions opts;
            opts.common.verbosity = Verbosity::kSilent;
            opts.common.max_iterations = 200;
            opts.advanced.max_restoration_entries = 4;
            SqpSolver driver(opts);
            Tally tally;
            driver.attach_trace(&tally);
            Vec x0 = p.model->start_point() * scale;
            try {
                driver.solve(hven::detail::problem_ref(*p.model), x0);
            } catch (const std::exception &) {
                continue;
            }
            EXPECT_EQ(tally.exited, tally.entered);
            EXPECT_EQ(tally.resumed_notices, tally.resumed);
            resumed += tally.resumed;
        }
    }
    EXPECT_GE(resumed, 1) << "premise: the scan reaches a resumed restoration";
}

} // namespace
} // namespace hven
