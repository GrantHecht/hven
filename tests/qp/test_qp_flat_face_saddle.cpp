// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_flat_face_saddle.cpp — M7 W2 NC2: THE FLAT FACE (qp_engine.h section 4c), on the walk
// seam (docs/testing.md). Synthetic QPs first, then the
// R2 dumps (tests/qp/fixtures/e2-d4/R2_*, the E2-D4 format;
// docs/notes/data/2026-09-m7-w2-r2-composite-landing/).

#include <algorithm>
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
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;
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
    std::vector<Record> records;
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

// THE DESCENT INVARIANT (qp_engine.h section 4c, THE FLAT FACE): no ride ascends a slope the
// arithmetic can read. `ride_sign_chosen` is the sign `ride_sign` returned, `ride_slope` the slope
// it read at the pass's starting iterate and `ride_slope_tol` the resolution it read it at. The
// ratio test returns max(0, .), so alpha >= 0 and this is equivalently "no ride raises f by more
// than alpha * ride_slope_tol" -- on a flat face, where f is linear along the ray, exactly so.
struct RideTally {
    long rides = 0;
    long zero_length = 0;
    long ascending = 0;
    double worst_ascent = 0.0;
};

RideTally ride_tally(const std::vector<Record> &records) {
    RideTally t;
    for (const auto &r : records) {
        if (r.action != Action::kRideStepped) {
            continue;
        }
        ++t.rides;
        if (!(r.dx_inf > r.step_tol)) {
            ++t.zero_length;
        }
        const double ascent = static_cast<double>(r.ride_sign_chosen) * r.ride_slope;
        if (ascent > r.ride_slope_tol) {
            ++t.ascending;
            t.worst_ascent = std::max(t.worst_ascent, ascent);
        }
    }
    return t;
}

