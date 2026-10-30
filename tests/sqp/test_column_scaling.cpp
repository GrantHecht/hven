// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_column_scaling.cpp — the M7 W2 SCL column (variable) layer of problem
// scaling (detail/drivers/column_scaling.h, SqpOptions::column_scaling) and the
// caller-unit feasibility guard it brought into the convergence test.
//
//   (a) THE RULE: Ruiz's column half, power-of-two factors, the clamp, the gate,
//       the no-data and non-finite cases, the size refusal.
//   (b) THE SCALED MODEL: every evaluation is the caller's reparametrized, and its
//       refusals.
//   (c) THE OPTION: the shipped default is kOff, validation, the dump lines.
//   (d) THE DRIVER: a badly column-scaled fit solves under kAuto and kAlways and
//       reports in the caller's units against an independent measurement (an
//       inequality row too); below the gate kAuto is the kOff solve plus the rule's
//       one counted probe; the scaled solve is the kOff solve of the scaled model,
//       its z mapped on every status (restoration's own price included); the rule
//       reads the point the solve stands on; the trace, the result and the ledger
//       agree; iteration events at depth 0 and 1 carry the caller's x and z; the
//       warm start crosses both ways, natively and as a payload, without a hot
//       factorization.
//   (e) THE GUARD: a row the row layer shrank, and a box violation a column factor
//       hid, are each refused as a convergence and the solve goes on to a point
//       that is feasible in the caller's units.

#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include <hven/core/ledger.h>
#include <hven/detail/drivers/column_scaling.h>
#include <hven/detail/drivers/problem_scaling.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/trace.h>

#include "support/hs_problems.h"

namespace hven::solvers {
namespace {

using detail::ColumnScaledModel;
using detail::ColumnScaling;
using detail::ColumnScalingRule;
using detail::compute_column_scaling;

SpMatRM dense_to_sparse(Index rows, Index cols, const std::vector<double> &v) {
    SpMatRM m(rows, cols);
    for (Index i = 0; i < rows; ++i) {
        for (Index j = 0; j < cols; ++j) {
            const double a = v[static_cast<std::size_t>(i * cols + j)];
            if (a != 0.0) {
                m.insert(i, j) = a;
            }
        }
    }
    m.makeCompressed();
    return m;
}

// The column layer off: the shipped default, spelled out so the comparisons below do
// not depend on it.
SqpOptions off_options() {
    SqpOptions o;
    o.column_scaling = SqpColumnScaling::kOff;
    return o;
}

// kAuto's gate for the driver fixtures: below their spreads (2^16 for the fit), which
// the shipped 2^26 is not -- these tests exercise the mechanism, not the default.
constexpr double kFixtureGate = 1e3;

bool is_power_of_two(double v) {
    int e = 0;
    return std::frexp(v, &e) == 0.5;
}

// ===========================================================================
// THE FIT MODEL: SAWPATH's shape in miniature. A quadratic p(t) = a0 + a1 t + a2 t^2
// is pinned to three values p_j at t = 0, 100, 200 by three LINEAR equalities whose
// Vandermonde columns span four orders (1 .. 4e4), and the p_j are fitted to y in
// least squares. Three points and three coefficients: the optimum is unique,
// p = y (f* = 0) and a the interpolating coefficients, (1, 0.0225, -1.25e-4).
// Variables: (a0, a1, a2, p0, p1, p2).
// ===========================================================================
class PolyFitModel : public NlpModel {
  public:
    PolyFitModel() = default;
    /// With p1 capped at @p p1_cap below y1 = 2 the cap is active at the optimum and
    /// carries a nonzero bound price.
    explicit PolyFitModel(double p1_cap) { upper_(4) = p1_cap; }
    Index n() const override { return 6; }
    Index me() const override { return 3; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        double f = 0.0;
        for (int j = 0; j < 3; ++j) {
            f += 0.5 * (x(3 + j) - y_[j]) * (x(3 + j) - y_[j]);
        }
        return f;
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g = Vec::Zero(6);
        for (int j = 0; j < 3; ++j) {
            g(3 + j) = x(3 + j) - y_[j];
        }
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(3);
        for (int j = 0; j < 3; ++j) {
            c(j) = x(3 + j) - (x(0) + x(1) * t_[j] + x(2) * t_[j] * t_[j]);
        }
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(6, 6);
        for (int j = 0; j < 3; ++j) {
            h.insert(3 + j, 3 + j) = obj_scale;
        }
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM jac(3, 6);
        for (int j = 0; j < 3; ++j) {
            jac.insert(j, 0) = -1.0;
            jac.insert(j, 1) = -t_[j];
            jac.insert(j, 2) = -t_[j] * t_[j];
            jac.insert(j, 3 + j) = 1.0;
        }
        jac.makeCompressed();
        return jac;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 6); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(6); }

    Vec x_star() const {
        Vec x(6);
        x << 1.0, 0.0225, -1.25e-4, y_[0], y_[1], y_[2];
        return x;
    }

  private:
    double t_[3] = {0.0, 100.0, 200.0};
    double y_[3] = {1.0, 2.0, 0.5};
    Vec lower_ = Vec::Constant(6, -1e3);
    Vec upper_ = Vec::Constant(6, 1e3);
};

// ===========================================================================
// THE STEEP ROW: one equality 1e8 (x^2 - 1) = 0 from x0 = 3. The row layer shrinks
// it by 100 / (6e8) at the start, so its scaled residual passes feas_tol at a caller
// |c| 6e6 times larger: Newton reaches a point whose scaled residual is inside the
// tolerance and whose caller residual is not, and the guard must refuse it.
// f = 0 keeps stationarity trivially met, so the row is the only thing judged.
// ===========================================================================
class SteepRowModel : public NlpModel {
  public:
    Index n() const override { return 1; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &) const override { return 0.0; }
    Vec eval_grad(const Vec &) const override { return Vec::Zero(1); }
    Vec eval_ce(const Vec &x) const override {
        return Vec::Constant(1, 1.0e8 * (x(0) * x(0) - 1.0));
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(1, 1);
        h.insert(0, 0) = lambda_e.size() > 0 ? 2.0e8 * lambda_e(0) : 0.0;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 1);
        j.insert(0, 0) = 2.0e8 * x(0);
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 1); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(1, 3.0); }

  private:
    Vec lower_ = Vec::Constant(1, 0.5);
    Vec upper_ = Vec::Constant(1, 10.0);
};

// ===========================================================================
// THE HIDDEN BOX VIOLATION: min (x1 - 2)^2 / 2 s.t. x0 - 1e-4 x1 = 0, x1 <= 1. The
// row's 1e-4 column gives x1 the column factor 2^13, so a start 1.5e-5 above x1's
// bound (taken verbatim: project_start_point off) reads 1.9e-9 in the engine's units
// -- inside feas_tol, with the bound active and its price correctly signed, so the
// scaled test passes at the start. In the caller's units the point is infeasible.
// ===========================================================================
class HiddenBoxModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 0.5 * (x(1) - 2.0) * (x(1) - 2.0); }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 0.0, x(1) - 2.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0) - 1.0e-4 * x(1)); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0;
        j.insert(0, 1) = -1.0e-4;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override {
        Vec x(2);
        x(1) = 1.0 + std::ldexp(1.0, -16);
        x(0) = 1.0e-4 * x(1);
        return x;
    }

  private:
    Vec lower_ = (Vec(2) << -10.0, -10.0).finished();
    Vec upper_ = (Vec(2) << 10.0, 1.0).finished();
};

// ===========================================================================
// (a) THE RULE
// ===========================================================================

TEST(ColumnScalingRule, FactorsArePowersOfTwoAndEquilibrateTheColumns) {
    // The fit model's Jacobian: Ruiz's column half is (1, ~0.53, 0.005, 1, ~52, ~198),
    // which rounds to (1, 1/2, 1/256, 1, 64, 256).
    PolyFitModel m;
    const SpMatRM Je = m.eval_jac_e(m.start_point());
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(6, Je, SpMatRM(0, 6), rule);
    ASSERT_TRUE(cs.active);
    ASSERT_EQ(6, cs.cols.size());
    const double expected[6] = {1.0, 0.5, 1.0 / 256.0, 1.0, 64.0, 256.0};
    for (Index j = 0; j < 6; ++j) {
        EXPECT_TRUE(is_power_of_two(cs.cols(j))) << j;
        EXPECT_DOUBLE_EQ(expected[j], cs.cols(j)) << j;
    }
    EXPECT_DOUBLE_EQ(1.0 / 256.0, cs.col_min);
    EXPECT_DOUBLE_EQ(256.0, cs.col_max);
    EXPECT_DOUBLE_EQ(65536.0, cs.spread);
}

TEST(ColumnScalingRule, TheGateLeavesAWellEnoughScaledProblemAloneButReportsTheSpread) {
    PolyFitModel m;
    const SpMatRM Je = m.eval_jac_e(m.start_point());
    ColumnScalingRule rule;
    rule.threshold = 1e5; // above the model's 65536
    const ColumnScaling cs = compute_column_scaling(6, Je, SpMatRM(0, 6), rule);
    EXPECT_FALSE(cs.active);
    EXPECT_EQ(0, cs.cols.size());
    EXPECT_DOUBLE_EQ(65536.0, cs.spread) << "reported whether or not the gate fired";
    rule.threshold = 65536.0; // the gate is >=
    EXPECT_TRUE(compute_column_scaling(6, Je, SpMatRM(0, 6), rule).active);
}

