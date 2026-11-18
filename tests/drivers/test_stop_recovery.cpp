// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-5 G2: the recovery-aware stop, on the interior-point engine. The cases
// are the interface design's test-plan rows
// (docs/notes/2026-11-m8-ap5-interface-design.md section 3.6):
//
//   T14 IPM in-place restoration rows are delivered with
//       IterationEvent::recovering = true (pins design item D3: the rows ARE
//       delivered, under either CallbackScope, with f and the diagnostics NaN).
//   T5  a stop inside restoration. A TOKEN stop returns the point restoration was
//       entered from (taken before the entry's multiplier initialisation); a
//       CALLBACK kStop on a `recovering` event returns the point that event
//       showed. Proximal and nested l1, with and without variable bounds (the
//       bounded case is where the post-factorization doors' mu-form right-hand
//       side would corrupt a naive stash's stationarity).
//   T9  armed-and-unfired == unarmed, counter for counter, on the
//       restoration-entering fixtures (the SQP half lives in test_callback.cpp,
//       beside its restoration fixtures).
//
// The fixture is a locally infeasible problem (c(x) = x0^2 + x1^2 + 1 = 0 has no
// real root), so the solve enters feasibility restoration and stays there.

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include "hven/core/stop.h"
#include "hven/detail/warmstart/warm_start_access.h"
#include "hven/ipm/ipm_solver.h"
#include "hven/ipm/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using hven::CallbackAction;
using hven::CallbackScope;
using hven::Index;
using hven::IpmOptions;
using hven::IpmRestorationMode;
using hven::IpmResult;
using hven::IpmSolver;
using hven::IterationEvent;
using hven::IterationPayload;
using hven::Problem;
using hven::SolveBudget;
using hven::SolveStatus;
using hven::StopReason;
using hven::StopSource;
using hven::Verbosity;

// min x0 + x1  s.t.  x0^2 + x1^2 + 1 = 0, optionally -5 <= x <= 5.
class InfeasibleDisc final : public hven::Nlp {
  public:
    explicit InfeasibleDisc(bool boxed) : boxed_(boxed) {}

