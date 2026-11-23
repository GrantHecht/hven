// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_iteration_telemetry.cpp -- M8 I19-1: the report-only per-row telemetry of the SQP engine's
// `sqp` block (SqpIterationRow) and of SqpIterationInfo.
//
//   pred_df / f_trial / h_trial / h_lin   against an independent recomputation from the model
//   soc_f / soc_h                         the Maratos fixture's SOC-rescued row
//   cap_binding vs tr_binding             a step-limit pin vs the radius, and that growth still
//                                         reads the MERGED flag (the decision is unchanged)
//   work totals                           the per-row sums reconcile with the solve's counters;
//                                         a ladder row's totals dominate its last rung's
//   t_s                                   monotone, informational

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <hven/hven.h>

namespace hven {
namespace {

// f = sum_j w_j (x_j - c_j)^2, linear equalities A x = b. Exact quadratic model: the QP predicts
// the objective change exactly.
struct QuadLinear final : Nlp {
    Vec w, c;
    std::vector<std::vector<double>> A;
    std::vector<double> b;
    NlpDimensions dimensions() const override {
        const Index n = w.size();
        return {.n = n,
                .m = static_cast<Index>(A.size()),
                .jacobian_nonzeros = static_cast<Index>(A.size()) * n,
                .hessian_nonzeros = n};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
        for (std::size_t i = 0; i < b.size(); ++i) {
            gl[static_cast<Index>(i)] = b[i];
            gu[static_cast<Index>(i)] = b[i];
        }
    }
    double eval_f(ConstVecRef x) const override {
        return (w.array() * (x - c).array().square()).sum();
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (w.array() * (x - c).array()).matrix();
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        for (std::size_t i = 0; i < A.size(); ++i) {
            double v = 0.0;
            for (Index j = 0; j < x.size(); ++j) {
                v += A[i][static_cast<std::size_t>(j)] * x[j];
            }
            g[static_cast<Index>(i)] = v;
        }
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef cc) const override {
        int k = 0;
        for (std::size_t i = 0; i < A.size(); ++i) {
            for (Index j = 0; j < w.size(); ++j) {
                r[k] = static_cast<int>(i);
                cc[k] = static_cast<int>(j);
                ++k;
            }
        }
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override {
        int k = 0;
        for (const auto &row : A) {
            for (const double a : row) {
                v[k++] = a;
            }
        }
    }
    void hessian_structure(IndexVecRef r, IndexVecRef cc) const override {
        for (Index j = 0; j < w.size(); ++j) {
            r[j] = static_cast<int>(j);
            cc[j] = static_cast<int>(j);
        }
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef, VecRef v) const override {
        v = 2.0 * s * w;
    }
};

std::shared_ptr<QuadLinear> quad_linear(Vec w, Vec c, std::vector<std::vector<double>> A,
                                        std::vector<double> b) {
    auto m = std::make_shared<QuadLinear>();
    m->w = std::move(w);
    m->c = std::move(c);
    m->A = std::move(A);
    m->b = std::move(b);
    return m;
}

// f = x0 + x1 + k (x0^2 + x1^2 - 1) (k = 0 for the plain circle problem; the Maratos fixture is
// f = 2 (x0^2 + x1^2 - 1) - x0), g = x0^2 + x1^2 = 1.
struct Circle final : Nlp {
    double a0, a1, k; // f = a0 x0 + a1 x1 + k (x0^2 + x1^2 - 1)
    Circle(double a0_, double a1_, double k_) : a0(a0_), a1(a1_), k(k_) {}
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
        gl[0] = 1.0;
        gu[0] = 1.0;
    }
    double eval_f(ConstVecRef x) const override {
        return a0 * x[0] + a1 * x[1] + k * (x[0] * x[0] + x[1] * x[1] - 1.0);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g[0] = a0 + 2.0 * k * x[0];
        g[1] = a1 + 2.0 * k * x[1];
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = x[0] * x[0] + x[1] * x[1]; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 0;
        c[1] = 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v[0] = 2.0 * x[0];
        v[1] = 2.0 * x[1];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 1;
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef lambda, VecRef v) const override {
        v[0] = s * 2.0 * k + lambda[0] * 2.0;
        v[1] = s * 2.0 * k + lambda[0] * 2.0;
    }
};

// f = sf (x0 + x1), g = sg (x0^2 + x1^2) with g = sg (equality) or g <= sg (inequality): the unit
// circle, with the objective and the row scaled by sf and sg so that row/objective scaling fires.
struct ScaledCircle final : Nlp {
    double sf, sg;
    bool ineq;
    ScaledCircle(double sf_, double sg_, bool ineq_) : sf(sf_), sg(sg_), ineq(ineq_) {}
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setConstant(-kInfinity);
        u.setConstant(kInfinity);
        gl[0] = ineq ? -kInfinity : sg;
        gu[0] = sg;
    }
    double eval_f(ConstVecRef x) const override { return sf * (x[0] + x[1]); }
    void eval_grad_f(ConstVecRef, VecRef g) const override {
        g[0] = sf;
        g[1] = sf;
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = sg * (x[0] * x[0] + x[1] * x[1]); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 0;
        c[1] = 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v[0] = 2.0 * sg * x[0];
        v[1] = 2.0 * sg * x[1];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 1;
    }
    void eval_hessian(ConstVecRef, double, ConstVecRef lambda, VecRef v) const override {
        v[0] = 2.0 * sg * lambda[0];
        v[1] = 2.0 * sg * lambda[0];
    }
};

// min |x|^2 / 2  s.t.  x0 = 5  with x in [0, 1]^2: the linearization is infeasible inside the box,
// the main QP is verified infeasible and the elastic ladder runs (and, here, exhausts).
struct InconsistentBoxed final : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 1, .hessian_nonzeros = 2};
    }
    void bounds(VecRef l, VecRef u, VecRef gl, VecRef gu) const override {
        l.setZero();
        u.setOnes();
        gl[0] = 5.0;
        gu[0] = 5.0;
    }
    double eval_f(ConstVecRef x) const override { return 0.5 * x.squaredNorm(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g = x; }
    void eval_g(ConstVecRef x, VecRef g) const override { g[0] = x[0]; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override { v[0] = 1.0; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r[0] = 0;
        c[0] = 0;
        r[1] = 1;
        c[1] = 1;
    }
    void eval_hessian(ConstVecRef, double s, ConstVecRef, VecRef v) const override {
        v[0] = s;
        v[1] = s;
    }
};

// One event per history row: the iterate x and the callback's SqpIterationInfo.
struct Tap {
    std::vector<Vec> x;
    std::vector<SqpIterationInfo> info;
    void attach(SqpSolver &s) {
        s.set_iteration_callback(
            [this](const IterationEvent &e) {
                x.emplace_back(e.x);
                info.push_back(e.sqp.value_or(SqpIterationInfo{}));
                return CallbackAction::kContinue;
            },
            kDefaultIterationPayload);
    }
};

bool accepted(const IterationRecord &h) {
    return h.sqp && h.sqp->verdict &&
           (*h.sqp->verdict == StepVerdict::kAcceptObjectiveStep ||
            *h.sqp->verdict == StepVerdict::kAcceptFeasibilityStep);
}

SqpOptions silent() {
    SqpOptions o;
    o.common.verbosity = Verbosity::kSilent;
    o.common.max_iterations = 60;
    return o;
}

// ---- pred_df, f_trial, h_trial and h_lin against the model ------------------------------------

TEST(IterationTelemetry, PredDfAndTheTrialPointAgreeWithAnIndependentRecomputation) {
    auto m = quad_linear(Vec{{1.0, 2.0}}, Vec{{10.0, 10.0}}, {{1.0, -1.0}}, {0.0});
    Problem p(m);
    SqpOptions o = silent();
    o.trust_region_initial = 2.0; // several radius-limited steps
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec{{0.0, 4.0}});
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(tap.x.size(), r.history.size());
    int checked = 0;
    for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
        const IterationRecord &h = r.history[k];
        ASSERT_TRUE(h.sqp.has_value());
        if (!accepted(h) || h.soc) {
            continue;
        }
        const Vec &xk = tap.x[k];
        const Vec &xn = tap.x[k + 1]; // the trial point: x + p
        const Vec step = xn - xk;
        Vec g(2);
        m->eval_grad_f(xk, g);
        const double pred =
            -(g.dot(step) + 0.5 * (2.0 * m->w.array() * step.array().square()).sum());
        ASSERT_TRUE(h.sqp->pred_df.has_value());
        ASSERT_TRUE(h.sqp->f_trial.has_value());
        ASSERT_TRUE(h.sqp->h_trial.has_value());
        EXPECT_NEAR(*h.sqp->pred_df, pred, 1e-9 * (1.0 + std::abs(pred))) << "row " << k;
        EXPECT_NEAR(*h.sqp->f_trial, m->eval_f(xn), 1e-9 * (1.0 + std::abs(m->eval_f(xn))))
            << "row " << k;
        EXPECT_NEAR(*h.sqp->h_trial, std::abs(xn[0] - xn[1]), 1e-9) << "row " << k;
        // A linear constraint: the linearization is exact, so the step leaves what it leaves.
        ASSERT_TRUE(h.sqp->h_lin.has_value());
        EXPECT_NEAR(*h.sqp->h_lin, std::abs(xn[0] - xn[1]), 1e-9) << "row " << k;
        // A quadratic model predicts an exact decrease.
        EXPECT_NEAR(*h.sqp->pred_df, m->eval_f(xk) - m->eval_f(xn), 1e-8) << "row " << k;
        EXPECT_FALSE(h.sqp->soc_f.has_value());
        EXPECT_FALSE(h.sqp->soc_h.has_value());
        ++checked;
    }
    EXPECT_GE(checked, 2) << "premise: several accepted rows";
    const IterationRecord &last = r.history.back();
    EXPECT_FALSE(last.sqp->qp_solved);
    EXPECT_FALSE(last.sqp->pred_df.has_value());
    EXPECT_FALSE(last.sqp->h_lin.has_value());
}

