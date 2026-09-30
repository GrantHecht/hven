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

#include <cmath>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

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
// while `ws` is back at the seed. Under E2 fix7 alone the SAME pass then reached its add site
// with `pass_view.usable()` false; since E2 fix8 (F7-1) that pass is SPENT, and the add of
// row 0 at a full unit of motion happens on the next pass, on a factor that holds its set.
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

    std::fprintf(stderr,
                 "[ e2f8 ] synthetic %d status=%d minors=%ld unrun=%ld restarts=%ld "
                 "budget_stops=%ld screen_pins=%ld a0=%d a1=%d\n",
                 static_cast<int>(GetParam()), static_cast<int>(s.status),
                 static_cast<long>(s.counters.minor_iters),
                 static_cast<long>(s.counters.dep_unrun_tests),
                 static_cast<long>(s.counters.dep_repair_restarts),
                 static_cast<long>(s.counters.dep_repair_budget_stops),
                 static_cast<long>(s.counters.dep_repair_screen_pins),
                 records.empty() ? -1 : static_cast<int>(records[0].action),
                 records.size() < 2 ? -1 : static_cast<int>(records[1].action));

    // E2 FIX8, F7-1 IS WHAT THIS NOW WITNESSES. The repair still pins x0, x1, x2 and undoes
    // itself -- but the pass it wasted is now SPENT instead of run on the `face`/border
    // state its probes left behind, so the add site is reached on the NEXT pass, with a
    // factor that holds the working set. The un-run test that fix7's rule answers therefore
    // does not occur here any more: `dep_unrun_tests` is 0, and rule A's own contract is
    // asserted where it cannot be taken away, at the factor-view level in
    // tests/sqp/test_qp_ws_independence.cpp.
    EXPECT_EQ(s.counters.dep_repair_restarts, 1);
    EXPECT_EQ(s.counters.dep_repair_budget_stops, 0) << "3 pins, budget max(8, ceil(4/2)) = 8";
    EXPECT_EQ(s.counters.dep_unrun_tests, 0) << "F7-1 removed the pass that had no factor";
    EXPECT_EQ(s.counters.dep_deferred_adds, 0) << "an uncomputed rho may not buy a deferred add";
    EXPECT_EQ(s.counters.dep_undecidable, 0);

    // Pass 0 is the spent one, and it PRICED nothing and STEPPED nowhere. Pass 1 is the
    // ordinary add of row 0 at a full unit of motion.
    ASSERT_GE(records.size(), 3U);
    EXPECT_EQ(records[0].action, Action::kStartRepairRefused);
    EXPECT_EQ(records[1].action, Action::kAdd);
    EXPECT_EQ(records[1].block, Block::kIneq);
    EXPECT_EQ(records[1].block_index, 0);
    EXPECT_GT(records[1].dx_inf, records[1].step_tol) << "a MOVING step, not a degenerate one";
    EXPECT_FALSE(records[1].degenerate);

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

// ===========================================================================
// E2 FIX8, THE BOUNDED REPAIR: ACOPR118 majors 17 and 18 of the hven-sqp lane's `default`
// cell, captured in the E4 fix-5 diagnosis. They are the QPs that cost that cell its
// `optimal` row: before fix8 all four (majors 17-20) returned kNumericalError after 2-3
// minors in BOTH algebras, on a kWrong inertia verdict that is arithmetically TRUE (Z'HZ on
// the walk's terminal face has one eigenvalue at -6.8e-2). OBSERVED cause: the
// temporary-vertex repair pinned 70 variables, the integer screen then fired
// (me + |W| + 1 = 345 > n = 344) and skipped the remaining 245 candidates, so the repair ran
// out with the inertia still kWrong and undid itself -- five pins short of candidate 172,
// which reads kOk. `independent_working_set = false`, which has neither screen nor test,
// pins past it and solves. Fix8's rule stops the screen deciding there, under a budget of
// max(8, ceil(n/2)) = 172 pins for these (75 used), and both algebras now certify.
// ===========================================================================

namespace {

using hven::test_support::QpDump;

QpDump e2d4_dump(const char *name) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + name);
}

const char *alg_name(WorkingSetLinearAlgebra alg) {
    return alg == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize";
}

