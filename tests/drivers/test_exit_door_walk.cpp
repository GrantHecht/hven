// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_exit_door_walk.cpp -- M8 TR-B (spec 5.3, M7-19): a walk over every exit door of both engines
// asserting the count identity at each: every record counted in `iterations` has exactly one
// depth-0 `iter` line and one callback event (SQP: row 0 is the start point, and a stop or abort
// door pushes a row without a step), and (TR-D/TR-D') `history` == the depth-0 `iter` stream, row
// for row, on both engines.

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include <hven/core/stop.h>
#include <hven/detail/drivers/return_best.h>
#include <hven/ipm/ipm_solver.h>
#include <hven/sqp/sqp_solver.h>
#include <hven/model/nlp.h>
#include <hven/model/problem.h>

#include "common_support/iteration_record_eq.h"
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
    std::vector<IterationRecord> history;
    std::vector<IterationRecord> stream;
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
        o.common.return_best =
            d.return_best ? hven::ReturnBest::kOnAnyNonOptimal : hven::ReturnBest::kNever;
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
        w.history = r.history;
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
        w.history = r.history;
        w.status = r.status;
        w.stop_reason = r.stop_reason;
    }
    w.rows = sink.depth0_rows();
    for (const CapturedRow &row : sink.rows) {
        if (row.depth == 0) {
            w.stream.push_back(row.record);
        }
    }
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
    ASSERT_EQ(w.history.size(), w.stream.size()) << "history == the depth-0 iter stream";
    for (std::size_t k = 0; k < w.history.size(); ++k) {
        EXPECT_TRUE(hven::test::same_record(w.history[k], w.stream[k])) << "history row " << k;
    }
    if (e == Engine::kIpm) {
        EXPECT_EQ(static_cast<Index>(w.history.size()), w.iterations);
    }
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

// ---------------------------------------------------------------------------
// M8 AP-5 G-RB: `common.return_best`, per value, on the forced exits of both engines.
// ---------------------------------------------------------------------------

struct Forced {
    SolveStatus status = SolveStatus::kOptimal;
    bool substituted = false;
    bool certified = false;
    bool best_published = false;
    std::string message;
};

/// A time-limit solve's callback: every event after the first outlasts the limit, so the limit is
/// read at the next iteration boundary, after the first iterate is tracked. (It arms the tracker
/// too.)
const IterationCallback slow_event = [](const IterationEvent &e) {
    if (e.iteration >= 1) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return CallbackAction::kContinue;
};

/// One solve of `model` on the named engine under `mode`, with an optional work limit, time limit
/// and iteration cap.
Forced forced(Engine e, std::shared_ptr<Nlp> model, const Vec &x0, ReturnBest mode, Index cap = 0,
              std::optional<Index> work = std::nullopt,
              std::optional<double> seconds = std::nullopt, bool nested_restoration = false,
              SolveBudget budget = {}) {
    Problem problem(std::move(model));
    SolveResult r;
    if (e == Engine::kIpm) {
        IpmOptions o;
        o.common.verbosity = Verbosity::kSilent;
        o.common.return_best = mode;
        if (cap > 0) {
            o.common.max_iterations = cap;
        }
        o.common.max_factorizations = work;
        o.common.time_limit_seconds = seconds;
        if (nested_restoration) {
            o.advanced.restoration_mode = IpmRestorationMode::kL1Nested;
            o.advanced.max_restoration_entries = 1;
        }
        IpmSolver solver(o);
        if (seconds) {
            solver.set_iteration_callback(slow_event);
        }
        r = solver.solve(problem, x0, {}, budget);
    } else {
        SqpOptions o;
        o.common.verbosity = Verbosity::kSilent;
        o.common.return_best = mode;
        if (cap > 0) {
            o.common.max_iterations = cap;
        }
        o.common.max_factorizations = work;
        o.common.time_limit_seconds = seconds;
        SqpSolver solver(o);
        if (seconds) {
            solver.set_iteration_callback(slow_event);
        }
        r = solver.solve(problem, x0, {}, budget);
    }
    if (r.returned_best_iterate) {
        EXPECT_TRUE(r.best_iterate.has_value()) << "a returned best implies a published one";
        if (r.best_iterate) {
            EXPECT_TRUE(r.x == r.best_iterate->x) << "the primary point IS the best iterate";
            EXPECT_EQ(r.f, r.best_iterate->f);
        }
    }
    return {r.status, r.returned_best_iterate, r.infeasibility_certified,
            r.best_iterate.has_value(), r.message};
}

