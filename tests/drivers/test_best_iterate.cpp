// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-5 G3: best-so-far and the kStopReturnBest action, on both engines
// (docs/notes/2026-11-m8-ap5-interface-design.md sections 3.3 and 3.6):
//
//   T7  kStopReturnBest -> the primary point IS best_iterate and
//       returned_best_iterate is set; a stop at an improving IPM point returns
//       that point as best (stop-path tracking). The empty-tracker fallback (the
//       shown point, returned_best_iterate false) is a defence no fixture reaches:
//       the first finite iterate of a caller's solve is always recorded.
//   T9  armed-and-unfired == unarmed, counter for counter, on a non-converging
//       exit (where best_iterate is built) and through the restoration-entering
//       IPM fixtures; the SQP restoration half is in test_callback.cpp.
//   T12 snapshot(): a deep copy that survives the event and the solve.
//
// best_iterate is checked against the engines' OWN established best paths: on a
// solve that returns its best (IpmOptions::return_best,
// SqpOptions::return_best_on_max_iterations) the primary fields and best_iterate
// come from different code and must agree bit for bit.

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <Eigen/Core>

#include "hven/core/stop.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

#include "support/hs071_problem.h"

namespace {

using hven::CallbackAction;
using hven::Index;
using hven::IpmOptions;
using hven::IpmResult;
using hven::IpmSolver;
using hven::IterationEvent;
using hven::IterationPayload;
using hven::IterationSnapshot;
using hven::Problem;
using hven::snapshot;
using hven::SolveBudget;
using hven::SolveResult;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpResult;
using hven::SqpSolver;
using hven::StopReason;
using hven::StopSource;
using hven::Verbosity;
using hven_drivers_tests::hs071_start;
using hven_drivers_tests::Hs071Problem;

enum class Engine { kIpm, kSqp };

const char *name(Engine e) { return e == Engine::kIpm ? "ipm" : "sqp"; }

constexpr Engine kEngines[] = {Engine::kIpm, Engine::kSqp};

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

bool same_scalar(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

// One solve on a fresh solver of the named engine. `cap` caps the major count
// (0: the default cap); `return_best` turns the engine's own option on; `cb`, when
// engaged, is installed as the iteration callback.
SolveResult solve(Engine e, Index cap, bool return_best,
                  const hven::IterationCallback &cb = nullptr, SolveBudget budget = {}) {
    Problem problem(std::make_shared<Hs071Problem>());
    if (e == Engine::kIpm) {
        IpmOptions o = quiet_ipm();
        o.return_best = return_best;
        if (cap > 0) {
            o.common.max_iterations = cap;
        }
        IpmSolver solver(o);
        if (cb) {
            solver.set_iteration_callback(cb);
        }
        return solver.solve(problem, hs071_start(), {}, budget);
    }
    SqpOptions o = quiet_sqp();
    o.return_best_on_max_iterations = return_best;
    if (cap > 0) {
        o.common.max_iterations = cap;
    }
    SqpSolver solver(o);
    if (cb) {
        solver.set_iteration_callback(cb);
    }
    return solver.solve(problem, hs071_start(), {}, budget);
}

const hven::IterationCallback kReadOnly = [](const IterationEvent &) {
    return CallbackAction::kContinue;
};

// The primary point of `r` and `best` are the same point, field for field.
void expect_primary_is(const SolveResult &r, const IterationSnapshot &best) {
    EXPECT_TRUE(bit_equal(r.x, best.x));
    EXPECT_TRUE(bit_equal(r.lambda, best.lambda));
    EXPECT_TRUE(bit_equal(r.z, best.z));
    EXPECT_TRUE(bit_equal(r.g, best.g));
    EXPECT_TRUE(same_scalar(r.f, best.f)) << r.f << " vs " << best.f;
    EXPECT_TRUE(same_scalar(r.stationarity, best.stationarity))
        << r.stationarity << " vs " << best.stationarity;
    EXPECT_TRUE(same_scalar(r.feasibility, best.feasibility))
        << r.feasibility << " vs " << best.feasibility;
    EXPECT_TRUE(same_scalar(r.complementarity, best.complementarity))
        << r.complementarity << " vs " << best.complementarity;
}

} // namespace

// ---------------------------------------------------------------------------
// T7: kStopReturnBest.
// ---------------------------------------------------------------------------

TEST(BestIterate, T7aAStopReturnBestOnTheFirstEventReturnsThatPointAsBest) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        std::optional<IterationSnapshot> first;
        const SolveResult r = solve(e, 0, false, [&](const IterationEvent &ev) {
            if (ev.iteration != 0) {
                return CallbackAction::kContinue;
            }
            first = snapshot(ev);
            return CallbackAction::kStopReturnBest;
        });
        ASSERT_TRUE(first.has_value());
        EXPECT_EQ(r.status, SolveStatus::kInterrupted);
        EXPECT_EQ(r.stop_reason, StopReason::kCallbackStop);
        EXPECT_TRUE(r.returned_best_iterate) << r.message;
        EXPECT_FALSE(r.returned_pre_restoration_point);
        ASSERT_TRUE(r.best_iterate.has_value());
        const IterationSnapshot &best = *r.best_iterate;
        // The stop-path tracking: the first event's point is the best, on the IPM
        // although the ordinary tracker had not yet run.
        EXPECT_EQ(best.iteration, 0);
        EXPECT_FALSE(best.recovering);
        EXPECT_TRUE(bit_equal(best.x, first->x));
        EXPECT_TRUE(bit_equal(best.lambda, first->lambda));
        EXPECT_TRUE(bit_equal(best.z, first->z));
        EXPECT_TRUE(same_scalar(best.f, first->f));
        EXPECT_EQ(best.g.size(), 2);
        expect_primary_is(r, best);
        EXPECT_NE(r.message.find("returned the best iterate"), std::string::npos) << r.message;
    }
}

