// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 AP-2 B3: the shared options' limits and their validation, on both engines.
//
//  * The work ceiling (CommonOptions::max_factorizations) ends a solve with
//    kWorkLimit at an iteration boundary; the time limit (time_limit_seconds)
//    with kTimeLimit. Both empty by default, and an empty or never-binding
//    limit is the unlimited solve, bit for bit.
//  * The SQP minor budget (SolveBudget::minor_budget) reports kWorkLimit.
//  * Each refused value is refused by name: CommonOptions' new fields at
//    validate(), a SolveBudget member below 1 at solve entry, the reserved
//    dense-arm values on both engines.
//  * Silent by default: a default-built solver of either engine writes nothing
//    to stdout during a solve.

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/problem.h"
#include "support/hs071_problem.h"

namespace {

using hven::solvers::CommonOptions;
using hven::solvers::IpmOptions;
using hven::solvers::IpmResult;
using hven::solvers::IpmSolver;
using hven::solvers::Problem;
using hven::solvers::SolveBudget;
using hven::solvers::SolveStatus;
using hven::solvers::SqpOptions;
using hven::solvers::SqpResult;
using hven::solvers::SqpSolver;
using hven::solvers::Verbosity;

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
constexpr double kInf = std::numeric_limits<double>::infinity();

Eigen::VectorXd hs071_start() {
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    return x0;
}

std::shared_ptr<hven_drivers_tests::Hs071Problem> hs071() {
    return std::make_shared<hven_drivers_tests::Hs071Problem>();
}

IpmResult ipm_solve(const IpmOptions &o, SolveBudget budget = {}) {
    Problem p(hs071());
    IpmSolver s(o);
    return s.solve(p, hs071_start(), budget);
}

SqpResult sqp_solve(const SqpOptions &o, SolveBudget budget = {}) {
    Problem p(hs071());
    SqpSolver s(o);
    return s.solve(p, hs071_start(), budget);
}

// validate() refuses the value and names the field.
template <typename Options> void expect_refused(const Options &o, const std::string &field) {
    try {
        hven::solvers::validate(o);
        ADD_FAILURE() << "expected " << field << " to be refused";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find(field), std::string::npos) << e.what();
    }
}

template <typename Options> void refuses_the_common_values() {
    {
        Options o;
        o.common.infinite_bound = 0.0;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = -1.0;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = kNaN;
        expect_refused(o, "common.infinite_bound");
        o.common.infinite_bound = kInf; // "every finite value is a real bound"
        EXPECT_NO_THROW(hven::solvers::validate(o));
    }
    for (const double bad : {0.0, -1.0, kNaN, kInf}) {
        Options o;
        o.common.time_limit_seconds = bad;
        expect_refused(o, "common.time_limit_seconds");
    }
    {
        Options o;
        o.common.max_factorizations = -1;
        expect_refused(o, "common.max_factorizations");
        o.common.max_factorizations = 0; // legal: stop at the first boundary
        EXPECT_NO_THROW(hven::solvers::validate(o));
    }
    for (const double bad : {0.0, -1e-6, kNaN, kInf}) {
        Options o;
        o.common.optimality_tol = bad;
        expect_refused(o, "common.optimality_tol");
        Options f;
        f.common.feasibility_tol = bad;
        expect_refused(f, "common.feasibility_tol");
    }
    {
        Options o;
        o.common.verbosity = static_cast<Verbosity>(4);
        expect_refused(o, "common.verbosity");
    }
    {
        Options o;
        o.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
        expect_refused(o, "linear.dense_factorization");
        o.linear.dense_factorization = hven::linear::DensePolicy::kAuto;
        expect_refused(o, "linear.dense_factorization");
        Options d;
        d.linear.dense_max_dimension = 50;
        expect_refused(d, "linear.dense_max_dimension");
    }
}

} // namespace

TEST(Limits, IpmValidateRefusesTheNewCommonValues) { refuses_the_common_values<IpmOptions>(); }

TEST(Limits, SqpValidateRefusesTheNewCommonValues) { refuses_the_common_values<SqpOptions>(); }

