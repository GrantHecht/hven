// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_stagnation_stop.cpp -- M7 W2 SNX, on the walk seam (docs/testing.md): THE STAGNATION
// READING (qp_engine.h, section 4). A run of ordinary full steps keeps the working set fixed; when
// it stops making progress the next pass is a KKT point of the working set for the drop rule alone,
// and with nothing to drop (or the readings spent) the walk ends kMaxIter at the iterate it stands
// on instead of spending the rest of its minor cap re-solving the same face.
//
// The fixtures are two `default`-label BRITGAS restoration subproblems that ran to their minor cap
// in-cell at 5ecd0dea (each dump's `result` line: status=1, minors=11700) on long idle runs. Both
// are feasible (HiGHS phase-1 l1 = 0) and nonconvex, so no global oracle applies; the reference is
// the in-cell walk's own capped endpoint, `x_final`, which the walk must not do worse than.
//   resto_qp4  -- the face's rows cannot be met by the regularized step (a dual_mu * lambda
//                 footprint on a rank-deficient, consistent face): the residual stays put.
//   resto_qp27 -- the iterate meets its rows and stands at the face's stationary point, which
//                 the KKT-point test does not certify: the steps are rounding-sized.
// The QpE2Fix9StandstillRetreat, QpNc2FlatFace and QpBpsBorderCertificate fixtures are the
// complementary case: idle runs a drop resolves, certified as before.

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

// MINOR AND FACTORIZATION COUNTS ARE BOUNDED, NOT PINNED: Release and Debug round these
// subproblems differently. The status, the stop counter, the objective against the capped
// endpoint and the rows are the assertions.
void expect_stopped_like_the_cap(const char *file, bool ends_on_a_stop) {
    const QpDump d =
        hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    // The premise: in-cell, this subproblem ran to its minor cap.
    ASSERT_NE(d.result.find("status=1 minors=11700"), std::string::npos) << d.result;
    ASSERT_TRUE(d.opts.independent_working_set) << "the dump is a `default`-label solve";
    ASSERT_EQ(d.x_final.size(), d.qp.n());

    const QpSolution s = hven::test_support::solve_qp_dump(d, d.opts);

    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const double f_cap = hven::test_support::qp_dump_objective(d, d.x_final);
    const double eq = (d.qp.Ae * s.x - d.qp.be).lpNorm<Eigen::Infinity>();
    const double eq_cap = (d.qp.Ae * d.x_final - d.qp.be).lpNorm<Eigen::Infinity>();
    std::printf("[ snx ] %s status=%d minors=%ld facts=%ld stagnation_drops=%ld "
                "stagnation_stops=%ld f=%.15g f_cap=%.15g eq=%.3e eq_cap=%.3e\n",
                file, static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.factorizations),
                static_cast<long>(s.counters.stagnation_drops),
                static_cast<long>(s.counters.stagnation_stops), f, f_cap, eq, eq_cap);
    std::printf("[ snx ] %s classification footprint: terminal_polish_solves=%ld saddle_exits=%ld "
                "saddle_exit_misses=%ld convex_certificates=%ld certificate_sign_refusals=%ld "
                "certificate_bound_refusals=%ld certificate_row_refusals=%ld\n",
                file, static_cast<long>(s.counters.terminal_polish_solves),
                static_cast<long>(s.counters.saddle_exits),
                static_cast<long>(s.counters.saddle_exit_misses),
                static_cast<long>(s.counters.convex_certificates),
                static_cast<long>(s.counters.certificate_sign_refusals),
                static_cast<long>(s.counters.certificate_bound_refusals),
                static_cast<long>(s.counters.certificate_row_refusals));

    // The reading answered the idle run: a drop, a stop, or both.
    EXPECT_GE(s.counters.stagnation_drops + s.counters.stagnation_stops, 1);
    EXPECT_LE(s.counters.stagnation_drops, hven::solvers::detail::kStagnationDropBudget);
    EXPECT_LE(s.counters.stagnation_stops, 1);
    EXPECT_LT(s.counters.minor_iters, 3000) << "the reading ends the idle run, not the cap";
    // A stop is the cap's own status; a walk the drops carried on finishes by its own exits. A
    // reading with nothing to drop never reaches the classification.
    if (ends_on_a_stop) {
        EXPECT_EQ(s.counters.stagnation_stops, 1);
        // This walk never reached the classification: no polish, no saddle exit.
        EXPECT_EQ(s.counters.terminal_polish_solves, 0);
        EXPECT_EQ(s.counters.saddle_exits + s.counters.saddle_exit_misses, 0);
    }
    if (s.counters.stagnation_stops == 1) {
        EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
    }
    ASSERT_TRUE(s.status == QpStatus::kMaxIter || s.status == QpStatus::kOptimal)
        << static_cast<int>(s.status);
    // No worse than the capped endpoint in objective, rows met as well.
    EXPECT_LE(f, f_cap + 1e-12 * std::max(1.0, std::abs(f_cap)));
    EXPECT_LE(eq, 2.0 * eq_cap + 1e-12);
    if (s.status == QpStatus::kOptimal) {
        const hven::test_support::QpDumpKkt k = hven::test_support::qp_dump_kkt(d, s);
        EXPECT_LE(k.eq, 1e-8);
        EXPECT_LE(k.box, 1e-9);
    }
    // Inside the box: the stop moves nothing.
    for (hven::Index i = 0; i < d.qp.n(); ++i) {
        ASSERT_GE(s.x(i), std::max(d.qp.lower(i), -d.overrides.tr_radius) - 1e-12) << i;
        ASSERT_LE(s.x(i), std::min(d.qp.upper(i), d.overrides.tr_radius) + 1e-12) << i;
    }
}

} // namespace

TEST(QpStagnationStop, BritgasRestorationFootprintFace) {
    expect_stopped_like_the_cap("SNX_BRITGAS_resto_qp4.txt", /*ends_on_a_stop=*/false);
}

TEST(QpStagnationStop, BritgasRestorationUncertifiedStationaryPoint) {
    expect_stopped_like_the_cap("SNX_BRITGAS_resto_qp27.txt", /*ends_on_a_stop=*/true);
}

// The reading belongs to the independent walk: the option-off walk keeps its cap.
TEST(QpStagnationStop, TheOptionOffWalkNeverStops) {
    const QpDump d = hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) +
                                                      "/SNX_BRITGAS_resto_qp27.txt");
    QpOptions o = d.opts;
    o.independent_working_set = false;
    o.max_iter = 400;
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);
    std::printf("[ snx ] option-off status=%d minors=%ld stagnation_stops=%ld\n",
                static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.stagnation_stops));
    EXPECT_EQ(s.counters.stagnation_stops, 0);
    EXPECT_EQ(s.counters.stagnation_drops, 0);
}