TEST(IterationTelemetry, HLinIsTheLinearizedViolationOfANonlinearRowAtTheStep) {
    auto m = std::make_shared<Circle>(1.0, 1.0, 0.0);
    Problem p(m);
    SqpOptions o = silent();
    o.trust_region_initial = 0.5; // the step cannot close the violation: h_lin > 0
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec{{3.0, 3.0}});
    ASSERT_GE(r.history.size(), 3u);
    int checked = 0;
    for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
        const IterationRecord &h = r.history[k];
        if (!accepted(h) || h.soc) {
            continue;
        }
        const Vec &xk = tap.x[k];
        const Vec &xn = tap.x[k + 1];
        const Vec step = xn - xk;
        const double c = xk[0] * xk[0] + xk[1] * xk[1] - 1.0;
        const double lin = c + 2.0 * xk[0] * step[0] + 2.0 * xk[1] * step[1];
        const double h_trial = std::abs(xn[0] * xn[0] + xn[1] * xn[1] - 1.0);
        ASSERT_TRUE(h.sqp->h_lin.has_value());
        ASSERT_TRUE(h.sqp->h_trial.has_value());
        EXPECT_NEAR(*h.sqp->h_lin, std::abs(lin), 1e-9 * (1.0 + std::abs(lin))) << "row " << k;
        EXPECT_NEAR(*h.sqp->h_trial, h_trial, 1e-9 * (1.0 + h_trial)) << "row " << k;
        if (k == 0) {
            EXPECT_GT(*h.sqp->h_lin, 1.0) << "premise: the radius left the linearization open";
        }
        ++checked;
    }
    EXPECT_GE(checked, 2);
}