TEST(Limits, IpmValidateRefusesTheTerminationAndLinearValues) {
    for (const double bad : {0.0, -1.0, kNaN, kInf}) {
        IpmOptions o;
        o.termination.complementarity_tol = bad;
        expect_refused(o, "termination.complementarity_tol");
    }
    {
        IpmOptions o;
        o.termination.complementarity_tol = 1e-1; // above acceptable_barrier_tol
        expect_refused(o, "termination.acceptable_barrier_tol");
    }
    {
        IpmOptions o;
        o.common.max_iterations = 0;
        expect_refused(o, "common.max_iterations");
    }
    {
        IpmOptions o;
        o.linear.backend_output = true;
        expect_refused(o, "linear.backend_output");
    }
    {
        IpmOptions o;
        o.linear.parallel_solve = static_cast<hven::solvers::KktSolveParallelism>(2);
        expect_refused(o, "linear.parallel_solve");
    }
    {
        IpmOptions o;
        o.advanced.acceptance_strategy = static_cast<hven::solvers::AcceptanceKind>(9);
        expect_refused(o, "advanced.acceptance_strategy");
    }
}

TEST(Limits, ABudgetMemberBelowOneIsRefusedAtSolveEntryOnBothEngines) {
    const SolveBudget no_iterations{.max_iterations = 0};
    const SolveBudget no_minors{.minor_budget = 0};
    for (const SolveBudget &b : {no_iterations, no_minors}) {
        EXPECT_THROW(ipm_solve(IpmOptions{}, b), std::invalid_argument);
        EXPECT_THROW(sqp_solve(SqpOptions{}, b), std::invalid_argument);
    }
    EXPECT_THROW(hven::solvers::validate(SolveBudget{.max_iterations = -3}), std::invalid_argument);
    EXPECT_NO_THROW(hven::solvers::validate(SolveBudget{}));
    EXPECT_NO_THROW(hven::solvers::validate(SolveBudget{.max_iterations = 1, .minor_budget = 1}));
}

// ---------------------------------------------------------------------------
// The work ceiling
// ---------------------------------------------------------------------------

TEST(Limits, IpmWorkLimitStopsAtTheBoundaryWithTheCurrentPoint) {
    const IpmResult free_run = ipm_solve(IpmOptions{});
    ASSERT_EQ(free_run.status, SolveStatus::kOptimal);
    const hven::Index free_facts = free_run.kkt_factor_counters.factorize_count;
    ASSERT_GT(free_facts, 4);

    IpmOptions o;
    o.common.max_factorizations = 3;
    const IpmResult capped = ipm_solve(o);
    EXPECT_EQ(capped.status, SolveStatus::kWorkLimit);
    ASSERT_EQ(capped.phases.size(), 1u);
    EXPECT_EQ(capped.phases[0].stop_reason, hven::solvers::IpmStopReason::kWorkLimit);
    EXPECT_GE(capped.kkt_factor_counters.factorize_count, 3);
    EXPECT_LT(capped.kkt_factor_counters.factorize_count, free_facts);
    EXPECT_LT(capped.iterations, free_run.iterations);
    EXPECT_TRUE(capped.x.allFinite());

    // 0 stops at the first boundary.
    o.common.max_factorizations = 0;
    const IpmResult none = ipm_solve(o);
    EXPECT_EQ(none.status, SolveStatus::kWorkLimit);
    EXPECT_LE(none.iterations, 1);
}

TEST(Limits, AnIpmWorkLimitEndsThePhaseSequence) {
    IpmOptions o;
    o.phases = {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize};
    o.common.max_factorizations = 0;
    const IpmResult r = ipm_solve(o);
    EXPECT_EQ(r.status, SolveStatus::kWorkLimit);
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_FALSE(r.phases[1].ran);
}

TEST(Limits, ANeverBindingIpmLimitIsTheUnlimitedSolveBitForBit) {
    const IpmResult free_run = ipm_solve(IpmOptions{});
    IpmOptions o;
    o.common.max_factorizations = 1'000'000;
    o.common.time_limit_seconds = 1.0e6;
    const IpmResult limited = ipm_solve(o);
    EXPECT_EQ(limited.status, free_run.status);
    EXPECT_EQ(limited.iterations, free_run.iterations);
    EXPECT_EQ(limited.kkt_factor_counters.factorize_count,
              free_run.kkt_factor_counters.factorize_count);
    for (Eigen::Index i = 0; i < free_run.x.size(); ++i) {
        EXPECT_EQ(limited.x[i], free_run.x[i]) << i;
    }
}

