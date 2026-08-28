// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_problem_scaling.cpp — the M6 W0.2 problem-scaling layer.
//
// WHAT THIS FILE PINS, in the order the task's own acceptance list gives:
//
//   (a) THE ACCEPTANCE CELL. The S = 1e12 dual-scale instance -- the one
//       test_b1_gate.cpp's fixture banner records as running out of majors
//       (60/60, "a pre-existing scaling limit of the driver") -- converges with
//       the layer on.
//   (b) HS25, the battery's excused row, MEASURED with the layer on. Whatever
//       it does is recorded here as an observation, not asserted as a win.
//   (c) THE OFF PATH IS THE OLD PATH. At the shipped default nothing is
//       installed and nothing is scaled, pinned as an exact equality of every
//       reported quantity against a driver built before this option existed --
//       which, the option being default-false, is any driver at all.
//   (d) THE ROUND TRIP. A scaled solve reports its objective, its three
//       multiplier blocks and its four residuals on the CALLER's scale, checked
//       against an independent re-measurement of the caller's own problem.
//   (e) THE CURRENCY SURVIVES THE BOUNDARY. A warm start exported from a scaled
//       solve is accepted by an UNSCALED solve of the same model, and lands in
//       the same place.
//
// Plus the unit-level pins on the factor rule itself, which is where a
// regression would be cheapest to catch.

#include <cmath>
#include <limits>
#include <string>

#include <gtest/gtest.h>

#include <hven/detail/drivers/problem_scaling.h>
#include <hven/drivers/sqp_driver.h>

#include "support/claim_stream_double.h"
#include "support/hs_problems.h"

namespace hven::solvers {
namespace {

using hven::solvers::detail::ProblemScaling;
using hven::solvers::detail::ScalingRule;

// ===========================================================================
// THE ACCEPTANCE MODEL, copied field for field from test_b1_gate.cpp's
// B1ScaledStalePrice.
//
//     min  -S*x + x^2/2   s.t.  x - eps <= 0,  -1 <= x <= 1
//
// It is copied rather than shared because the two files pin different things
// about it and a shared fixture would couple a W1 gate pin to a W0.2 scaling
// pin. THE SHAPE IS THE WHOLE POINT: the inequality Jacobian is the constant
// [1], i.e. ALREADY perfectly row-equilibrated, so no amount of row scaling
// touches this cell. What is 1e12 out of scale is the objective gradient
// (grad f(0) = -S), and with it the dual. This cell is what establishes that
// the layer's objective half is load-bearing and not decoration.
// ===========================================================================
class ScaledDualModel : public NlpModel {
  public:
    ScaledDualModel(double scale, double eps) : s_(scale), eps_(eps) {}
    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return -s_ * x(0) + 0.5 * x(0) * x(0); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, -s_ + x(0)); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(0) - eps_); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
        h.insert(0, 0) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 1); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 1);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(1, 0.0); }

    // x* = eps: the row binds, since the unconstrained minimizer sits at x = S.
    double x_star() const { return eps_; }
    double f_star() const { return -s_ * eps_ + 0.5 * eps_ * eps_; }

  private:
    double s_, eps_;
    Vec lower_ = Vec::Constant(1, -1.0);
    Vec upper_ = Vec::Constant(1, 1.0);
};

// A two-row model with genuinely uneven ROW norms, which the cell above
// deliberately does not have. Row 0's Jacobian is O(1e6) and row 1's is O(1),
// so the row half of the rule has something to do and its factors are
// hand-computable.
class UnevenRowModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return 0.5 * (x(0) * x(0) + x(1) * x(1)); }
    Vec eval_grad(const Vec &x) const override { return x; }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 1.0e6 * (x(0) - 1.0)); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(1) - 2.0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = obj_scale;
        h.insert(1, 1) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0e6;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(2, 0.0); }

  private:
    Vec lower_ = Vec::Constant(2, -10.0);
    Vec upper_ = Vec::Constant(2, 10.0);
};

// A model whose OBJECTIVE and whose EQUALITY ROW are both out of scale, which
// neither fixture above is: the acceptance cell has a perfectly scaled row and
// UnevenRowModel has grad f(x0) == 0 (so its objective factor is the identity).
// Both halves of the map are therefore live here, which is what makes it the
// fixture for the HISTORY export -- a row exported at engine scale is off by
// 1e4 in f and by 1e-4 in the violation, in opposite directions.
//
//     min  1e6*(0.5*(x0-1)^2 + 0.5*x1^2)
//     s.t. 1e6*(x0 - 0.5) = 0,  x1 - 2 <= 0,  -10 <= x <= 10,  x_start = 0
//
// At the start point: |grad f|inf = 1e6 -> sf = 1e-4; the equality row's
// Jacobian norm is 1e6 -> 1e-4; the inequality row's is 1 -> 1.0 (one-sided).
// The solution is x = (0.5, 0), f* = 125000.
class ScaledUnevenModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override {
        return 1.0e6 * (0.5 * (x(0) - 1.0) * (x(0) - 1.0) + 0.5 * x(1) * x(1));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 1.0e6 * (x(0) - 1.0), 1.0e6 * x(1);
        return g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec::Constant(1, 1.0e6 * (x(0) - 0.5)); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x(1) - 2.0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 1.0e6 * obj_scale;
        h.insert(1, 1) = 1.0e6 * obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0e6;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lower_ = Vec::Constant(2, -10.0);
    Vec upper_ = Vec::Constant(2, 10.0);
};

// A model the driver can DIFFERENTIATE at the start point but cannot MEASURE
// there: the derivatives are finite (so the factor rule has real data and
// produces sf = 1e-4), while the inequality residual is NaN (so the very first
// evaluate_kkt reports `finite == false` and the driver takes its non-finite
// iterate exit). That exit is the one path out of solve_impl that does not
// route through `finish`, which is exactly what makes it a scaling pin.
class NonFiniteRowModel : public NlpModel {
  public:
    Index n() const override { return 1; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override { return 5.0e5 + 1.0e6 * x(0) + 0.5 * x(0) * x(0); }
    Vec eval_grad(const Vec &x) const override { return Vec::Constant(1, 1.0e6 + x(0)); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override {
        return Vec::Constant(1, std::numeric_limits<double>::quiet_NaN());
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(1, 1);
        h.insert(0, 0) = obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 1); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 1);
        j.insert(0, 0) = 1.0;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(1); }

