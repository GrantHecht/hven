// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_ascent_footprint.cpp -- M8 WA-1: THE ASCENT GUARD's REGULARIZATION FOOTPRINT
// (walk_engine.h section 4c), on the walk seam (docs/testing.md).
//
// On a nearly dependent working set the refined dual_mu-regularized EQP step p leaves rows the
// iterate meets by the dual_mu footprint, so the guard's null-space test declines and the ordinary
// step from an on-row iterate can ascend f toward an indefinite face's saddle. The rule: on a
// trusted-kWrong face, ride -p to a blocker whose landing point keeps the working rows inside the
// row budget, else a dual_mu rung, else the ordinary step (counted); on a kOk face nothing is taken
// (counted when the penalty merit rises along p).
//
// Three layers: the two helpers on hand-made data; a synthetic dense QP (indefinite H, two nearly
// parallel rows -- an equality and a working inequality -- and a pinned column) driving the kWrong
// branches end to end; and YORKNET's restoration qp26 (tests/qp/fixtures/e2-d4/
// YRK_YORKNET_qp26_m42.txt, warm-started at pass 42 of the sparse-equilibration diagnosis's matched
// run, docs/notes/data/2026-11-m8-wa1/) whose base walk cycles to its minor cap through ordinary
// ascending steps. MINOR AND FACTORIZATION COUNTS ARE PRINTED, NEVER ASSERTED.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include <Eigen/Dense>
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
using Footprint = hven::detail::testing::QpWalkFootprint;
using Record = hven::detail::testing::QpWalkMinorRecord;

namespace {

constexpr auto kBorder = QpWorkingSetAlgebra::kSchurBorder;
constexpr auto kRefactorize = QpWorkingSetAlgebra::kRefactorize;

const char *alg_name(QpWorkingSetAlgebra alg) { return alg == kBorder ? "border" : "refactorize"; }

double objective(const QpProblem &qp, const Vec &x) {
    return 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x) + qp.g.dot(x);
}

struct Walk {
    QpSolution solution;
    std::vector<Record> records;
    bool truncated = false;
};

template <class Solve> Walk observed(Solve &&solve) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    w.solution = solve();
    w.records = Obs::records;
    w.truncated = Obs::truncated;
    Obs::active = false;
    Obs::reset();
    return w;
}

/// THE DESCENT INVARIANT OF EVERY ORDINARY STEP FROM AN ITERATE ON ITS ROWS, and the branch's
/// own tallies. Consecutive records are a pass and its successor, so the successor's `obj_in` is
/// the objective the pass reached. Counted over the ordinary steps (`kAdd`, `kMoveOnly`) that MOVED
/// from an iterate whose working rows and pins hold to `kOnRows` (a row-restoring step may pay for
/// feasibility in f) on a factorization that was not kSuspect, the rule's counted declines (the
/// ordinary step it had nothing to replace with) and kMeritFalls passes (the merit, not f, falls)
/// excepted.
struct Tally {
    Index ordinary = 0;
    Index ordinary_rose = 0;
    double worst_rise = 0.0;
    Index rides = 0; ///< kRide records
    Index rides_fell = 0;
    Index rungs = 0;       ///< kRung records
    Index reverts = 0;     ///< kRungReverted records
    Index refused = 0;     ///< kRideRefused records
    Index declined = 0;    ///< kDeclined records
    Index followed = 0;    ///< rides whose following pass completed, not a decline
    Index follow_rose = 0; ///< of those, landing above the ride's starting f
};

constexpr double kOnRows = 1e-6;
// A pass on a factorization with perturbed pivots (verdict kSuspect) solved its EQP through an
// untrustworthy factor; its step is outside the invariant (qp26's walk takes one such step at its
// start, alpha 3.9e-8, f +2.4e-9 with p'Hp > 0 -- an inexact solve, not a saddle).
constexpr int kSuspect = static_cast<int>(hven::detail::InertiaVerdict::kSuspect);

