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
// by issuing component. A call carries at least one column; the ordinary bucket is never empty;
// every other bucket is reached (the detector, the polish loops and, on the Schur-border algebra
// only, the bordered refinement), and the bordered refinement is silent on the refactorizing
// algebra, which never forms a border. The backend reports refinement steps inside the solves
// (Pardiso's default), so each reached bucket carries them too.
TEST(SqpBackendSolveTally, PartitionsTheSolvesAndEveryComponentIsReachable) {
    for (const auto algebra :
         {QpWorkingSetAlgebra::kRefactorize, QpWorkingSetAlgebra::kSchurBorder}) {
        SCOPED_TRACE(static_cast<int>(algebra));
        Index solves[4] = {0, 0, 0, 0};
        Index rhs[4] = {0, 0, 0, 0};
        Index refine[4] = {0, 0, 0, 0};
        for (const int hs : {14, 43, 76, 79}) {
            SqpOptions opts;
            opts.linear.dense_factorization = hven::linear::DensePolicy::kNever;
            opts.common.max_iterations = 60;
            opts.qp.working_set_algebra = algebra;
            const HsProblem p = make_hs(hs);
            SqpSolver driver(opts);
            const SqpResult sol =
                driver.solve(hven::detail::problem_ref(*p.model), (*p.model).start_point());
            const SqpCounters &c = sol.counters;
            const Index s4[4] = {c.backend_solves_ordinary, c.backend_solves_detector,
                                 c.backend_solves_polish, c.backend_solves_border_refine};
            const Index r4[4] = {c.backend_solve_rhs_ordinary, c.backend_solve_rhs_detector,
                                 c.backend_solve_rhs_polish, c.backend_solve_rhs_border_refine};
            const Index f4[4] = {c.backend_solve_refine_ordinary, c.backend_solve_refine_detector,
                                 c.backend_solve_refine_polish,
                                 c.backend_solve_refine_border_refine};
            EXPECT_GT(s4[0], 0) << hs;
            for (int k = 0; k < 4; ++k) {
                EXPECT_GE(r4[k], s4[k]) << hs << " bucket " << k;
                solves[k] += s4[k];
                rhs[k] += r4[k];
                refine[k] += f4[k];
            }
        }
        const bool bordered = algebra == QpWorkingSetAlgebra::kSchurBorder;
        for (int k = 0; k < 4; ++k) {
            if (k == 3 && !bordered) {
                EXPECT_EQ(solves[k], 0);
                EXPECT_EQ(rhs[k], 0);
                EXPECT_EQ(refine[k], 0);
            } else {
                EXPECT_GT(solves[k], 0) << "bucket " << k;
                EXPECT_GT(rhs[k], 0) << "bucket " << k;
                EXPECT_GT(refine[k], 0) << "bucket " << k;
            }
        }
    }
}

} // namespace
} // namespace hven
