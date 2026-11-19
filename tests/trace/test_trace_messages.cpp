// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_trace_messages.cpp -- M8 TR-B: the message identities of spec 2.7 on live solves of both
// engines. Counter identities where a counter exists; scenario pins where one does not (V5).

#include <limits>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>

#include <hven/core/errors.h>
#include <hven/core/stop.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>
#include <hven/sqp/sqp_solver.h>

#include "model/support/hs071_problem.h"
#include "trace/support/trace_battery.h"

namespace {

using namespace hven;
using namespace hven_model_tests;
using namespace hven_trace_tests;

/// min x0 + x1 s.t. x0^2 + x1^2 + 1 = 0 (infeasible). `nan_from > 0`: eval_f turns NaN from that
/// call on.
struct Infeasible final : Nlp {
    long nan_from = 0;
    mutable long calls = 0;
    NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(ConstVecRef x) const override {
        if (nan_from > 0 && ++calls >= nan_from) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return x[0] + x[1];
    }
    void eval_grad_f(ConstVecRef, VecRef g) const override { g << 1.0, 1.0; }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = x[0] * x[0] + x[1] * x[1] + 1.0; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v << 2.0 * x[0], 2.0 * x[1]; }
    void eval_hessian(ConstVecRef, double, ConstVecRef lambda, VecRef v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "Infeasible"; }
};

struct PoisonedHs071 final : Hs071Problem {
    mutable long calls = 0;
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
        Hs071Problem::eval_hessian(x, obj_factor, lambda, v);
        if (++calls >= 4) {
            v[2] = std::numeric_limits<double>::quiet_NaN();
        }
    }
};

Vec ones2() { return Vec::Constant(2, 1.0); }

Index count(const CaptureSink &s, MessageKind k) {
    Index n = 0;
    for (const MessageTraceEvent &m : s.messages) {
        n += m.kind == k ? 1 : 0;
    }
    return n;
}

Index count_s(const CaptureSink &s, MessageKind k, std::string_view what) {
    Index n = 0;
    for (const MessageTraceEvent &m : s.messages) {
        n += (m.kind == k && m.s && *m.s == what) ? 1 : 0;
    }
    return n;
}

IpmOptions ipm_restoring(IpmRestorationMode mode = IpmRestorationMode::kL1Nested) {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.advanced.restoration_mode = mode;
    o.advanced.max_restoration_entries = 2;
    return o;
}

SqpOptions sqp_quiet() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

/// Every restoration_exited has a legal `s`, and the severity rule (warning unless resumed).
void expect_exit_shape(const CaptureSink &sink) {
    for (const MessageTraceEvent &m : sink.messages) {
        if (m.kind != MessageKind::kRestorationExited) {
            continue;
        }
        ASSERT_TRUE(m.s.has_value());
        const std::string_view s = *m.s;
        EXPECT_TRUE(s == "resumed" || s == "locally_infeasible" || s == "failed" ||
                    s == "non_finite" || s == "interrupted")
            << s;
        EXPECT_EQ(m.severity == MessageSeverity::kNotice, s == "resumed");
        EXPECT_TRUE(m.b.has_value());
    }
}

// --- IPM ---------------------------------------------------------------------------------

TEST(TraceMessagesIpm, EveryRestorationEntryIsPairedWithAnExit) {
    Problem problem(std::make_shared<Infeasible>());
    IpmSolver solver(ipm_restoring());
    CaptureSink sink;
    solver.attach_trace(&sink);
    const IpmResult r = solver.solve(problem, ones2());
    ASSERT_GE(r.counters.restoration_entries, 1);
    EXPECT_EQ(count(sink, MessageKind::kRestorationEntered), r.counters.restoration_entries);
    EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
              count(sink, MessageKind::kRestorationEntered));
    EXPECT_GE(count_s(sink, MessageKind::kRestorationExited, "locally_infeasible"), 1);
    expect_exit_shape(sink);
    for (const MessageTraceEvent &m : sink.messages) {
        if (m.kind == MessageKind::kRestorationEntered) {
            EXPECT_TRUE(m.iter.has_value());
            ASSERT_TRUE(m.a.has_value());
            EXPECT_GT(*m.a, 0.0) << "theta at entry";
            EXPECT_EQ(m.severity, MessageSeverity::kNotice);
        }
    }
}