// ---- the same, with row/objective scaling active, and on inequality rows -----------------------

// h_trial, h_lin, f_trial against the model's own functions at the step read off the callback's
// iterates, and against the NEXT row's exported f / violation_l1 (the iterate the step moved to),
// in the caller's units. Returns the number of rows checked and how many had h_lin > 0.
struct CircleChecks {
    int checked = 0;
    int open_rows = 0;
};
CircleChecks check_circle_rows(const ScaledCircle &m, const SqpResult &r, const Tap &tap) {
    CircleChecks out;
    for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
        const IterationRecord &h = r.history[k];
        if (!accepted(h) || h.soc) {
            continue;
        }
        const Vec &xk = tap.x[k];
        const Vec &xn = tap.x[k + 1];
        const Vec step = xn - xk;
        const auto viol = [&](double g) {
            return m.ineq ? std::max(0.0, g - m.sg) : std::abs(g - m.sg);
        };
        const double g_trial = m.sg * (xn[0] * xn[0] + xn[1] * xn[1]);
        const double g_k = m.sg * (xk[0] * xk[0] + xk[1] * xk[1]);
        const double g_lin = g_k + 2.0 * m.sg * (xk[0] * step[0] + xk[1] * step[1]);
        const double h_trial = viol(g_trial);
        const double h_lin = viol(g_lin);
        const double f_trial = m.sf * (xn[0] + xn[1]);
        EXPECT_TRUE(h.sqp->h_trial.has_value() && h.sqp->h_lin.has_value() &&
                    h.sqp->f_trial.has_value())
            << "row " << k;
        if (!h.sqp->h_trial || !h.sqp->h_lin || !h.sqp->f_trial) {
            continue;
        }
        EXPECT_NEAR(*h.sqp->h_trial, h_trial, 1e-9 * (1.0 + h_trial)) << "row " << k;
        EXPECT_NEAR(*h.sqp->h_lin, h_lin, 1e-9 * (1.0 + h_lin)) << "row " << k;
        EXPECT_NEAR(*h.sqp->f_trial, f_trial, 1e-9 * (1.0 + std::abs(f_trial))) << "row " << k;
        // The next row is evaluated AT the trial point: its exported values are the same numbers.
        EXPECT_TRUE(r.history[k + 1].f.has_value());
        EXPECT_NEAR(r.history[k + 1].f.value_or(0.0), *h.sqp->f_trial,
                    1e-9 * (1.0 + std::abs(f_trial)))
            << "row " << k;
        EXPECT_NEAR(r.history[k + 1].sqp->violation_l1, *h.sqp->h_trial, 1e-9 * (1.0 + h_trial))
            << "row " << k;
        ++out.checked;
        out.open_rows += h_lin > 0.0 ? 1 : 0;
    }
    return out;
}

