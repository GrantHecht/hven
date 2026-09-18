// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// tests/sqp/test_qp_cap_levers.cpp -- M7 W2 M1: a subproblem that ran out of its
// own minor budget, the counters that say so, and the two levers behind toggles.
//
// WHAT THE FIXTURE REPRODUCES. On the CUTEst TRAIN set, nine cells exit
// `numerical_error` through ONE gate: a subproblem hits the minor cap, the
// driver reports `QpStatus::kMaxIter` as `SolveStatus::kNumericalError`, and the
// SECOND consecutive such failure ends the solve -- the two fatal majors
// spending the IDENTICAL minor count, which is what says a budget and not the
// arithmetic stopped them (docs/notes/2026-09-m7-w2-sqp-cutest-read.md section
// M1). `BoxPullModel` below is that shape at n = 6, made cheap by an EXPLICIT
// `QpOptions::max_iter`: the option's own precedence rule (`qp_types.h`) makes a
// positive value an absolute cap, so a fixture can reach in two minors what
// ACOPP118 reaches in 4 710.
//
// THE PREMISE IS ASSERTED, NOT ASSUMED. Every capped case below first measures
// what the SAME subproblem costs UNCAPPED (`uncapped_first_major_minors`) and
// refuses to run if the cap it then sets is not actually binding -- so a future
// change that made this model solve in one minor fails here loudly instead of
// quietly testing nothing.
//
// Names carry a `QpCap` prefix: this suite's TUs share a link unit.

#include <algorithm>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/core/solver_counters.h>
#include <hven/drivers/solve_status.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/trace_writer.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {
namespace {

constexpr Index kN = 6;

/// @brief f(x) = 0.5 * sum_i w_i (x_i - t_i)^2 over a box every target sits
///        ABOVE, with one linear inequality row the walk must also price.
///
/// Strictly convex, so the NLP has one minimizer -- the box's upper corner,
/// trimmed by the row. Every variable wants to move up and out, so the
/// subproblem's working set has to GROW: the walk adds bounds one minor at a
/// time, which is what makes the minor count of one subproblem large enough for
/// a small explicit cap to bite. The weights are distinct so no two ratio tests
/// tie, and the row keeps the QP from being a pure box problem.
class BoxPullModel : public NlpModel {
  public:
    Index n() const override { return kN; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        double f = 0.0;
        for (Index i = 0; i < kN; ++i) {
            const double d = x(i) - target(i);
            f += 0.5 * weight(i) * d * d;
        }
        return f;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(kN);
        for (Index i = 0; i < kN; ++i) {
            g(i) = weight(i) * (x(i) - target(i));
        }
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    /// One row, in hven's own `cI(x) <= 0` convention (`model/nlp_model.h`):
    /// sum(x) <= 3.5, which BINDS at the box corner every variable is pulled
    /// toward (sum = 6 there), so the subproblem prices a row as well as its
    /// bounds. The start point satisfies it.
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x.sum() - 3.5;
        return c;
    }

    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(kN, kN);
        for (Index i = 0; i < kN; ++i) {
            h.insert(i, i) = obj_scale * weight(i);
        }
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, kN); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, kN);
        for (Index i = 0; i < kN; ++i) {
            j.insert(0, i) = 1.0;
        }
        j.makeCompressed();
        return j;
    }

    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(kN); }

  private:
    static double weight(Index i) { return 1.0 + 0.25 * static_cast<double>(i); }
    static double target(Index i) { return 4.0 + static_cast<double>(i); }

    Vec lower_ = Vec::Zero(kN);
    Vec upper_ = Vec::Ones(kN);
};

SqpOptions base_options() {
    SqpOptions o;
    o.common.print_level = 10; // silent
    return o;
}

Vec cold_start() { return Vec::Zero(kN); }

