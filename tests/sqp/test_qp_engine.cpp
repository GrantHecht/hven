#include <cmath>
#include <random>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/qp/qp_engine.h>

#include "support/dense_oracle.h"

using namespace hven::solvers;
using hven::Index;
using hven::Vec;

namespace {

// --- Battery fixtures ---------------------------------------------------
//
// Repeated verbatim (modulo local naming) from test_dense_oracle.cpp /
// test_eqp_solve.cpp so this file stays self-contained; it deliberately does
// not include another test translation unit.

QpProblem simple_box_qp() {
    // min 1/2(x0^2 + x1^2) - x0 - 2 x1  s.t. x0 + x1 <= 1, 0 <= x <= 10.
    // Optimum (0, 1) is a DEGENERATE vertex: the inequality is active with
    // lambda = 1 and the bound x0 >= 0 is active with a zero multiplier.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Identity(2, 2);
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1, -2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);
    return qp;
}

QpProblem equality_only_qp() {
    // min 1/2||x||^2 s.t. x0 + x1 = 2  ->  x = (1,1), lambda_e = -1 (sign:
    // grad(f) + Ae^T lambda = 0)
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 2.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem all_bounds_active_qp() {
    // min 1/2||x - (5,5)||^2 s.t. 0 <= x <= 1  ->  corner (1,1), both bounds
    // active. No general constraints at all, so the final working set leaves
    // the reduced KKT system empty (n_free == me == n_working == 0).
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -5.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

QpProblem degenerate_qp() {
    // simple_box_qp with the inequality row DUPLICATED: at the optimum the
    // two identical rows are both active, so the active set is linearly
    // dependent and the multiplier is only determined up to how it splits
    // between the two rows.
    QpProblem qp = simple_box_qp();
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(2, 1.0);
    return qp;
}

QpProblem start_infeasible_qp() {
    // min 1/2(x0^2 + x1^2) - 0.5 x0
    //   s.t. -x0 - x1 <= -2   (i.e. x0 + x1 >= 2)
    //         x0 - x1 <= 0.3
    //         0 <= x <= 5
    // clamp(0, l, u) = (0,0) VIOLATES row 0 by 2, so the engine must run its
    // shifted-constraint homotopy. Optimum: both rows active at
    // x = (1.15, 0.85) with lambda = (0.75, 0.10).
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.5, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1, -1, 1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -2.0, 0.3;
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem crossed_bounds_qp() {
    // n=2, lower=(2,-5), upper=(1,5): x0's box [lower(0), upper(0)] = [2, 1]
    // is empty (lower > upper) while x1's box [-5,5] is perfectly ordinary on
    // its own -- a single crossed bound must still make the whole problem
    // kInfeasible. H = I, g = 0 (unconstrained minimum is x = 0, which is
    // squarely inside x1's box and would otherwise tempt a naive clamp into
    // reporting something plausible-looking).
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 2.0, -5.0;
    qp.upper = Vec(2);
    qp.upper << 1.0, 5.0;
    return qp;
}

QpProblem kfixed_probe_qp() {
    // n=2, lower(0)==upper(0)==1 (x0 is kFixed), x1 free. H = I, g = (-3, 0).
    // x1's unconstrained minimum (0) is also its bound-free optimum, so the
    // engine converges on the very first EQP solve with x0 pinned at 1.
    //
    // Hand-computed expectation for z(0): with me == mi == 0, price()'s
    // stationarity residual is r = H*x + g (no Ae^T/Ai^T terms), so
    //   z(0) = r(0) = H(0,:).x + g(0) = (1*1 + 0*0) + (-3) = 1 - 3 = -2.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -3.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 1.0, -1e20;
    qp.upper = Vec(2);
    qp.upper << 1.0, 1e20;
    return qp;
}

QpProblem badly_scaled_qp() {
    // min 1/2||x||^2  s.t.  0.01 x0 + 0.01 x1 <= -1  (i.e. x0 + x1 <= -100),
    // unbounded box. Optimum x = (-50,-50) with lambda = 5000.
    //
    // Regression guard for the tolerance scaling in refresh_shifts(). The row
    // is scaled down by 100, so its multiplier is scaled UP by 100 and the
    // regularized KKT solve (which satisfies Ai x - dual_mu*lambda = bi) leaves
    // a residual of order dual_mu*|lambda| ~ 1e-5, shrunk by refinement to
    // ~1e-8 but never to the 1e-9 absolute feas_tol. Judging the shift against
    // a bare feas_tol therefore mistook this CONVERGED solve for a live
    // homotopy shift and returned kInfeasible on a feasible problem.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 0.01, 0.01;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// --- Infeasible / degenerate-status fixtures ----------------------------

QpProblem infeasible_bounds_qp() {
    // min 1/2||x||^2  s.t.  x0 >= 2  AND  x0 <= 1,  -5 <= x <= 5.
    // The two inequality rows contradict each other, so no x is feasible.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1, 0, // -x0 <= -2   (x0 >= 2)
        1, 0;     //  x0 <=  1
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -2.0, 1.0;
    qp.lower = Vec::Constant(2, -5.0);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem infeasible_with_equality_qp() {
    // Same contradiction as infeasible_bounds_qp, but with an equality row
    // (x1 = 0) present so the inconsistency is diagnosed with the equality
    // block occupying part of the KKT system.
    QpProblem qp = infeasible_bounds_qp();
    Eigen::MatrixXd Aed(1, 2);
    Aed << 0, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.0);
    return qp;
}

QpProblem inconsistent_equalities_qp() {
    // min 1/2||x||^2 s.t. x0 + x1 = 1 AND x0 + x1 = 2. No inequalities at
    // all, so infeasibility is visible ONLY in the equality residual.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(2, 2);
    Aed << 1, 1, 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec(2);
    qp.be << 1.0, 2.0;
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -5.0);
    qp.upper = Vec::Constant(2, 5.0);
    return qp;
}

QpProblem equality_outside_box_qp() {
    // min 1/2||x||^2 s.t. x0 = 10, 0 <= x <= 1. The equality is individually
    // consistent but unreachable inside the box.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 10.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

// --- Large-but-legitimate solutions (runaway-guard probes) ---------------

QpProblem large_solution_free_qp() {
    // min 1/2||x - (1e5, 0)||^2, -1e7 <= x <= 1e7. x* = (1e5, 0): large, but
    // boxed by FINITE bounds, so it cannot be a runaway.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1e5, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e7);
    qp.upper = Vec::Constant(2, 1e7);
    return qp;
}

QpProblem large_bound_pinned_qp() {
    // min 1/2||x - (1e6, 0)||^2, 0 <= x0 <= 1e5. x0 pins AT its finite upper
    // bound of 1e5 -- a variable sitting on a bound has demonstrably not run
    // away.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1e6, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << 0.0, -1e5;
    qp.upper = Vec::Constant(2, 1e5);
    return qp;
}

QpProblem large_equality_qp() {
    // min 1/2||x||^2 s.t. x0 = 1e5, unbounded box. x* = (1e5, 0) with a ZERO
    // equality residual: genuinely optimal, merely large.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << 1, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1e5);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem large_solution_in_box_qp() {
    // min 1/2||x - (2e4, 0)||^2, 0 <= x <= 3e4. x* = (2e4, 0), interior.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -2e4, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 3e4);
    return qp;
}

// --- Ill-scaled but FEASIBLE ladder --------------------------------------

// min 1/2||x||^2 s.t. a*(x0 + x1) <= -1, unbounded box. Always feasible;
// exact solution x = (-1/(2a), -1/(2a)) with lambda = 1/(2a^2). Shrinking `a`
// scales the row down and the multiplier up as 1/a^2, walking the solve into
// the regularization's noise floor.
QpProblem ill_scaled_ineq_qp(double a) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << a, a;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem ill_scaled_eq_qp(double a) {
    QpProblem qp = ill_scaled_ineq_qp(a);
    Eigen::MatrixXd Aed(1, 2);
    Aed << a, a;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, -1.0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    return qp;
}

// min 1/2||x||^2 s.t. a(x0+x1) = 1 AND a(x0+x1) <= 1 - gap.
// Infeasible by `gap` (in row units) for any gap > 0, but the multiplier is
// inflated by the OBJECTIVE rather than by the contradiction: shrinking `a`
// drives |lambda| ~ 1/(2a^2) while the gap stays fixed.
QpProblem objective_inflated_lambda_qp(double a, double gap) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    Eigen::MatrixXd Aed(1, 2);
    Aed << a, a;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << a, a;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0 - gap);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// min -x0 + 1/2 x1^2 s.t. x1 >= 1, x0 unbounded below in the objective.
// x0 runs away to the regularization artifact while the working set holds a
// genuinely ACTIVE row carrying lambda = 1 -- so the kNumericalError path has
// a real multiplier to clear, unlike psd_singular_unbounded_qp.
QpProblem runaway_with_active_row_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 0, 0, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 0, -1; // -x1 <= -1  (x1 >= 1)
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem psd_singular_unbounded_qp() {
    // min -x0 + 1/2 x1^2 with NO bound on x0: H has an empty row/column for
    // x0, so the problem is unbounded below. Only the primal_delta
    // regularization makes it solvable, and it "solves" to x0 = 1/delta --
    // a pure artifact, not an optimum.
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 0, 0, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

QpProblem drop_before_infeasible_qp() {
    // min 1/2||x - (3,-3)||^2  s.t.  x0 + x1 >= 2,  0 <= x0 <= 1, 0 <= x1 <= 5.
    // Feasible (e.g. (1,1)); the optimum is exactly (1,1).
    //
    // The point of this fixture is the PATH, not the answer. The start point
    // (0,0) violates the general row (shift = 2). The objective pulls x1
    // negative, so the ratio test pins x1 at its LOWER bound on the very
    // first step -- and with x1 pinned at 0 the row needs x0 >= 2, which x0's
    // upper bound of 1 forbids. The homotopy shift therefore CANNOT close
    // until the drop rule releases x1. If infeasibility is declared before
    // the drop rule is consulted, this feasible problem reports kInfeasible.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -3.0, 3.0; // g = -c for c = (3,-3)
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << -1, -1; // -x0 - x1 <= -2
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -2.0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec(2);
    qp.upper << 1.0, 5.0;
    return qp;
}

QpProblem random_strictly_convex(int n, int mi, unsigned seed) {
    // H = M^T M + I is symmetric positive definite by construction, so the QP
    // is strictly convex and its optimum is unique. bi is kept strictly
    // positive so x = 0 is strictly feasible (the problem is never infeasible
    // and the cold start is never shifted).
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    Eigen::MatrixXd M(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M(i, j) = unit(rng);
        }
    }
    const Eigen::MatrixXd Hd = M.transpose() * M + Eigen::MatrixXd::Identity(n, n);

    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (int i = 0; i < n; ++i) {
        qp.g(i) = 2.0 * unit(rng);
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);

    Eigen::MatrixXd Aid(mi, n);
    for (int r = 0; r < mi; ++r) {
        for (int j = 0; j < n; ++j) {
            Aid(r, j) = unit(rng);
        }
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(mi);
    for (int r = 0; r < mi; ++r) {
        qp.bi(r) = 0.25 + std::abs(unit(rng));
    }
    qp.lower = Vec::Constant(n, -1.5);
    qp.upper = Vec::Constant(n, 1.5);
    return qp;
}

QpProblem random_strictly_convex_eq(int n, int me, int mi, unsigned seed) {
    // random_strictly_convex's sibling WITH equality rows -- the standing debt
    // item from the Phase-1 randomized battery, which covered inequalities and
    // bounds only.
    //
    // H = M^T M + I is symmetric positive definite exactly as there, so the QP
    // is strictly convex and its optimum unique. What is new is that the
    // problem must be FEASIBLE BY CONSTRUCTION: an equality row with random
    // coefficients and a random rhs is feasible only by luck once the box is
    // imposed, and an infeasible draw would make solve_dense_oracle throw
    // rather than disagree -- a test that fails for a reason unrelated to the
    // engine.
    //
    // So a witness x_feas is drawn INSIDE the box (|x_feas| <= 0.75 against
    // bounds of +/-1.5, i.e. strictly interior), the equality rhs is set to
    // Ae * x_feas, and each inequality rhs is Ai * x_feas plus a strictly
    // positive slack. x_feas is then a strictly-interior Slater point of the
    // whole system, which also keeps the LICQ/degeneracy pathologies out of
    // the way: what this battery is for is equality-row COVERAGE, not
    // degeneracy (which has its own fixtures).
    //
    // Note the cold start x = clamp(0, l, u) = 0 does NOT satisfy the equality
    // in general, so the homotopy shift on the equality row runs on essentially
    // every seed -- which is the path that was previously untested.
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    Eigen::MatrixXd M(n, n);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            M(i, j) = unit(rng);
        }
    }
    const Eigen::MatrixXd Hd = M.transpose() * M + Eigen::MatrixXd::Identity(n, n);

    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (int i = 0; i < n; ++i) {
        qp.g(i) = 2.0 * unit(rng);
    }

    Vec x_feas(n);
    for (int i = 0; i < n; ++i) {
        x_feas(i) = 0.75 * unit(rng);
    }

    Eigen::MatrixXd Aed(me, n);
    for (int r = 0; r < me; ++r) {
        for (int j = 0; j < n; ++j) {
            Aed(r, j) = unit(rng);
        }
    }
    qp.Ae = Aed.sparseView();
    qp.be = Aed * x_feas;

    Eigen::MatrixXd Aid(mi, n);
    for (int r = 0; r < mi; ++r) {
        for (int j = 0; j < n; ++j) {
            Aid(r, j) = unit(rng);
        }
    }
    qp.Ai = Aid.sparseView();
    qp.bi = Aid * x_feas;
    for (int r = 0; r < mi; ++r) {
        qp.bi(r) += 0.25 + std::abs(unit(rng));
    }

    qp.lower = Vec::Constant(n, -1.5);
    qp.upper = Vec::Constant(n, 1.5);
    return qp;
}

struct BatteryCase {
    std::string name;
    QpProblem qp;
    // false for degenerate_qp(): with linearly dependent active rows the
    // per-row multiplier split is not unique, so that case checks the SUM of
    // the duplicated rows' multipliers (and its own active-set expectation)
    // in a dedicated test instead.
    bool unique_multipliers = true;
};

std::vector<BatteryCase> battery() {
    std::vector<BatteryCase> cases;
    cases.push_back({"simple_box_qp", simple_box_qp(), true});
    cases.push_back({"equality_only_qp", equality_only_qp(), true});
    cases.push_back({"all_bounds_active_qp", all_bounds_active_qp(), true});
    cases.push_back({"degenerate_qp", degenerate_qp(), false});
    cases.push_back({"start_infeasible_qp", start_infeasible_qp(), true});
    cases.push_back({"random_strictly_convex(4,2,7)", random_strictly_convex(4, 2, 7), true});
    cases.push_back({"random_strictly_convex(5,3,11)", random_strictly_convex(5, 3, 11), true});
    return cases;
}

// Primal feasibility residual of `x` against the FULL problem.
double primal_violation(const QpProblem &qp, const Vec &x) {
    double worst = 0.0;
    if (qp.me() > 0) {
        worst = std::max(worst, (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0) {
        const Vec r = qp.Ai * x - qp.bi;
        worst = std::max(worst, r.maxCoeff());
    }
    for (Index i = 0; i < qp.n(); ++i) {
        worst = std::max(worst, qp.lower(i) - x(i));
        worst = std::max(worst, x(i) - qp.upper(i));
    }
    return worst;
}

} // namespace

TEST(QpEngine, OracleBattery) {
    for (const auto &c : battery()) {
        SCOPED_TRACE(c.name);
        const auto oracle = solve_dense_oracle(c.qp);
        const auto sol = QpEngine{QpOptions{}}.solve(c.qp);

        ASSERT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_LT((sol.x - oracle.x).norm(), 1e-8);
        EXPECT_LT(primal_violation(c.qp, sol.x), 1e-9);
        if (c.unique_multipliers) {
            EXPECT_EQ(sol.ineq_active, oracle.ineq_active);
            EXPECT_EQ(sol.bound_state, oracle.bound_state);
            EXPECT_LT((sol.lambda_e - oracle.lambda_e).norm(), 1e-6);
            EXPECT_LT((sol.lambda_i - oracle.lambda_i).norm(), 1e-6);
            EXPECT_LT((sol.z - oracle.z).norm(), 1e-6);
        }
        EXPECT_GT(sol.counters.minor_iters, 0);
    }
}

TEST(QpEngine, FixedVariableReachesOptimalWithPricedMultiplier) {
    // F5: kFixed is untested end to end elsewhere in this file (the dense
    // oracle has no kFixed label to compare against, since it does not model
    // lower(i) == upper(i) as a distinct state), so this test exercises the
    // engine alone: it must reach kOptimal, must actually hold x0 at its
    // pinned value, must report bound_state[0] == kFixed (not kAtLower /
    // kAtUpper), and must price z(0) per the documented stationarity formula
    // -- see kfixed_probe_qp()'s comment for the by-hand value.
    const QpProblem qp = kfixed_probe_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-9);
    EXPECT_NEAR(sol.x(1), 0.0, 1e-9);
    EXPECT_EQ(sol.bound_state[0], BoundState::kFixed);
    EXPECT_NEAR(sol.z(0), -2.0, 1e-9);
}

TEST(QpEngine, DegenerateDuplicateRowsAgreeOnMultiplierSum) {
    // The two identical rows of degenerate_qp() cannot both carry a uniquely
    // determined multiplier, so agreement is asserted on the SUM (which IS
    // unique: it is the multiplier of the single distinct constraint).
    const QpProblem qp = degenerate_qp();
    const auto oracle = solve_dense_oracle(qp);
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_LT((sol.x - oracle.x).norm(), 1e-8);
    EXPECT_NEAR(sol.lambda_i.sum(), oracle.lambda_i.sum(), 1e-6);
    EXPECT_LT((sol.z - oracle.z).norm(), 1e-6);
    // Whichever way the split lands, the duplicated constraint must be
    // recognized as active by at least one of its two rows.
    EXPECT_TRUE(sol.ineq_active[0] || sol.ineq_active[1]);
}

TEST(QpEngine, StartInfeasibleDrivesShiftsToZero) {
    const QpProblem qp = start_infeasible_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_LT(primal_violation(qp, sol.x), 1e-9);
    EXPECT_NEAR(sol.x(0), 1.15, 1e-8);
    EXPECT_NEAR(sol.x(1), 0.85, 1e-8);
    EXPECT_NEAR(sol.lambda_i(0), 0.75, 1e-7);
    EXPECT_NEAR(sol.lambda_i(1), 0.10, 1e-7);
}

TEST(QpEngine, BadlyScaledRowIsNotMistakenForInfeasibility) {
    // The scale-aware shift clamp (see badly_scaled_qp) is what keeps this
    // feasible problem from being reported kInfeasible.
    const QpProblem qp = badly_scaled_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), -50.0, 1e-6);
    EXPECT_NEAR(sol.x(1), -50.0, 1e-6);
    EXPECT_NEAR(sol.lambda_i(0), 5000.0, 1e-3);
    EXPECT_TRUE(sol.ineq_active[0]);
    // Feasibility is only certifiable to the regularization footprint
    // dual_mu*|lambda| ~ 5e-5 here, not to the bare 1e-9 feas_tol.
    EXPECT_LT(primal_violation(qp, sol.x), 1e-4);
}

