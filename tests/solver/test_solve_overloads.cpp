// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-4 G4: the overload set of solve() on both engines.
//
//  * `solve(p, x0, {})` and `solve(p, x0, {}, budget)` keep selecting the WarmStart
//    overload (a braced list does not deduce the budget template).
//  * `solve(p, x0, SolveBudget{...})` and `solve(p, x0, budget_lvalue)` select the
//    cold-start-with-budget overload, and equal the explicit WarmStart{} spelling.
//  * `{.stop = t}` alone, and anything that is not a SolveBudget, does not select it.

#include <concepts>
#include <memory>
#include <type_traits>

#include <gtest/gtest.h>

#include <hven/ipm/ipm_solver.h>
#include <hven/model/problem.h>
#include <hven/sqp/sqp_solver.h>

#include "model/support/hs071_problem.h"

namespace {

using hven::ConstVecRef;
using hven::Problem;
using hven::SolveBudget;
using hven::SolveStatus;
using hven::WarmStart;

template <class S, class... A>
concept CanSolve =
    requires(S &s, Problem &p, ConstVecRef x0, A &&...a) { s.solve(p, x0, std::forward<A>(a)...); };

// The compile-time half. `{}` is not a type, so it is probed through a lambda-free
// spelling: a default-constructed WarmStart is the closest stand-in.
static_assert(CanSolve<hven::IpmSolver>);
static_assert(CanSolve<hven::SqpSolver>);
static_assert(CanSolve<hven::IpmSolver, SolveBudget>);
static_assert(CanSolve<hven::SqpSolver, SolveBudget>);
static_assert(CanSolve<hven::IpmSolver, const SolveBudget &>);
static_assert(CanSolve<hven::SqpSolver, SolveBudget &>);
static_assert(CanSolve<hven::IpmSolver, WarmStart>);
static_assert(CanSolve<hven::SqpSolver, WarmStart>);
static_assert(CanSolve<hven::IpmSolver, WarmStart, SolveBudget>);
static_assert(CanSolve<hven::SqpSolver, WarmStart, SolveBudget>);
// Not a SolveBudget, not a warm start: refused at overload resolution.
static_assert(!CanSolve<hven::IpmSolver, int>);
static_assert(!CanSolve<hven::SqpSolver, double>);

Eigen::VectorXd hs071_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

template <class Solver> class SolveOverloads : public ::testing::Test {};
using Engines = ::testing::Types<hven::IpmSolver, hven::SqpSolver>;
TYPED_TEST_SUITE(SolveOverloads, Engines);

TYPED_TEST(SolveOverloads, ABareBraceSelectsTheWarmStartOverloadAndSolvesUnlimited) {
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    TypeParam s;
    const auto r = s.solve(p, hs071_start(), {});
    EXPECT_EQ(r.status, SolveStatus::kOptimal);
    const auto r2 = s.solve(p, hs071_start(), {}, {});
    EXPECT_EQ(r2.iterations, r.iterations);
}

TYPED_TEST(SolveOverloads, ASolveBudgetPrvalueAndLvalueSelectTheBudgetOverload) {
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    TypeParam s;
    const auto free_run = s.solve(p, hs071_start());
    ASSERT_EQ(free_run.status, SolveStatus::kOptimal);

    // A binding budget: the overload must have delivered it to the engine.
    const auto cut = s.solve(p, hs071_start(), SolveBudget{.max_iterations = 1});
    EXPECT_EQ(cut.status, SolveStatus::kIterationLimit);

    SolveBudget lvalue{.max_iterations = 1};
    const auto cut2 = s.solve(p, hs071_start(), lvalue);
    EXPECT_EQ(cut2.status, SolveStatus::kIterationLimit);
    EXPECT_EQ(cut2.iterations, cut.iterations);

    const auto spelled = s.solve(p, hs071_start(), WarmStart{}, SolveBudget{.max_iterations = 1});
    EXPECT_EQ(spelled.status, cut.status);
    EXPECT_EQ(spelled.iterations, cut.iterations);
    EXPECT_EQ((spelled.x - cut.x).norm(), 0.0);

    // A default budget is the unlimited solve.
    const auto none = s.solve(p, hs071_start(), SolveBudget{});
    EXPECT_EQ(none.iterations, free_run.iterations);
}

TYPED_TEST(SolveOverloads, TheBudgetIsValidatedOnTheNewOverloadToo) {
    Problem p(std::make_shared<hven_model_tests::Hs071Problem>());
    TypeParam s;
    EXPECT_THROW(s.solve(p, hs071_start(), SolveBudget{.max_iterations = 0}),
                 std::invalid_argument);
}

} // namespace
