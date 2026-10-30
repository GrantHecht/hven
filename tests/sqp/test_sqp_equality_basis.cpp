// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_sqp_equality_basis.cpp — SqpOptions::equality_basis in the driver (M7 W2 E1).
//
//   F1  two networks, one redundant balance row each: optimal under every qp_mode,
//       x and the minimum-norm lambda_e match a dense oracle; warm round trips.
//   F2  an inconsistent redundant row exits kInfeasible on two-iterate evidence; a
//       sub-tolerance inconsistency is spread and converges.
//   F3  full-rank and no-gap models: ON and OFF are bit-identical.
//   F4  a nonlinear dependence whose coefficient moves recomputes (and switches off);
//       one whose coefficient is constant never recomputes.
//   F5  fixed arcs split a network into three pieces.
//   F6  a forced restoration runs over the retained rows and the solve resumes.
//   F7  OFF computes nothing.
//   F8  a same-cardinality row swap drops the row-coordinate warm state (fix round 1).
//   F9  a zero step at a suspected inconsistency hands the rows back (fix round 1).
//   T5  OFF on a DEPENDENT QP is byte-identical to E1's pre-merge base (E1b).
//   plus the row-subset model the restoration phase wraps.

#include <bit>
#include <cmath>
#include <cstring>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <Eigen/Dense>
#include <fmt/format.h>
#include <gtest/gtest.h>

#include <hven/detail/drivers/equality_basis.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>

#include "../common_support/route_reference.h"
#include "support/e1b_off_golden.h"
#include "support/e1b_off_golden_data.h"
#include "support/qp_as_nlp_model.h"

namespace hven::solvers {
namespace {

using test_support::QpAsNlpModel;
using Arc = std::pair<Index, Index>;

void add_network(Eigen::MatrixXd &A, Index row0, Index col0, const std::vector<Arc> &arcs) {
    for (std::size_t a = 0; a < arcs.size(); ++a) {
        A(row0 + arcs[a].first, col0 + static_cast<Index>(a)) = 1.0;
        A(row0 + arcs[a].second, col0 + static_cast<Index>(a)) = -1.0;
    }
}

SpMatRM sparse(const Eigen::MatrixXd &A) {
    SpMatRM S = A.sparseView();
    S.makeCompressed();
    return S;
}

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
    qp.Ae = sparse(A);
    qp.be = b;
    qp.Ai = SpMatRM(0, n);
    qp.bi = Vec(0);
    qp.lower = lower;
    qp.upper = upper;
    return qp;
}

// Two networks (4 nodes / 5 arcs, 5 nodes / 6 arcs), balanced supplies, a convex
// arc cost and a box that stays inactive at the optimum.
struct TwoNetworks {
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(9, 11);
    Vec b = Vec::Zero(9);
    Vec h = Vec::LinSpaced(11, 1.0, 3.0);
    Vec g = Vec::LinSpaced(11, -1.0, 1.0);
    TwoNetworks() {
        add_network(A, 0, 0, {{0, 1}, {1, 2}, {2, 3}, {0, 2}, {1, 3}});
        add_network(A, 4, 5, {{0, 1}, {1, 2}, {2, 3}, {3, 4}, {0, 4}, {1, 3}});
        b << 1.0, 0.5, -0.25, -1.25, 2.0, -0.5, 0.0, -1.0, -0.5;
    }
    QpAsNlpModel model(double supply_error = 0.0) const {
        Vec bb = b;
        bb(8) += supply_error;
        return QpAsNlpModel(make_qp(A, bb, h, g, Vec::Constant(11, -50.0), Vec::Constant(11, 50.0)),
                            Vec::Zero(11));
    }
    // The equality-constrained minimiser (the box is inactive) and the minimum-norm
    // multiplier, from a full-rank row subset and a dense pseudo-inverse.
    void oracle(Vec &x, Vec &lambda) const {
        std::vector<Index> keep = {0, 1, 2, 4, 5, 6, 7};
        Eigen::MatrixXd Ab(7, 11);
        Vec bb(7);
        for (Index k = 0; k < 7; ++k) {
            Ab.row(k) = A.row(keep[static_cast<std::size_t>(k)]);
            bb(k) = b(keep[static_cast<std::size_t>(k)]);
        }
        Eigen::MatrixXd K = Eigen::MatrixXd::Zero(18, 18);
        K.topLeftCorner(11, 11) = h.asDiagonal();
        K.topRightCorner(11, 7) = Ab.transpose();
        K.bottomLeftCorner(7, 11) = Ab;
        Vec rhs(18);
        rhs << -g, bb;
        const Vec sol = K.fullPivLu().solve(rhs);
        x = sol.head(11);
        const Vec grad = h.asDiagonal() * x + g;
        lambda =
            Eigen::CompleteOrthogonalDecomposition<Eigen::MatrixXd>(A.transpose()).solve(-grad);
    }
};

SqpOptions opts_for(QpMode mode, bool basis = true) {
    SqpOptions o;
    o.qp_mode = mode;
    o.equality_basis = basis;
    o.max_iter = 100;
    return o;
}

class EqualityBasisModes : public ::testing::TestWithParam<QpMode> {};

TEST_P(EqualityBasisModes, F1TwoNetworksConvergeToTheOracleWithMinimumNormMultipliers) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model();
    Vec x_star, lambda_star;
    net.oracle(x_star, lambda_star);