// --- Infeasibility / numerical-status detection -------------------------

TEST(QpEngine, CrossedBoundsAreInfeasibleNotCertifiedOptimal) {
    // F1 regression: an empty box (lower(i) > upper(i)) must be reported
    // kInfeasible immediately, not run through the loop where a naive
    // clamp(0, l, u) can land on a bound and get certified kOptimal. Before
    // the fix, this fixture reported kOptimal at a clamped x -- silently
    // certifying a point that violates the very bound that made the box
    // empty in the first place.
    const QpProblem qp = crossed_bounds_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kInfeasible);
    ASSERT_TRUE(sol.x.allFinite());
    ASSERT_TRUE(sol.lambda_e.allFinite());
    ASSERT_TRUE(sol.lambda_i.allFinite());
    ASSERT_TRUE(sol.z.allFinite());
    // No 1/dual_mu artifacts may leak out on the infeasible path (same
    // contract as every other kInfeasible exit).
    EXPECT_TRUE(sol.lambda_i.isZero());
    EXPECT_TRUE(sol.z.isZero());
}

TEST(QpEngine, ContradictoryInequalityRowsAreInfeasible) {
    // Regression guard for a self-fulfilling feasibility tolerance. On an
    // INCONSISTENT working row the regularized solve satisfies
    // Ai x - dual_mu*lambda = bi by construction, so the residual IS
    // dual_mu*|lambda| exactly -- and lambda grows without bound as the
    // contradiction sharpens. An uncapped dual_mu*|lambda| allowance
    // therefore always covers the violation and kInfeasible is unreachable.
    const QpProblem qp = infeasible_bounds_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kInfeasible);
    // The true violation here is order 0.5, nowhere near any tolerance.
    EXPECT_GT(primal_violation(qp, sol.x), 1e-3);

    // No 1/dual_mu artifacts may leak out on the infeasible path: the
    // multipliers of an inconsistent working set are meaningless.
    ASSERT_TRUE(sol.lambda_i.allFinite());
    ASSERT_TRUE(sol.z.allFinite());
    ASSERT_TRUE(sol.x.allFinite());
    EXPECT_LT(sol.lambda_i.lpNorm<Eigen::Infinity>(), 1e6);
    EXPECT_LT(sol.z.lpNorm<Eigen::Infinity>(), 1e6);
}

