// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_implied_blocker.cpp -- M7 W2 QC-8: THE IMPLIED BLOCKER (qp_engine.h,
// QpEngine::blocker_implied_by_face). Three subproblems of DALLASM under the quasi-Newton
// Hessian mode (shipped defaults), every one FEASIBLE by an external oracle (HiGHS phase 1
// minimum l1 violation 0; Clarabel optimum, residuals ~1e-14), on which BOTH algebras of the
// independent working set's walk ended kNumericalError by the no-admissible-member rule
// (dep_blocker_terminations): a set of tied bounds dependent on the working set through the
// equality rows -- x84 - x108 - x121 + x157 on QP 6, the pair x178 / x179 on the elastic QPs --
// each blocks at zero motion with a rate that is the step's own leak off the equality rows
// (1e-7..1e-5, mu * lambda); the exchange keeps the span and so the step, the member it releases
// blocks next, and the entered-at-point lock ends the solve. The implied blocker passes such a
// blocker over before any exchange: the face satisfies it, so the ordinary step cannot.
//   * QC8_DALLASM_entry6 (solve 6; major 2, Delta 0.1): strictly convex (B positive
//     definite, eigenvalues 160..1089). The in-cell dump (its QP and warm seed byte-identical to
//     the entry dump of the same solve; `result` carries the in-cell outcome, kNumericalError in
//     54 minors on the border path); replayed: border 54 minors / refactorize 44 minors, f 417
//     above the optimum.
//   * QC8_DALLASM_qp1 (solve 1, the elastic rung of major 0; in-cell kNumericalError, 9 minors).
//   * QC8_DALLASM_qp7 (solve 7, the elastic rung of major 2; in-cell kNumericalError, 73 minors).
// The references are the oracle's (Clarabel) optima. MINOR AND FACTORIZATION COUNTS ARE PRINTED,
// NEVER ASSERTED (Release and Debug round these subproblems differently).

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::test_support::QpDump;

namespace {

constexpr auto kBorder = WorkingSetLinearAlgebra::kSchurBorder;
constexpr auto kRefactorize = WorkingSetLinearAlgebra::kRefactorize;

QpSolution solve_fixture(const char *file, WorkingSetLinearAlgebra alg, QpDump &d) {
    d = hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    EXPECT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf("[ qc8 ] %s %s status=%d minors=%ld facts=%ld f=%.12g implied=%ld blkterm=%ld\n",
                file, alg == kBorder ? "border" : "refactorize", static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                hven::test_support::qp_dump_objective(d, s.x),
                static_cast<long>(s.counters.dep_implied_passovers),
                static_cast<long>(s.counters.dep_blocker_terminations));
    return s;
}

void expect_optimal_at(const QpDump &d, const QpSolution &s, double f_oracle) {
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    EXPECT_GE(s.counters.dep_implied_passovers, 1);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    // The oracle's optimum, relative with a floor (measured within 1e-11).
    EXPECT_LE(std::abs(f - f_oracle), 1e-9 * std::max(1.0, std::abs(f_oracle)))
        << "f = " << f << " against the oracle's " << f_oracle;
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
}

constexpr double kEntry6Oracle = -92396.9539668143;
constexpr double kQp1Oracle = -125712.835806432;
constexpr double kQp7Oracle = -92397.2698484735;

} // namespace

TEST(QpQc8ImpliedBlocker, DallasmEntry6Border) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_entry6.txt", kBorder, d);
    // The premise: in-cell, this strictly convex feasible subproblem ended kNumericalError.
    EXPECT_NE(d.result.find("status=3 minors=54"), std::string::npos) << d.result;
    expect_optimal_at(d, s, kEntry6Oracle);
}
TEST(QpQc8ImpliedBlocker, DallasmEntry6Refactorize) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_entry6.txt", kRefactorize, d);
    expect_optimal_at(d, s, kEntry6Oracle);
}

TEST(QpQc8ImpliedBlocker, DallasmQp1Border) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_qp1.txt", kBorder, d);
    // The premise: in-cell, this elastic subproblem ended kNumericalError.
    EXPECT_NE(d.result.find("status=3"), std::string::npos) << d.result;
    expect_optimal_at(d, s, kQp1Oracle);
}
TEST(QpQc8ImpliedBlocker, DallasmQp1Refactorize) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_qp1.txt", kRefactorize, d);
    expect_optimal_at(d, s, kQp1Oracle);
}

TEST(QpQc8ImpliedBlocker, DallasmQp7Border) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_qp7.txt", kBorder, d);
    EXPECT_NE(d.result.find("status=3"), std::string::npos) << d.result;
    expect_optimal_at(d, s, kQp7Oracle);
}
TEST(QpQc8ImpliedBlocker, DallasmQp7Refactorize) {
    QpDump d;
    const QpSolution s = solve_fixture("QC8_DALLASM_qp7.txt", kRefactorize, d);
    expect_optimal_at(d, s, kQp7Oracle);
}