Tally tally(const std::vector<Record> &records) {
    Tally t;
    for (std::size_t k = 0; k < records.size(); ++k) {
        const Record &r = records[k];
        t.rides += r.ascent_footprint == Footprint::kRide ? 1 : 0;
        t.rungs += r.ascent_footprint == Footprint::kRung ? 1 : 0;
        t.reverts += r.ascent_footprint == Footprint::kRungReverted ? 1 : 0;
        t.refused += r.ascent_footprint == Footprint::kRideRefused ? 1 : 0;
        t.declined += r.ascent_footprint == Footprint::kDeclined ? 1 : 0;
        if (k + 1 == records.size()) {
            break;
        }
        const double next = records[k + 1].obj_in;
        // A ride's gain survives the pass after it -- that pass lands at or below the ride's start
        // -- unless that pass is the rule's counted decline.
        if (r.ascent_footprint == Footprint::kRide && k + 2 < records.size() &&
            records[k + 1].ascent_footprint != Footprint::kDeclined) {
            ++t.followed;
            t.follow_rose +=
                records[k + 2].obj_in - r.obj_in > 1e-12 * std::max(1.0, std::abs(r.obj_in)) ? 1
                                                                                             : 0;
        }
        // A ride falls (f's own rounding floor: a short ride's decrease can sit below it).
        if (r.ascent_footprint == Footprint::kRide &&
            !(next - r.obj_in > 1e-12 * std::max(1.0, std::abs(r.obj_in)))) {
            ++t.rides_fell;
        }
        if (!std::isfinite(r.obj_in) || !std::isfinite(next) || !(r.dx_inf > r.step_tol) ||
            !(r.active_res_inf <= kOnRows) || r.ascent_ride || r.inertia_verdict == kSuspect ||
            r.ascent_footprint == Footprint::kDeclined ||
            r.ascent_footprint == Footprint::kMeritFalls ||
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

/// The branch's records against its counters: every outcome is counted exactly once, a ride is a
/// guard ride, rungs stay inside their cap and move nothing.
void expect_counted(const Walk &w) {
    const Tally t = tally(w.records);
    const QpCounters &c = w.solution.counters;
    EXPECT_EQ(t.rides, c.ascent_footprint_rides);
    EXPECT_EQ(t.rungs, c.ascent_footprint_rungs);
    EXPECT_EQ(t.reverts, c.ascent_footprint_rung_reverts);
    EXPECT_EQ(t.refused, c.ascent_footprint_ride_refusals);
    EXPECT_LE(c.ascent_footprint_rung_reverts, 1) << "a reverted rung ends footprint rungs";
    EXPECT_EQ(t.declined, c.ascent_footprint_declines);
    EXPECT_LE(c.ascent_footprint_rungs, kMaxFootprintRungs);
    EXPECT_LE(c.ascent_footprint_rides, c.ascent_rides);
    Index flagged = 0;
    for (const Record &r : w.records) {
        flagged += r.ascent_ride ? 1 : 0;
        if (r.inertia_verdict == kSuspect && r.ascent_footprint != Footprint::kRungReverted) {
            EXPECT_EQ(static_cast<int>(r.ascent_footprint), static_cast<int>(Footprint::kNotAsked))
                << "a kSuspect pass is not the rule's case; minor " << r.minor;
        }
        if (r.ascent_footprint == Footprint::kRide) {
            EXPECT_TRUE(r.ascent_ride) << "minor " << r.minor;
        }
        if (r.ascent_footprint == Footprint::kRungReverted) {
            EXPECT_EQ(r.inertia_verdict, kSuspect) << "minor " << r.minor;
        }
        if (r.ascent_footprint == Footprint::kRung ||
            r.ascent_footprint == Footprint::kRungReverted ||
            r.ascent_footprint == Footprint::kRideRefused) {
            EXPECT_EQ(static_cast<int>(r.action), static_cast<int>(Action::kFootprintRung));
            EXPECT_FALSE(r.ascent_ride);
            EXPECT_EQ(r.dx_inf, 0.0) << "minor " << r.minor;
        }
        if (r.ascent_footprint == Footprint::kDeclined) {
            EXPECT_FALSE(r.ascent_ride) << "minor " << r.minor;
        }
    }
    EXPECT_EQ(flagged, c.ascent_rides);
    // A pass solved again without moving hands its successor the same iterate and the same drop.
    for (std::size_t k = 0; k + 1 < w.records.size(); ++k) {
        const Record &r = w.records[k];
        if (r.ascent_footprint == Footprint::kRung ||
            r.ascent_footprint == Footprint::kRungReverted ||
            r.ascent_footprint == Footprint::kRideRefused) {
            EXPECT_EQ(w.records[k + 1].obj_in, r.obj_in) << "minor " << r.minor;
            EXPECT_EQ(w.records[k + 1].post_drop, r.post_drop) << "minor " << r.minor;
        }
        if (r.ascent_footprint == Footprint::kRideRefused) {
            EXPECT_NE(static_cast<int>(w.records[k + 1].ascent_footprint),
                      static_cast<int>(Footprint::kRide))
                << "a refused ride re-taken; minor " << r.minor;
        }
    }
}

// ---------------------------------------------------------------------------
// THE HELPERS, on hand-made data.
// ---------------------------------------------------------------------------

/// n = 2; one equality row a_e = (1, 1), b_e = 1; two inequality rows a_0 = (1, -1), b_0 = 0 and
/// a_1 = (0, 2), b_1 = 1, of which only row 0 is in the working set.
QpProblem two_by_two() {
    QpProblem qp;
    Eigen::MatrixXd H(2, 2);
    H << 1.0, 0.0, 0.0, -1.0;
    qp.H = H.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1.0, 1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Ones(1);
    Eigen::MatrixXd Ai(2, 2);
    Ai << 1.0, -1.0, 0.0, 2.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec(2);
    qp.bi << 0.0, 1.0;
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

TEST(QpWa1AscentFootprint, MeritSlopeReadsTheWorkingRowsOnly) {
    const QpProblem qp = two_by_two();
    WorkingSet ws(2, 2);
    ws.add_ineq(0);
    Vec x(2);
    x << 0.75, 0.5; // r_e = 0.25, r_0 = 0.25, r_1 = 0 (row 1 is not working either way)
    const Vec Aix = qp.Ai * x;
    Vec gx(2);
    gx << 2.0, -1.0;
    const double mu = 0.5;
    Vec d(2);
    d << 1.0, 3.0; // a_e'd = 4, a_0'd = -2, a_1'd = 6
    const FootprintSlope m =
        footprint_merit_slope(qp, ws, {.x = x, .Aix = Aix, .gx = gx, .d = d, .mu = mu});
    // gx'd = -1; rows: (0.25 * 4 + 0.25 * -2) / 0.5 = 1.
    EXPECT_DOUBLE_EQ(m.slope, -1.0 + 1.0);
    EXPECT_DOUBLE_EQ(m.tol, kEngineDenomTol * (gx.norm() * d.norm() + (1.0 + 0.5) / mu));
    // Row 1 joins: it is at its bound times 2 off (r_1 = 2 * 0.5 - 1 = 0) -- nothing changes.
    ws.add_ineq(1);
    EXPECT_DOUBLE_EQ(
        footprint_merit_slope(qp, ws, {.x = x, .Aix = Aix, .gx = gx, .d = d, .mu = mu}).slope, 0.0);
}

TEST(QpWa1AscentFootprint, MeritSlopeSignCases) {
    const QpProblem qp = two_by_two();
    const WorkingSet ws(2, 2); // the equality row alone
    const Vec gx = Vec::Zero(2);
    Vec d(2);
    d << 1.0, 0.0; // a_e'd = 1
    const double mu = 1e-8;
    for (const double r : {1e-9, -1e-9}) {
        Vec x(2);
        x << 1.0 + r, 0.0;
        const Vec Aix = qp.Ai * x;
        const FootprintSlope m =
            footprint_merit_slope(qp, ws, {.x = x, .Aix = Aix, .gx = gx, .d = d, .mu = mu});
        // Moving further off the row raises M, back toward it lowers M: r * a'd / mu.
        EXPECT_NEAR(m.slope, r / mu, 1e-6 * std::abs(r / mu));
        const Vec minus_d = -d;
        EXPECT_NEAR(
            footprint_merit_slope(qp, ws, {.x = x, .Aix = Aix, .gx = gx, .d = minus_d, .mu = mu})
                .slope,
            -r / mu, 1e-6 * std::abs(r / mu));
        EXPECT_GT(m.tol, 0.0);
    }
    // On the row the merit's slope is f's.
    Vec x(2);
    x << 0.25, 0.75;
    Vec g(2);
    g << 3.0, -2.0;
    const Vec Aix = qp.Ai * x;
    EXPECT_DOUBLE_EQ(
        footprint_merit_slope(qp, ws, {.x = x, .Aix = Aix, .gx = g, .d = d, .mu = mu}).slope, 3.0);
}

TEST(QpWa1AscentFootprint, RowBudgetAtItsEdge) {
    // Strictly inside the shift test (1x the floor) and the structural margin (10x).
    EXPECT_GT(kFootprintRowBudget, 0.0);
    EXPECT_LT(kFootprintRowBudget, 1.0);
    const QpProblem qp = two_by_two();
    WorkingSet ws(2, 2);
    ws.add_ineq(0);
    const Vec n1e = (Eigen::MatrixXd(qp.Ae).cwiseAbs().rowwise().sum());
    const Vec n1i = (Eigen::MatrixXd(qp.Ai).cwiseAbs().rowwise().sum());
    const double ftol = 1e-9;
    const auto budget = [&](const Vec &x) {
        return FootprintBudgetArgs{
            .x = x, .ai_row_norm1 = n1i, .ae_row_norm1 = n1e, .feasibility_tol = ftol};
    };
    // Equality row at x = (1 + r, 0): row_scale = max(1, |b| = 1, |a|_1 |x|_inf = 2 (1 + r)).
    const auto at_eq = [&](double r) {
        Vec x(2);
        x << 1.0 + r, 0.0;
        // keep working row 0 satisfied: its residual is x0 - x1 = 1 + r > 0, so take it out
        WorkingSet e(2, 2);
        return footprint_rows_within_budget(qp, e, budget(x));
    };
    const double edge = kFootprintRowBudget * ftol * 2.0; // scale 2 (1 + r) ~ 2
    EXPECT_TRUE(at_eq(0.0));
    EXPECT_TRUE(at_eq(0.99 * edge));
    EXPECT_TRUE(at_eq(-0.99 * edge));
    EXPECT_FALSE(at_eq(1.01 * edge));
    EXPECT_FALSE(at_eq(-1.01 * edge));
    // The working inequality row is read on its infeasible side only: x = (0.5 + s/2, 0.5 - s/2)
    // keeps the equality row exact and puts r_0 = s.
    const auto at_ineq = [&](double s) {
        Vec x(2);
        x << 0.5 + 0.5 * s, 0.5 - 0.5 * s;
        return footprint_rows_within_budget(qp, ws, budget(x));
    };
    const double iedge = kFootprintRowBudget * ftol * 1.0; // scale max(1, 0, 2 * ~0.5) = 1
    EXPECT_TRUE(at_ineq(0.99 * iedge));
    EXPECT_FALSE(at_ineq(1.01 * iedge));
    EXPECT_TRUE(at_ineq(-0.25)) << "slack is feasible";
    // The scale is the landing point's own: far from the origin the same residual fits.
    Vec far(2);
    far << 1e6 + 1.0 + 1e-4, -1e6; // row_scale = |a_e|_1 |x|_inf ~ 2e6; r_e = 1e-4 < 1e-3
    EXPECT_TRUE(footprint_rows_within_budget(qp, WorkingSet(2, 2), budget(far)));
    // A NaN fails.
    Vec nan(2);
    nan << std::nan(""), 0.0;
    EXPECT_FALSE(footprint_rows_within_budget(qp, WorkingSet(2, 2), budget(nan)));
}

// ---------------------------------------------------------------------------
// THE SYNTHETIC kWRONG FACE. x = (x0, x1, x2, x3); H = diag(1, 1, -1, 1), g = (0, -1, 1, 0); x3
// fixed at 0 (the pin). Equality x0 + x3 = 0; working inequality x0 + eps x1 <= 0, nearly parallel
// to it; inequalities -x2 <= c (the blocker along -p) and x2 <= u. From x = 0 the working face
// fixes x0 and x1 and leaves x2 free with curvature -1: the EQP step heads for x2 = 1, the face's
// saddle, with (Hx + g)'p = +1 and p'Hp = -1, and the multipliers are of order 1/eps, so the
// regularized step leaves the two rows by about mu/eps -- relative to |p|, far past
// kRideNullspaceTol. The landing point along -p is at x2 = -c, its rows off by about c mu / eps.
// ---------------------------------------------------------------------------
QpProblem synthetic_face(double eps, double c, double u = 10.0) {
    QpProblem qp;
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(4, 4);
    H.diagonal() << 1.0, 1.0, -1.0, 1.0;
    qp.H = H.sparseView();
    qp.g = Vec(4);
    qp.g << 0.0, -1.0, 1.0, 0.0;
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 4);
    Ae << 1.0, 0.0, 0.0, 1.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Zero(1);
    Eigen::MatrixXd Ai = Eigen::MatrixXd::Zero(3, 4);
    Ai << 1.0, eps, 0.0, 0.0, //
        0.0, 0.0, -1.0, 0.0,  //
        0.0, 0.0, 1.0, 0.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec(3);
    qp.bi << 0.0, c, u;
    qp.lower = Vec::Constant(4, -1e20);
    qp.upper = Vec::Constant(4, 1e20);
    qp.lower(3) = 0.0;
    qp.upper(3) = 0.0;
    return qp;
}

// THE SYNTHETIC kOK FACE (found by a scan of the family, recorded in the WA-1 r5 evidence).
// x = (x0, x1, x2); H = diag(-0.1, -0.1, 1), g = (0, 1, 0). Equality x0 = 0; working inequality
// x0 + eps x1 <= 0 with eps^2 / 2 = 10 mu, so H + A'A / mu is positive definite (inertia kOk)
// while H is negative on the rows' span; rows -x1 <= 10 and x1 <= 10 bound the problem. The refined
// step from x = 0 leaves the rows and rises along p ((Hx + g)'p = +1.0e-3, p'Hp = -1.0e-7). From
// x0 = 5e-7 at mu = 1e-10 (inside the row's tolerance) the same p restores the row, and the merit's
// row part -r_0^2 / mu outweighs f's rise: M falls along p.
QpProblem synthetic_kok_face(double mu = 1e-8) {
    const double eps = std::sqrt(2.0 * 10.0 * mu);
    QpProblem qp;
    Eigen::MatrixXd H = Eigen::MatrixXd::Zero(3, 3);
    H.diagonal() << -0.1, -0.1, 1.0;
    qp.H = H.sparseView();
    qp.g = Vec(3);
    qp.g << 0.0, 1.0, 0.0;
    Eigen::MatrixXd Ae = Eigen::MatrixXd::Zero(1, 3);
    Ae << 1.0, 0.0, 0.0;
    qp.Ae = Ae.sparseView();
    qp.be = Vec::Zero(1);
    Eigen::MatrixXd Ai = Eigen::MatrixXd::Zero(3, 3);
    Ai << 1.0, eps, 0.0, //
        0.0, -1.0, 0.0,  //
        0.0, 1.0, 0.0;
    qp.Ai = Ai.sparseView();
    qp.bi = Vec(3);
    qp.bi << 0.0, 10.0, 10.0;
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    return qp;
}

Walk walk_seeded(const QpProblem &qp, QpWorkingSetAlgebra alg, double mu = 1e-8, double x0 = 0.0) {
    QpOptions o;
    o.max_iterations = 60;
    o.working_set_algebra = alg;
    o.dual_regularization = mu;
    QpSolution seed;
    seed.x = Vec::Zero(qp.n());
    seed.x(0) = x0;
    seed.ineq_active.assign(static_cast<std::size_t>(qp.mi()), false);
    seed.ineq_active[0] = true;
    return observed(
        [&] { return QpEngine(o, 0, hven::linear::DensePolicy::kNever).solve(qp, seed); });
}

void print_walk(const std::string &tag, const QpProblem &qp, const Walk &w) {
    const QpSolution &s = w.solution;
    std::printf("[ wa1 ] %s status=%d minors=%ld f=%.12g rides=%ld refused=%ld rungs=%ld "
                "declines=%ld mu_rungs=%ld\n",
                tag.c_str(), static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                objective(qp, s.x), static_cast<long>(s.counters.ascent_footprint_rides),
                static_cast<long>(s.counters.ascent_footprint_ride_refusals),
                static_cast<long>(s.counters.ascent_footprint_rungs),
                static_cast<long>(s.counters.ascent_footprint_declines),
                static_cast<long>(s.counters.dual_regularization_decreases));
    for (const Record &r : w.records) {
        std::printf("    minor %ld action=%d fp=%d verdict=%d obj_in=%.9g dx=%.3e res=%.3e "
                    "mu=%.1e post_drop=%d ride_outcome=%d\n",
                    static_cast<long>(r.minor), static_cast<int>(r.action),
                    static_cast<int>(r.ascent_footprint), r.inertia_verdict, r.obj_in, r.dx_inf,
                    r.active_res_inf, r.dual_mu, r.post_drop ? 1 : 0,
                    static_cast<int>(r.ride_outcome));
    }
}

/// The route-independent half of every synthetic walk: the records match the counters, every
/// footprint ride lowers f and the pass after it keeps the gain, no ordinary step from an iterate
/// on its rows rises, and the walk ends inside its cap and never infeasible.
void expect_portable(const QpProblem &qp, const Walk &w) {
    ASSERT_FALSE(w.truncated);
    expect_counted(w);
    const Tally t = tally(w.records);
    EXPECT_EQ(t.rides_fell, t.rides) << "a footprint ride did not lower f";
    EXPECT_EQ(t.follow_rose, 0) << "the pass after a footprint ride undid it";
    EXPECT_EQ(t.ordinary_rose, 0) << "worst rise " << t.worst_rise;
    EXPECT_NE(static_cast<int>(w.solution.status), static_cast<int>(QpStatus::kInfeasible));
    EXPECT_NE(static_cast<int>(w.solution.status), static_cast<int>(QpStatus::kIterationLimit));
    for (Index i = 0; i < qp.n(); ++i) {
        if (qp.lower(i) == qp.upper(i)) {
            EXPECT_EQ(w.solution.x(i), qp.lower(i)) << "the pin " << i;
        }
    }
}

/// The first pass's branch, asserted on the reference build and recorded off it.
void expect_first(const Walk &w, Footprint want, const std::string &key) {
    ASSERT_FALSE(w.records.empty());
    const Record &r = w.records.front();
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_EQ(static_cast<int>(r.ascent_footprint), static_cast<int>(want));
#else
    hven::test_support::record_off_reference_route(
        key, "first_footprint=" + std::to_string(static_cast<int>(r.ascent_footprint)) +
                 " verdict=" + std::to_string(r.inertia_verdict) +
                 " want=" + std::to_string(static_cast<int>(want)));
#endif
}

} // namespace

// THE RIDE: the blocker at c = 1e-6 is inside the row budget, so the first pass rides -p to it
// and f falls; the landing point's rows are inside the budget.
TEST(QpWa1AscentFootprint, SyntheticKWrongFaceRidesToABlockerInsideTheBudget) {
    const double c = 1e-6;
    const QpProblem qp = synthetic_face(1e-4, c);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("ride ") + alg_name(alg), qp, w);
        expect_portable(qp, w);
        expect_first(w, Footprint::kRide, std::string("wa1_synthetic_ride_") + alg_name(alg));
        if (!w.records.empty() && w.records.front().ascent_footprint == Footprint::kRide) {
            ASSERT_GE(w.records.size(), 2U);
            EXPECT_EQ(static_cast<int>(w.records.front().action),
                      static_cast<int>(Action::kRideStepped));
            EXPECT_LT(w.records[1].obj_in, w.records[0].obj_in);
            // The landing point (the next pass's own reading; row_scale 1 there).
            EXPECT_LE(w.records[1].active_res_inf, kFootprintRowBudget * 1e-9);
            EXPECT_NEAR(w.solution.x(2), -c, 1e-12) << "the ride's blocker joined and held";
        }
    }
}