TEST(QpEngine, ContradictoryInequalitiesWithEqualitiesPresentAreInfeasible) {
    const QpProblem qp = infeasible_with_equality_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kInfeasible);
    EXPECT_GT(primal_violation(qp, sol.x), 1e-3);
    ASSERT_TRUE(sol.lambda_e.allFinite());
    EXPECT_LT(sol.lambda_e.lpNorm<Eigen::Infinity>(), 1e6);
}

TEST(QpEngine, InconsistentEqualitiesAreInfeasible) {
    // mi == 0, so the shift machinery sees nothing at all; only an explicit
    // equality-residual check at the optimality gate can catch this.
    const QpProblem qp = inconsistent_equalities_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kInfeasible);
    EXPECT_GT((qp.Ae * sol.x - qp.be).lpNorm<Eigen::Infinity>(), 1e-3);
}

TEST(QpEngine, EqualityOutsideTheBoxIsInfeasible) {
    const QpProblem qp = equality_outside_box_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kInfeasible);
    EXPECT_GT((qp.Ae * sol.x - qp.be).lpNorm<Eigen::Infinity>(), 1e-3);
}

TEST(QpEngine, UnboundedProblemIsReportedAsNumericalErrorNotOptimal) {
    // x0 runs to ~1/primal_delta: the regularization talking, not an optimum;
    // reporting kOptimal there would be a lie.
    const QpProblem qp = psd_singular_unbounded_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kNumericalError);
    EXPECT_GT(std::abs(sol.x(0)), 1e7);
    // The runaway's multipliers are artifacts too and must not leak out.
    ASSERT_TRUE(sol.lambda_i.allFinite());
    ASSERT_TRUE(sol.z.allFinite());
    ASSERT_TRUE(sol.lambda_e.allFinite());
    EXPECT_LT(sol.z.lpNorm<Eigen::Infinity>(), 1e6);
}