TEST(ColumnScalingRule, AnEquilibratedJacobianIsTheIdentityAtAnyThreshold) {
    // Every entry +-1: every row and column norm is already 1, so Ruiz stops on its
    // first reading and no factor moves -- not even kAlways' threshold 1 installs.
    const SpMatRM J = dense_to_sparse(2, 3, {1, -1, 0, 0, 1, 1});
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(3, J, SpMatRM(0, 3), rule);
    EXPECT_FALSE(cs.active);
    EXPECT_DOUBLE_EQ(1.0, cs.spread);
}

TEST(ColumnScalingRule, AColumnWithNoDataKeepsTheFactorOneAndLeavesTheSpread) {
    // Column 1 is empty and column 2 holds only a NaN and a zero: neither carries a
    // unit. Column 0's 1e-6 entries are the only data.
    SpMatRM J(2, 3);
    J.insert(0, 0) = 1e-6;
    J.insert(0, 2) = std::numeric_limits<double>::quiet_NaN();
    J.insert(1, 2) = 0.0;
    J.insert(1, 0) = 1e-6;
    J.makeCompressed();
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(3, J, SpMatRM(0, 3), rule);
    // One data column: its factor moved, so kAlways' threshold installs it, and the
    // two columns without data keep exactly 1.
    ASSERT_TRUE(cs.active);
    EXPECT_GT(cs.cols(0), 1.0) << "the 1e-6 column, scaled up";
    EXPECT_TRUE(is_power_of_two(cs.cols(0)));
    EXPECT_DOUBLE_EQ(1.0, cs.cols(1));
    EXPECT_DOUBLE_EQ(1.0, cs.cols(2));
    // The spread is over the data columns only: one column, spread 1, so any gate
    // above 1 leaves the problem alone.
    EXPECT_DOUBLE_EQ(1.0, cs.spread);
    EXPECT_DOUBLE_EQ(cs.col_min, cs.col_max);
    rule.threshold = 2.0;
    EXPECT_FALSE(compute_column_scaling(3, J, SpMatRM(0, 3), rule).active);
}

TEST(ColumnScalingRule, TheBothBlocksAreRead) {
    // The equality block alone is equilibrated; the inequality block's 1e4 entry is
    // what makes column 1 large. A rule that read Je only would install nothing.
    const SpMatRM Je = dense_to_sparse(1, 2, {1, 1});
    const SpMatRM Ji = dense_to_sparse(1, 2, {0, 1e4});
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(2, Je, Ji, rule);
    ASSERT_TRUE(cs.active);
    EXPECT_LT(cs.cols(1), cs.cols(0));
}

TEST(ColumnScalingRule, TheClampKeepsAPowerOfTwoInsideTheLimit) {
    const SpMatRM J = dense_to_sparse(1, 2, {1.0, 1e-30});
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    rule.factor_limit = 1e3; // floor(log2 1e3) = 9
    const ColumnScaling cs = compute_column_scaling(2, J, SpMatRM(0, 2), rule);
    ASSERT_TRUE(cs.active);
    EXPECT_DOUBLE_EQ(512.0, cs.cols(1)) << "2^9, the largest power of two <= 1e3";
    EXPECT_TRUE(is_power_of_two(cs.cols(0)));
}

TEST(ColumnScalingRule, AJacobianOfTheWrongWidthIsRefused) {
    const SpMatRM J = dense_to_sparse(1, 2, {1.0, 2.0});
    EXPECT_THROW(compute_column_scaling(3, J, SpMatRM(0, 3), ColumnScalingRule{}),
                 std::invalid_argument);
    EXPECT_THROW(compute_column_scaling(2, SpMatRM(0, 2), dense_to_sparse(1, 3, {1, 1, 1}),
                                        ColumnScalingRule{}),
                 std::invalid_argument);
    // A 0-row block must still have n columns.
    EXPECT_THROW(compute_column_scaling(2, dense_to_sparse(1, 2, {1, 1}), SpMatRM(0, 3),
                                        ColumnScalingRule{}),
                 std::invalid_argument);
}

// ===========================================================================
// (b) THE SCALED MODEL
// ===========================================================================

TEST(ColumnScaledModel, EveryEvaluationIsTheCallersReparametrized) {
    test_support::Hs14Model base;
    Vec d(2);
    d << 0.5, 4.0;
    const ColumnScaledModel m(base, d);
    const Vec x = base.start_point();
    const Vec xs = m.to_scaled(x);
    ASSERT_TRUE(m.to_caller(xs).isApprox(x, 0.0)) << "exact: the factors are powers of two";

    EXPECT_EQ(base.eval_f(x), m.eval_f(xs));
    Vec g, gs;
    base.eval_grad_in_place(x, g);
    m.eval_grad_in_place(xs, gs);
    EXPECT_TRUE(gs.isApprox(Vec(g.cwiseProduct(d)), 0.0));
    Vec ce, ces, ci, cis;
    base.eval_ce_in_place(x, ce);
    m.eval_ce_in_place(xs, ces);
    base.eval_ci_in_place(x, ci);
    m.eval_ci_in_place(xs, cis);
    EXPECT_TRUE(ces.isApprox(ce, 0.0));
    EXPECT_TRUE(cis.isApprox(ci, 0.0));

    SpMatRM je, jes;
    base.eval_jac_e_in_place(x, je);
    m.eval_jac_e_in_place(xs, jes);
    EXPECT_TRUE(Eigen::MatrixXd(jes).isApprox(Eigen::MatrixXd(je) * d.asDiagonal(), 0.0));
    SpMatRM ji, jis;
    base.eval_jac_i_in_place(x, ji);
    m.eval_jac_i_in_place(xs, jis);
    EXPECT_TRUE(Eigen::MatrixXd(jis).isApprox(Eigen::MatrixXd(ji) * d.asDiagonal(), 0.0));

    const Vec le = Vec::Constant(base.me(), 0.5), li = Vec::Constant(base.mi(), 2.0);
    SpMatRM h, hs;
    base.eval_hess_in_place(x, 0.75, le, li, h);
    m.eval_hess_in_place(xs, 0.75, le, li, hs);
    EXPECT_TRUE(Eigen::MatrixXd(hs).isApprox(
        Eigen::MatrixXd(d.asDiagonal() * Eigen::MatrixXd(h) * d.asDiagonal()), 0.0));
    EXPECT_EQ(h.nonZeros(), hs.nonZeros()) << "the pattern is the model's own";

    double f = 0.0;
    Vec vce, vci;
    m.eval_values(xs, f, vce, vci);
    EXPECT_EQ(base.eval_f(x), f);
    EXPECT_TRUE(vce.isApprox(ce, 0.0));

    EXPECT_TRUE(m.lower().isApprox(Vec(base.lower().cwiseQuotient(d)), 0.0));
    EXPECT_TRUE(m.upper().isApprox(Vec(base.upper().cwiseQuotient(d)), 0.0));
    EXPECT_TRUE(m.start_point().isApprox(xs, 0.0));
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    EXPECT_TRUE(m.eval_grad(xs).isApprox(gs, 0.0));
    EXPECT_TRUE(m.eval_ce(xs).isApprox(ce, 0.0));
    EXPECT_TRUE(m.eval_ci(xs).isApprox(ci, 0.0));
    EXPECT_TRUE(Eigen::MatrixXd(m.eval_jac_e(xs)).isApprox(Eigen::MatrixXd(jes), 0.0));
    EXPECT_TRUE(Eigen::MatrixXd(m.eval_jac_i(xs)).isApprox(Eigen::MatrixXd(jis), 0.0));
    EXPECT_TRUE(Eigen::MatrixXd(m.eval_hess(xs, 0.75, le, li)).isApprox(Eigen::MatrixXd(hs), 0.0));
    HVEN_SUPPRESS_DEPRECATED_END
}

TEST(ColumnScaledModel, AnInfiniteBoundStaysInfinite) {
    PolyFitModel base;
    class Unbounded final : public NlpModel {
      public:
        explicit Unbounded(const NlpModel &m) : m_(m) {}
        Index n() const override { return m_.n(); }
        Index me() const override { return m_.me(); }
        Index mi() const override { return m_.mi(); }
        double eval_f(const Vec &x) const override { return m_.eval_f(x); }
        HVEN_SUPPRESS_DEPRECATED_BEGIN
        Vec eval_grad(const Vec &x) const override { return m_.eval_grad(x); }
        Vec eval_ce(const Vec &x) const override { return m_.eval_ce(x); }
        Vec eval_ci(const Vec &x) const override { return m_.eval_ci(x); }
        SpMatRM eval_hess(const Vec &x, double w, const Vec &a, const Vec &b) const override {
            return m_.eval_hess(x, w, a, b);
        }
        SpMatRM eval_jac_e(const Vec &x) const override { return m_.eval_jac_e(x); }
        SpMatRM eval_jac_i(const Vec &x) const override { return m_.eval_jac_i(x); }
        HVEN_SUPPRESS_DEPRECATED_END
        const Vec &lower() const override { return lo_; }
        const Vec &upper() const override { return up_; }
        Vec start_point() const override { return m_.start_point(); }

      private:
        const NlpModel &m_;
        Vec lo_ = Vec::Constant(6, -std::numeric_limits<double>::infinity());
        Vec up_ = Vec::Constant(6, std::numeric_limits<double>::infinity());
    } unbounded(base);
    const ColumnScaledModel m(unbounded, Vec::Constant(6, 0.25));
    EXPECT_TRUE(std::isinf(m.lower()(2)) && m.lower()(2) < 0.0);
    EXPECT_TRUE(std::isinf(m.upper()(5)) && m.upper()(5) > 0.0);
}

