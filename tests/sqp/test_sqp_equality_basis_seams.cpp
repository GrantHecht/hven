// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_sqp_equality_basis_seams.cpp — SqpOptions::equality_basis through its
// HVEN_TESTING seam (M7 W2 E1b; docs/testing.md). Compiled only into
// hven_sqp_seam_tests, which recompiles the library with HVEN_TESTING.
//
//   T6  F9's negative control: with the zero-step hand-off disabled, the inconsistent
//       duplicate pair never completes its two-iterate evidence and every mode runs
//       to max_iter -- so F9's kInfeasible is the hand-off's doing.
//   T7  an SSN prox-centre lift that fails its estimate check (a seam tolerance below
//       the inflated a-posteriori estimate it reports) is counted in lift_refusals
//       and refuses the basis: the QPs carry every row thereafter.
//   T8  a warm structural probe whose projection fails its estimate (a seam tolerance
//       below the estimate it reports) is counted in warm_probe_refusals and
//       lift_refusals, and the warm start resolves kSeeded instead of kWarm.

#include <cmath>
#include <limits>
#include <utility>
#include <vector>

#include <Eigen/Dense>
#include <gtest/gtest.h>

#include <hven/detail/drivers/equality_basis_testing.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "support/qp_as_nlp_model.h"

namespace hven::solvers {
namespace {

using test_support::QpAsNlpModel;
using Seam = detail::testing::EqualityBasisSeam;

struct SeamReset {
    SeamReset() { Seam::reset(); }
    ~SeamReset() { Seam::reset(); }
};

// min 1/2 x^T diag(h) x + g^T x  s.t.  A x = b,  lower <= x <= upper.
QpProblem make_qp(const Eigen::MatrixXd &A, const Vec &b, const Vec &h, const Vec &g,
                  const Vec &lower, const Vec &upper) {
    QpProblem qp;
    const Index n = A.cols();
    qp.H = SpMatRM(n, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index i = 0; i < n; ++i) {
        t.emplace_back(i, i, h(i));
    }
    qp.H.setFromTriplets(t.begin(), t.end());
    qp.g = g;
    qp.Ae = A.sparseView();
    qp.Ae.makeCompressed();
    qp.be = b;
    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);
    qp.lower = lower;
    qp.upper = upper;
    return qp;
}

SqpOptions opts_for(QpMode mode) {
    SqpOptions o;
    o.qp_mode = mode;
    o.equality_basis = true;
    o.max_iter = 100;
    return o;
}

// F9's model (test_sqp_equality_basis.cpp): an inconsistent duplicate pair whose
// start is the reduced problem's stationary point.
TEST(SqpEqualityBasisSeam, T6DisabledHandOffRunsF9ToMaxIter) {
    const SeamReset guard;
    Eigen::MatrixXd A(2, 2);
    A << 1.0, 0.0, 1.0, 0.0;
    Vec b(2);
    b << 1.0, 1.001;
    Vec h(2);
    h << 2.0, 2.0;
    Vec g(2);
    g << -2.0 * 1.0005, 0.0;
    Vec x0(2);
    x0 << 1.0005, 0.0;
    const QpAsNlpModel model(make_qp(A, b, h, g, Vec::Constant(2, -10.0), Vec::Constant(2, 10.0)),
                             x0);
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions o = opts_for(mode);
        // The control arm: the seam off reproduces F9.
        Seam::disable_hand_off = false;
        SqpSolver with(o);
        const SqpResult on = with.solve(model);
        EXPECT_EQ(on.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(on.equality_basis.handed_off);
        EXPECT_LT(on.counters.major_iters, o.max_iter);
        // The negative control.
        Seam::disable_hand_off = true;
        SqpSolver without(o);
        const SqpResult off = without.solve(model);
        EXPECT_FALSE(off.equality_basis.handed_off);
        EXPECT_EQ(off.counters.major_iters, o.max_iter);
        EXPECT_EQ(off.status, SolveStatus::kMaxIter);
        EXPECT_EQ(off.equality_basis.inconsistent_row, -1);
    }
}

// HS7 (ln(1 + x1^2) - x2 s.t. (1 + x1^2)^2 + x2^2 = 4) with its equality row
// repeated at twice the scale: a nonlinear dependence with a constant coefficient,
// so the basis drops row 1 once and never recomputes. HS7's SSN prox ladder arms
// on a certifying subproblem, which is what records a prox centre and lifts its
// multiplier (test_sqp_solver.cpp's prox-centre pin; the premise is asserted).
class DoubledHs7 final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override { return std::log(1.0 + x(0) * x(0)) - x(1); }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * x(0) / (1.0 + x(0) * x(0)), -1.0;
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        const double p = 1.0 + x(0) * x(0);
        const double c = p * p + x(1) * x(1) - 4.0;
        Vec out(2);
        out << c, 2.0 * c;
        return out;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        const double x1 = x(0);
        const double p = 1.0 + x1 * x1;
        const double le = lambda_e(0) + 2.0 * lambda_e(1);
        SpMatRM H(2, 2);
        H.insert(0, 0) = obj_scale * (2.0 - 2.0 * x1 * x1) / (p * p) + le * (4.0 + 12.0 * x1 * x1);
        H.insert(1, 1) = le * 2.0;
        H.makeCompressed();
        return H;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        const double j0 = 4.0 * x(0) * (1.0 + x(0) * x(0));
        const double j1 = 2.0 * x(1);
        SpMatRM J(2, 2);
        J.insert(0, 0) = j0;
        J.insert(0, 1) = j1;
        J.insert(1, 0) = 2.0 * j0;
        J.insert(1, 1) = 2.0 * j1;
        J.makeCompressed();
        return J;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return bound_lo_; }
    const Vec &upper() const override { return bound_hi_; }
    Vec start_point() const override { return Vec::Constant(2, 2.0); }

  private:
    Vec bound_lo_ = Vec::Constant(2, -std::numeric_limits<double>::infinity());
    Vec bound_hi_ = Vec::Constant(2, std::numeric_limits<double>::infinity());
};

