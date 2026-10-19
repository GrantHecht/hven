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
#include <limits>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/qp_dump.h"

using namespace hven::solvers;
using hven::test_support::QpDump;

namespace {

// MINOR AND FACTORIZATION COUNTS ARE BOUNDED, NOT PINNED: Release and Debug round these
// subproblems differently. The status, the stop counter, the objective against the capped
// endpoint and the rows are the assertions.
void expect_stopped_like_the_cap(const char *file, bool ends_on_a_stop,
                                 const WorkingSetLinearAlgebra *alg = nullptr) {
    const QpDump d =
        hven::test_support::load_qp_dump(std::string(HVEN_QP_E2D4_FIXTURES) + "/" + file);
    // The premise: in-cell, this subproblem ran to its minor cap.
    ASSERT_NE(d.result.find("status=1 minors=11700"), std::string::npos) << d.result;
    ASSERT_TRUE(d.opts.independent_working_set) << "the dump is a `default`-label solve";
    ASSERT_EQ(d.x_final.size(), d.qp.n());

    QpOptions o = d.opts;
    if (alg != nullptr) {
        o.ws_algebra = *alg;
    }
    const QpSolution s = hven::test_support::solve_qp_dump(d, o);

    const double f = hven::test_support::qp_dump_objective(d, s.x);
    const double f_cap = hven::test_support::qp_dump_objective(d, d.x_final);
    const double eq = (d.qp.Ae * s.x - d.qp.be).lpNorm<Eigen::Infinity>();
    const double eq_cap = (d.qp.Ae * d.x_final - d.qp.be).lpNorm<Eigen::Infinity>();
    std::printf("[ snx ] %s alg=%d status=%d minors=%ld facts=%ld stagnation_drops=%ld "
                "stagnation_stops=%ld f=%.15g f_cap=%.15g eq=%.3e eq_cap=%.3e\n",
                file, static_cast<int>(o.ws_algebra), static_cast<int>(s.status),
                static_cast<long>(s.counters.minor_iters),
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
    // No worse than the capped endpoint in objective; the rows within twice the capped
    // endpoint's miss (the two endpoints are different points of the same footprint face).
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

TEST(QpStagnationStop, BritgasRestorationUncertifiedStationaryPointBorder) {
    constexpr WorkingSetLinearAlgebra kAlg = WorkingSetLinearAlgebra::kSchurBorder;
    expect_stopped_like_the_cap("SNX_BRITGAS_resto_qp27.txt", /*ends_on_a_stop=*/true, &kAlg);
}

TEST(QpStagnationStop, BritgasRestorationUncertifiedStationaryPointRefactorize) {
    constexpr WorkingSetLinearAlgebra kAlg = WorkingSetLinearAlgebra::kRefactorize;
    expect_stopped_like_the_cap("SNX_BRITGAS_resto_qp27.txt", /*ends_on_a_stop=*/true, &kAlg);
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

// ---- Synthetic fixtures, with the walk observer attached ----------------------------------------

namespace {

using Obs = hven::solvers::detail::testing::QpWalkObserver;
using Action = hven::solvers::detail::testing::QpWalkAction;
using Record = hven::solvers::detail::testing::QpWalkMinorRecord;

struct Walk {
    QpSolution solution;
    std::vector<Record> records;
};

Walk observed(const QpProblem &qp, const QpOptions &o) {
    Obs::reset();
    Obs::active = true;
    Walk w;
    const QpEngine engine{o};
    w.solution = engine.solve(qp);
    w.records = Obs::records;
    const bool truncated = Obs::truncated;
    Obs::active = false;
    Obs::reset();
    EXPECT_FALSE(truncated);
    return w;
}

QpProblem free_problem(hven::Index n) {
    QpProblem qp;
    qp.H.resize(n, n);
    qp.g = hven::Vec::Zero(n);
    qp.Ae.resize(0, n);
    qp.be = hven::Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = hven::Vec(0);
    qp.lower = hven::Vec::Constant(n, -std::numeric_limits<double>::infinity());
    qp.upper = hven::Vec::Constant(n, std::numeric_limits<double>::infinity());
    return qp;
}

long count_actions(const Walk &w, Action a) {
    return static_cast<long>(std::count_if(w.records.begin(), w.records.end(),
                                           [a](const Record &r) { return r.action == a; }));
}

void print_walk(const char *tag, const Walk &w) {
    const QpSolution &s = w.solution;
    std::printf("[ snx ] %s status=%d minors=%ld stagnation_drops=%ld stagnation_stops=%ld "
                "mu_refines=%ld mu_rescues=%ld last_action=%d\n",
                tag, static_cast<int>(s.status), static_cast<long>(s.counters.minor_iters),
                static_cast<long>(s.counters.stagnation_drops),
                static_cast<long>(s.counters.stagnation_stops),
                static_cast<long>(s.counters.terminal_mu_refines),
                static_cast<long>(s.counters.terminal_mu_refine_rescues),
                w.records.empty() ? -1 : static_cast<int>(w.records.back().action));
}

// A CONTRACTING RESIDUAL. a (x0 + x1) = 0.1 over two free variables, a = 1e-6, H = 0, g = 0 (the
// objective is constant, so its clause never counts as progress), no inequality rows unless
// `row_bound` > 0, no finite bounds: nothing to drop on the row alone. Under the proximal primal
// term delta and the dual term mu, one ordinary pass leaves the row residual r at r * rate with
// rate = (mu delta / (2 a^2 + mu delta))^2 (measured on the walk's own records below); dual_mu is
// chosen for the rate, primal_delta stays the default. The KKT-point test certifies once the
// stationarity residual |a lambda| falls under opt_tol, many passes after any reading.
struct Contracting {
    QpProblem qp;
    QpOptions o;
};

Contracting contracting_row(double rate, double row_bound) {
    Contracting c;
    c.o.max_iter = 3000;
    const double a = 1e-6;
    const double rho = std::sqrt(rate);
    c.o.dual_mu = 2.0 * a * a * rho / (1.0 - rho) / c.o.primal_delta;
    c.qp = free_problem(2);
    Eigen::MatrixXd Ae(1, 2);
    Ae << a, a;
    c.qp.Ae = Ae.sparseView();
    c.qp.be = hven::Vec::Constant(1, 0.1);
    if (row_bound > 0.0) {
        Eigen::MatrixXd Ai(1, 2);
        Ai << 1, 0;
        c.qp.Ai = Ai.sparseView();
        c.qp.bi = hven::Vec::Constant(1, row_bound);
    }
    return c;
}

// The premise, on the records: the first twelve passes are ordinary full steps whose residual
// contracts at `rate` -- slower than a halving per kStagnationSteps when rate > 0.933.
void expect_contracts_at(const Walk &w, double rate) {
    ASSERT_GE(w.records.size(), 12U);
    for (std::size_t k = 1; k < 12; ++k) {
        EXPECT_EQ(static_cast<int>(w.records[k - 1].action), static_cast<int>(Action::kMoveOnly))
            << k - 1;
        EXPECT_NEAR(w.records[k].active_res_inf / w.records[k - 1].active_res_inf, rate, 1e-6) << k;
    }
}

void expect_certified(const Walk &w) {
    const QpSolution &s = w.solution;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
    EXPECT_EQ(s.counters.stagnation_stops, 0);
    EXPECT_EQ(s.counters.stagnation_drops, 0);
    EXPECT_EQ(count_actions(w, Action::kStagnationStop), 0);
}

} // namespace

// rate 0.8: the residual halves within every run of kStagnationSteps, so no reading is taken.
TEST(QpStagnationStop, AResidualContractingAtPointEightCertifiesWithoutAReading) {
    const Contracting c = contracting_row(0.8, 0.0);
    const Walk w = observed(c.qp, c.o);
    print_walk("rate 0.8", w);
    expect_contracts_at(w, 0.8);
    expect_certified(w);
    EXPECT_EQ(w.solution.counters.terminal_mu_refines, 0);
    EXPECT_EQ(count_actions(w, Action::kStagnationRefine), 0);
}

// rate 0.95: a reading is taken with nothing to drop; the dual_mu footprint explains the
// residual, the mu-free re-solve is adopted, and the next pass certifies it.
TEST(QpStagnationStop, AResidualContractingAtPointNineFiveIsAnsweredByTheMuFreeResolve) {
    const Contracting c = contracting_row(0.95, 0.0);
    const Walk w = observed(c.qp, c.o);
    print_walk("rate 0.95", w);
    expect_contracts_at(w, 0.95);
    expect_certified(w);
    EXPECT_GE(w.solution.counters.terminal_mu_refine_rescues, 1);
    EXPECT_EQ(count_actions(w, Action::kStagnationRefine), 1);
    EXPECT_LT(w.solution.counters.minor_iters, 30) << "the re-solve, not ~200 contracting passes";
}

// rate 0.95 beside an inequality row x0 <= 49900 that the mu-free point (50000, 50000) violates:
// every reading before the walk reaches the row has its re-solve refused and nothing to drop, at a
// point off its row -- the walk steps on (it is still converging) instead of ending there.
TEST(QpStagnationStop, AContractingRunOffItsRowsIsNotCutOff) {
    const Contracting c = contracting_row(0.95, 49900.0);
    const Walk w = observed(c.qp, c.o);
    print_walk("rate 0.95 + row", w);
    expect_contracts_at(w, 0.95);
    expect_certified(w);
    const QpSolution &s = w.solution;
    EXPECT_GT(s.counters.terminal_mu_refines, s.counters.terminal_mu_refine_rescues)
        << "a re-solve was refused";
    EXPECT_GE(count_actions(w, Action::kAdd), 1) << "the walk reached the row";
    EXPECT_NEAR(s.x(0), 49900.0, 1e-6);
    EXPECT_NEAR(s.x(1), 50100.0, 1e-6);
}

// NOTHING TO DROP AT A POINT ON ITS ROWS (the classification-site stop). x1 is free and priced by
// g = (0, 1e-8) at primal_delta = 1: each full step moves it 1e-8 and lowers f by 1e-16, under the
// objective clause's 1e-12, and the free-block stationarity residual stays at 1e-8 > opt_tol. The
// row x0 = 0 is met from the start and carries no multiplier (no footprint to re-solve).
TEST(QpStagnationStop, AReadingWithNothingToDropOnItsRowsEndsTheWalk) {
    QpProblem qp = free_problem(2);
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, 0;
    qp.Ae = Ae.sparseView();
    qp.be = hven::Vec::Constant(1, 0.0);
    qp.g(1) = 1e-8;
    QpOptions o;
    o.max_iter = 400;
    o.primal_delta = 1.0;
    const Walk w = observed(qp, o);
    print_walk("idle on the row", w);
    const QpSolution &s = w.solution;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
    EXPECT_EQ(s.counters.stagnation_stops, 1);
    EXPECT_EQ(s.counters.stagnation_drops, 0);
    EXPECT_EQ(s.counters.minor_iters, 1 + detail::kStagnationSteps + 1);
    EXPECT_EQ(s.counters.terminal_mu_refines, 0);
    EXPECT_EQ(s.counters.certificate_row_refusals + s.counters.terminal_farkas_certificates, 0);
    ASSERT_FALSE(w.records.empty());
    EXPECT_EQ(static_cast<int>(w.records.back().action), static_cast<int>(Action::kStagnationStop));
}

// The same idle run on a row with a multiplier, x0 + x1 = 0, g = (1, 1 + 1e-8): the footprint is
// open, the mu-free re-solve is adopted with nothing to drop and the walk resumes from it ONCE;
// the next reading, idle again, ends the walk at its re-solved point.
TEST(QpStagnationStop, TheMuFreeResumeIsSpentOncePerSolve) {
    QpProblem qp = free_problem(2);
    Eigen::MatrixXd Ae(1, 2);
    Ae << 1, 1;
    qp.Ae = Ae.sparseView();
    qp.be = hven::Vec::Constant(1, 0.0);
    qp.g(0) = 1.0;
    qp.g(1) = 1.0 + 1e-8;
    QpOptions o;
    o.max_iter = 400;
    o.primal_delta = 1.0;
    const Walk w = observed(qp, o);
    print_walk("idle on a priced row", w);
    const QpSolution &s = w.solution;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
    EXPECT_EQ(s.counters.stagnation_stops, 1);
    EXPECT_EQ(s.counters.stagnation_drops, 0);
    EXPECT_EQ(s.counters.terminal_mu_refine_rescues, 2);
    EXPECT_EQ(count_actions(w, Action::kStagnationRefine), 1);
    EXPECT_LT(s.counters.minor_iters, 40);
    ASSERT_FALSE(w.records.empty());
    EXPECT_EQ(static_cast<int>(w.records.back().action), static_cast<int>(Action::kStagnationStop));
    EXPECT_LE(std::abs(s.x(0) + s.x(1)), 1e-12);
}

// RESIDUAL PROGRESS BELOW THE ROWS' FLOOR IS NOT PROGRESS. The idle run above, on a row
// a x0 = 1e-11 (a = 1e-6) that the start point misses by 1e-11, under its floor feas_tol: dual_mu
// is chosen so each pass cuts that residual by about 0.7, halving it every two passes, and motion
// under the floor is not progress, so the walk still ends on the reading's first pass.
TEST(QpStagnationStop, AResidualHalvingBelowTheFloorIsNotProgress) {
    QpProblem qp = free_problem(2);
    const double a = 1e-6;
    Eigen::MatrixXd Ae(1, 2);
    Ae << a, 0;
    qp.Ae = Ae.sparseView();
    qp.be = hven::Vec::Constant(1, 1e-11);
    qp.g(1) = 1e-8;
    QpOptions o;
    o.max_iter = 400;
    o.primal_delta = 1.0;
    const double rho = std::sqrt(0.5);
    o.dual_mu = a * a * rho / (1.0 - rho) / o.primal_delta;
    const Walk w = observed(qp, o);
    print_walk("halving below the floor", w);
    ASSERT_GE(w.records.size(), 4U);
    EXPECT_LT(w.records[0].active_res_inf, o.feas_tol);
    for (std::size_t k = 1; k < 4; ++k) {
        EXPECT_LT(w.records[k].active_res_inf, 0.75 * w.records[k - 1].active_res_inf) << k;
    }
    const QpSolution &s = w.solution;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
    EXPECT_EQ(s.counters.stagnation_stops, 1);
    EXPECT_EQ(s.counters.minor_iters, 1 + detail::kStagnationSteps + 1);
}

// A STUCK RUN OFF ITS ROWS ENDS THE WALK. One row a x0 = 1, a = 1e-7, x0 in [-1, 1] (infeasible:
// the row cannot be met inside the box), H = 0, g = 0, primal_delta = 1, dual_mu = 1e-2: each
// regularized step restores the row by a fraction 1e-12 of its residual, so the residual stands
// still, the mu-free re-solve's point (x0 = 1e7) is refused by the box, and the run's contraction
// could not bring the row to its floor in any number of minors: the walk ends on the reading's
// first pass instead of spending its cap.
TEST(QpStagnationStop, AStuckRunOffItsRowsEndsTheWalk) {
    QpProblem qp = free_problem(1);
    Eigen::MatrixXd Ae(1, 1);
    Ae << 1e-7;
    qp.Ae = Ae.sparseView();
    qp.be = hven::Vec::Constant(1, 1.0);
    qp.lower = hven::Vec::Constant(1, -1.0);
    qp.upper = hven::Vec::Constant(1, 1.0);
    QpOptions o;
    o.max_iter = 400;
    o.primal_delta = 1.0;
    o.dual_mu = 1e-2;
    const Walk w = observed(qp, o);
    print_walk("stuck off its rows", w);
    const QpSolution &s = w.solution;
    EXPECT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kMaxIter));
    EXPECT_EQ(s.counters.stagnation_stops, 1);
    EXPECT_EQ(s.counters.stagnation_drops, 0);
    EXPECT_EQ(s.counters.minor_iters, 1 + detail::kStagnationSteps + 1);
    EXPECT_GE(s.counters.terminal_mu_refines, 1);
    EXPECT_EQ(s.counters.terminal_mu_refine_rescues, 0) << "the box refuses the mu-free point";
    ASSERT_FALSE(w.records.empty());
    EXPECT_EQ(static_cast<int>(w.records.back().action), static_cast<int>(Action::kStagnationStop));
    EXPECT_GT(w.records.back().active_res_inf, 0.9) << "off its row";
}