TEST(ColumnScaledModel, BadFactorsAreRefused) {
    PolyFitModel base;
    EXPECT_THROW(ColumnScaledModel(base, Vec::Ones(5)), std::invalid_argument);
    Vec d = Vec::Ones(6);
    d(2) = 0.0;
    EXPECT_THROW(ColumnScaledModel(base, d), std::invalid_argument);
    d(2) = -1.0;
    EXPECT_THROW(ColumnScaledModel(base, d), std::invalid_argument);
    d(2) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(ColumnScaledModel(base, d), std::invalid_argument);
    d(2) = std::numeric_limits<double>::infinity();
    EXPECT_THROW(ColumnScaledModel(base, d), std::invalid_argument);
}

// ===========================================================================
// (c) THE OPTION
// ===========================================================================

TEST(ColumnScalingOption, TheShippedDefaultIsAutoAtTheHalfPrecisionGate) {
    const SqpOptions o;
    EXPECT_EQ(SqpColumnScaling::kAuto, o.column_scaling);
    EXPECT_FALSE(o.enable_scaling) << "the row layer stays opt-in";
    EXPECT_DOUBLE_EQ(std::ldexp(1.0, 26), o.column_scaling_threshold);
    EXPECT_DOUBLE_EQ(1.0 / std::sqrt(std::numeric_limits<double>::epsilon()),
                     o.column_scaling_threshold);
}

TEST(ColumnScalingOption, ValidationRefusesTheSentinelAndABadThreshold) {
    SqpOptions o;
    o.column_scaling = SqpColumnScaling::kSqpColumnScalingCount;
    EXPECT_THROW(SqpSolver{o}, std::invalid_argument);
    for (const double t : {0.5, 0.0, -1.0, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
        SqpOptions p;
        p.column_scaling_threshold = t;
        EXPECT_THROW(SqpSolver{p}, std::invalid_argument) << t;
    }
    SqpOptions ok;
    ok.column_scaling_threshold = 1.0;
    EXPECT_NO_THROW(SqpSolver{ok});
}

TEST(ColumnScalingOption, TheDumpCarriesBothFields) {
    SqpOptions o;
    o.column_scaling = SqpColumnScaling::kAuto;
    o.column_scaling_threshold = 4096.0;
    std::ostringstream os;
    dump_options(o, os);
    const std::string s = os.str();
    EXPECT_NE(std::string::npos, s.find("column_scaling = kAuto")) << s;
    EXPECT_NE(std::string::npos, s.find("column_scaling_threshold = 4096")) << s;
    for (const auto mode : {SqpColumnScaling::kOff, SqpColumnScaling::kAlways}) {
        SqpOptions p;
        p.column_scaling = mode;
        std::ostringstream q;
        dump_options(p, q);
        EXPECT_NE(std::string::npos,
                  q.str().find(mode == SqpColumnScaling::kOff ? "column_scaling = kOff"
                                                              : "column_scaling = kAlways"));
    }
}

// ===========================================================================
// (d) THE DRIVER
// ===========================================================================

void expect_caller_certified(const NlpModel &model, const SqpResult &sol) {
    // AN INDEPENDENT MEASUREMENT of the caller's own problem at the returned point
    // and prices: the model-taking evaluate_kkt knows nothing about any factor.
    const SqpKkt k = evaluate_kkt(model, sol.x, sol.lambda_e, sol.lambda_i, SqpOptions{}.feas_tol);
    EXPECT_DOUBLE_EQ(k.stationarity, sol.sqp_stationarity);
    EXPECT_DOUBLE_EQ(k.feasibility, sol.sqp_feasibility);
    EXPECT_DOUBLE_EQ(k.complementarity, sol.sqp_complementarity);
    EXPECT_DOUBLE_EQ(k.residual(), sol.kkt_residual);
    ASSERT_EQ(k.z.size(), sol.z.size());
    for (Index i = 0; i < sol.z.size(); ++i) {
        EXPECT_DOUBLE_EQ(k.z(i), sol.z(i)) << "bound price " << i;
    }
    EXPECT_LE(k.feasibility, SqpOptions{}.feas_tol) << "feasible in the caller's units";
    EXPECT_DOUBLE_EQ(model.eval_f(sol.x), sol.f);
    // THE SHARED FOUR, by their one definition, on the caller's own evaluations.
    Vec g, ce, ci;
    SpMatRM je, ji;
    model.eval_grad_in_place(sol.x, g);
    model.eval_ce_in_place(sol.x, ce);
    model.eval_ci_in_place(sol.x, ci);
    model.eval_jac_e_in_place(sol.x, je);
    model.eval_jac_i_in_place(sol.x, ji);
    const DeclaredDiagnostics dd =
        compute_declared_diagnostics(sol.x, sol.lambda_e, sol.lambda_i, sol.z, g, je, ji, ce, ci,
                                     model.lower(), model.upper(), {});
    EXPECT_DOUBLE_EQ(dd.stationarity, sol.stationarity);
    EXPECT_DOUBLE_EQ(dd.feasibility_e, sol.feasibility_e);
    EXPECT_DOUBLE_EQ(dd.feasibility_i, sol.feasibility_i);
    EXPECT_DOUBLE_EQ(dd.complementarity, sol.complementarity);
}

TEST(ColumnScalingDriver, AutoSolvesTheBadlyScaledFitInTheCallersUnits) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    EXPECT_TRUE(sol.scaling.columns_active);
    EXPECT_TRUE(sol.scaling.active) << "kAuto engages the row/objective layer when it fires";
    EXPECT_DOUBLE_EQ(65536.0, sol.scaling.col_spread);
    EXPECT_DOUBLE_EQ(1.0 / 256.0, sol.scaling.col_min);
    EXPECT_DOUBLE_EQ(256.0, sol.scaling.col_max);
    const Vec xs = model.x_star();
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(xs(i), sol.x(i), 1e-6 * std::max(1e-4, std::abs(xs(i)))) << i;
    }
    expect_caller_certified(model, sol);
    // The warm start carries the caller's point and prices, and none of the
    // scaled-space state.
    ASSERT_TRUE(sol.warm_start.valid);
    EXPECT_TRUE(sol.warm_start.x.isApprox(sol.x, 0.0));
    EXPECT_TRUE(sol.warm_start.z.isApprox(sol.z, 0.0));
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.tr_radius);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.funnel_width);
    EXPECT_EQ(nullptr, sol.warm_start.hot);
    EXPECT_FALSE(sol.warm_start.has_prox_center);
}

TEST(ColumnScalingDriver, AnActiveBoundsPriceIsReMeasuredInTheCallersUnits) {
    // p1 <= 1.5 binds (y1 = 2): the optimum interpolates (1, 1.5, 0.5), a = (1, 0.0125,
    // -7.5e-5), and the cap's price is the objective's pull, |p1 - y1| = 0.5. The
    // engine's price is z~ = d z with d = 64 on that column; the export must be the
    // caller's, measured on its own model.
    PolyFitModel model(1.5);
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.columns_active);
    const double xs[6] = {1.0, 0.0125, -7.5e-5, 1.0, 1.5, 0.5};
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(xs[i], sol.x(i), 1e-6 * std::max(1e-4, std::abs(xs[i]))) << i;
    }
    EXPECT_NEAR(0.5, std::abs(sol.z(4)), 1e-6) << "the cap's price, in the caller's units";
    expect_caller_certified(model, sol);
}

TEST(ColumnScalingDriver, AlwaysScalesTheColumnsAloneWhenTheRowLayerIsOff) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    EXPECT_TRUE(sol.scaling.columns_active);
    EXPECT_FALSE(sol.scaling.active) << "kAlways leaves the row layer to enable_scaling";
    expect_caller_certified(model, sol);
}

void expect_same_solve(const SqpResult &a, const SqpResult &b) {
    EXPECT_EQ(a.status, b.status);
    EXPECT_TRUE(a.x.isApprox(b.x, 0.0));
    EXPECT_TRUE(a.lambda_e.isApprox(b.lambda_e, 0.0));
    EXPECT_TRUE(a.lambda_i.isApprox(b.lambda_i, 0.0));
    EXPECT_TRUE(a.z.isApprox(b.z, 0.0));
    EXPECT_EQ(a.f, b.f);
    EXPECT_EQ(a.kkt_residual, b.kkt_residual);
    EXPECT_EQ(a.history.size(), b.history.size());
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.evals_values + 1, b.counters.evals_values)
        << "the rule's Jacobian probe is counted as the warm probe is";
    EXPECT_EQ(a.counters.evals_full, b.counters.evals_full)
        << "the rule's Jacobian call is not a full evaluation";
    EXPECT_EQ(a.counters.eval_hess_calls, b.counters.eval_hess_calls);
    EXPECT_EQ(a.counters.scaled_convergence_refusals, b.counters.scaled_convergence_refusals);
    EXPECT_FALSE(b.scaling.columns_active);
    EXPECT_FALSE(b.scaling.active);
}

// Forwards every call to a model and counts the derivative calls by kind.
class CountingModel final : public NlpModel {
  public:
    explicit CountingModel(const NlpModel &m) : m_(m) {}
    mutable long grads = 0, jacs = 0, hess = 0, values = 0;
    Index n() const override { return m_.n(); }
    Index me() const override { return m_.me(); }
    Index mi() const override { return m_.mi(); }
    double eval_f(const Vec &x) const override {
        ++values;
        return m_.eval_f(x);
    }
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    Vec eval_grad(const Vec &x) const override {
        ++grads;
        return m_.eval_grad(x);
    }
    Vec eval_ce(const Vec &x) const override { return m_.eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return m_.eval_ci(x); }
    SpMatRM eval_hess(const Vec &x, double w, const Vec &a, const Vec &b) const override {
        ++hess;
        return m_.eval_hess(x, w, a, b);
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        ++jacs;
        return m_.eval_jac_e(x);
    }
    SpMatRM eval_jac_i(const Vec &x) const override {
        ++jacs;
        return m_.eval_jac_i(x);
    }
    HVEN_SUPPRESS_DEPRECATED_END
    const Vec &lower() const override { return m_.lower(); }
    const Vec &upper() const override { return m_.upper(); }
    Vec start_point() const override { return m_.start_point(); }

