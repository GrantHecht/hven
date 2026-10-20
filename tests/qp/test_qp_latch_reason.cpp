// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_latch_reason.cpp -- M7 W2 RLA, on the walk seam (docs/testing.md): THE LATCH AND ITS
// REASON (qp_engine.h, section 4). Border mode latches onto the elimination path where a K0
// rebuild cannot help; a kCap latch (more pins than schur_cap) releases when the pins are back
// under the cap, as before, while a kConditioning latch (C ill-conditioned or singular with the
// pins under the cap) is no longer re-tested at every pass. It releases when K0's rows move, when
// a CULPRIT pin (one C's near-null or dominant direction lives on) leaves, when a pin arrives at
// an indefinite C judged mild, or on the kLatchChangedEntries-th entry on another set.
//
// Three layers: SchurComplement's two extreme directions and definite(); detail::take_latch and
// detail::latch_still_holds on hand-built border states (every branch of the rule); and the walk,
// whose counters must show the reason it latched for -- on a synthetic convex fixture, with the
// answer refactorize mode gives, and on BRITGAS's first subproblem, where the thrash was measured.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;

namespace {

SpMatRM identity_upper(Index n) {
    SpMatRM K(n, n);
    for (Index i = 0; i < n; ++i) {
        K.insert(i, i) = 1.0;
    }
    K.makeCompressed();
    return K;
}

// A border state over K0 = I_n (no constraint rows) with each listed variable pinned by a unit
// border, in that order, exactly as sync_borders leaves it: the ledger names each pin. With
// K0^-1 = I the border's diagonal d = c + 1 makes C's entry for that pin exactly c, so C is
// diag(c) in add order, with no pivot of K0 near the backend's perturbation threshold.
void pin_borders(BorderState &b, Index n, const std::vector<std::pair<Index, double>> &pins,
                 const QpOptions &opts) {
    detail::factorize_checked(b.kkt, identity_upper(n));
    b.schur.emplace(b.kkt, opts);
    for (const auto &[i, c] : pins) {
        Vec v = Vec::Zero(n);
        v(i) = 1.0;
        b.schur->add_border(v, c + 1.0);
        b.ledger.push_back({BorderLedgerEntry::Kind::kVarPin, i});
    }
}

WorkingSet ws_with(Index n, Index mi, const std::vector<Index> &pins,
                   const std::vector<Index> &rows = {}) {
    WorkingSet ws(n, mi);
    for (const Index i : pins) {
        ws.bound_state()[static_cast<std::size_t>(i)] = BoundState::kAtUpper;
    }
    for (const Index r : rows) {
        ws.add_ineq(r);
    }
    return ws;
}

// A conditioning latch taken by hand on pins {0,1,2,3}, rows {1}; culprits {0,1}.
void hand_conditioning_latch(BorderState &b, bool arrival_cures) {
    b.latched = true;
    b.latch_reason = LatchReason::kConditioning;
    b.latch_pins = {0, 1, 2, 3};
    b.latch_rows = {1};
    b.latch_culprits = {0, 1};
    b.latch_arrival_cures = arrival_cures;
    b.latch_changed_entries = 0;
}

constexpr Index kN = 8;
constexpr Index kMi = 3;
constexpr Index kCap = 6;

} // namespace

// ---- SchurComplement: the two extreme directions -------------------------------------------