/// The minors the FIRST subproblem costs with no cap in the way -- the premise
/// every capped case below is built on, measured rather than assumed.
Index uncapped_first_major_minors() {
    BoxPullModel model;
    SqpSolver driver(base_options());
    const SqpResult out = driver.solve(model, cold_start());
    EXPECT_FALSE(out.history.empty());
    return out.history.empty() ? 0 : out.history.front().qp_minor_iters;
}

} // namespace

// ---------------------------------------------------------------------------
// The counters -- the label half. No toggle is touched in this block.
// ---------------------------------------------------------------------------

TEST(QpCapCounters, AHealthySolveCountsNoExitAndRoutesNothing) {
    BoxPullModel model;
    SqpSolver driver(base_options());
    const SqpResult out = driver.solve(model, cold_start());

    ASSERT_EQ(out.status, SolveStatus::kOptimal);
    EXPECT_EQ(out.counters.qp_maxiter_exits, 0);
    EXPECT_EQ(out.counters.qp_numerical_exits, 0);
    EXPECT_EQ(out.counters.qp_failures_routed, 0);
    EXPECT_EQ(out.counters.qp_failures_in_a_row_peak, 0);
    EXPECT_EQ(out.counters.qp_cap_retries, 0);
}

TEST(QpCapCounters, TheCappedExitIsCountedApartFromANumericalOne) {
    // THE NINE CELLS' ROW at unit scale, and the pin on what this engine does
    // TODAY: two majors, each spending the IDENTICAL capped minor count, then
    // `kNumericalError`. The two exit counters are what make the two
    // indistinguishable-by-status failures distinguishable at all.
    const Index uncapped = uncapped_first_major_minors();
    ASSERT_GT(uncapped, 1) << "the fixture no longer needs more than one minor; "
                              "the cap below would not bite";

    SqpOptions o = base_options();
    o.qp.max_iter = 1; // an EXPLICIT absolute cap; qp_types.h's precedence rule
    // AND AN EXPLICIT BUDGET OF 1, WHICH IS THE ENGINE BEFORE M7 W2 M1. The
    // SHIPPED budget is 3 (the TRAIN reading, sqp_solver_types.h); this test
    // pins what the field's OFF value reproduces, so it writes it out rather
    // than depending on the default, and `TheDefaultIsThreeAndOneIsTheOldGate`
    // below pins the default itself.
    o.qp_failure_budget = 1;
    BoxPullModel model;
    SqpSolver driver(o);
    const SqpResult out = driver.solve(model, cold_start());

    EXPECT_EQ(out.status, SolveStatus::kNumericalError);
    EXPECT_EQ(out.counters.major_iters, 2);
    // WHAT THE FAILURES WERE: both budget exhaustions, neither a broken
    // factorization. Before M1 these two were one number.
    EXPECT_EQ(out.counters.qp_maxiter_exits, 2);
    EXPECT_EQ(out.counters.qp_numerical_exits, 0);
    // WHAT THE DRIVER DID: routed the first, refused the second.
    EXPECT_EQ(out.counters.qp_failures_routed, 1);
    EXPECT_EQ(out.counters.qp_failures_in_a_row_peak, 1);
    EXPECT_EQ(out.counters.qp_cap_retries, 0);
    // `rejected_steps` keeps its meaning and still counts the routed failure,
    // which is what makes qp_failures_routed a SUBSET of it rather than a
    // replacement.
    EXPECT_EQ(out.counters.rejected_steps, 1);

    // THE IDENTICAL MINOR COUNT, which is the fact that says a BUDGET and not
    // the arithmetic stopped these two majors.
    ASSERT_EQ(out.history.size(), 2u);
    EXPECT_EQ(out.history[0].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(out.history[1].qp_status, QpStatus::kMaxIter);
    EXPECT_EQ(out.history[0].qp_minor_iters, out.history[1].qp_minor_iters);
    EXPECT_EQ(out.history[0].qp_minor_iters, 1);
}

// ---------------------------------------------------------------------------
// Lever 1 -- the consecutive-failure budget
// ---------------------------------------------------------------------------

TEST(QpCapBudget, TheDefaultIsThreeAndOneIsTheOldGate) {
    // THE SHIPPED DEFAULT IS 3, chosen on the CUTEst TRAIN 23 by the M7 rule
    // (docs/notes/data/2026-09-m7-w2-m1-qp-cap/), and this is the pin on the
    // literal as well as on what it does: a defaulted solve must be the solve
    // an explicit 3 gives, and an explicit 1 must be the engine before the field
    // existed -- two majors, one routed failure -- on the same capped fixture.
    EXPECT_EQ(SqpOptions{}.qp_failure_budget, 3);

    BoxPullModel model;

    SqpOptions d = base_options();
    d.qp.max_iter = 1;
    SqpSolver defaulted(d);
    const SqpResult a = defaulted.solve(model, cold_start());

    SqpOptions three = d;
    three.qp_failure_budget = 3;
    SqpSolver written(three);
    const SqpResult b = written.solve(model, cold_start());

    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_failures_routed, b.counters.qp_failures_routed);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.counters.qp_failures_routed, 3);

    SqpOptions one = d;
    one.qp_failure_budget = 1;
    SqpSolver old_gate(one);
    const SqpResult c = old_gate.solve(model, cold_start());

    EXPECT_EQ(c.status, SolveStatus::kNumericalError);
    EXPECT_EQ(c.counters.major_iters, 2);
    EXPECT_EQ(c.counters.qp_failures_routed, 1);
    EXPECT_EQ(c.counters.qp_failures_in_a_row_peak, 1);
}