  private:
    const NlpModel &m_;
};

TEST(ColumnScalingDriver, TheRuleCostsOneJacobianCallAndNothingElse) {
    test_support::Hs14Model base;
    CountingModel off_model(base), on_model(base);
    SqpSolver off{off_options()};
    const SqpResult a = off.solve(off_model, base.start_point());
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver on{opts};
    const SqpResult b = on.solve(on_model, base.start_point());
    ASSERT_FALSE(b.scaling.columns_active);
    EXPECT_EQ(off_model.values, on_model.values);
    EXPECT_EQ(off_model.grads, on_model.grads);
    EXPECT_EQ(off_model.hess, on_model.hess);
    EXPECT_EQ(off_model.jacs + 2, on_model.jacs) << "one call per Jacobian block (me, mi > 0)";
}

TEST(ColumnScalingDriver, BelowTheGateAutoIsTheOffSolve) {
    test_support::Hs14Model model;
    SqpSolver off{off_options()};
    const SqpResult a = off.solve(model, model.start_point());
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver on{opts};
    const SqpResult b = on.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, a.status);
    EXPECT_LT(b.scaling.col_spread, opts.column_scaling_threshold);
    EXPECT_GE(b.scaling.col_spread, 1.0);
    expect_same_solve(a, b);
}

TEST(ColumnScalingDriver, TheIterationEventCarriesTheCallersPointAndPrices) {
    PolyFitModel model(1.5); // the cap binds, so a bound price is nonzero (z~ = 64 z)
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver driver{opts};
    Vec last_x, last_z;
    driver.set_iteration_callback([&](const IterationEvent &ev) {
        last_x = ev.x;
        last_z = ev.z;
        return CallbackAction::kContinue;
    });
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    ASSERT_EQ(6, last_x.size());
    // The terminal row's event describes the returned point, in the caller's units:
    // the same doubles (the factors are powers of two).
    EXPECT_TRUE(last_x.isApprox(sol.x, 0.0)) << last_x.transpose() << " vs " << sol.x.transpose();
    ASSERT_GT(std::abs(sol.z(4)), 0.1) << "the fixture's premise: a live bound price";
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(sol.z(i), last_z(i), 1e-9) << i;
    }
}

// THE WARM START'S SCALED-SPACE STATE IS NOT CARRIED IN. A radius is an l-infinity
// box in the variables the solve ran in; a caller-space radius read as a scaled one
// would be wrong by the factors, so the column-scaled solve starts at tr_init.
TEST(ColumnScalingDriver, ACallerSpaceRadiusIsNotCarriedIntoAScaledSolve) {
    PolyFitModel model;
    SqpSolver plain{off_options()};
    SqpResult cold = plain.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);
    SqpWarmStart warm = cold.warm_start;
    warm.tr_radius = 0.125; // a carried radius distinct from tr_init
    warm.x = model.start_point();
    // the control: an unscaled warm solve takes the radius
    SqpOptions off_warm = off_options();
    off_warm.common.start_level = StartLevel::kWarm;
    SqpSolver control{off_warm};
    const SqpResult c = control.solve(model, model.start_point(), warm, SolveBudget{});
    ASSERT_FALSE(c.history.empty());
    ASSERT_DOUBLE_EQ(0.125, c.history.front().tr_radius) << "the premise: kWarm carries it";
    SqpOptions opts = off_warm;
    opts.column_scaling =
        SqpColumnScaling::kAlways; // the row layer off: its own ingest refusal would hide this one
    SqpSolver scaled{opts};
    const SqpResult s = scaled.solve(model, model.start_point(), warm, SolveBudget{});
    ASSERT_TRUE(s.scaling.columns_active);
    ASSERT_FALSE(s.history.empty());
    EXPECT_DOUBLE_EQ(opts.tr_init, s.history.front().tr_radius);
    EXPECT_EQ(SolveStatus::kOptimal, s.status);
}

// ===========================================================================
// THE INFEASIBLE CIRCLE AND LINE, one variable in units of 1e-4: x0^2 + (1e-4 y)^2 = 1
// and x0 + 1e-4 y = 2 have no common point, so the solve ends in restoration. The
// column-scaled solve must be EXACTLY the unscaled solve of the column-scaled model
// -- restoration included, whose feasibility model is built around the scaled model
// and must not scale its variables a second time -- mapped back.
// ===========================================================================
class ScaledInfeasibleModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + 1e-4 * x(1); }
    Vec eval_grad(const Vec &) const override { return (Vec(2) << 1.0, 1e-4).finished(); }
    Vec eval_ce(const Vec &x) const override {
        const double y = 1e-4 * x(1);
        return (Vec(2) << x(0) * x(0) + y * y - 1.0, x(0) + y - 2.0).finished();
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0e-8 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0e-8 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0e-4;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return (Vec(2) << 2.0, 2.0e4).finished(); }

  private:
    Vec lower_ = (Vec(2) << -10.0, -1e5).finished();
    Vec upper_ = (Vec(2) << 0.5, 1e5).finished(); // x0's cap binds in restoration
};

// THE EQUIVALENCE, every exported vector: the scaled solve's x, lambda and z are the
// reference's mapped (x = d x~, z = z~ / d), on EVERY status -- a bound price the
// inner finish exported from the feasibility problem (any restoration exit that
// adopted its prices) is mapped, grad L's is re-measured, and on these fixtures the
// re-measure is bit-equal to the map (power-of-two d). scaled_kkt_residual is the
// inner residual under the column layer alone.
struct Equivalence {
    SqpResult ref;
    SqpResult sol;
    Vec d;
};

template <class Model> Equivalence expect_scaled_equivalence(const Model &model, int max_iter) {
    const Vec x0 = model.start_point();
    // The rule reads the start PROJECTED onto the box, as the solve starts there.
    SpMatRM je;
    model.eval_jac_e_in_place(x0.cwiseMax(model.lower()).cwiseMin(model.upper()), je);
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(model.n(), je, SpMatRM(0, model.n()), rule);
    EXPECT_TRUE(cs.active);
    Equivalence e;
    e.d = cs.active ? cs.cols : Vec::Ones(model.n());

    SqpOptions opts = off_options();
    opts.max_iter = max_iter;
    SqpSolver plain{opts};
    const ColumnScaledModel scaled_model(model, e.d);
    e.ref = plain.solve(scaled_model, scaled_model.to_scaled(x0));

    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver driver{opts};
    e.sol = driver.solve(model, x0);
    const SqpResult &ref = e.ref;
    const SqpResult &sol = e.sol;
    EXPECT_TRUE(sol.scaling.columns_active);
    EXPECT_FALSE(sol.scaling.active);

    EXPECT_EQ(ref.status, sol.status);
    EXPECT_EQ(ref.infeasibility_certified, sol.infeasibility_certified);
    EXPECT_EQ(ref.counters.major_iters, sol.counters.major_iters);
    EXPECT_EQ(ref.counters.restoration_iters, sol.counters.restoration_iters);
    EXPECT_EQ(ref.counters.restoration_entries, sol.counters.restoration_entries);
    EXPECT_EQ(ref.counters.qp_minor_iters, sol.counters.qp_minor_iters);
    EXPECT_EQ(ref.counters.factorizations, sol.counters.factorizations);
    EXPECT_EQ(ref.counters.restoration_h_out, sol.counters.restoration_h_out);
    EXPECT_EQ(ref.counters.evals_full + 1, sol.counters.evals_full) << "the re-measurement";
    EXPECT_EQ(ref.counters.evals_values + 1, sol.counters.evals_values) << "the rule's probe";
    EXPECT_EQ(ref.history.size(), sol.history.size());
    EXPECT_TRUE(sol.x.isApprox(scaled_model.to_caller(ref.x), 0.0));
    EXPECT_TRUE(sol.lambda_e.isApprox(ref.lambda_e, 0.0));
    EXPECT_TRUE(sol.lambda_i.isApprox(ref.lambda_i, 0.0));
    EXPECT_EQ(ref.z.size(), sol.z.size());
    if (ref.z.size() == sol.z.size()) {
        EXPECT_TRUE(sol.z.isApprox(Vec(ref.z.cwiseQuotient(e.d)), 0.0))
            << sol.z.transpose() << " vs " << ref.z.transpose() << " / d";
    }
    EXPECT_EQ(ref.kkt_residual, sol.scaling.scaled_kkt_residual)
        << "the inner residual, under the column layer alone";
    return e;
}

TEST(ColumnScalingDriver, TheScaledSolveIsTheUnscaledSolveOfTheScaledModel) {
    ScaledInfeasibleModel model;
    const Equivalence e = expect_scaled_equivalence(model, 60);
    ASSERT_GE(e.ref.counters.restoration_iters, 1) << "the premise: restoration runs";
    std::printf("[ SCL ] scaled-model equivalence: status %d majors %ld resto %ld\n",
                static_cast<int>(e.sol.status), static_cast<long>(e.sol.counters.major_iters),
                static_cast<long>(e.sol.counters.restoration_iters));
}