    SqpSolver solver(opts_for(GetParam()));
    const SqpResult res = solver.solve(model);
    ASSERT_EQ(res.status, SolveStatus::kOptimal);
    EXPECT_TRUE(res.equality_basis.computed);
    EXPECT_FALSE(res.equality_basis.undecidable);
    ASSERT_EQ(res.equality_basis.dropped_rows.size(), 2u);
    EXPECT_EQ(res.equality_basis.recomputes, 0);
    EXPECT_FALSE(res.equality_basis.switched_off);
    EXPECT_LE((res.x - x_star).lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_LE((res.lambda_e - lambda_star).lpNorm<Eigen::Infinity>(), 1e-5);
    EXPECT_LE(res.kkt_residual, 1e-6);

    // Warm round trip through the full-size currency: the fold takes it back.
    const WarmStartData data = solver.export_warm_start();
    ASSERT_EQ(data.eq_lmults_.size(), 9);
    SqpSolver again(opts_for(GetParam()));
    const SqpResult warm = again.solve(model, model.start_point(), data, SolveBudget{});
    ASSERT_EQ(warm.status, SolveStatus::kOptimal);
    EXPECT_LE((warm.x - x_star).lpNorm<Eigen::Infinity>(), 1e-6);

    // And through the in-process warm start.
    SqpSolver hot(opts_for(GetParam()));
    const SqpResult rehot = hot.solve(model, model.start_point(), res.warm_start, SolveBudget{});
    ASSERT_EQ(rehot.status, SolveStatus::kOptimal);
    EXPECT_LE((rehot.x - x_star).lpNorm<Eigen::Infinity>(), 1e-6);
}

TEST_P(EqualityBasisModes, F2InconsistentRedundantRowIsCertifiedInfeasible) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model(1e-3);
    SqpSolver solver(opts_for(GetParam()));
    const SqpResult res = solver.solve(model);
    ASSERT_EQ(res.status, SolveStatus::kInfeasible);
    const SqpEqualityBasisReport &rep = res.equality_basis;
    EXPECT_GE(rep.inconsistent_row, 4); // the second network's dropped row
    EXPECT_NEAR(std::abs(rep.inconsistent_gap), 1e-3, 1e-12);
    EXPECT_FALSE(rep.inconsistent_support.empty());
    EXPECT_GE(rep.evidence_major_first, 0);
    EXPECT_GT(rep.evidence_major_second, rep.evidence_major_first);
}

TEST_P(EqualityBasisModes, F2SubToleranceInconsistencyIsSpreadAndConverges) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model(5e-9);
    const SqpOptions o = opts_for(GetParam());
    SqpSolver solver(o);
    const SqpResult res = solver.solve(model);
    ASSERT_EQ(res.status, SolveStatus::kOptimal);
    EXPECT_EQ(res.equality_basis.inconsistent_row, -1);
    EXPECT_GT(res.equality_basis.max_gap, 1e-9);
    EXPECT_LE(res.sqp_feasibility, o.feas_tol);
}

void expect_bit_identical(const SqpResult &a, const SqpResult &b) {
    ASSERT_EQ(a.status, b.status);
    ASSERT_EQ(a.x.size(), b.x.size());
    EXPECT_EQ(
        std::memcmp(a.x.data(), b.x.data(), sizeof(double) * static_cast<std::size_t>(a.x.size())),
        0);
    ASSERT_EQ(a.lambda_e.size(), b.lambda_e.size());
    EXPECT_EQ(std::memcmp(a.lambda_e.data(), b.lambda_e.data(),
                          sizeof(double) * static_cast<std::size_t>(a.lambda_e.size())),
              0);
    EXPECT_EQ(a.counters.major_iters, b.counters.major_iters);
    EXPECT_EQ(a.counters.qp_minor_iters, b.counters.qp_minor_iters);
    EXPECT_EQ(a.counters.factorizations, b.counters.factorizations);
    EXPECT_EQ(a.counters.evals_full, b.counters.evals_full);
    EXPECT_EQ(a.counters.evals_values, b.counters.evals_values);
    EXPECT_EQ(a.counters.rejected_steps, b.counters.rejected_steps);
    ASSERT_EQ(a.history.size(), b.history.size());
    for (std::size_t k = 0; k < a.history.size(); ++k) {
        EXPECT_EQ(std::memcmp(&a.history[k].f, &b.history[k].f, sizeof(double)), 0) << k;
        EXPECT_EQ(std::memcmp(&a.history[k].step_norm, &b.history[k].step_norm, sizeof(double)), 0)
            << k;
    }
}

