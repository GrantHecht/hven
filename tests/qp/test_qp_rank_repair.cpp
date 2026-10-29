// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_rank_repair.cpp -- M7 W2 RDV: THE DEAD-END RANK REPAIR (qp_engine.h, Degeneracy;
// ws_independence.h, rank_repair). Three subproblems of the exact-Hessian TRAIN cells YORKNET and
// SAWPATH, every one FEASIBLE by an external oracle (HiGHS phase 1 minimum l1 violation 0), on
// which BOTH algebras of the independent working set's walk ended kNumericalError at the add
// site's dead end (no admissible member: dep_blocker_terminations, or a cond1 certificate that
// failed verification: dep_certificate_rejections) while standing at a RANK-DEFICIENT working set
// (normalized sigma_min 1e-13 .. 1e-17) off its equality rows:
//   * C11_YORKNET_qp1 (the elastic rung of major 0; convex, H a nonnegative diagonal). Stall-
//     admitted pins (a regularized test whose refinement stalled at rho 1e-2: H is zero on 493 of
//     508 directions, so the face's delta pivots are the backend's to perturb) made the face
//     INCONSISTENT: e110 = -3.3e-4 x6 - x21 + x22 + x390 with x21, x22, x390 pinned; deficiency 4
//     at the dead end, the screen (me + |W| = n) calling every later blocker dependent. Oracle:
//     Clarabel Solved, f* = 1696529.7293693814 (rows to 9e-13).
//   * C11_SAWPATH_qp1 (the elastic rung of major 0; convex). The equality block ITSELF is
//     numerically rank-deficient (normalized spectrum 24 .. 1e-11, row norms to 6e10): the screen
//     counts its 578 rows and calls a blocker of rho 4.9e-5 dependent, its certificate rejected.
//     No sharp oracle: Clarabel AlmostSolved, f = -376.225617384 at a point 8e-4 off its rows
//     (row-equilibrated, -435.6 at a relative 1e-10): the QP is ill-posed at the feasibility
//     tolerance, so the walk's exactly feasible point is checked against the oracle's value with
//     the first-order bound its own residual allows (below), not to 1e-9.
//   * RDV_YORKNET_qp5 (a restoration sub-solve's QP; nonconvex, 96 negative eigenvalues). An
//     exchange on coefficients polluted by residual / sigma_min released a pin outside the
//     dependency it was decided on (x8 completes e12 = -x0 + x8 + x324 - x580 exactly; the
//     coefficients also carried 3.9e-4 on x429, the pin released): a CONSISTENT deficiency of 1.
//     No oracle optimum; success is a KKT point the walk certifies, read here independently.
// YORKNET qp1 reaches the oracle's optimum on both configurations; for the other two see
// expect_repaired. MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "../common_support/route_reference.h"
#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
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
    std::printf(
        "[ rdv ] %s %s status=%d minors=%ld facts=%ld f=%.12g rank_repairs=%ld "
        "releases=%ld misses=%ld blkterm=%ld certrej=%ld\n",
        file, alg == kBorder ? "border" : "refactorize", static_cast<int>(s.status),
        static_cast<long>(s.counters.minor_iters), static_cast<long>(s.counters.factorizations),
        hven::test_support::qp_dump_objective(d, s.x), static_cast<long>(s.counters.rank_repairs),
        static_cast<long>(s.counters.rank_repair_releases),
        static_cast<long>(s.counters.rank_repair_misses),
        static_cast<long>(s.counters.dep_blocker_terminations),
        static_cast<long>(s.counters.dep_certificate_rejections));
    return s;
}

// The pins' multipliers carry their bound's sign (z >= 0 at a lower bound, <= 0 at an upper one).
double pin_sign_violation(const QpSolution &s) {
    double worst = 0.0;
    for (std::size_t i = 0; i < s.bound_state.size(); ++i) {
        const double zi = s.z(static_cast<Index>(i));
        if (s.bound_state[i] == BoundState::kAtLower) {
            worst = std::max(worst, -zi);
        } else if (s.bound_state[i] == BoundState::kAtUpper) {
            worst = std::max(worst, zi);
        }
    }
    return worst;
}

// A KKT point the walk certified, read independently of the walk: rows and box to an absolute
// tolerance, the multipliers' signs, stationarity and complementarity (qp_dump_kkt's scaled
// readings), and at least one repair taken where the dead end stood.
void expect_certified_kkt(const QpDump &d, const QpSolution &s, double stationarity_tol) {
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(s.counters.rank_repairs, 1);
    EXPECT_GE(s.counters.rank_repair_releases, s.counters.rank_repairs);
    EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    EXPECT_EQ(s.counters.dep_certificate_rejections, 0);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-8);
    EXPECT_LE(k.ineq, 1e-8);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, stationarity_tol);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-8);
    EXPECT_LE(pin_sign_violation(s), 1e-9 * k.scale);
}