  private:
    Vec lower_ = Vec::Constant(1, -10.0);
    Vec upper_ = Vec::Constant(1, 10.0);
};

// THE RESTORATION FIXTURE, copied field for field from test_sqp_restoration.cpp's
// InfeasibleCircleLineModel (a circle and a line that do not meet), for the same
// reason ScaledDualModel is copied above: that file pins the certificate itself
// and this one pins what the SCALING LAYER does to the exit that carries it.
//
//     min  x0 + x1   s.t.  x0^2 + x1^2 - 1 = 0,  x0 + x1 - 2 = 0
//
// grad f is (1, 1) everywhere, so the objective factor is the full 100 while
// both row factors are the identity (the row rule never amplifies) -- which is
// precisely the shape that catches an ENGINE->CALLER map applied to multipliers
// that never entered the engine's space: the selectors would come back 100x
// small.
class InfeasibleCircleLineModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return x(0) + x(1); }
    Vec eval_grad(const Vec &) const override { return Vec::Ones(2); }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) + x(1) * x(1) - 1.0, x(0) + x(1) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double, const Vec &lambda_e, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 2.0 * lambda_e(0);
        h.insert(1, 1) = 2.0 * lambda_e(0);
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM j(2, 2);
        j.insert(0, 0) = 2.0 * x(0);
        j.insert(0, 1) = 2.0 * x(1);
        j.insert(1, 0) = 1.0;
        j.insert(1, 1) = 1.0;
        j.makeCompressed();
        return j;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }

  private:
    // The model is unbounded in both coordinates; 1e20 is this suite's
    // convention for "no bound" (test_sqp_restoration.cpp uses the same value).
    Vec lower_ = Vec::Constant(2, -1e20);
    Vec upper_ = Vec::Constant(2, 1e20);
};

// A model whose solution sits ON A BOUND whose price carries a JACOBIAN term,
// which neither fixture above does: both of theirs converge to an interior
// point, where z is identically zero and any treatment of it looks right.
//
//     min  1e6*(0.5*(x0-3)^2 + 0.5*(x1-3)^2)
//     s.t. 1e6*(x0 + 0.3*x1 - 0.5) <= 0,  -10 <= x <= (10, 1),  x_start = 0
//
// At the start point |grad f|inf = 3e6 -> sf = 100/3e6, and the row's Jacobian
// norm is 1e6 -> 1e-4. Hand-derived solution: both the row and x1's upper bound
// bind, so x* = (0.2, 1), the row price is 2.8, and
// z1* = grad f(1) + 3e5*2.8 = -2e6 + 8.4e5 = -1.16e6 -- a bound price with a
// Jacobian term in it, which is what makes a divide-back and a re-measurement
// numerically distinguishable at all.
class ScaledBoundActiveModel : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }
    double eval_f(const Vec &x) const override {
        return 1.0e6 * (0.5 * (x(0) - 3.0) * (x(0) - 3.0) + 0.5 * (x(1) - 3.0) * (x(1) - 3.0));
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 1.0e6 * (x(0) - 3.0), 1.0e6 * (x(1) - 3.0);
        return g;
    }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        return Vec::Constant(1, 1.0e6 * (x(0) + 0.3 * x(1) - 0.5));
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        SpMatRM h(2, 2);
        h.insert(0, 0) = 1.0e6 * obj_scale;
        h.insert(1, 1) = 1.0e6 * obj_scale;
        h.makeCompressed();
        return h;
    }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 2); }
    SpMatRM eval_jac_i(const Vec &) const override {
        SpMatRM j(1, 2);
        j.insert(0, 0) = 1.0e6;
        j.insert(0, 1) = 3.0e5;
        j.makeCompressed();
        return j;
    }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lower_ = Vec::Constant(2, -10.0);
    Vec upper_ = (Vec(2) << 10.0, 1.0).finished();
};

SpMatRM row_matrix(std::initializer_list<std::initializer_list<double>> rows) {
    const Index m = static_cast<Index>(rows.size());
    const Index n = m == 0 ? 0 : static_cast<Index>(rows.begin()->size());
    SpMatRM out(m, n);
    Index r = 0;
    for (const auto &row : rows) {
        Index c = 0;
        for (const double v : row) {
            if (v != 0.0) {
                out.insert(r, c) = v;
            }
            ++c;
        }
        ++r;
    }
    out.makeCompressed();
    return out;
}

} // namespace

// ===========================================================================
// THE FACTOR RULE (detail/drivers/problem_scaling.h).
// ===========================================================================

TEST(ProblemScalingRule, TheObjectiveFactorAimsTheGradientAtMaxGradient) {
    ScalingRule rule;
    rule.max_gradient = 100.0;
    rule.factor_limit = 1e12;
    const Vec grad = Vec::Constant(2, 1.0e6);
    const ProblemScaling s =
        detail::compute_problem_scaling(grad, SpMatRM(0, 2), SpMatRM(0, 2), rule);
    EXPECT_TRUE(s.active);
    EXPECT_DOUBLE_EQ(1.0e-4, s.obj) << "100 / 1e6";
    EXPECT_DOUBLE_EQ(100.0, (s.obj * grad).cwiseAbs().maxCoeff()) << "the aim is hit exactly";
}

TEST(ProblemScalingRule, TheObjectiveRuleIsTwoSidedAndScalesASmallGradientUp) {
    // THE DEPARTURE FROM IPOPT, pinned: a one-sided min(1, g_max/||g||) rule
    // would return 1.0 here and leave HS25's false certificate standing.
    ScalingRule rule;
    const ProblemScaling s = detail::compute_problem_scaling(Vec::Constant(1, 1.0e-11),
                                                             SpMatRM(0, 1), SpMatRM(0, 1), rule);
    EXPECT_GT(s.obj, 1.0);
    EXPECT_DOUBLE_EQ(1.0e12, s.obj) << "100 / 1e-11 = 1e13, CLAMPED at the default limit";
}