SqpResult solve_circle(const ScaledCircle &model, bool scaled, Tap &tap) {
    SqpOptions o = silent();
    o.trust_region_initial = 0.5; // radius-limited steps: the linearization stays open
    o.row_scaling = scaled;
    o.column_scaling = SqpColumnScaling::kOff;
    SqpSolver s(o);
    tap.attach(s);
    auto m = std::make_shared<ScaledCircle>(model);
    Problem p(m);
    return s.solve(p, Vec{{3.0, 3.0}});
}

TEST(IterationTelemetry, TrialDiagnosticsOnAnInequalityRowAreTheModelsOwn) {
    for (const bool ineq : {true, false}) {
        SCOPED_TRACE(ineq ? "inequality" : "equality");
        Tap tap;
        const ScaledCircle model(1.0, 1.0, ineq);
        const SqpResult r = solve_circle(model, /*scaled=*/false, tap);
        ASSERT_FALSE(r.scaling.active);
        const CircleChecks c = check_circle_rows(model, r, tap);
        EXPECT_GE(c.checked, 3);
        EXPECT_GE(c.open_rows, 1) << "premise: the radius left the linearization open";
    }
}

TEST(IterationTelemetry, TrialDiagnosticsAreInCallerUnitsWhenRowAndObjectiveScalingAreActive) {
    for (const bool ineq : {true, false}) {
        SCOPED_TRACE(ineq ? "inequality" : "equality");
        Tap tap;
        const ScaledCircle model(1.0e4, 1.0e3, ineq);
        const SqpResult r = solve_circle(model, /*scaled=*/true, tap);
        ASSERT_TRUE(r.scaling.active) << "premise: scaling fired";
        ASSERT_LT(r.scaling.objective, 0.1) << "premise: a real objective factor";
        ASSERT_LT(r.scaling.row_min, 0.5) << "premise: a real row factor";
        const CircleChecks c = check_circle_rows(model, r, tap);
        EXPECT_GE(c.checked, 3);
        EXPECT_GE(c.open_rows, 1) << "premise: the radius left the linearization open";
    }
}