TEST(QpEngine, RunawayWithActiveRowClearsItsMultipliers) {
    // Unlike psd_singular_unbounded_qp (mi == me == 0, every variable free --
    // where "the multipliers are cleared" is vacuously true), this fixture
    // reaches kNumericalError with a genuinely active row whose multiplier is
    // 1. Deleting the kNumericalError arm of the clearing makes this fail.
    const QpProblem qp = runaway_with_active_row_qp();
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kNumericalError);
    EXPECT_GT(std::abs(sol.x(0)), 1e7); // x0 ran away
    EXPECT_NEAR(sol.x(1), 1.0, 1e-8);   // the active row still holds
    ASSERT_EQ(sol.lambda_i.size(), 1);
    EXPECT_TRUE(sol.ineq_active[0]); // the row IS in the working set
    EXPECT_TRUE(sol.lambda_i.isZero());
    EXPECT_TRUE(sol.z.isZero());
}

TEST(QpEngine, EffectivelyInfiniteBoundsStillDetectTheRunaway) {
    // A hand-written "effectively infinite" bound of 1e18 is nowhere near
    // kEngineInfBound (1e20), but the artifact at 2e8 can never legitimately
    // reach it -- the solve would have pinned the variable there first. The
    // runaway test must therefore treat any bound beyond
    // unbounded_artifact_scale() as non-constraining.
    QpProblem qp = psd_singular_unbounded_qp();
    qp.lower = Vec::Constant(2, -1e18);
    qp.upper = Vec::Constant(2, 1e18);
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    EXPECT_EQ(sol.status, QpStatus::kNumericalError);
    EXPECT_GT(std::abs(sol.x(0)), 1e7);
}

TEST(QpEngine, ObjectiveInflatedMultiplierHidesASmallContradiction) {
    // KNOWN, OWNED LIMITATION -- this test pins CURRENT behavior, and the
    // expected verdict on the a = 1e-3 row is WRONG in the mathematical sense.
    //
    // Both problems below are genuinely INFEASIBLE (an equality demanding
    // a(x0+x1) = 1 alongside an inequality capping it at 1 - 3e-4). Detection
    // rests on the violation reaching kStructuralResidualFrac of the
    // regularization footprint dual_mu*|lambda|. Here |lambda| is inflated by
    // the OBJECTIVE (it grows as 1/(2a^2) as the row is scaled down) while the
    // contradiction's gap stays fixed, so shrinking `a` inflates the footprint
    // out from under a fixed violation until the contradiction hides beneath
    // it. This is the false-kOptimal direction of the kStructuralResidualFrac
    // trade-off, and it is the exact mirror of the false-kInfeasible direction
    // pinned by IllScaledFeasibleRowsAreNotDeclaredInfeasible.
    //
    // Per the project spec the SQP driver is the second detection layer and
    // restores this case; the engine does not pretend to catch it alone. See
    // the TRUSTWORTHY RANGE note in qp_engine.h.
    EXPECT_EQ(QpEngine{QpOptions{}}.solve(objective_inflated_lambda_qp(1e-2, 3e-4)).status,
              QpStatus::kInfeasible); // still caught at this scale
    EXPECT_EQ(QpEngine{QpOptions{}}.solve(objective_inflated_lambda_qp(3e-3, 3e-4)).status,
              QpStatus::kInfeasible); // still caught at this scale
    EXPECT_EQ(QpEngine{QpOptions{}}.solve(objective_inflated_lambda_qp(1e-3, 3e-4)).status,
              QpStatus::kOptimal); // KNOWN MISS: contradiction hidden by footprint
}

TEST(QpEngine, LargeButLegitimateSolutionsAreOptimalNotNumericalError) {
    // The runaway guard must be BOUND-RELATIVE. A large ||x|| is only
    // suspicious when nothing bounds the component it grew in; a variable
    // boxed by finite bounds cannot run away, and one pinned at a bound
    // demonstrably has not. Each of these solves to a large, legitimate x.
    struct Probe {
        const char *name;
        QpProblem qp;
        double expect_x0;
    };
    const std::vector<Probe> probes = {
        {"free x*=1e5 within +/-1e7", large_solution_free_qp(), 1e5},
        {"x0 pinned at finite upper 1e5", large_bound_pinned_qp(), 1e5},
        {"equality x0=1e5, zero residual", large_equality_qp(), 1e5},
        {"x*=2e4 within [0,3e4]", large_solution_in_box_qp(), 2e4},
    };
    for (const auto &p : probes) {
        SCOPED_TRACE(p.name);
        const auto sol = QpEngine{QpOptions{}}.solve(p.qp);
        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), p.expect_x0, 1e-3 * p.expect_x0);
        EXPECT_LT(primal_violation(p.qp, sol.x), 1e-6);
    }
}