TEST(ProblemScalingRule, TheRowRuleIsOneSidedAndNeverAmplifiesARow) {
    // THE ASYMMETRY, pinned. A row whose Jacobian is small at the start point is
    // left EXACTLY alone rather than amplified: amplifying it would multiply its
    // residual by up to factor_limit and manufacture infeasibility out of a row
    // that has none. Large rows are still shrunk, which is what equilibration is
    // actually for.
    ScalingRule rule;
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0), row_matrix({{1.0e-9}, {1.0}, {1.0e6}}), SpMatRM(0, 1), rule);
    ASSERT_EQ(3, s.eq_rows.size());
    EXPECT_DOUBLE_EQ(1.0, s.eq_rows(0)) << "tiny row: untouched, NOT amplified by 1e11";
    EXPECT_DOUBLE_EQ(1.0, s.eq_rows(1)) << "a norm below max_gradient is already fine";
    EXPECT_DOUBLE_EQ(1.0e-4, s.eq_rows(2)) << "100 / 1e6: large rows ARE shrunk";
}

TEST(ProblemScalingRule, TheClampIsSymmetricAndHonoured) {
    ScalingRule rule;
    rule.max_gradient = 1.0;
    rule.factor_limit = 10.0;
    EXPECT_DOUBLE_EQ(0.1, detail::compute_problem_scaling(Vec::Constant(1, 1.0e30), SpMatRM(0, 1),
                                                          SpMatRM(0, 1), rule)
                              .obj);
    EXPECT_DOUBLE_EQ(10.0, detail::compute_problem_scaling(Vec::Constant(1, 1.0e-30), SpMatRM(0, 1),
                                                           SpMatRM(0, 1), rule)
                               .obj)
        << "the ceiling binds on the OBJECTIVE, which is the two-sided half";
}

TEST(ProblemScalingRule, ARowWithNoDataTakesTheIdentityRatherThanTheCeiling) {
    // The guard that matters: a structurally empty row has no scale to read, and
    // amplifying it by the clamp's ceiling on the strength of having no data in
    // it would manufacture a 1e12 residual out of nothing.
    ScalingRule rule;
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0), row_matrix({{0.0, 0.0}, {4.0, 0.0}}), SpMatRM(0, 2), rule);
    ASSERT_EQ(2, s.eq_rows.size());
    EXPECT_DOUBLE_EQ(1.0, s.eq_rows(0)) << "empty row -> identity, never the ceiling";
    EXPECT_DOUBLE_EQ(1.0, s.eq_rows(1)) << "norm 4 is below max_gradient; the row rule leaves it";
}

TEST(ProblemScalingRule, ANonFiniteNormTakesTheIdentityToo) {
    ScalingRule rule;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_DOUBLE_EQ(1.0, detail::compute_problem_scaling(Vec::Constant(1, inf), SpMatRM(0, 1),
                                                          SpMatRM(0, 1), rule)
                              .obj);
    EXPECT_DOUBLE_EQ(1.0, detail::compute_problem_scaling(Vec::Constant(1, nan), SpMatRM(0, 1),
                                                          SpMatRM(0, 1), rule)
                              .obj);
}

TEST(ProblemScalingRule, RowExtremesFoldOverBothBlocksAndDefaultToIdentity) {
    ScalingRule rule;
    const ProblemScaling s = detail::compute_problem_scaling(
        Vec::Constant(1, 1.0), row_matrix({{1.0e4}}), row_matrix({{1.0e8}}), rule);
    EXPECT_DOUBLE_EQ(1.0e-2, s.row_max()) << "the equality row, 100 / 1e4";
    EXPECT_DOUBLE_EQ(1.0e-6, s.row_min()) << "the inequality row, 100 / 1e8";

    // No rows at all -- a bound-constrained problem -- reports the identity
    // rather than an empty-range sentinel.
    const ProblemScaling none =
        detail::compute_problem_scaling(Vec::Constant(1, 1.0), SpMatRM(0, 1), SpMatRM(0, 1), rule);
    EXPECT_DOUBLE_EQ(1.0, none.row_max());
    EXPECT_DOUBLE_EQ(1.0, none.row_min());
}

// ===========================================================================
// PIN (c): THE OFF PATH IS THE OLD PATH.
// ===========================================================================

TEST(ProblemScalingOffPath, TheDefaultIsOffAndReportsTheIdentity) {
    const SqpOptions defaults;
    EXPECT_FALSE(defaults.enable_scaling) << "THE SHIPPED DEFAULT; flipping it is an M8 decision";
    EXPECT_DOUBLE_EQ(100.0, defaults.scaling_max_gradient);
    EXPECT_DOUBLE_EQ(1e12, defaults.scaling_factor_limit);

    UnevenRowModel model;
    SqpDriver driver{SqpOptions{}};
    const SqpSolution sol = driver.solve(model);
    EXPECT_FALSE(sol.scaling.active);
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.obj);
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_max);
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_min);
    // On an unscaled solve the two residuals are the SAME measurement, not two
    // that happen to agree -- the scaled one is copied from the gate's own read.
    EXPECT_DOUBLE_EQ(sol.kkt_residual, sol.scaling.scaled_kkt_residual);
}

TEST(ProblemScalingOffPath, SettingTheRuleWithoutTheToggleChangesNothing) {
    // The toggle is the ONLY thing that arms the layer: a caller who tunes the
    // rule and forgets the switch gets exactly the solve they had before, bit
    // for bit -- which is what makes "default OFF" a claim about arithmetic
    // rather than about intent.
    UnevenRowModel model;
    SqpDriver base{SqpOptions{}};
    const SqpSolution a = base.solve(model);

    SqpOptions tuned;
    tuned.scaling_max_gradient = 3.0;
    tuned.scaling_factor_limit = 5.0;
    ASSERT_FALSE(tuned.enable_scaling);
    SqpDriver other{tuned};
    const SqpSolution b = other.solve(model);

    EXPECT_EQ(a.status, b.status);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.evals_full, b.counters.evals_full);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_DOUBLE_EQ(a.f, b.f);
    EXPECT_DOUBLE_EQ(a.kkt_residual, b.kkt_residual);
    EXPECT_DOUBLE_EQ(a.stationarity, b.stationarity);
    EXPECT_DOUBLE_EQ(a.feasibility, b.feasibility);
    EXPECT_DOUBLE_EQ(a.complementarity, b.complementarity);
    ASSERT_EQ(a.x.size(), b.x.size());
    EXPECT_TRUE(a.x.isApprox(b.x, 0.0)) << "EXACT equality, not approximate";
    EXPECT_TRUE(a.lambda_e.isApprox(b.lambda_e, 0.0));
    EXPECT_TRUE(a.lambda_i.isApprox(b.lambda_i, 0.0));
    EXPECT_TRUE(a.z.isApprox(b.z, 0.0));
}