// ===========================================================================
// THE BOX-BLOCKED EQUALITY, one variable in units of 1e4: 1e-4 x0 = 5 with
// 0 <= x0 <= 1e4 is out of reach, so restoration ends on x0's cap, where the
// feasibility problem's bound price (the certificate's, -1 in the row's units) and
// grad L's (0: lambda_e cancels the objective there) differ. min 1/2 (1e-4 x0)^2 +
// 1/2 x1^2, x1 in [0, 1].
// ===========================================================================
class ScaledBoxBlockedModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        const double y = 1e-4 * x(0);
        return 0.5 * (y * y + x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override { return (Vec(2) << 1e-8 * x(0), x(1)).finished(); }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 1e-4 * x(0) - 5.0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 1e-8 * obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1e-4;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return (Vec(2) << 5e3, 0.5).finished(); }

  private:
    Vec lower_ = Vec::Zero(2);
    Vec upper_ = (Vec(2) << 1e4, 1.0).finished();
};

TEST(ColumnScalingDriver, ACertifiedExitMapsTheFeasibilityPrice) {
    // Restoration ends on x0's cap: the certificate's price there is not grad L's,
    // which a re-measurement would have exported instead.
    ScaledBoxBlockedModel model;
    const Equivalence e = expect_scaled_equivalence(model, 60);
    ASSERT_EQ(SolveStatus::kInfeasible, e.sol.status);
    ASSERT_TRUE(e.sol.infeasibility_certified);
    const SqpKkt k = evaluate_kkt(model, e.sol.x, e.sol.lambda_e, e.sol.lambda_i, 1e-6);
    ASSERT_LT(e.sol.z(0), 0.0) << "the premise: the cap carries the certificate's price";
    EXPECT_NE(k.z(0), e.sol.z(0)) << "the premise: grad L's price differs";
}

TEST(ColumnScalingDriver, ARestorationExitAtTheCapMapsTheFeasibilityPriceToo) {
    // THE SAME EXPORT ON EVERY STATUS: restoration adopts its own prices on every
    // exit that moved x, so a solve the iteration cap stops inside restoration
    // (kMaxIter) exports the feasibility problem's price as the certified exit does.
    // Swept over the caps that end before, inside and after the phase.
    ScaledInfeasibleModel model;
    bool saw_discriminating_max_iter = false;
    for (int cap = 1; cap <= 8; ++cap) {
        SCOPED_TRACE(cap);
        const Equivalence c = expect_scaled_equivalence(model, cap);
        const SqpKkt k = evaluate_kkt(model, c.sol.x, c.sol.lambda_e, c.sol.lambda_i, 1e-6);
        if (c.sol.status == SolveStatus::kMaxIter && c.sol.counters.restoration_iters > 0 &&
            k.z.size() == c.sol.z.size() && !k.z.isApprox(c.sol.z, 1e-6)) {
            saw_discriminating_max_iter = true;
        }
    }
    EXPECT_TRUE(saw_discriminating_max_iter)
        << "the premise: a kMaxIter restoration exit whose price is not grad L's";
}

TEST(ColumnScalingDriver, TheWarmStartCrossesBothWays) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver first{opts};
    const SqpResult cold = first.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);

    // INTO A COLUMN-SCALED SOLVE: x / d, z * d on the way in.
    SqpOptions warm_opts = opts;
    warm_opts.common.start_level = StartLevel::kWarm;
    SqpSolver second{warm_opts};
    Vec first_x;
    second.set_iteration_callback([&](const IterationEvent &ev) {
        if (first_x.size() == 0) {
            first_x = ev.x;
        }
        return CallbackAction::kContinue;
    });
    const SqpResult again =
        second.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
    // The first major stands on the warm point: x~ = x / d in, x = d x~ out, the same
    // doubles (power-of-two d).
    EXPECT_TRUE(first_x.isApprox(cold.warm_start.x, 0.0))
        << first_x.transpose() << " vs " << cold.warm_start.x.transpose();
    EXPECT_NE(StartLevel::kCold, again.counters.start_level_used);
    ASSERT_EQ(SolveStatus::kOptimal, again.status);
    EXPECT_TRUE(again.scaling.columns_active);
    std::printf("[ SCL ] warm re-solve: cold majors %ld, warm majors %ld\n",
                static_cast<long>(cold.counters.major_iters),
                static_cast<long>(again.counters.major_iters));
    EXPECT_LE(again.counters.major_iters, 1) << "it starts at the optimum, in the right units";
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(cold.x(i), again.x(i), 1e-8 * std::max(1e-4, std::abs(cold.x(i)))) << i;
    }

    // INTO AN UNSCALED SOLVE: the export is in the caller's units.
    SqpOptions plain = off_options();
    plain.common.start_level = StartLevel::kWarm;
    SqpSolver third{plain};
    const SqpResult crossed =
        third.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
    EXPECT_NE(StartLevel::kCold, crossed.counters.start_level_used);
    ASSERT_EQ(SolveStatus::kOptimal, crossed.status);
    EXPECT_FALSE(crossed.scaling.columns_active);
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(cold.x(i), crossed.x(i), 1e-8 * std::max(1e-4, std::abs(cold.x(i)))) << i;
    }
}

// ===========================================================================
// THE BILINEAR ROW: x0 x1 = 2, so the Jacobian (x1, x0) -- and with it the rule's
// spread -- depends on the point read. Near (1, 2) it is 2 (below kAuto's gate); at
// (1/100, 200) it is ~2^14 (above it). min 1/2 ||x - (1, 2)||^2.
// ===========================================================================
class BilinearRowModel : public NlpModel {
  public:
    BilinearRowModel() = default;
    BilinearRowModel(Vec lower, Vec upper) : lower_(std::move(lower)), upper_(std::move(upper)) {}
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return 0.5 * ((x(0) - 1.0) * (x(0) - 1.0) + (x(1) - 2.0) * (x(1) - 2.0));
    }
    Vec eval_grad(const Vec &x) const override {
        return (Vec(2) << x(0) - 1.0, x(1) - 2.0).finished();
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, x(0) * x(1) - 2.0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM h(2, 2);
        const double l = lambda_e.size() > 0 ? lambda_e(0) : 0.0;
        h.insert(0, 0) = obj_scale;
        h.insert(0, 1) = l; // the upper triangle only (nlp_model.h)
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = x(1);
        j.insert(0, 1) = x(0);
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return (Vec(2) << 1.0, 2.0).finished(); }

  private:
    Vec lower_ = Vec::Constant(2, 1e-3);
    Vec upper_ = Vec::Constant(2, 1e3);
};

double rule_spread_at(const NlpModel &model, const Vec &x) {
    SpMatRM je;
    model.eval_jac_e_in_place(x, je);
    return compute_column_scaling(model.n(), je, SpMatRM(0, model.n()), ColumnScalingRule{}).spread;
}

TEST(ColumnScalingDriver, TheRuleReadsTheWarmPointWhenTheIngestWouldTakeIt) {
    BilinearRowModel model;
    const Vec far = (Vec(2) << 0.01, 200.0).finished();
    const double spread_x0 = rule_spread_at(model, model.start_point());
    const double spread_warm = rule_spread_at(model, far);
    ASSERT_LT(spread_x0, 1e3) << "the premise: x0 is below the gate";
    ASSERT_GE(spread_warm, 1e3) << "the premise: the warm point is above it";

    // A valid warm object (dimensions and working set from a real solve), moved to
    // the far point.
    SqpSolver plain{off_options()};
    const SqpResult seed = plain.solve(model, model.start_point());
    ASSERT_TRUE(seed.warm_start.valid);
    SqpWarmStart warm = seed.warm_start;
    warm.x = far;

    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    opts.common.start_level = StartLevel::kWarm;
    SqpSolver warm_driver{opts};
    const SqpResult w = warm_driver.solve(model, model.start_point(), warm, SolveBudget{});
    EXPECT_NE(StartLevel::kCold, w.counters.start_level_used);
    EXPECT_TRUE(w.scaling.columns_active) << "the rule read warm.x";
    EXPECT_DOUBLE_EQ(spread_warm, w.scaling.col_spread);

    // AT A kCold CEILING the ingest never takes warm.x, so neither does the rule.
    opts.common.start_level = StartLevel::kCold;
    SqpSolver cold_driver{opts};
    const SqpResult c = cold_driver.solve(model, model.start_point(), warm, SolveBudget{});
    EXPECT_EQ(StartLevel::kCold, c.counters.start_level_used);
    EXPECT_FALSE(c.scaling.columns_active) << "the rule read x0";
    EXPECT_DOUBLE_EQ(spread_x0, c.scaling.col_spread);

    // AN INVALID warm object: x0 is read.
    opts.common.start_level = StartLevel::kWarm;
    SqpSolver invalid_driver{opts};
    const SqpResult i =
        invalid_driver.solve(model, model.start_point(), SqpWarmStart{}, SolveBudget{});
    EXPECT_FALSE(i.scaling.columns_active);
    EXPECT_DOUBLE_EQ(spread_x0, i.scaling.col_spread);
}

TEST(ColumnScalingDriver, TheRuleReadsTheStartAsTheSolveDoes) {
    // x0 = (1/100, 200) is outside x1 <= 1: projected it is (1/2, 1), below the gate;
    // verbatim it is above it. The rule follows project_start_point.
    const BilinearRowModel model((Vec(2) << 0.5, 1e-3).finished(), (Vec(2) << 1e3, 1.0).finished());
    const Vec x0 = (Vec(2) << 0.01, 200.0).finished();
    const Vec projected = x0.cwiseMax(model.lower()).cwiseMin(model.upper());
    ASSERT_LT(rule_spread_at(model, projected), 1e3);
    ASSERT_GE(rule_spread_at(model, x0), 1e3);
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    opts.max_iter = 5;
    SqpSolver projecting{opts};
    const SqpResult p = projecting.solve(model, x0);
    EXPECT_FALSE(p.scaling.columns_active);
    EXPECT_DOUBLE_EQ(rule_spread_at(model, projected), p.scaling.col_spread);
    opts.project_start_point = false;
    SqpSolver verbatim{opts};
    const SqpResult v = verbatim.solve(model, x0);
    EXPECT_TRUE(v.scaling.columns_active);
    EXPECT_DOUBLE_EQ(rule_spread_at(model, x0), v.scaling.col_spread);
}