// C = diag(-1e11, -1, -1e3) over pins {0, 1, 3}. The near-null direction lives on pin 1, the
// dominant one on pin 0; pin 3 sits in neither.
TEST(QpLatchReason, SchurExtremeDirectionsFindTheirBorders) {
    QpOptions opts;
    BorderState b;
    pin_borders(b, 4, {{0, -1e11}, {1, -1.0}, {3, -1e3}}, opts);
    ASSERT_TRUE(b.schur->needs_refactorization()); // cond ~1e11 > schur_cond_max
    EXPECT_TRUE(b.schur->definite());

    const Vec lo = b.schur->near_null_direction();
    const Vec hi = b.schur->dominant_direction();
    ASSERT_EQ(lo.size(), 3);
    ASSERT_EQ(hi.size(), 3);
    EXPECT_DOUBLE_EQ(lo.cwiseAbs().maxCoeff(), 1.0);
    EXPECT_DOUBLE_EQ(hi.cwiseAbs().maxCoeff(), 1.0);
    EXPECT_DOUBLE_EQ(std::abs(lo(1)), 1.0);
    EXPECT_LT(std::abs(lo(0)), 1e-6);
    EXPECT_LT(std::abs(lo(2)), detail::kLatchCulpritFrac);
    EXPECT_DOUBLE_EQ(std::abs(hi(0)), 1.0);
    EXPECT_LT(std::abs(hi(1)), 1e-6);
    EXPECT_LT(std::abs(hi(2)), detail::kLatchCulpritFrac);
}

TEST(QpLatchReason, SchurDirectionsAreEmptyWithoutBordersAndDefiniteNeedsOneSign) {
    QpOptions opts;
    BorderState b;
    pin_borders(b, 3, {}, opts);
    EXPECT_EQ(b.schur->near_null_direction().size(), 0);
    EXPECT_EQ(b.schur->dominant_direction().size(), 0);
    EXPECT_FALSE(b.schur->definite());

    BorderState c;
    pin_borders(c, 3, {{0, -1.0}, {1, 2.0}}, opts); // indefinite
    EXPECT_FALSE(c.schur->definite());
    EXPECT_EQ(c.schur->near_null_direction().size(), 2);
    EXPECT_EQ(c.schur->dominant_direction().size(), 2);

    BorderState d;
    pin_borders(d, 3, {{0, 1.0}, {2, 2.0}}, opts); // every eigenvalue positive
    EXPECT_TRUE(d.schur->definite());
}

// An exactly singular C has no usable factor, so no near-null direction; C = 0 has no dominant
// one either (the power step vanishes); and definite() is false.
TEST(QpLatchReason, SchurExactlySingularHasNoNearNullDirection) {
    QpOptions opts;
    BorderState b;
    pin_borders(b, 2, {{0, 0.0}}, opts);
    ASSERT_TRUE(b.schur->needs_refactorization());
    EXPECT_EQ(b.schur->near_null_direction().size(), 0);
    EXPECT_EQ(b.schur->dominant_direction().size(), 0);
    EXPECT_FALSE(b.schur->definite());

    BorderState c;
    pin_borders(c, 2, {{0, 0.0}, {1, -2.0}}, opts); // singular, a nonzero dominant direction
    ASSERT_TRUE(c.schur->needs_refactorization());
    EXPECT_EQ(c.schur->near_null_direction().size(), 0);
    const Vec hi = c.schur->dominant_direction();
    ASSERT_EQ(hi.size(), 2);
    EXPECT_DOUBLE_EQ(std::abs(hi(1)), 1.0);
    EXPECT_EQ(hi(0), 0.0);
}

// ---- take_latch -------------------------------------------------------------------------------

TEST(QpLatchReason, TakeLatchPastTheCapRecordsTheCapReason) {
    QpOptions opts;
    opts.schur_cap = 2;
    BorderState b;
    pin_borders(b, 4, {{0, -1.0}, {1, -1.0}, {2, -1.0}}, opts);
    const WorkingSet ws = ws_with(4, 0, {0, 1, 2});
    QpCounters c;
    detail::take_latch(ws, b, c, opts);
    EXPECT_TRUE(b.latched);
    EXPECT_EQ(b.latch_reason, LatchReason::kCap);
    EXPECT_EQ(c.border_latches_cap, 1);
    EXPECT_EQ(c.border_latches_conditioning, 0);
}