TEST(QpEngine, BoundPinnedLargeSolutionKeepsItsBoundLabel) {
    const auto sol = QpEngine{QpOptions{}}.solve(large_bound_pinned_qp());
    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_EQ(sol.bound_state[0], BoundState::kAtUpper);
    EXPECT_NEAR(sol.x(0), 1e5, 1e-6);
}

TEST(QpEngine, IllScaledFeasibleRowsAreNotDeclaredInfeasible) {
    // Walking `a` down scales the row's multiplier up as 1/a^2, pushing the
    // regularized residual toward the noise floor. These are all FEASIBLE, so
    // none may report kInfeasible: a violation that sits far below the
    // regularization's own footprint is solver noise, not evidence of
    // infeasibility.
    //
    // KNOWN LIMIT: the ladder stops at 3e-4. By a = 1e-4 the multiplier
    // (5e7) drives the residual to ~22% of the noise floor and the returned
    // x is itself ~11% wrong, so the engine can no longer honestly call that
    // point optimal -- see the header contract's trustworthy-range note.
    for (double a : {1e-2, 3e-3, 1e-3, 3e-4}) {
        SCOPED_TRACE(a);
        const auto ineq = QpEngine{QpOptions{}}.solve(ill_scaled_ineq_qp(a));
        EXPECT_EQ(ineq.status, QpStatus::kOptimal);
        EXPECT_NEAR(ineq.x(0), -1.0 / (2.0 * a), 1e-2 * std::abs(1.0 / (2.0 * a)));

        const auto eq = QpEngine{QpOptions{}}.solve(ill_scaled_eq_qp(a));
        EXPECT_EQ(eq.status, QpStatus::kOptimal);
        EXPECT_NEAR(eq.x(0), -1.0 / (2.0 * a), 1e-2 * std::abs(1.0 / (2.0 * a)));
    }
}

TEST(QpEngine, DropRuleRunsBeforeInfeasibilityIsDeclared) {
    // See drop_before_infeasible_qp: a bound pinned by the ratio test blocks
    // the homotopy shift from closing, and only the drop rule can release it.
    //
    // This test is ordering-sensitive BY CONSTRUCTION. Declaring
    // kInfeasible before consulting drop_worst() (rather than only after it
    // reports nothing left to drop) flips this case from kOptimal to
    // kInfeasible -- verified by reverting the ordering locally.
    const QpProblem qp = drop_before_infeasible_qp();
    const auto oracle = solve_dense_oracle(qp);
    const auto sol = QpEngine{QpOptions{}}.solve(qp);

    ASSERT_EQ(sol.status, QpStatus::kOptimal);
    EXPECT_NEAR(sol.x(0), 1.0, 1e-8);
    EXPECT_NEAR(sol.x(1), 1.0, 1e-8);
    EXPECT_LT((sol.x - oracle.x).norm(), 1e-8);
    EXPECT_LT(primal_violation(qp, sol.x), 1e-9);
    // x1 must have been released back to free by the drop rule.
    EXPECT_EQ(sol.bound_state[1], BoundState::kFree);
}

TEST(QpEngine, RandomizedAgainstOracle) {
    for (unsigned seed = 0; seed < 50; ++seed) {
        const QpProblem qp = random_strictly_convex(5, 3, seed);
        const auto oracle = solve_dense_oracle(qp);
        const auto sol = QpEngine{QpOptions{}}.solve(qp);

        ASSERT_EQ(sol.status, QpStatus::kOptimal) << "seed " << seed;
        EXPECT_LT((sol.x - oracle.x).norm(), 1e-7) << "seed " << seed;
        EXPECT_LT(primal_violation(qp, sol.x), 1e-9) << "seed " << seed;
    }
}

// The equality-row half of the randomized battery -- the standing debt item.
// RandomizedAgainstOracle above covers inequalities and bounds; this covers
// me = 1 as well, in BOTH ws_algebra modes.
//
// Unlike the indefinite batteries in test_qp_engine_indefinite.cpp, cross-mode
// equality IS asserted here, and legitimately so: these problems are strictly
// convex, so the optimum is unique and both modes must find the same one. The
// full solution is compared -- x, the active set, the bound labels, and every
// multiplier -- not just the objective; with H positive definite and a strictly
// interior Slater point the active gradients are generically independent, so
// the multiplier split is unique and there is no reason to accept less.
//
// Measured: 50 seeds x 2 modes, all kOptimal, zero disagreements with
// solve_dense_oracle on any of those quantities.
TEST(QpEngine, RandomizedWithEqualityRowAgainstOracle) {
    for (unsigned seed = 0; seed < 50; ++seed) {
        SCOPED_TRACE("seed " + std::to_string(seed));
        const QpProblem qp = random_strictly_convex_eq(/*n=*/5, /*me=*/1, /*mi=*/3, seed);
        const auto oracle = solve_dense_oracle(qp);
        // Fixture precondition: the equality row is LIVE at the cold start
        // x = clamp(0, l, u) = 0, whose equality residual is exactly -be(0).
        // A generator that happened to produce be == 0 would leave the
        // equality satisfied from the first iterate and this battery would say
        // nothing about the equality homotopy it exists to cover.
        ASSERT_GT(std::abs(qp.be(0)), 1e-9);

        for (const auto algebra :
             {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
            QpOptions opts;
            opts.ws_algebra = algebra;
            const auto sol = QpEngine{opts}.solve(qp);
            SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border"
                                                                          : "refactorize");

            ASSERT_EQ(sol.status, QpStatus::kOptimal);
            EXPECT_LT((sol.x - oracle.x).norm(), 1e-7);
            EXPECT_LT(primal_violation(qp, sol.x), 1e-8);
            EXPECT_EQ(sol.ineq_active, oracle.ineq_active);
            EXPECT_EQ(sol.bound_state, oracle.bound_state);
            EXPECT_LT((sol.lambda_e - oracle.lambda_e).norm(), 1e-6);
            EXPECT_LT((sol.lambda_i - oracle.lambda_i).norm(), 1e-6);
            EXPECT_LT((sol.z - oracle.z).norm(), 1e-6);
        }
    }
}

TEST(QpEngine, WarmStartFromOptimalSolutionConvergesImmediately) {
    const QpProblem qp = random_strictly_convex(5, 3, 3);
    const auto cold = QpEngine{QpOptions{}}.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);

    const auto warm = QpEngine{QpOptions{}}.solve(qp, cold);
    ASSERT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_LT((warm.x - cold.x).norm(), 1e-9);
    EXPECT_EQ(warm.bound_state, cold.bound_state);
    EXPECT_EQ(warm.ineq_active, cold.ineq_active);
    EXPECT_LT(warm.counters.minor_iters, cold.counters.minor_iters);
}

TEST(QpEngine, MaxIterBudgetIsReported) {
    QpOptions opts;
    opts.max_iter = 1;
    const auto sol = QpEngine{opts}.solve(all_bounds_active_qp());
    EXPECT_EQ(sol.status, QpStatus::kMaxIter);
    EXPECT_EQ(sol.counters.minor_iters, 1);
}