TEST(IterationTelemetry, PredDfIsInCallerUnitsWhenRowAndObjectiveScalingAreActive) {
    auto m = quad_linear(Vec{{1.0e4, 2.0e4}}, Vec{{10.0, 10.0}}, {{1.0e3, -1.0e3}}, {0.0});
    Problem p(m);
    SqpOptions o = silent();
    o.trust_region_initial = 2.0;
    o.row_scaling = true;
    o.column_scaling = SqpColumnScaling::kOff;
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec{{0.0, 6.0}});
    ASSERT_TRUE(r.history[0].sqp->h_trial.has_value());
    EXPECT_GT(*r.history[0].sqp->h_trial, 0.0) << "premise: the radius left the row violated";
    ASSERT_TRUE(r.scaling.active) << "premise: scaling fired";
    ASSERT_LT(r.scaling.objective, 0.1);
    ASSERT_LT(r.scaling.row_min, 0.5);
    int checked = 0;
    for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
        const IterationRecord &h = r.history[k];
        if (!accepted(h) || h.soc) {
            continue;
        }
        const Vec &xk = tap.x[k];
        const Vec &xn = tap.x[k + 1];
        const Vec step = xn - xk;
        Vec g(2);
        m->eval_grad_f(xk, g);
        const double pred =
            -(g.dot(step) + 0.5 * (2.0 * m->w.array() * step.array().square()).sum());
        const double h_trial = std::abs(1.0e3 * (xn[0] - xn[1]));
        const double h_lin = h_trial; // a linear row: the linearization is exact
        ASSERT_TRUE(h.sqp->pred_df && h.sqp->f_trial && h.sqp->h_trial && h.sqp->h_lin);
        EXPECT_NEAR(*h.sqp->pred_df, pred, 1e-7 * (1.0 + std::abs(pred))) << "row " << k;
        // The quadratic model is exact, so the prediction is the caller-unit objective decrease.
        EXPECT_NEAR(*h.sqp->pred_df, m->eval_f(xk) - m->eval_f(xn), 1e-7 * (1.0 + std::abs(pred)))
            << "row " << k;
        EXPECT_NEAR(*h.sqp->f_trial, m->eval_f(xn), 1e-7 * (1.0 + std::abs(m->eval_f(xn))))
            << "row " << k;
        EXPECT_NEAR(*h.sqp->h_trial, h_trial, 1e-7 * (1.0 + h_trial)) << "row " << k;
        EXPECT_NEAR(*h.sqp->h_lin, h_lin, 1e-7 * (1.0 + h_lin)) << "row " << k;
        ASSERT_TRUE(r.history[k + 1].f.has_value());
        EXPECT_NEAR(*r.history[k + 1].f, *h.sqp->f_trial, 1e-7 * (1.0 + std::abs(pred)))
            << "row " << k;
        EXPECT_NEAR(r.history[k + 1].sqp->violation_l1, *h.sqp->h_trial, 1e-7 * (1.0 + h_trial))
            << "row " << k;
        ++checked;
    }
    EXPECT_GE(checked, 2) << "premise: several accepted rows";
}

TEST(IterationTelemetry, HLinIsNullUnderAnEqualityBasisReduction) {
    // Two dependent equality rows: the basis drops one, so the QP's rows are not the model's.
    auto m = quad_linear(Vec{{1.0, 2.0, 3.0}}, Vec{{4.0, 5.0, 6.0}},
                         {{1.0, -1.0, 0.0}, {2.0, -2.0, 0.0}, {0.0, 1.0, -1.0}}, {0.0, 0.0, 0.0});
    Problem p(m);
    SqpOptions o = silent();
    o.trust_region_initial = 1.0;
    o.advanced.equality_basis = true;
    SqpSolver s(o);
    const SqpResult r = s.solve(p, Vec{{0.0, 3.0, -2.0}});
    ASSERT_TRUE(r.equality_basis.computed);
    ASSERT_FALSE(r.equality_basis.dropped_rows.empty()) << "premise: the basis dropped a row";
    int stepped = 0;
    for (const IterationRecord &h : r.history) {
        if (!h.sqp->qp_solved || !h.sqp->pred_df) {
            continue;
        }
        EXPECT_FALSE(h.sqp->h_lin.has_value()) << "the reduced QP's rows are not the model's";
        EXPECT_TRUE(h.sqp->h_trial.has_value());
        ++stepped;
    }
    EXPECT_GE(stepped, 1);
}

