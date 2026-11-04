// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AP-2 B5: evaluation failure and the exception contract (core/errors.h), on both
// engines through the public handles.
//
//   * A non-finite value (NaN, +Inf, -Inf) in the objective, the gradient, a
//     constraint, a Jacobian entry or a Hessian entry at a trial point, and a
//     thrown EvaluationError there, is a rejected trial: the solve recovers and
//     converges, and the failure is counted in SolveResult::evaluation_errors.
//   * At the start point a non-finite value ends the solve kNumericalError with a
//     message naming the quantity; a thrown EvaluationError propagates.
//   * Any other exception propagates unchanged -- the same type, the same object
//     -- and last_result() then holds the interrupted solve, finished into the
//     problem's row space, with status kInterrupted and stop reason kException.
//   * A callback-requested stop and an escaped exception are told apart by the
//     stop reason and the message.
//   * After any throw the same solver and Problem solve again exactly as a fresh
//     pair does.
//
// The faults are injected by call count (FaultyHs071::fire_at): the fire_at-th
// call of one callback, which on HS071 is past every start-point evaluation of
// both engines, so it lands on a trial point or on the derivatives of a point a
// step reached. `fired` is asserted so no case passes vacuously.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <vector>

#include <Eigen/Core>

#include "drivers/support/hs071_problem.h"
#include "hven/core/errors.h"
#include "hven/detail/interior/utils/thread_pool.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/drivers/trace.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using hven::Index;
using hven::solvers::BackendError;
using hven::solvers::CallbackAction;
using hven::solvers::Error;
using hven::solvers::EvaluationError;
using hven::solvers::IpmOptions;
using hven::solvers::IpmResult;
using hven::solvers::IpmSolver;
using hven::solvers::IpmStopReason;
using hven::solvers::IterationEvent;
using hven::solvers::Problem;
using hven::solvers::SolveResult;
using hven::solvers::SolveStatus;
using hven::solvers::SqpOptions;
using hven::solvers::SqpResult;
using hven::solvers::SqpSolver;
using hven::solvers::SqpStopReason;
using hven::solvers::Verbosity;
using hven_drivers_tests::hs071_start;
using hven_drivers_tests::Hs071Problem;

// A start point strictly inside HS071's box, so the interior-point engine
// evaluates it as given (a point on a bound is pushed inward first).
Eigen::VectorXd interior_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.5, 4.5, 4.5, 1.5;
    return x0;
}

enum class Callback { kObjective, kGradient, kConstraints, kJacobian, kHessian };
enum class Fault { kNone, kNaN, kPosInf, kNegInf, kEvaluationError, kForeign, kForeignNonStd };
enum class Engine { kIpm, kSqp };

const char *name(Callback c) {
    switch (c) {
    case Callback::kObjective:
        return "objective";
    case Callback::kGradient:
        return "gradient";
    case Callback::kConstraints:
        return "constraints";
    case Callback::kJacobian:
        return "jacobian";
    case Callback::kHessian:
        return "hessian";
    }
    return "?";
}

const char *name(Fault f) {
    switch (f) {
    case Fault::kNone:
        return "none";
    case Fault::kNaN:
        return "nan";
    case Fault::kPosInf:
        return "pos_inf";
    case Fault::kNegInf:
        return "neg_inf";
    case Fault::kEvaluationError:
        return "evaluation_error";
    case Fault::kForeign:
        return "foreign";
    case Fault::kForeignNonStd:
        return "foreign_non_std";
    }
    return "?";
}

const char *name(Engine e) { return e == Engine::kIpm ? "ipm" : "sqp"; }

// The foreign exception: a type hven has never heard of, carrying a token so the
// caught object can be told to be the thrown one.
struct Sentinel : std::exception {
    explicit Sentinel(int t) : token(t) {}
    const char *what() const noexcept override { return "sentinel from the model"; }
    int token;
};
constexpr int kSentinelToken = 314159;
constexpr int kNonStdToken = 42;

// HS071 with one fault: on the fire_at-th call of `where` (1-based), or -- in
// start mode -- on every call at the start point.
//
// THE RIDERS' MODES (M8 B5 riders): `sticky` faults on EVERY call from the
// fire_at-th on, and `fresh_only` narrows that to points the model never
// evaluated before then -- a region the solve cannot step into, whose edge is
// exactly where it stood, so every rollback lands back on a clean point and
// every step out of it faults again. `obj_factor_filter` counts only Hessian
// calls with a nonzero (+1) or zero (-1) objective factor; the interior-point
// engine's feasibility modes evaluate the Hessian with a zero one. `fix_x0`
// fixes x0 at 1, its value at the optimum.
class FaultyHs071 final : public hven::solvers::Nlp {
  public:
    Callback where = Callback::kObjective;
    Fault fault = Fault::kNone;
    long fire_at = 0;
    bool start_mode = false;
    bool sticky = false;
    bool fresh_only = false;
    int obj_factor_filter = 0;
    bool fix_x0 = false;
    mutable std::vector<Eigen::VectorXd> seen;
    // The start point start mode faults at, exactly as the engine evaluates it.
    Eigen::VectorXd start = interior_start();
    mutable long calls = 0;
    mutable long fired = 0;