TEST(ColumnScalingDriver, TheArgumentRefusalsAreSolveImplsOwn) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    SqpSolver driver{opts};
    EXPECT_THROW(driver.solve(model, Vec::Zero(5)), std::invalid_argument);
    Vec bad = Vec::Zero(6);
    bad(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(driver.solve(model, bad), std::invalid_argument);
}

// ===========================================================================
// THE THREE CONSUMERS OF ONE COUNTERS OBJECT AGREE on a fired solve: the
// `sqp.solve.end` event, the returned result and the ledger record. The column
// layer's re-measurement (+1 evals_full) and its probe (+1 evals_values) are both
// made before the emission.
// ===========================================================================
class EndSink final : public TraceSink {
  public:
    std::vector<SqpCounters> ends;
    std::vector<std::pair<bool, double>> columns; // (columns_active, col_spread) per end
    void on_ipqp_iter(const IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const QpModeTraceEvent &) override {}
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {}
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &e) override {
        ends.push_back(e.counters);
        columns.emplace_back(e.columns_active, e.col_spread);
    }
};

void expect_three_consumers_agree(const SqpResult &sol, const EndSink &sink, const Ledger &ledger) {
    ASSERT_FALSE(sink.ends.empty());
    ASSERT_EQ(1u, ledger.sqp_records().size());
    const SqpCounters &t = sink.ends.back(); // the outer solve closes last
    const SqpCounters &l = ledger.sqp_records()[0].counters;
    // The column pair rides the end line (M7 W2 SCW): fired or not, and the spread.
    EXPECT_EQ(sol.scaling.columns_active, sink.columns.back().first);
    EXPECT_EQ(sol.scaling.col_spread, sink.columns.back().second);
    EXPECT_EQ(sol.counters.evals_full, t.evals_full);
    EXPECT_EQ(sol.counters.evals_full, l.evals_full);
    EXPECT_EQ(sol.counters.evals_values, t.evals_values);
    EXPECT_EQ(sol.counters.evals_values, l.evals_values);
    EXPECT_EQ(sol.counters.major_iters, t.major_iters);
    EXPECT_EQ(sol.counters.major_iters, l.major_iters);
    EXPECT_EQ(sol.counters.scaled_convergence_refusals, t.scaled_convergence_refusals);
    EXPECT_EQ(sol.counters.scaled_convergence_refusals, l.scaled_convergence_refusals);
}

TEST(ColumnScalingDriver, TheTraceTheResultAndTheLedgerAgreeOnAFiredSolve) {
    PolyFitModel model;
    SqpSolver plain{off_options()};
    const SqpResult ref = plain.solve(model, model.start_point());
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver driver{opts};
    EndSink sink;
    Ledger ledger;
    driver.attach_trace(&sink);
    driver.attach_ledger(&ledger, "scl");
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_TRUE(sol.scaling.columns_active);
    EXPECT_GT(sol.scaling.col_spread, 1.0);
    expect_three_consumers_agree(sol, sink, ledger);
    EXPECT_GE(sol.counters.evals_values, 1) << "the probe is in it";
    EXPECT_EQ(ref.counters.evals_values, 0) << "the kOff solve has no probe";

    // AND ON A SOLVE THE GATE LEFT ALONE: the probe alone, in all three.
    test_support::Hs14Model hs;
    SqpOptions auto_opts;
    auto_opts.column_scaling = SqpColumnScaling::kAuto;
    auto_opts.column_scaling_threshold = kFixtureGate;
    SqpSolver quiet{auto_opts};
    EndSink sink2;
    Ledger ledger2;
    quiet.attach_trace(&sink2);
    quiet.attach_ledger(&ledger2, "scl");
    const SqpResult q = quiet.solve(hs, hs.start_point());
    ASSERT_FALSE(q.scaling.columns_active);
    EXPECT_GE(q.scaling.col_spread, 1.0) << "the rule ran and its spread is reported";
    expect_three_consumers_agree(q, sink2, ledger2);
}

// THE DEPTH-1 EVENTS carry the caller's units in the caller's variables: each one
// is the reference's (the kOff solve of the scaled model) with its first n entries
// of x multiplied by d and of z divided by d, and nothing else moved.
template <class Model>
void expect_mapped_events(const Model &model, int max_iter, bool scaled_bound_price) {
    const Vec x0 = model.start_point();
    SpMatRM je;
    model.eval_jac_e_in_place(x0.cwiseMax(model.lower()).cwiseMin(model.upper()), je);
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(2, je, SpMatRM(0, 2), rule);
    ASSERT_TRUE(cs.active);
    struct Seen {
        Index depth;
        Vec x, z, lambda_e;
    };
    auto record = [](std::vector<Seen> &into) {
        return [&into](const IterationEvent &ev) {
            into.push_back(Seen{ev.depth.value_or(-1), ev.x, ev.z, ev.lambda_e});
            return CallbackAction::kContinue;
        };
    };
    SqpOptions opts = off_options();
    opts.max_iter = max_iter;
    std::vector<Seen> ref_events, events;
    SqpSolver plain{opts};
    plain.set_iteration_callback(record(ref_events));
    const ColumnScaledModel scaled_model(model, cs.cols);
    (void)plain.solve(scaled_model, scaled_model.to_scaled(x0));
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver driver{opts};
    driver.set_iteration_callback(record(events));
    (void)driver.solve(model, x0);
    ASSERT_EQ(ref_events.size(), events.size());
    Index depth1 = 0;
    bool depth1_bound_price = false;
    for (std::size_t k = 0; k < events.size(); ++k) {
        SCOPED_TRACE(k);
        const Seen &r = ref_events[k];
        const Seen &e = events[k];
        ASSERT_EQ(r.depth, e.depth);
        ASSERT_EQ(r.x.size(), e.x.size());
        ASSERT_EQ(r.z.size(), e.z.size());
        ASSERT_GE(e.x.size(), 2);
        Vec x_expect = r.x;
        x_expect.head(2).array() *= cs.cols.array();
        Vec z_expect = r.z;
        z_expect.head(2).array() /= cs.cols.array();
        EXPECT_TRUE(e.x.isApprox(x_expect, 0.0))
            << e.x.transpose() << " vs " << x_expect.transpose();
        EXPECT_TRUE(e.z.isApprox(z_expect, 0.0))
            << e.z.transpose() << " vs " << z_expect.transpose();
        EXPECT_TRUE(e.lambda_e.isApprox(r.lambda_e, 0.0));
        if (e.depth == 1) {
            ++depth1;
            for (Index j = 0; j < 2; ++j) {
                depth1_bound_price = depth1_bound_price || (cs.cols(j) != 1.0 && r.z(j) != 0.0);
            }
            EXPECT_GT(e.x.size(), 2) << "the feasibility problem carries slacks beyond x";
        }
    }
    EXPECT_GE(depth1, 1) << "the premise: restoration rows were seen";
    if (scaled_bound_price) {
        EXPECT_TRUE(depth1_bound_price) << "the premise: a depth-1 bound price on a scaled column";
    }
}

TEST(ColumnScalingDriver, RestorationEventsCarryTheCallersUnitsInTheCallersVariables) {
    {
        SCOPED_TRACE("infeasible circle and line");
        expect_mapped_events(ScaledInfeasibleModel{}, 60, false);
    }
    {
        SCOPED_TRACE("box-blocked equality"); // restoration ends on x0's scaled cap
        expect_mapped_events(ScaledBoxBlockedModel{}, 60, true);
    }
}

// A HOT FACTORIZATION IS A FACTORIZATION OF A DIFFERENT KKT MATRIX in the scaled
// variables; its structure hash matches (a positive diagonal scaling keeps every
// pattern), so the hand-off must drop it rather than let the hash resolve kHot.
TEST(ColumnScalingDriver, AnUnscaledHotFactorizationDoesNotResolveHotInAScaledSolve) {
    PolyFitModel model;
    SqpOptions hot_opts = off_options();
    hot_opts.common.start_level = StartLevel::kHot;
    SqpSolver first{hot_opts};
    const SqpResult cold = first.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);
    ASSERT_NE(nullptr, cold.warm_start.hot) << "the premise: a hot handle was exported";
    // The control: the unscaled solve takes it.
    SqpSolver control{hot_opts};
    const SqpResult c = control.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
    ASSERT_EQ(StartLevel::kHot, c.counters.start_level_used) << "the premise: kHot resolves";

    SqpOptions opts = hot_opts;
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver scaled{opts};
    const SqpResult s = scaled.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
    ASSERT_TRUE(s.scaling.columns_active);
    EXPECT_NE(StartLevel::kHot, s.counters.start_level_used);
    EXPECT_NE(StartLevel::kCold, s.counters.start_level_used) << "the values still cross";
    EXPECT_EQ(nullptr, s.warm_start.hot);
    EXPECT_EQ(SolveStatus::kOptimal, s.status);
    expect_caller_certified(model, s);
}

// THE SSN PROX BLOCK lives in the scaled dual space: a column-scaled solve exports
// none of it (the hand-off clears what record_solve would attach).
TEST(ColumnScalingDriver, TheSsnProxBlockIsNotExportedFromAScaledSolve) {
    // HS27, whose SSN ladder arms (test_warm_start.cpp's prox-block fixture).
    auto problem = test_support::make_hs(27);
    const NlpModel &model = *problem.model;
    SqpOptions opts = off_options();
    opts.max_iter = 60;
    opts.qp_mode = QpMode::kSsn;
    SqpSolver plain{opts};
    const SqpResult ref = plain.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, ref.status);
    ASSERT_GT(ref.warm_start.prox_sigma, 0.0) << "the premise: the unscaled solve exports one";
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver scaled{opts};
    const SqpResult sol = scaled.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.columns_active);
    EXPECT_EQ(0.0, sol.warm_start.prox_sigma);
    EXPECT_FALSE(sol.warm_start.has_prox_center);
    EXPECT_EQ(0, sol.warm_start.prox_center_x.size());
}