TEST(BestIterate, T7bAStopReturnBestAfterSeveralEventsReturnsTheBestOfThem) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        std::vector<IterationSnapshot> seen;
        const SolveResult r = solve(e, 0, false, [&](const IterationEvent &ev) {
            seen.push_back(snapshot(ev));
            return ev.iteration == 3 ? CallbackAction::kStopReturnBest : CallbackAction::kContinue;
        });
        ASSERT_EQ(r.status, SolveStatus::kInterrupted);
        ASSERT_TRUE(r.returned_best_iterate) << r.message;
        ASSERT_TRUE(r.best_iterate.has_value());
        expect_primary_is(r, *r.best_iterate);
        // The best is a delivered iterate: its point is one event's point.
        bool found = false;
        for (const IterationSnapshot &s : seen) {
            if (s.iteration == r.best_iterate->iteration && bit_equal(s.x, r.best_iterate->x)) {
                found = true;
                // And it carries that event's own scalars.
                EXPECT_TRUE(same_scalar(s.f, r.best_iterate->f));
                EXPECT_TRUE(same_scalar(s.step_norm, r.best_iterate->step_norm));
                EXPECT_EQ(s.phase, r.best_iterate->phase);
                EXPECT_EQ(s.depth, r.best_iterate->depth);
                EXPECT_EQ(s.radius.has_value(), r.best_iterate->radius.has_value());
                EXPECT_EQ(s.mu.has_value(), r.best_iterate->mu.has_value());
            }
        }
        EXPECT_TRUE(found) << "best_iterate is not the point of any delivered event";
    }
}

TEST(BestIterate, AStopReturnBestOnTheConvergingRowLosesToConvergence) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        const SolveResult reference = solve(e, 0, false, kReadOnly);
        ASSERT_EQ(reference.status, SolveStatus::kOptimal);
        const Index last = reference.iterations;
        const SolveResult r = solve(e, 0, false, [&](const IterationEvent &ev) {
            return ev.iteration >= last ? CallbackAction::kStopReturnBest
                                        : CallbackAction::kContinue;
        });
        EXPECT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_FALSE(r.returned_best_iterate);
        EXPECT_FALSE(r.best_iterate.has_value());
        EXPECT_TRUE(bit_equal(r.x, reference.x));
    }
}

// ---------------------------------------------------------------------------
// best_iterate against the engines' own return-best exit.
// ---------------------------------------------------------------------------

TEST(BestIterate, BestIterateEqualsThePrimaryPointOnTheEnginesOwnReturnBestExit) {
    for (const Engine e : kEngines) {
        for (const Index cap : {2, 3}) {
            SCOPED_TRACE(std::string(name(e)) + " cap " + std::to_string(cap));
            const SolveResult r = solve(e, cap, true);
            ASSERT_EQ(r.status, SolveStatus::kIterationLimit);
            ASSERT_TRUE(r.returned_best_iterate) << r.message;
            ASSERT_TRUE(r.best_iterate.has_value())
                << "the return-best option arms the tracker with no callback and no token";
            expect_primary_is(r, *r.best_iterate);
            EXPECT_FALSE(r.best_iterate->recovering);
        }
    }
}

