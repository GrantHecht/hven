// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_ascent_footprint.cpp -- M8 WA-1: THE ASCENT GUARD's REGULARIZATION FOOTPRINT
// (walk_engine.h section 4c), on the walk seam (docs/testing.md).
//
// The fixture (tests/qp/fixtures/e2-d4/YRK_YORKNET_qp26_m42.txt) is YORKNET's restoration qp26
// (YRK_YORKNET_qp26.txt) warm-started from the walk state at the top of pass 42 of the
// sparse-equilibration diagnosis's matched run (docs/notes/data/2026-11-m8-wa1/), with the trust
// region baked into the bounds (the walk's own effective bounds). From there the walk drops a
// bound, rides negative curvature, and the ride's add leaves an INDEFINITE face whose inertia
// reads a trusted kWrong. The next EQP step p heads for that face's saddle: (Hx + g)'p = +50.4
// and p'Hp = -86.6 (verified), from an iterate on its rows. Before WA-1 the guard did not ride:
// the dual_mu-regularized p leaves two equality rows by mu * lambda (4.5e-4 on row 191, relative
// 2e-6 > kRideNullspaceTol), so `ride_stays_in_working_set(p)` refused and the ordinary step
// raised f by 13.6, left the iterate off row 191, and the walk entered a period-5 cycle to its
// minor cap. With WA-1 the guard reads the step in the regularized face's penalty merit
// f + |A_W x - b_W|^2 / (2 mu) and goes along -p when the merit rises too (walk_engine.h 4c).
//
// The assertions are the descent of every ordinary step from an iterate on its rows (the
// row-restoring steps of an iterate off its rows are excluded), that the footprint branch fired,
// and the KKT battery if the walk certifies. MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER
// ASSERTED.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/walk_engine.h>

#include "qp/support/qp_dump.h"
#include "support/route_reference.h"

using namespace hven;
using namespace hven::detail;
using hven::Index;
using hven::Vec;
using hven::test_support::QpDump;
using Obs = hven::detail::testing::QpWalkObserver;
using Action = hven::detail::testing::QpWalkAction;
using Record = hven::detail::testing::QpWalkMinorRecord;

namespace {

constexpr auto kBorder = QpWorkingSetAlgebra::kSchurBorder;
constexpr auto kRefactorize = QpWorkingSetAlgebra::kRefactorize;

const char *alg_name(QpWorkingSetAlgebra alg) { return alg == kBorder ? "border" : "refactorize"; }

struct Walk {
    QpSolution solution;
    std::vector<Record> records;
    bool truncated = false;
};

Walk observed(const QpDump &d, const QpOptions &o) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    w.solution = hven::test_support::solve_qp_dump(d, o);
    w.records = Obs::records;
    w.truncated = Obs::truncated;
    Obs::active = false;
    Obs::reset();
    return w;
}

/// THE DESCENT INVARIANT OF EVERY ORDINARY STEP FROM AN ITERATE ON ITS ROWS. Consecutive records
/// are a pass and its successor, so the successor's `obj_in` is the objective the pass reached.
/// Counted over the ordinary steps (`kAdd`, `kMoveOnly`) that MOVED from an iterate whose working
/// rows and pins hold to `kOnRows` (a row-restoring step may legitimately pay for feasibility in
/// f), whatever the direction's own null-space reading -- that reading is what WA-1 is about --
/// and whose factorization was trusted.
struct OnRowsTally {
    Index ordinary = 0;
    Index ordinary_rose = 0;
    double worst_rise = 0.0;
    Index footprint_rides =
        0; ///< passes whose guard rode -p on the footprint branch (ascent_footprint == 2)
};

constexpr double kOnRows = 1e-6;
// A pass whose factorization reported perturbed pivots (verdict kSuspect) solved its EQP through an
// untrustworthy factor; its step is outside the invariant this file pins (the start of this walk
// takes one such step, alpha 3.9e-8, f +2.4e-9 with p'Hp > 0 -- an inexact solve, not a saddle).
constexpr int kSuspect = static_cast<int>(hven::detail::InertiaVerdict::kSuspect);