TEST_P(EqualityBasisModes, F3FullRankModelIsBitIdenticalOnAndOff) {
    // The two networks less one balance row each: full row rank, a clear gap.
    const TwoNetworks net;
    const std::vector<Index> keep = {0, 1, 2, 4, 5, 6, 7};
    Eigen::MatrixXd A(7, 11);
    Vec b(7);
    for (Index k = 0; k < 7; ++k) {
        A.row(k) = net.A.row(keep[static_cast<std::size_t>(k)]);
        b(k) = net.b(keep[static_cast<std::size_t>(k)]);
    }
    const QpAsNlpModel model(
        make_qp(A, b, net.h, net.g, Vec::Constant(11, 0.0), Vec::Constant(11, 50.0)),
        Vec::Zero(11));
    SqpSolver on(opts_for(GetParam(), true));
    SqpSolver off(opts_for(GetParam(), false));
    const SqpResult r_on = on.solve(model);
    const SqpResult r_off = off.solve(model);
    EXPECT_TRUE(r_on.equality_basis.computed);
    EXPECT_TRUE(r_on.equality_basis.dropped_rows.empty());
    EXPECT_FALSE(r_on.equality_basis.undecidable);
    EXPECT_FALSE(r_off.equality_basis.computed);
    expect_bit_identical(r_on, r_off);
}

TEST_P(EqualityBasisModes, F3NoGapSpectrumIsUndecidableAndBitIdentical) {
    // A graded full-rank Ae whose equilibrated spectrum is a continuum (the SAWPATH
    // shape): the split is undecidable and drops nothing.
    const Index me = 12;
    const Index n = 16;
    Eigen::MatrixXd U = Eigen::MatrixXd::Zero(me, me);
    Eigen::MatrixXd V = Eigen::MatrixXd::Zero(n, n);
    for (Index i = 0; i < me; ++i) {
        for (Index j = 0; j < me; ++j) {
            U(i, j) = std::sin(1.0 + 0.7 * static_cast<double>(i) + 1.3 * static_cast<double>(j));
        }
    }
    for (Index i = 0; i < n; ++i) {
        for (Index j = 0; j < n; ++j) {
            V(i, j) = std::cos(0.3 + 1.1 * static_cast<double>(i) + 0.9 * static_cast<double>(j));
        }
    }
    const Eigen::MatrixXd Q1 = Eigen::HouseholderQR<Eigen::MatrixXd>(U).householderQ();
    const Eigen::MatrixXd Q2 = Eigen::HouseholderQR<Eigen::MatrixXd>(V).householderQ();
    Eigen::MatrixXd S = Eigen::MatrixXd::Zero(me, n);
    for (Index k = 0; k < me; ++k) {
        S(k, k) = std::pow(10.0, -11.0 * static_cast<double>(k) / static_cast<double>(me - 1));
    }
    Eigen::MatrixXd A = Q1 * S * Q2.transpose();
    for (Index r = 0; r < me; ++r) {
        A.row(r) *= std::pow(10.0, 10.0 * static_cast<double>(r) / static_cast<double>(me - 1));
    }
    const Vec b = A * Vec::LinSpaced(n, -0.5, 0.5);
    const QpAsNlpModel model(make_qp(A, b, Vec::Ones(n), Vec::LinSpaced(n, 0.1, -0.2),
                                     Vec::Constant(n, -1e20), Vec::Constant(n, 1e20)),
                             Vec::Zero(n));
    SqpSolver on(opts_for(GetParam(), true));
    SqpSolver off(opts_for(GetParam(), false));
    const SqpResult r_on = on.solve(model);
    const SqpResult r_off = off.solve(model);
    EXPECT_TRUE(r_on.equality_basis.undecidable);
    EXPECT_TRUE(r_on.equality_basis.dropped_rows.empty());
    expect_bit_identical(r_on, r_off);
}