    hven::solvers::NlpDimensions dimensions() const override { return base_.dimensions(); }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        base_.bounds(xl, xu, gl, gu);
        if (fix_x0) {
            xl[0] = 1.0;
            xu[0] = 1.0;
        }
    }
    double eval_f(hven::ConstVecRef x) const override {
        double f = base_.eval_f(x);
        if (fires(Callback::kObjective, x)) {
            f = poison();
        }
        return f;
    }
    void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
        base_.eval_grad_f(x, g);
        if (fires(Callback::kGradient, x)) {
            g[1] = poison();
        }
    }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        base_.eval_g(x, g);
        if (fires(Callback::kConstraints, x)) {
            g[0] = poison();
        }
    }
    void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        base_.jacobian_structure(r, c);
    }
    void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
        base_.eval_jacobian(x, v);
        if (fires(Callback::kJacobian, x)) {
            v[5] = poison();
        }
    }
    void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
        base_.hessian_structure(r, c);
    }
    void eval_hessian(hven::ConstVecRef x, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        base_.eval_hessian(x, obj_factor, lambda, v);
        if (fires(Callback::kHessian, x, obj_factor)) {
            v[2] = poison();
        }
    }
    std::string name() const override { return "FaultyHs071"; }

  private:
    // Counts the call, decides whether it faults, and throws for the throwing
    // faults; a value fault returns true and the caller writes poison().
    bool fires(Callback c, hven::ConstVecRef x, double obj_factor = 1.0) const {
        if (c != where || fault == Fault::kNone) {
            return false;
        }
        if (c == Callback::kHessian && obj_factor_filter != 0 &&
            (obj_factor_filter > 0) == (obj_factor == 0.0)) {
            return false;
        }
        ++calls;
        bool hit = false;
        if (start_mode) {
            hit = (x - start).cwiseAbs().maxCoeff() == 0.0;
        } else if (sticky) {
            if (calls < fire_at) {
                if (fresh_only) {
                    seen.emplace_back(x);
                }
            } else {
                hit = !fresh_only || !was_seen(x);
            }
        } else {
            hit = calls == fire_at;
        }
        if (!hit) {
            return false;
        }
        ++fired;
        switch (fault) {
        case Fault::kEvaluationError:
            throw EvaluationError("FaultyHs071: cannot evaluate here");
        case Fault::kForeign:
            throw Sentinel(kSentinelToken);
        case Fault::kForeignNonStd:
            throw kNonStdToken;
        default:
            return true;
        }
    }

  public:
    bool was_seen_public(hven::ConstVecRef x) const { return was_seen(x); }

  private:
    bool was_seen(hven::ConstVecRef x) const {
        for (const Eigen::VectorXd &p : seen) {
            if (p == x) {
                return true;
            }
        }
        return false;
    }
    double poison() const {
        switch (fault) {
        case Fault::kNaN:
            return std::numeric_limits<double>::quiet_NaN();
        case Fault::kPosInf:
            return std::numeric_limits<double>::infinity();
        case Fault::kNegInf:
            return -std::numeric_limits<double>::infinity();
        default:
            return 0.0;
        }
    }

    Hs071Problem base_;
};

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

// One solve on a fresh solver, the result sliced to the shared core plus the two
// engine facts every pin below reads.
struct SolveRun {
    SolveResult result;
    long fired = 0;
    Eigen::Index rollbacks = 0;
};

SolveRun solve_once(Engine engine, const std::shared_ptr<FaultyHs071> &nlp) {
    Problem problem(nlp);
    SolveRun run;
    if (engine == Engine::kIpm) {
        IpmSolver solver(quiet_ipm());
        const IpmResult r = solver.solve(problem, hs071_start());
        run.result = r;
        run.rollbacks = r.nonfinite_step_rollbacks;
    } else {
        SqpSolver solver(quiet_sqp());
        const SqpResult r = solver.solve(problem, hs071_start());
        run.result = r;
        run.rollbacks = r.counters.nonfinite_step_rollbacks;
    }
    run.fired = nlp->fired;
    return run;
}

Eigen::VectorXd hs071_solution() {
    Eigen::VectorXd x(4);
    x << 1.0, 4.74299963, 3.82114998, 1.37940829;
    return x;
}

constexpr long kFireAt = 4;

// --- a trial-point failure is a rejected trial ----------------------------------

struct TrialCase {
    Engine engine;
    Callback where;
    Fault fault;
};

std::string trial_case_name(const ::testing::TestParamInfo<TrialCase> &info) {
    return std::string(name(info.param.engine)) + "_" + name(info.param.where) + "_" +
           name(info.param.fault);
}

class TrialFailure : public ::testing::TestWithParam<TrialCase> {};

TEST_P(TrialFailure, IsARejectedTrialAndTheSolveRecovers) {
    const TrialCase c = GetParam();
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->where = c.where;
    nlp->fault = c.fault;
    nlp->fire_at = kFireAt;
    SolveRun run;
    ASSERT_NO_THROW(run = solve_once(c.engine, nlp));
    ASSERT_EQ(run.fired, 1) << "the fault never reached the solve";
    EXPECT_EQ(run.result.status, SolveStatus::kOptimal) << run.result.message;
    EXPECT_LE((run.result.x - hs071_solution()).lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_GE(run.result.evaluation_errors.count, 1);
    EXPECT_FALSE(run.result.evaluation_errors.last_message.empty());
    if (c.fault == Fault::kEvaluationError) {
        EXPECT_NE(run.result.evaluation_errors.last_message.find("cannot evaluate here"),
                  std::string::npos)
            << run.result.evaluation_errors.last_message;
    } else {
        EXPECT_NE(run.result.evaluation_errors.last_message.find("non-finite"), std::string::npos)
            << run.result.evaluation_errors.last_message;
    }
}

std::vector<TrialCase> trial_cases() {
    std::vector<TrialCase> out;
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (Callback w : {Callback::kObjective, Callback::kGradient, Callback::kConstraints,
                           Callback::kJacobian, Callback::kHessian}) {
            for (Fault f : {Fault::kNaN, Fault::kPosInf, Fault::kNegInf, Fault::kEvaluationError}) {
                out.push_back({e, w, f});
            }
        }
    }
    return out;
}

INSTANTIATE_TEST_SUITE_P(BothEngines, TrialFailure, ::testing::ValuesIn(trial_cases()),
                         trial_case_name);

// THE IPM BUG THIS GROUP CLOSES, pinned by name: a -Inf objective at a trial point
// passes every acceptance comparison, so before B5 the interior-point engine
// accepted it. Now it is rejected and the solve ends at the true optimum.
TEST(TrialFailure, ANegativeInfiniteObjectiveIsNeverAcceptedByTheInteriorPointEngine) {
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->where = Callback::kObjective;
    nlp->fault = Fault::kNegInf;
    nlp->fire_at = kFireAt;
    const SolveRun run = solve_once(Engine::kIpm, nlp);
    ASSERT_EQ(run.fired, 1);
    EXPECT_EQ(run.result.status, SolveStatus::kOptimal);
    EXPECT_TRUE(std::isfinite(run.result.f));
    EXPECT_NEAR(run.result.f, 17.0140173, 1e-6);
}