// THE RUNG: the blocker at c = 1 lies past the row budget, so the pass takes a dual_mu rung --
// nothing moves, the same working set is solved again at mu / 100 -- and never a move along -p.
TEST(QpWa1AscentFootprint, SyntheticKWrongFaceTakesARungWhenTheBlockerIsPastTheBudget) {
    const QpProblem qp = synthetic_face(1e-4, 1.0);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("rung ") + alg_name(alg), qp, w);
        expect_portable(qp, w);
        expect_first(w, Footprint::kRung, std::string("wa1_synthetic_rung_") + alg_name(alg));
        for (std::size_t k = 0; k + 1 < w.records.size(); ++k) {
            if (w.records[k].ascent_footprint == Footprint::kRung ||
                w.records[k].ascent_footprint == Footprint::kRungReverted) {
                EXPECT_EQ(w.records[k + 1].obj_in, w.records[k].obj_in) << "a rung moved x";
            }
            if (w.records[k].ascent_footprint == Footprint::kRung) {
                EXPECT_DOUBLE_EQ(w.records[k + 1].dual_mu,
                                 w.records[k].dual_mu * kDualRegDecreaseFactor);
            }
        }
        EXPECT_LE(w.solution.counters.dual_regularization_decreases, kMaxDualRegDecreases);
    }
}