// =====================================================================
// PHASE-7 TASK 6b (docket D0, option 3) -- THE EXPORT INVARIANT ON z:
// A FREE VARIABLE CARRIES NO BOUND PRICE.
// =====================================================================
//
// qp_engine.h's interface contract, clause 6b:
//     bound_state[i] == kFree  =>  z(i) == 0.0, on EVERY status.
//
// THE DEFECT THIS PINS, which the Phase-7 gate battery found at scale
// (docs/notes/2026-08-08-ssn-gate-battery.md; the failing cell exported
// z(1882) = +0.173 on a variable reported kFree with tr_active false):
// `price()` writes z only at pinned indices, but it runs at the TOP of a
// minor iteration and the working set is mutated afterwards. A converging
// exit always re-prices after the last mutation; a kMaxIter exit does not,
// so a bound the drop rule released on the final minor leaves its price
// behind. Latent under kWalk (nothing reads QpSolution::z there -- see
// sqp_driver.h), FATAL under kSsn (ssn_start_from_qp_seed copies the field
// into the next subproblem's seed, where the absent-bound test throws).
//
// THE FIXTURE IS THE MECHANISM, MINIMALLY. Both variables are seeded pinned
// at a lower bound they do not belong at; the first minor prices both at
// z = -0.5, finds the step negligible, and the Dantzig drop rule releases
// the WORSE one (exact tie -> lowest index -> variable 0). The cap of 1
// then ends the solve with variable 0 kFree and variable 1 still kAtLower.
//
// AND IT PINS BOTH HALVES, which is why it is written with two variables
// rather than one: the freed index must lose its price, and the still-pinned
// index must KEEP its price. The second half is what distinguishes this
// repair from docket D0's option 2 ("zero z on kMaxIter"), which would also
// pass the first half while discarding a price that is still meaningful.
TEST(QpEngineExportInvariant, AFreeVariableCarriesNoBoundPriceAtACappedExit) {
    // min 1/2||x||^2 - 0.5*(x0 + x1) over [0, 10]^2. Unconstrained optimum
    // (0.5, 0.5) is interior, so neither bound belongs in the working set.
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -0.5);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Zero(2);
    qp.upper = Vec::Constant(2, 10.0);

    QpSolution seed;
    seed.status = QpStatus::kOptimal;
    seed.x = Vec::Zero(2);
    seed.lambda_e = Vec(0);
    seed.lambda_i = Vec(0);
    seed.z = Vec::Zero(2);
    seed.bound_state = {BoundState::kAtLower, BoundState::kAtLower};
    seed.tr_active = {false, false};
    seed.ineq_active = {};

    QpOptions opts;
    opts.max_iter = 1;
    const auto sol = QpEngine{opts}.solve(qp, seed);

    ASSERT_EQ(sol.status, QpStatus::kMaxIter) << "the fixture must reach the capped exit";
    ASSERT_EQ(sol.counters.minor_iters, 1);
    // The mechanism actually happened: exactly one of the two seeded pins was
    // released by the drop rule on that single minor.
    ASSERT_EQ(sol.bound_state[0], BoundState::kFree) << "the drop rule must have freed variable 0";
    ASSERT_EQ(sol.bound_state[1], BoundState::kAtLower) << "variable 1 must still be pinned";

    // HALF ONE -- the invariant. Pre-repair this read -0.5.
    EXPECT_EQ(sol.z(0), 0.0) << "a kFree variable must carry no bound price";
    // HALF TWO -- and the repair is a CLEAR ON THE FREED SET, not a blanket
    // wipe: the still-pinned variable keeps the price that was computed for it.
    EXPECT_LT(sol.z(1), -0.4) << "a still-pinned bound must keep its price";
}

// The same invariant, asserted across the whole battery of shipped fixtures
// and every status they reach, so that a future exit path added to run()
// cannot reintroduce the defect silently. This is the assertion that turns
// clause 6b from a comment into a checked postcondition.
TEST(QpEngineExportInvariant, HoldsOnEveryBatteryFixtureAndEveryStatus) {
    struct Case {
        const char *name;
        QpProblem qp;
    };
    const std::vector<Case> cases = {
        {"simple_box", simple_box_qp()},
        {"equality_only", equality_only_qp()},
        {"all_bounds_active", all_bounds_active_qp()},
        {"degenerate", degenerate_qp()},
        {"start_infeasible", start_infeasible_qp()},
        {"kfixed_probe", kfixed_probe_qp()},
        {"badly_scaled", badly_scaled_qp()},
        {"infeasible_bounds", infeasible_bounds_qp()},
        {"infeasible_equality", infeasible_with_equality_qp()},
    };
    // Caps of 1 and 2 force a kMaxIter exit mid-solve; 0 is the shipped
    // SENTINEL, which derives a cap large enough that these fixtures all
    // solve out -- so the loop covers both the capped and the converged sides.
    for (const Index cap : {Index{1}, Index{2}, Index{0}}) {
        for (const Case &c : cases) {
            QpOptions opts;
            opts.max_iter = cap;
            const auto sol = QpEngine{opts}.solve(c.qp);
            for (Index i = 0; i < c.qp.n(); ++i) {
                if (sol.bound_state[static_cast<std::size_t>(i)] == BoundState::kFree) {
                    EXPECT_EQ(sol.z(i), 0.0) << c.name << " cap=" << cap << " index " << i
                                             << " status " << static_cast<int>(sol.status);
                }
            }
        }
    }
}

// =====================================================================
// PHASE-6 TASK 4 (M6) -- THE SIZE-DERIVED max_iter.
//
// QpOptions::max_iter's default became a SENTINEL meaning "derive the cap
// from this subproblem's size" (qp_types.h). The derivation is three small
// functions in qp_engine.h's detail namespace, tested here directly so the
// policy is pinned away from any solve, and so a change to the coefficient
// or the floor fails LOUDLY rather than through a distant counter pin.
TEST(QpEngineSizeDerivedCap, BaseCountsRealBoundsAndTheDerivationIsFloored) {
    // qp_cap_base = n + mi + #bounded, over the CALLER's bounds.
    QpProblem qp = all_bounds_active_qp(); // n = 2, mi = 0, both bounds finite
    EXPECT_EQ(detail::qp_cap_base(qp), 2 + 0 + 2);

    // An infinite side does not make a variable unbounded -- one finite side
    // is enough for the walk to be able to pin it.
    qp.lower(0) = -1e21;
    EXPECT_EQ(detail::qp_cap_base(qp), 2 + 0 + 2);
    qp.upper(0) = 1e21;
    EXPECT_EQ(detail::qp_cap_base(qp), 2 + 0 + 1) << "variable 0 is now free on both sides";

    // The FLOOR dominates every small problem, which is why no
    // Hock-Schittkowski-sized fixture in this suite changes under M6.
    EXPECT_EQ(detail::derived_qp_max_iter(3), detail::kQpMaxIterFloor);
    EXPECT_EQ(detail::derived_qp_max_iter(detail::kQpMaxIterFloor / detail::kQpMaxIterCoeff),
              detail::kQpMaxIterFloor);
    // And the multiple dominates once the problem is big enough. base = 8000
    // is F7 at N = 1000 (5N variables + N rows + 2N bounded controls).
    EXPECT_EQ(detail::derived_qp_max_iter(8000), detail::kQpMaxIterCoeff * 8000);
    EXPECT_GT(detail::kQpMaxIterCoeff * 8000, detail::kQpMaxIterFloor);
}