TEST(QpCapBudget, ABudgetOfThreeRoutesThreeFailuresAndThenExits) {
    // THE SHIPPED DEFAULT'S OWN ROW, written out: this is what a capped
    // subproblem costs at 3, and the test above pins that the default gives it.
    SqpOptions o = base_options();
    o.qp.max_iter = 1;
    o.qp_failure_budget = 3;
    BoxPullModel model;
    SqpSolver driver(o);
    const SqpResult out = driver.solve(model, cold_start());

    // FOUR majors now: three routed failures, then the fourth is refused.
    EXPECT_EQ(out.status, SolveStatus::kNumericalError);
    EXPECT_EQ(out.counters.qp_failures_routed, 3);
    EXPECT_EQ(out.counters.qp_failures_in_a_row_peak, 3);
    EXPECT_EQ(out.counters.qp_maxiter_exits, 4);
    EXPECT_EQ(out.counters.qp_numerical_exits, 0);
    EXPECT_EQ(out.counters.major_iters, 4);
    EXPECT_EQ(out.counters.rejected_steps, 3);
    // The peak counts ADMITTED failures, so the one that ENDED the solve is not
    // in it: four exits, three routed, a peak of three.
    EXPECT_EQ(out.counters.qp_cap_retries, 0);
}

TEST(QpCapBudget, ARaisedBudgetChangesNothingOnASolveThatNeverFails) {
    BoxPullModel model;
    SqpSolver off(base_options());
    const SqpResult a = off.solve(model, cold_start());

    SqpOptions o = base_options();
    o.qp_failure_budget = 5;
    SqpSolver raised(o);
    const SqpResult b = raised.solve(model, cold_start());

    ASSERT_EQ(a.status, SolveStatus::kOptimal);
    EXPECT_EQ(b.status, SolveStatus::kOptimal);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.x, b.x);
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.counters.f, b.counters.f) << #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
}

// ---------------------------------------------------------------------------
// Lever 2 -- the cap retry
// ---------------------------------------------------------------------------