constexpr ReturnBest kModes[] = {ReturnBest::kNever, ReturnBest::kOnLimit,
                                 ReturnBest::kOnAnyNonOptimal};

TEST(ReturnBestForcedExits, TheLimitExitsSubstituteUnderOnLimitAndOnAnyNonOptimalOnly) {
    const auto hs071 = [] { return std::shared_ptr<Nlp>(std::make_shared<Hs071Problem>()); };
    const Vec x0 = hs071_start();
    struct Exit {
        const char *name;
        SolveStatus status;
        Index cap;
        std::optional<Index> work;
        std::optional<double> seconds;
    };
    const Exit exits[] = {
        {"iteration_limit", SolveStatus::kIterationLimit, 3, std::nullopt, std::nullopt},
        {"work_limit", SolveStatus::kWorkLimit, 0, Index{2}, std::nullopt},
        {"time_limit", SolveStatus::kTimeLimit, 0, std::nullopt, 0.05},
    };
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        // Warm the backend, so the time limit is not spent in a first-call setup.
        (void)forced(e, hs071(), x0, ReturnBest::kNever);
        for (const Exit &x : exits) {
            for (ReturnBest mode : kModes) {
                SCOPED_TRACE(std::string(name(e)) + " " + x.name + " mode " +
                             std::to_string(static_cast<int>(mode)));
                const Forced f = forced(e, hs071(), x0, mode, x.cap, x.work, x.seconds);
                ASSERT_EQ(f.status, x.status) << "premise: the exit was reached";
                EXPECT_EQ(f.substituted, mode != ReturnBest::kNever)
                    << "published " << f.best_published << ": " << f.message;
                if (!x.seconds) {
                    EXPECT_EQ(f.best_published, mode != ReturnBest::kNever)
                        << "the option arms the tracker; nothing else does here";
                }
            }
        }
    }
}

TEST(ReturnBestForcedExits, ANonFiniteAbortSubstitutesUnderOnAnyNonOptimalOnly) {
    const auto poisoned = [] { return std::shared_ptr<Nlp>(std::make_shared<PoisonedHs071>()); };
    const Vec x0 = hs071_start();
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (ReturnBest mode : kModes) {
            SCOPED_TRACE(std::string(name(e)) + " mode " + std::to_string(static_cast<int>(mode)));
            const Forced f = forced(e, poisoned(), x0, mode);
            ASSERT_EQ(f.status, SolveStatus::kNumericalError) << "premise: the abort door";
            EXPECT_EQ(f.substituted, mode == ReturnBest::kOnAnyNonOptimal);
        }
    }
}

TEST(ReturnBestForcedExits, ACertifiedInfeasibleExitNeverSubstitutes) {
    const auto infeasible = [] { return std::shared_ptr<Nlp>(std::make_shared<Infeasible>()); };
    for (ReturnBest mode : kModes) {
        SCOPED_TRACE("mode " + std::to_string(static_cast<int>(mode)));
        const Forced f = forced(Engine::kSqp, infeasible(), infeasible_start(), mode);
        ASSERT_EQ(f.status, SolveStatus::kInfeasible) << "premise: the infeasible exit";
        EXPECT_FALSE(f.substituted);
    }
}

// M8 AP-5 G-S: the interior-point engine's restoration-locally-infeasible exit reports kInfeasible,
// uncertified, and -- as every kInfeasible exit -- never substitutes the best iterate, whatever
// `return_best` says. The best IS recorded before restoration (published), so the absence of a
// substitution is not vacuous.
TEST(ReturnBestForcedExits, TheIpmRestorationLocallyInfeasibleExitIsInfeasibleAndNeverSubstitutes) {
    const auto infeasible = [] { return std::shared_ptr<Nlp>(std::make_shared<Infeasible>()); };
    for (ReturnBest mode : kModes) {
        SCOPED_TRACE("mode " + std::to_string(static_cast<int>(mode)));
        const Forced f = forced(Engine::kIpm, infeasible(), infeasible_start(), mode, 0,
                                std::nullopt, std::nullopt, true);
        ASSERT_EQ(f.status, SolveStatus::kInfeasible) << f.message;
        EXPECT_FALSE(f.certified) << "uncertified: no claim is made about the model";
        EXPECT_FALSE(f.substituted);
        EXPECT_EQ(f.message,
                  "locally infeasible (uncertified: restoration reached a locally infeasible "
                  "point)");
        EXPECT_EQ(f.best_published, mode != ReturnBest::kNever)
            << "armed by the option; a best was recorded before restoration";
    }
}