// THE FIT WITH AN INEQUALITY ROW: 100 a1 + 1e4 a2 <= 1/2 (the slope-like row) cuts
// the interpolant (where it reads 1), so the row binds with a positive price.
class PolyFitIneqModel final : public PolyFitModel {
  public:
    Index mi() const override { return 1; }
    Vec eval_ci(const Vec &x) const override {
        return Vec::Constant(1, 100.0 * x(1) + 1e4 * x(2) - 0.5);
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 6);
        j.insert(0, 1) = 100.0;
        j.insert(0, 2) = 1e4;
        j.makeCompressed();
        return j;
    }
};

TEST(ColumnScalingDriver, AnInequalityRowSolvesInTheCallersUnits) {
    PolyFitIneqModel model;
    SqpSolver plain{off_options()};
    const SqpResult ref = plain.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, ref.status);
    for (const auto mode : {SqpColumnScaling::kAlways, SqpColumnScaling::kAuto}) {
        SCOPED_TRACE(mode == SqpColumnScaling::kAlways ? "kAlways" : "kAuto");
        SqpOptions opts;
        opts.column_scaling = mode;
        opts.column_scaling_threshold = kFixtureGate;
        SqpSolver driver{opts};
        const SqpResult sol = driver.solve(model, model.start_point());
        ASSERT_EQ(SolveStatus::kOptimal, sol.status);
        ASSERT_TRUE(sol.scaling.columns_active);
        ASSERT_EQ(1, sol.lambda_i.size());
        EXPECT_GT(sol.lambda_i(0), 1e-3) << "the row binds";
        EXPECT_NEAR(ref.lambda_i(0), sol.lambda_i(0), 1e-6 * std::abs(ref.lambda_i(0)));
        for (Index i = 0; i < 6; ++i) {
            EXPECT_NEAR(ref.x(i), sol.x(i), 1e-6 * std::max(1e-4, std::abs(ref.x(i)))) << i;
        }
        EXPECT_LE(model.eval_ci(sol.x)(0), opts.feas_tol);
        expect_caller_certified(model, sol);
    }
}

// THE SHARED PAYLOAD ROUTE (consume_payload) into a column-scaled solve: the
// declared-space currency a column-scaled solve exports is in the caller's units,
// and a column-scaled solve ingests it as it ingests the native warm start.
TEST(ColumnScalingDriver, ThePayloadRouteCrossesAColumnScaledSolve) {
    PolyFitModel model(1.5); // a live bound price, so z crosses too
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver first{opts};
    const SqpResult cold = first.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);
    const WarmStartData data = first.export_warm_start();
    ASSERT_EQ(6, data.primal_.size());
    EXPECT_TRUE(data.primal_.isApprox(cold.x, 0.0)) << "exported in the caller's units";
    EXPECT_TRUE(data.bound_lmults_.isApprox(cold.z, 0.0));

    SqpSolver second{opts};
    const SqpResult again = second.solve(model, model.start_point(), data, SolveBudget{});
    EXPECT_NE(StartLevel::kCold, again.counters.start_level_used);
    ASSERT_EQ(SolveStatus::kOptimal, again.status);
    EXPECT_TRUE(again.scaling.columns_active);
    EXPECT_LE(again.counters.major_iters, 1) << "it starts at the optimum, in the right units";
    for (Index i = 0; i < 6; ++i) {
        EXPECT_NEAR(cold.x(i), again.x(i), 1e-8 * std::max(1e-4, std::abs(cold.x(i)))) << i;
    }
    expect_caller_certified(model, again);

    // And into an unscaled solve.
    SqpSolver third{off_options()};
    const SqpResult crossed = third.solve(model, model.start_point(), data, SolveBudget{});
    EXPECT_NE(StartLevel::kCold, crossed.counters.start_level_used);
    ASSERT_EQ(SolveStatus::kOptimal, crossed.status);
    EXPECT_LE(crossed.counters.major_iters, 1);
}

// ===========================================================================
// (e) THE GUARD
// ===========================================================================

TEST(ScaledConvergenceGuard, AShrunkRowIsNotConvergedUntilTheCallersResidualIs) {
    // At feas_tol = 1e-3 the scaled test accepts |x^2 - 1| <= 6e-5 and the caller's
    // only |x^2 - 1| <= 1e-11. Newton squares the error, so the first iterate inside
    // the scaled band is at least ~(6e-5)^2 / 4 away -- inside the band and outside
    // the caller's tolerance whatever path the radius makes it take.
    SteepRowModel model;
    SqpOptions opts = off_options(); // the row layer alone
    opts.enable_scaling = true;
    opts.feas_tol = 1e-3;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.active);
    EXPECT_LT(sol.scaling.row_min, 1e-6) << "the row was shrunk";
    EXPECT_GE(sol.counters.scaled_convergence_refusals, 1)
        << "a scaled-feasible, caller-infeasible iterate was met and refused";
    const double c = model.eval_ce(sol.x)(0);
    EXPECT_LE(std::abs(c), opts.feas_tol) << "the caller's own residual at the returned point";
    EXPECT_LE(sol.sqp_feasibility, opts.feas_tol);

    // UNSCALED, the guard is never entered.
    SqpOptions plain_opts = off_options();
    plain_opts.feas_tol = opts.feas_tol;
    SqpSolver plain{plain_opts};
    const SqpResult ref = plain.solve(model, model.start_point());
    EXPECT_EQ(0, ref.counters.scaled_convergence_refusals);
}

TEST(ScaledConvergenceGuard, ABoxViolationAColumnFactorHidIsRefused) {
    HiddenBoxModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    opts.project_start_point = false;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point());
    ASSERT_TRUE(sol.scaling.columns_active);
    EXPECT_GE(sol.scaling.col_max, 4096.0) << "x1's 1e-4 column, scaled up";
    EXPECT_GE(sol.counters.scaled_convergence_refusals, 1)
        << "the start passes the scaled test and is infeasible in the caller's units";
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    EXPECT_LE(sol.x(1), model.upper()(1) + opts.feas_tol) << "the returned point is in the box";
    EXPECT_LE(sol.sqp_feasibility, opts.feas_tol);
}

// ===========================================================================
// THE HIDDEN GRADIENT (M7 W2 SCW): min 3 x0 + x1^2 / 2 s.t. 1e16 x0 + x1 = 1. The
// rule shrinks x0's column to ~2^-27, so its stationarity component is read through
// that factor: a warm start at x* whose price is off by 2e-15 carries a caller-unit
// gradient of 20 in x0 and passes the scaled test at major 0.
// ===========================================================================
class HiddenGradientModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return 3.0 * x(0) + 0.5 * x(1) * x(1); }
    Vec eval_grad(const Vec &x) const override { return (Vec(2) << 3.0, x(1)).finished(); }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 1e16 * x(0) + x(1) - 1.0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1e16;
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lower_ = Vec::Constant(2, -1e20);
    Vec upper_ = Vec::Constant(2, 1e20);
};

TEST(ScaledConvergenceGuard, ACallerUnitGradientAColumnFactorHidIsRefused) {
    HiddenGradientModel model;
    SqpSolver plain{off_options()};
    const SqpResult cold = plain.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);
    ASSERT_TRUE(cold.warm_start.valid);
    ASSERT_EQ(0, cold.warm_start.column_factors.size()) << "kOff records no decision";
    SqpWarmStart warm = cold.warm_start;
    warm.lambda_e(0) += 2e-15; // caller-unit grad L in x0: 1e16 * 2e-15 = 20

    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAlways;
    opts.common.start_level = StartLevel::kWarm;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, model.start_point(), warm, SolveBudget{});
    ASSERT_TRUE(sol.scaling.columns_active);
    ASSERT_LE(sol.scaling.col_min, std::ldexp(1.0, -26)) << "the premise: x0's column shrunk";
    EXPECT_NE(StartLevel::kCold, sol.counters.start_level_used);
    EXPECT_GE(sol.counters.scaled_convergence_refusals, 1)
        << "the warm point passes the scaled test with a caller-unit gradient of 20";
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    EXPECT_GE(sol.counters.major_iters, 1);
    EXPECT_LE(sol.sqp_stationarity, 1.0) << "the caller-unit ceiling holds at the answer";
}

// ===========================================================================
// THE SHIPPED DEFAULT (M7 W2 SCW): kAuto at 2^26. Below the gate it is the kOff solve
// plus one Jacobian read per row block, bit for bit.
// ===========================================================================
TEST(ColumnScalingDefault, BelowTheGateTheDefaultIsTheOffSolvePlusTheProbe) {
    test_support::Hs14Model base;
    CountingModel off_model(base), on_model(base);
    const SqpResult a = SqpSolver{off_options()}.solve(off_model, base.start_point());
    const SqpResult b = SqpSolver{SqpOptions{}}.solve(on_model, base.start_point());
    ASSERT_FALSE(b.scaling.columns_active);
    EXPECT_FALSE(b.scaling.active);
    EXPECT_EQ(a.status, b.status);
    EXPECT_TRUE(a.x.isApprox(b.x, 0.0));
    EXPECT_TRUE(a.lambda_i.isApprox(b.lambda_i, 0.0));
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.evals_full, b.counters.evals_full);
    EXPECT_EQ(a.counters.evals_values + 1, b.counters.evals_values);
    EXPECT_EQ(off_model.values, on_model.values);
    EXPECT_EQ(off_model.grads, on_model.grads);
    EXPECT_EQ(off_model.hess, on_model.hess);
    EXPECT_EQ(off_model.jacs + 2, on_model.jacs);
    EXPECT_EQ(a.warm_start.column_factors.size(), 0);
    ASSERT_EQ(b.warm_start.column_factors.size(), base.n());
    EXPECT_TRUE((b.warm_start.column_factors.array() == 1.0).all()) << "decided: unscaled";
}

