// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_flat_face_saddle.cpp — M7 W2 NC2: THE FLAT FACE (qp_engine.h section 4c), on the walk
// seam (docs/testing.md). Synthetic QPs first, then the
// R2 dumps (tests/qp/fixtures/e2-d4/R2_*, the E2-D4 format;
// docs/notes/data/2026-09-m7-w2-r2-composite-landing/).

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;
using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using RideRead = hven::solvers::detail::testing::QpWalkRideOutcome;

namespace {

constexpr std::array<WorkingSetLinearAlgebra, 2> kAlgebras = {
    WorkingSetLinearAlgebra::kRefactorize, WorkingSetLinearAlgebra::kSchurBorder};

const char *alg_name(WorkingSetLinearAlgebra alg) {
    return alg == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize";
}

QpOptions nc2_opts(WorkingSetLinearAlgebra alg, bool independent = true) {
    QpOptions o;
    o.max_iter = 60;
    o.independent_working_set = independent;
    o.ws_algebra = alg;
    return o;
}

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

struct Walk {
    QpSolution solution;
    std::vector<hven::solvers::detail::testing::QpWalkMinorRecord> records;
};

Walk observed(const QpProblem &qp, const QpSolution *seed, const QpOptions &o) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    const QpEngine engine{o};
    w.solution = seed != nullptr ? engine.solve(qp, *seed) : engine.solve(qp);
    w.records = Obs::records;
    Obs::active = false;
    Obs::reset();
    return w;
}

// ---------------------------------------------------------------------------
// (a) THE FLAT FACE: an LP whose wrong-signed drop opens a ray.
//
// min x0 + 2 x1 subject to x0 + x1 + x2 = 1, 0 <= x <= 1. Seeded at the vertex
// (1, 0, 0) with x1 and x2 at their lower bounds: the equality prices lambda = -1,
// so z1 = 1 (right-signed) and z2 = -1 (wrong-signed at a lower bound). The walk
// drops x2 onto the face {x0 + x2 = 1}, where H = 0: the quotient is exactly 0, the
// direction is a ray, blocked where x0 reaches 0 and x2 reaches 1 (a tie). Optimum
// f = 0 at (0, 0, 1).
// ---------------------------------------------------------------------------

QpProblem flat_ray_lp() {
    QpProblem qp;
    qp.H.resize(3, 3);
    qp.g = Vec(3);
    qp.g << 1.0, 2.0, 0.0;
    Eigen::MatrixXd Ae(1, 3);
    Ae << 1.0, 1.0, 1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(3);
    qp.upper = Vec::Ones(3);
    return qp;
}

QpSolution flat_ray_seed() {
    QpSolution seed;
    seed.x = Vec(3);
    seed.x << 1.0, 0.0, 0.0;
    seed.bound_state = {BoundState::kFree, BoundState::kAtLower, BoundState::kAtLower};
    seed.ineq_active = {};
    return seed;
}

} // namespace

TEST(QpNc2FlatFace, AWrongSignedDropOnAnLpFaceRidesTheRayToItsBlocker) {
    const QpProblem qp = flat_ray_lp();
    const QpSolution seed = flat_ray_seed();
    for (const WorkingSetLinearAlgebra alg : kAlgebras) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = observed(qp, &seed, nc2_opts(alg));
        const QpSolution &s = w.solution;
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(objective(qp, s.x), 0.0, 1e-12);
        EXPECT_NEAR(s.x(2), 1.0, 1e-12);
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 0);
        EXPECT_EQ(s.counters.ride_sign_vetoes, 0);
        // The sign came from the drop's own multiplier, not from the measured slope.
        EXPECT_GE(s.counters.ride_predicted_signs, 1);
        // The gate fired on the exact-zero quotient against a FINITE tolerance
        // (hessian_scale floors at 1: 1e-12 on an LP), and the ray was ridden.
        bool rode = false;
        for (const auto &r : w.records) {
            if (r.post_drop && r.released_index == 2) {
                EXPECT_TRUE(r.curvature_fired);
                EXPECT_EQ(r.curvature, 0.0);
                EXPECT_EQ(r.curvature_tol, 1e-12);
                if (r.ride_outcome == RideRead::kStepped) {
                    rode = true;
                    // x0 reaches 0 as x2 reaches 1: a tie, the scan order's pick.
                    EXPECT_TRUE(r.block_index == 0 || r.block_index == 2);
                }
            }
        }
        EXPECT_TRUE(rode);
    }
}