constexpr double kYorknetQp1Oracle = 1696529.7293693814;
// SAWPATH qp1: the oracle's value at a point 8e-4 off its rows, and the first-order bound on
// how far below an exactly feasible KKT point the true minimum can sit: f(x) - f* <= ||r||_inf
// ||x - x*||_1 with r the walk's stationarity residual (6.1e-5 measured) and x, x* in the box
// (sum of widths 1.17e3) -- 0.072, rounded up to 0.1.
constexpr double kSawpathQp1Oracle = -376.225617384;
constexpr double kSawpathQp1Band = 0.1;

void expect_yorknet_qp1(WorkingSetLinearAlgebra alg) {
    QpDump d;
    const QpSolution s = solve_fixture("C11_YORKNET_qp1.txt", alg, d);
    // The premise: in-cell, this convex feasible subproblem ended kNumericalError (468 minors).
    EXPECT_NE(d.result.find("status=3 minors=468"), std::string::npos) << d.result;
    expect_certified_kkt(d, s, 1e-8);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    EXPECT_LE(std::abs(f - kYorknetQp1Oracle), 1e-9 * std::abs(kYorknetQp1Oracle))
        << "f = " << f << " against the oracle's " << kYorknetQp1Oracle;
}

void print_walk(const char *tag, const QpDump &d, const QpSolution &s) {
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf("[ rdv ] %s status=%d minors=%ld stagnation_drops=%ld stagnation_stops=%ld "
                "f=%.12g stationarity=%.3e eq=%.3e\n",
                tag, static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.stagnation_drops),
                static_cast<long>(s.counters.stagnation_stops),
                hven::test_support::qp_dump_objective(d, s.x), k.stationarity, k.eq);
}

// SAWPATH qp1 and YORKNET qp5: the dead end is repaired on every configuration; what the walk then
// reaches is NOT configuration-independent. Release certifies a KKT point (asserted below). Debug's
// rounding leads both into a two-pass cycle of full steps on the face they reach (SAWPATH:
// stationarity stalls on the equality block's 1e-11 singular values; YORKNET qp5: on a full-rank
// square face, sigma_min 0.027, the EQP answer from the point on the rows misses them by 1.2e-2
// and the next answer returns); until M7 W2 SNX the minor cap ended both cycles. THE STAGNATION
// READING (qp_engine.h) now ends each with a drop, and Debug too certifies the KKT point Release
// reaches (declared, Debug only): SAWPATH f = -376.163994774, inside the oracle band below;
// YORKNET qp5 f = -1078.49295775, Release's pin. The kOptimal status stays asserted in Release
// only. Asserted on both: the rule's own effect (no dead-end exit, a repair taken) and NO FALSE
// CERTIFICATE (kOptimal only at a KKT point; never kInfeasible).
void expect_repaired(const QpDump &d, const QpSolution &s, double stationarity_tol,
                     const char *off_reference_key = nullptr) {
#if !HVEN_TEST_ROUTE_REFERENCE
    // Off the reference build a fixture that names a key (SAWPATH qp1: an exactly dependent
    // equality block Pardiso's pivot perturbation decides on MKL and nothing equivalent decides
    // elsewhere) records its route instead of pinning it (WX design section 2.1): the repair
    // count, the certificate-rejection and blocker-termination counters, the stagnation counters
    // and the status. The portable contract: never kInfeasible on a feasible fixture, and, if
    // kOptimal, the battery. Fixtures without a key keep the pins below on every build.
    if (off_reference_key != nullptr) {
        hven::test_support::record_off_reference_route(
            off_reference_key,
            "status=" + std::to_string(static_cast<int>(s.status)) +
                " minors=" + std::to_string(s.counters.minor_iters) +
                " rank_repairs=" + std::to_string(s.counters.rank_repairs) +
                " dep_blocker_terminations=" + std::to_string(s.counters.dep_blocker_terminations) +
                " dep_certificate_rejections=" +
                std::to_string(s.counters.dep_certificate_rejections) +
                " stagnation_drops=" + std::to_string(s.counters.stagnation_drops) +
                " stagnation_stops=" + std::to_string(s.counters.stagnation_stops));
        EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
        if (s.status == QpStatus::kOptimal) {
            // The battery without the route counters (a repair taken, no blocker termination,
            // no certificate rejection), which expect_certified_kkt pins for the reference
            // build.
            const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
            EXPECT_LE(k.eq, 1e-8);
            EXPECT_LE(k.ineq, 1e-8);
            EXPECT_LE(k.box, 1e-9);
            EXPECT_LE(k.stationarity, stationarity_tol);
            EXPECT_LE(k.sign, 1e-9);
            EXPECT_LE(k.complementarity, 1e-8);
            EXPECT_LE(pin_sign_violation(s), 1e-9 * k.scale);
        }
        return;
    }
#else
    (void)off_reference_key;
#endif
    EXPECT_GE(s.counters.rank_repairs, 1);
    EXPECT_EQ(s.counters.dep_blocker_terminations, 0);
    EXPECT_EQ(s.counters.dep_certificate_rejections, 0);
    EXPECT_TRUE(s.status == QpStatus::kOptimal || s.status == QpStatus::kMaxIter)
        << static_cast<int>(s.status);
#ifdef NDEBUG
    // Release: the repaired walk REACHES its KKT point.
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
#else
    // Debug: the declared move, a stagnation drop out of the two-pass cycle, then the KKT point.
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_GE(s.counters.stagnation_drops, 1);
#endif
    if (s.status == QpStatus::kOptimal) {
        expect_certified_kkt(d, s, stationarity_tol);
    }
}

