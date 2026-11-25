// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_issue18_accuracy_stop.cpp -- issue #18 and THE ACCURACY STOP (eqp_solve.h). The fixture
// is the issue's dump verbatim (e2-d4/I18_qp_m137.txt): a feasible, convex 22-variable SQP
// subproblem in a 3.8e-6 trust region that ended kNumericalError at feasibility 1e-9 and 1e-10 on
// both algebras, because an EQP answer off by 1.2e-5 (three trust radii) was accepted.
// Portable: kOptimal and the KKT battery against the effective box. Route (reference build only):
// minors, factorizations and the refinement counters.

#include <cstdio>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/eqp_solve.h>
#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"
#include "support/route_reference.h"

using namespace hven;
using namespace hven::detail;
using hven::test_support::QpDump;

namespace {

QpDump load_fixture() {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                            "/I18_qp_m137.txt");
}

QpSolution solve_at(const QpDump &d, QpWorkingSetAlgebra alg, double feas) {
    QpOptions o = d.opts;
    o.working_set_algebra = alg;
    o.feasibility_tol = feas;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf("[ i18 ] %s feas=%g status=%d minors=%ld facts=%ld eqp_refine=%ld "
                "border_refine=%ld f=%.12g\n",
                alg == QpWorkingSetAlgebra::kSchurBorder ? "border" : "refactorize", feas,
                static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                static_cast<long>(s.counters.eqp_refine_steps),
                static_cast<long>(s.counters.border_refine_steps),
                hven::test_support::qp_dump_objective(d, s.x));
    return s;
}

void expect_solved(const QpDump &d, const QpSolution &s) {
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-9);
    EXPECT_LE(k.ineq, 1e-9);
    EXPECT_LE(k.box, 1e-12);
    EXPECT_LE(k.stationarity, 1e-9);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-9);
    // The objective is not asserted: g's 1e2 entries turn a 1e-12 move of x into 1e-10 of f
    // (~3e-9).
}

void check_algebra(QpWorkingSetAlgebra alg) {
    const QpDump d = load_fixture();
    for (const double feas : {1e-9, 1e-10}) {
        SCOPED_TRACE(feas);
        const QpSolution s = solve_at(d, alg, feas);
        expect_solved(d, s);
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_EQ(s.counters.minor_iters, 8);
        EXPECT_EQ(s.counters.factorizations, alg == QpWorkingSetAlgebra::kSchurBorder ? 1 : 8);
        if (alg == QpWorkingSetAlgebra::kRefactorize) {
            EXPECT_GE(s.counters.eqp_refine_steps, 1) << "the eliminated path's accuracy stop";
        }
#else
        hven::test_support::record_off_reference_route(
            "issue18_feas" + std::to_string(feas),
            "minors=" + std::to_string(s.counters.minor_iters) +
                " facts=" + std::to_string(s.counters.factorizations) +
                " eqp_refine=" + std::to_string(s.counters.eqp_refine_steps));
#endif
    }
}

} // namespace

TEST(QpIssue18AccuracyStop, TheLoaderKeepsTheDumpedConvexityPromise) {
    const QpDump d = load_fixture();
    EXPECT_NE(d.result.find("hkc=1"), std::string::npos) << d.result;
    EXPECT_TRUE(d.opts.hessian_known_convex);
    EXPECT_EQ(d.opts.feasibility_tol, 1e-9) << "the dumped tolerance";
    EXPECT_TRUE(d.opts.independent_working_set);
    EXPECT_EQ(d.opts.working_set_algebra, QpWorkingSetAlgebra::kSchurBorder);
}

TEST(QpIssue18AccuracyStop, TheFeasibleQpIsSolvedOnTheBorderAlgebra) {
    check_algebra(QpWorkingSetAlgebra::kSchurBorder);
}

TEST(QpIssue18AccuracyStop, TheFeasibleQpIsSolvedOnTheRefactorizeAlgebra) {
    check_algebra(QpWorkingSetAlgebra::kRefactorize);
}

TEST(QpIssue18AccuracyStop, TheVerdictAndKeepRules) {
    // Resolved iff the last correction is at most half the step (or of step_tol, if larger).
    EXPECT_TRUE(accuracy_resolved(0.5, 1.0, 1e-9));
    EXPECT_FALSE(accuracy_resolved(0.51, 1.0, 1e-9));
    EXPECT_TRUE(accuracy_resolved(5e-10, 1e-12, 1e-9));
    EXPECT_FALSE(accuracy_resolved(std::numeric_limits<double>::quiet_NaN(), 1.0, 1e-9));
    EXPECT_FALSE(accuracy_resolved(0.1, std::numeric_limits<double>::quiet_NaN(), 1e-9));
    // Kept iff the residual falls strictly and both it and the correction shrink tenfold.
    EXPECT_TRUE(accuracy_step_contracts(1e-3, 1e-2, 1e-3, 1e-2));
    EXPECT_FALSE(accuracy_step_contracts(2e-3, 1e-2, 1e-3, 1e-2));
    EXPECT_FALSE(accuracy_step_contracts(1e-3, 1e-2, 2e-3, 1e-2));
    EXPECT_FALSE(accuracy_step_contracts(0.0, 0.0, 0.0, 1.0));
    EXPECT_FALSE(accuracy_step_contracts(std::numeric_limits<double>::quiet_NaN(), 1.0, 0.0, 1.0));
}