// At the cap exactly the reason is conditioning: the cap test is `pinned > schur_cap`.
TEST(QpLatchReason, TakeLatchUnderTheCapRecordsTheMembersAndTheCulprits) {
    QpOptions opts;
    opts.schur_cap = 3;
    BorderState b;
    pin_borders(b, 4, {{0, -1e11}, {1, -1.0}, {3, -1e3}}, opts);
    WorkingSet ws = ws_with(4, 2, {0, 1, 3}, {1});
    QpCounters c;
    b.latch_changed_entries = 5; // stale: a latch starts its own count
    detail::take_latch(ws, b, c, opts);
    EXPECT_TRUE(b.latched);
    EXPECT_EQ(b.latch_reason, LatchReason::kConditioning);
    EXPECT_EQ(c.border_latches_conditioning, 1);
    EXPECT_EQ(c.border_latches_cap, 0);
    EXPECT_EQ(b.latch_pins, (std::vector<Index>{0, 1, 3}));
    EXPECT_EQ(b.latch_rows, (std::vector<Index>{1}));
    EXPECT_EQ(b.latch_culprits, (std::vector<Index>{0, 1}));
    EXPECT_FALSE(b.latch_arrival_cures); // definite C
    EXPECT_EQ(b.latch_changed_entries, 0);
}

// An indefinite C mildly past the limit may be cured by an arrival; a severe one, or a definite
// one, is not trusted to be.
TEST(QpLatchReason, TakeLatchJudgesWhetherAnArrivalMayCure) {
    QpOptions opts;
    const auto latch_on = [&](double c0, double c1, double c2) {
        BorderState b;
        pin_borders(b, 3, {{0, c0}, {1, c1}, {2, c2}}, opts);
        EXPECT_TRUE(b.schur->needs_refactorization()) << c2;
        const WorkingSet ws = ws_with(3, 0, {0, 1, 2});
        QpCounters c;
        detail::take_latch(ws, b, c, opts);
        EXPECT_EQ(b.latch_reason, LatchReason::kConditioning);
        return b.latch_arrival_cures;
    };
    const double limit = detail::kLatchArrivalCondFactor * opts.schur_cond_max;
    // Indefinite, cond 3e9: past schur_cond_max (1e9), within the factor.
    EXPECT_TRUE(latch_on(-1.0, 1.0, -3e9));
    // Indefinite at the factor exactly, and just past it.
    EXPECT_TRUE(latch_on(-1.0, 1.0, -limit));
    EXPECT_FALSE(latch_on(-1.0, 1.0, -2.0 * limit));
    // Definite, cond 3e9: an arrival cannot cure it.
    EXPECT_FALSE(latch_on(-1.0, -1.0, -3e9));
    // Indefinite, cond 1, but uniformly tiny: nearly singular.
    EXPECT_FALSE(latch_on(-1e-13, 1e-13, -1e-13));
}

// No usable direction (an exactly singular C): every pin is a culprit.
TEST(QpLatchReason, TakeLatchOnASingularCMakesEveryPinACulprit) {
    QpOptions opts;
    BorderState b;
    pin_borders(b, 3, {{0, 0.0}, {2, -1.0}}, opts);
    ASSERT_TRUE(b.schur->needs_refactorization());
    const WorkingSet ws = ws_with(3, 0, {0, 2});
    QpCounters c;
    detail::take_latch(ws, b, c, opts);
    EXPECT_EQ(b.latch_reason, LatchReason::kConditioning);
    EXPECT_EQ(b.latch_culprits, (std::vector<Index>{0, 2}));
    EXPECT_FALSE(b.latch_arrival_cures);
}

// ---- latch_still_holds: every branch ------------------------------------------------------------