    hven::NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        const double b = boxed_ ? 5.0 : inf;
        xl << -b, -b;
        xu << b, b;
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override { return x[0] + x[1]; }
    void eval_grad_f(hven::ConstVecRef, hven::VecRef g) const override { g << 1.0, 1.0; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        g[0] = x[0] * x[0] + x[1] * x[1] + 1.0;
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        v << 2.0 * x[0], 2.0 * x[1];
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(hven::ConstVecRef, double, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
    }
    std::string name() const override { return "InfeasibleDisc"; }

  private:
    bool boxed_;
};

Eigen::VectorXd start() {
    Eigen::VectorXd x0(2);
    x0 << 1.0, 0.5;
    return x0;
}

IpmOptions options(IpmRestorationMode mode) {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.advanced.restoration_mode = mode;
    o.advanced.max_restoration_entries = 1;
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

// What one delivered event said, owned.
struct Seen {
    Index iteration = 0;
    bool recovering = false;
    double f = 0.0, stationarity = 0.0, feasibility = 0.0, complementarity = 0.0;
    Eigen::VectorXd x, lambda, z;
};

Seen copy_of(const IterationEvent &e) {
    return Seen{e.iteration, e.recovering, e.f, e.stationarity, e.feasibility, e.complementarity,
                e.x,         e.lambda,     e.z};
}

struct RestoCase {
    const char *label;
    IpmRestorationMode mode;
    bool boxed;
};

const RestoCase kRestoCases[] = {
    {"proximal", IpmRestorationMode::kProximalSwitch, false},
    {"proximal-boxed", IpmRestorationMode::kProximalSwitch, true},
    {"nested-l1", IpmRestorationMode::kL1Nested, false},
    {"nested-l1-boxed", IpmRestorationMode::kL1Nested, true},
};

// The reference: a read-only callback, every payload, the scope named.
struct Reference {
    IpmResult result;
    std::vector<Seen> seen;
    std::size_t first_recovering = 0; // index into `seen`
    bool any_recovering = false;
};

Reference run_reference(const RestoCase &m, CallbackScope scope) {
    Reference ref;
    Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
    IpmSolver solver(options(m.mode));
    solver.set_iteration_callback(
        [&](const IterationEvent &e) {
            if (e.recovering && !ref.any_recovering) {
                ref.any_recovering = true;
                ref.first_recovering = ref.seen.size();
            }
            ref.seen.push_back(copy_of(e));
            return CallbackAction::kContinue;
        },
        IterationPayload::kAll, scope);
    ref.result = solver.solve(problem, start());
    return ref;
}

} // namespace

// ---------------------------------------------------------------------------
// T14 (pins D3 first): the in-place restoration rows are delivered, flagged.
// ---------------------------------------------------------------------------

TEST(StopRecovery, T14IpmRestorationRowsAreDeliveredWithRecoveringSetUnderEitherScope) {
    for (const RestoCase &m : kRestoCases) {
        for (CallbackScope scope : {CallbackScope::kTopLevel, CallbackScope::kIncludeRestoration}) {
            SCOPED_TRACE(m.label);
            const Reference ref = run_reference(m, scope);
            ASSERT_TRUE(ref.any_recovering)
                << "premise: the solve must enter restoration, and its rows must be delivered "
                   "(D3) -- no recovering event arrived";
            ASSERT_GE(ref.first_recovering, 1u) << "the solve enters restoration from an iterate "
                                                   "an event already described";
            EXPECT_EQ(static_cast<Index>(ref.seen.size()), ref.result.iterations)
                << "events == iterations, restoration rows included";
            Index recovering = 0;
            for (const Seen &s : ref.seen) {
                if (s.recovering) {
                    ++recovering;
                    // No declared evaluation of a restoration point exists, so f
                    // and the three diagnostics are NaN there.
                    EXPECT_TRUE(std::isnan(s.f));
                    EXPECT_TRUE(std::isnan(s.stationarity));
                } else {
                    EXPECT_TRUE(std::isfinite(s.f)) << "iteration " << s.iteration;
                }
            }
            EXPECT_GE(recovering, 1);
            // The rows before the first recovering one are ordinary.
            for (std::size_t k = 0; k < ref.first_recovering; ++k) {
                EXPECT_FALSE(ref.seen[k].recovering);
            }
        }
    }
}

// T12 (M8 AP-5 G5): a recovering interior-point event carries no caller-space
// rows and no bound sides under kAll (no declared evaluation of a restoration
// point exists); the ordinary rows around it carry both.
TEST(StopRecovery, T12IpmRecoveringEventsCarryNoRowsAndNoBoundSides) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver solver(options(m.mode));
        Index recovering = 0;
        Index ordinary = 0;
        solver.set_iteration_callback(
            [&](const IterationEvent &e) {
                EXPECT_EQ(e.x.size(), 2);
                if (e.recovering) {
                    ++recovering;
                    EXPECT_EQ(e.g.size(), 0) << "iteration " << e.iteration;
                    EXPECT_EQ(e.violation.size(), 0);
                    EXPECT_EQ(e.z_lower.size(), 0);
                    EXPECT_EQ(e.z_upper.size(), 0);
                } else {
                    ++ordinary;
                    EXPECT_EQ(e.g.size(), 1) << "iteration " << e.iteration;
                    EXPECT_EQ(e.violation.size(), 1);
                    EXPECT_EQ(e.z_lower.size(), 2);
                    EXPECT_EQ(e.z_upper.size(), 2);
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll);
        (void)solver.solve(problem, start());
        EXPECT_GE(recovering, 1) << "premise: the solve enters restoration";
        EXPECT_GE(ordinary, 1);
    }
}

// ---------------------------------------------------------------------------
// T5: a stop inside restoration.
// ---------------------------------------------------------------------------

// A TOKEN stop requested from the callback on the first recovering event is
// honoured at the same iteration top, so the restoration iterate was the point
// shown -- and the result is the entry point instead.
TEST(StopRecovery, T5ATokenStopInsideRestorationReturnsTheRestorationEntryPoint) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        const Reference ref = run_reference(m, CallbackScope::kTopLevel);
        ASSERT_TRUE(ref.any_recovering);
        const Seen &entry = ref.seen[ref.first_recovering - 1];
        // Stop on the third recovering event (or the last there is): the first
        // one shows the entry iterate itself (the entry takes no step), so only a
        // later one is a point restoration moved to.
        std::size_t stop_k = ref.first_recovering;
        std::size_t seen_recovering = 0;
        for (std::size_t k = ref.first_recovering; k < ref.seen.size() && seen_recovering < 3;
             ++k) {
            if (ref.seen[k].recovering) {
                stop_k = k;
                ++seen_recovering;
            }
        }
        const Seen &shown = ref.seen[stop_k];

        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver solver(options(m.mode));
        Index events = 0;
        const Index stop_event = static_cast<Index>(stop_k);
        solver.set_iteration_callback(
            [&](const IterationEvent &e) {
                if (events++ == stop_event) {
                    source.request_stop();
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll);
        const IpmResult r = solver.solve(problem, start(), {}, budget);

        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_EQ(events, stop_event + 1) << "stopped at the chosen recovering iteration top";
        EXPECT_TRUE(r.returned_pre_restoration_point) << r.message;
        EXPECT_NE(r.message.find("; returned the point before feasibility restoration"),
                  std::string::npos)
            << r.message;
        // THE ENTRY POINT, not the restoration iterate the event showed. The entry
        // is taken from the iterate the previous event described (a
        // post-factorization door: that event fired for it and nothing moved it
        // since), so every primary field is that event's, bit for bit.
        EXPECT_TRUE(bit_equal(r.x, entry.x));
        EXPECT_FALSE(bit_equal(r.x, shown.x) && bit_equal(r.lambda, shown.lambda))
            << "the restoration iterate was not returned";
        EXPECT_TRUE(bit_equal(r.lambda, entry.lambda))
            << "the multipliers are the entry's, not the entry's re-initialised ones";
        EXPECT_TRUE(bit_equal(r.z, entry.z));
        EXPECT_NEAR(r.f, entry.f, 1e-12 * (1.0 + std::abs(entry.f)));
        EXPECT_TRUE(same_scalar(r.stationarity, entry.stationarity))
            << "declared stationarity at the entry point (the bounded cases would corrupt it "
               "if the stash kept the mu-form right-hand side)";
        EXPECT_TRUE(same_scalar(r.feasibility, entry.feasibility));
        EXPECT_TRUE(same_scalar(r.complementarity, entry.complementarity));
        // The convergence column's row is the stash's (the entry iterate's), not an
        // empty or restoration row: its equality residual is the declared |c| the
        // entry event reported (one equality, no inequality, no scaling on it).
        EXPECT_NEAR(r.convergence.equality, entry.feasibility, 1e-12 * (1.0 + entry.feasibility));
        ASSERT_EQ(r.g.size(), 1) << "g is the declared constraint value at the entry point";
        EXPECT_NEAR(r.g[0], r.x[0] * r.x[0] + r.x[1] * r.x[1] + 1.0, 1e-12);
        // THE WARM START OF A STASH EXIT (pins the migration-0.2.md row): it is the
        // entry point's x and equality multipliers, CORE ONLY. No polish extension
        // is attached even when the problem is boxed -- the capture still treats a
        // restoration-flagged exit conservatively (solve_exit_restoration_active_).
        ASSERT_FALSE(r.warm_start.empty()) << "a stash exit still exports a warm start";
        using hven::detail::WarmStartAccess;
        EXPECT_TRUE(bit_equal(WarmStartAccess::primal(r.warm_start), entry.x));
        EXPECT_TRUE(bit_equal(WarmStartAccess::eq(r.warm_start), entry.lambda));
        EXPECT_TRUE(WarmStartAccess::extensions(r.warm_start).empty())
            << "no polish extension on a stash exit";
    }
}

// A CALLBACK stop on a `recovering` event returns the restoration-entry point too
// (M8 AP-5 G7: one rule for a stop in restoration, token or callback, as the SQP
// engine already had), with or without a token installed: the callback alone arms
// the entry stash.
TEST(StopRecovery, T5BACallbackStopOnARecoveringEventReturnsTheRestorationEntryPoint) {
    for (const RestoCase &m : kRestoCases) {
        for (const bool with_token : {false, true}) {
            SCOPED_TRACE(std::string(m.label) + (with_token ? " +token" : " callback only"));
            const Reference ref = run_reference(m, CallbackScope::kTopLevel);
            ASSERT_TRUE(ref.any_recovering);
            const Seen &entry = ref.seen[ref.first_recovering - 1];
            // The second recovering event (or the first if there is one): a point
            // restoration moved to, not the entry iterate itself.
            std::size_t stop_k = ref.first_recovering;
            if (stop_k + 1 < ref.seen.size() && ref.seen[stop_k + 1].recovering) {
                ++stop_k;
            }
            const Seen &shown = ref.seen[stop_k];

            Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
            IpmSolver solver(options(m.mode));
            std::size_t events = 0;
            solver.set_iteration_callback(
                [&](const IterationEvent &) {
                    return events++ == stop_k ? CallbackAction::kStop : CallbackAction::kContinue;
                },
                IterationPayload::kAll);
            StopSource source;
            SolveBudget budget;
            if (with_token) {
                budget.stop = source.token(); // never requested: the callback's stop wins
            }
            const IpmResult r = solver.solve(problem, start(), {}, budget);

            EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
            EXPECT_EQ(r.stop_reason, StopReason::kCallbackStop);
            EXPECT_EQ(events, stop_k + 1);
            EXPECT_TRUE(r.returned_pre_restoration_point) << r.message;
            EXPECT_NE(r.message.find("; returned the point before feasibility restoration"),
                      std::string::npos)
                << r.message;
            EXPECT_TRUE(bit_equal(r.x, entry.x)) << "the entry point, not the shown one";
            EXPECT_FALSE(bit_equal(r.x, shown.x) && bit_equal(r.lambda, shown.lambda));
            EXPECT_TRUE(bit_equal(r.lambda, entry.lambda));
            EXPECT_TRUE(bit_equal(r.z, entry.z));
            EXPECT_TRUE(same_scalar(r.stationarity, entry.stationarity));
            EXPECT_TRUE(same_scalar(r.feasibility, entry.feasibility));
        }
    }
}

// A token that is requested while the solve is NOT in restoration is untouched by
// the stash: the current iterate, the last event's point (G1's rule).
TEST(StopRecovery, T5CATokenStopOutsideRestorationStillReturnsTheCurrentIterate) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        const Reference ref = run_reference(m, CallbackScope::kTopLevel);
        ASSERT_TRUE(ref.any_recovering);
        ASSERT_GE(ref.first_recovering, 2u);
        const Index stop_at = ref.seen[ref.first_recovering - 2].iteration;

        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver solver(options(m.mode));
        Seen last;
        solver.set_iteration_callback(
            [&](const IterationEvent &e) {
                last = copy_of(e);
                if (!e.recovering && e.iteration == stop_at) {
                    source.request_stop();
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll);
        const IpmResult r = solver.solve(problem, start(), {}, budget);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted);
        EXPECT_EQ(r.stop_reason, StopReason::kStopRequested);
        EXPECT_FALSE(last.recovering);
        EXPECT_TRUE(bit_equal(r.x, last.x));
        EXPECT_FALSE(r.returned_pre_restoration_point);
        EXPECT_EQ(r.message.find("before feasibility restoration"), std::string::npos) << r.message;
    }
}