TEST(TraceMessagesIpm, AStopInsideRestorationWritesAnInterruptedExit) {
    for (const bool token : {false, true}) {
        SCOPED_TRACE(token ? "token" : "callback");
        Problem problem(std::make_shared<Infeasible>());
        IpmSolver solver(ipm_restoring());
        CaptureSink sink;
        solver.attach_trace(&sink);
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        solver.set_iteration_callback([&](const IterationEvent &e) {
            if (e.recovering) {
                if (token) {
                    source.request_stop();
                    return CallbackAction::kContinue;
                }
                return CallbackAction::kStop;
            }
            return CallbackAction::kContinue;
        });
        const IpmResult r = solver.solve(problem, ones2(), {}, budget);
        ASSERT_EQ(r.status, SolveStatus::kInterrupted);
        EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
                  count(sink, MessageKind::kRestorationEntered));
        EXPECT_EQ(count_s(sink, MessageKind::kRestorationExited, "interrupted"), 1);
        EXPECT_EQ(count(sink, MessageKind::kStopAtIteration), 1);
        EXPECT_EQ(count_s(sink, MessageKind::kStopAtIteration, token ? "token" : "callback"), 1);
    }
}

TEST(TraceMessagesIpm, ANonFiniteEvaluationInsideRestorationWritesANonFiniteExit) {
    Index found = 0;
    for (long from = 3; from <= 60; ++from) {
        auto nlp = std::make_shared<Infeasible>();
        nlp->nan_from = from;
        Problem problem(nlp);
        IpmSolver solver(ipm_restoring());
        CaptureSink sink;
        solver.attach_trace(&sink);
        const IpmResult r = solver.solve(problem, ones2());
        EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
                  count(sink, MessageKind::kRestorationEntered))
            << "from " << from;
        EXPECT_EQ(count(sink, MessageKind::kRestorationEntered), r.counters.restoration_entries)
            << "from " << from;
        expect_exit_shape(sink);
        found += count_s(sink, MessageKind::kRestorationExited, "non_finite");
    }
    EXPECT_GE(found, 1) << "premise: some fault position lands inside restoration";
}

TEST(TraceMessagesIpm, ResumedExitsExistOnTheBatteryAndEveryEntryIsPaired) {
    Index resumed = 0;
    for (const IpmRestorationMode mode :
         {IpmRestorationMode::kL1Nested, IpmRestorationMode::kProximalSwitch}) {
        for (const BatteryCase &c : battery()) {
            for (const double scale : {1.0, 6.0, -5.0}) {
                Problem problem(c.make());
                IpmSolver solver(ipm_restoring(mode));
                CaptureSink sink;
                solver.attach_trace(&sink);
                Vec x0 = c.start();
                x0 *= scale;
                try {
                    const IpmResult r = solver.solve(problem, x0);
                    EXPECT_EQ(count(sink, MessageKind::kRestorationEntered),
                              r.counters.restoration_entries);
                } catch (const Error &) {
                    continue;
                }
                EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
                          count(sink, MessageKind::kRestorationEntered))
                    << c.name << " scale " << scale;
                expect_exit_shape(sink);
                resumed += count_s(sink, MessageKind::kRestorationExited, "resumed");
            }
        }
    }
    EXPECT_GE(resumed, 1) << "premise: the scan reaches a resumed exit";
}

// --- SQP ---------------------------------------------------------------------------------