TEST(ProblemScalingOptions, TheRuleIsValidatedAtTheBoundary) {
    // Validated UNCONDITIONALLY -- an options object is a value whose toggle may
    // be flipped later, so a nonsensical rule is refused when it is set.
    SqpOptions o;
    o.scaling_max_gradient = 0.0;
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.scaling_max_gradient = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);
    o.scaling_max_gradient = std::numeric_limits<double>::infinity();
    EXPECT_THROW(validate_sqp_options(o), std::invalid_argument);

    SqpOptions p;
    p.scaling_factor_limit = 0.5;
    EXPECT_THROW(validate_sqp_options(p), std::invalid_argument)
        << "a limit below 1 inverts [1/L, L]";
    p.scaling_factor_limit = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(validate_sqp_options(p), std::invalid_argument);
    p.scaling_factor_limit = 1.0;
    EXPECT_NO_THROW(validate_sqp_options(p)) << "exactly 1 is the identity clamp, and is legal";
}

// ===========================================================================
// PIN (a): THE ACCEPTANCE CELL.
// ===========================================================================

TEST(ProblemScalingAcceptance, TheS1e12DualScaleCellConvergesWithScalingOn) {
    ScaledDualModel model(1e12, 5e-7);

    SqpOptions off;
    off.max_iter = 60;
    SqpDriver cold_off{off};
    const SqpSolution before = cold_off.solve(model, model.start_point());

    SqpOptions on = off;
    on.enable_scaling = true;
    SqpDriver cold_on{on};
    const SqpSolution after = cold_on.solve(model, model.start_point());

    // THE MEASUREMENT, printed so the record carries it rather than only the
    // verdict.
    std::printf("[ W0.2 ] S=1e12 OFF: status=%d majors=%lld f=%.15g kkt=%.6g\n",
                static_cast<int>(before.status),
                static_cast<long long>(before.counters.major_iters), before.f, before.kkt_residual);
    std::printf("[ W0.2 ] S=1e12 ON : status=%d majors=%lld f=%.15g kkt=%.6g scaled_kkt=%.6g "
                "sf=%.6g\n",
                static_cast<int>(after.status), static_cast<long long>(after.counters.major_iters),
                after.f, after.kkt_residual, after.scaling.scaled_kkt_residual, after.scaling.obj);

    // THE PIN. The instance the fixture banner in test_b1_gate.cpp records as
    // running out of majors converges here.
    EXPECT_EQ(SqpStatus::kOptimal, after.status);
    EXPECT_TRUE(after.scaling.active);
    EXPECT_LT(after.counters.major_iters, 60) << "it no longer exhausts the budget";
    // AT THE RIGHT ANSWER, on the caller's scale: x* = eps and f* = -S*eps.
    ASSERT_EQ(1, after.x.size());
    EXPECT_NEAR(model.x_star(), after.x(0), 1e-9);
    EXPECT_NEAR(model.f_star(), after.f, 1e-3 * std::abs(model.f_star()));
    // The row is active, so its price is the objective's own scale -- reported
    // on the CALLER's scale, i.e. ~S, not ~1.
    ASSERT_EQ(1, after.lambda_i.size());
    EXPECT_NEAR(1e12, after.lambda_i(0), 1e-3 * 1e12);
}

TEST(ProblemScalingAcceptance, TheObjectiveFactorIsTheOneDoingTheWorkOnThatCell) {
    // The cell's row Jacobian is the constant [1], so the row half of the rule
    // is the identity on it and the objective half carries the whole effect.
    // Pinned so a future change that moves the benefit into row scaling has to
    // say so.
    ScaledDualModel model(1e12, 5e-7);
    SqpOptions on;
    on.max_iter = 60;
    on.enable_scaling = true;
    SqpDriver driver{on};
    const SqpSolution sol = driver.solve(model, model.start_point());
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_max) << "already row-equilibrated, and left alone";
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_min);
    EXPECT_DOUBLE_EQ(1.0e-10, sol.scaling.obj) << "100 / 1e12";
}

// ===========================================================================
// PIN (b): HS25, MEASURED.
// ===========================================================================

TEST(ProblemScalingAcceptance, Hs25IsMeasuredWithScalingOn) {
    test_support::Hs25Model model;

    SqpDriver off{SqpOptions{}};
    const SqpSolution before = off.solve(model, model.start_point());

    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver on{opts};
    const SqpSolution after = on.solve(model, model.start_point());

    std::printf("[ W0.2 ] HS25 OFF: status=%d majors=%lld f=%.15g kkt=%.6g\n",
                static_cast<int>(before.status),
                static_cast<long long>(before.counters.major_iters), before.f, before.kkt_residual);
    std::printf("[ W0.2 ] HS25 ON : status=%d majors=%lld f=%.15g kkt=%.6g scaled_kkt=%.6g "
                "sf=%.6g\n",
                static_cast<int>(after.status), static_cast<long long>(after.counters.major_iters),
                after.f, after.kkt_residual, after.scaling.scaled_kkt_residual, after.scaling.obj);

    // WHAT IS ASSERTED, and it is deliberately NOT "it converges to f* = 0".
    //
    // THE MEASURED OUTCOME, recorded here as the finding rather than paraphrased
    // as a win: scaling ON turns HS25 from a zero-major kOptimal at
    // f = 32.834999999663594 into a THREE-major kNumericalError at
    // f = 27.3633944547233. The objective IMPROVES by ~16.7%, and the false
    // certificate is refused -- but the solve does not converge, and the exit is
    // an error rather than a budget exhaustion.
    //
    // WHY THAT IS THE CEILING HERE, and why no rule change reaches past it: the
    // start point is a numerically exact stationary point of a function that is
    // constant to fourteen digits there (hs_problems.h's banner). The objective
    // factor lifts the GRADIENT into usable units -- which is what refuses the
    // certificate -- but it scales f's own noise floor by exactly the same
    // factor, so the line search is still comparing values that differ below
    // roundoff. This layer can make the solver try; it cannot manufacture
    // information that is not in the double.
    //
    // The battery's own pin on this row is untouched, because the battery runs
    // at the shipped default and the shipped default is OFF.
    EXPECT_TRUE(after.scaling.active);
    EXPECT_GT(after.scaling.obj, 1.0) << "the two-sided objective rule, scaling UP";
    EXPECT_EQ(0, before.counters.major_iters) << "the excused row, unscaled: certified at x0";
    EXPECT_EQ(SqpStatus::kOptimal, before.status);

    EXPECT_GT(after.counters.major_iters, 0)
        << "with the objective in usable units the solver at least TRIES";
    EXPECT_NE(SqpStatus::kOptimal, after.status)
        << "THE POINT: the zero-major certificate at a non-minimum is refused";
    // The objective genuinely improves -- pinned with margin, since this is the
    // only positive claim the cell supports.
    EXPECT_LT(after.f, before.f - 1.0);
}