// ---------------------------------------------------------------------------
// T9 (IPM half): armed-and-unfired == unarmed through a restoration episode.
// ---------------------------------------------------------------------------

TEST(StopRecovery, T9AnArmedUnfiredSolveThroughRestorationIsTheUnarmedSolve) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        Problem plain_problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver plain_solver(options(m.mode));
        const IpmResult plain = plain_solver.solve(plain_problem, start());

        StopSource source; // never requested
        SolveBudget budget;
        budget.stop = source.token();
        Problem armed_problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver armed_solver(options(m.mode));
        armed_solver.set_iteration_callback(
            [](const IterationEvent &) { return CallbackAction::kContinue; },
            IterationPayload::kAll);
        const IpmResult armed = armed_solver.solve(armed_problem, start(), {}, budget);

        ASSERT_TRUE(last_ran_phase(plain)->restoration_entries.has_value());
        ASSERT_GE(*last_ran_phase(plain)->restoration_entries, 1)
            << "premise: the fixture enters restoration";
        EXPECT_EQ(armed.status, plain.status);
        EXPECT_EQ(armed.stop_reason, plain.stop_reason);
        EXPECT_EQ(armed.message, plain.message);
        EXPECT_EQ(armed.iterations, plain.iterations);
        EXPECT_EQ(armed.work.factorizations, plain.work.factorizations);
        EXPECT_EQ(armed.work.objective_evals, plain.work.objective_evals);
        EXPECT_EQ(armed.work.constraint_evals, plain.work.constraint_evals);
        EXPECT_EQ(armed.work.jacobian_evals, plain.work.jacobian_evals);
        EXPECT_EQ(armed.work.hessian_evals, plain.work.hessian_evals);
        EXPECT_EQ(*last_ran_phase(armed)->restoration_entries,
                  *last_ran_phase(plain)->restoration_entries);
        EXPECT_EQ(last_ran_phase(armed)->restoration_iters,
                  last_ran_phase(plain)->restoration_iters);
        EXPECT_TRUE(bit_equal(armed.x, plain.x));
        EXPECT_TRUE(bit_equal(armed.lambda, plain.lambda));
        EXPECT_TRUE(bit_equal(armed.z, plain.z));
        EXPECT_TRUE(bit_equal(armed.g, plain.g));
        EXPECT_TRUE(same_scalar(armed.f, plain.f));
        EXPECT_TRUE(same_scalar(armed.stationarity, plain.stationarity));
        EXPECT_TRUE(same_scalar(armed.feasibility, plain.feasibility));
        EXPECT_TRUE(same_scalar(armed.complementarity, plain.complementarity));
        EXPECT_TRUE(same_scalar(armed.convergence.stationarity, plain.convergence.stationarity));
    }
}

