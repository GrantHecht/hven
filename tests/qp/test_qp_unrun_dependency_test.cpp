// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_unrun_dependency_test.cpp -- M7 W2 E2 FIX ROUND 7, on the walk seam
// (docs/testing.md): AN UN-RUN DEPENDENCY TEST IS NOT A VERDICT OF DEPENDENCE
// (qp_engine.h, Degeneracy; settler S39). With no factor for the pass's working set the
// test computes NOTHING -- rho stays at its NaN default, `solves` at 0 -- so the integer
// screen decides alone: positive it is an exact proof, negative the blocker is an ORDINARY
// ADD, the pre-E2 admission (S17), counted `dep_unrun_tests`. Before this round that case
// returned kUndecidable, which `DepTest::dependent()` reads as dependent, and the blocker
// was charged the DEFERRED-ADD path -- a re-check pass, and a known-dependent mark if that
// re-check removed it -- on the strength of a rho that had never been computed.
//
// The test's own contract (every field of the verdict, and the screen-positive half) is
// asserted at the unit level in tests/sqp/test_qp_ws_independence.cpp. What is here is the
// WALK: a pass that actually reaches its add site with no factor, and a blocker there that
// MOVES.

#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using Block = hven::solvers::detail::testing::QpWalkBlock;
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;

// ===========================================================================
// A 4-variable synthetic whose FIRST pass reaches the add site with no factor, and whose
// blocker there MOVES.
//
//   min  1/2 (x0^2 + x1^2 + x2^2) - 1/2 x3^2 + 1/2 (x0 + x1 + x2) + 2 x3
//   s.t. x3 <= 1,  -x3 <= 1,  -1/2 <= x0,x1,x2 <= 10,  x3 UNBOUNDED in the box
//
// x3's curvature is negative and its box is infinite, so `pin_at_best_bound` has nothing to
// pin it against: the temporary-vertex repair at the first pass pins x0, x1 and x2 in turn,
// each probe still reads kWrong (x3 stays free), the candidate list runs out, and the repair
// undoes itself -- leaving `face` and the border stack describing the last set it probed
// while `ws` is back at the seed. The walk does not `continue` on that false, so the SAME
// pass reaches its add site with `pass_view.usable()` false. Its blocker is row 0 at a full
// unit of motion.
//
// The walk then certifies the exact optimum: x = (-1/2, -1/2, -1/2, -1) at f = -23/8. x3
// sits at the ROW, not at a box bound, so this is a genuine active-set answer and not a
// clamp.
// ===========================================================================

namespace {

hven::SpMatRM dense_to_sparse(const Eigen::MatrixXd &m) { return m.sparseView(); }

QpProblem unrun_synthetic() {
    Eigen::MatrixXd H = Eigen::MatrixXd::Identity(4, 4);
    H(3, 3) = -1.0;
    Eigen::MatrixXd Ai(2, 4);
    Ai << 0, 0, 0, 1, 0, 0, 0, -1;
    QpProblem qp;
    qp.H = dense_to_sparse(H.triangularView<Eigen::Upper>().toDenseMatrix());
    qp.g = Vec::Constant(4, 0.5);
    qp.g(3) = 2.0;
    qp.Ae = dense_to_sparse(Eigen::MatrixXd(0, 4));
    qp.be = Vec(0);
    qp.Ai = dense_to_sparse(Ai);
    qp.bi = Vec::Constant(2, 1.0);
    qp.lower = Vec::Constant(4, -0.5);
    qp.upper = Vec::Constant(4, 10.0);
    qp.lower(3) = -1e20;
    qp.upper(3) = 1e20;
    return qp;
}

} // namespace

class QpE2Fix7Unrun : public ::testing::TestWithParam<WorkingSetLinearAlgebra> {};

TEST_P(QpE2Fix7Unrun, AMovingBlockerWithNoFactorIsAnOrdinaryAdd) {
    const QpProblem qp = unrun_synthetic();
    QpOptions o;
    o.ws_algebra = GetParam();
    o.independent_working_set = true;
    o.max_iter = 100;

    Obs::reset();
    Obs::active = true;
    const QpEngine engine(o);
    const QpSolution s = engine.solve(qp);
    const std::vector<Record> records = Obs::records;
    Obs::active = false;
    Obs::reset();

    // THE TEST DID NOT RUN, and it was not read as a verdict of dependence.
    EXPECT_EQ(s.counters.dep_unrun_tests, 1);
    EXPECT_EQ(s.counters.dep_deferred_adds, 0) << "an uncomputed rho may not buy a deferred add";
    EXPECT_EQ(s.counters.dep_undecidable, 0);

    // The blocker at the first pass is row 0, it MOVED, and the pass took the ordinary add.
    ASSERT_GE(records.size(), 2U);
    EXPECT_EQ(records[0].action, Action::kAdd);
    EXPECT_EQ(records[0].block, Block::kIneq);
    EXPECT_EQ(records[0].block_index, 0);
    EXPECT_GT(records[0].dx_inf, records[0].step_tol) << "a MOVING step, not a degenerate one";
    EXPECT_FALSE(records[0].degenerate);
    // The repair ran and failed: it is what leaves the pass without a factor. A repair that
    // had SUCCEEDED would have recorded kStartRepair here and continued.
    EXPECT_NE(records[0].action, Action::kStartRepair);

    // And the walk certifies the exact optimum.
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_NEAR(s.x(0), -0.5, 1e-9);
    EXPECT_NEAR(s.x(1), -0.5, 1e-9);
    EXPECT_NEAR(s.x(2), -0.5, 1e-9);
    EXPECT_NEAR(s.x(3), -1.0, 1e-9);
    const double f = qp.g.dot(s.x) + 0.5 * s.x.dot(qp.H.selfadjointView<Eigen::Upper>() * s.x);
    EXPECT_NEAR(f, -2.875, 1e-9);
}

INSTANTIATE_TEST_SUITE_P(BothAlgebras, QpE2Fix7Unrun,
                         ::testing::Values(WorkingSetLinearAlgebra::kRefactorize,
                                           WorkingSetLinearAlgebra::kSchurBorder));