// ===========================================================================
// PIN (d): THE ROUND TRIP.
// ===========================================================================

TEST(ProblemScalingRoundTrip, AScaledSolveReportsOnTheCallersScale) {
    UnevenRowModel model;
    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(SqpStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.active);
    // The row half of the rule has real work to do on this model, which is why
    // it is the round trip's subject rather than the acceptance cell.
    EXPECT_DOUBLE_EQ(1.0e-4, sol.scaling.row_min) << "100 / 1e6, the equality row, shrunk";
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_max)
        << "the inequality row's norm is 1, already below max_gradient: left alone";

    // THE OBJECTIVE, against the model's own evaluation at the returned point.
    EXPECT_DOUBLE_EQ(model.eval_f(sol.x), sol.f);

    // THE FOUR RESIDUALS, against an INDEPENDENT measurement of the caller's own
    // problem at the returned multipliers -- the model-taking evaluate_kkt,
    // which knows nothing about the seam or its factors.
    const SqpKkt independent =
        evaluate_kkt(model, sol.x, sol.lambda_e, sol.lambda_i, SqpOptions{}.feas_tol);
    EXPECT_DOUBLE_EQ(independent.stationarity, sol.stationarity);
    EXPECT_DOUBLE_EQ(independent.feasibility, sol.feasibility);
    EXPECT_DOUBLE_EQ(independent.complementarity, sol.complementarity);
    EXPECT_DOUBLE_EQ(independent.residual(), sol.kkt_residual);
    ASSERT_EQ(sol.z.size(), independent.z.size());
    // TO THE SAME STANDARD AS THE FOUR RESIDUALS BESIDE IT, not a looser one:
    // `z` is a coordinate of the same `grad_lag` those were folded out of, and
    // since fix round 1 it comes from the SAME caller-scale re-measurement
    // rather than from a divide-back of the engine's own vector.
    for (Index i = 0; i < sol.z.size(); ++i) {
        EXPECT_DOUBLE_EQ(independent.z(i), sol.z(i)) << "bound price " << i;
    }

    // AND THE DISCLOSED GAP IS REPORTED, not hidden: the gate read the scaled
    // residual, and it is carried beside the caller-scale one.
    EXPECT_TRUE(std::isfinite(sol.scaling.scaled_kkt_residual));
    EXPECT_LE(sol.scaling.scaled_kkt_residual, SqpOptions{}.kkt_tol)
        << "the number the convergence test actually gated on";
}

TEST(ProblemScalingRoundTrip, TheMultipliersAreTheCallersNotTheEngines) {
    // The sharpest form of the round trip: solve the SAME model scaled and
    // unscaled, and require the two to report the same multipliers. If the
    // export forgot its map, this is where the sf/s_j factor would show.
    UnevenRowModel model;
    SqpDriver off{SqpOptions{}};
    const SqpSolution a = off.solve(model);

    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver on{opts};
    const SqpSolution b = on.solve(model);

    ASSERT_EQ(SqpStatus::kOptimal, a.status);
    ASSERT_EQ(SqpStatus::kOptimal, b.status);
    EXPECT_NEAR(a.f, b.f, 1e-9 * std::max(1.0, std::abs(a.f)));
    ASSERT_EQ(a.lambda_e.size(), b.lambda_e.size());
    for (Index i = 0; i < a.lambda_e.size(); ++i) {
        EXPECT_NEAR(a.lambda_e(i), b.lambda_e(i), 1e-8 * std::max(1.0, std::abs(a.lambda_e(i))));
    }
    ASSERT_EQ(a.lambda_i.size(), b.lambda_i.size());
    for (Index i = 0; i < a.lambda_i.size(); ++i) {
        EXPECT_NEAR(a.lambda_i(i), b.lambda_i(i), 1e-8 * std::max(1.0, std::abs(a.lambda_i(i))));
    }
}

// ===========================================================================
// PIN (e): THE CURRENCY CROSSES THE BOUNDARY.
// ===========================================================================

TEST(ProblemScalingWarmStart, AScaledSolvesExportIsAcceptedByAnUnscaledSolve) {
    UnevenRowModel model;

    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver scaled{opts};
    const SqpSolution first = scaled.solve(model);
    ASSERT_EQ(SqpStatus::kOptimal, first.status);
    ASSERT_TRUE(first.scaling.active);
    ASSERT_TRUE(first.warm_start.valid);

    // THE HAND-OFF: a warm start built by a SCALED solve, ingested by an
    // UNSCALED one. It must be accepted (not silently degraded to cold) and it
    // must land where a cold unscaled solve lands.
    SqpOptions warm_opts;
    warm_opts.start_level = StartLevel::kWarm;
    SqpDriver consumer{warm_opts};
    const SqpSolution second = consumer.solve(model, model.start_point(), first.warm_start, 0);

    EXPECT_NE(StartLevel::kCold, second.counters.start_level_used)
        << "the currency was ACCEPTED, which is only true if it arrived in the caller's units";
    EXPECT_EQ(SqpStatus::kOptimal, second.status);
    EXPECT_FALSE(second.scaling.active);

    SqpDriver reference{SqpOptions{}};
    const SqpSolution cold = reference.solve(model);
    EXPECT_NEAR(cold.f, second.f, 1e-9 * std::max(1.0, std::abs(cold.f)));
    ASSERT_EQ(cold.x.size(), second.x.size());
    for (Index i = 0; i < cold.x.size(); ++i) {
        EXPECT_NEAR(cold.x(i), second.x(i), 1e-8);
    }
}