// ---------------------------------------------------------------------------
// M8 AP-5 G3 on the restoration fixtures: best_iterate and kStopReturnBest.
// ---------------------------------------------------------------------------

// The tracker is suspended in restoration, so best_iterate is a caller-space
// iterate from before it, and a stop inside restoration reports it separately from
// the point it returns.
TEST(StopRecovery, G3BestIterateOnARestorationSolveIsAnIterateFromOutsideRestoration) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        const Reference ref = run_reference(m, CallbackScope::kTopLevel);
        ASSERT_TRUE(ref.any_recovering);
        const IpmResult &r = ref.result;
        ASSERT_NE(r.status, SolveStatus::kOptimal);
        ASSERT_TRUE(r.best_iterate.has_value());
        const hven::IterationSnapshot &best = *r.best_iterate;
        EXPECT_FALSE(best.recovering);
        EXPECT_TRUE(std::isfinite(best.f));
        EXPECT_TRUE(std::isfinite(best.feasibility));
        ASSERT_EQ(best.g.size(), 1);
        EXPECT_NEAR(best.g[0], best.x[0] * best.x[0] + best.x[1] * best.x[1] + 1.0,
                    1e-12 * (1.0 + std::abs(best.g[0])));
        bool delivered = false;
        for (const Seen &s : ref.seen) {
            delivered = delivered || (!s.recovering && bit_equal(s.x, best.x));
        }
        EXPECT_TRUE(delivered) << "best_iterate is a non-recovering delivered point";
    }
}