void expect_descent_invariant(const RideTally &t) {
    EXPECT_EQ(t.ascending, 0) << "a ride ascended the exact slope by up to " << t.worst_ascent
                              << ", over " << t.rides << " rides";
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
        expect_descent_invariant(ride_tally(w.records));
        // The sign came from the MEASURED slope, which resolves here: this vertex IS a stationary
        // point of its working set and the 3x3 regularized system is conditioned well enough that
        // the EQP's own unregularized refinement step drives the residual rho to rounding, so
        // g'p and the predicted slope s AGREE and the predicted-slope branch -- the FLOOR, which
        // only an iterate whose residual dominates its multiplier can reach -- is not taken.
        EXPECT_EQ(s.counters.ride_predicted_signs, 0);
        // The gate fired on the exact-zero quotient against a FINITE tolerance
        // (hessian_scale floors at 1: 1e-12 on an LP), and the ray was ridden.
        bool rode = false;
        for (std::size_t i = 1; i < w.records.size(); ++i) {
            const Record &r = w.records[i];
            if (!r.post_drop || r.released_index != 2) {
                continue;
            }
            EXPECT_TRUE(r.curvature_fired);
            EXPECT_EQ(r.curvature, 0.0);
            EXPECT_EQ(r.curvature_tol, 1e-12);
            // THE IDENTITY THE RULE RESTS ON, pinned: s = violation * dropped_directional(p) IS
            // g'p at a stationary point of an independent retained set. The violation is the
            // DROP pass's own (the record before this one); the directional and the slope are
            // this pass's.
            const double s_pred = w.records[i - 1].decision_multiplier * r.released_directional;
            EXPECT_LE(std::abs(r.ride_slope - s_pred), r.ride_slope_tol)
                << "predicted " << s_pred << " against measured " << r.ride_slope;
            if (r.ride_outcome == RideRead::kStepped) {
                rode = true;
                // x0 reaches 0 as x2 reaches 1: a tie, the scan order's pick.
                EXPECT_TRUE(r.block_index == 0 || r.block_index == 2);
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
        expect_descent_invariant(ride_tally(w.records));
        Index flat = 0;
        for (std::size_t i = 1; i < w.records.size(); ++i) {
            const Record &r = w.records[i];
            if (!r.post_drop || r.ride_outcome != RideRead::kSlopeFlatVeto) {
                continue;
            }
            ++flat;
            EXPECT_EQ(r.action, Action::kRideUndone);
            EXPECT_LE(std::abs(r.ride_slope), r.ride_slope_tol);
            // AND THE MARK'S OWN TEST (fix round 1): |s| is at or below the resolution of the
            // slope it was meant to predict -- here the dot product's own floor, since the
            // residual |g'p - s| is at rounding at this stationary vertex. So this undone drop IS
            // a kPtNoise drop, which is why zero-motion exhaustion is not the exit above.
            const double s_pred = w.records[i - 1].decision_multiplier * r.released_directional;
            EXPECT_LE(std::abs(s_pred),
                      std::max(r.ride_slope_tol, std::abs(r.ride_slope - s_pred)));
        }
        EXPECT_EQ(flat, 1);

        // Above the resolution the ray is ridden to (0, 1, 0).
        const Walk up = observed(noise_lp(-2e-8), &seed, nc2_opts(alg));
        ASSERT_EQ(static_cast<int>(up.solution.status), static_cast<int>(QpStatus::kOptimal));
        EXPECT_NEAR(up.solution.x(1), 1.0, 1e-12);
        EXPECT_EQ(up.solution.counters.ride_sign_vetoes, 0);
        expect_descent_invariant(ride_tally(up.records));
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

// Release and Debug round these LPs differently -- they decide on multipliers of 1e-7...1e-8, and
// -O0 against -O3 under SAFER_FAST puts a given pass on different sides of a drop tolerance -- so
// the walk's OUTCOME is asserted per configuration, from what was MEASURED in this round, while the
// rule's own statements (the descent invariant; no zero-motion exhaustion) are asserted in both.
#ifdef NDEBUG
constexpr bool kDebugBuild = false;
#else
constexpr bool kDebugBuild = true;
#endif

// Cs_STEENBRB (the composite restoration's sub-QPs at radius 16; H == 0, an LP whose Clarabel
// optimum is in the R2 evidence, replays/cs_steenbrb/*.oracle.txt). Before NC2 every wrong-signed
// drop on this flat face was VETOED -- the measured slope DISAGREED IN SIGN with the drop's own
// multiplier -- and the walk spent its budget undoing drops (1312 of them on qp3) or ended in
// zero-motion exhaustion on the exempt noise-level multiplier a veto left behind.
//
// WHAT THE RULE GUARANTEES HERE, and is asserted in both configurations:
//   - THE DESCENT INVARIANT: no ride ascends the slope the arithmetic reads. That is the fix
//     round's own subject: NC2 as first landed took the multiplier's sign even against a resolving
//     measured slope, and on this face half its rides climbed.
//   - zero_motion_exhausted == 0: a drop undone on a sign veto is a NOISE drop by the mark's own
//     test here (the multiplier is below the residual the two slopes disclose), so it does not
//     count toward exhaustion and the base exit is unreachable.
void expect_cs(const char *file, WorkingSetLinearAlgebra alg, const char *premise,
               double f_clarabel, QpStatus release_status, QpStatus debug_status) {
    const QpDump d = r2_dump(file);
    EXPECT_NE(d.result.find(premise), std::string::npos);
    QpOptions o = d.opts;
    o.ws_algebra = alg;
    Obs::reset();
    Obs::active = true;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    const std::vector<Record> records = Obs::records;
    Obs::active = false;
    Obs::reset();

    const RideTally t = ride_tally(records);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    std::printf("[ nc2 ] %s %s status=%d f=%.12g minors=%ld rides=%ld zero_length=%ld "
                "predicted_signs=%ld vetoes=%ld undos=%ld zmexh=%ld zmcap=%ld\n",
                file, alg_name(alg), static_cast<int>(s.status), f,
                static_cast<long>(s.counters.minor_iters), t.rides, t.zero_length,
                static_cast<long>(s.counters.ride_predicted_signs),
                static_cast<long>(s.counters.ride_sign_vetoes),
                static_cast<long>(s.counters.ride_declines_fallthrough_blocked),
                static_cast<long>(s.counters.zero_motion_exhausted),
                static_cast<long>(s.counters.zero_motion_cap_hits));

    expect_descent_invariant(t);
    EXPECT_EQ(s.counters.zero_motion_exhausted, 0);

    const QpStatus expected = kDebugBuild ? debug_status : release_status;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(expected))
        << "measured per configuration in NC2 fix round 1";
    if (s.status != QpStatus::kOptimal) {
        // A CAPPED CELL STILL SAYS SOMETHING, and it is the honest half of M6's statement: the
        // walk reaches the optimum's OBJECTIVE and cannot certify it inside the budget. Stated at
        // 1e-6 relative, a decade above the 1.1e-7 (Release) / 9.5e-8 (Debug) measured, because
        // where the walk stops inside its budget is exactly the rounding-sensitive part.
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
        EXPECT_LE(std::abs(f - f_clarabel), 1e-6 * std::abs(f_clarabel));
        return;
    }
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    EXPECT_LE(k.eq, 1e-5);
    EXPECT_LE(k.ineq, 1e-5);
    EXPECT_LE(k.box, 1e-9);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-9);
    EXPECT_LE(k.complementarity, 1e-6);
    // Within the resolution of the delta-regularized EQP on a flat face: the round's measurements
    // land inside 1e-7 of the oracle, so M6's gate is stated one power above.
    EXPECT_LE(std::abs(f - f_clarabel), 2e-7 * std::abs(f_clarabel));
}

} // namespace

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp2.txt", WorkingSetLinearAlgebra::kRefactorize, "status=3",
              -192.000444927, QpStatus::kOptimal, QpStatus::kOptimal);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp2NeverAscendsTheSlopeBorder) {
    if (kDebugBuild) {
        // M6's per-fixture Debug budget. This one cell costs 745 s in Debug under `border` --
        // it is the only one of the six that CONVERGES in Debug, so it pays for ~254
        // factorizations at -O0, and border/refactorize is 833x here against 14x on the two
        // cells that cycle without adding. Measured in NC2 fix round 1, both algebras agreeing
        // in every counter and in f on this dump in BOTH configurations, so what the skip costs
        // is the border linear algebra on one dump in one configuration; the assertion below
        // runs in Release, and `...Qp2...Refactorize` covers the same walk in Debug in 0.9 s.
        GTEST_SKIP() << "Debug border on this dump costs 745 s, over M6's 600 s per-fixture "
                        "budget; asserted in Release and, under refactorize, in Debug";
    }
    expect_cs("R2_Cs_STEENBRB_qp2.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=3",
              -192.000444927, QpStatus::kOptimal, QpStatus::kOptimal);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp3.txt", WorkingSetLinearAlgebra::kRefactorize, "status=1",
              -384.000315772, QpStatus::kMaxIter, QpStatus::kMaxIter);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp3NeverAscendsTheSlopeBorder) {
    expect_cs("R2_Cs_STEENBRB_qp3.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=1",
              -384.000315772, QpStatus::kMaxIter, QpStatus::kMaxIter);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp4NeverAscendsTheSlopeRefactorize) {
    expect_cs("R2_Cs_STEENBRB_qp4.txt", WorkingSetLinearAlgebra::kRefactorize, "status=3",
              -192.000158557, QpStatus::kOptimal, QpStatus::kMaxIter);
}

TEST(QpNc2FlatFace, TheSteenbrbRestorationLpQp4NeverAscendsTheSlopeBorder) {
    expect_cs("R2_Cs_STEENBRB_qp4.txt", WorkingSetLinearAlgebra::kSchurBorder, "status=3",
              -192.000158557, QpStatus::kOptimal, QpStatus::kMaxIter);
}