TEST(ProblemScalingWarmStart, AScaledExportDropsTheScaledSpaceStateItCannotMap) {
    // The declared limitation, pinned so it is a decision rather than a
    // surprise: the funnel width, the two regularization parameters and the hot
    // factorization have no caller-scale image, so a scaled solve exports the
    // sentinels for them instead of numbers in the wrong units. `tr_radius`
    // survives, because no variable was scaled.
    UnevenRowModel model;
    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model);
    ASSERT_TRUE(sol.warm_start.valid);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.funnel_width);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.primal_delta);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.dual_mu);
    EXPECT_EQ(nullptr, sol.warm_start.hot);
    EXPECT_FALSE(sol.warm_start.has_prox_center);
    EXPECT_EQ(0, sol.warm_start.prox_center_x.size());
    EXPECT_GE(sol.warm_start.tr_radius, 0.0) << "an x-space radius, carried unchanged";
}

TEST(ProblemScalingWarmStart, TheExportedCurrencyIsTheSolutionsOwnMultipliers) {
    // The tightest statement of the export contract, and the one that catches a
    // block being written from the ENGINE-scale parameter instead of the
    // caller-scale export: whatever `SqpSolution` reports, the WarmStart beside
    // it must carry the SAME numbers, exactly. True on both paths -- on an
    // unscaled solve because the two are the same object, on a scaled one
    // because both went through the same map.
    UnevenRowModel model;
    for (const bool scaled : {false, true}) {
        SqpOptions opts;
        opts.enable_scaling = scaled;
        SqpDriver driver{opts};
        const SqpSolution sol = driver.solve(model);
        ASSERT_TRUE(sol.warm_start.valid) << "scaled=" << scaled;
        ASSERT_EQ(sol.lambda_e.size(), sol.warm_start.lambda_e.size());
        EXPECT_TRUE(sol.warm_start.lambda_e.isApprox(sol.lambda_e, 0.0))
            << "equality prices, EXACTLY, scaled=" << scaled;
        ASSERT_EQ(sol.lambda_i.size(), sol.warm_start.lambda_i.size());
        EXPECT_TRUE(sol.warm_start.lambda_i.isApprox(sol.lambda_i, 0.0))
            << "inequality prices, EXACTLY, scaled=" << scaled;
        ASSERT_EQ(sol.z.size(), sol.warm_start.z.size());
        EXPECT_TRUE(sol.warm_start.z.isApprox(sol.z, 0.0))
            << "bound prices, EXACTLY, scaled=" << scaled;
        EXPECT_TRUE(sol.warm_start.x.isApprox(sol.x, 0.0)) << "scaled=" << scaled;
    }
}

TEST(ProblemScalingWarmStart, ACallerScaleSeedIsMappedInOnAScaledSolve) {
    // The ingest direction of the same map. A seed carrying the CALLER-scale
    // price of the binding row must be adopted as a usable engine-space price;
    // if the map were missing, a 1e12 price would enter a solve whose duals are
    // O(1) and the solve would be poisoned by its own warm start.
    ScaledDualModel model(1e12, 5e-7);
    SqpOptions opts;
    opts.max_iter = 60;
    opts.enable_scaling = true;
    opts.start_level = StartLevel::kWarm;

    WarmStart seed;
    seed.x = Vec::Constant(1, model.x_star());
    seed.lambda_e = Vec(0);
    seed.lambda_i = Vec::Constant(1, 1e12); // the TRUE caller-scale price
    seed.z = Vec::Zero(1);
    seed.ineq_active.assign(1, 1);
    seed.bound_active.assign(1, 0);
    seed.qp_working_set = WorkingSet(1, 1);
    seed.structure_hash = 0;
    seed.valid = true;

    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model, model.start_point(), seed, 0);
    EXPECT_EQ(SqpStatus::kOptimal, sol.status);
    EXPECT_NEAR(model.x_star(), sol.x(0), 1e-9);
    EXPECT_NEAR(1e12, sol.lambda_i(0), 1e-3 * 1e12) << "and it comes back on the caller's scale";
}

// ===========================================================================
// FIX ROUND 1, FINDING 1: THE ITERATION HISTORY IS ON THE CALLER'S SCALE.
//
// `SqpSolution::f` and the four terminal residuals were mapped back from the
// first commit; the `history` rows were not, so a scaled solve printed a table
// (sqp_print.cpp) whose f column was `sf` times the f reported underneath it.
// The six scale-carrying columns are mapped at the export boundary now.
// ===========================================================================

TEST(ProblemScalingHistory, TheExportedRowsAreOnTheCallersScale) {
    ScaledUnevenModel model;
    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(SqpStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.active);
    ASSERT_FALSE(sol.history.empty());
    // Both halves of the map are live on this fixture, which is what makes the
    // columns below distinguishable from their engine-scale selves.
    EXPECT_DOUBLE_EQ(1.0e-4, sol.scaling.obj);
    EXPECT_DOUBLE_EQ(1.0e-4, sol.scaling.row_min);
    EXPECT_DOUBLE_EQ(1.0, sol.scaling.row_max);

    // THE FIRST ROW IS THE START POINT, where both columns are hand-computable
    // and the engine-scale answer is four orders away in each direction:
    // f(x0) = 5e5 (engine: 50) and h(x0) = |1e6 * -0.5| = 5e5 (engine: 50).
    const SqpIterate &first = sol.history.front();
    const Vec x0 = model.start_point();
    EXPECT_DOUBLE_EQ(model.eval_f(x0), first.f);
    EXPECT_NEAR(5.0e5, first.violation_l1, 1.0e-6 * 5.0e5);
    EXPECT_GT(first.violation_l1, 1.0e4)
        << "the engine-scale value would be 50 -- four orders below this floor";
    // kkt_residual folds the mapped halves, so it carries the mapped violation.
    EXPECT_GE(first.kkt_residual, 4.0e5);

    // THE LAST ROW AND THE SOLUTION AGREE ON f, EXACTLY. Both are the same
    // engine-scale double divided by the same factor, so this is bit equality
    // rather than a tolerance. It does NOT extend to the four residual columns:
    // the terminal ones are RE-MEASURED on the caller's own model at the export
    // boundary (an independent evaluation, deliberately -- see finish()), while
    // a history row is the mapped image of the measurement the iteration made.
    EXPECT_DOUBLE_EQ(sol.f, sol.history.back().f);
    EXPECT_DOUBLE_EQ(model.eval_f(sol.x), sol.history.back().f);

    // AND THE PRINTED TABLE IS ONE SCALE WITH ITS OWN TRAILER (finding 7).
    const std::string table = format_iteration_table(sol);
    EXPECT_NE(table.find("Scaling: obj="), std::string::npos) << table;
    EXPECT_NE(table.find("scaled_kkt="), std::string::npos) << table;
}