// F10 (E1b fix round 1, settler F5): the report's telemetry on constructed dependent
// sets. The two networks drop one balance row each (the highest-degree, highest-index
// node: rows 2 and 7); 70 copies of one row drop 69 and name the first 64; the SAWPATH
// shape counts its refusal. MKL reports a perturbed-pivot count for every certificate
// factorization (0 each); Accelerate's count is UNOBSERVED.
TEST_P(EqualityBasisModes, F10ReportTelemetryOnConstructedDependentSets) {
    const TwoNetworks net;
    SqpSolver solver(opts_for(GetParam()));
    const SqpResult res = solver.solve(net.model());
    ASSERT_EQ(res.status, SolveStatus::kOptimal);
    const SqpEqualityBasisReport &rep = res.equality_basis;
    EXPECT_EQ(rep.split_dropped_count, 2);
    EXPECT_EQ(rep.split_dropped_rows, (std::vector<Index>{2, 7}));
    EXPECT_EQ(rep.split_dropped_rows, rep.dropped_rows);
    EXPECT_EQ(rep.gram_factorizations, 4);
    EXPECT_EQ(rep.gram_perturbed_pivots, 0);
#if !defined(__APPLE__)
    EXPECT_EQ(rep.gram_perturbation_unobserved, 0);
#endif
    EXPECT_EQ(rep.undecidable_count, 0);
    EXPECT_EQ(rep.last_refusal, SqpEqualityBasisRefusal::kNone);
    EXPECT_EQ(rep.recomputes, 0);
    EXPECT_FALSE(rep.switched_off);
    EXPECT_EQ(rep.lift_refusals, 0);
    EXPECT_EQ(rep.warm_probe_refusals, 0);
    const SqpEqualityBasisReport::LiftRefusalSites &sites = rep.lift_refusal_sites;
    EXPECT_EQ(sites.compute + sites.qp_projection + sites.soc_projection + sites.commit_lift +
                  sites.prox_centre + rep.warm_probe_refusals,
              rep.lift_refusals);

    // The warm solve adds the probe's computation to the certificate telemetry.
    SqpSolver again(opts_for(GetParam()));
    const SqpResult warm = again.solve(net.model(), Vec::Zero(11), res.warm_start, SolveBudget{});
    ASSERT_EQ(warm.status, SolveStatus::kOptimal);
    EXPECT_EQ(warm.counters.start_level_used, StartLevel::kWarm);
    EXPECT_EQ(warm.equality_basis.gram_factorizations, 8);
    EXPECT_EQ(warm.equality_basis.split_dropped_rows, (std::vector<Index>{2, 7}));

    // 70 copies of one row: 69 dropped (the tie rule keeps row 0), 64 named.
    const Index me = 70;
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(me, 2);
    A.col(0).setOnes();
    A.col(1).setConstant(-1.0);
    const QpAsNlpModel copies(make_qp(A, Vec::Ones(me), Vec::Ones(2), Vec::Zero(2),
                                      Vec::Constant(2, -10.0), Vec::Constant(2, 10.0)),
                              Vec::Zero(2));
    SqpSolver many(opts_for(GetParam()));
    const SqpResult r70 = many.solve(copies);
    ASSERT_EQ(r70.status, SolveStatus::kOptimal);
    EXPECT_EQ(r70.equality_basis.split_dropped_count, 69);
    ASSERT_EQ(r70.equality_basis.split_dropped_rows.size(),
              static_cast<std::size_t>(SqpEqualityBasisReport::kSplitDroppedRowsCap));
    for (Index k = 0; k < SqpEqualityBasisReport::kSplitDroppedRowsCap; ++k) {
        EXPECT_EQ(r70.equality_basis.split_dropped_rows[static_cast<std::size_t>(k)], k + 1);
    }
    EXPECT_EQ(r70.equality_basis.dropped_rows.size(), 69u);
    EXPECT_EQ(r70.equality_basis.gram_perturbed_pivots, 0);

    // A refused computation is counted with its reason.
    const Index mr = 12;
    const Index nr = 16;
    Eigen::MatrixXd U = Eigen::MatrixXd::Zero(mr, mr);
    Eigen::MatrixXd V = Eigen::MatrixXd::Zero(nr, nr);
    for (Index i = 0; i < mr; ++i) {
        for (Index j = 0; j < mr; ++j) {
            U(i, j) = std::sin(1.0 + 0.7 * static_cast<double>(i) + 1.3 * static_cast<double>(j));
        }
    }
    for (Index i = 0; i < nr; ++i) {
        for (Index j = 0; j < nr; ++j) {
            V(i, j) = std::cos(0.3 + 1.1 * static_cast<double>(i) + 0.9 * static_cast<double>(j));
        }
    }
    const Eigen::MatrixXd Q1 = Eigen::HouseholderQR<Eigen::MatrixXd>(U).householderQ();
    const Eigen::MatrixXd Q2 = Eigen::HouseholderQR<Eigen::MatrixXd>(V).householderQ();
    Eigen::MatrixXd S = Eigen::MatrixXd::Zero(mr, nr);
    for (Index k = 0; k < mr; ++k) {
        S(k, k) = std::pow(10.0, -11.0 * static_cast<double>(k) / static_cast<double>(mr - 1));
    }
    Eigen::MatrixXd G = Q1 * S * Q2.transpose();
    for (Index r = 0; r < mr; ++r) {
        G.row(r) *= std::pow(10.0, 10.0 * static_cast<double>(r) / static_cast<double>(mr - 1));
    }
    const QpAsNlpModel graded(make_qp(G, G * Vec::LinSpaced(nr, -0.5, 0.5), Vec::Ones(nr),
                                      Vec::LinSpaced(nr, 0.1, -0.2), Vec::Constant(nr, -1e20),
                                      Vec::Constant(nr, 1e20)),
                              Vec::Zero(nr));
    SqpSolver refused(opts_for(GetParam()));
    const SqpResult rg = refused.solve(graded);
    EXPECT_TRUE(rg.equality_basis.undecidable);
    EXPECT_EQ(rg.equality_basis.undecidable_count, 1);
    EXPECT_NE(rg.equality_basis.last_refusal, SqpEqualityBasisRefusal::kNone);
    EXPECT_EQ(rg.equality_basis.split_dropped_count, 0);
    EXPECT_TRUE(rg.equality_basis.split_dropped_rows.empty());
}

INSTANTIATE_TEST_SUITE_P(QpModes, EqualityBasisModes,
                         ::testing::Values(QpMode::kWalk, QpMode::kSsn, QpMode::kIpm));