// A DERIVATIVE FAILURE AT THE POINT A STEP REACHED is rejected after the fact on
// both engines: the step is rolled back.
TEST(TrialFailure, ANonFiniteHessianAtAReachedPointRollsTheStepBackOnBothEngines) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kHessian;
        nlp->fault = Fault::kNaN;
        nlp->fire_at = kFireAt;
        const SolveRun run = solve_once(e, nlp);
        ASSERT_EQ(run.fired, 1);
        EXPECT_EQ(run.result.status, SolveStatus::kOptimal);
        EXPECT_GE(run.rollbacks, 1);
        EXPECT_NE(run.result.evaluation_errors.last_message.find("rolled back"), std::string::npos)
            << run.result.evaluation_errors.last_message;
    }
}

// --- the start point ------------------------------------------------------------

struct StartCase {
    Engine engine;
    Callback where;
    const char *quantity; // a word the message must contain
};

std::string start_case_name(const ::testing::TestParamInfo<StartCase> &info) {
    return std::string(name(info.param.engine)) + "_" + name(info.param.where);
}

class NonFiniteStart : public ::testing::TestWithParam<StartCase> {};

TEST_P(NonFiniteStart, EndsNumericalErrorWithAMessageNamingIt) {
    const StartCase c = GetParam();
    for (Fault f : {Fault::kNaN, Fault::kPosInf}) {
        SCOPED_TRACE(name(f));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = c.where;
        nlp->fault = f;
        nlp->start_mode = true;
        Problem problem(nlp);
        SolveResult r;
        std::string reason;
        if (c.engine == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            ASSERT_NO_THROW(r = solver.solve(problem, interior_start()));
            EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteStart);
        } else {
            SqpSolver solver(quiet_sqp());
            ASSERT_NO_THROW(r = solver.solve(problem, interior_start()));
            EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kNonFiniteStart);
        }
        EXPECT_GE(nlp->fired, 1);
        EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
        EXPECT_NE(r.message.find("non-finite"), std::string::npos) << r.message;
        EXPECT_NE(r.message.find(c.quantity), std::string::npos) << r.message;
        EXPECT_NE(r.message.find("start point"), std::string::npos) << r.message;
        EXPECT_TRUE(r.x == interior_start()) << "the start point is never stepped from";
    }
}

INSTANTIATE_TEST_SUITE_P(
    BothEngines, NonFiniteStart,
    ::testing::Values(StartCase{Engine::kIpm, Callback::kObjective, "objective"},
                      StartCase{Engine::kIpm, Callback::kGradient, "gradient"},
                      StartCase{Engine::kIpm, Callback::kConstraints, "constraint"},
                      StartCase{Engine::kIpm, Callback::kJacobian, "Jacobian"},
                      StartCase{Engine::kIpm, Callback::kHessian, "Hessian"},
                      StartCase{Engine::kSqp, Callback::kObjective, "objective"},
                      StartCase{Engine::kSqp, Callback::kGradient, "gradient"},
                      StartCase{Engine::kSqp, Callback::kConstraints, "constraint"},
                      StartCase{Engine::kSqp, Callback::kJacobian, "Jacobian"},
                      StartCase{Engine::kSqp, Callback::kHessian, "Hessian"}),
    start_case_name);

TEST(StartPointEvaluationError, PropagatesOnBothEnginesFromEveryCallback) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (Callback w : {Callback::kObjective, Callback::kGradient, Callback::kConstraints,
                           Callback::kJacobian, Callback::kHessian}) {
            SCOPED_TRACE(std::string(name(e)) + "_" + name(w));
            auto nlp = std::make_shared<FaultyHs071>();
            nlp->where = w;
            nlp->fault = Fault::kEvaluationError;
            nlp->start_mode = true;
            Problem problem(nlp);
            if (e == Engine::kIpm) {
                IpmSolver solver(quiet_ipm());
                EXPECT_THROW(solver.solve(problem, interior_start()), EvaluationError);
                EXPECT_EQ(solver.last_result().status, SolveStatus::kInterrupted);
                EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kException);
            } else {
                SqpSolver solver(quiet_sqp());
                EXPECT_THROW(solver.solve(problem, interior_start()), EvaluationError);
                EXPECT_EQ(solver.last_result().status, SolveStatus::kInterrupted);
                EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kException);
            }
            EXPECT_EQ(nlp->fired, 1);
        }
    }
}

// --- a foreign exception propagates unchanged; last_result() keeps the progress ---

template <class Solver>
void expect_interrupted_partial(const Solver &solver, const char *what_fragment) {
    const auto &partial = solver.last_result();
    EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
    EXPECT_GE(partial.iterations, 1) << "the fault fires mid-solve";
    ASSERT_EQ(partial.x.size(), 4);
    EXPECT_TRUE(partial.x.allFinite());
    // THE PROBLEM'S ROW SPACE: one multiplier per HS071 row, as a returned result.
    ASSERT_EQ(partial.lambda.size(), 2);
    EXPECT_TRUE(partial.lambda.allFinite());
    // Unmeasured, never zero-filled: the exception may have arrived mid-evaluation.
    EXPECT_EQ(partial.g.size(), 0);
    EXPECT_TRUE(std::isnan(partial.stationarity));
    EXPECT_GT(partial.work.objective_evals, 0);
    EXPECT_GT(partial.work.factorizations, 0);
    EXPECT_NE(partial.message.find("interrupted by an exception that escaped the solve"),
              std::string::npos)
        << partial.message;
    EXPECT_NE(partial.message.find(what_fragment), std::string::npos) << partial.message;
}