TEST(ProblemScalingHistory, AnUnscaledSolvesHistoryIsUntouched) {
    // The OFF-path half of the same pin: the mapping is behind one predicate,
    // so an unscaled solve's rows are the measurements themselves, and the
    // trailer says so in one word.
    ScaledUnevenModel model;
    SqpDriver driver{SqpOptions{}};
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(SqpStatus::kOptimal, sol.status);
    EXPECT_FALSE(sol.scaling.active);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_DOUBLE_EQ(model.eval_f(model.start_point()), sol.history.front().f);
    EXPECT_DOUBLE_EQ(sol.f, sol.history.back().f);
    EXPECT_NE(format_iteration_table(sol).find("Scaling: off"), std::string::npos);
}

// ===========================================================================
// FIX ROUND 1, FINDING 2: THE ONE EXIT THAT DOES NOT ROUTE THROUGH finish().
// ===========================================================================

TEST(ProblemScalingHistory, TheNonFiniteIterateExitStillUnscalesAndReports) {
    NonFiniteRowModel model;
    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(SqpStatus::kNumericalError, sol.status)
        << "the fixture must reach the non-finite-iterate exit for this pin to mean anything";
    // THE REPORT IS WRITTEN. Before the fix this exit left it default-
    // constructed, so a scaled solve claimed `active == false` about itself.
    EXPECT_TRUE(sol.scaling.active);
    EXPECT_DOUBLE_EQ(1.0e-4, sol.scaling.obj) << "100 / |grad f(x0)|inf = 100 / 1e6";
    EXPECT_TRUE(std::isnan(sol.scaling.scaled_kkt_residual))
        << "nothing was measured at this point, in either space";

    // AND f IS THE CALLER'S. The model is finite in f and in every derivative
    // here -- only the inequality residual is NaN -- so the objective is a real
    // number and reporting the engine's 50 instead of the caller's 5e5 would be
    // a silent unit error rather than a NaN.
    EXPECT_DOUBLE_EQ(model.eval_f(model.start_point()), sol.f);
    ASSERT_FALSE(sol.history.empty());
    EXPECT_DOUBLE_EQ(sol.f, sol.history.back().f);
    // The residual columns stay NaN, which is what "nothing was measured" reads
    // as; the multipliers stay cleared, which the map leaves cleared.
    EXPECT_TRUE(std::isnan(sol.kkt_residual));
    EXPECT_TRUE(sol.lambda_i.isZero(0.0));

    // AND THE CURRENCY DROP IS THE ONE `finish` APPLIES (fix round 2). This
    // exit builds its own WarmStart rather than routing through that boundary,
    // so the scaled-space state it may not export has to be dropped here too --
    // otherwise a scaled solve's failed-exit object carries a primal_delta and
    // a dual_mu set in the SCALED subproblem's units under field names whose
    // contract says caller-scale. Benign while `valid` is false, which it is
    // here; pinned anyway, because "no consumer looks today" is not a contract.
    EXPECT_FALSE(sol.warm_start.valid) << "the unevaluable exit hands back a COLD object";
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.funnel_width);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.primal_delta);
    EXPECT_DOUBLE_EQ(-1.0, sol.warm_start.dual_mu);
    EXPECT_EQ(nullptr, sol.warm_start.hot);
    EXPECT_FALSE(sol.warm_start.has_prox_center);
    EXPECT_EQ(0, sol.warm_start.prox_center_x.size());
}

// ===========================================================================
// FIX ROUND 1, FINDING 3: THE RESTORATION SUB-SOLVE'S MULTIPLIERS ARE ALREADY
// THE CALLER'S, AND MUST NOT BE MAPPED A SECOND TIME.
// ===========================================================================

TEST(ProblemScalingRestoration, TheAdoptedSelectorsAreNotMappedTwice) {
    // The sub-solve runs with scaling forced OFF over the RAW model, so its
    // subgradient selectors and its bound prices come back in the caller's
    // units already. Applying the export boundary's ENGINE->CALLER map to them
    // divides by sf a second time -- at sf = 100 on this fixture, the certified
    // selectors would read 0.00707 and -0.01 instead of 0.707 and -1.
    InfeasibleCircleLineModel model;

    SqpOptions off_opts;
    off_opts.max_iter = 200;
    SqpDriver off{off_opts};
    const SqpSolution a = off.solve(model);
    ASSERT_EQ(SqpStatus::kInfeasible, a.status);
    ASSERT_TRUE(a.infeasibility_certified);

    SqpOptions on_opts = off_opts;
    on_opts.enable_scaling = true;
    SqpDriver on{on_opts};
    const SqpSolution b = on.solve(model);

    ASSERT_TRUE(b.scaling.active);
    EXPECT_DOUBLE_EQ(100.0, b.scaling.obj) << "grad f is (1,1) everywhere: the full two-sided lift";
    EXPECT_DOUBLE_EQ(1.0, b.scaling.row_min) << "the row rule never amplifies";
    EXPECT_DOUBLE_EQ(1.0, b.scaling.row_max);
    ASSERT_EQ(SqpStatus::kInfeasible, b.status)
        << "the scaled solve must still reach the restoration exit this pin is about";
    EXPECT_TRUE(b.infeasibility_certified);

    // THE SELECTORS ARE THE SUB-SOLVE'S OWN, on the caller's scale, and they
    // are ADMISSIBLE -- |selector| <= 1 is what makes them a certificate at
    // all, and it is exactly the property a spurious 1/sf preserves while a
    // spurious sf destroys. Pinned against the unscaled solve's own answer.
    const double t = 1.0 / std::sqrt(2.0);
    EXPECT_NEAR(t, b.lambda_e(0), 1e-5);
    EXPECT_NEAR(-1.0, b.lambda_e(1), 1e-5);
    EXPECT_NEAR(a.lambda_e(0), b.lambda_e(0), 1e-5);
    EXPECT_NEAR(a.lambda_e(1), b.lambda_e(1), 1e-5);
    EXPECT_LE(b.lambda_e.cwiseAbs().maxCoeff(), 1.0 + 1e-9)
        << "a selector outside [-1, 1] is not a subgradient of h";

    // AND THE POINT, AND THE CALLER-SCALE RE-MEASUREMENT AT IT. The four
    // terminal residuals are taken on the caller's own model at exactly these
    // multipliers, so an independent measurement must reproduce them.
    EXPECT_NEAR(t, b.x(0), 1e-5);
    EXPECT_NEAR(t, b.x(1), 1e-5);
    const SqpKkt independent = evaluate_kkt(model, b.x, b.lambda_e, b.lambda_i, off_opts.feas_tol);
    EXPECT_DOUBLE_EQ(independent.stationarity, b.stationarity);
    EXPECT_DOUBLE_EQ(independent.feasibility, b.feasibility);
    EXPECT_DOUBLE_EQ(independent.residual(), b.kkt_residual);
    // The bound prices are the FEASIBILITY problem's own (the certificate's,
    // not grad L's), so they are compared against the unscaled solve's rather
    // than against a re-measurement -- and they too carry no second factor.
    ASSERT_EQ(a.z.size(), b.z.size());
    for (Index i = 0; i < a.z.size(); ++i) {
        EXPECT_NEAR(a.z(i), b.z(i), 1e-6) << "bound price " << i;
    }
}