TEST(IterationTelemetry, TheSocPointIsReportedOnTheRescuedRowAndOnlyThere) {
    constexpr double kTheta = 0.1;
    auto m = std::make_shared<Circle>(-1.0, 0.0, 2.0); // Maratos: 2 (x^2 + y^2 - 1) - x
    Problem p(m);
    SqpOptions o = silent();
    o.common.optimality_tol = 5e-4;
    o.common.feasibility_tol = 5e-4;
    o.advanced.second_order_correction = true;
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec{{std::cos(kTheta), std::sin(kTheta)}});
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.counters.soc_applied, 1) << "premise: the classic Maratos rescue";
    bool saw = false;
    for (std::size_t k = 0; k + 1 < r.history.size(); ++k) {
        const IterationRecord &h = r.history[k];
        if (!h.soc) {
            EXPECT_FALSE(h.sqp->soc_f.has_value()) << "row " << k;
            EXPECT_FALSE(h.sqp->soc_h.has_value()) << "row " << k;
            continue;
        }
        saw = true;
        const Vec &xs = tap.x[k + 1]; // the SOC point is the iterate the rescue moved to
        const double f_soc = m->eval_f(xs);
        ASSERT_TRUE(h.sqp->soc_f.has_value());
        ASSERT_TRUE(h.sqp->soc_h.has_value());
        EXPECT_NEAR(*h.sqp->soc_f, f_soc, 1e-9 * (1.0 + std::abs(f_soc)));
        EXPECT_NEAR(*h.sqp->soc_h, std::abs(xs[0] * xs[0] + xs[1] * xs[1] - 1.0), 1e-12);
        // The raw QP step was the one rejected: its violation exceeded the rescue's.
        ASSERT_TRUE(h.sqp->h_trial.has_value());
        EXPECT_GT(*h.sqp->h_trial, *h.sqp->soc_h);
        EXPECT_NE(*h.sqp->f_trial, *h.sqp->soc_f);
    }
    EXPECT_TRUE(saw);
}

// ---- the binding split ------------------------------------------------------------------------

SqpOptions limit_options(double r, double radius) {
    SqpOptions o = silent();
    o.caller_step_limit = r;
    o.trust_region_initial = radius;
    o.trust_region_max = 1e4;
    return o;
}

TEST(IterationTelemetry, ACapPinIsCapBindingAndNotTrBinding) {
    auto m = quad_linear(Vec{{1.0, 1.0}}, Vec{{10.0, 10.0}}, {{1.0, -1.0}}, {0.0});
    Problem p(m);
    SqpOptions o = limit_options(0.1, 1e3); // the radius is far wider than the cap
    o.common.max_iterations = 3;
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec::Zero(2));
    ASSERT_GE(r.history.size(), 2u);
    const IterationRecord &h0 = r.history[0];
    ASSERT_TRUE(h0.sqp && h0.sqp->qp_solved);
    EXPECT_TRUE(h0.sqp->cap_binding);
    EXPECT_FALSE(h0.sqp->tr_binding) << "the radius is 1e3: only the cap limits the step";
    EXPECT_TRUE(tap.info[0].cap_binding);
    EXPECT_FALSE(tap.info[0].tr_binding);
    // THE DECISION IS UNCHANGED: radius growth still reads the merged flag, so a cap-bound
    // accepted step grows the radius exactly as a radius-bound one does.
    ASSERT_TRUE(accepted(h0));
    EXPECT_DOUBLE_EQ(r.history[1].sqp->trust_radius, 2.0 * h0.sqp->trust_radius);
}

TEST(IterationTelemetry, TheRadiusIsTrBindingAndNotCapBinding) {
    auto m = quad_linear(Vec{{1.0, 1.0}}, Vec{{10.0, 10.0}}, {{1.0, -1.0}}, {0.0});
    Problem p(m);
    SqpOptions o = limit_options(0.0, 0.5); // no step limit; a small radius
    o.common.max_iterations = 3;
    SqpSolver s(o);
    Tap tap;
    tap.attach(s);
    const SqpResult r = s.solve(p, Vec::Zero(2));
    ASSERT_GE(r.history.size(), 1u);
    const IterationRecord &h0 = r.history[0];
    ASSERT_TRUE(h0.sqp && h0.sqp->qp_solved);
    EXPECT_TRUE(h0.sqp->tr_binding);
    EXPECT_FALSE(h0.sqp->cap_binding);
    EXPECT_TRUE(tap.info[0].tr_binding);
    EXPECT_FALSE(tap.info[0].cap_binding);
}