// Release and Debug round these subproblems differently (the same reason
// test_qp_flat_face_saddle.cpp states for the R2 dumps: -O0 against -O3 under SAFER_FAST puts a
// pass on different sides of a drop tolerance), so MINOR COUNTS are printed and never asserted;
// the status, the objective and the KKT battery are the assertions.
void expect_acopr118(const char *file, WorkingSetLinearAlgebra alg, double f_reference) {
    const QpDump d = e2d4_dump(file);
    // The dump's own in-cell outcome, verbatim: kNumericalError (status=3) in 2-3 minors. This is
    // the premise of the whole fixture, so it is asserted rather than trusted.
    EXPECT_NE(d.result.find("status=3"), std::string::npos);

    QpOptions o = d.opts;
    o.ws_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";

    Obs::reset();
    Obs::active = true;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    const std::vector<Record> records = Obs::records;
    Obs::active = false;
    Obs::reset();

    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf("[ e2f7 ] %s %s status=%d minors=%ld facts=%ld f=%.12g stat=%.3e "
                "screen_pins=%ld pin_skips=%ld deferred=%ld undecidable=%ld unrun=%ld "
                "budget_stops=%ld restarts=%ld\n",
                file, alg_name(alg), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations), f, k.stationarity,
                static_cast<long>(s.counters.dep_repair_screen_pins),
                static_cast<long>(s.counters.dep_pin_skips),
                static_cast<long>(s.counters.dep_deferred_adds),
                static_cast<long>(s.counters.dep_undecidable),
                static_cast<long>(s.counters.dep_unrun_tests),
                static_cast<long>(s.counters.dep_repair_budget_stops),
                static_cast<long>(s.counters.dep_repair_restarts));

    // RULE (B) FIRED: the repair tried candidate pins the integer screen had condemned. Without
    // this the walk never gets past its third minor on these dumps.
    EXPECT_GE(s.counters.dep_repair_screen_pins, 1)
        << "the screen-positive repair pins are the whole mechanism under test";
    // The BOUND was not the binding constraint here: the repair reached its verdict inside
    // its budget (max(8, ceil(344/2)) = 172 pins; 75 were used), and no pass was spent on a
    // repair that undid itself.
    EXPECT_EQ(s.counters.dep_repair_budget_stops, 0);
    EXPECT_EQ(s.counters.dep_repair_restarts, 0);

    // RULE (A): no blocker on these dumps is charged the deferred-add path any more. Before the
    // round the first pass ran with a stale factor and pin 291's test returned kUndecidable with
    // rho = NaN and solves = 0; with the repair succeeding, that pass does not occur at all.
    EXPECT_EQ(s.counters.dep_deferred_adds, 0);
    EXPECT_EQ(s.counters.dep_undecidable, 0);

    // THE WALK GETS PAST MINOR 3 -- the pre-round walk ended at minor 2 with a kTerminal record
    // and kNumericalError. Asserted on the trace, not on the counter, so "the loop ran" and "the
    // loop reported" are separate facts.
    ASSERT_GE(records.size(), 5U) << "the pre-round walk recorded 2-3 minors and stopped";
    for (std::size_t i = 0; i < records.size(); ++i) {
        EXPECT_NE(records[i].action, Action::kUnset) << "minor " << i;
    }
    // The first pass is the START REPAIR, which is what rule (B) restores: the pre-round trace
    // had an ordinary kAdd there (of bound 291) on an unrepaired, indefinite face.
    EXPECT_EQ(records[0].action, Action::kStartRepair);
    EXPECT_GT(records[0].ws_size_out, records[0].ws_size_in) << "the repair pinned something";
    // ... and the pass after it reads a HEALTHY inertia, which the pre-round walk never did.
    EXPECT_EQ(records[1].inertia_verdict, static_cast<int>(detail::InertiaVerdict::kOk));

    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    if (s.status != QpStatus::kOptimal) {
        return;
    }
    // The objective the SAME dump reaches with independent_working_set = false, which is the
    // reference this round has: an answer to the same QP produced by the walk with the whole
    // working-set-independence apparatus off. The two algebras land within 5e-8 relative of each
    // other, so the gate is stated a decade above that.
    EXPECT_LE(std::abs(f - f_reference), 1e-6 * std::abs(f_reference));
    EXPECT_LE(k.eq, 1e-5);
    EXPECT_LE(k.ineq, 1e-5);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-6);
}

} // namespace

// ACOPR118 major 17 (trust region 0.25), the first of the four that cost the cell. The
// independent-working-set-OFF walk certifies this dump at f = -0.154376254 (border) /
// -0.154376247 (refactorize); the ON walk returned +0.1054162515 at 3 minors before this round.
TEST(QpE2Fix7Acopr118, Major17SubproblemCertifiesRefactorize) {
    expect_acopr118("ACOPR118_e4_qp5.txt", WorkingSetLinearAlgebra::kRefactorize, -0.15437625);
}

TEST(QpE2Fix7Acopr118, Major17SubproblemCertifiesBorder) {
    expect_acopr118("ACOPR118_e4_qp5.txt", WorkingSetLinearAlgebra::kSchurBorder, -0.15437625);
}

// ACOPR118 major 18 (trust region 0.125). Kept beside major 17 because it is the one dump of the
// four where the ON walk now does BETTER than the OFF walk rather than matching it: OFF under the
// border algebra spends its whole 1000-minor budget and exits kMaxIter, ON certifies.
TEST(QpE2Fix7Acopr118, Major18SubproblemCertifiesRefactorize) {
    expect_acopr118("ACOPR118_e4_qp6.txt", WorkingSetLinearAlgebra::kRefactorize, 0.039528206);
}