OnRowsTally on_rows_tally(const std::vector<Record> &records) {
    OnRowsTally t;
    for (std::size_t k = 0; k < records.size(); ++k) {
        const Record &r = records[k];
        t.footprint_rides += r.ascent_footprint == 2 ? 1 : 0;
        if (k + 1 == records.size()) {
            break;
        }
        const double next = records[k + 1].obj_in;
        if (!std::isfinite(r.obj_in) || !std::isfinite(next) || !(r.dx_inf > r.step_tol) ||
            !(r.active_res_inf <= kOnRows) || r.ascent_ride || r.inertia_verdict == kSuspect ||
            (r.action != Action::kAdd && r.action != Action::kMoveOnly)) {
            continue;
        }
        const double floor = 1e-12 * std::max(1.0, std::abs(r.obj_in));
        ++t.ordinary;
        t.ordinary_rose += next - r.obj_in > floor ? 1 : 0;
        t.worst_rise = std::max(t.worst_rise, next - r.obj_in);
    }
    return t;
}

QpDump qp26_m42() {
    return hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                            "/YRK_YORKNET_qp26_m42.txt");
}

void expect_qp26_m42(QpWorkingSetAlgebra alg) {
    const QpDump d = qp26_m42();
    ASSERT_TRUE(d.has_seed);
    ASSERT_FALSE(std::isfinite(d.overrides.trust_region_radius)) << "the radius is in the bounds";
    QpOptions o = d.opts;
    o.working_set_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const Walk w = observed(d, o);
    const QpSolution &s = w.solution;
    ASSERT_FALSE(w.truncated);
    const OnRowsTally t = on_rows_tally(w.records);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf(
        "[ wa1 ] YRK_YORKNET_qp26_m42 %s status=%d minors=%ld facts=%ld f=%.12g "
        "ascent_rides=%ld footprint_rides=%ld on_rows_ordinary=%ld rose=%ld "
        "worst_rise=%.3e eq_drift=%ld eq=%.2e stat=%.2e comp=%.2e\n",
        alg_name(alg), static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
        static_cast<long>(s.counters.factorizations), f, static_cast<long>(s.counters.ascent_rides),
        static_cast<long>(t.footprint_rides), static_cast<long>(t.ordinary),
        static_cast<long>(t.ordinary_rose), t.worst_rise,
        static_cast<long>(s.counters.eq_drift_violations), k.eq, k.stationarity, k.complementarity);

    // The guard's own invariants, on every build: every flagged ride is counted, a footprint ride
    // is a guard ride, and the walk ends inside its cap.
    Index flagged = 0;
    for (const Record &r : w.records) {
        flagged += r.ascent_ride ? 1 : 0;
        if (r.ascent_footprint == 2) {
            EXPECT_TRUE(r.ascent_ride) << "minor " << r.minor;
        }
    }
    EXPECT_EQ(flagged, s.counters.ascent_rides);
    // The reversed step stays inside the ordinary step's row budget: no equality row drifts.
    EXPECT_EQ(s.counters.eq_drift_violations, 0);
    EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kIterationLimit));
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_EQ(t.ordinary_rose, 0) << "an ordinary step from an iterate on its rows ascended, by "
                                     "up to "
                                  << t.worst_rise;
    EXPECT_GE(t.footprint_rides, 1) << "the regularization-footprint branch did not fire";
    ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
#else
    hven::test_support::record_off_reference_route(
        std::string("yorknet_qp26_m42_") + alg_name(alg),
        "status=" + std::to_string(static_cast<int>(s.status)) +
            " minors=" + std::to_string(s.counters.minor_iters) +
            " ascent_rides=" + std::to_string(s.counters.ascent_rides) +
            " footprint_rides=" + std::to_string(t.footprint_rides) +
            " on_rows_ordinary_rose=" + std::to_string(t.ordinary_rose) +
            " worst_rise=" + std::to_string(t.worst_rise) + " f=" + std::to_string(f));
    ASSERT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kInfeasible));
    if (s.status != QpStatus::kOptimal) {
        return;
    }
#endif
    EXPECT_LE(k.eq, 1e-6);
    EXPECT_LE(k.box, 1e-8);
    EXPECT_LE(k.stationarity, 1e-6);
    EXPECT_LE(k.sign, 1e-8);
    EXPECT_LE(k.complementarity, 1e-6);
}

} // namespace

TEST(QpWa1AscentFootprint, YorknetQp26MatchedFaceBorder) { expect_qp26_m42(kBorder); }

TEST(QpWa1AscentFootprint, YorknetQp26MatchedFaceRefactorize) { expect_qp26_m42(kRefactorize); }