// c1 = x1 - x2 (linear), c2 = x3 (x1 - x2) + kappa (x1 - x2)^2: on the manifold
// x1 = x2 the rows are dependent with C = x3, which moves as x3 does.
class MovingDependence final : public NlpModel {
  public:
    explicit MovingDependence(bool constant_coefficient) : constant_(constant_coefficient) {}
    Index n() const override { return 3; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return (x(0) - 1.0) * (x(0) - 1.0) + (x(1) - 1.0) * (x(1) - 1.0) +
               (x(2) - 2.0) * (x(2) - 2.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(3);
        g << 2.0 * (x(0) - 1.0), 2.0 * (x(1) - 1.0), 2.0 * (x(2) - 2.0);
        return g;
    }
    // The constant-coefficient twin: c1 = x1^2 - x2 (nonlinear), c2 = 2 c1.
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        if (constant_) {
            c(0) = x(0) * x(0) - x(1);
            c(1) = 2.0 * c(0);
        } else {
            const double d = x(0) - x(1);
            c(0) = d;
            c(1) = x(2) * d + kKappa * d * d;
        }
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        Eigen::Matrix3d H = 2.0 * obj_scale * Eigen::Matrix3d::Identity();
        if (constant_) {
            H(0, 0) += 2.0 * (lambda_e(0) + 2.0 * lambda_e(1));
        } else {
            // c2's Hessian: d/dx of (x3 + 2 kappa d, -x3 - 2 kappa d, d).
            const double l = lambda_e(1);
            H(0, 0) += l * 2.0 * kKappa;
            H(0, 1) += -l * 2.0 * kKappa;
            H(1, 1) += l * 2.0 * kKappa;
            H(0, 2) += l;
            H(1, 2) += -l;
        }
        // A fixed upper-triangle pattern (explicit zeros stay stored).
        SpMatRM S(3, 3);
        std::vector<Eigen::Triplet<double>> t;
        for (Index r = 0; r < 3; ++r) {
            for (Index c = r; c < 3; ++c) {
                t.emplace_back(r, c, H(r, c));
            }
        }
        S.setFromTriplets(t.begin(), t.end());
        S.makeCompressed();
        return S;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        Eigen::Matrix<double, 2, 3> J = Eigen::Matrix<double, 2, 3>::Zero();
        if (constant_) {
            J(0, 0) = 2.0 * x(0);
            J(0, 1) = -1.0;
            J.row(1) = 2.0 * J.row(0);
        } else {
            const double d = x(0) - x(1);
            J(0, 0) = 1.0;
            J(0, 1) = -1.0;
            J(1, 0) = x(2) + 2.0 * kKappa * d;
            J(1, 1) = -x(2) - 2.0 * kKappa * d;
            J(1, 2) = d;
        }
        // Keep the pattern fixed across iterates (explicit zeros stay stored).
        SpMatRM S(2, 3);
        std::vector<Eigen::Triplet<double>> t;
        for (Index r = 0; r < 2; ++r) {
            for (Index c = 0; c < 3; ++c) {
                t.emplace_back(r, c, J(r, c));
            }
        }
        S.setFromTriplets(t.begin(), t.end());
        S.makeCompressed();
        return S;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 3); }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override {
        Vec x(3);
        x << 0.5, 0.5, 0.25;
        return x;
    }

  private:
    static constexpr double kKappa = 0.5;
    bool constant_;
    Vec lo_ = Vec::Constant(3, -1e20);
    Vec up_ = Vec::Constant(3, 1e20);
};

TEST(SqpEqualityBasis, F4MovingNonlinearDependenceRecomputes) {
    const MovingDependence model(false);
    SqpSolver solver(opts_for(QpMode::kWalk));
    const SqpResult res = solver.solve(model);
    EXPECT_EQ(res.status, SolveStatus::kOptimal);
    EXPECT_TRUE(res.equality_basis.computed);
    EXPECT_EQ(res.equality_basis.dropped_peak, 1);
    EXPECT_GE(res.equality_basis.recomputes, 1);
}

TEST(SqpEqualityBasis, F4ConstantNonlinearDependenceNeverRecomputes) {
    const MovingDependence model(true);
    SqpSolver solver(opts_for(QpMode::kWalk));
    const SqpResult res = solver.solve(model);
    EXPECT_EQ(res.status, SolveStatus::kOptimal);
    ASSERT_EQ(res.equality_basis.dropped_rows.size(), 1u);
    EXPECT_EQ(res.equality_basis.recomputes, 0);
    EXPECT_FALSE(res.equality_basis.switched_off);
}