// THE DECLINE: from dual_mu = 1e-6 (eps = 1e-4) the footprint stays past kRideNullspaceTol through
// the rung (1e-8) on trusted kWrong factors and the blocker stays past the budget, so with the rung
// spent the ordinary step stands -- counted.
TEST(QpWa1AscentFootprint, SyntheticKWrongFaceDeclinesWithTheRungsSpent) {
    const QpProblem qp = synthetic_face(1e-4, 1.0);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg, 1e-6);
        print_walk(std::string("decline ") + alg_name(alg), qp, w);
        ASSERT_FALSE(w.truncated);
        expect_counted(w);
        // The blocker at c = 1 is past the budget at every rung (footprint ~ c mu / eps >= 1e-6).
        EXPECT_EQ(w.solution.counters.ascent_footprint_rides, 0);
        for (const Record &r : w.records) {
            if (r.ascent_footprint == Footprint::kDeclined) {
                EXPECT_TRUE(r.action == Action::kAdd || r.action == Action::kMoveOnly)
                    << "a decline takes the ordinary step; minor " << r.minor;
            }
        }
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_EQ(w.solution.counters.ascent_footprint_rungs, kMaxFootprintRungs);
        EXPECT_GE(w.solution.counters.ascent_footprint_declines, 1);
#else
        hven::test_support::record_off_reference_route(
            std::string("wa1_synthetic_decline_") + alg_name(alg),
            "rungs=" + std::to_string(w.solution.counters.ascent_footprint_rungs) +
                " declines=" + std::to_string(w.solution.counters.ascent_footprint_declines));