TEST(ForeignException, PropagatesUnchangedAndLastResultHoldsTheInterruptedSolve) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        for (Callback w : {Callback::kObjective, Callback::kConstraints, Callback::kHessian}) {
            SCOPED_TRACE(std::string(name(e)) + "_" + name(w));
            auto nlp = std::make_shared<FaultyHs071>();
            nlp->where = w;
            nlp->fault = Fault::kForeign;
            nlp->fire_at = kFireAt;
            Problem problem(nlp);
            const auto check = [&](auto &solver) {
                bool caught = false;
                try {
                    (void)solver.solve(problem, hs071_start());
                } catch (const Sentinel &s) {
                    caught = true;
                    EXPECT_EQ(typeid(s), typeid(Sentinel)) << "the original type, not a wrap";
                    EXPECT_EQ(s.token, kSentinelToken) << "the thrown object";
                }
                ASSERT_TRUE(caught);
                ASSERT_EQ(nlp->fired, 1);
                expect_interrupted_partial(solver, "sentinel from the model");

                // THE BASIC GUARANTEE: the same solver and Problem, fault cleared,
                // solve exactly as a fresh pair does -- and last_result() is
                // emptied by the solve that returned (it holds only a partial).
                nlp->fault = Fault::kNone;
                const auto again = solver.solve(problem, hs071_start());
                auto fresh_nlp = std::make_shared<FaultyHs071>();
                Problem fresh_problem(fresh_nlp);
                std::decay_t<decltype(solver)> fresh(solver.options());
                const auto reference = fresh.solve(fresh_problem, hs071_start());
                EXPECT_EQ(again.status, reference.status);
                EXPECT_EQ(again.iterations, reference.iterations);
                EXPECT_TRUE(again.x == reference.x) << "bitwise the fresh solve's point";
                EXPECT_EQ(solver.last_result().x.size(), 0);
                EXPECT_EQ(solver.last_result().iterations, 0);
                EXPECT_TRUE(solver.last_result().message.empty());
            };
            if (e == Engine::kIpm) {
                IpmSolver solver(quiet_ipm());
                check(solver);
            } else {
                SqpSolver solver(quiet_sqp());
                check(solver);
            }
        }
    }
}

TEST(ForeignException, ANonStdExceptionPropagatesAsThrownOnBothEngines) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kObjective;
        nlp->fault = Fault::kForeignNonStd;
        nlp->fire_at = kFireAt;
        Problem problem(nlp);
        const auto check = [&](auto &solver) {
            bool caught = false;
            try {
                (void)solver.solve(problem, hs071_start());
            } catch (int v) {
                caught = true;
                EXPECT_EQ(v, kNonStdToken);
            }
            ASSERT_TRUE(caught);
            expect_interrupted_partial(solver, "not derived from std::exception");
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver);
            EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kException);
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver);
            EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kException);
        }
    }
}

// THE IPM NARROWS (the declared behaviour change): a standard exception thrown at a
// trial point -- std::domain_error, the "cannot evaluate here" a model may have
// used before B5 -- is no longer absorbed. It propagates, as it always did on the
// SQP engine.
TEST(ForeignException, TheInteriorPointEngineNoLongerAbsorbsAStandardException) {
    struct DomainErrorHs071 final : hven::solvers::Nlp {
        Hs071Problem base;
        mutable long calls = 0;
        hven::solvers::NlpDimensions dimensions() const override { return base.dimensions(); }
        void bounds(hven::VecRef a, hven::VecRef b, hven::VecRef c, hven::VecRef d) const override {
            base.bounds(a, b, c, d);
        }
        double eval_f(hven::ConstVecRef x) const override {
            if (++calls == kFireAt) {
                throw std::domain_error("DomainErrorHs071: outside the domain");
            }
            return base.eval_f(x);
        }
        void eval_grad_f(hven::ConstVecRef x, hven::VecRef g) const override {
            base.eval_grad_f(x, g);
        }
        void eval_g(hven::ConstVecRef x, hven::VecRef g) const override { base.eval_g(x, g); }
        void jacobian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
            base.jacobian_structure(r, c);
        }
        void eval_jacobian(hven::ConstVecRef x, hven::VecRef v) const override {
            base.eval_jacobian(x, v);
        }
        void hessian_structure(hven::IndexVecRef r, hven::IndexVecRef c) const override {
            base.hessian_structure(r, c);
        }
        void eval_hessian(hven::ConstVecRef x, double s, hven::ConstVecRef l,
                          hven::VecRef v) const override {
            base.eval_hessian(x, s, l, v);
        }
    };
    Problem problem(std::make_shared<DomainErrorHs071>());
    IpmSolver solver(quiet_ipm());
    EXPECT_THROW(solver.solve(problem, hs071_start()), std::domain_error);
    EXPECT_EQ(solver.last_result().status, SolveStatus::kInterrupted);
}

// --- a callback-requested stop and an escaped exception are told apart ----------

TEST(InterruptKinds, TheStopReasonAndTheMessageTellTheCallbackFromAnException) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        const auto stop_at_two = [](const IterationEvent &ev) {
            return ev.iteration >= 2 ? CallbackAction::kStop : CallbackAction::kContinue;
        };
        const auto throw_at_two = [](const IterationEvent &ev) {
            if (ev.iteration >= 2) {
                throw Sentinel(kSentinelToken);
            }
            return CallbackAction::kContinue;
        };
        const auto check = [&](auto &solver, auto callback_reason, auto exception_reason) {
            Problem problem(std::make_shared<Hs071Problem>());
            solver.set_iteration_callback(stop_at_two);
            const auto stopped = solver.solve(problem, hs071_start());
            EXPECT_EQ(stopped.status, SolveStatus::kInterrupted);
            EXPECT_EQ(solver.last_stop_reason(), callback_reason);
            EXPECT_EQ(stopped.message, "interrupted by the iteration callback");

            solver.set_iteration_callback(throw_at_two);
            EXPECT_THROW((void)solver.solve(problem, hs071_start()), Sentinel);
            EXPECT_EQ(solver.last_result().status, SolveStatus::kInterrupted);
            EXPECT_EQ(solver.last_stop_reason(), exception_reason);
            EXPECT_EQ(solver.last_result().message,
                      "interrupted by an exception that escaped the solve: sentinel from the "
                      "model");
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver, IpmStopReason::kInterrupted, IpmStopReason::kException);
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver, SqpStopReason::kInterrupted, SqpStopReason::kException);
        }
    }
}

// --- the base, and a trial failure leaves the solver reusable ---------------------

TEST(ExceptionContract, EveryRuntimeFailureHvenRaisesIsAnError) {
    static_assert(std::is_base_of_v<std::runtime_error, Error>);
    static_assert(std::is_base_of_v<Error, EvaluationError>);
    static_assert(std::is_base_of_v<Error, BackendError>);
    const EvaluationError eval("e");
    const BackendError backend("b");
    EXPECT_STREQ(static_cast<const Error &>(eval).what(), "e");
    EXPECT_STREQ(static_cast<const Error &>(backend).what(), "b");
}