TEST(BestIterate, AnArmedSolveWithTheOptionOffReportsTheSameBestWithoutSubstituting) {
    for (const Engine e : kEngines) {
        for (const Index cap : {2, 3}) {
            SCOPED_TRACE(std::string(name(e)) + " cap " + std::to_string(cap));
            const SolveResult with_option = solve(e, cap, true);
            const SolveResult armed = solve(e, cap, false, kReadOnly);
            ASSERT_EQ(armed.status, SolveStatus::kIterationLimit);
            EXPECT_FALSE(armed.returned_best_iterate);
            ASSERT_TRUE(armed.best_iterate.has_value());
            ASSERT_TRUE(with_option.best_iterate.has_value());
            // The same tracker, whatever armed it.
            EXPECT_EQ(armed.best_iterate->iteration, with_option.best_iterate->iteration);
            EXPECT_TRUE(bit_equal(armed.best_iterate->x, with_option.best_iterate->x));
            EXPECT_TRUE(bit_equal(armed.best_iterate->lambda, with_option.best_iterate->lambda));
            EXPECT_TRUE(bit_equal(armed.best_iterate->z, with_option.best_iterate->z));
            EXPECT_TRUE(bit_equal(armed.best_iterate->g, with_option.best_iterate->g));
            // And the primary is the last iterate, not the substitution.
            expect_primary_is(with_option, *armed.best_iterate);
        }
    }
}

TEST(BestIterate, ATokenAloneArmsTheTracker) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        StopSource source; // never requested
        SolveBudget budget;
        budget.stop = source.token();
        const SolveResult r = solve(e, 3, false, nullptr, budget);
        ASSERT_EQ(r.status, SolveStatus::kIterationLimit);
        ASSERT_TRUE(r.best_iterate.has_value());
        EXPECT_FALSE(r.returned_best_iterate);
        // The same best as the option's, scalars included (the step that reached it is
        // taken when a token alone arms the tracker, with no callback installed).
        const SolveResult with_option = solve(e, 3, true);
        const SolveResult with_callback = solve(e, 3, false, kReadOnly);
        ASSERT_TRUE(with_option.best_iterate.has_value());
        ASSERT_TRUE(with_callback.best_iterate.has_value());
        for (const SolveResult *other : {&with_option, &with_callback}) {
            EXPECT_EQ(r.best_iterate->iteration, other->best_iterate->iteration);
            EXPECT_TRUE(same_scalar(r.best_iterate->step_norm, other->best_iterate->step_norm));
            EXPECT_TRUE(bit_equal(r.best_iterate->x, other->best_iterate->x));
        }
        if (e == Engine::kIpm && r.best_iterate->iteration > 0) {
            EXPECT_GT(r.best_iterate->step_norm, 0.0) << "a step reached it";
        }
    }
}

TEST(BestIterate, NothingIsTrackedAndNothingReportedWhenNothingIsArmed) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        const SolveResult r = solve(e, 3, false);
        ASSERT_EQ(r.status, SolveStatus::kIterationLimit);
        EXPECT_FALSE(r.best_iterate.has_value());
    }
}

TEST(BestIterate, AnOptimalSolveCarriesNoBestIterate) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        const SolveResult r = solve(e, 0, true, kReadOnly);
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        EXPECT_FALSE(r.best_iterate.has_value());
        EXPECT_FALSE(r.returned_best_iterate);
    }
}

TEST(BestIterate, ATokenStopReturnsTheCurrentPointAndReportsTheBestSeparately) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        std::vector<IterationSnapshot> seen;
        const SolveResult r = solve(
            e, 0, false,
            [&](const IterationEvent &ev) {
                seen.push_back(snapshot(ev));
                if (ev.iteration == 3) {
                    source.request_stop();
                }
                return CallbackAction::kContinue;
            },
            budget);
        ASSERT_EQ(r.status, SolveStatus::kInterrupted);
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_FALSE(r.returned_best_iterate);
        ASSERT_FALSE(seen.empty());
        EXPECT_TRUE(bit_equal(r.x, seen.back().x)) << "a token stop returns the current iterate";
        ASSERT_TRUE(r.best_iterate.has_value());
        EXPECT_LE(r.best_iterate->iteration, seen.back().iteration);
    }
}

// ---------------------------------------------------------------------------
// T9: armed-and-unfired is the unarmed solve where best_iterate is built.
// ---------------------------------------------------------------------------