#endif
    }
}

// THE DEGENERATE PIVOT: rows -x2 <= 0 and x2 <= 0 are tight at x = 0 and out of the working set,
// so neither the ride along -p nor the step along p moves: the ordinary add, no rung.
TEST(QpWa1AscentFootprint, SyntheticKWrongFaceAtADegenerateVertexTakesTheOrdinaryAdd) {
    const QpProblem qp = synthetic_face(1e-4, 0.0, 0.0);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("degenerate ") + alg_name(alg), qp, w);
        expect_portable(qp, w);
        expect_first(w, Footprint::kDegenerate,
                     std::string("wa1_synthetic_degenerate_") + alg_name(alg));
        if (!w.records.empty() && w.records.front().ascent_footprint == Footprint::kDegenerate) {
            EXPECT_EQ(static_cast<int>(w.records.front().action), static_cast<int>(Action::kAdd));
            EXPECT_EQ(w.records.front().dual_mu, 1e-8);
        }
    }
}

// THE RIDE REFUSED AT ITS LANDING POINT: an upper bound x0 <= 1e-10 is the ride's first blocker,
// inside the row budget, and dependent on the working set (the bound completes the equality row on
// the pinned column x3) with no member to exchange; the add site passes it over to the blocker at
// x2 = -c = -4e-5, where the rows sit outside the budget. The pass is solved again without the ride
// (a rung, then the ride at mu = 1e-10 lands inside it).
TEST(QpWa1AscentFootprint, SyntheticRidePassedOverOutOfTheBudgetIsRefused) {
    QpProblem qp = synthetic_face(1e-4, 4e-5);
    qp.upper(0) = 1e-10;
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("refused ") + alg_name(alg), qp, w);
        expect_portable(qp, w);
        // Every ride lands inside the row budget (row_scale 1 at these points).
        for (std::size_t k = 0; k + 1 < w.records.size(); ++k) {
            if (w.records[k].ascent_footprint == Footprint::kRide) {
                EXPECT_LE(w.records[k + 1].active_res_inf, kFootprintRowBudget * 1e-9)
                    << "a footprint ride landed outside the row budget; minor "
                    << w.records[k].minor;
            }
        }
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_EQ(w.solution.counters.ascent_footprint_ride_refusals, 1);
        EXPECT_EQ(static_cast<int>(w.solution.status), static_cast<int>(QpStatus::kOptimal));