// THE PRECEDENCE RULE, which is the escape hatch the whole default change
// rests on: any POSITIVE max_iter wins outright, at any size, including one
// far below what the derivation would have granted. Every tiny-cap fixture in
// this suite (MaxIterBudgetIsReported above, the continuation batteries) is
// protected by exactly this.
TEST(QpEngineSizeDerivedCap, AnExplicitCapWinsOutrightAndTheSentinelDerives) {
    const QpProblem qp = all_bounds_active_qp();
    const Index base = detail::qp_cap_base(qp);

    EXPECT_EQ(detail::effective_qp_max_iter(qp, 1), 1);
    EXPECT_EQ(detail::effective_qp_max_iter(qp, 7), 7);
    EXPECT_EQ(detail::effective_qp_max_iter(qp, 999999), 999999);
    // The sentinel (0, the shipped default) and any non-positive value derive.
    EXPECT_EQ(detail::effective_qp_max_iter(qp, 0), detail::derived_qp_max_iter(base));
    EXPECT_EQ(detail::effective_qp_max_iter(qp, -1), detail::derived_qp_max_iter(base));

    // And the engine honours it end to end: a cap of 1 still reports kMaxIter
    // after exactly one minor, on a problem whose derived cap is 500.
    QpOptions opts;
    opts.max_iter = 1;
    const auto capped = QpEngine{opts}.solve(qp);
    EXPECT_EQ(capped.status, QpStatus::kMaxIter);
    EXPECT_EQ(capped.counters.minor_iters, 1);

    // The default sentinel solves the same problem to optimality.
    QpOptions defaults;
    ASSERT_LE(defaults.max_iter, 0) << "the shipped default must BE the sentinel";
    const auto solved = QpEngine{defaults}.solve(qp);
    EXPECT_EQ(solved.status, QpStatus::kOptimal);
}

// =====================================================================
// TIER 3: QpEngine::refine_on_face -- THE STABLE-FACE REFINEMENT ENTRY
// (Phase-7 Task 5, fix round 1)
// =====================================================================
//
// This is the entry `docs/superpowers/specs/2026-08-05-phase-7-design.md`
// specifies for a kernel that identifies an active set to its own tolerance and
// then wants the point that set EXACTLY determines. It is reached from the
// driver only under `qp_mode == QpMode::kSsn`; these arms drive it directly, on
// hand-built faces, so both polarities (accepted / refused) are covered without
// a driver and without depending on which faces the SSN kernel happens to
// produce today.
//
// EVERY ARM ASSERTS THE COST, because "refused" must not be confused with
// "free": the rank pre-screen and the empty-face short-circuit refuse BEFORE
// factorizing (0), while a gate refusal has already paid its one factorization.
namespace {

// The face a QpSolution names, built by hand: `rows` are the working
// inequalities, `bounds` the per-variable pin states, `x` the point the caller
// stopped at.
QpSolution face_of(const Vec &x, std::vector<BoundState> bounds, std::vector<bool> rows) {
    QpSolution s;
    s.status = QpStatus::kOptimal;
    s.x = x;
    s.lambda_e = Vec(0);
    s.lambda_i = Vec::Zero(static_cast<Index>(rows.size()));
    s.z = Vec::Zero(x.size());
    s.bound_state = std::move(bounds);
    s.ineq_active = std::move(rows);
    s.tr_active.assign(static_cast<std::size_t>(x.size()), false);
    return s;
}

} // namespace

// (a) THE ACCEPTED CASE, pinned against the ANALYTIC EQP answer rather than
// against an observation. On simple_box_qp the face {row 0 working, every
// variable free} has KKT x + g + Ai^T lambda = 0 with x0 + x1 = 1, i.e.
// (1 - lambda) + (2 - lambda) = 1, so lambda = 1 and x = (0, 1) exactly -- which
// is also this QP's true optimum. The caller is handed in AT A DIFFERENT POINT
// of the same face, so what the test measures is that the refinement SOLVED the
// face rather than passing the input through.
TEST(QpEngineFaceRefine, AnExactFaceSolveLandsOnTheAnalyticEqpAnswer) {
    const QpProblem qp = simple_box_qp();
    Vec off(2);
    off << 0.10, 0.85; // on the face's ROW, but not at its solution
    const QpSolution face =
        face_of(off, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{true});

    QpSolution out;
    QpEngine engine{QpOptions{}};
    ASSERT_TRUE(engine.refine_on_face(qp, face, SolveOverrides{}, out));

    EXPECT_NEAR(0.0, out.x(0), 1e-12);
    EXPECT_NEAR(1.0, out.x(1), 1e-12);
    EXPECT_NEAR(1.0, out.lambda_i(0), 1e-9) << "the face's own price, priced by the exact solve";
    EXPECT_EQ(1, out.counters.factorizations) << "exactly one, and it is charged";
    EXPECT_EQ(0, out.counters.minor_iters) << "no walk ran: this is not an active-set search";
    // The face is the CALLER's and is carried through, not re-derived.
    EXPECT_TRUE(out.ineq_active[0]);
    EXPECT_EQ(BoundState::kFree, out.bound_state[0]);
}

// (b) THE REFUSED CASE. The same QP with a face that claims x1 is pinned at its
// UPPER bound of 10 while row 0 (x0 + x1 <= 1) is working: the face solve is
// then forced to x0 = -9, which is outside the QP's own box. An EQP knows
// nothing about the constraints that are OFF its face, so this is exactly the
// failure mode the gate exists for -- and the caller must get its own answer
// back untouched, not the escaped one.
TEST(QpEngineFaceRefine, AFaceThatLeavesTheBoxIsRefusedAndTheCallerKeepsItsAnswer) {
    const QpProblem qp = simple_box_qp();
    Vec at(2);
    at << 0.0, 10.0;
    const QpSolution face =
        face_of(at, {BoundState::kFree, BoundState::kAtUpper}, std::vector<bool>{true});

    QpSolution out;
    QpEngine engine{QpOptions{}};
    EXPECT_FALSE(engine.refine_on_face(qp, face, SolveOverrides{}, out));

    EXPECT_EQ(face.x, out.x) << "REFUSED means UNCHANGED -- the caller's certificate stands";
    EXPECT_EQ(1, out.counters.factorizations)
        << "a gate refusal has already paid; only the pre-screens are free";
}

// (c) THE RANK PRE-SCREEN, which refuses BEFORE anything is factorized: a face
// with more equality rows (model equalities + working inequalities) than free
// variables cannot be a regular face, and handing its singular K to the backend
// would trade a usable answer for a thrown Pardiso error.
TEST(QpEngineFaceRefine, AnOverDeterminedFaceIsRefusedWithoutFactorizing) {
    const QpProblem qp = simple_box_qp();
    Vec at(2);
    at << 0.0, 0.0;
    const QpSolution face = face_of(at, {BoundState::kAtLower, BoundState::kAtLower},
                                    std::vector<bool>{true}); // 1 row, 0 free

    QpSolution out;
    QpEngine engine{QpOptions{}};
    EXPECT_FALSE(engine.refine_on_face(qp, face, SolveOverrides{}, out));
    EXPECT_EQ(0, out.counters.factorizations);
}