TEST(QpLatchReason, PinsPastTheCapHoldEitherReasonAndCountNothing) {
    for (const LatchReason why : {LatchReason::kCap, LatchReason::kConditioning}) {
        BorderState b;
        hand_conditioning_latch(b, true);
        b.latch_reason = why;
        const WorkingSet ws = ws_with(kN, kMi, {2, 3, 4, 5, 6, 7, 0}); // 7 > kCap, culprit 1 gone
        QpCounters c;
        EXPECT_TRUE(detail::latch_still_holds(ws, b, c, kCap));
        EXPECT_EQ(c.border_latch_holds, 0);
        EXPECT_EQ(b.latch_changed_entries, 0);
    }
}

TEST(QpLatchReason, ACapLatchReleasesUnderTheCap) {
    BorderState b;
    hand_conditioning_latch(b, false);
    b.latch_reason = LatchReason::kCap;
    const WorkingSet ws = ws_with(kN, kMi, {0, 1, 2, 3}, {1}); // the very latch-time set
    QpCounters c;
    EXPECT_FALSE(detail::latch_still_holds(ws, b, c, kCap));
    EXPECT_EQ(c.border_latch_holds, 0);
}

TEST(QpLatchReason, AConditioningLatchReleasesWhenTheRowsMove) {
    for (const std::vector<Index> &rows :
         {std::vector<Index>{}, std::vector<Index>{1, 2}, std::vector<Index>{0}}) {
        BorderState b;
        hand_conditioning_latch(b, false);
        const WorkingSet ws = ws_with(kN, kMi, {0, 1, 2, 3}, rows);
        QpCounters c;
        EXPECT_FALSE(detail::latch_still_holds(ws, b, c, kCap)) << rows.size();
    }
}

TEST(QpLatchReason, AConditioningLatchReleasesWhenACulpritLeaves) {
    for (const Index gone : {0, 1}) {
        BorderState b;
        hand_conditioning_latch(b, false);
        std::vector<Index> pins = {0, 1, 2, 3, 5};
        pins.erase(std::find(pins.begin(), pins.end(), gone));
        const WorkingSet ws = ws_with(kN, kMi, pins, {1});
        QpCounters c;
        EXPECT_FALSE(detail::latch_still_holds(ws, b, c, kCap)) << gone;
    }
}

TEST(QpLatchReason, AConditioningLatchHoldsWhenOnlyANonCulpritLeaves) {
    BorderState b;
    hand_conditioning_latch(b, true); // arrivals would cure -- but nothing arrives
    const WorkingSet ws = ws_with(kN, kMi, {0, 1, 3}, {1});
    QpCounters c;
    EXPECT_TRUE(detail::latch_still_holds(ws, b, c, kCap));
    EXPECT_EQ(c.border_latch_holds, 1);
    EXPECT_EQ(b.latch_changed_entries, 1);
}

// A swap -- non-culprit 2 out, 6 in -- is an arrival: it releases a latch whose C an arrival may
// cure, and holds one whose C it cannot.
TEST(QpLatchReason, AnArrivalReleasesOnlyWhereItMayCure) {
    const WorkingSet swap = ws_with(kN, kMi, {0, 1, 3, 6}, {1});
    const WorkingSet grow = ws_with(kN, kMi, {0, 1, 2, 3, 6}, {1});
    for (const WorkingSet *ws : {&swap, &grow}) {
        BorderState cures;
        hand_conditioning_latch(cures, true);
        QpCounters c1;
        EXPECT_FALSE(detail::latch_still_holds(*ws, cures, c1, kCap));
        EXPECT_EQ(c1.border_latch_holds, 0);

        BorderState stays;
        hand_conditioning_latch(stays, false);
        QpCounters c2;
        EXPECT_TRUE(detail::latch_still_holds(*ws, stays, c2, kCap));
        EXPECT_EQ(c2.border_latch_holds, 1);
        EXPECT_EQ(stays.latch_changed_entries, 1);
    }
}