#else
        hven::test_support::record_off_reference_route(
            std::string("wa1_synthetic_refused_") + alg_name(alg),
            "refusals=" + std::to_string(w.solution.counters.ascent_footprint_ride_refusals) +
                " status=" + std::to_string(static_cast<int>(w.solution.status)));
#endif
    }
}

// THE kOK DECLINE: on the kOk face the refined p raises the penalty merit; nothing is taken, the
// ordinary step stands, counted.
TEST(QpWa1AscentFootprint, SyntheticKOkFaceWhoseMeritRisesDeclines) {
    const QpProblem qp = synthetic_kok_face();
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("kok ") + alg_name(alg), qp, w);
        ASSERT_FALSE(w.truncated);
        expect_counted(w);
        EXPECT_EQ(w.solution.counters.ascent_footprint_rides, 0);
        EXPECT_EQ(w.solution.counters.ascent_footprint_rungs, 0);
        expect_first(w, Footprint::kDeclined, std::string("wa1_synthetic_kok_") + alg_name(alg));
        if (!w.records.empty() && w.records.front().ascent_footprint == Footprint::kDeclined) {
            EXPECT_TRUE(w.records.front().action == Action::kAdd ||
                        w.records.front().action == Action::kMoveOnly);
        }
    }
}

// THE MERIT GATE: from an iterate inside its row's tolerance the refined p restores the row, M
// falls along it, and the ordinary step along p stands, not counted.
TEST(QpWa1AscentFootprint, SyntheticKOkFaceWhoseMeritFallsKeepsTheRefinedStep) {
    const QpProblem qp = synthetic_kok_face(1e-10);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg, 1e-10, 5e-7);
        print_walk(std::string("merit-falls ") + alg_name(alg), qp, w);
        ASSERT_FALSE(w.truncated);
        expect_counted(w);
        expect_first(w, Footprint::kMeritFalls,
                     std::string("wa1_synthetic_merit_falls_") + alg_name(alg));
        if (!w.records.empty() && w.records.front().ascent_footprint == Footprint::kMeritFalls) {
            EXPECT_TRUE(w.records.front().action == Action::kAdd ||
                        w.records.front().action == Action::kMoveOnly);
            EXPECT_EQ(w.solution.counters.ascent_footprint_declines, 0);
        }
    }
}

