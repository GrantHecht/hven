// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_exit_door_walk.cpp -- M8 TR-B (spec 5.3, M7-19): a walk over every exit door of both engines
// asserting the count identity at each: every record counted in `iterations` has exactly one
// depth-0 `iter` line and one callback event (SQP: iterations + 1 lines, row 0 is the start point,
// and one event per line).

#include <gtest/gtest.h>

#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <hven/core/stop.h>
#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "support/hs071_problem.h"
#include "support/trace_battery.h"

namespace {

using namespace hven;
using namespace hven_drivers_tests;

enum class Engine { kIpm, kSqp };
const char *name(Engine e) { return e == Engine::kIpm ? "ipm" : "sqp"; }

/// HS071 whose Hessian turns NaN from the `from`-th call on: every later point faults, so the solve
/// ends on the non-finite evaluation door.
struct PoisonedHs071 final : Hs071Problem {
    long from = 4;
    mutable long calls = 0;
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
        Hs071Problem::eval_hessian(x, obj_factor, lambda, v);
        if (++calls >= from) {
            v[2] = std::numeric_limits<double>::quiet_NaN();
        }
    }
};

/// min x0 + x1 s.t. x0^2 + x1^2 + 1 = 0: infeasible, so restoration is entered.
struct Infeasible final : Nlp {
    NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(ConstVecRef x) const override { return x[0] + x[1]; }
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

Vec infeasible_start() {
    Vec x(2);
    x << 1.0, 0.5;
    return x;
}

struct Door {
    const char *name;
    std::function<std::shared_ptr<Nlp>()> make;
    std::function<Vec()> start;
    /// The 0-based event index a callback stop or a token request lands on; -1 none; -2 the last
    /// event.
    int stop_event = -1;
    bool token = false;
    int max_iterations = 0; // 0: default
    bool restoration = false;
    bool return_best = true;
    /// A premise the door must have reached (a status or a substring of the message / stop reason).
    std::function<bool(SolveStatus, StopReason)> reached;
};

struct Walked {
    Index rows = 0;
    Index events = 0;
    Index iterations = 0;
    SolveStatus status = SolveStatus::kOptimal;
    StopReason stop_reason = StopReason::kNone;
    std::vector<MessageTraceEvent> messages;
};

Walked walk(Engine e, const Door &d, int stop_event) {
    Problem problem(d.make());
    CaptureSink sink;
    Walked w;
    StopSource source;
    SolveBudget budget;
    if (d.token) {
        budget.stop = source.token();
    }
    int seen = 0;
    const auto cb = [&](const IterationEvent &) {
        const int at = seen++;
        ++w.events;
        if (stop_event >= 0 && at == stop_event) {
            if (d.token) {
                source.request_stop();
                return CallbackAction::kContinue;
            }
            return CallbackAction::kStop;
        }
        return CallbackAction::kContinue;
    };
    if (e == Engine::kIpm) {
        IpmOptions o;
        o.common.verbosity = Verbosity::kSilent;
        o.return_best = d.return_best;
        if (d.max_iterations > 0) {
            o.common.max_iterations = d.max_iterations;
        }
        if (d.restoration) {
            o.advanced.restoration_mode = IpmRestorationMode::kL1Nested;
            o.advanced.max_restoration_entries = 1;
        }
        IpmSolver solver(o);
        solver.attach_trace(&sink);
        solver.set_iteration_callback(cb);
        const IpmResult r = solver.solve(problem, d.start(), {}, budget);
        w.iterations = r.iterations;
        w.status = r.status;
        w.stop_reason = r.stop_reason;
    } else {
        SqpOptions o;
        o.common.verbosity = Verbosity::kSilent;
        if (d.max_iterations > 0) {
            o.common.max_iterations = d.max_iterations;
        }
        SqpSolver solver(o);
        solver.attach_trace(&sink);
        solver.set_iteration_callback(cb);
        const SqpResult r = solver.solve(problem, d.start(), {}, budget);
        w.iterations = static_cast<Index>(r.history.size());
        w.status = r.status;
        w.stop_reason = r.stop_reason;
    }
    w.rows = sink.depth0_rows();
    w.messages = sink.messages;
    return w;
}

std::vector<Door> doors() {
    const auto hs071 = [] { return std::shared_ptr<Nlp>(std::make_shared<Hs071Problem>()); };
    const auto poisoned = [] { return std::shared_ptr<Nlp>(std::make_shared<PoisonedHs071>()); };
    const auto hs071_x0 = [] { return Vec(hs071_start()); };
    const auto any = [](SolveStatus, StopReason) { return true; };
    std::vector<Door> v;
    v.push_back({"nonfinite_abort", poisoned, hs071_x0, -1, false, 0, false, true,
                 [](SolveStatus s, StopReason r) {
                     return s == SolveStatus::kNumericalError && r == StopReason::kNonFiniteIterate;
                 }});
    v.push_back({"nonfinite_abort_no_best", poisoned, hs071_x0, -1, false, 0, false, false,
                 [](SolveStatus s, StopReason r) {
                     return s == SolveStatus::kNumericalError && r == StopReason::kNonFiniteIterate;
                 }});
    v.push_back({"callback_stop", hs071, hs071_x0, 3, false, 0, false, true,
                 [](SolveStatus s, StopReason r) {
                     return s == SolveStatus::kInterrupted && r == StopReason::kCallbackStop;
                 }});
    v.push_back(
        {"token_stop", hs071, hs071_x0, 3, true, 0, false, true, [](SolveStatus s, StopReason r) {
             return s == SolveStatus::kInterrupted && r == StopReason::kStopRequested;
         }});
    v.push_back({"iteration_limit", hs071, hs071_x0, -1, false, 3, false, true,
                 [](SolveStatus s, StopReason) { return s == SolveStatus::kIterationLimit; }});
    v.push_back({"last_event_stop", hs071, hs071_x0, -2, false, 0, false, true, any});
    v.push_back({"infeasible_restoration",
                 [] { return std::shared_ptr<Nlp>(std::make_shared<Infeasible>()); },
                 [] { return infeasible_start(); }, -1, false, 0, true, true, any});
    return v;
}

void expect_identity(Engine e, const Door &d, const Walked &w) {
    EXPECT_EQ(w.rows, w.iterations) << "one depth-0 iter line per counted record";
    EXPECT_EQ(w.events, w.rows) << "one callback event per depth-0 iter line";
    if (e == Engine::kIpm && std::string_view(d.name) == "infeasible_restoration") {
        bool locally_infeasible = false;
        for (const MessageTraceEvent &m : w.messages) {
            locally_infeasible = locally_infeasible || (m.kind == MessageKind::kRestorationExited &&
                                                        m.s && *m.s == "locally_infeasible");
        }
        EXPECT_TRUE(locally_infeasible) << "premise: the locally infeasible door was not reached";
    }
    EXPECT_TRUE(d.reached(w.status, w.stop_reason))
        << "premise: the door was not reached (status " << static_cast<int>(w.status) << ")";
}

TEST(ExitDoorWalk, EveryExitDoorOfBothEnginesKeepsTheCountIdentity) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (const Door &d : doors()) {
            if (e == Engine::kSqp &&
                (d.restoration || std::string_view(d.name) == "nonfinite_abort_no_best")) {
                continue;
            }
            SCOPED_TRACE(std::string(name(e)) + " " + d.name);
            int stop_event = d.stop_event;
            if (stop_event == -2) {
                // The last event: a late stop on a converged phase (the interrupted-point door).
                stop_event = static_cast<int>(walk(e, d, -1).events) - 1;
                ASSERT_GE(stop_event, 0);
            }
            expect_identity(e, d, walk(e, d, stop_event));
        }
    }
}

} // namespace