TEST(BestIterate, T9ATrackerArmedByACallbackAndATokenMovesNoCounter) {
    for (const Engine e : kEngines) {
        for (const Index cap : {2, 4, 0}) {
            SCOPED_TRACE(std::string(name(e)) + " cap " + std::to_string(cap));
            const SolveResult plain = solve(e, cap, false);
            StopSource source;
            SolveBudget budget;
            budget.stop = source.token();
            const SolveResult armed = solve(e, cap, false, kReadOnly, budget);
            EXPECT_EQ(armed.status, plain.status);
            EXPECT_EQ(armed.stop_reason, plain.stop_reason);
            EXPECT_EQ(armed.message, plain.message);
            EXPECT_EQ(armed.iterations, plain.iterations);
            EXPECT_EQ(armed.work.objective_evals, plain.work.objective_evals);
            EXPECT_EQ(armed.work.gradient_evals, plain.work.gradient_evals);
            EXPECT_EQ(armed.work.constraint_evals, plain.work.constraint_evals);
            EXPECT_EQ(armed.work.jacobian_evals, plain.work.jacobian_evals);
            EXPECT_EQ(armed.work.hessian_evals, plain.work.hessian_evals);
            EXPECT_EQ(armed.work.factorizations, plain.work.factorizations);
            EXPECT_TRUE(bit_equal(armed.x, plain.x));
            EXPECT_TRUE(bit_equal(armed.lambda, plain.lambda));
            EXPECT_TRUE(bit_equal(armed.z, plain.z));
            EXPECT_TRUE(bit_equal(armed.g, plain.g));
            EXPECT_TRUE(same_scalar(armed.f, plain.f));
            EXPECT_TRUE(same_scalar(armed.stationarity, plain.stationarity));
            EXPECT_TRUE(same_scalar(armed.feasibility, plain.feasibility));
            EXPECT_TRUE(same_scalar(armed.complementarity, plain.complementarity));
            EXPECT_EQ(armed.best_iterate.has_value(), plain.status != SolveStatus::kOptimal &&
                                                          plain.status != SolveStatus::kAcceptable);
        }
    }
}

TEST(BestIterate, T9TheReturnBestOptionOutcomeDoesNotDependOnAnAttachedCallback) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        const SolveResult alone = solve(e, 3, true);
        const SolveResult attached = solve(e, 3, true, kReadOnly);
        EXPECT_EQ(attached.status, alone.status);
        EXPECT_EQ(attached.iterations, alone.iterations);
        EXPECT_EQ(attached.returned_best_iterate, alone.returned_best_iterate);
        EXPECT_TRUE(bit_equal(attached.x, alone.x));
        EXPECT_TRUE(bit_equal(attached.lambda, alone.lambda));
        EXPECT_TRUE(same_scalar(attached.f, alone.f));
        ASSERT_TRUE(alone.best_iterate.has_value());
        ASSERT_TRUE(attached.best_iterate.has_value());
        EXPECT_TRUE(bit_equal(attached.best_iterate->x, alone.best_iterate->x));
    }
}

// ---------------------------------------------------------------------------
// T12 (snapshot): a deep copy.
// ---------------------------------------------------------------------------

TEST(BestIterate, T12ASnapshotOutlivesTheEventAndTheSolveAndMatchesTheEvent) {
    for (const Engine e : kEngines) {
        SCOPED_TRACE(name(e));
        struct Pair {
            IterationSnapshot snap;
            Eigen::VectorXd x, lambda, z;
            double f, stationarity;
        };
        std::vector<Pair> pairs;
        const SolveResult r = solve(e, 0, false, [&](const IterationEvent &ev) {
            pairs.push_back({snapshot(ev), ev.x, ev.lambda, ev.z, ev.f, ev.stationarity});
            return CallbackAction::kContinue;
        });
        ASSERT_EQ(r.status, SolveStatus::kOptimal);
        ASSERT_FALSE(pairs.empty());
        for (const Pair &p : pairs) {
            EXPECT_EQ(p.snap.x.size(), 4);
            EXPECT_TRUE(bit_equal(p.snap.x, p.x));
            EXPECT_TRUE(bit_equal(p.snap.lambda, p.lambda));
            EXPECT_TRUE(bit_equal(p.snap.z, p.z));
            EXPECT_TRUE(same_scalar(p.snap.f, p.f));
            EXPECT_TRUE(same_scalar(p.snap.stationarity, p.stationarity));
            EXPECT_EQ(p.snap.g.size(), 0) << "an event carries no row values";
        }
    }
}

TEST(BestIterate, T12ASnapshotOfAScalarsOnlyEventHasNoVectors) {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmSolver solver(quiet_ipm());
    std::vector<IterationSnapshot> seen;
    solver.set_iteration_callback(
        [&](const IterationEvent &ev) {
            seen.push_back(snapshot(ev));
            return CallbackAction::kContinue;
        },
        IterationPayload::kScalars);
    const IpmResult r = solver.solve(problem, hs071_start());
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_FALSE(seen.empty());
    for (const IterationSnapshot &s : seen) {
        EXPECT_EQ(s.x.size(), 0);
        EXPECT_EQ(s.lambda.size(), 0);
        EXPECT_EQ(s.z.size(), 0);
        EXPECT_TRUE(std::isnan(s.stationarity));
        EXPECT_TRUE(s.mu.has_value());
    }
}