// THE REVERTED RUNG: at eps = 3e-5 the first pass takes a rung and, on the eliminated path, the
// face at mu = 1e-10 factors with perturbed pivots (kSuspect): the rung is reverted -- dual_mu
// restored, no further footprint rung -- and the face is solved again at mu = 1e-8. The border path
// reads that face trusted (kWrong) and keeps the rung.
TEST(QpWa1AscentFootprint, SyntheticRungOnASuspectFaceIsReverted) {
    const QpProblem qp = synthetic_face(3e-5, 1.0);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg);
        print_walk(std::string("revert ") + alg_name(alg), qp, w);
        expect_portable(qp, w);
        for (std::size_t k = 0; k + 1 < w.records.size(); ++k) {
            if (w.records[k].ascent_footprint == Footprint::kRungReverted) {
                EXPECT_DOUBLE_EQ(w.records[k + 1].dual_mu,
                                 w.records[k].dual_mu / kDualRegDecreaseFactor);
                for (std::size_t m = k + 1; m < w.records.size(); ++m) {
                    EXPECT_NE(static_cast<int>(w.records[m].ascent_footprint),
                              static_cast<int>(Footprint::kRung))
                        << "a footprint rung after a revert; minor " << w.records[m].minor;
                }
            }
        }
        if (alg != kRefactorize) {
            continue;
        }
#if HVEN_TEST_ROUTE_REFERENCE
        ASSERT_GE(w.records.size(), 3U);
        EXPECT_EQ(static_cast<int>(w.records[0].ascent_footprint),
                  static_cast<int>(Footprint::kRung));
        EXPECT_EQ(static_cast<int>(w.records[1].ascent_footprint),
                  static_cast<int>(Footprint::kRungReverted));
        EXPECT_EQ(w.solution.counters.ascent_footprint_rung_reverts, 1);
#else
        hven::test_support::record_off_reference_route(
            std::string("wa1_synthetic_revert_") + alg_name(alg),
            "reverts=" + std::to_string(w.solution.counters.ascent_footprint_rung_reverts));
#endif
    }
}