TEST(SqpEqualityBasisSeam, T7FailedProxCentreLiftIsCountedAndRefusesTheBasis) {
    const SeamReset guard;
    const DoubledHs7 model;
    SqpOptions o;
    o.qp_mode = QpMode::kSsn;
    o.max_iter = 60;
    o.equality_basis = true;

    // The control arm: no seam tolerance; the basis holds and the centre is lifted.
    SqpSolver control(o);
    const SqpResult ok = control.solve(model);
    ASSERT_EQ(ok.status, SolveStatus::kOptimal);
    ASSERT_EQ(ok.warm_start.prox_center_lambda.size(), 2)
        << "fixture premise: a certifying SSN subproblem recorded a prox centre";
    EXPECT_EQ(ok.equality_basis.lift_refusals, 0);
    EXPECT_EQ(ok.equality_basis.dropped_rows, std::vector<Index>{1});

    // Every nonzero bound now fails at the prox-centre site only: counted, and the
    // basis is refused at the next build (every row back in the QPs).
    Seam::prox_lift_forward_tol = 1e-300;
    SqpSolver solver(o);
    const SqpResult res = solver.solve(model);
    EXPECT_GE(res.equality_basis.lift_refusals, 1);
    EXPECT_TRUE(res.equality_basis.dropped_rows.empty());
    EXPECT_EQ(res.equality_basis.dropped_peak, 1);
}

// The two networks of test_sqp_equality_basis.cpp's F1/F2 with a 5e-9 supply error on
// the second network's last node: the split drops one row per network and the
// projection's rhs gamma is nonzero at the start, so its estimate is positive.
TEST(SqpEqualityBasisSeam, T8RefusedWarmProbeIsCountedAndDegradesTheWarmLevel) {
    const SeamReset guard;
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(9, 11);
    const auto add = [&A](Index row0, Index col0,
                          const std::vector<std::pair<Index, Index>> &arcs) {
        for (std::size_t a = 0; a < arcs.size(); ++a) {
            A(row0 + arcs[a].first, col0 + static_cast<Index>(a)) = 1.0;
            A(row0 + arcs[a].second, col0 + static_cast<Index>(a)) = -1.0;
        }
    };
    add(0, 0, {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}});
    add(4, 5, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 4}, {1, 3}});
    Vec b(9);
    b << 1.0, 0.5, -0.25, -1.25, 2.0, -0.5, 0.0, -1.0, -0.5 + 5e-9;
    const QpAsNlpModel model(make_qp(A, b, Vec::LinSpaced(11, 1.0, 3.0),
                                     Vec::LinSpaced(11, -1.0, 1.0), Vec::Constant(11, -50.0),
                                     Vec::Constant(11, 50.0)),
                             Vec::Zero(11));
    for (const QpMode mode : {QpMode::kWalk, QpMode::kSsn, QpMode::kIpm}) {
        SCOPED_TRACE(static_cast<int>(mode));
        const SqpOptions o = opts_for(mode);
        Seam::warm_probe_forward_tol = 0.0;
        SqpSolver first(o);
        const SqpResult cold = first.solve(model);
        ASSERT_EQ(cold.status, SolveStatus::kOptimal);
        ASSERT_EQ(cold.equality_basis.dropped_rows.size(), 2u);

        // The control arm: the probe reduces and the warm start resolves kWarm.
        SqpSolver control(o);
        const SqpResult warm =
            control.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
        ASSERT_EQ(warm.status, SolveStatus::kOptimal);
        EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
        EXPECT_EQ(warm.equality_basis.warm_probe_refusals, 0);
        EXPECT_EQ(warm.equality_basis.lift_refusals, 0);

        // Every positive estimate now fails at the probe only: counted, and the level
        // degrades to kSeeded. The solve's own split is untouched.
        Seam::warm_probe_forward_tol = 1e-300;
        SqpSolver refused(o);
        const SqpResult seeded =
            refused.solve(model, model.start_point(), cold.warm_start, SolveBudget{});
        ASSERT_EQ(seeded.status, SolveStatus::kOptimal);
        EXPECT_EQ(seeded.counters.start_level_used, StartLevel::kSeeded);
        EXPECT_EQ(seeded.equality_basis.warm_probe_refusals, 1);
        EXPECT_EQ(seeded.equality_basis.lift_refusals, 1);
        EXPECT_EQ(seeded.equality_basis.dropped_rows.size(), 2u);
    }
}

} // namespace
} // namespace hven::solvers
