// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Both solvers are movable handles over a private implementation (AP-2 group
// B1). What a move must preserve is the engine itself, so the pin is on
// behaviour, not on a type trait: a solver that has solved, been moved, and
// solves again returns BITWISE the result a solver that was never moved returns
// for its second solve. The moved-from solver is then destroyed and assigned
// to; using it for anything else is refused by name.
//
// Reference and moved arms are separate solver instances running the same two
// solves, so the comparison is between two second solves with the same history
// (a re-solve reuses the symbolic analysis on both arms alike).

#include <gtest/gtest.h>

#include <bit>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <utility>

#include <Eigen/Core>

#include <hven/drivers/ipm_solver.h>
#include <hven/drivers/ipm_solver_types.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/make_nlp_program.h>
#include <hven/model/nlp_model.h>
#include <hven/model/non_linear_program.h>

#include "sqp/support/hs_problems.h"
#include "support/hs071_problem.h"

namespace {

bool bits_equal(double a, double b) {
    return std::bit_cast<std::uint64_t>(a) == std::bit_cast<std::uint64_t>(b);
}

bool vectors_bitwise_equal(const Eigen::VectorXd &a, const Eigen::VectorXd &b) {
    if (a.size() != b.size()) {
        return false;
    }
    return a.size() == 0 || std::memcmp(a.data(), b.data(),
                                        static_cast<std::size_t>(a.size()) * sizeof(double)) == 0;
}

void expect_same_solve(const hven::solvers::SolveResult &a, const hven::solvers::SolveResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.iterations, b.iterations);
    EXPECT_TRUE(bits_equal(a.f, b.f));
    EXPECT_TRUE(vectors_bitwise_equal(a.x, b.x));
    EXPECT_TRUE(vectors_bitwise_equal(a.lambda_e, b.lambda_e));
    EXPECT_TRUE(vectors_bitwise_equal(a.lambda_i, b.lambda_i));
}

hven::solvers::IpmOptions ipm_options() {
    hven::solvers::IpmOptions o;
    o.common.threads = 1;
    o.common.print_level = 10;
    return o;
}

hven::solvers::SqpOptions sqp_options() {
    hven::solvers::SqpOptions o;
    o.common.threads = 1;
    o.common.print_level = 10;
    return o;
}

} // namespace

TEST(SolverMove, IpmSolverSolvesBitwiseEqualAfterAMove) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    const Eigen::VectorXd x0 = hven_drivers_tests::hs071_start();

    hven::solvers::IpmSolver reference(ipm_options());
    (void)reference.solve(*program, x0);
    const hven::solvers::IpmResult expected = reference.solve(*program, x0);

    hven::solvers::IpmSolver original(ipm_options());
    (void)original.solve(*program, x0);
    hven::solvers::IpmSolver moved(std::move(original));
    const hven::solvers::IpmResult actual = moved.solve(*program, x0);

    expect_same_solve(expected, actual);
    EXPECT_EQ(expected.kkt_factor_counters.factorize_count,
              actual.kkt_factor_counters.factorize_count);
    EXPECT_EQ(expected.kkt_analyses_this_call, actual.kkt_analyses_this_call);

    // The moved-from solver may be destroyed, and assigned to: after the
    // assignment it is a working solver again, and a cold solve of a fresh
    // solver's first-solve history reproduces the first-solve answer.
    original = hven::solvers::IpmSolver(ipm_options());
    const hven::solvers::IpmResult again = original.solve(*program, x0);
    hven::solvers::IpmSolver fresh(ipm_options());
    expect_same_solve(fresh.solve(*program, x0), again);
}

TEST(SolverMove, IpmMoveAssignmentTransfersTheEngine) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    const Eigen::VectorXd x0 = hven_drivers_tests::hs071_start();

    hven::solvers::IpmSolver reference(ipm_options());
    (void)reference.solve(*program, x0);
    const hven::solvers::IpmResult expected = reference.solve(*program, x0);

    hven::solvers::IpmSolver source(ipm_options());
    (void)source.solve(*program, x0);
    hven::solvers::IpmSolver target(ipm_options());
    target = std::move(source);
    EXPECT_TRUE(target.kkt_pattern_is_analyzed(*program));
    expect_same_solve(expected, target.solve(*program, x0));
}

TEST(SolverMove, AMovedFromIpmSolverRefusesUseByName) {
    hven::solvers::IpmSolver source(ipm_options());
    hven::solvers::IpmSolver sink(std::move(source));
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<hven_drivers_tests::Hs071Problem>());
    EXPECT_THROW((void)source.solve(*program, hven_drivers_tests::hs071_start()), std::logic_error);
    EXPECT_THROW(source.set_options(ipm_options()), std::logic_error);
    EXPECT_THROW(source.clear_iteration_callback(), std::logic_error);
}

TEST(SolverMove, SqpSolverSolvesBitwiseEqualAfterAMove) {
    const auto p = hven::solvers::test_support::make_hs(7);
    const Eigen::VectorXd x0 = p.model->start_point();

    hven::solvers::SqpSolver reference(sqp_options());
    (void)reference.solve(*p.model, x0);
    const hven::solvers::SqpResult expected = reference.solve(*p.model, x0);

    hven::solvers::SqpSolver original(sqp_options());
    (void)original.solve(*p.model, x0);
    hven::solvers::SqpSolver moved(std::move(original));
    const hven::solvers::SqpResult actual = moved.solve(*p.model, x0);

    expect_same_solve(expected, actual);
    EXPECT_EQ(expected.counters.major_iters, actual.counters.major_iters);
    EXPECT_EQ(expected.counters.qp_minor_iters, actual.counters.qp_minor_iters);
    EXPECT_EQ(expected.counters.factorizations, actual.counters.factorizations);

    original = hven::solvers::SqpSolver(sqp_options());
    const hven::solvers::SqpResult again = original.solve(*p.model, x0);
    hven::solvers::SqpSolver fresh(sqp_options());
    expect_same_solve(fresh.solve(*p.model, x0), again);
}

TEST(SolverMove, SqpMoveAssignmentTransfersTheEngine) {
    const auto p = hven::solvers::test_support::make_hs(7);
    const Eigen::VectorXd x0 = p.model->start_point();

    hven::solvers::SqpSolver reference(sqp_options());
    (void)reference.solve(*p.model, x0);
    const hven::solvers::SqpResult expected = reference.solve(*p.model, x0);

    hven::solvers::SqpSolver source(sqp_options());
    (void)source.solve(*p.model, x0);
    hven::solvers::SqpSolver target(sqp_options());
    target = std::move(source);
    expect_same_solve(expected, target.solve(*p.model, x0));
    EXPECT_NO_THROW((void)target.export_warm_start());
}

TEST(SolverMove, AMovedFromSqpSolverRefusesUseByName) {
    hven::solvers::SqpSolver source(sqp_options());
    hven::solvers::SqpSolver sink(std::move(source));
    const auto p = hven::solvers::test_support::make_hs(7);
    EXPECT_THROW((void)source.solve(*p.model, p.model->start_point()), std::logic_error);
    EXPECT_THROW(source.set_options(sqp_options()), std::logic_error);
    EXPECT_THROW((void)source.export_warm_start(), std::logic_error);
}