// The latch-time set re-forms the identical C: it holds without spending the bound, however often.
TEST(QpLatchReason, TheLatchTimeSetHoldsWithoutSpendingTheBound) {
    BorderState b;
    hand_conditioning_latch(b, true);
    const WorkingSet ws = ws_with(kN, kMi, {0, 1, 2, 3}, {1});
    QpCounters c;
    for (Index k = 0; k < 3 * detail::kLatchChangedEntries; ++k) {
        ASSERT_TRUE(detail::latch_still_holds(ws, b, c, kCap)) << k;
    }
    EXPECT_EQ(b.latch_changed_entries, 0);
    EXPECT_EQ(c.border_latch_holds, 3 * detail::kLatchChangedEntries);
}

// On any other set the latch holds kLatchChangedEntries - 1 entries and releases on the next.
TEST(QpLatchReason, TheChangedEntryBoundReleases) {
    BorderState b;
    hand_conditioning_latch(b, false);
    const WorkingSet other = ws_with(kN, kMi, {0, 1, 2, 3, 6}, {1}); // an arrival it cannot cure
    const WorkingSet same = ws_with(kN, kMi, {0, 1, 2, 3}, {1});
    QpCounters c;
    for (Index k = 1; k < detail::kLatchChangedEntries; ++k) {
        ASSERT_TRUE(detail::latch_still_holds(other, b, c, kCap)) << k;
        ASSERT_TRUE(detail::latch_still_holds(same, b, c, kCap)) << k; // interleaved: not counted
    }
    EXPECT_EQ(b.latch_changed_entries, detail::kLatchChangedEntries - 1);
    EXPECT_FALSE(detail::latch_still_holds(other, b, c, kCap));
    EXPECT_EQ(c.border_latch_holds, 2 * (detail::kLatchChangedEntries - 1));
}

// ---- the walk ---------------------------------------------------------------------------------

namespace {

// min 1/2 x'Hx + g'x over a box. x0 and x1 share a nearly singular 2x2 block of H (eigenvalues
// 2 - 1e-6 and 1e-6), so once both are pinned the pins' block of K0^-1 -- C, up to the -mu
// diagonal -- has a condition estimate near 5e5, past the fixture's schur_cond_max (1e4) while
// K0 itself is far from any pivot the backend would perturb. x2 stays free; x3..x6 are decoupled
// and reach their upper bounds one per pass AFTER x0 and x1 (step 0.3..0.6 against their 0.2).
// Convex: C stays definite, so those four arrivals cannot cure it and the conditioning latch
// holds through them.
QpProblem singular_pair_then_arrivals_qp() {
    const Index n = 7;
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(n, n);
    Hd(0, 1) = 1.0 - 1e-6;
    Hd(1, 0) = 1.0 - 1e-6;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    qp.g << -10.0, -10.0, -0.5, -10.0, -10.0, -10.0, -10.0;
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(n);
    qp.lower(2) = -1e6;
    qp.upper = Vec(n);
    qp.upper << 1.0, 1.0, 1e6, 3.0, 4.0, 5.0, 6.0;
    return qp;
}

} // namespace