TEST(ExceptionContract, AnAbsorbedEvaluationErrorLeavesTheSolverReusable) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kConstraints;
        nlp->fault = Fault::kEvaluationError;
        nlp->fire_at = kFireAt;
        Problem problem(nlp);
        const auto check = [&](auto &solver) {
            const auto first = solver.solve(problem, hs071_start());
            ASSERT_EQ(nlp->fired, 1);
            EXPECT_GE(first.evaluation_errors.count, 1);
            nlp->fault = Fault::kNone;
            const auto again = solver.solve(problem, hs071_start());
            std::decay_t<decltype(solver)> fresh(solver.options());
            Problem fresh_problem(std::make_shared<FaultyHs071>());
            const auto reference = fresh.solve(fresh_problem, hs071_start());
            EXPECT_EQ(again.status, reference.status);
            EXPECT_TRUE(again.x == reference.x);
            EXPECT_EQ(again.evaluation_errors.count, 0) << "the count is per solve";
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver);
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver);
        }
    }
}

// =============================================================================
// THE B5 RIDERS (M8). Each block names the rider it pins.
// =============================================================================

// A locally infeasible problem -- c(x) = x0^2 + x1^2 + 1 = 0 has no real
// solution -- so the interior-point engine's optimality phase enters feasibility
// restoration, and the SQP's requests a restoration sub-solve. The Hessian
// faults on the fire_at-th call with a ZERO objective factor (the interior-point
// engine's restoration modes evaluate it so); `throw_armed` makes the next
// objective or constraint evaluation throw the foreign Sentinel.
class FaultyInfeasible final : public hven::solvers::Nlp {
  public:
    Fault fault = Fault::kNone;
    long fire_at = 0;
    mutable long zero_factor_calls = 0;
    mutable long fired = 0;
    mutable bool throw_armed = false;
    // The objective is NaN within this radius of the origin -- the stationary
    // point of the violation, where a restoration sub-solve ends (0: never).
    double nan_f_radius = 0.0;

    hven::solvers::NlpDimensions dimensions() const override { return {2, 1, 2, 2}; }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        const double inf = std::numeric_limits<double>::infinity();
        xl << -inf, -inf;
        xu << inf, inf;
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(hven::ConstVecRef x) const override {
        maybe_throw();
        if (x.norm() < nan_f_radius) {
            return std::numeric_limits<double>::quiet_NaN();
        }
        return x[0] + x[1];
    }
    void eval_grad_f(hven::ConstVecRef, hven::VecRef g) const override { g << 1.0, 1.0; }
    void eval_g(hven::ConstVecRef x, hven::VecRef g) const override {
        maybe_throw();
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
    void eval_hessian(hven::ConstVecRef, double obj_factor, hven::ConstVecRef lambda,
                      hven::VecRef v) const override {
        v << 2.0 * lambda[0], 2.0 * lambda[0];
        if (fault == Fault::kNone || obj_factor != 0.0 || ++zero_factor_calls != fire_at) {
            return;
        }
        ++fired;
        if (fault == Fault::kForeign) {
            throw Sentinel(kSentinelToken);
        }
        v[0] = std::numeric_limits<double>::quiet_NaN();
    }
    std::string name() const override { return "FaultyInfeasible"; }

  private:
    void maybe_throw() const {
        if (throw_armed) {
            throw_armed = false;
            ++fired;
            throw Sentinel(kSentinelToken);
        }
    }
};

Eigen::VectorXd infeasible_start() {
    Eigen::VectorXd x0(2);
    x0 << 1.0, 0.5;
    return x0;
}

IpmOptions restoring_ipm() {
    IpmOptions o = quiet_ipm();
    o.advanced.restoration_mode = hven::solvers::RestorationMode::kL1Nested;
    o.advanced.restoration_max_entries = 1;
    return o;
}

bool all_zero(const Eigen::VectorXd &v) { return v.size() == 0 || v.isZero(0.0); }

// --- R2 + R7: a chain of rollbacks walks the cap down to its floor -------------

// Every point the solve reaches after the fire_at-th Hessian evaluation faults;
// every point it evaluated before does not. Each rollback lands on a clean point
// and every capped step out of it faults again, so the cap must shrink on every
// rollback -- the reset after an alpha = 0 iteration this rider removes would
// let the same full step repeat until the iteration cap -- and the floor ends
// the phase kNonFiniteIterate, at the point whose evaluation failed, with its
// multipliers cleared.
TEST(RiderRollback, AChainOfRollbacksReachesTheCapFloorAndEndsAtTheFailedPoint) {
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->where = Callback::kHessian;
    nlp->fault = Fault::kNaN;
    nlp->fire_at = kFireAt;
    nlp->sticky = true;
    nlp->fresh_only = true;
    Problem problem(nlp);
    IpmSolver solver(quiet_ipm());
    IpmResult r;
    ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteIterate);
    const Index max_rollbacks = quiet_ipm().advanced.line_search_max_backtracks;
    EXPECT_GE(r.nonfinite_step_rollbacks, 2) << "a CHAIN of rollbacks, not one";
    EXPECT_LE(r.nonfinite_step_rollbacks, max_rollbacks)
        << "each rollback divides the cap by at least backtrack_divisor";
    EXPECT_LT(r.iterations, quiet_ipm().common.max_iterations) << "the floor, not the cap";
    EXPECT_NE(r.message.find("step cap floor"), std::string::npos) << r.message;
    // R7: the point returned is the one whose evaluation failed -- one the model
    // never evaluated cleanly -- and nothing priced there is reported.
    ASSERT_EQ(r.x.size(), 4);
    EXPECT_FALSE(nlp->was_seen_public(r.x)) << "the failed point, not the last clean one";
    EXPECT_TRUE(all_zero(r.lambda));
    EXPECT_TRUE(all_zero(r.z));
}

// R7 on the SQP: the same exit returns the same shape -- the point whose
// evaluation failed, multipliers (row and bound) cleared.
TEST(RiderRollback, BothEnginesReturnTheFailedPointWithMultipliersClearedOnANonFiniteIterate) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kHessian;
        nlp->fault = Fault::kNaN;
        nlp->fire_at = kFireAt;
        nlp->sticky = true; // every point from here on faults, the restored ones too
        Problem problem(nlp);
        SolveResult r;
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
            EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteIterate);
        } else {
            SqpSolver solver(quiet_sqp());
            ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
            EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kNonFiniteIterate);
        }
        EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
        EXPECT_NE(r.message.find("Hessian"), std::string::npos) << r.message;
        EXPECT_EQ(r.message.find("start point"), std::string::npos) << r.message;
        ASSERT_EQ(r.x.size(), 4);
        EXPECT_TRUE(r.x.allFinite());
        EXPECT_TRUE(all_zero(r.lambda)) << "no price at the failed point is evidence";
        EXPECT_TRUE(all_zero(r.z));
    }
}