TEST(IterationTelemetry, NeitherFlagIsSetWhenNothingLimitsTheStep) {
    auto m = quad_linear(Vec{{1.0, 1.0}}, Vec{{1.0, 1.0}}, {{1.0, -1.0}}, {0.0});
    Problem p(m);
    SqpOptions o = limit_options(0.0, 1e3);
    SqpSolver s(o);
    const SqpResult r = s.solve(p, Vec::Zero(2));
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    for (const IterationRecord &h : r.history) {
        EXPECT_FALSE(h.sqp->tr_binding);
        EXPECT_FALSE(h.sqp->cap_binding);
    }
}

// ---- work totals ------------------------------------------------------------------------------

TEST(IterationTelemetry, RowWorkTotalsReconcileWithTheSolvesCounters) {
    auto m = std::make_shared<Circle>(-1.0, 0.0, 2.0);
    Problem p(m);
    SqpOptions o = silent();
    o.advanced.second_order_correction = true;
    SqpSolver s(o);
    const SqpResult r = s.solve(p, Vec{{std::cos(0.1), std::sin(0.1)}});
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    ASSERT_EQ(r.counters.restoration_iters, 0) << "premise: no restoration phase folded work";
    Index minors = 0;
    Index facts = 0;
    Index delta = 0;
    for (const IterationRecord &h : r.history) {
        minors += h.sqp->qp_minor_iterations_total;
        facts += h.sqp->qp_factorizations_total;
        delta += h.sqp->factorizations_delta;
        EXPECT_GE(h.sqp->qp_minor_iterations_total, h.sqp->qp_minor_iterations.value_or(0));
        EXPECT_GE(h.sqp->qp_factorizations_total, h.sqp->qp_factorizations.value_or(0));
    }
    EXPECT_EQ(minors, r.counters.qp_minor_iters);
    EXPECT_EQ(facts, r.counters.factorizations);
    EXPECT_EQ(delta, r.counters.factorizations);
    EXPECT_EQ(r.history.back().factorizations, r.counters.factorizations);
    // The SOC re-solve's own work is part of its row's total (it is a hot re-solve and may cost
    // no factorization: the identities above are what pin it, since the solve's counters hold it).
    bool saw_soc = false;
    for (const IterationRecord &h : r.history) {
        saw_soc = saw_soc || h.soc;
    }
    EXPECT_TRUE(saw_soc);
}

TEST(IterationTelemetry, ALadderRowsTotalsDominateItsLastRungAndItsOwnShare) {
    auto m = std::make_shared<InconsistentBoxed>();
    Problem p(m);
    SqpOptions o = silent();
    SqpSolver s(o);
    const SqpResult r = s.solve(p, Vec{{0.5, 0.5}});
    ASSERT_GE(r.counters.elastic_activations, 1) << "premise: the main QP is infeasible";
    ASSERT_GT(r.counters.restoration_iters, 0) << "premise: a restoration phase folded work";
    int ladders = 0;
    int multi_rung = 0;
    Index minors = 0;
    Index facts = 0;
    Index delta = 0;
    for (const IterationRecord &h : r.history) {
        minors += h.sqp->qp_minor_iterations_total;
        facts += h.sqp->qp_factorizations_total;
        delta += h.sqp->factorizations_delta;
        if (!h.sqp->ladder) {
            continue;
        }
        ++ladders;
        const ElasticLadderRow &l = *h.sqp->ladder;
        EXPECT_GT(l.rho_0, 0.0);
        EXPECT_GE(l.rho, l.rho_0);
        EXPECT_GE(l.minor_iterations, h.sqp->qp_minor_iterations.value_or(0));
        EXPECT_GE(l.factorizations, h.sqp->qp_factorizations.value_or(0));
        EXPECT_GE(l.factorizations, 1) << "the chain factorizes at least once";
        if (l.rungs > 0) {
            ++multi_rung;
            // The row's own figures are the last rung's; the ladder's are every rung's.
            EXPECT_GT(l.factorizations, h.sqp->qp_factorizations.value_or(0));
        }
        EXPECT_GE(h.sqp->qp_minor_iterations_total, l.minor_iterations);
        EXPECT_GE(h.sqp->qp_factorizations_total, l.factorizations);
    }
    EXPECT_GE(ladders, 1);
    ASSERT_GT(multi_rung, 0) << "premise: a ladder row climbed, so the M6 kill is live";
    // The row deltas see everything the solve charged; the row totals leave out the work a
    // restoration phase folded in (its own depth-1 rows carry it).
    EXPECT_EQ(delta, r.counters.factorizations);
    EXPECT_LE(minors, r.counters.qp_minor_iters);
    EXPECT_LE(facts, r.counters.factorizations);
    EXPECT_LT(facts, delta) << "the restoration phase's factorizations are not a row's work";
    EXPECT_LT(minors, r.counters.qp_minor_iters)
        << "nor are the restoration phase's QP minor iterations";
}