TEST(ReturnBestForcedExits, TheProbeBudgetStopKeepsItsOrdinaryExitUnderEveryMode) {
    const auto hs071 = [] { return std::shared_ptr<Nlp>(std::make_shared<Hs071Problem>()); };
    for (ReturnBest mode : kModes) {
        SolveBudget budget;
        budget.max_minor_iterations = 1;
        const Forced f = forced(Engine::kSqp, hs071(), hs071_start(), mode, 0, std::nullopt,
                                std::nullopt, false, budget);
        ASSERT_NE(f.status, SolveStatus::kOptimal) << "premise: the budget ran out";
        EXPECT_FALSE(f.substituted) << "the probe budget is a per-call contract, not a limit";
    }
}

TEST(ReturnBestForcedExits, TheSharedRuleNamesEveryStatus) {
    using detail::return_best_applies;
    for (SolveStatus s :
         {SolveStatus::kOptimal, SolveStatus::kInfeasible, SolveStatus::kInterrupted}) {
        for (ReturnBest mode : kModes) {
            EXPECT_FALSE(return_best_applies(mode, s));
        }
    }
    for (SolveStatus s :
         {SolveStatus::kIterationLimit, SolveStatus::kWorkLimit, SolveStatus::kTimeLimit}) {
        EXPECT_FALSE(return_best_applies(ReturnBest::kNever, s));
        EXPECT_TRUE(return_best_applies(ReturnBest::kOnLimit, s));
        EXPECT_TRUE(return_best_applies(ReturnBest::kOnAnyNonOptimal, s));
    }
    for (SolveStatus s : {SolveStatus::kAcceptable, SolveStatus::kStalled, SolveStatus::kDiverging,
                          SolveStatus::kNumericalError}) {
        EXPECT_FALSE(return_best_applies(ReturnBest::kNever, s));
        EXPECT_FALSE(return_best_applies(ReturnBest::kOnLimit, s));
        EXPECT_TRUE(return_best_applies(ReturnBest::kOnAnyNonOptimal, s));
    }
}

TEST(ReturnBestForcedExits, TheSharedOrderIsFeasibilityFirstBandedStrictAndObjectiveSecond) {
    using detail::best_improves;
    const double tol = 1e-6;
    // Outside the band, the smaller violation wins whatever the objective.
    EXPECT_TRUE(best_improves(1e-3, 100.0, 1e-2, -100.0, tol));
    EXPECT_FALSE(best_improves(1e-2, -100.0, 1e-3, 100.0, tol));
    // Inside the band the violations tie and the objective decides, both ways.
    EXPECT_TRUE(best_improves(5e-7, 1.0, 1e-9, 2.0, tol));
    EXPECT_FALSE(best_improves(1e-9, 2.0, 5e-7, 1.0, tol));
    // Strict: an equal key does not replace the incumbent, inside or outside the band.
    EXPECT_FALSE(best_improves(0.0, 1.0, 0.0, 1.0, tol));
    EXPECT_FALSE(best_improves(1e-3, 1.0, 1e-3, 1.0, tol));
    // A band point beats any point outside it, however good that point's objective.
    EXPECT_TRUE(best_improves(0.0, 1e9, 2e-6, -1e9, tol));
    // NaN never wins a violation comparison; a NaN objective loses a band tie.
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_FALSE(best_improves(nan, 0.0, 1.0, 0.0, tol));
    EXPECT_TRUE(best_improves(0.0, 5.0, 0.0, nan, tol));
    EXPECT_FALSE(best_improves(0.0, nan, 0.0, 5.0, tol));
}

TEST(ReturnBestForcedExits, TheOptionIsOffByDefaultAndValidated) {
    EXPECT_EQ(CommonOptions{}.return_best, ReturnBest::kNever);
    IpmOptions o;
    o.common.return_best = static_cast<ReturnBest>(3);
    EXPECT_THROW(validate(o), std::invalid_argument);
}

} // namespace