TEST(QpCapRetry, ADoubledCapThatSufficesRecoversTheSolve) {
    // THE CONSTRUCTION: cap the subproblem one minor SHORT of what it needs, so
    // the doubled cap (2 * (m - 1) >= m for every m >= 2) always suffices. The
    // premise is measured, not assumed.
    const Index uncapped = uncapped_first_major_minors();
    ASSERT_GE(uncapped, 2) << "the fixture cannot be capped one minor short";

    SqpOptions o = base_options();
    o.qp.max_iter = uncapped - 1;
    o.qp_cap_retry = true;
    BoxPullModel model;
    SqpSolver driver(o);
    const SqpResult out = driver.solve(model, cold_start());

    EXPECT_EQ(out.status, SolveStatus::kOptimal);
    // THE FIRED-COUNTER: the retry ran. The attempt it RESCUED is not in
    // `qp_maxiter_exits` -- that field counts the exits the driver ACTED on,
    // and the retry replaced this one as the major's answer before the routing
    // ran -- so the pair's invariant is what says a cap was reached at all:
    // `qp_maxiter_exits + qp_cap_retries` is the number of capped ATTEMPTS.
    EXPECT_GE(out.counters.qp_cap_retries, 1);
    EXPECT_GE(out.counters.qp_maxiter_exits + out.counters.qp_cap_retries, 1);
    // The retry's minors are REAL WORK and are folded in like any subproblem's:
    // the first major's own row records only the capped attempt, so the
    // aggregate must exceed the rows' sum by at least the retry's own minors.
    Index rows = 0;
    for (const SqpIterate &row : out.history) {
        rows += row.qp_minor_iters;
    }
    EXPECT_GT(out.counters.qp_minor_iters, rows);
    // Nothing was routed: the retry ran BEFORE the failure routing, so the
    // rescued major never became a failure at all.
    EXPECT_EQ(out.counters.qp_failures_routed, 0);
}

TEST(QpCapRetry, ADoubledCapThatStillDoesNotSufficeFailsAsTodayAndSaysSo) {
    const Index uncapped = uncapped_first_major_minors();
    ASSERT_GT(uncapped, 2) << "the fixture cannot be capped below half what it needs";

    SqpOptions o = base_options();
    o.qp.max_iter = 1; // 2 * 1 is still below what the subproblem needs
    o.qp_cap_retry = true;
    o.qp_failure_budget = 1; // lever 2 ALONE, against the pre-M1 routing gate
    BoxPullModel model;
    SqpSolver driver(o);
    const SqpResult out = driver.solve(model, cold_start());

    EXPECT_EQ(out.status, SolveStatus::kNumericalError);
    // TWO majors as today, each having spent its cap TWICE: two attempts and
    // two retries, of which only the RETRIES' exits reach the routing site --
    // so two counted exits, two retries, and four capped attempts by the pair's
    // invariant. The routing on top of them is unchanged.
    EXPECT_EQ(out.counters.major_iters, 2);
    EXPECT_EQ(out.counters.qp_cap_retries, 2);
    EXPECT_EQ(out.counters.qp_maxiter_exits, 2);
    EXPECT_EQ(out.counters.qp_numerical_exits, 0);
    EXPECT_EQ(out.counters.qp_failures_routed, 1);
    EXPECT_EQ(out.counters.qp_failures_in_a_row_peak, 1);
}

TEST(QpCapRetry, OffNeverRetriesAndIsTodaysSolveExactly) {
    BoxPullModel model;
    SqpOptions capped = base_options();
    capped.qp.max_iter = 1;

    SqpSolver off(capped);
    const SqpResult a = off.solve(model, cold_start());

    SqpOptions written = capped;
    written.qp_cap_retry = false; // the default, written out
    SqpSolver also_off(written);
    const SqpResult b = also_off.solve(model, cold_start());

    EXPECT_EQ(a.status, b.status);
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.counters.f, b.counters.f) << #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
    EXPECT_EQ(a.counters.qp_cap_retries, 0);
}

TEST(QpCapRetry, ItChangesNothingOnASolveThatNeverHitsTheCap) {
    BoxPullModel model;
    SqpSolver off(base_options());
    const SqpResult a = off.solve(model, cold_start());

    SqpOptions o = base_options();
    o.qp_cap_retry = true;
    SqpSolver on(o);
    const SqpResult b = on.solve(model, cold_start());

    ASSERT_EQ(a.status, SolveStatus::kOptimal);
    EXPECT_EQ(b.status, SolveStatus::kOptimal);
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.x, b.x);
#define HVEN_TEST_SAME(f, absent) EXPECT_EQ(a.counters.f, b.counters.f) << #f;
    HVEN_SQP_COUNTERS_FIELDS(HVEN_TEST_SAME)