TEST(TraceMessagesSqp, RestorationEntryExitAndTheElasticLadderOnAnInfeasibleProblem) {
    Problem problem(std::make_shared<Infeasible>());
    SqpSolver solver(sqp_quiet());
    CaptureSink sink;
    solver.attach_trace(&sink);
    const SqpResult r = solver.solve(problem, ones2());
    const Index nonfinite = count_s(sink, MessageKind::kRestorationExited, "non_finite");
    ASSERT_GE(r.counters.restoration_entries, 1);
    EXPECT_EQ(count(sink, MessageKind::kRestorationEntered),
              r.counters.restoration_entries + nonfinite)
        << "the counter counts completed excursions";
    EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
              count(sink, MessageKind::kRestorationEntered));
    EXPECT_GE(count(sink, MessageKind::kElasticExhausted), 1);
    for (const MessageTraceEvent &m : sink.messages) {
        if (m.kind == MessageKind::kElasticExhausted) {
            ASSERT_TRUE(m.k.has_value());
            EXPECT_GE(*m.k, 1);
            EXPECT_EQ(m.severity, MessageSeverity::kWarning);
        }
    }
    expect_exit_shape(sink);
    EXPECT_EQ(count(sink, MessageKind::kStopAtIteration), 0);
}

TEST(TraceMessagesSqp, ARefusedEntryWritesOneRefusalNamingTheRule) {
    {
        SqpOptions o = sqp_quiet();
        o.advanced.max_restoration_entries = 0;
        Problem problem(std::make_shared<Infeasible>());
        SqpSolver solver(o);
        CaptureSink sink;
        solver.attach_trace(&sink);
        const SqpResult r = solver.solve(problem, ones2());
        EXPECT_EQ(r.stop_reason, StopReason::kRestorationRefused);
        EXPECT_EQ(count(sink, MessageKind::kRestorationRefused), 1);
        EXPECT_EQ(count_s(sink, MessageKind::kRestorationRefused, "entry_rule"), 1);
        EXPECT_EQ(count(sink, MessageKind::kRestorationEntered), 0);
    }
    Index budget = 0;
    for (int cap = 1; cap <= 60; ++cap) {
        SqpOptions o = sqp_quiet();
        o.common.max_iterations = cap;
        Problem problem(std::make_shared<Infeasible>());
        SqpSolver solver(o);
        CaptureSink sink;
        solver.attach_trace(&sink);
        solver.solve(problem, ones2());
        budget += count_s(sink, MessageKind::kRestorationRefused, "budget");
        EXPECT_EQ(count(sink, MessageKind::kRestorationExited),
                  count(sink, MessageKind::kRestorationEntered))
            << "cap " << cap;
    }
    EXPECT_GE(budget, 1) << "premise: some iteration cap lands on the restoration budget";
}

TEST(TraceMessagesSqp, SubproblemFailureLinesMatchTheExitCounters) {
    for (const Index budget : {Index{1}, Index{3}}) {
        SqpOptions o = sqp_quiet();
        o.qp.max_iterations = 1;
        o.advanced.max_consecutive_qp_failures = budget;
        Problem problem(std::make_shared<Hs071Problem>());
        SqpSolver solver(o);
        CaptureSink sink;
        solver.attach_trace(&sink);
        const SqpResult r = solver.solve(problem, Vec(hs071_start()));
        const Index expected = r.counters.qp_maxiter_exits + r.counters.qp_numerical_exits +
                               r.counters.nonfinite_subproblems;
        ASSERT_GE(expected, 1) << "premise";
        EXPECT_EQ(count(sink, MessageKind::kSubproblemFailure), expected);
        // The failure that ends the solve is the last one and a warning; routed ones are notices.
        Index warnings = 0;
        for (const MessageTraceEvent &m : sink.messages) {
            if (m.kind == MessageKind::kSubproblemFailure) {
                warnings += m.severity == MessageSeverity::kWarning ? 1 : 0;
                ASSERT_TRUE(m.s.has_value());
                ASSERT_TRUE(m.k.has_value());
            }
        }
        EXPECT_EQ(warnings, r.stop_reason == StopReason::kSubproblemFailure ? 1 : 0);
    }
}

