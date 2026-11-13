// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-5 G1: the cooperative stop token (core/stop.h, SolveBudget::stop) and the
// one StopReason vocabulary, on both engines. The cases are the interface
// design's test plan rows (docs/notes/2026-11-m8-ap5-interface-design.md §3.6):
//
//   T1  a token stopped before solve() -> kInterrupted at the start, no step
//       taken (SQP 0 iterations; IPM 1, its start row), kStopRequested; the
//       start is NOT converged (a converged start would outrank the stop).
//   T2  a cross-thread stop: the callback at event k signals a second thread,
//       which calls request_stop() and acknowledges; the returned x is the last
//       delivered event's x. Deterministic (std::latch handshake, no sleeps, no
//       jthread). Also the ThreadSanitizer case.
//   T6  converged beats stop: a token requested on the converging row -> kOptimal.
//   T8  precedence at one boundary: a stop with a work limit and a time limit
//       also reached -> kInterrupted.
//   T10 N solvers on N threads, one stopped, the others equal to serial.
//   T13 multi-phase IPM: a token ends the sequence; the remaining phases have
//       ran == false.
//
// The orchestrator's ruling (G1): a token stop has its OWN console message
// kinds, distinct from the callback's interrupt text -- pinned below on the
// console sink for both stops, so neither borrows the other's words.
//
// An unfired token is free on the trajectory: the AReadOnly... style identity
// for an armed-but-unfired token is pinned here too (both engines, counter for
// counter on the shared result).

#include <gtest/gtest.h>

#include <atomic>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <latch>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <Eigen/Core>

#include "hven/core/stop.h"
#include "hven/drivers/console_trace_sink.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/model/problem.h"

#include "support/hs071_problem.h"

namespace {

using hven::CallbackAction;
using hven::Index;
using hven::IpmOptions;
using hven::IpmResult;
using hven::IpmSolver;
using hven::IterationEvent;
using hven::Problem;
using hven::SolveBudget;
using hven::SolveResult;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpResult;
using hven::SqpSolver;
using hven::StopReason;
using hven::StopSource;
using hven::StopToken;
using hven::Verbosity;
using hven_drivers_tests::hs071_start;
using hven_drivers_tests::Hs071Problem;

enum class Engine { kIpm, kSqp };

const char *name(Engine e) { return e == Engine::kIpm ? "ipm" : "sqp"; }

IpmOptions quiet_ipm() {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

SqpOptions quiet_sqp() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    return o;
}

bool bit_equal(const Eigen::VectorXd &a, const Eigen::VectorXd &b) {
    if (a.size() != b.size()) {
        return false;
    }
    for (Eigen::Index i = 0; i < a.size(); ++i) {
        if (std::bit_cast<std::uint64_t>(a[i]) != std::bit_cast<std::uint64_t>(b[i])) {
            return false;
        }
    }
    return true;
}

// One solve on a fresh solver of the named engine, `cb` installed as the
// iteration callback; the result sliced to the shared core.
template <class Callback>
SolveResult solve_once(Engine e, SolveBudget budget, Callback &&cb,
                       std::optional<IpmOptions> ipm = std::nullopt,
                       std::optional<SqpOptions> sqp = std::nullopt) {
    Problem problem(std::make_shared<Hs071Problem>());
    if (e == Engine::kIpm) {
        IpmSolver solver(ipm ? *ipm : quiet_ipm());
        solver.set_iteration_callback(cb);
        return solver.solve(problem, hs071_start(), {}, budget);
    }
    SqpSolver solver(sqp ? *sqp : quiet_sqp());
    solver.set_iteration_callback(cb);
    return solver.solve(problem, hs071_start(), {}, budget);
}

SolveResult solve_plain(Engine e, SolveBudget budget = {},
                        std::optional<IpmOptions> ipm = std::nullopt,
                        std::optional<SqpOptions> sqp = std::nullopt) {
    Problem problem(std::make_shared<Hs071Problem>());
    if (e == Engine::kIpm) {
        IpmSolver solver(ipm ? *ipm : quiet_ipm());
        return solver.solve(problem, hs071_start(), {}, budget);
    }
    SqpSolver solver(sqp ? *sqp : quiet_sqp());
    return solver.solve(problem, hs071_start(), {}, budget);
}

} // namespace

