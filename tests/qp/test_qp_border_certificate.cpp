// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_border_certificate.cpp -- M7 W2 BPS: THE BORDERED ANSWER'S CERTIFICATE
// (bordered_eqp.h, refine_bordered_solve_iterated). On the Schur-border path the walk stood
// still to its minor cap -- full steps back and forth on one working set, inertia kOk, on a
// face the bound-eliminated path certifies in a handful of minors -- because the bordered
// refinement's footprint stop left an answer far less accurate than the eliminated one:
//   * BPS_ACOPR118_qp26 (F81's ACOPR118 arm lane, solve 26, in-cell kMaxIter): the refinement
//     DIVERGES along the face's soft direction and stops above its own target with the answer
//     0.83 off; the walk takes ascending full steps of 0.12. TARGET MET fails, the pass takes
//     the eliminated twin, and the face is certified in 3 minors.
//   * BPS_ACOPR118_entry9 (LK's h5 lane, solve 9 at entry): the loop meets its target after
//     one step with the answer 3.3e-9 off -- exactly the full step it proposes, back and forth
//     for 5 300 passes. STEP RESOLVED refines it past the footprint, the step falls below
//     step_tol, and the walk certifies.
//   * BPS_ACOPP57_entry1 (m7's ACOPP57 lane, solve 1 at entry; in-cell kOptimal after a
//     569-pass full-step jitter at a 9.9e-6 equality-row residual, kOk): the regression check
//     against E2 F8-1's candidate S, which retreated this solve and lost ACOPP57. The
//     certificate never fires here (the jitter is off the equality rows, where the resolution
//     reading is not asked, and every refinement meets its target).
//
// REFERENCES: the kRefactorize algebra of the same production walk, which this rule never
// touches. MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED (Release and Debug round
// these subproblems differently; test_qp_standstill_retreat.cpp states the same).

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
    std::printf("[ bps ] %s %s status=%d minors=%ld facts=%ld f=%.12g fallbacks=%ld refine=%ld\n",
                file, alg == kBorder ? "border" : "refactorize", static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                hven::test_support::qp_dump_objective(d, s.x),
                static_cast<long>(s.counters.border_uncertified_fallbacks),
                static_cast<long>(s.counters.border_refine_steps));
    return s;
}

void expect_certified_at(const QpDump &d, const QpSolution &s, double f_reference) {
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    // A BASIN check, relative with a floor (fix 9's rider R10-3): the same local minimiser
    // the reference reaches, not a digit-for-digit pin.
    EXPECT_LE(std::abs(f - f_reference), 1e-6 * std::max(1.0, std::abs(f_reference)))
        << "f = " << f << " against the reference " << f_reference;
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-8);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
}

} // namespace

// TARGET MET: the diverging refinement's pass takes the eliminated twin.
TEST(QpBpsBorderCertificate, Acopr118Qp26Border) {
    QpDump d;
    const QpSolution s = solve_fixture("BPS_ACOPR118_qp26.txt", kBorder, d);
    // The premise: in-cell, this subproblem ran to its iteration cap.
    EXPECT_NE(d.result.find("status=1"), std::string::npos) << d.result;
    EXPECT_GE(s.counters.border_uncertified_fallbacks, 1);
    expect_certified_at(d, s, -0.000976904148597);
}
TEST(QpBpsBorderCertificate, Acopr118Qp26Refactorize) {
    QpDump d;
    const QpSolution s = solve_fixture("BPS_ACOPR118_qp26.txt", kRefactorize, d);
    EXPECT_EQ(s.counters.border_uncertified_fallbacks, 0);
    expect_certified_at(d, s, -0.000976904148597);
}

// STEP RESOLVED: the period-2 standstill is refined to zero motion and certified.
TEST(QpBpsBorderCertificate, Acopr118Entry9Border) {
    QpDump d;
    const QpSolution s = solve_fixture("BPS_ACOPR118_entry9.txt", kBorder, d);
    // The mechanism pinned: STEP RESOLVED (refinement past the footprint), not the fallback.
    // Measured: 0 fallbacks both configs; refine steps 638 Release / 641 Debug over 384 / 386
    // minors (ratio 1.66), so the config-stable pin is the inequality, at 3/2 of the minors:
    // with STEP RESOLVED always true the ratio is 1.03.
    EXPECT_EQ(s.counters.border_uncertified_fallbacks, 0);
    EXPECT_GT(2 * s.counters.border_refine_steps, 3 * s.counters.minor_iters);
    expect_certified_at(d, s, 75.2165872705);
}
TEST(QpBpsBorderCertificate, Acopr118Entry9Refactorize) {
    QpDump d;
    const QpSolution s = solve_fixture("BPS_ACOPR118_entry9.txt", kRefactorize, d);
    expect_certified_at(d, s, 75.2165872705);
}

// THE REGRESSION CHECK against candidate S: the kOk jitter off the equality rows is left alone.
TEST(QpBpsBorderCertificate, Acopp57Entry1BorderUntouched) {
    QpDump d;
    const QpSolution s = solve_fixture("BPS_ACOPP57_entry1.txt", kBorder, d);
    EXPECT_EQ(s.counters.border_uncertified_fallbacks, 0);
    expect_certified_at(d, s, 677.229629532);
}