// A kSUSPECT PASS IS NOT THE RULE'S CASE: started at dual_mu = 1e-10, the eps = 3e-5 face factors
// with perturbed pivots on the eliminated path from the first pass, and its p meets every other
// entry test of the rule (rising, negative curvature, off the rows' null space, on the rows).
TEST(QpWa1AscentFootprint, SyntheticKSuspectPassIsNotTheRulesCase) {
    const QpProblem qp = synthetic_face(3e-5, 1.0);
    for (const auto alg : {kRefactorize, kBorder}) {
        SCOPED_TRACE(alg_name(alg));
        const Walk w = walk_seeded(qp, alg, 1e-10);
        print_walk(std::string("suspect ") + alg_name(alg), qp, w);
        ASSERT_FALSE(w.truncated);
        expect_counted(w); // every kSuspect record that is not a revert is kNotAsked
        if (alg != kRefactorize) {
            continue;
        }
#if HVEN_TEST_ROUTE_REFERENCE
        ASSERT_FALSE(w.records.empty());
        const Record &r = w.records.front();
        EXPECT_EQ(r.inertia_verdict, kSuspect);
        EXPECT_GT(r.step_slope, 0.0);
        EXPECT_LT(r.step_php, 0.0);
        EXPECT_EQ(r.step_in_face, 0);
        EXPECT_LE(r.active_res_inf, kOnRows);
#else
        hven::test_support::record_off_reference_route(
            std::string("wa1_synthetic_suspect_") + alg_name(alg),
            "first_verdict=" +
                std::to_string(w.records.empty() ? -9 : w.records.front().inertia_verdict));
#endif
    }
}

// ---------------------------------------------------------------------------
// YORKNET qp26 FROM PASS 42. From there the walk drops a bound, rides negative curvature, and the
// ride's add leaves an INDEFINITE face (trusted kWrong) whose refined EQP step heads for its saddle
// ((Hx + g)'p = +50.4, p'Hp = -86.6) and leaves two equality rows by the footprint (4.5e-4 on row
// 191). Without the rule the ordinary step raised f by 13.6 and the walk entered a period-5 cycle
// to its minor cap. With it the walk rides -p; the next pass is a kOk face whose merit rises along
// p, the rule declines (nothing is taken on kOk), and that ordinary step leaves the rows: the walk
// ends kNumericalError on the reference build. Eq drift is printed, not asserted.
// ---------------------------------------------------------------------------
namespace {

void expect_qp26_m42(QpWorkingSetAlgebra alg) {
    const QpDump d = hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                                      "/YRK_YORKNET_qp26_m42.txt");
    ASSERT_TRUE(d.has_seed);
    ASSERT_FALSE(std::isfinite(d.overrides.trust_region_radius)) << "the radius is in the bounds";
    QpOptions o = d.opts;
    o.working_set_algebra = alg;
    ASSERT_TRUE(o.independent_working_set) << "the dump is a `default`-label solve";
    const Walk w = observed([&] { return hven::test_support::solve_qp_dump(d, o); });
    const QpSolution &s = w.solution;
    ASSERT_FALSE(w.truncated);
    const Tally t = tally(w.records);
    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
    std::printf(
        "[ wa1 ] YRK_YORKNET_qp26_m42 %s status=%d minors=%ld facts=%ld f=%.12g "
        "ascent_rides=%ld fp_rides=%ld fp_refused=%ld fp_rungs=%ld fp_declines=%ld "
        "on_rows_ordinary=%ld rose=%ld worst_rise=%.3e eq_drift=%ld eq=%.2e stat=%.2e "
        "comp=%.2e\n",
        alg_name(alg), static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
        static_cast<long>(s.counters.factorizations), f, static_cast<long>(s.counters.ascent_rides),
        static_cast<long>(s.counters.ascent_footprint_rides),
        static_cast<long>(s.counters.ascent_footprint_ride_refusals),
        static_cast<long>(s.counters.ascent_footprint_rungs),
        static_cast<long>(s.counters.ascent_footprint_declines), static_cast<long>(t.ordinary),
        static_cast<long>(t.ordinary_rose), t.worst_rise,
        static_cast<long>(s.counters.eq_drift_violations), k.eq, k.stationarity, k.complementarity);
    expect_counted(w);
    EXPECT_EQ(t.rides_fell, t.rides) << "a footprint ride did not lower f";
    EXPECT_NE(static_cast<int>(s.status), static_cast<int>(QpStatus::kIterationLimit));
#if HVEN_TEST_ROUTE_REFERENCE
    EXPECT_EQ(t.ordinary_rose, 0) << "an ordinary step from an iterate on its rows ascended, by "
                                     "up to "
                                  << t.worst_rise;
    EXPECT_GE(t.rides, 1) << "the regularization-footprint branch took nothing";
    EXPECT_EQ(t.follow_rose, 0) << "the pass after a footprint ride undid it";
    EXPECT_GE(s.counters.ascent_footprint_declines, 1) << "the kOk face's decline";
    if (s.status != QpStatus::kOptimal) {
        return;
    }
#else
    hven::test_support::record_off_reference_route(
        std::string("yorknet_qp26_m42_") + alg_name(alg),
        "status=" + std::to_string(static_cast<int>(s.status)) + " minors=" +
            std::to_string(s.counters.minor_iters) + " fp_rides=" + std::to_string(t.rides) +
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