TEST(TraceMessagesSqp, ANonFiniteSubproblemIsOneNonFiniteFailureLine) {
    Problem problem(std::make_shared<PoisonedHs071>());
    SqpSolver solver(sqp_quiet());
    CaptureSink sink;
    solver.attach_trace(&sink);
    const SqpResult r = solver.solve(problem, Vec(hs071_start()));
    ASSERT_GE(r.counters.nonfinite_subproblems, 1);
    EXPECT_EQ(count_s(sink, MessageKind::kSubproblemFailure, "non_finite"),
              r.counters.nonfinite_subproblems);
}

TEST(TraceMessagesSqp, AStartPointOutsideTheBoxIsOneClampLineCarryingTheCounter) {
    {
        Problem problem(std::make_shared<Hs071Problem>());
        SqpSolver solver(sqp_quiet());
        CaptureSink sink;
        solver.attach_trace(&sink);
        const SqpResult r = solver.solve(problem, Vec(Vec::Zero(4)));
        ASSERT_GE(r.counters.start_point_clamps, 1);
        ASSERT_EQ(count(sink, MessageKind::kStartPointClamped), 1);
        for (const MessageTraceEvent &m : sink.messages) {
            if (m.kind == MessageKind::kStartPointClamped) {
                EXPECT_EQ(m.k, r.counters.start_point_clamps);
            }
        }
        EXPECT_FALSE(sink.messages.empty());
        EXPECT_EQ(sink.messages.front().kind, MessageKind::kStartPointClamped);
    }
    {
        Problem problem(std::make_shared<Hs071Problem>());
        SqpSolver solver(sqp_quiet());
        CaptureSink sink;
        solver.attach_trace(&sink);
        const SqpResult r = solver.solve(problem, Vec(hs071_start()));
        EXPECT_EQ(r.counters.start_point_clamps, 0);
        EXPECT_EQ(count(sink, MessageKind::kStartPointClamped), 0);
    }
}

// --- both engines: stop_at_iteration -----------------------------------------------------

TEST(TraceMessagesBoth, StopAtIterationIsOneLineIffTheStopReasonSaysSo) {
    for (const bool ipm : {true, false}) {
        for (const int mode : {0, 1, 2}) { // none, callback, token
            SCOPED_TRACE(std::string(ipm ? "ipm " : "sqp ") + (mode == 0   ? "none"
                                                               : mode == 1 ? "callback"
                                                                           : "token"));
            Problem problem(std::make_shared<Hs071Problem>());
            CaptureSink sink;
            StopSource source;
            SolveBudget budget;
            budget.stop = source.token();
            int seen = 0;
            const auto cb = [&](const IterationEvent &) {
                if (++seen == 3) {
                    if (mode == 2) {
                        source.request_stop();
                    } else if (mode == 1) {
                        return CallbackAction::kStop;
                    }
                }
                return CallbackAction::kContinue;
            };
            StopReason reason = StopReason::kNone;
            if (ipm) {
                IpmOptions o;
                o.common.verbosity = Verbosity::kSilent;
                IpmSolver solver(o);
                solver.attach_trace(&sink);
                solver.set_iteration_callback(cb);
                reason = solver.solve(problem, Vec(hs071_start()), {}, budget).stop_reason;
            } else {
                SqpSolver solver(sqp_quiet());
                solver.attach_trace(&sink);
                solver.set_iteration_callback(cb);
                reason = solver.solve(problem, Vec(hs071_start()), {}, budget).stop_reason;
            }
            const Index n = count(sink, MessageKind::kStopAtIteration);
            EXPECT_EQ(n,
                      (reason == StopReason::kCallbackStop || reason == StopReason::kStopRequested)
                          ? 1
                          : 0);
            EXPECT_EQ(n, mode == 0 ? 0 : 1);
            if (mode != 0) {
                EXPECT_EQ(
                    count_s(sink, MessageKind::kStopAtIteration, mode == 1 ? "callback" : "token"),
                    1);
            }
        }
    }
}

} // namespace
