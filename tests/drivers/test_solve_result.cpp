// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The result core: the shared declared diagnostics, the snapshot export, the
// interior-point phase sequence and the shared budget.
//
// THE DIAGNOSTICS PINS BELOW ARE INDEPENDENT COMPUTATIONS. Nothing here asks an
// engine what it measured; the model quantities are written out by hand at a
// point whose KKT multipliers are known, and the expected answers follow from
// the definitions in drivers/solve_result.h rather than from any engine's own
// convergence test. That is the whole point of the pin: if the shared
// definition ever drifts toward one engine's internal measure, these fail.

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include <hven/drivers/solve_result.h>

using hven::Index;
using hven::SpMatRM;
using hven::Vec;
using hven::solvers::compute_declared_diagnostics;
using hven::solvers::DeclaredDiagnostics;

namespace {

/// An empty m x n sparse block, for a problem class the fixture does not use.
SpMatRM empty_block(Index rows, Index cols) { return SpMatRM(rows, cols); }

/// A dense row vector as a one-row sparse Jacobian block.
SpMatRM row_block(const std::vector<double> &row) {
    const Index n = static_cast<Index>(row.size());
    SpMatRM m(1, n);
    std::vector<Eigen::Triplet<double>> t;
    for (Index j = 0; j < n; ++j) {
        t.emplace_back(0, j, row[static_cast<std::size_t>(j)]);
    }
    m.setFromTriplets(t.begin(), t.end());
    m.makeCompressed();
    return m;
}

Vec vec_of(const std::vector<double> &v) {
    Vec out(static_cast<Index>(v.size()));
    for (std::size_t i = 0; i < v.size(); ++i) {
        out(static_cast<Index>(i)) = v[i];
    }
    return out;
}

constexpr double kInf = std::numeric_limits<double>::infinity();

} // namespace

// ===========================================================================
// (1) The definition closes on a genuine KKT point.
// ===========================================================================

// HS071 -- min x1*x4*(x1+x2+x3) + x3, subject to
//   x1*x2*x3*x4 >= 25, x1^2+x2^2+x3^2+x4^2 = 40, 1 <= xi <= 5.
//
// In hven's declared convention the inequality is written ci <= 0, so
//   ce(x) = x1^2+x2^2+x3^2+x4^2 - 40,   ci(x) = 25 - x1*x2*x3*x4,
// and the Lagrangian is L = f + lambda_e*ce + lambda_i*ci - z.
//
// THE POINT AND ITS PRICES BELOW WERE COMPUTED OUTSIDE THIS TREE, by Newton's
// method on the five KKT equations (stationarity in x2, x3, x4; the equality
// row; the active inequality row) with x1 pinned at its lower bound -- not by
// running either hven engine on the problem. x1 sits ON its lower bound, which
// is what makes z(0) non-zero and gives the canonical bound product something
// to price.
TEST(DeclaredDiagnostics, ClosesOnAKktPointOfHs071) {
    const double x1 = 1.0;
    const double x2 = 4.7429996372644174;
    const double x3 = 3.8211499841848742;
    const double x4 = 1.3794082931726723;
    const double lambda_e = 0.16146856677050586;
    const double lambda_i = 0.55229366012072689;

    const Vec x = vec_of({x1, x2, x3, x4});
    // grad f, by hand: d/dx1 = x4*(2*x1+x2+x3), d/dx2 = x1*x4,
    //                  d/dx3 = x1*x4 + 1,       d/dx4 = x1*(x1+x2+x3).
    const Vec grad =
        vec_of({x4 * (2.0 * x1 + x2 + x3), x1 * x4, x1 * x4 + 1.0, x1 * (x1 + x2 + x3)});
    // Je = grad(sum xi^2) = 2x; Ji = grad(25 - prod xi) = -(the cofactors).
    const SpMatRM Je = row_block({2.0 * x1, 2.0 * x2, 2.0 * x3, 2.0 * x4});
    const SpMatRM Ji =
        row_block({-(x2 * x3 * x4), -(x1 * x3 * x4), -(x1 * x2 * x4), -(x1 * x2 * x3)});
    const Vec ce = vec_of({x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4 - 40.0});
    const Vec ci = vec_of({25.0 - x1 * x2 * x3 * x4});

    // z is the bound price: zero on the three interior coordinates, and on the
    // coordinate at its lower bound it is exactly what stationarity leaves --
    // grad L there. Written out rather than read back from the function under
    // test.
    const double grad_lag_1 = grad(0) + lambda_e * 2.0 * x1 + lambda_i * (-(x2 * x3 * x4));
    const Vec z = vec_of({grad_lag_1, 0.0, 0.0, 0.0});
    EXPECT_NEAR(grad_lag_1, 1.0878712286669412, 1e-12);

    const DeclaredDiagnostics d = compute_declared_diagnostics(
        x, vec_of({lambda_e}), vec_of({lambda_i}), z, grad, Je, Ji, ce, ci,
        vec_of({1.0, 1.0, 1.0, 1.0}), vec_of({5.0, 5.0, 5.0, 5.0}), {});

    EXPECT_LT(d.stationarity, 1e-9);
    EXPECT_LT(d.feasibility_e, 1e-9);
    // The inequality is ACTIVE here, so its residual is 0 rather than negative;
    // the positive part is 0 either way and no bound is violated.
    EXPECT_LT(d.feasibility_i, 1e-9);
    // Both complementarity families vanish at this point for DIFFERENT reasons:
    // lambda_i * ci because ci is 0 at an active row, and the lower-bound
    // product because x1 - l1 is 0 at a bound the price is paid at. A
    // definition that dropped either family would still read 0 here, which is
    // why the two tests below pin the families themselves.
    EXPECT_LT(d.complementarity, 1e-9);
}