TEST(StopRecovery, G3AStopReturnBestOnARecoveringEventReturnsTheBestNotTheShownPoint) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        const Reference ref = run_reference(m, CallbackScope::kTopLevel);
        ASSERT_TRUE(ref.any_recovering);
        const Seen &shown = ref.seen[ref.first_recovering];

        Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver solver(options(m.mode));
        solver.set_iteration_callback(
            [&](const IterationEvent &e) {
                return e.recovering ? CallbackAction::kStopReturnBest : CallbackAction::kContinue;
            },
            IterationPayload::kAll);
        const IpmResult r = solver.solve(problem, start());

        EXPECT_EQ(r.status, SolveStatus::kInterrupted) << r.message;
        EXPECT_EQ(r.stop_reason, StopReason::kCallbackStop);
        EXPECT_FALSE(r.returned_pre_restoration_point);
        ASSERT_TRUE(r.best_iterate.has_value());
        EXPECT_TRUE(r.returned_best_iterate) << r.message;
        const hven::IterationSnapshot &best = *r.best_iterate;
        EXPECT_FALSE(best.recovering);
        EXPECT_TRUE(bit_equal(r.x, best.x));
        EXPECT_TRUE(bit_equal(r.lambda, best.lambda));
        EXPECT_TRUE(bit_equal(r.z, best.z));
        EXPECT_TRUE(bit_equal(r.g, best.g));
        EXPECT_TRUE(same_scalar(r.stationarity, best.stationarity));
        EXPECT_TRUE(same_scalar(r.feasibility, best.feasibility));
        EXPECT_FALSE(bit_equal(r.x, shown.x) && bit_equal(r.lambda, shown.lambda))
            << "the restoration iterate the callback was shown is not what comes back";
        // The declared diagnostics describe the best point: not NaN, which is what a
        // restoration-flagged exit would otherwise report.
        EXPECT_TRUE(std::isfinite(r.feasibility));
    }
}