// --- R4 / the no-rollback doors: an active restoration -------------------------

TEST(RiderRollback, AFailureWhileRestorationIsActiveIsNeverRolledBack) {
    auto nlp = std::make_shared<FaultyInfeasible>();
    nlp->fault = Fault::kNaN;
    nlp->fire_at = 2; // a point a restoration step reached
    Problem problem(nlp);
    IpmSolver solver(restoring_ipm());
    IpmResult r;
    ASSERT_NO_THROW(r = solver.solve(problem, infeasible_start()));
    ASSERT_EQ(nlp->fired, 1) << "the fault never reached restoration";
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteIterate);
    EXPECT_EQ(r.nonfinite_step_rollbacks, 0) << "the snapshot carries no elastic variables";
    ASSERT_TRUE(r.last_feas_rest_entries.has_value());
    EXPECT_EQ(*r.last_feas_rest_entries, 1);
    EXPECT_TRUE(all_zero(r.lambda));
}

// --- the no-rollback doors: a phase's first iterate ----------------------------

// Arms the model's next Hessian evaluation when the second phase begins: its
// first evaluation is that phase's first iterate.
struct PhaseTwoArmingSink final : hven::solvers::TraceSink {
    std::shared_ptr<FaultyHs071> nlp;
    void on_ipm_phase_begin(const hven::solvers::IpmPhaseTraceEvent &event) override {
        if (event.phase == 1) {
            nlp->fire_at = nlp->calls + 1;
        }
    }
};

// The second phase's first iterate: nothing this phase stepped, so nothing to
// roll back -- and it is not the start point, so the solve ends
// kNonFiniteIterate.
TEST(RiderRollback, AFailureAtAPhasesFirstIterateEndsTheSolveWithoutARollback) {
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->where = Callback::kHessian;
    nlp->fault = Fault::kNaN;
    nlp->fire_at = std::numeric_limits<long>::max();
    Problem problem(nlp);
    IpmOptions o = quiet_ipm();
    o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
    IpmSolver solver(o);
    PhaseTwoArmingSink sink;
    sink.nlp = nlp;
    solver.attach_trace(&sink);
    IpmResult r;
    ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
    ASSERT_EQ(nlp->fired, 1);
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteIterate);
    EXPECT_EQ(r.nonfinite_step_rollbacks, 0);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_TRUE(r.phases[1].ran);
    EXPECT_EQ(r.message.find("start point"), std::string::npos) << r.message;
}

// --- R3: the start point is screened before anything is factorized -------------

TEST(RiderStartScreen, ANonFiniteStartJacobianNeverReachesTheFactorization) {
    for (Fault f : {Fault::kNaN, Fault::kPosInf}) {
        SCOPED_TRACE(name(f));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kJacobian;
        nlp->fault = f;
        nlp->start_mode = true;
        Problem problem(nlp);
        IpmSolver solver(quiet_ipm());
        IpmResult r;
        ASSERT_NO_THROW(r = solver.solve(problem, interior_start()));
        EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
        EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteStart);
        EXPECT_EQ(r.work.factorizations, 0) << "the initial factorization never ran";
        EXPECT_NE(r.message.find("Jacobian"), std::string::npos) << r.message;
        EXPECT_TRUE(all_zero(r.lambda));
        // AND THE SOLVER IS NOT LEFT CLAIMING AN ANALYSIS IT NEVER RAN: the next
        // solve, fault cleared, is a fresh pair's.
        nlp->fault = Fault::kNone;
        const IpmResult again = solver.solve(problem, interior_start());
        IpmSolver fresh(quiet_ipm());
        Problem fresh_problem(std::make_shared<FaultyHs071>());
        const IpmResult reference = fresh.solve(fresh_problem, interior_start());
        EXPECT_EQ(again.status, SolveStatus::kOptimal);
        EXPECT_EQ(again.iterations, reference.iterations);
        EXPECT_TRUE(again.x == reference.x);
    }
}

// The initial factorization holds no Hessian (it is the multiplier estimate's
// augmented system), and the first iterate's screen ends the solve before the
// main loop factorizes one: a non-finite start Hessian never reaches a factor.
TEST(RiderStartScreen, ANonFiniteStartHessianNeverReachesTheFactorization) {
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->where = Callback::kHessian;
    nlp->fault = Fault::kNaN;
    nlp->start_mode = true;
    Problem problem(nlp);
    IpmSolver solver(quiet_ipm());
    int hook_calls = 0;
    solver.set_kkt_hook([&hook_calls](int, double, const Eigen::Ref<const Eigen::VectorXd> &,
                                      double, const Eigen::Ref<const Eigen::VectorXd> &,
                                      const Eigen::Ref<const Eigen::VectorXd> &,
                                      Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
        ++hook_calls;
        return 0;
    });
    IpmResult r;
    ASSERT_NO_THROW(r = solver.solve(problem, interior_start()));
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNonFiniteStart);
    EXPECT_LE(r.work.factorizations, 1) << "the Hessian-free initial factorization at most";
    EXPECT_EQ(hook_calls, 0) << "the screen ends the iteration before the KKT hook sees it";
}

// --- foreign exceptions from inside restoration, a trace sink, the KKT hook -----

template <class Solver> void expect_interrupted_with_sentinel(const Solver &solver) {
    const auto &partial = solver.last_result();
    EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
    EXPECT_NE(partial.message.find("interrupted by an exception that escaped the solve"),
              std::string::npos)
        << partial.message;
    EXPECT_NE(partial.message.find("sentinel from the model"), std::string::npos)
        << partial.message;
    EXPECT_TRUE(partial.x.allFinite());
}

template <class Solver, class Call> void expect_sentinel(Solver &solver, Call &&call) {
    bool caught = false;
    try {
        call();
    } catch (const Sentinel &s) {
        caught = true;
        EXPECT_EQ(s.token, kSentinelToken);
    }
    EXPECT_TRUE(caught) << "the original object, unchanged";
    expect_interrupted_with_sentinel(solver);
}