// ---------------------------------------------------------------------------
// The type itself.
// ---------------------------------------------------------------------------

TEST(StopToken, AnEmptyTokenHasNoSourceAndIsNeverRequested) {
    const StopToken empty;
    EXPECT_FALSE(empty.stop_possible());
    EXPECT_FALSE(empty.stop_requested());
    const SolveBudget budget;
    EXPECT_FALSE(budget.stop.stop_possible()) << "the SolveBudget default carries no source";
}

TEST(StopToken, EveryTokenOfASourceSeesItsStop) {
    StopSource source;
    const StopToken a = source.token();
    const StopToken b = a; // a copy reads the same flag
    EXPECT_TRUE(a.stop_possible());
    EXPECT_FALSE(a.stop_requested());
    EXPECT_FALSE(source.stop_requested());
    source.request_stop();
    EXPECT_TRUE(a.stop_requested());
    EXPECT_TRUE(b.stop_requested());
    EXPECT_TRUE(source.stop_requested());
    source.request_stop(); // idempotent
    EXPECT_TRUE(a.stop_requested());
    StopSource other;
    EXPECT_FALSE(other.token().stop_requested()) << "sources are independent";
}

// ---------------------------------------------------------------------------
// T1: a token stopped before solve().
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T1ATokenStoppedBeforeTheSolveEndsItAtTheStart) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        // PREMISE: the start is not converged, so the stop is what ends the solve.
        const SolveResult free_run = solve_plain(e);
        ASSERT_EQ(free_run.status, SolveStatus::kOptimal);
        ASSERT_GT(free_run.iterations, 0) << "a converged start would outrank the stop";

        // The point a callback stop on the FIRST event returns: the start, as the
        // engine first describes it.
        std::optional<Eigen::VectorXd> first_x;
        const SolveResult cb_stop = solve_once(e, {}, [&](const IterationEvent &ev) {
            if (!first_x) {
                first_x = Eigen::VectorXd(ev.x);
            }
            return CallbackAction::kStop;
        });
        ASSERT_TRUE(first_x.has_value());

        StopSource source;
        source.request_stop();
        SolveBudget budget;
        budget.stop = source.token();
        const SolveResult r = solve_plain(e, budget);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        // NO STEP TAKEN. The engines count differently, as SolveResult::iterations
        // documents: the SQP counts subproblems solved (0); the interior-point
        // engine counts its iteration ROWS, the start row included (1 -- the
        // same count a callback kStop on the first event reports, one event per
        // row).
        EXPECT_EQ(r.iterations, e == Engine::kIpm ? 1 : 0);
        if (e == Engine::kIpm) {
            // The IPM's first event is the start row, delivered before any
            // factorization of the loop: the token stop at the first iteration
            // top spends exactly what a callback stop on that event spends.
            EXPECT_EQ(r.iterations, cb_stop.iterations);
            EXPECT_EQ(r.work.factorizations, cb_stop.work.factorizations)
                << "the token stop spends exactly what the first event's callback stop spends";
        } else {
            // The SQP's first event is row 0, delivered AFTER the first
            // subproblem was solved (that row records the accepted step the
            // callback's kStop then declines, Q15): the callback stop has paid
            // for one subproblem. The token is polled at the exit conjunction
            // BEFORE the first subproblem, so it pays for none -- the same point
            // (asserted below), strictly less work.
            EXPECT_EQ(cb_stop.iterations, 1);
            EXPECT_EQ(r.work.factorizations, 0);
            EXPECT_LT(r.work.factorizations, cb_stop.work.factorizations);
        }
        EXPECT_EQ(r.message, "interrupted by a stop request (SolveBudget::stop)");
        EXPECT_TRUE(bit_equal(r.x, *first_x)) << "the start, as the first event describes it";
        EXPECT_TRUE(bit_equal(r.x, cb_stop.x));
        EXPECT_FALSE(r.returned_best_iterate);
    }
}