TEST(Limits, SqpWorkLimitStopsAtTheBoundaryWithTheCurrentPoint) {
    const SqpResult free_run = sqp_solve(SqpOptions{});
    ASSERT_EQ(free_run.status, SolveStatus::kOptimal);
    ASSERT_GT(free_run.counters.factorizations, 2);

    SqpOptions o;
    o.common.max_factorizations = 1;
    const SqpResult capped = sqp_solve(o);
    EXPECT_EQ(capped.status, SolveStatus::kWorkLimit);
    EXPECT_GE(capped.counters.factorizations, 1);
    EXPECT_LT(capped.counters.major_iters, free_run.counters.major_iters);
    EXPECT_EQ(capped.counters.probe_budget_stops, 0);
    EXPECT_TRUE(capped.x.allFinite());

    o.common.max_factorizations = 0;
    const SqpResult none = sqp_solve(o);
    EXPECT_EQ(none.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(none.counters.factorizations, 0);
    EXPECT_EQ(none.counters.major_iters, 0);
}

TEST(Limits, ANeverBindingSqpLimitIsTheUnlimitedSolveBitForBit) {
    const SqpResult free_run = sqp_solve(SqpOptions{});
    SqpOptions o;
    o.common.max_factorizations = 1'000'000;
    o.common.time_limit_seconds = 1.0e6;
    const SqpResult limited = sqp_solve(o);
    EXPECT_EQ(limited.status, free_run.status);
    EXPECT_EQ(limited.counters.major_iters, free_run.counters.major_iters);
    EXPECT_EQ(limited.counters.factorizations, free_run.counters.factorizations);
    EXPECT_EQ(limited.counters.qp_minor_iters, free_run.counters.qp_minor_iters);
    for (Eigen::Index i = 0; i < free_run.x.size(); ++i) {
        EXPECT_EQ(limited.x[i], free_run.x[i]) << i;
    }
}

// The SQP minor budget is a work ceiling and reports kWorkLimit (owner decision
// 5's relabel; it reported kMaxIter before M8 AP-2 B3). The point is unchanged.
TEST(Limits, TheSqpMinorBudgetReportsWorkLimit) {
    const SqpResult r = sqp_solve(SqpOptions{}, SolveBudget{.minor_budget = 1});
    EXPECT_EQ(r.status, SolveStatus::kWorkLimit);
    EXPECT_EQ(r.counters.probe_budget_stops, 1);
}

// ---------------------------------------------------------------------------
// The time limit. Wall-clock: the only stable assertion is the label, at a
// limit no solve can meet. Never used in a measured run.
// ---------------------------------------------------------------------------

TEST(Limits, AnUnmeetableTimeLimitReportsTimeLimitOnBothEngines) {
    IpmOptions io;
    io.common.time_limit_seconds = std::numeric_limits<double>::denorm_min();
    EXPECT_EQ(ipm_solve(io).status, SolveStatus::kTimeLimit);

    SqpOptions so;
    so.common.time_limit_seconds = std::numeric_limits<double>::denorm_min();
    const SqpResult r = sqp_solve(so);
    EXPECT_EQ(r.status, SolveStatus::kTimeLimit);
    EXPECT_EQ(r.counters.major_iters, 0);
}

// ---------------------------------------------------------------------------
// Silent by default (owner decision 4)
// ---------------------------------------------------------------------------

TEST(Verbosity, DefaultSolversOfBothEnginesWriteNothingToStdout) {
    std::fflush(stdout);
    testing::internal::CaptureStdout();
    {
        Problem p(hs071());
        IpmSolver ipm;
        SqpSolver sqp;
        (void)ipm.solve(p, hs071_start());
        (void)sqp.solve(p, hs071_start());
    }
    std::fflush(stdout);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(out, "");
}

TEST(Verbosity, OneLineTurnsTheConsoleOn) {
    std::fflush(stdout);
    testing::internal::CaptureStdout();
    {
        Problem p(hs071());
        IpmOptions o;
        o.common.verbosity = Verbosity::kIterations;
        IpmSolver ipm(o);
        (void)ipm.solve(p, hs071_start());
    }
    std::fflush(stdout);
    const std::string out = testing::internal::GetCapturedStdout();
    EXPECT_NE(out.find("IpmSolver"), std::string::npos);
    EXPECT_EQ(out.find("InteriorPointSolver"), std::string::npos);
}