// THE DECISION ITSELF (detail::footprint_decision), on hand-made numbers. Along -p the objective
// is f(x) - a s + a^2 php / 2 and the merit M(x) - a ms + a^2 mc / 2 (ms = s + merit_slope_part,
// mc = php + merit_curv_part): the move must be M's own minimiser along -p, and both must fall.
constexpr double kNoRowBudget = std::numeric_limits<double>::infinity();

TEST(QpWa1AscentFootprint, DecisionNothingWhenTheMeritDoesNotRise) {
    // f rises along p but the rows' part makes the merit fall: the ordinary step stands.
    const auto d = detail::footprint_decision(1.0, -2.0, -1.5, 0.5, 1e-12, true, 3.0, kNoRowBudget);
    EXPECT_FALSE(d.ride);
    EXPECT_EQ(d.move, 0.0);
    // A NaN slope decides nothing.
    const auto n =
        detail::footprint_decision(std::nan(""), -2.0, 0.0, 0.5, 1e-12, true, 3.0, kNoRowBudget);
    EXPECT_FALSE(n.ride);
    EXPECT_EQ(n.move, 0.0);
}

TEST(QpWa1AscentFootprint, DecisionRidesWhenTheMeritIsConcaveAlongP) {
    // mc = -2 + 0.5 < 0: both fall for every alpha -- to the blocker, or unbounded to the ride.
    EXPECT_TRUE(
        detail::footprint_decision(1.0, -2.0, 0.0, 0.5, 1e-12, true, 3.0, kNoRowBudget).ride);
    const auto u = detail::footprint_decision(1.0, -2.0, 0.0, 0.5, 1e-12, false, 0.0, kNoRowBudget);
    EXPECT_TRUE(u.ride);
    EXPECT_EQ(u.move, 0.0);
}

TEST(QpWa1AscentFootprint, DecisionRidesWhenTheBlockerComesFirst) {
    // mc = -2 + 6 = 4, ms = 1 + 1 = 2: M's minimiser along -p at a = 0.5; a blocker at 0.5 wins.
    EXPECT_TRUE(
        detail::footprint_decision(1.0, -2.0, 1.0, 6.0, 1e-12, true, 0.5, kNoRowBudget).ride);
    EXPECT_TRUE(
        detail::footprint_decision(1.0, -2.0, 1.0, 6.0, 1e-12, true, 0.25, kNoRowBudget).ride);
}

TEST(QpWa1AscentFootprint, DecisionMovesToTheMeritMinimiserBeforeTheBlocker) {
    const double s = 1.0, php = -2.0, msp = 1.0, mcp = 6.0;
    for (const bool blocked : {true, false}) {
        const auto d =
            detail::footprint_decision(s, php, msp, mcp, 1e-12, blocked, 0.75, kNoRowBudget);
        EXPECT_FALSE(d.ride);
        ASSERT_DOUBLE_EQ(d.move, -0.5); // x += move * p: a = 0.5 along -p
        const double a = -d.move;
        const double df = -a * s + 0.5 * a * a * php;
        const double dm = -a * (s + msp) + 0.5 * a * a * (php + mcp);
        EXPECT_LT(df, 0.0);
        EXPECT_LT(dm, 0.0);
        // The minimiser: the merit's slope along -p vanishes there.
        EXPECT_NEAR(-(s + msp) + a * (php + mcp), 0.0, 1e-15);
    }
}

TEST(QpWa1AscentFootprint, DecisionStopsAtTheRowBudget) {
    // M concave along p (would ride to the blocker at 3.0), but a working row reaches its
    // tolerance at 0.4 along -p: a move to the row budget, nothing joins.
    const auto d = detail::footprint_decision(1.0, -2.0, 0.0, 0.5, 1e-12, true, 3.0, 0.4);
    EXPECT_FALSE(d.ride);
    EXPECT_DOUBLE_EQ(d.move, -0.4);
    // The nearer of M's minimiser (0.5) and the row budget (0.3) stops the move.
    EXPECT_DOUBLE_EQ(detail::footprint_decision(1.0, -2.0, 1.0, 6.0, 1e-12, false, 0.0, 0.3).move,
                     -0.3);
    // A blocker no later than the budget still rides.
    EXPECT_TRUE(detail::footprint_decision(1.0, -2.0, 0.0, 0.5, 1e-12, true, 0.4, 0.4).ride);
}
