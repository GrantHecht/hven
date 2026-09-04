// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_activity_telemetry.cpp -- M6 W4 task 3: the mode-selection telemetry.
// `SqpIterate`'s six activity fields and the four `SqpCounters` folds, in two
// families.
//
//   (i)  ARITHMETIC, on `census_major_activity` directly, from hand-built
//        active-set pairs, where the two sets are known BY CONSTRUCTION rather
//        than predicted from a trajectory.
//
//   (ii) THE DRIVER, on fixtures whose QP active set at the answer is known:
//        two HS cells hand-counted, a bound-only QP-NLP reading zero, and two
//        one-variable QP-NLPs either side of `kWeakActivityMargin`.
//
// The stream side of the same fields is pinned in test_trace_writer.cpp (the
// re-derived `sqp.major` golden lines, the field-by-field row comparison, and
// the restoration sub-solve's own first row at depth 1).

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include <Eigen/SparseCore>
#include <gtest/gtest.h>

#include <hven/drivers/sqp_driver.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

#include "support/hs_problems.h"
#include "support/ssn_fixtures.h"

namespace hven::solvers {
namespace {

using ::hven::solvers::test_support::HsProblem;
using ::hven::solvers::test_support::make_hs;
using ::hven::solvers::test_support::weakly_active_qp;

constexpr double kInfinity = std::numeric_limits<double>::infinity();

// --- (i) the arithmetic, on hand-built pairs -------------------------------

/// A QP with `mi` rows on `n` variables, whose `Ai`/`bi` are the identity rows
/// `x_k <= b_k` so a chosen slack vector is producible exactly.
QpProblem row_box_qp(Index n, const Vec &bi) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(n, n).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(n);
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(bi.size(), n);
    for (Index k = 0; k < bi.size(); ++k) {
        Aid(k, k % n) = 1.0;
    }
    qp.Ai = Aid.sparseView();
    qp.bi = bi;
    qp.lower = Vec::Constant(n, -1e20);
    qp.upper = Vec::Constant(n, 1e20);
    return qp;
}

SqpIterate census(const QpProblem &qp, const QpSolution &qs,
                  const std::vector<bool> &prev_rows = {},
                  const std::vector<BoundState> &prev_bounds = {}) {
    SqpIterate row;
    Vec slack;
    census_major_activity(qp, qs, prev_rows, prev_bounds, slack, row);
    return row;
}

TEST(ActivityTelemetry, TheFirstMajorCountsAgainstTheEmptySet) {
    // Two active rows and two non-free variables, against no previous major:
    // four changes, and the census reads each half separately.
    const QpProblem qp = row_box_qp(3, (Vec(3) << 5.0, 5.0, 5.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(3);
    qs.lambda_i = (Vec(3) << 2.0, 0.0, 3.0).finished();
    qs.ineq_active = {true, false, true};
    qs.bound_state = {BoundState::kAtLower, BoundState::kFree, BoundState::kAtUpper};

    const SqpIterate row = census(qp, qs);
    EXPECT_EQ(row.active_set_delta, 4);
    EXPECT_EQ(row.active_rows, 2);
    EXPECT_EQ(row.active_lower_sides, 1);
    EXPECT_EQ(row.active_upper_sides, 1);
}

TEST(ActivityTelemetry, OneRowEntersAndOneBoundSideLeavesReadsTwo) {
    // THE BRIEF'S OWN CASE, and it is two because the two halves are counted
    // together: dropping either half of the rule reads 1.
    const QpProblem qp = row_box_qp(2, (Vec(2) << 5.0, 5.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(2);
    qs.lambda_i = (Vec(2) << 1.0, 1.0).finished();
    qs.ineq_active = {true, true};
    qs.bound_state = {BoundState::kFree, BoundState::kFree};

    const std::vector<bool> prev_rows = {true, false};
    const std::vector<BoundState> prev_bounds = {BoundState::kAtLower, BoundState::kFree};

    const SqpIterate row = census(qp, qs, prev_rows, prev_bounds);
    EXPECT_EQ(row.active_set_delta, 2);
    // The halves, so a failure says which one moved.
    EXPECT_EQ(row.active_rows, 2);
    EXPECT_EQ(row.active_lower_sides, 0);
    EXPECT_EQ(row.active_upper_sides, 0);
}

TEST(ActivityTelemetry, ASideThatSwapsLowerForUpperCountsOnce) {
    // The rule is a PER-VARIABLE state change, not a two-element set edit: a
    // variable that crossed its box counts one, not two.
    const QpProblem qp = row_box_qp(1, Vec(0));
    QpSolution qs;
    qs.x = Vec::Zero(1);
    qs.lambda_i = Vec(0);
    qs.ineq_active = {};
    qs.bound_state = {BoundState::kAtUpper};

    EXPECT_EQ(census(qp, qs, {}, {BoundState::kAtLower}).active_set_delta, 1);
    EXPECT_EQ(census(qp, qs, {}, {BoundState::kAtUpper}).active_set_delta, 0);
}

TEST(ActivityTelemetry, AFixedVariableIsBothSidesAndItsArrivalCountsOnce) {
    const QpProblem qp = row_box_qp(1, Vec(0));
    QpSolution qs;
    qs.x = Vec::Zero(1);
    qs.lambda_i = Vec(0);
    qs.ineq_active = {};
    qs.bound_state = {BoundState::kFixed};

    const SqpIterate row = census(qp, qs, {}, {BoundState::kAtLower});
    EXPECT_EQ(row.active_lower_sides, 1);
    EXPECT_EQ(row.active_upper_sides, 1);
    EXPECT_EQ(row.active_set_delta, 1);
}

TEST(ActivityTelemetry, AnUnchangedSetReadsZero) {
    const QpProblem qp = row_box_qp(2, (Vec(2) << 5.0, 5.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(2);
    qs.lambda_i = (Vec(2) << 1.0, 0.0).finished();
    qs.ineq_active = {true, false};
    qs.bound_state = {BoundState::kAtLower, BoundState::kFree};

    EXPECT_EQ(
        census(qp, qs, {true, false}, {BoundState::kAtLower, BoundState::kFree}).active_set_delta,
        0);
}

TEST(ActivityTelemetry, WeakActivityIsRelativeToTheLargestPriceAndInclusiveAtTheMargin) {
    // ||lambda||inf is 1000, so the gate is 1e-3. The three prices sit just
    // below, exactly on, and just above it.
    const QpProblem qp = row_box_qp(4, (Vec(4) << 9.0, 9.0, 9.0, 9.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(4);
    qs.lambda_i = (Vec(4) << 1000.0, 1e-4, 1e-3, 2e-3).finished();
    qs.ineq_active = {true, true, true, true};
    qs.bound_state = std::vector<BoundState>(4, BoundState::kFree);

    // 1000 is its own scale and never weak; 1e-4 and 1e-3 are; 2e-3 is not.
    EXPECT_EQ(census(qp, qs).weak_active_rows, 2);

    // AN INACTIVE ROW IS NEVER WEAK, whatever its price: the reading is about
    // the working set, and the other half of the pair is `near_active_rows`.
    qs.ineq_active = {true, false, false, true};
    EXPECT_EQ(census(qp, qs).weak_active_rows, 0);
}

TEST(ActivityTelemetry, NearActivityReadsTheNonnegativeSlackAndItsOwnScale) {
    // s = bi - Ai p with p = 0, so the slacks ARE bi: 4, 4e-6, 4e-5 and a
    // violated -1. ||s||inf is 4, so the gate is 4e-6.
    const QpProblem qp = row_box_qp(4, (Vec(4) << 4.0, 4e-6, 4e-5, -1.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(4);
    qs.lambda_i = Vec::Zero(4);
    qs.ineq_active = {false, false, false, false};
    qs.bound_state = std::vector<BoundState>(4, BoundState::kFree);

    // 4e-6 is on the gate and counts; 4e-5 is above it and does not; the
    // violated row is at-or-past its boundary and counts.
    EXPECT_EQ(census(qp, qs).near_active_rows, 2);

    // AN ACTIVE ROW IS NEVER NEAR: it is in the set, not approaching it.
    qs.ineq_active = {false, true, false, true};
    EXPECT_EQ(census(qp, qs).near_active_rows, 0);
}

TEST(ActivityTelemetry, AMismatchedExportContributesZeroToItsOwnHalfOnly) {
    // The bound half is well-formed and the row half is not: the census reports
    // the half it can read and 0 for the other, rather than indexing past an
    // end or refusing both.
    const QpProblem qp = row_box_qp(2, (Vec(2) << 5.0, 5.0).finished());
    QpSolution qs;
    qs.x = Vec::Zero(2);
    qs.lambda_i = Vec::Zero(2);
    qs.ineq_active = {true}; // size 1 against mi == 2
    qs.bound_state = {BoundState::kAtUpper, BoundState::kAtUpper};

    const SqpIterate row = census(qp, qs);
    EXPECT_EQ(row.active_rows, 0);
    EXPECT_EQ(row.near_active_rows, 0);
    EXPECT_EQ(row.weak_active_rows, 0);
    EXPECT_EQ(row.active_upper_sides, 2);
    EXPECT_EQ(row.active_set_delta, 2);
}

// --- (ii) the driver -------------------------------------------------------

/// @brief A `QpProblem` presented as an `NlpModel`, so a QP fixture's OWN
///        active set is what the driver's first subproblem reports.
///
/// EXACT BY CONSTRUCTION: f is the QP's objective and the rows are affine, so
/// the subproblem the driver builds at x is this QP shifted to p = x* - x. One
/// major solves it, and its `ineq_active`/`bound_state` are the QP's own.
class QpAsNlpModel final : public NlpModel {
  public:
    QpAsNlpModel(QpProblem qp, Vec x0) : qp_(std::move(qp)), x0_(std::move(x0)) {
        // The eval boundary refuses an uncompressed matrix return by name
        // (nlp_adapter.h), and `resize` alone leaves one.
        qp_.H.makeCompressed();
        qp_.Ae.makeCompressed();
        qp_.Ai.makeCompressed();
    }

    Index n() const override { return qp_.n(); }
    Index me() const override { return qp_.me(); }
    Index mi() const override { return qp_.mi(); }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.dot(qp_.H.selfadjointView<Eigen::Upper>() * x) + qp_.g.dot(x);
    }
    Vec eval_grad(const Vec &x) const override {
        return Vec(qp_.H.selfadjointView<Eigen::Upper>() * x) + qp_.g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec(qp_.Ae * x) - qp_.be; }
    Vec eval_ci(const Vec &x) const override { return Vec(qp_.Ai * x) - qp_.bi; }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return SpMatRM(qp_.H * obj_scale);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return qp_.Ae;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return qp_.Ai;
    }
    const Vec &lower() const override { return qp_.lower; }
    const Vec &upper() const override { return qp_.upper; }
    Vec start_point() const override { return x0_; }

  private:
    QpProblem qp_;
    Vec x0_;
};

/// The rows a solve REPORTS on -- every row but the ones whose QpSolution never
/// became a major's answer.
std::vector<SqpIterate> reporting_rows(const SqpSolution &sol) {
    std::vector<SqpIterate> out;
    for (const SqpIterate &r : sol.history) {
        if (r.qp_solved) {
            out.push_back(r);
        }
    }
    return out;
}

/// A one-variable QP-NLP: min 1/2 x^2 - a x subject to two rows `x <= b_k`,
/// started at x0. With a > 0 the tighter row is active at a price of exactly a.
QpProblem one_var_qp(double a, double b0, double b1) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(1, 1).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = (Vec(1) << -a).finished();
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 1);
    Aid << 1.0, 1.0;
    qp.Ai = Aid.sparseView();
    qp.bi = (Vec(2) << b0, b1).finished();
    qp.lower = Vec::Constant(1, -kInfinity);
    qp.upper = Vec::Constant(1, kInfinity);
    return qp;
}

SqpOptions telemetry_opts() {
    SqpOptions opts;
    // WIDE ENOUGH THAT NO STEP BELOW IS TRUST-REGION LIMITED, so each fixture's
    // reported active set is the one its own algebra determines.
    opts.tr_init = 4.0;
    return opts;
}

TEST(ActivityTelemetry, NearActiveRowsIsNonVacuousOnAControlledSlack) {
    // Rows x <= 1 and x <= eps, minimizer x* = 0, started at -0.5. One major
    // steps to 0 with both rows inactive at slacks 1 and eps, and ||s||inf is
    // 1, so the gate is exactly 1e-6.
    for (const auto &cell :
         {std::pair<double, Index>{1e-9, 1}, std::pair<double, Index>{1e-3, 0}}) {
        QpAsNlpModel model(one_var_qp(0.0, 1.0, cell.first), (Vec(1) << -0.5).finished());
        SqpDriver driver(telemetry_opts());
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        const std::vector<SqpIterate> rows = reporting_rows(sol);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(rows.front().near_active_rows, cell.second) << "slack " << cell.first;
        EXPECT_EQ(rows.front().active_rows, 0) << "slack " << cell.first;
        EXPECT_EQ(sol.counters.near_active_peak, cell.second) << "slack " << cell.first;
    }
}

TEST(ActivityTelemetry, WeakActiveRowsIsNonVacuousOnAControlledPrice) {
    // min 1/2 x^2 - a x with x <= 0 active at a price of exactly a, from -0.5.
    // ||lambda||inf is a <= 1, so the gate is 1e-6 either way and the MARGIN
    // alone decides -- the second cell falsifies the first.
    for (const auto &cell :
         {std::pair<double, Index>{1e-9, 1}, std::pair<double, Index>{1e-3, 0}}) {
        QpAsNlpModel model(one_var_qp(cell.first, 0.0, 10.0), (Vec(1) << -0.5).finished());
        SqpDriver driver(telemetry_opts());
        const SqpSolution sol = driver.solve(model);
        ASSERT_EQ(sol.status, SqpStatus::kOptimal);
        const std::vector<SqpIterate> rows = reporting_rows(sol);
        ASSERT_FALSE(rows.empty());
        EXPECT_EQ(rows.front().active_rows, 1) << "price " << cell.first;
        EXPECT_EQ(rows.front().weak_active_rows, cell.second) << "price " << cell.first;
        EXPECT_EQ(sol.counters.weak_active_peak, cell.second) << "price " << cell.first;
    }
}

TEST(ActivityTelemetry, EveryFieldIsZeroOnABoundOnlyQpNlpWithNoActiveBound) {
    // min 1/2||x - c||^2 on a box c sits strictly inside, no rows at all: the
    // solve has nothing to report and must say so with zeros rather than with a
    // number carried over from somewhere.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = (Vec(2) << -0.5, 0.25).finished(); // minimizer (0.5, -0.25)
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);

    QpAsNlpModel model(qp, Vec::Zero(2));
    SqpDriver driver(telemetry_opts());
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    ASSERT_FALSE(reporting_rows(sol).empty());
    for (const SqpIterate &r : sol.history) {
        EXPECT_EQ(r.active_set_delta, 0);
        EXPECT_EQ(r.weak_active_rows, 0);
        EXPECT_EQ(r.near_active_rows, 0);
        EXPECT_EQ(r.active_rows, 0);
        EXPECT_EQ(r.active_lower_sides, 0);
        EXPECT_EQ(r.active_upper_sides, 0);
    }
    EXPECT_EQ(sol.counters.active_set_delta_total, 0);
    EXPECT_EQ(sol.counters.active_set_delta_peak, 0);
    EXPECT_EQ(sol.counters.weak_active_peak, 0);
    EXPECT_EQ(sol.counters.near_active_peak, 0);
}

TEST(ActivityTelemetry, TheCensusEqualsAHandCountOnHs35sActiveRow) {
    // HS35 is a strictly convex QP with one linear row and x >= 0. Its solution
    // (4/3, 7/9, 4/9) is strictly inside the box and satisfies the row with
    // equality, so the hand count is one active row and no active bound side.
    const HsProblem p = make_hs(35);
    SqpDriver driver(telemetry_opts());
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const std::vector<SqpIterate> rows = reporting_rows(sol);
    ASSERT_FALSE(rows.empty());
    const SqpIterate &last = rows.back();
    EXPECT_EQ(last.active_rows, 1);
    EXPECT_EQ(last.active_lower_sides, 0);
    EXPECT_EQ(last.active_upper_sides, 0);
    // THE FIRST-MAJOR IDENTITY, on a fixture with no `kFixed` variable: the
    // count against the empty set is exactly the census.
    EXPECT_EQ(rows.front().active_set_delta, rows.front().active_rows +
                                                 rows.front().active_lower_sides +
                                                 rows.front().active_upper_sides);
}

TEST(ActivityTelemetry, TheCensusEqualsAHandCountOnHs45sVertex) {
    // HS45 has no general constraints and a solution at a pure vertex -- all
    // five variables at their UPPER bounds -- so the hand count is five upper
    // sides, no lower side and no row.
    const HsProblem p = make_hs(45);
    SqpOptions opts = telemetry_opts();
    opts.max_iter = 200;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const std::vector<SqpIterate> rows = reporting_rows(sol);
    ASSERT_FALSE(rows.empty());
    const SqpIterate &last = rows.back();
    EXPECT_EQ(last.active_rows, 0);
    EXPECT_EQ(last.active_lower_sides, 0);
    EXPECT_EQ(last.active_upper_sides, 5);
}

TEST(ActivityTelemetry, ARowWithNoAnswerOfItsOwnReportsZeroAndCarriesTheSetForward) {
    // The stopped-AT-iterate row is the shipped instance of "no QpSolution
    // became this major's answer": it must read zero in all six fields even
    // though the major before it did not.
    const HsProblem p = make_hs(35);
    SqpDriver driver(telemetry_opts());
    const SqpSolution sol = driver.solve(*p.model);
    ASSERT_FALSE(sol.history.empty());
    const SqpIterate &stopped = sol.history.back();
    ASSERT_FALSE(stopped.qp_solved);
    EXPECT_EQ(stopped.active_set_delta, 0);
    EXPECT_EQ(stopped.weak_active_rows, 0);
    EXPECT_EQ(stopped.near_active_rows, 0);
    EXPECT_EQ(stopped.active_rows, 0);
    EXPECT_EQ(stopped.active_lower_sides, 0);
    EXPECT_EQ(stopped.active_upper_sides, 0);
}

TEST(ActivityTelemetry, TheFourFoldsAreTheRowsOwnSumAndPeaks) {
    // The fold rule, read off the history the same solve returned: three peaks
    // by max and one sum, over the REPORTING rows and no others.
    for (const int number : {24, 35, 45, 76}) {
        const HsProblem p = make_hs(number);
        SqpOptions opts = telemetry_opts();
        opts.max_iter = 200;
        SqpDriver driver(opts);
        const SqpSolution sol = driver.solve(*p.model);
        Index total = 0;
        Index delta_peak = 0;
        Index weak_peak = 0;
        Index near_peak = 0;
        for (const SqpIterate &r : sol.history) {
            total += r.active_set_delta;
            delta_peak = std::max(delta_peak, r.active_set_delta);
            weak_peak = std::max(weak_peak, r.weak_active_rows);
            near_peak = std::max(near_peak, r.near_active_rows);
        }
        EXPECT_EQ(sol.counters.active_set_delta_total, total) << "HS" << number;
        EXPECT_EQ(sol.counters.active_set_delta_peak, delta_peak) << "HS" << number;
        EXPECT_EQ(sol.counters.weak_active_peak, weak_peak) << "HS" << number;
        EXPECT_EQ(sol.counters.near_active_peak, near_peak) << "HS" << number;
        // NON-VACUOUS: at least one of these cells has real churn to report.
        if (number == 24) {
            EXPECT_GT(sol.counters.active_set_delta_total, 0);
        }
    }
}

TEST(ActivityTelemetry, TheTieFixtureIsFlaggedByBothReadingsForDifferentReasons) {
    // THE M6 REGISTER'S TIE (test_ssn_engine.cpp's
    // WeaklyActiveRowFinishesUncertain) through the DRIVER at kSsn.
    //
    // Row 1 sits at s == 0 with lambda == 0, so both readings fire, for the
    // different reasons `kWeakActivityMargin` states.
    //
    // THE DISJUNCTION IS THE PIN, not a weakening. The tie's reading is a COIN
    // -- the register's L-1 flake: the row comes back active on one resolution
    // and inactive on the other, within one backend.
    //
    // So `weak >= 1` alone is a pin a rerun of identical source can break.
    // `weak + near >= 1` holds whichever way it lands: the two halves partition
    // on `ineq_active`.
    QpAsNlpModel model(weakly_active_qp(), Vec::Zero(2));
    SqpOptions opts = telemetry_opts();
    opts.qp_mode = QpMode::kSsn;
    SqpDriver driver(opts);
    const SqpSolution sol = driver.solve(model);
    ASSERT_EQ(sol.status, SqpStatus::kOptimal);
    const std::vector<SqpIterate> rows = reporting_rows(sol);
    ASSERT_FALSE(rows.empty());

    Index flagged = 0;
    for (const SqpIterate &r : rows) {
        flagged = std::max(flagged, r.weak_active_rows + r.near_active_rows);
    }
    EXPECT_GE(flagged, 1) << "the magnitude margin saw no tie";
    EXPECT_GE(sol.counters.ssn.ssn_uncertain_peak, 1) << "the kink band saw no tie";
    // The claim stops there. Which of the two readings fires is the coin, and
    // nothing here asserts a direction.
    RecordProperty("weak_active_peak", static_cast<int>(sol.counters.weak_active_peak));
    RecordProperty("near_active_peak", static_cast<int>(sol.counters.near_active_peak));
}

} // namespace
} // namespace hven::solvers