void expect_sawpath_qp1(WorkingSetLinearAlgebra alg) {
    QpDump d;
    const QpSolution s = solve_fixture("C11_SAWPATH_qp1.txt", alg, d);
    print_walk("SAWPATH qp1", d, s);
    EXPECT_NE(d.result.find("status=3 minors=6"), std::string::npos) << d.result;
    // Scaled stationarity 5.0e-7 measured in Release and 2.2e-6 in Debug: the equality block's
    // 1e-11 singular values bound the multipliers' accuracy, so one bound, 1e-5, holds both.
    expect_repaired(d, s, 1e-5, alg == kBorder ? "sawpath_qp1_border" : "sawpath_qp1_refactorize");
    if (s.status == QpStatus::kOptimal) {
        const double f = hven::test_support::qp_dump_objective(d, s.x);
        EXPECT_LE(std::abs(f - kSawpathQp1Oracle), kSawpathQp1Band)
            << "f = " << f << " against the oracle's " << kSawpathQp1Oracle;
    }
}

void expect_yorknet_qp5(WorkingSetLinearAlgebra alg) {
    QpDump d;
    const QpSolution s = solve_fixture("RDV_YORKNET_qp5.txt", alg, d);
    print_walk("YORKNET qp5", d, s);
    EXPECT_NE(d.result.find("status=3 minors=546"), std::string::npos) << d.result;
    expect_repaired(d, s, 1e-8);
#ifdef NDEBUG
    // Release reaches the KKT point f = -1078.49 (RDV measurement; no oracle optimum: a nonconvex
    // restoration sub-solve, so the value is pinned as the walk's own certified point).
    EXPECT_NEAR(hven::test_support::qp_dump_objective(d, s.x), -1078.49, 5e-3);
#endif
}

} // namespace

TEST(QpRdvRankRepair, YorknetQp1Border) { expect_yorknet_qp1(kBorder); }
TEST(QpRdvRankRepair, YorknetQp1Refactorize) { expect_yorknet_qp1(kRefactorize); }
TEST(QpRdvRankRepair, SawpathQp1Border) { expect_sawpath_qp1(kBorder); }
TEST(QpRdvRankRepair, SawpathQp1Refactorize) { expect_sawpath_qp1(kRefactorize); }
TEST(QpRdvRankRepair, YorknetQp5Border) { expect_yorknet_qp5(kBorder); }
TEST(QpRdvRankRepair, YorknetQp5Refactorize) { expect_yorknet_qp5(kRefactorize); }

// THE SEAM DROP IDENTITY of a repair: with the walk observer attached, every kRankRepair record's
// working set shrinks by exactly the members it released, so the records' total equals the
// rank_repair_releases counter, and their number equals rank_repairs. (YORKNET qp5's repair is
// the cheapest of the three to reach.)
TEST(QpRdvRankRepair, TheSeamRecordsARepairWithItsDropIdentity) {
    using Obs = hven::solvers::detail::testing::QpWalkObserver;
    using Action = hven::solvers::detail::testing::QpWalkAction;
    Obs::reset();
    Obs::active = true;
    QpDump d;
    const QpSolution s = solve_fixture("RDV_YORKNET_qp5.txt", kBorder, d);
    Obs::active = false;
    ASSERT_FALSE(Obs::truncated);
    ASSERT_GE(s.counters.rank_repairs, 1);
    Index repairs = 0;
    Index shrunk = 0;
    for (const auto &r : Obs::records) {
        if (r.action == Action::kRankRepair) {
            ++repairs;
            shrunk += r.ws_size_in + r.shift_adds - r.ws_size_out;
        }
    }
    EXPECT_EQ(repairs, s.counters.rank_repairs);
    EXPECT_EQ(shrunk, s.counters.rank_repair_releases);
    Obs::reset();
}