// ===========================================================================
// FIX ROUND 1, FINDING 4: THE FACTORS ARE INDEXED BY ROW, SO A RE-LAY THAT
// CHANGES A ROW COUNT UNDER THEM IS REFUSED RATHER THAN READ PAST.
//
// No in-tree provider can do this within one solve -- NlpModelAggregate re-lays
// only from its own model, whose row counts are fixed for the object's life,
// and the driver never asks it to. The guard is structural all the same: the
// apply sites index the factor blocks directly, and Eigen's own bounds asserts
// are compiled out under NDEBUG (CLAUDE.md section 4), so the alternative to a
// refusal here is an out-of-range read in Release.
// ===========================================================================

TEST(ProblemScalingSeam, ARelayThatChangesARowCountUnderActiveFactorsIsRefused) {
    hven::sqp_tests::SettableClaimStreamSource source(2, 1, 1);
    source.set_kkt_stream({}, {}, {0, 0}, {0, 0}, {0, 0});
    AggregateEvalSeam seam(source);

    detail::ProblemScaling sc;
    sc.active = true;
    sc.obj = 0.5;
    sc.eq_rows = Vec::Constant(1, 0.25);
    sc.ineq_rows = Vec::Constant(1, 4.0);
    ASSERT_NO_THROW(seam.install_scaling(sc));

    const Vec x = (Vec(2) << 0.5, -0.25).finished();
    ASSERT_NO_THROW(seam.eval_nlp(x, Vec::Zero(1), Vec::Zero(1)));

    // A re-lay that keeps the row counts is FINE: the factors still describe
    // the rows they were computed for.
    source.set_row_counts(1, 1);
    ASSERT_NO_THROW(seam.eval_nlp(x, Vec::Zero(1), Vec::Zero(1)));

    // One that changes them is not, and the message names both shapes.
    source.set_row_counts(1, 2);
    try {
        seam.eval_nlp(x, Vec::Zero(1), Vec::Zero(1));
        FAIL() << "a re-lay to different row counts must be refused while factors are installed";
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("(1, 2)"), std::string::npos) << message;
        EXPECT_NE(message.find("(1, 1)"), std::string::npos) << message;
        EXPECT_NE(message.find("scaling"), std::string::npos) << message;
    }
}

TEST(ProblemScalingSeam, AnUnscaledSeamStillAcceptsARowCountChange) {
    // The guard is scoped to an ACTIVE scaling and to nothing else: the seam's
    // ordinary re-lay behaviour, which every unscaled solve relies on, is
    // untouched.
    hven::sqp_tests::SettableClaimStreamSource source(2, 1, 1);
    source.set_kkt_stream({}, {}, {0, 0}, {0, 0}, {0, 0});
    AggregateEvalSeam seam(source);

    const Vec x = (Vec(2) << 0.5, -0.25).finished();
    ASSERT_NO_THROW(seam.eval_nlp(x, Vec::Zero(1), Vec::Zero(1)));
    source.set_row_counts(1, 2);
    NlpEval ev;
    ASSERT_NO_THROW(ev = seam.eval_nlp(x, Vec::Zero(1), Vec::Zero(2)));
    EXPECT_EQ(2, ev.ci.size());
}

// ===========================================================================
// FIX ROUND 1, FINDING 5: THE BOUND PRICES COME FROM THE CALLER-SCALE
// RE-MEASUREMENT, NOT FROM A DIVIDE-BACK OF THE ENGINE'S OWN VECTOR.
//
// The four terminal residuals were already re-measured on the caller's model,
// for a stated floating-point reason -- multiplying by a factor and dividing it
// back is not the identity. `z` is a coordinate of the very `grad_lag` those
// were folded out of, so it belongs to the same measurement, and this pin holds
// it to the same standard: BIT equality with an independent measurement, not a
// tolerance. It needs a solution ON A BOUND, because an interior optimum has
// z == 0 and 0/sf == 0 hides the difference.
// ===========================================================================

TEST(ProblemScalingRoundTrip, TheBoundPricesAreTheReMeasurementsNotADivideBack) {
    ScaledBoundActiveModel model;
    SqpOptions opts;
    opts.enable_scaling = true;
    SqpDriver driver{opts};
    const SqpSolution sol = driver.solve(model);

    ASSERT_EQ(SqpStatus::kOptimal, sol.status);
    ASSERT_TRUE(sol.scaling.active);
    EXPECT_NEAR(0.2, sol.x(0), 1e-9);
    EXPECT_NEAR(1.0, sol.x(1), 1e-9) << "x1's upper bound binds";
    ASSERT_EQ(2, sol.z.size());
    EXPECT_NEAR(-1.16e6, sol.z(1), 1e-3) << "the hand-derived bound price, on the CALLER's scale";

    const SqpKkt independent =
        evaluate_kkt(model, sol.x, sol.lambda_e, sol.lambda_i, SqpOptions{}.feas_tol);
    // EXACT: the same fold, on the same model, at the same point and the same
    // multipliers. A divide-back of the engine's own vector is a DIFFERENT
    // computation -- sf*grad and (s_j*Ji)^T*lambda~ summed and then divided --
    // and it is not required to land on the same bits.
    for (Index i = 0; i < sol.z.size(); ++i) {
        EXPECT_EQ(independent.z(i), sol.z(i)) << "bound price " << i;
    }
    EXPECT_EQ(independent.stationarity, sol.stationarity);
}

} // namespace hven::solvers