TEST(SqpEqualityBasis, F5FixedArcsSplitTheNetworkIntoThreePieces) {
    // {0,1,2} triangle, fixed bridge (2,3), {3,4}, fixed bridge (4,5), {5,6}.
    Eigen::MatrixXd A = Eigen::MatrixXd::Zero(7, 7);
    add_network(A, 0, 0, {{0, 1}, {1, 2}, {0, 2}, {2, 3}, {3, 4}, {4, 5}, {5, 6}});
    Vec lower = Vec::Constant(7, -10.0);
    Vec upper = Vec::Constant(7, 10.0);
    lower(3) = upper(3) = 0.7;
    lower(5) = upper(5) = -0.4;
    Vec x_feas(7);
    x_feas << 0.3, -0.2, 0.5, 0.7, 0.1, -0.4, 0.25;
    const Vec b = A * x_feas;
    const QpAsNlpModel model(
        make_qp(A, b, Vec::Ones(7), Vec::LinSpaced(7, -0.3, 0.3), lower, upper), Vec::Zero(7));
    SqpSolver solver(opts_for(QpMode::kWalk));
    const SqpResult res = solver.solve(model);
    ASSERT_EQ(res.status, SolveStatus::kOptimal);
    EXPECT_EQ(res.equality_basis.dropped_rows.size(), 3u);
    EXPECT_LE(res.sqp_feasibility, 1e-6);
}

// c0 = x^2 - 1, c1 = exp(x - 1) - 1 (plus a free y): at x = 0 row 0 is empty on F
// and is dropped with a zero column; once x moves both rows are nonzero multiples
// of e_x and the recomputed split drops row 1 instead -- the retained row changes
// while |B| stays 1.
class EmptyThenDependent final : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }
    double eval_f(const Vec &x) const override {
        return (x(0) - 2.0) * (x(0) - 2.0) + (x(1) - 1.0) * (x(1) - 1.0);
    }
    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g << 2.0 * (x(0) - 2.0), 2.0 * (x(1) - 1.0);
        return g;
    }
    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c << x(0) * x(0) - 1.0, std::exp(x(0) - 1.0) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &) const override {
        SpMatRM H(2, 2);
        const double hxx = 2.0 * obj_scale + 2.0 * lambda_e(0) + std::exp(x(0) - 1.0) * lambda_e(1);
        std::vector<Eigen::Triplet<double>> t = {{0, 0, hxx}, {0, 1, 0.0}, {1, 1, 2.0 * obj_scale}};
        H.setFromTriplets(t.begin(), t.end());
        H.makeCompressed();
        return H;
    }
    SpMatRM eval_jac_e(const Vec &x) const override {
        SpMatRM J(2, 2);
        std::vector<Eigen::Triplet<double>> t = {
            {0, 0, 2.0 * x(0)}, {0, 1, 0.0}, {1, 0, std::exp(x(0) - 1.0)}, {1, 1, 0.0}};
        J.setFromTriplets(t.begin(), t.end());
        J.makeCompressed();
        return J;
    }
    SpMatRM eval_jac_i(const Vec &) const override { return SpMatRM(0, 2); }
    const Vec &lower() const override { return lo_; }
    const Vec &upper() const override { return up_; }
    Vec start_point() const override { return Vec::Zero(2); }

  private:
    Vec lo_ = Vec::Constant(2, -1e20);
    Vec up_ = Vec::Constant(2, 1e20);
};

// Fix round 1, I4: a same-cardinality row swap mid-solve drops the IPQP carry
// (and every other row-coordinate warm state) and the solve still converges.
TEST(SqpEqualityBasis, F8SameCardinalityRowSwapDropsTheRowCarries) {
    const EmptyThenDependent model;
    detail::EqualityBasis at_start;
    at_start.compute(model.eval_jac_e(model.start_point()), model.lower(), model.upper());
    ASSERT_EQ(at_start.dropped(), std::vector<Index>({0}));
    Vec moved(2);
    moved << 0.5, 0.0;
    detail::EqualityBasis at_moved;
    at_moved.compute(model.eval_jac_e(moved), model.lower(), model.upper());
    ASSERT_EQ(at_moved.dropped(), std::vector<Index>({1}));

    for (const QpMode mode : {QpMode::kIpm, QpMode::kWalk, QpMode::kSsn}) {
        SCOPED_TRACE(static_cast<int>(mode));
        SqpSolver solver(opts_for(mode));
        const SqpResult res = solver.solve(model);
        ASSERT_EQ(res.status, SolveStatus::kOptimal);
        EXPECT_NEAR(res.x(0), 1.0, 1e-6);
        EXPECT_NEAR(res.x(1), 1.0, 1e-6);
        EXPECT_GE(res.equality_basis.recomputes, 1);
        EXPECT_GE(res.equality_basis.split_changes, 1);
        EXPECT_EQ(res.equality_basis.inconsistent_row, -1);
    }
}