// ---------------------------------------------------------------------------
// T2: a stop requested from another thread.
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T2ACrossThreadStopReturnsTheLastDeliveredEventsPoint) {
    constexpr Index kSignalAt = 2;
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();

        std::latch signal(1); // the callback -> the stopper: "stop now"
        std::latch acked(1);  // the stopper -> the callback: "requested"
        std::thread stopper([&] {
            signal.wait();
            source.request_stop();
            acked.count_down();
        });

        std::vector<Eigen::VectorXd> seen;
        bool signalled = false;
        const SolveResult r = solve_once(e, budget, [&](const IterationEvent &ev) {
            seen.emplace_back(ev.x);
            if (!signalled && ev.iteration == kSignalAt) {
                signalled = true;
                signal.count_down();
                acked.wait(); // deterministic: the stop is requested before this returns
            }
            return CallbackAction::kContinue;
        });
        if (!signalled) {
            signal.count_down(); // never leave the stopper waiting
        }
        stopper.join();

        ASSERT_TRUE(signalled) << "fixture premise: the solve reached event " << kSignalAt;
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        ASSERT_FALSE(seen.empty());
        EXPECT_TRUE(bit_equal(r.x, seen.back())) << "the returned x is the last event's x";
        if (e == Engine::kIpm) {
            // The IPM polls right after the event, before its factorization: the
            // signalling event is the last one.
            EXPECT_EQ(static_cast<Index>(seen.size()), kSignalAt + 1);
        } else {
            // The SQP polls right after the row is recorded (an accepted step is
            // not committed) or, after any other row, at the next exit test,
            // whose terminal row describes the same point. Either way no
            // subproblem is solved after the request.
            EXPECT_LE(static_cast<Index>(seen.size()), kSignalAt + 2);
        }
    }
}

// ---------------------------------------------------------------------------
// T6: converged beats stop.
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T6ATokenRequestedOnTheConvergingRowLosesToConvergence) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        Index last_event = -1;
        Index events = 0;
        const SolveResult reference = solve_once(e, {}, [&](const IterationEvent &ev) {
            last_event = ev.iteration;
            ++events;
            return CallbackAction::kContinue;
        });
        ASSERT_EQ(reference.status, SolveStatus::kOptimal);
        ASSERT_GT(last_event, 0);

        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        Index count = 0;
        const SolveResult r = solve_once(e, budget, [&](const IterationEvent &) {
            if (++count == events) {
                source.request_stop(); // on the converging row's own event
            }
            return CallbackAction::kContinue;
        });
        EXPECT_TRUE(source.stop_requested()) << "the stop really was requested";
        EXPECT_EQ(r.status, SolveStatus::kOptimal) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kNone);
        EXPECT_EQ(r.iterations, reference.iterations);
        EXPECT_TRUE(bit_equal(r.x, reference.x));
    }
}

// ---------------------------------------------------------------------------
// T8: the stop outranks a work limit and a time limit reached at the same
// boundary (converged > interrupted > work > time > cap).
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T8AStopOutranksAWorkAndATimeLimitAtTheSameBoundary) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        IpmOptions io = quiet_ipm();
        SqpOptions so = quiet_sqp();
        // Both limits reached by the first boundary: one factorization at most,
        // and a deadline far shorter than any solve.
        io.common.max_factorizations = 1;
        io.common.time_limit_seconds = 1e-12;
        so.common.max_factorizations = 1;
        so.common.time_limit_seconds = 1e-12;

        // PREMISE: without the stop, the limits end the solve.
        const SolveResult limited = solve_plain(e, {}, io, so);
        ASSERT_TRUE(limited.status == SolveStatus::kWorkLimit ||
                    limited.status == SolveStatus::kTimeLimit)
            << limited.message;
        EXPECT_EQ(limited.stop_reason, StopReason::kNone) << "the status names a limit";

        StopSource source;
        source.request_stop();
        SolveBudget budget;
        budget.stop = source.token();
        const SolveResult r = solve_plain(e, budget, io, so);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_TRUE(bit_equal(r.x, limited.x)) << "the same boundary, a different label";
    }
}