TEST(RiderForeignException, ThrownInsideRestorationPropagatesUnchangedOnBothEngines) {
    {
        SCOPED_TRACE("ipm");
        auto nlp = std::make_shared<FaultyInfeasible>();
        nlp->fault = Fault::kForeign;
        nlp->fire_at = 2;
        Problem problem(nlp);
        IpmSolver solver(restoring_ipm());
        expect_sentinel(solver, [&] { (void)solver.solve(problem, infeasible_start()); });
        EXPECT_EQ(nlp->fired, 1);
        EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kException);
        EXPECT_GE(solver.last_result().iterations, 1);
    }
    {
        SCOPED_TRACE("sqp");
        auto nlp = std::make_shared<FaultyInfeasible>();
        Problem problem(nlp);
        SqpSolver solver(quiet_sqp());
        // The sub-solve's own iterations report depth 1: the first of them arms
        // the model, whose next evaluation -- inside the sub-solve -- throws.
        solver.set_iteration_callback(
            [&nlp](const IterationEvent &ev) {
                if (ev.depth.value_or(0) == 1 && nlp->fired == 0) {
                    nlp->throw_armed = true;
                }
                return CallbackAction::kContinue;
            },
            hven::solvers::IterationPayload::kScalars,
            hven::solvers::CallbackScope::kIncludeRestoration);
        expect_sentinel(solver, [&] { (void)solver.solve(problem, infeasible_start()); });
        EXPECT_EQ(nlp->fired, 1) << "the restoration sub-solve never ran";
        EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kException);
    }
}

// A sink that throws on its third iteration line.
struct ThrowingSink final : hven::solvers::TraceSink {
    int calls = 0;
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { tick(); }
    void on_sqp_major(const hven::solvers::SqpMajorTraceEvent &) override { tick(); }
    void tick() {
        if (++calls == 3) {
            throw Sentinel(kSentinelToken);
        }
    }
};

TEST(RiderForeignException, ThrownByATraceSinkPropagatesUnchangedOnBothEngines) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        Problem problem(std::make_shared<FaultyHs071>());
        ThrowingSink sink;
        const auto check = [&](auto &solver) {
            solver.attach_trace(&sink);
            expect_sentinel(solver, [&] { (void)solver.solve(problem, hs071_start()); });
            EXPECT_EQ(sink.calls, 3);
            EXPECT_GE(solver.last_result().iterations, 2);
            // THE BASIC GUARANTEE, and the sink may be replaced after the throw.
            solver.attach_trace(nullptr);
            const auto again = solver.solve(problem, hs071_start());
            EXPECT_EQ(again.status, SolveStatus::kOptimal);
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver);
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver);
        }
    }
}

TEST(RiderForeignException, ThrownByTheKktHookPropagatesUnchanged) {
    Problem problem(std::make_shared<FaultyHs071>());
    IpmSolver solver(quiet_ipm());
    int calls = 0;
    solver.set_kkt_hook([&calls](int, double, const Eigen::Ref<const Eigen::VectorXd> &, double,
                                 const Eigen::Ref<const Eigen::VectorXd> &,
                                 const Eigen::Ref<const Eigen::VectorXd> &,
                                 Eigen::SparseMatrix<double, Eigen::RowMajor> &) -> int {
        if (++calls == 3) {
            throw Sentinel(kSentinelToken);
        }
        return 0;
    });
    expect_sentinel(solver, [&] { (void)solver.solve(problem, hs071_start()); });
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kException);
    EXPECT_GE(solver.last_result().iterations, 2);
    solver.clear_kkt_hook();
    EXPECT_EQ(solver.solve(problem, hs071_start()).status, SolveStatus::kOptimal);
}

// --- the thread pool's policy: a foreign exception from ANY lane wins -----------

void store_thrown(hven::utils::detail::DispatchContext &ctx, const std::function<void()> &f) {
    try {
        f();
    } catch (...) {
        ctx.store_exception();
    }
}

std::exception_ptr thrown(const std::function<void()> &f) {
    try {
        f();
    } catch (...) {
        return std::current_exception();
    }
    return nullptr;
}

TEST(RiderThreadPool, AForeignExceptionFromAnyLaneIsRethrownOverAnEvaluationError) {
    const auto eval = [] { throw EvaluationError("lane cannot evaluate"); };
    const auto foreign = [] { throw Sentinel(kSentinelToken); };
    {
        SCOPED_TRACE("pool lanes: evaluation error first, foreign second");
        hven::utils::detail::DispatchContext ctx(0);
        store_thrown(ctx, eval);
        store_thrown(ctx, foreign);
        EXPECT_THROW(ctx.rethrow_if_exception(), Sentinel);
    }
    {
        SCOPED_TRACE("pool lane an evaluation error, the calling thread's lane foreign");
        hven::utils::detail::DispatchContext ctx(0);
        store_thrown(ctx, eval);
        EXPECT_THROW(ctx.rethrow_if_exception(thrown(foreign)), Sentinel);
    }
    {
        SCOPED_TRACE("a full set of pool evaluation errors cannot hide the inline foreign one");
        hven::utils::detail::DispatchContext ctx(0);
        for (int k = 0; k < hven::utils::detail::DispatchContext::kMaxCaptured + 3; ++k) {
            store_thrown(ctx, eval);
        }
        EXPECT_THROW(ctx.rethrow_if_exception(thrown(foreign)), Sentinel);
    }
    {
        SCOPED_TRACE("only evaluation errors: one of them, unchanged");
        hven::utils::detail::DispatchContext ctx(0);
        store_thrown(ctx, eval);
        EXPECT_THROW(ctx.rethrow_if_exception(thrown(eval)), EvaluationError);
    }
    {
        SCOPED_TRACE("nothing thrown: nothing rethrown");
        hven::utils::detail::DispatchContext ctx(0);
        EXPECT_NO_THROW(ctx.rethrow_if_exception());
    }
}

// --- R8 + R9: last_result() holds only a partial ----------------------------------