TEST(QpLatchReason, TheWalkHoldsAConditioningLatchThroughArrivalsItCannotCure) {
    const QpProblem qp = singular_pair_then_arrivals_qp();
    QpOptions opts;
    opts.schur_cond_max = 1e4; // the pinned pair's C (~5e5) past it, every K0 pivot clean

    QpOptions ref_opts = opts;
    ref_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    const QpSolution ref = QpEngine{ref_opts}.solve(qp);
    ASSERT_EQ(ref.status, QpStatus::kOptimal);

    QpOptions bor_opts = opts;
    bor_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    const QpSolution bor = QpEngine{bor_opts}.solve(qp);
    ASSERT_EQ(bor.status, QpStatus::kOptimal);
    EXPECT_LT((bor.x - ref.x).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_EQ(bor.bound_state, ref.bound_state);

    const QpCounters &c = bor.counters;
    std::printf("[ rla ] latches cap=%ld cond=%ld releases=%ld holds=%ld facts=%ld minors=%ld\n",
                static_cast<long>(c.border_latches_cap),
                static_cast<long>(c.border_latches_conditioning),
                static_cast<long>(c.border_latch_releases), static_cast<long>(c.border_latch_holds),
                static_cast<long>(c.factorizations), static_cast<long>(c.minor_iters));
    EXPECT_EQ(c.border_latches_cap, 0);
    EXPECT_EQ(c.border_latches_conditioning, 1);
    EXPECT_EQ(c.border_latch_releases, 0);
    EXPECT_GE(c.border_latch_holds, 4); // x3..x6 arrive under the latch
}

// The same walk with a cap below the pin count: the latch is a cap latch, never re-tested on its
// conditioning, and it releases only when the pins are back under the cap.
TEST(QpLatchReason, TheWalkRecordsACapLatchAsBefore) {
    const QpProblem qp = singular_pair_then_arrivals_qp();
    QpOptions opts;
    opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    opts.schur_cap = 1;
    const QpSolution s = QpEngine{opts}.solve(qp);
    ASSERT_EQ(s.status, QpStatus::kOptimal);
    EXPECT_GE(s.counters.border_latches_cap, 1);
    EXPECT_EQ(s.counters.border_latches_conditioning, 0);
    EXPECT_EQ(s.counters.border_latch_holds, 0);
}

// THE NATURAL CASE. BRITGAS's first `default` subproblem (n = 450, me = 360, no rows; in-cell at
// 6f12bec2: kInfeasible after 508 minors and 1024 factorizations): from minor 11 on, C over ~11
// to ~90 pins is ill-conditioned with the pins far under schur_cap, and the reason-blind latch
// released, rebuilt K0, re-bordered every pin and re-latched at EVERY pass -- two factorizations
// per minor. Over its first kMinors minors the reason-aware latch must take conditioning latches,
// hold some passes, release on others (its culprits leave, or pins arrive at a mild indefinite C),
// and pay well under the reason-blind two factorizations per latched minor.
TEST(QpLatchReason, BritgasMainSubproblemHoldsItsConditioningLatch) {
    constexpr Index kMinors = 120;
    constexpr Index kSlack = 24; // the start repair's probes and the first K0 (7 in Release)
    const hven::test_support::QpDump d = hven::test_support::load_qp_dump(
        std::string(HVEN_QP_E2D4_FIXTURES) + "/RLA_BRITGAS_qp0.txt");
    ASSERT_NE(d.result.find("status=2 minors=508 facts=1024"), std::string::npos) << d.result;
    QpOptions o = d.opts;
    ASSERT_EQ(o.ws_algebra, WorkingSetLinearAlgebra::kSchurBorder);
    o.max_iter = kMinors;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    const QpCounters &c = s.counters;
    std::printf(
        "[ rla ] BRITGAS qp0 first %ld: status=%d minors=%ld facts=%ld cap=%ld cond=%ld "
        "releases=%ld holds=%ld\n",
        static_cast<long>(kMinors), static_cast<int>(s.status), static_cast<long>(c.minor_iters),
        static_cast<long>(c.factorizations), static_cast<long>(c.border_latches_cap),
        static_cast<long>(c.border_latches_conditioning),
        static_cast<long>(c.border_latch_releases), static_cast<long>(c.border_latch_holds));
    EXPECT_EQ(c.border_latches_cap, 0);
    EXPECT_GE(c.border_latches_conditioning, 1);
    EXPECT_GE(c.border_latch_releases, 1);
    EXPECT_GE(c.border_latch_holds, 1);
    // A latched pass costs its one eliminated factorization and a release one K0 rebuild more;
    // a held pass is one the reason-blind latch paid both on.
    EXPECT_LE(c.factorizations, c.minor_iters + c.border_latch_releases + kSlack);
}