// A restoration phase's own (depth-1) rows carry the work the parent's rows leave out: summed over
// every row of every depth, the totals reconcile with the solve's counters.
struct RowCollector final : TraceSink {
    std::vector<IterationRecord> rows;
    void on_iter(const IterTraceEvent &e) override { rows.push_back(e.record); }
};

TEST(IterationTelemetry, ARestorationPhasesRowsCarryTheWorkTheParentLeavesOut) {
    auto m = std::make_shared<InconsistentBoxed>();
    Problem p(m);
    SqpSolver s(silent());
    RowCollector sink;
    s.attach_trace(&sink);
    const SqpResult r = s.solve(p, Vec{{0.5, 0.5}});
    ASSERT_GT(r.counters.restoration_iters, 0) << "premise: a restoration phase ran";
    Index minors = 0;
    Index facts = 0;
    int nested = 0;
    for (const IterationRecord &h : sink.rows) {
        ASSERT_TRUE(h.sqp.has_value());
        minors += h.sqp->qp_minor_iterations_total;
        facts += h.sqp->qp_factorizations_total;
        if (h.sqp->depth > 0) {
            ++nested;
            EXPECT_TRUE(std::isfinite(h.sqp->t_s));
            EXPECT_GE(h.sqp->t_s, 0.0);
            EXPECT_LE(h.sqp->t_s, r.wall_seconds + 1e-3) << "counts from the nested solve's entry";
        }
    }
    ASSERT_GT(nested, 0) << "premise: depth-1 rows were emitted";
    EXPECT_EQ(minors, r.counters.qp_minor_iters);
    EXPECT_EQ(facts, r.counters.factorizations);
}

TEST(IterationTelemetry, SocPointIsReportedOnARowWhereTheCorrectionWasTriedAndLost) {
    // A Maratos step far from the solution: the correction is tried and its own point is judged
    // too, and sometimes the correction does not win. The row still reports the point it tried.
    int tried_and_lost = 0;
    for (const double theta : {0.3, 0.6, 0.9, 1.2}) {
        auto m = std::make_shared<Circle>(-1.0, 0.0, 2.0);
        Problem p(m);
        SqpOptions o = silent();
        o.advanced.second_order_correction = true;
        o.trust_region_initial = 5.0;
        SqpSolver s(o);
        Tap tap;
        tap.attach(s);
        const SqpResult r = s.solve(p, Vec{{std::cos(theta), std::sin(theta)}});
        for (std::size_t k = 0; k < r.history.size(); ++k) {
            const IterationRecord &h = r.history[k];
            if (!h.sqp->soc_f.has_value()) {
                continue;
            }
            EXPECT_TRUE(h.sqp->soc_h.has_value());
            if (!h.soc) {
                ++tried_and_lost;
            }
        }
    }
    EXPECT_GE(tried_and_lost, 1) << "premise: at least one correction was tried and lost";
}

// ---- the clock --------------------------------------------------------------------------------

TEST(IterationTelemetry, TheRowClockIsMonotoneAndInsideTheSolve) {
    auto m = std::make_shared<Circle>(-1.0, 0.0, 2.0);
    Problem p(m);
    SqpSolver s(silent());
    const SqpResult r = s.solve(p, Vec{{std::cos(0.1), std::sin(0.1)}});
    ASSERT_GE(r.history.size(), 3u);
    double prev = 0.0;
    for (const IterationRecord &h : r.history) {
        EXPECT_TRUE(std::isfinite(h.sqp->t_s));
        EXPECT_GE(h.sqp->t_s, prev);
        prev = h.sqp->t_s;
    }
    EXPECT_GT(prev, 0.0);
    EXPECT_LE(prev, r.wall_seconds + 1e-3);
}

} // namespace
} // namespace hven