// (d) THE EMPTY FACE. Every variable pinned and no rows at all: the face
// already DETERMINES x, so there is nothing to refine and nothing to pay --
// the same empty-system case eliminated_candidate short-circuits.
TEST(QpEngineFaceRefine, AFullyPinnedFaceIsRefusedAndCostsNothing) {
    const QpProblem qp = all_bounds_active_qp();
    Vec at(2);
    at << 1.0, 1.0;
    const QpSolution face =
        face_of(at, {BoundState::kAtUpper, BoundState::kAtUpper}, std::vector<bool>{});

    QpSolution out;
    QpEngine engine{QpOptions{}};
    EXPECT_FALSE(engine.refine_on_face(qp, face, SolveOverrides{}, out));
    EXPECT_EQ(0, out.counters.factorizations);
}

// (e) A TRUST-REGION PIN IS PART OF THE FACE HERE, and that is the one place
// this transcription deliberately differs from the walk's seed ingest (which
// IGNORES tr_active, so a radius artefact from one major cannot be asserted as
// a genuine bound in the next). This function is re-solving THE SAME
// subproblem, whose box really does include the radius.
//
// ANALYTIC: at Delta = 0.5 the window about the clamped origin is [0, 0.5]^2.
// With x1 TR-pinned at 0.5 and row 0 (x0 + x1 <= 1) working, the face gives
// x0 = 0.5 and, from x0 + g0 + lambda = 0, lambda = 0.5. The TR pin must leave
// NO PRICE behind -- section 6's reporting exclusion, which run() applies to
// its own answers for the same reason.
TEST(QpEngineFaceRefine, ATrustRegionPinIsHonouredAsPartOfTheFaceAndPricesNothing) {
    const QpProblem qp = simple_box_qp();
    Vec at(2);
    at << 0.2, 0.5;
    QpSolution face = face_of(at, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{true});
    face.tr_active[1] = true;

    SolveOverrides overrides;
    overrides.tr_radius = 0.5;

    QpSolution out;
    QpEngine engine{QpOptions{}};
    ASSERT_TRUE(engine.refine_on_face(qp, face, overrides, out));
    EXPECT_NEAR(0.5, out.x(0), 1e-12);
    EXPECT_NEAR(0.5, out.x(1), 1e-12) << "the TR pin held";
    EXPECT_NEAR(0.5, out.lambda_i(0), 1e-9);
    EXPECT_EQ(0.0, out.z(1)) << "a trust-region pin is not a bound of the caller's problem";

    // AND THE SAME FACE WITHOUT THE RADIUS lands somewhere else entirely --
    // which is what makes the assertion above about the RADIUS rather than
    // about arithmetic that would have happened anyway.
    QpSolution wide;
    ASSERT_TRUE(engine.refine_on_face(qp, face, SolveOverrides{}, wide));
    EXPECT_GT(std::abs(wide.x(1) - 0.5), 0.1);
}

// (f) THE INACTIVE-ROW GATE. An EQP knows nothing about the constraints that
// are OFF its face, so a face that omits a row the answer needs produces a
// point INSIDE the box that nonetheless violates that row. This is a different
// failure from (b) -- the box gate cannot see it -- and it is the assertion
// that the identified face was the RIGHT one.
//
// ANALYTIC. min 1/2||x||^2 - 5 x0 - 5 x1 over [-10, 10]^2 with rows
// x0 + x1 <= 1 (row 0) and x0 <= 0.2 (row 1).
//   * Face {row 0}: x + g + lambda (1,1) = 0 with x0 + x1 = 1 gives
//     x = (0.5, 0.5), which is inside the box but violates row 1 by 0.3.
//     REFUSED.
//   * Face {row 0, row 1}: x = (0.2, 0.8), which satisfies both. ACCEPTED.
// Both polarities on one fixture, so a gate that stopped refusing and a gate
// that started refusing everything are distinguishable.
TEST(QpEngineFaceRefine, AFaceThatViolatesARowItLeftOffIsRefused) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -5.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1.0, 0.2;
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);

    Vec at(2);
    at << 0.5, 0.5;
    QpEngine engine{QpOptions{}};

    QpSolution refused;
    EXPECT_FALSE(engine.refine_on_face(
        qp, face_of(at, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{true, false}),
        SolveOverrides{}, refused))
        << "the omitted row is violated by 0.3 at the face's own answer";
    EXPECT_EQ(1, refused.counters.factorizations) << "a gate refusal has already paid";
    EXPECT_EQ(at, refused.x);

    QpSolution taken;
    ASSERT_TRUE(engine.refine_on_face(
        qp, face_of(at, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{true, true}),
        SolveOverrides{}, taken))
        << "the COMPLETE face is accepted, so the refusal above is about the row and not about "
           "this fixture";
    EXPECT_NEAR(0.2, taken.x(0), 1e-12);
    EXPECT_NEAR(0.8, taken.x(1), 1e-12);
}

// (g) THE INERTIA GATE, which is the walk's own (detail::inertia_verdict)
// applied to the same reduced system with the same expected inertia. A face
// whose free block carries NEGATIVE CURVATURE is not a minimizer face: the KKT
// solve returns a stationary point of the wrong type, and handing that to the
// funnel as a step is exactly the "landed on the saddle" failure the escape
// routing exists to prevent.
//
// ANALYTIC. min 1/2 x0^2 - x1^2 - x0 - x1 over [-10, 10]^2, no rows. The face
// "everything free" has expected inertia (2 positive, 0 negative) and actual
// (1, 1), so the verdict is not kOk and the refinement is refused -- AFTER
// paying its factorization, which is what the verdict is read from.
//
// **BACKEND NOTE.** `inertia_verdict` reads the evidence's perturbed-pivot
// count (absent on Accelerate) and zero class, whose
// meaning differs on Apple Accelerate (checklist sections (h) and (j)). This
// arm asserts a REFUSAL, which is the safe direction on any backend whose
// verdict degrades; it is the ACCEPTING arms above that a degraded verdict
// would move.
TEST(QpEngineFaceRefine, AnIndefiniteFaceIsRefusedByTheInertiaGate) {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1, 0, 0, -2;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -1.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);

    Vec at(2);
    at << 1.0, -0.5;
    const QpSolution face =
        face_of(at, {BoundState::kFree, BoundState::kFree}, std::vector<bool>{});

    QpSolution out;
    QpEngine engine{QpOptions{}};
    EXPECT_FALSE(engine.refine_on_face(qp, face, SolveOverrides{}, out))
        << "expected inertia (2, 0); this face's free block is (1, 1)";
    EXPECT_EQ(1, out.counters.factorizations)
        << "the verdict is READ FROM the factorization, so it cannot be refused before paying";
    EXPECT_EQ(at, out.x) << "and the caller keeps its own answer";
}