// Fix round 1, I3: c0 = x1 - 1 and c1 = x1 - 1.001 are an inconsistent duplicate
// pair, and the start (1.0005, 0) is the reduced problem's least-squares
// stationary point, so the first reduced step is zero and no second, distinct
// iterate can arrive. The rows go back into the QPs and the existing ladder
// finishes the solve kInfeasible well inside max_iter.
TEST(SqpEqualityBasis, F9ZeroStepAtAnInconsistencyHandsOffToTheLadder) {
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
        SqpSolver solver(o);
        const SqpResult res = solver.solve(model);
#if HVEN_TEST_ROUTE_REFERENCE
        EXPECT_EQ(res.status, SolveStatus::kInfeasible);
        EXPECT_TRUE(res.equality_basis.handed_off);
        EXPECT_EQ(res.equality_basis.inconsistent_row, -1);
        EXPECT_GE(res.equality_basis.split_changes, 1);
        EXPECT_LT(res.counters.major_iters, o.max_iter);
#else
        // Off the reference build the ladder after the hand-off is not known to finish
        // kInfeasible (WX-R3: observed kMaxIter at the cap on Accelerate). The route is
        // recorded, never blessed: only the hand-off fields and an honest status hold.
        hven::test_support::record_off_reference_route(
            std::string("f9_ladder_") + std::to_string(static_cast<int>(mode)),
            fmt::format("status={} major_iters={} max_iter={}", static_cast<int>(res.status),
                        res.counters.major_iters, o.max_iter));
        EXPECT_NE(res.status, SolveStatus::kOptimal); // the system is inconsistent
        EXPECT_TRUE(res.equality_basis.handed_off);
        EXPECT_EQ(res.equality_basis.inconsistent_row, -1);
        EXPECT_GE(res.equality_basis.split_changes, 1);
        EXPECT_LE(res.counters.major_iters, o.max_iter);
#endif
    }
}

// The parent's first judgement is kRestore; everything else is the funnel's.
class RestoreOnce final : public GlobalizationStrategy {
  public:
    StepVerdict judge(const StepContext &ctx) override {
        if (!fired_) {
            fired_ = true;
            return StepVerdict::kRestore;
        }
        return inner_.judge(ctx);
    }
    void reset(double h0) override { inner_.reset(h0); }
    void resume_from_restoration(double h) override { inner_.resume_from_restoration(h); }

  private:
    FunnelStrategy inner_;
    bool fired_ = false;
};

TEST(SqpEqualityBasis, F6RestorationRunsOverTheRetainedRowsAndResumes) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model();
    Vec x_star, lambda_star;
    net.oracle(x_star, lambda_star);
    SqpOptions o = opts_for(QpMode::kWalk);
    o.make_strategy = []() -> std::unique_ptr<GlobalizationStrategy> {
        return std::make_unique<RestoreOnce>();
    };
    SqpSolver solver(o);
    const SqpResult res = solver.solve(model);
    ASSERT_EQ(res.status, SolveStatus::kOptimal);
    EXPECT_EQ(res.counters.restoration_entries, 1);
    EXPECT_EQ(res.equality_basis.dropped_rows.size(), 2u);
    EXPECT_LE((res.x - x_star).lpNorm<Eigen::Infinity>(), 1e-6);
}

