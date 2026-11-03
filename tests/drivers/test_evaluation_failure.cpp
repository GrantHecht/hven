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

#include <cmath>
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
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/solve_result.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

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
class FaultyHs071 final : public hven::solvers::Nlp {
  public:
    Callback where = Callback::kObjective;
    Fault fault = Fault::kNone;
    long fire_at = 0;
    bool start_mode = false;
    // The start point start mode faults at, exactly as the engine evaluates it.
    Eigen::VectorXd start = interior_start();
    mutable long calls = 0;
    mutable long fired = 0;

    hven::solvers::NlpDimensions dimensions() const override { return base_.dimensions(); }
    void bounds(hven::VecRef xl, hven::VecRef xu, hven::VecRef gl, hven::VecRef gu) const override {
        base_.bounds(xl, xu, gl, gu);
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
        if (fires(Callback::kHessian, x)) {
            v[2] = poison();
        }
    }
    std::string name() const override { return "FaultyHs071"; }

  private:
    // Counts the call, decides whether it faults, and throws for the throwing
    // faults; a value fault returns true and the caller writes poison().
    bool fires(Callback c, hven::ConstVecRef x) const {
        if (c != where || fault == Fault::kNone) {
            return false;
        }
        ++calls;
        const bool hit = start_mode ? (x - start).cwiseAbs().maxCoeff() == 0.0 : calls == fire_at;
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
                // solve exactly as a fresh pair does -- and last_result() is the
                // new solve's.
                nlp->fault = Fault::kNone;
                const auto again = solver.solve(problem, hs071_start());
                auto fresh_nlp = std::make_shared<FaultyHs071>();
                Problem fresh_problem(fresh_nlp);
                std::decay_t<decltype(solver)> fresh(solver.options());
                const auto reference = fresh.solve(fresh_problem, hs071_start());
                EXPECT_EQ(again.status, reference.status);
                EXPECT_EQ(again.iterations, reference.iterations);
                EXPECT_TRUE(again.x == reference.x) << "bitwise the fresh solve's point";
                EXPECT_EQ(solver.last_result().status, again.status);
                EXPECT_TRUE(solver.last_result().x == again.x);
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

} // namespace
