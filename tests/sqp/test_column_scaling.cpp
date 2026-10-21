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
//       reports in the caller's units against an independent measurement; below
//       the gate kAuto is the kOff solve (plus the rule's one evaluation); the
//       iteration event's x and z are the caller's; the warm start crosses both
//       ways.
//   (e) THE GUARD: a row the row layer shrank, and a box violation a column factor
//       hid, are each refused as a convergence and the solve goes on to a point
//       that is feasible in the caller's units.

#include <cmath>
#include <cstdio>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/drivers/column_scaling.h>
#include <hven/drivers/sqp_solver.h>

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

TEST(ColumnScalingOption, TheShippedDefaultIsOff) {
    const SqpOptions o;
    EXPECT_EQ(SqpColumnScaling::kOff, o.column_scaling);
    EXPECT_FALSE(o.enable_scaling) << "the row layer stays opt-in too";
    EXPECT_DOUBLE_EQ(1e3, o.column_scaling_threshold);
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
    EXPECT_EQ(a.counters.evals_values, b.counters.evals_values);
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

TEST(ColumnScalingDriver, TheScaledSolveIsTheUnscaledSolveOfTheScaledModel) {
    ScaledInfeasibleModel model;
    const Vec x0 = model.start_point();
    // The rule reads the start PROJECTED onto the box, as the solve starts there:
    // x0(0) = 2 is above its cap 0.5.
    SpMatRM je;
    model.eval_jac_e_in_place(x0.cwiseMax(model.lower()).cwiseMin(model.upper()), je);
    ColumnScalingRule rule;
    rule.threshold = 1.0;
    const ColumnScaling cs = compute_column_scaling(2, je, SpMatRM(0, 2), rule);
    ASSERT_TRUE(cs.active);
    ASSERT_NE(cs.cols(0), cs.cols(1));

    SqpOptions opts = off_options();
    opts.max_iter = 60;
    SqpSolver plain{opts};
    const ColumnScaledModel scaled_model(model, cs.cols);
    const SqpResult ref = plain.solve(scaled_model, scaled_model.to_scaled(x0));

    opts.column_scaling = SqpColumnScaling::kAlways;
    SqpSolver driver{opts};
    const SqpResult sol = driver.solve(model, x0);
    ASSERT_TRUE(sol.scaling.columns_active);
    ASSERT_GE(ref.counters.restoration_iters, 1) << "the premise: restoration runs";

    EXPECT_EQ(ref.status, sol.status);
    EXPECT_EQ(ref.counters.major_iters, sol.counters.major_iters);
    EXPECT_EQ(ref.counters.restoration_iters, sol.counters.restoration_iters);
    EXPECT_EQ(ref.counters.restoration_entries, sol.counters.restoration_entries);
    EXPECT_EQ(ref.counters.qp_minor_iters, sol.counters.qp_minor_iters);
    EXPECT_EQ(ref.counters.factorizations, sol.counters.factorizations);
    EXPECT_EQ(ref.counters.restoration_h_out, sol.counters.restoration_h_out);
    EXPECT_EQ(ref.history.size(), sol.history.size());
    EXPECT_TRUE(sol.x.isApprox(scaled_model.to_caller(ref.x), 0.0));
    EXPECT_TRUE(sol.lambda_e.isApprox(ref.lambda_e, 0.0));
    if (sol.status == SolveStatus::kInfeasible) {
        // THE CERTIFICATE'S BOUND PRICE is the feasibility problem's, mapped: z = z~ / d.
        ASSERT_EQ(2, sol.z.size());
        EXPECT_TRUE(sol.z.isApprox(Vec(ref.z.cwiseQuotient(cs.cols)), 0.0))
            << sol.z.transpose() << " vs " << ref.z.transpose();
        std::printf("[ SCL ] infeasible certificate: z = (%g, %g)\n", sol.z(0), sol.z(1));
    }
    std::printf("[ SCL ] scaled-model equivalence: status %d majors %ld resto %ld\n",
                static_cast<int>(sol.status), static_cast<long>(sol.counters.major_iters),
                static_cast<long>(sol.counters.restoration_iters));
}

TEST(ColumnScalingDriver, TheWarmStartCrossesBothWays) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    SqpSolver first{opts};
    const SqpResult cold = first.solve(model, model.start_point());
    ASSERT_EQ(SolveStatus::kOptimal, cold.status);

    // INTO A COLUMN-SCALED SOLVE: x / d, z * d on the way in.
    SqpOptions warm_opts = opts;
    warm_opts.common.start_level = StartLevel::kWarm;
    SqpSolver second{warm_opts};
    const SqpResult again =
        second.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
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

TEST(ColumnScalingDriver, TheRuleReadsTheWarmPointWhenOneIsCarried) {
    // A warm point at the solution of the fit and a cold x0 far from it give the
    // same Jacobian (the rows are linear), so the factors agree; what this pins is
    // that a warm solve with an INVALID warm object reads x0 and still scales.
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    SqpSolver driver{opts};
    SqpWarmStart invalid;
    const SqpResult sol = driver.solve(model, model.start_point(), invalid, SolveBudget{});
    ASSERT_EQ(SolveStatus::kOptimal, sol.status);
    EXPECT_TRUE(sol.scaling.columns_active);
}

TEST(ColumnScalingDriver, TheArgumentRefusalsAreSolveImplsOwn) {
    PolyFitModel model;
    SqpOptions opts;
    opts.column_scaling = SqpColumnScaling::kAuto;
    SqpSolver driver{opts};
    EXPECT_THROW(driver.solve(model, Vec::Zero(5)), std::invalid_argument);
    Vec bad = Vec::Zero(6);
    bad(1) = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(driver.solve(model, bad), std::invalid_argument);
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

} // namespace
} // namespace hven::solvers