namespace {

// ---------------------------------------------------------------------------
// (c) A NOISE-LEVEL DROP: the undo, counted.
//
// min -2e-9 x1 + 1e4 x2 subject to x0 + x1 + x2 = 1, 0 <= x <= 1, seeded at the
// vertex (1, 0, 0) with x1 and x2 at their lower bounds. x1's multiplier is
// -2e-9, past opt_tol = 1e-9, so x1 is dropped onto the flat face {x0 + x1 = 1};
// the measured slope along the ray and the predicted one (violation * rate) are
// both below ride_sign's tolerance 1e-12 * ||g|| * ||p|| (||g|| = 1e4): the drop
// is undone and counted, never ridden and never the ordinary step. At -2e-8 the
// same QP's slope clears the tolerance and the ray is ridden instead.
// ---------------------------------------------------------------------------

QpProblem noise_lp(double z1) {
    QpProblem qp = flat_ray_lp();
    qp.g << 0.0, z1, 1e4;
    return qp;
}

} // namespace

TEST(QpNc2FlatFace, ANoiseLevelDropIsUndoneAndCountedNotRidden) {
    const QpSolution seed = flat_ray_seed();
    for (const WorkingSetLinearAlgebra alg : kAlgebras) {
        SCOPED_TRACE(alg_name(alg));
        const QpProblem qp = noise_lp(-2e-9);
        const Walk w = observed(qp, &seed, nc2_opts(alg));
        const QpSolution &s = w.solution;
        ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_EQ(s.counters.ride_sign_vetoes, 1);
        EXPECT_EQ(s.counters.ride_declines_fallthrough_blocked, 1);
        EXPECT_EQ(s.counters.ride_predicted_signs, 0);
        EXPECT_EQ(s.counters.zero_motion_exhausted, 0);
        EXPECT_NEAR(s.x(0), 1.0, 1e-12) << "the vertex is kept: nothing moved";
        Index flat = 0;
        for (const auto &r : w.records) {
            if (r.post_drop && r.ride_outcome == RideRead::kSlopeFlatVeto) {
                ++flat;
                EXPECT_EQ(r.action, Action::kRideUndone);
                EXPECT_LE(std::abs(r.ride_slope), r.ride_slope_tol);
            }
        }
        EXPECT_EQ(flat, 1);

        // Above the resolution the ray is ridden to (0, 1, 0).
        const QpSolution rode = observed(noise_lp(-2e-8), &seed, nc2_opts(alg)).solution;
        ASSERT_EQ(static_cast<int>(rode.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(rode.x(1), 1.0, 1e-12);
        EXPECT_EQ(rode.counters.ride_sign_vetoes, 0);
    }
}

// ===========================================================================
// THE R2 DUMPS (tests/qp/fixtures/e2-d4/R2_Cs_STEENBRB_qp*.txt; the E2-D4 format, FORMAT.txt
// there). ONE TEST PER (DUMP, ALGEBRA): these are R2's own restoration sub-QPs (n = 636, hundreds
// of minors), and a Debug ctest case that solved both algebras would run for twice as long as it
// has to.
// ===========================================================================

namespace {

using hven::test_support::QpDump;

QpDump r2_dump(const char *name) {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + name);
}

// Cs_STEENBRB (the composite restoration's sub-QPs at radius 16; H == 0, an LP whose Clarabel
// optimum is in the R2 evidence, replays/cs_steenbrb/*.oracle.txt): before NC2 every wrong-signed
// drop on the flat face was VETOED -- ride_sign read a measured slope that DISAGREED IN SIGN with
// the drop's own multiplier -- and the walk spent its budget undoing drops (1312 of them on qp3) or
// ended in zero-motion exhaustion. THE FLAT FACE takes the sign from the multiplier instead.
//
// WHAT IS ASSERTED, and what is not. The rule FIRES (ride_predicted_signs), the base failure mode
// is GONE (zero_motion_exhausted == 0), and an answer the walk calls optimal is at the Clarabel
// optimum and passes the E2-D4 KKT battery. The CONVERSION ITSELF is NOT asserted, because it is
// configuration-dependent and measured to be so: in Release all three dumps reach kOptimal (qp2 248
// minors at f = -192.000440284, qp3 294 at -384.000285512, qp4 238 at -192.000148701, identically
// on both algebras), while in Debug qp2 (both algebras) and qp3 (refactorize) run to the iteration
// cap and qp4 converges at 256 minors and f = -192.000152406. These LPs decide on multipliers of
// 1e-7...1e-8 and the two configurations round differently (-O0 against -O3 under SAFER_FAST), so
// which side of a drop tolerance a given pass lands on is not a cross-configuration property. The
// round's own measurement lives in the NC2 report, taken in Release; this fixture pins the
// mechanism, which does hold in both.
void expect_cs(const char *file, WorkingSetLinearAlgebra alg, const char *premise,
               double f_clarabel) {
    const QpDump d = r2_dump(file);
    EXPECT_NE(d.result.find(premise), std::string::npos);
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf("[ nc2 ] %s %s status=%d f=%.12g minors=%ld predicted_signs=%ld vetoes=%ld "
                "undos=%ld zmexh=%ld\n",
                file, alg_name(alg), static_cast<int>(s.status),
                hven::test_support::qp_dump_objective(d, s.x),
                static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.ride_predicted_signs),
                static_cast<long>(s.counters.ride_sign_vetoes),
                static_cast<long>(s.counters.ride_declines_fallthrough_blocked),
                static_cast<long>(s.counters.zero_motion_exhausted));
    // The rule fires on this face, and it fires a lot: 55 to 80 times in Release.
    EXPECT_GE(s.counters.ride_predicted_signs, 1);
    // The base walk's exit on qp2 and qp4 was zero-motion exhaustion on the exempt noise-level
    // multiplier its veto left behind. Under the rule the veto does not happen, and kPtNoise keeps
    // any that does out of the exhaustion test, so that exit is unreachable here whatever else the
    // walk does.
    EXPECT_EQ(s.counters.zero_motion_exhausted, 0);
    if (s.status != QpStatus::kOptimal) {
        return; // Debug: the cap. The statement above is the one that holds in both.
    }
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-5);
    EXPECT_LE(k.ineq, 1e-5);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-6);
    // Within the resolution of the delta-regularized EQP on a flat face: Release lands 2.4e-8,
    // 7.9e-8 and 5.1e-8 of the oracle away, so the gate is stated one power above the worst.
    EXPECT_LE(std::abs(hven::test_support::qp_dump_objective(d, s.x) - f_clarabel),
              2e-7 * std::abs(f_clarabel));
}

} // namespace

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp2TakesTheMultipliersSignRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp2.txt", WorkingSetLinearAlgebra::kRefactorize, "status=3",
              -192.000444927);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp2TakesTheMultipliersSignBorder) {
    expect_cs("R2_Cs_STEENBRB_qp2.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=3",
              -192.000444927);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp3TakesTheMultipliersSignRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp3.txt", WorkingSetLinearAlgebra::kRefactorize, "status=1",
              -384.000315772);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp3TakesTheMultipliersSignBorder) {
    expect_cs("R2_Cs_STEENBRB_qp3.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=1",
              -384.000315772);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp4TakesTheMultipliersSignRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp4.txt", WorkingSetLinearAlgebra::kRefactorize, "status=3",
              -192.000158557);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp4TakesTheMultipliersSignBorder) {
    expect_cs("R2_Cs_STEENBRB_qp4.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=3",
              -192.000158557);
}