// ===========================================================================
// (2) The canonical two-sided bound split.
// ===========================================================================

// A signed z = zL - zU cannot recover two separate prices at a two-sided bound
// where both are positive. The shared diagnostic therefore prices the LOWER
// side with max(z,0) and the UPPER side with max(-z,0), and this pins that
// choice against the alternative it is not.
TEST(DeclaredDiagnostics, TwoSidedBoundUsesCanonicalSplit) {
    // One variable, strictly inside [0, 1], with a signed price that came from
    // zL = 2 and zU = 3 -- both strictly positive, which is the case the split
    // exists for.
    const Vec x = vec_of({0.5});
    const Vec z = vec_of({2.0 - 3.0});
    const Vec grad = vec_of({z(0)}); // stationary at that price: grad - z = 0
    const Vec empty(0);

    const DeclaredDiagnostics d =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 1), empty_block(0, 1),
                                     empty, empty, vec_of({0.0}), vec_of({1.0}), {});

    EXPECT_NEAR(d.stationarity, 0.0, 1e-15);
    // z is negative, so the LOWER product max(z,0)*(x-l) is 0 and the UPPER
    // product max(-z,0)*(u-x) = 1 * 0.5 carries the whole measure.
    EXPECT_NEAR(d.complementarity, 0.5, 1e-15);
    // What it is NOT: the true two-price measure max(zL*(x-l), zU*(u-x)) would
    // read max(2*0.5, 3*0.5) = 1.5 here. The canonical split cannot see zL and
    // zU separately and does not pretend to -- an engine that holds both keeps
    // its own measure on its own result.
    EXPECT_NE(d.complementarity, 1.5);
}

// ===========================================================================
// (3) An excluded coordinate is not measured.
// ===========================================================================

// Under the interior-point engine's MakeParameter treatment an eliminated
// variable has no row in the reduced problem at all: the reduced gradient
// reports 0 there, and a 0 that means "no row" must not enter an inf-norm
// beside 0s that mean "stationary". The exclusion list is how the engine says
// so.
TEST(DeclaredDiagnostics, ExcludedCoordinateIsNotMeasured) {
    // Coordinate 0 carries a large residual; coordinate 1 a tiny one. No
    // constraints, no finite bounds, so stationarity is the whole measurement.
    const Vec x = vec_of({3.0, 7.0});
    const Vec grad = vec_of({100.0, 1e-9});
    const Vec z = vec_of({0.0, 0.0});
    const Vec empty(0);
    const Vec lower = vec_of({-kInf, -kInf});
    const Vec upper = vec_of({kInf, kInf});

    const DeclaredDiagnostics measured =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {});
    EXPECT_NEAR(measured.stationarity, 100.0, 1e-15);

    const DeclaredDiagnostics excluded =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {0});
    EXPECT_NEAR(excluded.stationarity, 1e-9, 1e-18);

    // An infinite bound prices nothing: 0 * inf is not a measurement, and the
    // complementarity here has no finite bound and no inequality row to read.
    EXPECT_NEAR(excluded.complementarity, 0.0, 1e-15);

    // Excluding EVERY coordinate leaves nothing measured, which is NaN and not
    // zero -- the same discipline the rest of the result core keeps.
    const DeclaredDiagnostics none =
        compute_declared_diagnostics(x, empty, empty, z, grad, empty_block(0, 2), empty_block(0, 2),
                                     empty, empty, lower, upper, {0, 1});
    EXPECT_TRUE(std::isnan(none.stationarity));
    // The other three were still measured: exclusion is about the stationarity
    // coordinates alone.
    EXPECT_FALSE(std::isnan(none.feasibility_e));
    EXPECT_FALSE(std::isnan(none.feasibility_i));
    EXPECT_FALSE(std::isnan(none.complementarity));
}