// ===========================================================================
// THE ENGAGED ROW LAYER DOES NOT AMPLIFY THE OBJECTIVE (M7 W2 SCW). kAuto's own
// engagement takes IPOPT's one-sided objective rule; the explicit toggle keeps the
// two-sided one.
// ===========================================================================
// The caller's model with its objective weighted by w (gradient and Hessian with it).
class WeightedObjective final : public NlpModel {
  public:
    WeightedObjective(const NlpModel &m, double w) : m_(m), w_(w) {}
    Index n() const override { return m_.n(); }
    Index me() const override { return m_.me(); }
    Index mi() const override { return m_.mi(); }
    double eval_f(const Vec &x) const override { return w_ * m_.eval_f(x); }
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    Vec eval_grad(const Vec &x) const override { return w_ * m_.eval_grad(x); }
    Vec eval_ce(const Vec &x) const override { return m_.eval_ce(x); }
    Vec eval_ci(const Vec &x) const override { return m_.eval_ci(x); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &a, const Vec &b) const override {
        return m_.eval_hess(x, w_ * obj_scale, a, b);
    }
    SpMatRM eval_jac_e(const Vec &x) const override { return m_.eval_jac_e(x); }
    SpMatRM eval_jac_i(const Vec &x) const override { return m_.eval_jac_i(x); }
    HVEN_SUPPRESS_DEPRECATED_END
    const Vec &lower() const override { return m_.lower(); }
    const Vec &upper() const override { return m_.upper(); }
    Vec start_point() const override { return m_.start_point(); }

  private:
    const NlpModel &m_;
    double w_;
};

TEST(ColumnScalingDriver, TheEngagedRowLayerNeverAmplifiesTheObjective) {
    detail::ScalingRule rule;
    const Vec small = Vec::Constant(2, 1e-3);
    const SpMatRM none(0, 2);
    EXPECT_DOUBLE_EQ(1e5, detail::compute_problem_scaling(small, none, none, rule).obj);
    rule.amplify_objective = false;
    EXPECT_DOUBLE_EQ(1.0, detail::compute_problem_scaling(small, none, none, rule).obj);
    const Vec large = Vec::Constant(2, 1e4);
    EXPECT_DOUBLE_EQ(1e-2, detail::compute_problem_scaling(large, none, none, rule).obj)
        << "shrinking is unchanged";

    // The fit with its objective weighted 1e-4: the column-scaled start gradient is
    // then ~1e-2, far below max_gradient = 100, which the two-sided rule amplifies.
    const PolyFitModel fit;
    const WeightedObjective model(fit, 1e-4);
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    const SqpResult engaged = SqpSolver{opts}.solve(model, model.start_point());
    ASSERT_TRUE(engaged.scaling.columns_active);
    ASSERT_TRUE(engaged.scaling.active);
    EXPECT_DOUBLE_EQ(1.0, engaged.scaling.obj) << "not amplified";
    opts.enable_scaling = true;
    const SqpResult asked = SqpSolver{opts}.solve(model, model.start_point());
    ASSERT_TRUE(asked.scaling.active);
    EXPECT_GT(asked.scaling.obj, 1.0) << "the caller's own toggle keeps the two-sided rule";
}

// The ceiling's divisor carries the objective factor: a hidden caller-unit gradient is
// measured against obj * d(i), so dropping obj from it would pass this solve.
TEST(ScaledConvergenceGuard, TheCeilingDividesByTheObjectiveFactorOnTheDefaultPath) {
    HiddenGradientModel base;
    const WeightedObjective model(base, 1e4);
    SqpSolver plain{off_options()};
    const SqpResult cold = plain.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);
    SqpWarmStart warm = cold.warm_start;
    warm.lambda_e(0) += 2e-15; // caller-unit grad L in x0: 1e16 * 2e-15 = 20

    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    opts.common.start_level = StartLevel::kWarm;
    // The row layer reads its gradient at x0: x1 = 1 puts it at 1e4, above max_gradient.
    const Vec x_start = (Vec(2) << 0.0, 1.0).finished();
    const SqpResult sol = SqpSolver{opts}.solve(model, x_start, warm, SolveBudget{});
    ASSERT_TRUE(sol.scaling.columns_active);
    ASSERT_TRUE(sol.scaling.active);
    EXPECT_LT(sol.scaling.obj, 1.0) << "the premise: the row layer shrank the objective";
    EXPECT_GE(sol.counters.scaled_convergence_refusals, 1)
        << "scaled stationarity / (obj * d) is 20 in the caller's units";
}

// ===========================================================================
// THE DECISION TRAVELS WITH THE WARM START (M7 W2 SCW, SqpWarmStart::column_factors).
// ===========================================================================
TEST(ColumnScalingWarmChain, AFiredDecisionIsCarriedWithoutAProbe) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    const SqpResult cold = SqpSolver{opts}.solve(model, model.start_point());
    ASSERT_TRUE(cold.scaling.columns_active);
    const Vec &d = cold.warm_start.column_factors;
    ASSERT_EQ(model.n(), d.size());
    EXPECT_FALSE((d.array() == 1.0).all());
    for (Index j = 0; j < d.size(); ++j) {
        EXPECT_TRUE(is_power_of_two(d(j))) << j;
    }

    opts.common.start_level = StartLevel::kWarm;
    const SqpResult carried =
        SqpSolver{opts}.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
    SqpWarmStart cleared = cold.warm_start;
    cleared.column_factors = Vec();
    const SqpResult reread =
        SqpSolver{opts}.solve(model, model.start_point(), cleared, SolveBudget{});
    ASSERT_TRUE(carried.scaling.columns_active);
    ASSERT_TRUE(reread.scaling.columns_active);
    EXPECT_TRUE(carried.warm_start.column_factors.isApprox(d, 0.0)) << "the chain keeps d";
    EXPECT_EQ(reread.counters.evals_values, carried.counters.evals_values + 1)
        << "a carried decision costs no probe";
    EXPECT_EQ(SolveStatus::kOptimal, carried.status);
}

TEST(ColumnScalingWarmChain, AnUnscaledDecisionIsNotReReadAtTheWarmPoint) {
    BilinearRowModel model;
    const Vec far = (Vec(2) << 0.01, 200.0).finished();
    ASSERT_GE(rule_spread_at(model, far), kFixtureGate) << "the premise: far fires";
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    opts.column_scaling_threshold = kFixtureGate;
    const SqpResult cold = SqpSolver{opts}.solve(model, model.start_point());
    ASSERT_FALSE(cold.scaling.columns_active);
    ASSERT_EQ(2, cold.warm_start.column_factors.size());
    SqpWarmStart warm = cold.warm_start;
    warm.x = far;

    opts.common.start_level = StartLevel::kWarm;
    const SqpResult w = SqpSolver{opts}.solve(model, model.start_point(), warm, SolveBudget{});
    EXPECT_NE(StartLevel::kCold, w.counters.start_level_used);
    EXPECT_FALSE(w.scaling.columns_active) << "the chain's decision, not the far point's";
    EXPECT_DOUBLE_EQ(1.0, w.scaling.col_spread);

    // A record this problem cannot use is ignored: the rule runs at warm.x, here the
    // cold answer, where it does not fire -- so a bad record taken would show. The
    // subnormal and huge powers of two are outside [1/limit, limit]: taken, they
    // would overflow the scaled start.
    for (const Vec &bad : {Vec(Vec::Constant(1, 0.5)), Vec((Vec(2) << 1.0, 3.0).finished()),
                           Vec((Vec(2) << 0.5, -2.0).finished()),
                           Vec((Vec(2) << 1.0, std::ldexp(1.0, -1074)).finished()),
                           Vec((Vec(2) << std::ldexp(1.0, 1000), 1.0).finished())}) {
        SqpWarmStart b = cold.warm_start;
        b.column_factors = bad;
        const SqpResult r = SqpSolver{opts}.solve(model, model.start_point(), b, SolveBudget{});
        EXPECT_FALSE(r.scaling.columns_active) << bad.transpose();
        EXPECT_DOUBLE_EQ(rule_spread_at(model, cold.warm_start.x), r.scaling.col_spread)
            << bad.transpose();
    }

    // At a kCold ceiling the ingest takes nothing, the record included: the rule reads x0.
    opts.common.start_level = StartLevel::kCold;
    SqpWarmStart fired = warm;
    fired.column_factors = (Vec(2) << 1.0, 1.0 / 128.0).finished();
    const SqpResult c = SqpSolver{opts}.solve(model, model.start_point(), fired, SolveBudget{});
    EXPECT_FALSE(c.scaling.columns_active);
    EXPECT_DOUBLE_EQ(rule_spread_at(model, model.start_point()), c.scaling.col_spread);

    // kOff never reads it, and records nothing.
    SqpOptions off = off_options();
    off.common.start_level = StartLevel::kWarm;
    const SqpResult o = SqpSolver{off}.solve(model, model.start_point(), fired, SolveBudget{});
    EXPECT_FALSE(o.scaling.columns_active);
    EXPECT_EQ(0, o.warm_start.column_factors.size());
}

} // namespace
} // namespace hven::solvers