TEST(StopRecovery, G3ATokenStopInRestorationAlsoReportsABestIterateSeparately) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        const Reference ref = run_reference(m, CallbackScope::kTopLevel);
        ASSERT_TRUE(ref.any_recovering);
        StopSource source;
        SolveBudget budget;
        budget.stop = source.token();
        Problem problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver solver(options(m.mode));
        solver.set_iteration_callback(
            [&](const IterationEvent &e) {
                if (e.recovering) {
                    source.request_stop();
                }
                return CallbackAction::kContinue;
            },
            IterationPayload::kAll);
        const IpmResult r = solver.solve(problem, start(), {}, budget);
        EXPECT_EQ(r.status, SolveStatus::kInterrupted);
        EXPECT_TRUE(r.returned_pre_restoration_point);
        EXPECT_FALSE(r.returned_best_iterate);
        ASSERT_TRUE(r.best_iterate.has_value());
        EXPECT_FALSE(r.best_iterate->recovering);
    }
}

// T9 on the restoration fixtures: arming the tracker (a callback, a token) moves
// nothing, whatever the best tracker then reports.
TEST(StopRecovery, G3TheTrackerArmedThroughARestorationEpisodeMovesNoCounter) {
    for (const RestoCase &m : kRestoCases) {
        SCOPED_TRACE(m.label);
        Problem plain_problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver plain_solver(options(m.mode));
        const IpmResult plain = plain_solver.solve(plain_problem, start());
        EXPECT_FALSE(plain.best_iterate.has_value());

        Problem armed_problem(std::make_shared<InfeasibleDisc>(m.boxed));
        IpmSolver armed_solver(options(m.mode));
        armed_solver.set_iteration_callback(
            [](const IterationEvent &) { return CallbackAction::kContinue; },
            IterationPayload::kScalars);
        const IpmResult armed = armed_solver.solve(armed_problem, start());
        EXPECT_EQ(armed.status, plain.status);
        EXPECT_EQ(armed.iterations, plain.iterations);
        EXPECT_EQ(armed.work.factorizations, plain.work.factorizations);
        EXPECT_EQ(armed.work.objective_evals, plain.work.objective_evals);
        EXPECT_EQ(armed.work.constraint_evals, plain.work.constraint_evals);
        EXPECT_EQ(armed.work.hessian_evals, plain.work.hessian_evals);
        EXPECT_TRUE(bit_equal(armed.x, plain.x));
        EXPECT_TRUE(bit_equal(armed.lambda, plain.lambda));
        EXPECT_TRUE(same_scalar(armed.f, plain.f));
        EXPECT_TRUE(armed.best_iterate.has_value());
    }
}
