// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// THE BACKEND-FAILURE HALF OF THE EXCEPTION CONTRACT, THROUGH THE PUBLIC HANDLES
// (M8 B5 riders, R3 + R11). core/errors.h: a factorization failure the engine
// can report is a status; one it cannot is a BackendError, which propagates out
// of `solve` with the interrupted solve kept in last_result().
//
// No real backend fails on HS071, so the failures are injected through the
// linear layer's existing HVEN_TESTING seam (docs/testing.md), at the scope each
// injector is faithful for:
//   * AnalyzeFaultInjector is faithful in every scenario: it raises the
//     symbolic phase's failure instead of the backend call and before the new
//     session replaces the live one.
//   * FactorizeFaultInjector is faithful only on a session that has never
//     factorized successfully -- here, a fresh solver's very first (initial) KKT
//     factorization.
//
// Compiled into hven_sqp_seam_tests, which recompiles the whole library with
// HVEN_TESTING and does not link hven::hven.

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <Eigen/Core>

#include "../drivers/support/hs071_problem.h"
#include "hven/core/errors.h"
#include "hven/detail/linear/fault_injection.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/ipm_solver_types.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/drivers/sqp_solver_types.h"
#include "hven/model/problem.h"

namespace {

using hven::BackendError;
using hven::IpmOptions;
using hven::IpmResult;
using hven::IpmSolver;
using hven::IpmStopReason;
using hven::Problem;
using hven::SolveStatus;
using hven::SqpOptions;
using hven::SqpSolver;
using hven::SqpStopReason;
using hven::Verbosity;
using hven::linear::detail::testing::AnalyzeFaultInjector;
using hven::linear::detail::testing::FactorizeFaultInjector;
using hven_drivers_tests::hs071_start;
using hven_drivers_tests::Hs071Problem;

// Process-global statics: disarmed by a guard, so a failed assertion never
// leaves the next test running under an injected fault.
struct BackendSeamArmedAnalyze {
    explicit BackendSeamArmedAnalyze(int code) {
        AnalyzeFaultInjector::injected_backend_code = code;
        AnalyzeFaultInjector::active = true;
    }
    ~BackendSeamArmedAnalyze() { AnalyzeFaultInjector::active = false; }
};

struct BackendSeamArmedFactorize {
    explicit BackendSeamArmedFactorize(int code) {
        FactorizeFaultInjector::injected_backend_code = code;
        FactorizeFaultInjector::active = true;
    }
    ~BackendSeamArmedFactorize() { FactorizeFaultInjector::active = false; }
};

// The injectors sit in the platform session's adapter, so these fixtures run on
// the sparse arm on purpose: kNever explicitly (M8 DN-3 flip; HS071's KKT patterns
// are below both engines' kAuto thresholds and would never reach the platform).
IpmOptions backend_seam_ipm() {
    IpmOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.linear.dense_factorization = hven::linear::DensePolicy::kNever;
    return o;
}

SqpOptions backend_seam_sqp() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.linear.dense_factorization = hven::linear::DensePolicy::kNever;
    return o;
}

// The interior-point engine's initial KKT factorization CAN be reported: a
// failed symbolic analysis there is a status, never the linear layer's
// "solve before a successful factorize()" precondition error.
TEST(BackendErrorSeam, AFailedInitialAnalysisIsAStatusOnTheInteriorPointEngine) {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmSolver solver(backend_seam_ipm());
    IpmResult r;
    {
        const BackendSeamArmedAnalyze armed(-2);
        ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
    }
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_EQ(solver.last_stop_reason(), IpmStopReason::kNone);
    EXPECT_NE(r.message.find("initial KKT factorization failed"), std::string::npos) << r.message;
    EXPECT_EQ(r.work.factorizations, 0) << "the analysis failed, so nothing was factorized";
    // Not left claiming the analysis: the next solve analyzes afresh and converges.
    const IpmResult again = solver.solve(problem, hs071_start());
    EXPECT_EQ(again.status, SolveStatus::kOptimal) << again.message;
}

// A numeric refusal of that first factorization (the injector's one faithful
// scenario: a session that never factorized) is the same status.
TEST(BackendErrorSeam, AFailedInitialFactorizationIsAStatusOnTheInteriorPointEngine) {
    Problem problem(std::make_shared<Hs071Problem>());
    IpmSolver solver(backend_seam_ipm());
    IpmResult r;
    {
        const BackendSeamArmedFactorize armed(-4);
        ASSERT_NO_THROW(r = solver.solve(problem, hs071_start()));
    }
    EXPECT_EQ(r.status, SolveStatus::kNumericalError) << r.message;
    EXPECT_NE(r.message.find("initial KKT factorization failed"), std::string::npos) << r.message;
    EXPECT_NE(r.last_kkt_info, Eigen::Success);
    EXPECT_EQ(r.work.factorizations, 1);
}

// The SQP's KKT factor reports a backend failure as a BackendError, which no
// status can carry: it propagates out of `solve` through the handle, and
// last_result() holds the interrupted solve.
TEST(BackendErrorSeam, ABackendErrorSurfacesThroughTheSqpHandleIntoLastResult) {
    Problem problem(std::make_shared<Hs071Problem>());
    SqpSolver solver(backend_seam_sqp());
    bool caught = false;
    {
        const BackendSeamArmedAnalyze armed(-2);
        try {
            (void)solver.solve(problem, hs071_start());
        } catch (const BackendError &e) {
            caught = true;
            EXPECT_NE(std::string(e.what()).find("backend error -2"), std::string::npos)
                << e.what();
        }
    }
    ASSERT_TRUE(caught) << "a BackendError, through the handle";
    const auto &partial = solver.last_result();
    EXPECT_EQ(partial.status, SolveStatus::kInterrupted);
    EXPECT_EQ(solver.last_stop_reason(), SqpStopReason::kException);
    EXPECT_NE(partial.message.find("interrupted by an exception that escaped the solve"),
              std::string::npos)
        << partial.message;
    EXPECT_NE(partial.message.find("backend error -2"), std::string::npos) << partial.message;
    ASSERT_EQ(partial.x.size(), 4);
    // THE BASIC GUARANTEE: injector disarmed, the same solver converges.
    EXPECT_EQ(solver.solve(problem, hs071_start()).status, SolveStatus::kOptimal);
}

} // namespace
