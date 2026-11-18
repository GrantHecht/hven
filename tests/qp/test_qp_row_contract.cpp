// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// test_qp_row_contract.cpp -- kOptimal's row contract: every equality row and working
// inequality row is within 1x of row_tolerance (the 10x margin is the kInfeasible
// classifier's alone). The fixture is the flat-normal synthetic: n = 2, H the projector onto
// null(a), so H a = 0 and the objective offers the walk no curvature along the row normal;
// the seed sits k floor units off the row along that normal. Before the contract the walk
// certified kOptimal at the seed for k in (1, 10).

#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include <hven/detail/qp/walk/walk_engine.h>

using namespace hven;
using namespace hven::detail;
using hven::Index;
using hven::Vec;

namespace {

constexpr QpWorkingSetAlgebra kBothModes[] = {QpWorkingSetAlgebra::kSchurBorder,
                                              QpWorkingSetAlgebra::kRefactorize};

// min 1/2 x'Hx + g'x s.t. x0 + x1 = 0, H = I - aa'/|a|^2 (a = (1, 1)), g = (-1, 1): optimum
// x = (1, -1), lambda_e = 0, row_scale = ||a||_1 * ||x||_inf = 2.
QpProblem flat_normal_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 0.5, -0.5, -0.5, 0.5;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView(); // upper triangle only
    qp.g = Vec(2);
    qp.g << -1.0, 1.0;
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1.0, 1.0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Zero(1);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

constexpr double kRowScale = 2.0;

QpSolution seed_off_row(double k, const QpOptions &opts) {
    // The row residual a'x - b = k * floor, displaced along the normal a / |a|^2.
    const double delta = k * opts.feasibility_tol * kRowScale;
    QpSolution seed;
    seed.status = QpStatus::kOptimal;
    seed.x = Vec(2);
    seed.x << 1.0 + 0.5 * delta, -1.0 + 0.5 * delta;
    seed.lambda_e = Vec::Zero(1);
    seed.lambda_i = Vec(0);
    seed.z = Vec::Zero(2);
    seed.bound_state = {BoundState::kFree, BoundState::kFree};
    seed.tr_active = {false, false};
    seed.ineq_active = {};
    return seed;
}

double eq_residual(const QpProblem &qp, const QpSolution &s) {
    return std::abs(qp.Ae.row(0).dot(s.x) - qp.be(0));
}

} // namespace

TEST(QpRowContract, FlatNormalGrayZoneSeedIsRepairedToOneFloorUnit) {
    const QpProblem qp = flat_normal_qp();
    for (const auto alg : kBothModes) {
        for (const double k : {1.5, 3.0, 6.9, 9.5}) {
            QpOptions opts;
            opts.working_set_algebra = alg;
            const QpSolution seed = seed_off_row(k, opts);
            ASSERT_NEAR(eq_residual(qp, seed), k * opts.feasibility_tol * kRowScale,
                        1e-3 * opts.feasibility_tol);
            const QpSolution s =
                QpEngine(opts, 0, hven::linear::DensePolicy::kNever).solve(qp, seed);
            SCOPED_TRACE("k=" + std::to_string(k) +
                         " alg=" + std::to_string(static_cast<int>(alg)));
            ASSERT_EQ(static_cast<int>(s.status), static_cast<int>(QpStatus::kOptimal));
            EXPECT_LE(eq_residual(qp, s), opts.feasibility_tol * kRowScale);
            // The walk steps off the gray-zone seed (a second pass) instead of certifying it.
            EXPECT_GE(s.counters.minor_iters, 2);
            EXPECT_EQ(s.counters.optimal_row_gray_exits, 0);
        }
    }
}
