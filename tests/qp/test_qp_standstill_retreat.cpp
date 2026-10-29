// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_standstill_retreat.cpp -- M7 W2 E2 FIX ROUND 9, on the walk seam (docs/testing.md):
// THE STANDSTILL RETREAT (qp_engine.h). On the border path the independent walk could stand
// still forever off its own equality rows: on an exactly rank-deficient face the border
// factor's step and dependency coefficients are inaccurate, the inertia reads kWrong on a face
// whose reduced Hessian is positive definite, and the iterate takes tiny full steps (or
// zero-motion drop/re-add pairs) that neither KKT-point disjunct can recognise. When such a
// pass stands still, the walk now takes the border retreat and re-checks.
//
// The fixtures are the seven ACOPR118 / ACOPP118 `default` sub-QPs that capped in-cell at the
// fix8 head (each dump's `result` line: status=1, kMaxIter). Every one is feasible (phase-1 LP
// minimum 0) inside a finite trust-region box, so a minimiser exists. The eighth capped QP,
// ACOPP118 qp2, also fails on the bound-eliminated path and is not a fixture here.
//
// REFERENCES. ACOPR118 qp3-qp6 and ACOPP118 qp0-qp1: the kRefactorize algebra of the same
// production walk, which never takes this retreat and certifies these six; ACOPP118 qp3, which
// kRefactorize does not certify: an independent SLSQP solve (scipy, used as an oracle only),
// f = 1592.5507. ACOPP118 qp0/qp1's SLSQP values (1155.7880, 1172.3580) agree with their
// kRefactorize references.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "../common_support/route_reference.h"
#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::test_support::QpDump;

namespace {

const char *alg_name(WorkingSetLinearAlgebra alg) {
    return alg == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize";
}

// MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED: Release and Debug round these
// subproblems differently (test_qp_unrun_dependency_test.cpp states the same for the
// ACOPR118 dumps). The status, the objective, the KKT battery and the retreat count are the
// assertions.
void expect_certified(const char *file, WorkingSetLinearAlgebra alg, double f_reference) {
    const QpDump d =
        hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    // The premise: in-cell, this subproblem ran to its iteration cap.
    ASSERT_NE(d.result.find("status=1"), std::string::npos) << d.result;

    QpOptions o = d.opts;
    o.ws_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";

    const QpSolution s = hven::test_support::solve_qp_dump(d, o);

    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf("[ e2f9 ] %s %s status=%d minors=%ld facts=%ld f=%.12g eq=%.2e stat=%.2e "
                "standstill_retreats=%ld dep_border_retreats=%ld\n",
                file, alg_name(alg), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations), f, k.eq, k.stationarity,
                static_cast<long>(s.counters.standstill_retreats),
                static_cast<long>(s.counters.dep_border_retreats));

#if HVEN_TEST_ROUTE_REFERENCE
    // The retreat is a border-path rule, once per solve.
    EXPECT_EQ(s.counters.standstill_retreats, alg == WorkingSetLinearAlgebra::kSchurBorder ? 1 : 0);

    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
#else
    // Off the reference build the walk's route -- whether it stood still and retreated, and
    // whether this capped fixture certifies inside its iteration budget -- is recorded, not
    // pinned (WX design section 2.1). The portable contract: an honest status for a feasible
    // fixture (the dump's premise is an in-cell kMaxIter), and, if kOptimal, the battery and
    // the reference basin below. The refactorize algebra never takes the retreat on any build.
    hven::test_support::record_off_reference_route(
        std::string(file) + "_" + alg_name(alg),
        "status=" + std::to_string(static_cast<int>(s.status)) +
            " standstill_retreats=" + std::to_string(s.counters.standstill_retreats) +
            " dep_border_retreats=" + std::to_string(s.counters.dep_border_retreats) +
            " minors=" + std::to_string(s.counters.minor_iters) +
            " facts=" + std::to_string(s.counters.factorizations));
    if (alg != WorkingSetLinearAlgebra::kSchurBorder) {
        EXPECT_EQ(s.counters.standstill_retreats, 0);
    }
    EXPECT_TRUE(s.status == QpStatus::kOptimal || s.status == QpStatus::kMaxIter)
        << "status " << static_cast<int>(s.status);
#endif
    if (s.status != QpStatus::kOptimal) {
        return;
    }
    EXPECT_LE(std::abs(f - f_reference), 1e-6 * std::max(1.0, std::abs(f_reference)))
        << "f = " << f << " against the reference " << f_reference;
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
}

// RELEASE-ONLY fixtures, measured in Debug and skipped there: ACOPP118 qp0 (certifies in Debug
// too, in 231 s against 4 s) and ACOPP118 qp1 (Debug's rounding caps it on BOTH algebras, the
// kRefactorize one included, which this rule never touches -- a property of the dump, not of
// the retreat).
#ifdef NDEBUG
constexpr bool kDebugBuild = false;
#else
constexpr bool kDebugBuild = true;
#endif

constexpr auto kBorder = WorkingSetLinearAlgebra::kSchurBorder;
constexpr auto kRefactorize = WorkingSetLinearAlgebra::kRefactorize;

} // namespace

// ACOPR118 majors at trust regions 0.25 / 0.125 / 0.0625 / 0.03125. qp3 and qp5 cycled in-cell
// (a zero-motion drop, a stall-admitted re-add, a full step back); qp4 and qp6 stood still on
// one working set taking full steps of ~1e-7.
TEST(QpE2Fix9StandstillRetreat, Acopr118Qp3Border) {
    expect_certified("ACOPR118_f81_qp3.txt", kBorder, -0.00104614257);
}
TEST(QpE2Fix9StandstillRetreat, Acopr118Qp3Refactorize) {
    expect_certified("ACOPR118_f81_qp3.txt", kRefactorize, -0.00104614257);
}
TEST(QpE2Fix9StandstillRetreat, Acopr118Qp4Border) {
    expect_certified("ACOPR118_f81_qp4.txt", kBorder, -0.00046234282757);
}
TEST(QpE2Fix9StandstillRetreat, Acopr118Qp5Border) {
    expect_certified("ACOPR118_f81_qp5.txt", kBorder, -0.000203076910836);
}
TEST(QpE2Fix9StandstillRetreat, Acopr118Qp6Border) {
    expect_certified("ACOPR118_f81_qp6.txt", kBorder, -8.1598307225e-05);
}

// ACOPP118 majors at trust regions 2 / 1 / 0.25: full-step standstills, the iterate up to 1.0
// off its equality rows.
TEST(QpE2Fix9StandstillRetreat, Acopp118Qp0Border) {
    if (kDebugBuild) {
        GTEST_SKIP() << "Release-only (see kDebugBuild)";
    }
    expect_certified("ACOPP118_f81_qp0.txt", kBorder, 1155.78801918);
}
TEST(QpE2Fix9StandstillRetreat, Acopp118Qp1Border) {
    if (kDebugBuild) {
        GTEST_SKIP() << "Release-only (see kDebugBuild)";
    }
    expect_certified("ACOPP118_f81_qp1.txt", kBorder, 1172.35800871);
}
TEST(QpE2Fix9StandstillRetreat, Acopp118Qp1Refactorize) {
    if (kDebugBuild) {
        GTEST_SKIP() << "Release-only (see kDebugBuild)";
    }
    expect_certified("ACOPP118_f81_qp1.txt", kRefactorize, 1172.35800871);
}
TEST(QpE2Fix9StandstillRetreat, Acopp118Qp3Border) {
    expect_certified("ACOPP118_f81_qp3.txt", kBorder, 1592.5507);
}
