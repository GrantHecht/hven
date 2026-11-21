// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// SqpCounters::backend_symbolic_analyses / backend_symbolic_repeats (PF-4): the
// AnalysisTally an SQP solve opens over its body. On the platform session (no
// dense arm) every analysis `symbolic_analyses` counts is one the tally sees,
// so the tally is never below it; a repeat is an analysis, so never above it.

#include <gtest/gtest.h>

#include <hven/detail/model/problem_access.h>
#include <hven/detail/sqp/sqp_solver_impl.h>

#include "model/support/hs_problems.h"

namespace hven {
namespace {

using ::hven::test_support::HsProblem;
using ::hven::test_support::make_hs;

TEST(SqpBackendAnalysisTally, CoversTheCountedAnalysesOnThePlatformSession) {
    for (const int hs : {14, 43, 76}) {
        SCOPED_TRACE(hs);
        SqpOptions opts;
        opts.linear.dense_factorization = hven::linear::DensePolicy::kNever;
        opts.common.max_iterations = 60;
        const HsProblem p = make_hs(hs);
        SqpSolver driver(opts);
        const SqpResult sol =
            driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
        EXPECT_GE(sol.counters.backend_symbolic_analyses, 1);
        EXPECT_GE(sol.counters.backend_symbolic_analyses, sol.counters.symbolic_analyses);
        EXPECT_LE(sol.counters.backend_symbolic_repeats, sol.counters.backend_symbolic_analyses);
    }
}

TEST(SqpBackendAnalysisTally, TheDenseArmPaysNoPlatformAnalysis) {
    SqpOptions opts;
    opts.linear.dense_factorization = hven::linear::DensePolicy::kAlways;
    opts.common.max_iterations = 60;
    const HsProblem p = make_hs(43);
    SqpSolver driver(opts);
    const SqpResult sol =
        driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
    // Only a platform-session retake could analyze on the platform here.
    EXPECT_LE(sol.counters.backend_symbolic_analyses, sol.counters.sparse_scaled_retakes +
                                                          sol.counters.sparse_matched_retakes +
                                                          sol.counters.dense_platform_retakes);
}

// THE SOLVE TALLY (PF-2b): the twelve backend_solve* counters partition the solve's backend solves
// by issuing component. Invariants every solve carries: a call carries at least one column, the
// ordinary bucket is never empty on a solve that factored anything, and the polish bucket is
// reached by any walk solve (the polish loop issues its first solve whenever an EQP was solved).
TEST(SqpBackendSolveTally, PartitionsTheSolvesAndEveryComponentIsReachable) {
    Index detector = 0;
    Index polish = 0;
    Index border = 0;
    for (const int hs : {14, 43, 76, 79}) {
        for (const auto algebra :
             {QpWorkingSetAlgebra::kRefactorize, QpWorkingSetAlgebra::kSchurBorder}) {
            SCOPED_TRACE(hs);
            SqpOptions opts;
            opts.linear.dense_factorization = hven::linear::DensePolicy::kNever;
            opts.common.max_iterations = 60;
            opts.qp.working_set_algebra = algebra;
            const HsProblem p = make_hs(hs);
            SqpSolver driver(opts);
            const SqpResult sol =
                driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
            const SqpCounters &c = sol.counters;
            EXPECT_GT(c.backend_solves_ordinary, 0);
            EXPECT_GE(c.backend_solve_rhs_ordinary, c.backend_solves_ordinary);
            EXPECT_GE(c.backend_solve_rhs_detector, c.backend_solves_detector);
            EXPECT_GE(c.backend_solve_rhs_polish, c.backend_solves_polish);
            EXPECT_GE(c.backend_solve_rhs_border_refine, c.backend_solves_border_refine);
            EXPECT_GE(c.backend_solve_refine_ordinary, 0);
            detector += c.backend_solves_detector;
            polish += c.backend_solves_polish;
            border += c.backend_solves_border_refine;
        }
    }
    EXPECT_GT(detector, 0);
    EXPECT_GT(polish, 0);
    EXPECT_GT(border, 0);
}

} // namespace
} // namespace hven