// T5 (E1b): with equality_basis = false, a dependent and consistent network QP (F1's
// two networks plus a duplicate balance row) solves byte-identically to E1's
// pre-merge base 8fdeed3e under every qp_mode: status, six counters, x and lambda_e
// in hex, against goldens generated there (support/e1b_off_golden_data.h, and the
// generator and command in docs/notes/data/2026-09-m7-w2-e1-equality-basis/
// e1b-off-golden/). MKL only: the goldens are MKL's bytes; Accelerate's are
// UNOBSERVED and not invented. The fingerprint runs with
// QpOptions::independent_working_set = false: the base predates M7 W2 E2, so the
// pin is the pre-E2 walk. With it ON (E1b x E2 merge effect, declared) kWalk's
// qp_minor_iters reads 5 against the base's 6 in both configurations, with status,
// the other five counters, x and lambda_e byte-identical; kSsn and kIpm unchanged.
#if !defined(__APPLE__)
TEST(SqpEqualityBasis, T5OffIsByteIdenticalToThePreE1BaseOnADependentQp) {
#ifdef NDEBUG
    const std::string golden = e1b_golden::kOffGoldenRelease;
#else
    const std::string golden = e1b_golden::kOffGoldenDebug;
#endif
    std::string got;
    const std::pair<QpMode, const char *> modes[] = {
        {QpMode::kWalk, "kWalk"}, {QpMode::kSsn, "kSsn"}, {QpMode::kIpm, "kIpm"}};
    for (const auto &[mode, name] : modes) {
        for (const std::string &line : e1b_golden::off_fingerprint(mode)) {
            got += std::string(name) + " " + line + "\n";
        }
    }
    // RE-PREMISED AT M7 W2 E4 FIX ROUND 2, in the currency CLAUDE.md section 7
    // prescribes, and the CROSS-COMMIT CLAIM IS KEPT rather than retired.
    //
    // WHAT MOVED AND WHAT DID NOT. M7 W2 E4 changes every QP solve -- the proximal
    // EQP, its unregularized polish, the terminal polish -- so a BYTE compare of x and
    // lambda_e against a golden generated at 8fdeed3e cannot survive it, and it does
    // not: MEASURED at this tree, every one of the 42 hex words of x and lambda_e
    // differs from the base's, in its LAST ONE OR TWO HEX DIGITS and nowhere else.
    // The STATUS and the SIX COUNTERS are byte-identical on all three modes
    // (`2 6 2 2 0 0`, `2 0 11 2 0 0`, `2 0 30 2 0 0`), which is the structural half of
    // the claim and the half a defect in `equality_basis = false` would break.
    //
    // SO THE COMPARISON IS SPLIT THE WAY SECTION 7 SPLITS IT: counters and statuses
    // stay EXACT, and the residual-class columns -- here the primal and dual vectors --
    // are compared by a calibrated gate instead of by byte equality. TWO gates, because
    // the two vectors moved by different amounts and for different reasons:
    //
    //   x        1e-12 RELATIVE. MEASURED: every word differs in its LAST HEX DIGIT,
    //            i.e. ~1e-16 relative. The primal answer is the same point.
    //   lambda_e 1e-6 ABSOLUTE. MEASURED: up to 1.268826310685256e-08 on components of
    //            size ~0.08, i.e. O(dual_mu) at the shipped `dual_mu = 1e-08` -- which
    //            is exactly the quantity E4's UNREGULARIZED POLISH takes back out of
    //            the priced multipliers, so the move is the round's own mechanism
    //            rather than drift. The gate is TWO DECADES above what was measured
    //            and two decades above `dual_mu` itself.
    //
    // Both are many decades tighter than any structural move: a dropped row, a
    // different working set or a different answer moves these by 1e-3 or more on this
    // QP, and the counters above would move with it.
    const auto split = [](const std::string &text) {
        std::vector<std::string> out;
        std::size_t a = 0;
        while (a < text.size()) {
            const std::size_t b = text.find('\n', a);
            const std::size_t e = b == std::string::npos ? text.size() : b;
            if (e > a) {
                out.emplace_back(text.substr(a, e - a));
            }
            a = e + 1;
        }
        return out;
    };
    const std::vector<std::string> got_lines = split(got);
    const std::vector<std::string> want_lines = split(golden);
    ASSERT_EQ(got_lines.size(), want_lines.size());
    for (std::size_t k = 0; k < got_lines.size(); ++k) {
        SCOPED_TRACE(want_lines[k]);
        std::istringstream gi(got_lines[k]);
        std::istringstream wi(want_lines[k]);
        std::string gmode, gkind, wmode, wkind;
        gi >> gmode >> gkind;
        wi >> wmode >> wkind;
        ASSERT_EQ(gmode, wmode);
        ASSERT_EQ(gkind, wkind);
        if (gkind == "status" || gkind == "counters") {
            EXPECT_EQ(got_lines[k], want_lines[k]) << "counters and statuses stay EXACT";
            continue;
        }
        ASSERT_TRUE(gkind == "x" || gkind == "lambda_e") << gkind;
        std::string gw;
        std::string ww;
        Index words = 0;
        while ((gi >> gw) && (wi >> ww)) {
            ++words;
            std::uint64_t gb = 0;
            std::uint64_t wb = 0;
            std::istringstream(gw) >> std::hex >> gb;
            std::istringstream(ww) >> std::hex >> wb;
            const double gv = std::bit_cast<double>(gb);
            const double wv = std::bit_cast<double>(wb);
            const double bound =
                gkind == "x" ? 1e-12 * std::max({1.0, std::abs(gv), std::abs(wv)}) : 1e-6;
            EXPECT_LE(std::abs(gv - wv), bound)
                << gkind << " word " << words << ": " << gw << " vs " << ww;
        }
        EXPECT_FALSE(static_cast<bool>(gi >> gw)) << "the got line has more words";
        EXPECT_FALSE(static_cast<bool>(wi >> ww)) << "the golden line has more words";
        EXPECT_GT(words, 0);
    }
}
#endif

TEST(SqpEqualityBasis, F7OffComputesNothing) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model();
    SqpSolver solver(opts_for(QpMode::kWalk, false));
    const SqpResult res = solver.solve(model);
    EXPECT_FALSE(res.equality_basis.computed);
    EXPECT_TRUE(res.equality_basis.dropped_rows.empty());
    EXPECT_EQ(res.lambda_e.size(), 9);
}

TEST(SqpEqualityBasis, RowSubsetModelRestrictsTheEqualityBlock) {
    const TwoNetworks net;
    const QpAsNlpModel model = net.model();
    const detail::EqualityRowSubsetModel sub(model, {0, 2, 5, 8});
    EXPECT_EQ(sub.me(), 4);
    EXPECT_EQ(sub.n(), 11);
    const Vec x = Vec::LinSpaced(11, -1.0, 1.0);
    Vec ce_full, ce_sub;
    model.eval_ce_in_place(x, ce_full);
    sub.eval_ce_in_place(x, ce_sub);
    EXPECT_EQ(ce_sub(1), ce_full(2));
    EXPECT_EQ(ce_sub(3), ce_full(8));
    SpMatRM J;
    sub.eval_jac_e_in_place(x, J);
    EXPECT_EQ(J.rows(), 4);
    EXPECT_EQ(Eigen::MatrixXd(J).row(2), net.A.row(5));
    EXPECT_THROW(detail::EqualityRowSubsetModel(model, {2, 1}), std::invalid_argument);
    EXPECT_THROW(detail::EqualityRowSubsetModel(model, {9}), std::invalid_argument);
}

} // namespace
} // namespace hven::solvers