TEST(QpE2Fix7Acopr118, Major18SubproblemCertifiesBorder) {
    expect_acopr118("ACOPR118_e4_qp6.txt", WorkingSetLinearAlgebra::kSchurBorder, 0.039528206);
}

// ===========================================================================
// E2 FIX8, THE BUDGET ITSELF, AND WHAT IT COSTS WHEN IT BINDS. n = 12, H = -I on the box
// [-1, 1]^12, no rows: every free variable carries negative curvature, so the
// temporary-vertex repair needs ALL TWELVE pins to read kOk -- more than its budget
// max(kRepairPinFloor, ceil(12/2)) = 8. With independent_working_set on, the repair stops on
// the budget and undoes itself, the pass it ran in is spent (F7-1: the probes took the
// pass's factor), and the walk then has no way off the indefinite face: kNumericalError --
// the outcome of EVERY failed start repair on a truly indefinite face, which is what the
// screen used to produce when it aborted one. Off, the repair is unbounded, as it always
// was, pins all twelve, and the walk certifies a vertex (every vertex of this box is a strict
// local minimizer). THIS IS THE BOUND'S DECLARED COST, pinned so it cannot move silently: a
// fraction-of-n budget cannot see the inertia deficit, and a QP whose deficit exceeds it is
// cut. None of the 838 measured repairs is (qp_engine.h); a deficit-aware budget is
// registered (E2 fix8 report, F8-2).
// ===========================================================================

namespace {

QpProblem concave_box(Index n) {
    QpProblem qp;
    Eigen::MatrixXd H = -Eigen::MatrixXd::Identity(n, n);
    qp.H = dense_to_sparse(H);
    qp.g = Vec(n);
    for (Index i = 0; i < n; ++i) {
        qp.g(i) = (i % 2 == 0 ? 0.25 : -0.25) * (1.0 + 0.01 * static_cast<double>(i));
    }
    qp.Ae = dense_to_sparse(Eigen::MatrixXd(0, n));
    qp.be = Vec(0);
    qp.Ai = dense_to_sparse(Eigen::MatrixXd(0, n));
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, -1.0);
    qp.upper = Vec::Constant(n, 1.0);
    return qp;
}

} // namespace

class QpE2Fix8Budget : public ::testing::TestWithParam<WorkingSetLinearAlgebra> {};

TEST_P(QpE2Fix8Budget, ARepairThatNeedsMorePinsThanItsBudgetStopsAndTheWalkFails) {
    constexpr Index kN = 12;
    ASSERT_EQ(detail::repair_pin_budget(kN), 8) << "the fixture needs n above the budget";
    const QpProblem qp = concave_box(kN);
    for (const bool independent : {true, false}) {
        SCOPED_TRACE(independent ? "independent_working_set" : "option off");
        QpOptions o;
        o.ws_algebra = GetParam();
        o.independent_working_set = independent;
        o.max_iter = 500;
        Obs::reset();
        Obs::active = true;
        const QpEngine engine(o);
        const QpSolution s = engine.solve(qp);
        const std::vector<Record> records = Obs::records;
        Obs::active = false;
        Obs::reset();
        std::printf("[ e2f8 ] budget %s %s status=%d minors=%ld facts=%ld budget_stops=%ld "
                    "restarts=%ld a0=%d\n",
                    GetParam() == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize",
                    independent ? "on" : "off", static_cast<int>(s.status),
                    static_cast<long>(s.counters.minor_iters),
                    static_cast<long>(s.counters.factorizations),
                    static_cast<long>(s.counters.dep_repair_budget_stops),
                    static_cast<long>(s.counters.dep_repair_restarts),
                    records.empty() ? -1 : static_cast<int>(records[0].action));
        EXPECT_EQ(s.counters.dep_repair_budget_stops, independent ? 1 : 0);
        ASSERT_FALSE(records.empty());
        if (independent) {
            // The budget stopped the repair, the probes had taken the pass's factor, so the
            // pass was spent -- and the walk ends where a failed repair leaves it.
            EXPECT_EQ(records[0].action, Action::kStartRepairRefused);
            EXPECT_EQ(s.counters.dep_repair_restarts, 1);
            EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kNumericalError));
            continue;
        }
        EXPECT_EQ(records[0].action, Action::kStartRepair) << "unbounded, it succeeds";
        EXPECT_EQ(s.counters.dep_repair_restarts, 0);
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        for (Index i = 0; i < kN; ++i) {
            EXPECT_NEAR(std::abs(s.x(i)), 1.0, 1e-9) << "coordinate " << i << " is at a bound";
        }
    }
}

INSTANTIATE_TEST_SUITE_P(BothAlgebras, QpE2Fix8Budget,
                         ::testing::Values(WorkingSetLinearAlgebra::kRefactorize,
                                           WorkingSetLinearAlgebra::kSchurBorder));