TEST(RiderLastResult, AnArgumentRefusalLeavesAPartialAndASuccessLeavesItEmpty) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        Problem problem(std::make_shared<FaultyHs071>());
        const Eigen::VectorXd wrong = Eigen::VectorXd::Ones(3);
        const auto check = [&](auto &solver) {
            EXPECT_THROW((void)solver.solve(problem, wrong), std::invalid_argument);
            const auto &partial = solver.last_result();
            EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
            EXPECT_EQ(partial.iterations, 0);
            // R9: the fixed words are set before anything that can fail -- the
            // mis-sized point cannot be mapped into the problem's row space.
            EXPECT_NE(partial.message.find("interrupted by an exception that escaped the solve"),
                      std::string::npos)
                << partial.message;
            const auto ok = solver.solve(problem, hs071_start());
            EXPECT_EQ(ok.status, SolveStatus::kOptimal);
            EXPECT_EQ(solver.last_result().x.size(), 0) << "a returned solve leaves it empty";
            EXPECT_TRUE(solver.last_result().message.empty());
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver);
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver);
        }
    }
}

TEST(RiderLastResult, SetOptionsIsAcceptedAfterAThrow) {
    for (Engine e : {Engine::kIpm, Engine::kSqp}) {
        SCOPED_TRACE(name(e));
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kObjective;
        nlp->fault = Fault::kForeign;
        nlp->fire_at = kFireAt;
        Problem problem(nlp);
        const auto check = [&](auto &solver, auto options) {
            EXPECT_THROW((void)solver.solve(problem, hs071_start()), Sentinel);
            options.common.max_iterations = 200;
            EXPECT_NO_THROW(solver.set_options(options)) << "no solve is left in flight";
            EXPECT_EQ(solver.options().common.max_iterations, 200);
            nlp->fault = Fault::kNone;
            EXPECT_EQ(solver.solve(problem, hs071_start()).status, SolveStatus::kOptimal);
        };
        if (e == Engine::kIpm) {
            IpmSolver solver(quiet_ipm());
            check(solver, quiet_ipm());
        } else {
            SqpSolver solver(quiet_sqp());
            check(solver, quiet_sqp());
        }
    }
}

// --- the interrupted path through the problem-shape seams ----------------------

// A fixed variable held by an internal equality row (MakeConstraint): the
// partial is in the DECLARED space -- one multiplier per declared row, the
// fixing row's off it, the fixed coordinate at its value.
TEST(RiderInterrupted, AFixedVariableUnderMakeConstraintIsReportedInTheDeclaredSpace) {
    auto nlp = std::make_shared<FaultyHs071>();
    nlp->fix_x0 = true;
    nlp->where = Callback::kObjective;
    nlp->fault = Fault::kForeign;
    nlp->fire_at = kFireAt;
    Problem problem(nlp);
    IpmOptions o = quiet_ipm();
    o.fixed_variable_treatment = hven::solvers::FixedVariableTreatment::kMakeConstraint;
    IpmSolver solver(o);
    EXPECT_THROW((void)solver.solve(problem, hs071_start()), Sentinel);
    ASSERT_EQ(nlp->fired, 1);
    const IpmResult &partial = solver.last_result();
    EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
    EXPECT_GE(partial.iterations, 1);
    ASSERT_EQ(partial.x.size(), 4);
    EXPECT_TRUE(partial.x.allFinite());
    ASSERT_EQ(partial.lambda.size(), 2) << "the fixing row is the treatment's, not a declared row";
    EXPECT_TRUE(partial.lambda.allFinite());
    EXPECT_EQ(partial.z.size(), 4);
}

// An objective scale: the partial's multipliers are in the CALLER's units --
// the units the iteration callback was shown the same iterate in. The callback
// at iteration 3 arms the fault for the next objective evaluation, a trial of
// that iteration's line search, so the partial's point is the iterate the
// callback was just shown, and its multipliers must be the ones shown beside it
// -- at scale 1 and at scale 4.
TEST(RiderInterrupted, AnObjectiveScaledSolveIsReportedInTheCallersUnits) {
    for (double scale : {1.0, 4.0}) {
        SCOPED_TRACE(scale);
        auto nlp = std::make_shared<FaultyHs071>();
        nlp->where = Callback::kObjective;
        nlp->fault = Fault::kForeign;
        nlp->fire_at = std::numeric_limits<long>::max();
        Problem problem(nlp);
        IpmOptions o = quiet_ipm();
        o.objective_scale = scale;
        IpmSolver solver(o);
        std::vector<std::pair<Eigen::VectorXd, Eigen::VectorXd>> shown;
        solver.set_iteration_callback([&](const IterationEvent &ev) {
            shown.emplace_back(Eigen::VectorXd(ev.x), Eigen::VectorXd(ev.lambda));
            if (ev.iteration == 3) {
                nlp->fire_at = nlp->calls + 1;
            }
            return CallbackAction::kContinue;
        });
        EXPECT_THROW((void)solver.solve(problem, hs071_start()), Sentinel);
        ASSERT_EQ(nlp->fired, 1);
        const IpmResult &partial = solver.last_result();
        EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
        ASSERT_EQ(partial.lambda.size(), 2);
        const Eigen::VectorXd *lambda_shown = nullptr;
        for (const auto &[x, lambda] : shown) {
            if (x == partial.x) {
                lambda_shown = &lambda;
            }
        }
        ASSERT_NE(lambda_shown, nullptr) << "the partial's point is an iterate the callback saw";
        EXPECT_LE((partial.lambda - *lambda_shown).lpNorm<Eigen::Infinity>(),
                  1e-12 * std::max(1.0, lambda_shown->lpNorm<Eigen::Infinity>()))
            << "partial " << partial.lambda.transpose() << " vs shown "
            << lambda_shown->transpose();
    }
}

// --- SqpStopReason::kRestorationNonFinite, reached without a seam --------------

// The restoration sub-solve minimizes the violation alone and ends near the
// origin, where the objective -- which the sub-solve never reads -- is NaN: the
// main model cannot be measured there, so the requesting point is returned
// with its multipliers cleared.
TEST(RiderRestorationStop, ARestorationEndingWhereTheModelIsNotFiniteIsReportedAsSuch) {
    auto nlp = std::make_shared<FaultyInfeasible>();
    nlp->nan_f_radius = 0.05;
    Problem problem(nlp);
    SqpSolver solver(quiet_sqp());
    SqpResult r;
    ASSERT_NO_THROW(r = solver.solve(problem, infeasible_start()));
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kRestorationNonFinite) << r.message;
    EXPECT_TRUE(r.x.allFinite());
    EXPECT_GE(r.x.norm(), nlp->nan_f_radius) << "the requesting point, not the restored one";
    EXPECT_TRUE(all_zero(r.lambda));
}

} // namespace
