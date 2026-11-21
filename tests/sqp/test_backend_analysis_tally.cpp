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

} // namespace
} // namespace hven