// ---------------------------------------------------------------------------
// An armed token that never fires changes nothing (both engines).
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, AnUnfiredTokenChangesNothing) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        const SolveResult plain = solve_plain(e);
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        const SolveResult armed = solve_plain(e, budget);
        EXPECT_EQ(armed.status, plain.status);
        EXPECT_EQ(armed.stop_reason, plain.stop_reason);
        EXPECT_EQ(armed.iterations, plain.iterations);
        EXPECT_EQ(armed.work.factorizations, plain.work.factorizations);
        EXPECT_EQ(armed.work.objective_evals, plain.work.objective_evals);
        EXPECT_TRUE(bit_equal(armed.x, plain.x));
        EXPECT_TRUE(bit_equal(armed.lambda, plain.lambda));
    }
}

// ---------------------------------------------------------------------------
// T10: N solvers on N threads, one stopped, the others equal to serial.
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T10OneOfNConcurrentSolvesStopsAndTheOthersMatchSerial) {
    constexpr int kThreads = 4;
    constexpr int kStopped = 1;
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        IpmOptions io = quiet_ipm();
        io.common.threads = 1;
        SqpOptions so = quiet_sqp();
        so.common.threads = 1;
        const SolveResult serial = solve_plain(e, {}, io, so);
        ASSERT_EQ(serial.status, SolveStatus::kOptimal);

        std::vector<StopSource> sources(kThreads);
        sources[kStopped].request_stop();
        std::vector<SolveResult> results(kThreads);
        std::vector<std::thread> threads;
        threads.reserve(kThreads);
        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&, t] {
                SolveBudget budget;
                budget.stop = sources[t].token();
                results[t] = solve_plain(e, budget, io, so);
            });
        }
        for (std::thread &th : threads) {
            th.join();
        }
        for (int t = 0; t < kThreads; ++t) {
            SCOPED_TRACE(t);
            if (t == kStopped) {
                EXPECT_EQ(results[t].status, SolveStatus::kInterrupted);
                EXPECT_EQ(results[t].stop_reason, StopReason::kStopRequested);
                // No step taken (the IPM counts the start row; see T1).
                EXPECT_EQ(results[t].iterations, e == Engine::kIpm ? 1 : 0);
                continue;
            }
            // Counters and statuses exact; the point through the concurrency
            // suite's gate (MKL is address-sensitive in the last digits).
            EXPECT_EQ(results[t].status, serial.status);
            EXPECT_EQ(results[t].stop_reason, StopReason::kNone);
            EXPECT_EQ(results[t].iterations, serial.iterations);
            EXPECT_EQ(results[t].work.factorizations, serial.work.factorizations);
            ASSERT_EQ(results[t].x.size(), serial.x.size());
            for (Eigen::Index i = 0; i < serial.x.size(); ++i) {
                const double scale = std::max(std::abs(serial.x[i]), std::abs(results[t].x[i]));
                EXPECT_LE(std::abs(results[t].x[i] - serial.x[i]),
                          std::max(1.0e-5 * scale, 1.0e-13))
                    << i;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// T13: a token ends the interior-point phase sequence.
// ---------------------------------------------------------------------------

TEST(StopTokenSolve, T13ATokenEndsTheIpmPhaseSequence) {
    IpmOptions o = quiet_ipm();
    o.phases = {hven::IpmPhase::kSolve, hven::IpmPhase::kOptimize};
    Problem problem(std::make_shared<Hs071Problem>());
    IpmSolver solver(o);
    StopSource source;
    SolveBudget budget;
    budget.stop = source.token();
    std::vector<Eigen::VectorXd> seen;
    solver.set_iteration_callback([&](const IterationEvent &ev) {
        seen.emplace_back(ev.x);
        if (seen.size() == 2) {
            source.request_stop(); // in phase 0, on a committed iterate
        }
        return CallbackAction::kContinue;
    });
    const IpmResult r = solver.solve(problem, hs071_start(), {}, budget);

    EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
    EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
    EXPECT_EQ(seen.size(), 2u) << "the solve stopped at the iteration whose event requested it";
    ASSERT_FALSE(seen.empty());
    EXPECT_TRUE(bit_equal(r.x, seen.back()));
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_EQ(r.phases[0].status, SolveStatus::kInterrupted);
    EXPECT_EQ(r.phases[0].stop_reason, StopReason::kStopRequested);
    EXPECT_FALSE(r.phases[1].ran) << "a stop ends the phase SEQUENCE, not only the phase";
    EXPECT_EQ(r.phases[1].stop_reason, StopReason::kNone);
}

// ---------------------------------------------------------------------------
// The console's words: a token stop has its own message kinds.
// ---------------------------------------------------------------------------

namespace {

// A two-phase interior-point solve stopped on its second event in phase 0,
// by the token (`by_token`) or by the callback, with a console sink attached;
// returns everything the sink printed.
std::string console_of_a_stopped_two_phase_solve(bool by_token) {
    IpmOptions o = quiet_ipm();
    o.phases = {hven::IpmPhase::kSolve, hven::IpmPhase::kOptimize};
    Problem problem(std::make_shared<Hs071Problem>());
    IpmSolver solver(o);
    StopSource source;
    SolveBudget budget;
    budget.stop = source.token();
    int events = 0;
    solver.set_iteration_callback([&](const IterationEvent &) {
        if (++events == 2) {
            if (!by_token) {
                return CallbackAction::kStop;
            }
            source.request_stop();
        }
        return CallbackAction::kContinue;
    });
    std::FILE *f = std::tmpfile();
    EXPECT_NE(f, nullptr);
    if (f == nullptr) {
        return {};
    }
    std::string text;
    {
        hven::ConsoleTraceSink sink(
            hven::ConsoleTraceSink::Format{false, hven::Verbosity::kIterations}, f);
        solver.attach_trace(&sink);
        const IpmResult r = solver.solve(problem, hs071_start(), {}, budget);
        solver.attach_trace(nullptr);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, by_token ? StopReason::kStopRequested : StopReason::kCallbackStop);
        std::fflush(f);
        std::rewind(f);
        char buf[4096];
        std::size_t n = 0;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
            text.append(buf, n);
        }
    }
    std::fclose(f);
    return text;
}

} // namespace

TEST(StopTokenSolve, ATokenStopPrintsItsOwnConsoleMessagesNotTheCallbacks) {
    const std::string printed = console_of_a_stopped_two_phase_solve(/*by_token=*/true);
    EXPECT_NE(printed.find("Solve stopped by a stop request (SolveBudget::stop) at iteration "),
              std::string::npos)
        << printed;
    EXPECT_NE(printed.find("Solve stopped by a stop request (SolveBudget::stop); skipping "
                           "remaining phases."),
              std::string::npos)
        << printed;
    EXPECT_EQ(printed.find("iteration callback"), std::string::npos)
        << "a token stop never borrows the callback's words\n"
        << printed;
}

TEST(StopTokenSolve, ACallbackStopKeepsItsOwnConsoleMessages) {
    const std::string printed = console_of_a_stopped_two_phase_solve(/*by_token=*/false);
    EXPECT_NE(printed.find("Solve interrupted by the iteration callback at iteration "),
              std::string::npos)
        << printed;
    EXPECT_NE(printed.find("Solve interrupted by the iteration callback; skipping remaining "
                           "phases."),
              std::string::npos)
        << printed;
    EXPECT_EQ(printed.find("stop request"), std::string::npos) << printed;
}