#undef HVEN_TEST_SAME
}

TEST(QpCapRetry, EachRetryAnnouncesItselfOnTheTraceAtItsOwnSite) {
    // THE NON-VACUOUS READING OF THE `cap_retry` SITE. The site-scan suite
    // (tests/sqp/test_qp_mode_sites.cpp) asserts `cap_retry == qp_cap_retries`
    // on cells that all run at this toggle's default, where both are 0; this is
    // the same identity where the lever actually fires.
    const Index uncapped = uncapped_first_major_minors();
    ASSERT_GT(uncapped, 1) << "the fixture no longer needs more than one minor";

    SqpOptions o = base_options();
    o.qp.max_iter = 1;
    o.qp_cap_retry = true;
    BoxPullModel model;
    SqpSolver driver(o);
    std::ostringstream os;
    JsonLinesTraceSink sink(os);
    driver.attach_trace(&sink);
    const SqpResult out = driver.solve(model, cold_start());

    Index lines = 0;
    std::istringstream in(os.str());
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("\"ev\":\"qp.mode\"") != std::string::npos &&
            line.find("\"site\":\"cap_retry\"") != std::string::npos) {
            ++lines;
        }
    }
    EXPECT_GT(out.counters.qp_cap_retries, 0);
    EXPECT_EQ(lines, out.counters.qp_cap_retries);
}

// ---------------------------------------------------------------------------
// Validation and the dump
// ---------------------------------------------------------------------------

TEST(QpCapValidation, ABudgetBelowOneIsRefusedByNameAndByValue) {
    SqpOptions o = base_options();
    o.qp_failure_budget = 0;
    try {
        SqpSolver driver(o);
        FAIL() << "a zero failure budget was accepted";
    } catch (const std::invalid_argument &e) {
        const std::string what = e.what();
        EXPECT_NE(what.find("qp_failure_budget"), std::string::npos) << what;
        EXPECT_NE(what.find("(0)"), std::string::npos) << what;
    }
    o.qp_failure_budget = -3;
    EXPECT_THROW(SqpSolver{o}, std::invalid_argument);
}

TEST(QpCapValidation, ABudgetOfOneOrMoreIsAcceptedAndBothRetryStatesAre) {
    SqpOptions o = base_options();
    o.qp_failure_budget = 1;
    EXPECT_NO_THROW(SqpSolver{o});
    o.qp_failure_budget = 7;
    EXPECT_NO_THROW(SqpSolver{o});
    o.qp_cap_retry = true;
    EXPECT_NO_THROW(SqpSolver{o});
    o.qp_cap_retry = false;
    EXPECT_NO_THROW(SqpSolver{o});
}

TEST(QpCapValidation, TheDumpCarriesBothLinesInDeclarationPosition) {
    SqpOptions o = base_options();
    o.qp_failure_budget = 4;
    o.qp_cap_retry = true;
    std::ostringstream os;
    dump_options(o, os);
    const std::string text = os.str();
    EXPECT_NE(text.find("qp_failure_budget = 4\n"), std::string::npos) << text;
    EXPECT_NE(text.find("qp_cap_retry = true\n"), std::string::npos) << text;
    // DECLARATION POSITION: both sit after `project_start_point` and before the
    // `common.` block, which is what the drift pin's line count is counting.
    EXPECT_LT(text.find("project_start_point"), text.find("qp_failure_budget"));
    EXPECT_LT(text.find("qp_failure_budget"), text.find("qp_cap_retry"));
    EXPECT_LT(text.find("qp_cap_retry"), text.find("common.threads"));
}

} // namespace hven::solvers
