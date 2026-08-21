// test_qp_engine_indefinite.cpp — INDEFINITE-H coverage of the QP engine
// (qp_engine.h section 4b: the inertia gate and the temporary-vertex start
// repair; section 4c: the negative-curvature ride after a drop).
//
// WHY THIS FILE DOES NOT CHECK CROSS-MODE EQUALITY. The convex battery
// (test_qp_engine_border.cpp's BorderModeMatchesRefactorizeMode) holds
// kSchurBorder and kRefactorize observationally equivalent. That claim does
// NOT extend to an indefinite H: the bound-eliminated K and the full-variable
// K0 have structurally different inertia (elimination removes exactly the rows
// that carry the negative curvature), so the two modes may legitimately walk
// different working sets. Indefinite fixtures are therefore validated against
// their HAND-KNOWN minimizers plus a second-order-necessity check computed in
// the test itself, per mode -- never against each other.
//
// Task 8 generalized that into an enumerated LOCAL-MINIMIZER ORACLE
// (tests/sqp/support/dense_oracle.h's enumerate_local_minimizers), which the last
// third of this file exercises: the oracle's own hand-derived self-tests, then
// a battery -- eight hand-built indefinite QPs plus 25 randomized ones -- that
// only requires the engine's answer to be SOME local minimizer, which is all a
// descent method promises on a nonconvex QP and is the strongest statement that
// survives the two modes walking different working sets.

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <string>
#include <vector>

#include <Eigen/Dense>

#include <gtest/gtest.h>

#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/qp/qp_engine.h>

#include "support/dense_oracle.h"

using namespace hven::solvers;
using hven::Index;
using hven::SpMatRM;
using hven::Vec;

namespace {

// hven's SymmetricFactor validates the STRUCTURAL diagonal at analyze() --
// every row's diagonal entry must be present in the pattern, value zero or
// not (symmetric_factor.h; the engine's own assemblies emit it
// unconditionally). The dissolved seam forwarded the pattern unvalidated, so
// these dense-built fixtures could drop the zero diagonal via sparseView();
// this builder keeps it explicit, exactly as test_kkt_calls.cpp's fixture
// does.
SpMatRM upper_with_structural_diag(const Eigen::MatrixXd &D) {
    SpMatRM K(D.rows(), D.cols());
    for (Eigen::Index r = 0; r < D.rows(); ++r) {
        for (Eigen::Index c = r; c < D.cols(); ++c) {
            if (r == c || D(r, c) != 0.0) {
                K.insert(r, c) = D(r, c);
            }
        }
    }
    K.makeCompressed();
    return K;
}

// min 1/2 (x0^2 - x1^2) over [-1,1]^2: H = diag(1, -1), g = 0, no general
// constraints. The only stationary point of the unconstrained objective is the
// SADDLE x = (0,0) (objective 0); the true QP minimizers are x = (0, +/-1)
// with objective -0.5. The engine's start point is clamp(0, l, u) = (0,0),
// i.e. it starts exactly on the saddle.
QpProblem saddle_box_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = -1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1.0);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

// The Task 3 findings probe's near-singular pattern (a decoupled tiny
// diagonal) embedded in a QP: H = diag(1, h11, 1) with an equality row pinning
// x0 = 0 and box [-1,1]^3.
//
// h11 is chosen relative to QpOptions::primal_delta (1e-8), which is what K0's
// (1,1) diagonal entry ends up being h11 + delta:
//   h11 == -1e-8 makes that entry EXACTLY zero -- the findings doc's
//     ExactlySingular case, and pardiso perturbs a pivot (measured:
//     a perturbed-pivot count of 1) while reporting a plausible (3, 1).
//   h11 == -2e-8 leaves it at -1e-8: nonsingular, genuinely indefinite, and
//     pardiso reports the true (2, 2) with ZERO perturbed pivots.
// Either way the minimizer is x = (0, 1, 1): x0 is fixed by the equality, x1's
// objective g1*x1 + h11*x1^2/2 = -x1 + O(1e-8) decreases to its upper bound,
// and x2's 1/2 x2^2 - x2 is minimized at x2 = 1 (its upper bound).
QpProblem near_singular_indefinite_qp(double h11) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = h11;
    Hd(2, 2) = 1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(3, -1.0);
    Eigen::MatrixXd Aed(1, 3);
    Aed << 1, 0, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1.0);
    qp.upper = Vec::Constant(3, 1.0);
    return qp;
}

// The SAME cancellation, with the ratio test's accidental rescue removed --
// the fixture the SUSPECT-STALL GATE (qp_engine.h section 4b) is pinned on.
//
// H = diag(1, -delta, 1), g = (-1,-1,-1), equality x0 = 0, box [-1,1] on x0/x2
// and [-x1_bound, x1_bound] on x1, solved with QpOptions::primal_delta ==
// delta so K0's (1,1) entry is again EXACTLY zero and the seed factorization is
// again suspect on both backends.
//
// The only lever is x1_bound, and it decides whether the backend's
// singular-direction garbage is USABLE. With x1_bound == 1 the huge value
// Pardiso emits along the fabricated pivot overshoots the box, the ratio test
// clips it, x1 pins at its upper bound and the engine lands the true minimizer
// -- the accidental MKL rescue documented in D9. With x1_bound == 1e12 nothing
// clips it: at delta == 1e-10 the garbage lands at x1 ~ 4e8, which is BELOW
// is_runaway's threshold (0.1/delta == 1e9), so the runaway guard does not fire
// either, the next iteration's step is negligible, and the loop reaches the
// certification branch with x1 FREE and its reduced gradient at 1.04.
//
// PRE-FIX BEHAVIOR ON MKL (measured, Task 3b, both algebra modes): status
// kOptimal, x = (1e-16, 4.04e8, 1), minor_iters 2, factorizations 3, free-block
// stationarity residual 1.04. That is a certified-optimal NON-KKT point on the
// backend D9 was reported as recovering on -- the audit's "the same scenario
// exists latently on MKL in a different costume", made concrete.
QpProblem suspect_stall_qp(double delta, double x1_bound) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = -delta;
    Hd(2, 2) = 1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(3, -1.0);
    Eigen::MatrixXd Aed(1, 3);
    Aed << 1, 0, 0;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec(3);
    qp.lower << -1.0, -x1_bound, -1.0;
    qp.upper = Vec(3);
    qp.upper << 1.0, x1_bound, 1.0;
    return qp;
}

// The free-block stationarity residual the gate measures, recomputed here from
// the returned solution alone so the test judges the engine's answer rather
// than re-running the engine's own helper.
double free_block_stationarity_of(const QpProblem &qp, const QpSolution &sol) {
    Vec r = qp.H.selfadjointView<Eigen::Upper>() * sol.x + qp.g;
    if (qp.me() > 0) {
        r += qp.Ae.transpose() * sol.lambda_e;
    }
    if (qp.mi() > 0) {
        r += qp.Ai.transpose() * sol.lambda_i;
    }
    double worst = 0.0;
    for (Index i = 0; i < qp.n(); ++i) {
        if (sol.bound_state[static_cast<std::size_t>(i)] == BoundState::kFree) {
            worst = std::max(worst, std::abs(r(i)));
        }
    }
    return worst;
}

// --- Section 4c fixtures: a DROP exposes negative curvature ---------------
//
// Both fixtures below are built so the SEED working set is second-order
// CONSISTENT (no temporary-vertex repair runs at iteration 0) and the negative
// curvature is exposed only later, by the drop rule. That is the only way to
// exercise the ride: per section 4b's Cauchy-interlacing note, adding rows can
// never reintroduce negative curvature, so a drop is the sole source.
//
// The trick that makes the seed consistent on an indefinite H is the homotopy:
// general rows VIOLATED at the start point x = clamp(0, l, u) are placed in the
// working set by refresh_shifts() before the loop's first iteration, and if H
// is positive definite on the null space of those rows the gate returns kOk.

// (a) RIDE TO A BOUND. n = 2, H = [[1, 2], [2, 1]] (eigenvalues 3 and -1),
// g = 0, one row x0 <= -1, box x0 in [-20, 20], x1 in [-5, 5].
//
// Hand trace (all four steps verified against the engine):
//   iter 0  x = (0, 0); row 0 is violated by 1, so the seed working set is
//           {row 0}. null({e0}) = span{e1} and H11 = 1 > 0, so the reduced
//           Hessian is PD and the gate returns kOk -- NO repair. The EQP
//           solves x0 = -1, 2*x0 + x1 = 0 => x = (-1, 2); nothing blocks
//           (the nearest ratio is 2.5), so the full step is taken.
//   iter 1  KKT point. grad = Hx = (3, 0), so row 0 prices at lambda = -3 < 0
//           and the drop rule releases it.
//   iter 2  Working set is now EMPTY and H itself is indefinite. The EQP
//           "solution" is the SADDLE x* = (0, 0), so the step is
//           p = (0,0) - (-1,2) = (1, -2) with p'Hp = 1 - 8 + 4 = -3 < 0:
//           NEGATIVE CURVATURE, and p points at x0 = 0, i.e. straight back
//           INTO the row just dropped. The ride negates it: dir = (-1, 2),
//           which leaves row 0 feasibly (a0.dir = -1 < 0) and descends
//           (grad.dir = -3 < 0). Ratio test, uncapped: row 0 recedes, x0's
//           lower bound is 19 away, x1's UPPER BOUND is 1.5 away and blocks.
//           x = (-1,2) + 1.5*(-1,2) = (-2.5, 5), x1 pinned at its upper bound.
//   iter 3  Free x0 against x1 = 5: x0 = -2*5 = -10, interior to [-20, 20],
//           full step.
//   iter 4  KKT point x = (-10, 5): grad = (0, -15), z1 = -15 <= 0 at an upper
//           bound, row 0 inactive with lambda = 0. kOptimal, objective -37.5,
//           and the reduced Hessian over the one free direction is [1] > 0.
//
// -37.5 is also the GLOBAL minimum (checked by hand over both x1 faces), so
// the assertions do not merely pin whichever local minimizer this walk found.
//
// PRE-FIX BEHAVIOR (measured, before the ride existed): kMaxIter in BOTH
// modes, stalled at x = (-1, 2) with lambda_0 = -3. Iteration 2's step points
// into the dropped row, whose slack is exactly zero, so the ratio test returns
// alpha = 0, the row is immediately re-added, and iterations 1-2 cycle until
// max_iter (500 minor iterations, 500 factorizations in kRefactorize).
QpProblem ride_to_bound_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1, 2, 2, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec(2);
    qp.lower << -20.0, -5.0;
    qp.upper = Vec(2);
    qp.upper << 20.0, 5.0;
    return qp;
}

// (b) RIDE TO A DIFFERENT ROW. n = 3,
//     H = [[1, 0, 2], [0, 1, 0], [2, 0, 1]],  g = 0,
//     rows  x0 <= -1 (row 0),  x1 <= -1 (row 1),  x2 <= 6 (row 2),
//     box [-20, 20]^3.
//
// H's {0,2} block is [[1,2],[2,1]] with eigenvalues 3 and -1, so H is
// indefinite; H is PD on span{e2} (H22 = 1) and on span{e0} (H00 = 1), but
// INDEFINITE on span{e0, e2}. That is the whole design: the seed working set
// {row 0, row 1} leaves only span{e2} free (consistent), and dropping row 0
// opens span{e0, e2} (inconsistent).
//
// Hand trace:
//   iter 0  x = 0; rows 0 and 1 are each violated by 1 and enter the working
//           set, row 2 (x2 <= 6) is slack. Reduced Hessian on span{e2} is
//           [1] > 0 => gate kOk, no repair. EQP: x0 = x1 = -1 and
//           2*(-1) + x2 = 0 => x = (-1, -1, 2). Nearest ratio is 3 (row 2),
//           so the full step is taken and both shifts close.
//   iter 1  KKT point, grad = Hx = (3, -1, 0). Rows 0 and 1 price at
//           lambda = (-3, 1): row 0 is released.
//   iter 2  Working set {row 1}. The free subspace is span{e0, e2}, where the
//           reduced Hessian [[1,2],[2,1]] has eigenvalue -1 -- and by Cauchy
//           interlacing exactly one negative eigenvalue, since it was PD
//           before the drop. The EQP returns the SADDLE (0, -1, 0), so
//           p = (1, 0, -2), p'Hp = 1 - 8 + 4 = -3 < 0. Ride along
//           dir = (-1, 0, 2): row 0 recedes (a0.dir = -1), row 2 approaches
//           (a2.dir = 2) with slack 6 - 2 = 4, so it blocks at alpha = 2 --
//           BEFORE either box bound (19 and 9 away). x = (-3, -1, 6), and
//           ROW 2 -- a different row from the one dropped -- enters.
//   iter 3  Free x0 against x1 = -1, x2 = 6: x0 + 2*6 = 0 => x0 = -12,
//           interior to [-20, 20], full step (ratio 1.889 > 1).
//   iter 4  KKT point x = (-12, -1, 6): grad = (0, -1, -18) so
//           lambda = (0, 1, 18) -- both working rows strictly positive, row 0
//           inactive and satisfied (-12 <= -1). kOptimal, objective -53.5.
//           The free subspace is span{e0} and H00 = 1 > 0.
//
// PRE-FIX BEHAVIOR (measured): kMaxIter in BOTH modes, stalled at
// x = (-1, -1, 2) with lambda = (-3, 1, 0) -- the same drop/re-add cycle as
// fixture (a).
QpProblem ride_to_row_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    Hd << 1, 0, 2, 0, 1, 0, 2, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(3);
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(3, 3);
    Aid << 1, 0, 0, 0, 1, 0, 0, 0, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(3);
    qp.bi << -1.0, -1.0, 6.0;
    qp.lower = Vec::Constant(3, -20.0);
    qp.upper = Vec::Constant(3, 20.0);
    return qp;
}

// The brief's diagonal fixture, shared between
// DiagonalIndefiniteBoxReachesTheNegativeCurvatureBound (which asserts which of
// its two local minimizers the engine walks to) and the Task 8 oracle self-test
// (which asserts that BOTH are enumerated).
//
// min 0.1*x0 - 1/2 x0^2 + 1/2 x1^2 over [-2, 2]^2. Separable: x1's term is
// convex and minimized at the interior point x1 = 0; x0's term is CONCAVE, so
// its minima over [-2, 2] are the endpoints -- and BOTH of them are genuine
// local minimizers here, x0 = -2 (objective -2.2, the global one) and x0 = +2
// (objective -1.8). OracleEnumeratesBothDiagonalBoxMinimizers checks the
// multipliers that make the second one a KKT point too.
QpProblem diagonal_indefinite_box_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = -1.0;
    Hd(1, 1) = 1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 0.1, 0.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -2.0);
    qp.upper = Vec::Constant(2, 2.0);
    return qp;
}

double objective(const QpProblem &qp, const Vec &x) {
    return qp.g.dot(x) + 0.5 * x.dot(qp.H.selfadjointView<Eigen::Upper>() * x);
}

// SECOND-ORDER NECESSITY, checked directly rather than taken on trust: with no
// general constraints active, the critical cone is spanned by the FREE
// variables, so the reduced Hessian is just H's principal submatrix over them
// and second-order necessity is that submatrix being PSD. Returns its smallest
// eigenvalue (+inf when nothing is free, which is vacuously second-order
// consistent).
double smallest_free_eigenvalue(const QpProblem &qp, const QpSolution &sol) {
    std::vector<Index> free_idx;
    for (Index i = 0; i < qp.n(); ++i) {
        if (sol.bound_state[static_cast<std::size_t>(i)] == BoundState::kFree) {
            free_idx.push_back(i);
        }
    }
    if (free_idx.empty()) {
        return std::numeric_limits<double>::infinity();
    }
    const Eigen::MatrixXd Hupper = Eigen::MatrixXd(qp.H);
    const Eigen::MatrixXd Hd = Hupper.selfadjointView<Eigen::Upper>();
    const auto k = static_cast<Index>(free_idx.size());
    Eigen::MatrixXd Hred(k, k);
    for (Index a = 0; a < k; ++a) {
        for (Index b = 0; b < k; ++b) {
            Hred(a, b) =
                Hd(free_idx[static_cast<std::size_t>(a)], free_idx[static_cast<std::size_t>(b)]);
        }
    }
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hred);
    return es.eigenvalues().minCoeff();
}

// Second-order necessity over the FULL active set -- working inequality rows
// and equalities as well as pinned variables -- which is what the fixtures
// below with general constraints need (smallest_free_eigenvalue above only
// covers the box-only case). Builds the active-constraint Jacobian, takes an
// orthonormal basis Z of its null space, and returns the smallest eigenvalue of
// Z^T H Z. Returns +inf when that null space is trivial: with every direction
// pinned there is nothing for negative curvature to live in, so the point is
// vacuously second-order consistent.
//
// Under STRICT complementarity (every active multiplier nonzero, which the
// fixtures assert separately) the critical cone IS that null space, so a
// nonnegative return value is exactly second-order necessity.
double smallest_reduced_eigenvalue(const QpProblem &qp, const QpSolution &sol) {
    const Index n = qp.n();
    const Eigen::MatrixXd Aid(qp.Ai);
    const Eigen::MatrixXd Aed(qp.Ae);
    std::vector<Eigen::RowVectorXd> act;
    for (Index j = 0; j < qp.mi(); ++j) {
        if (sol.ineq_active[static_cast<std::size_t>(j)]) {
            act.push_back(Aid.row(j));
        }
    }
    for (Index j = 0; j < qp.me(); ++j) {
        act.push_back(Aed.row(j));
    }
    for (Index i = 0; i < n; ++i) {
        if (sol.bound_state[static_cast<std::size_t>(i)] != BoundState::kFree) {
            act.push_back(Eigen::RowVectorXd::Unit(n, i));
        }
    }
    const Eigen::MatrixXd Hupper(qp.H);
    const Eigen::MatrixXd Hd = Hupper.selfadjointView<Eigen::Upper>();

    Eigen::MatrixXd Z;
    if (act.empty()) {
        Z = Eigen::MatrixXd::Identity(n, n);
    } else {
        Eigen::MatrixXd A(static_cast<Index>(act.size()), n);
        for (std::size_t k = 0; k < act.size(); ++k) {
            A.row(static_cast<Index>(k)) = act[k];
        }
        const Eigen::FullPivLU<Eigen::MatrixXd> lu(A);
        if (lu.rank() >= n) {
            return std::numeric_limits<double>::infinity();
        }
        // Orthonormalize the raw kernel basis so Z^T H Z is a genuine
        // restriction of H rather than a congruence by a skewed basis.
        const Eigen::MatrixXd ker = lu.kernel();
        const Eigen::HouseholderQR<Eigen::MatrixXd> qr(ker);
        Z = qr.householderQ() * Eigen::MatrixXd::Identity(n, ker.cols());
    }
    const Eigen::MatrixXd Hred = Z.transpose() * Hd * Z;
    Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hred);
    return es.eigenvalues().minCoeff();
}

// Index of the enumerated point nearest `x` (infinity norm), or -1 if the
// nearest one is farther than `tol`. Used by the Task 8 oracle self-tests and
// by EngineLandsOnALocalMinimizer.
Index index_of_point(const std::vector<QpSolution> &pts, const Vec &x, double tol) {
    Index best = -1;
    double best_dist = tol;
    for (std::size_t k = 0; k < pts.size(); ++k) {
        const double d = (pts[k].x - x).lpNorm<Eigen::Infinity>();
        if (d <= best_dist) {
            best_dist = d;
            best = static_cast<Index>(k);
        }
    }
    return best;
}

bool contains_point(const std::vector<QpSolution> &pts, const Vec &x, double tol) {
    return index_of_point(pts, x, tol) >= 0;
}

Vec vec2(double a, double b) {
    Vec v(2);
    v << a, b;
    return v;
}

Vec vec3(double a, double b, double c) {
    Vec v(3);
    v << a, b, c;
    return v;
}

// Multiplier signs at the reported active set (qp_problem.h's convention:
// z >= 0 at a lower bound, z <= 0 at an upper bound).
void expect_bound_multiplier_signs(const QpProblem &qp, const QpSolution &sol, double tol) {
    for (Index i = 0; i < qp.n(); ++i) {
        switch (sol.bound_state[static_cast<std::size_t>(i)]) {
        case BoundState::kAtLower:
            EXPECT_GE(sol.z(i), -tol) << "variable " << i;
            break;
        case BoundState::kAtUpper:
            EXPECT_LE(sol.z(i), tol) << "variable " << i;
            break;
        default:
            break;
        }
    }
}

} // namespace

// (a) The engine starts ON the saddle (0,0) of an indefinite QP and must still
// reach a genuine local minimizer.
//
// PRE-FIX BEHAVIOR (measured before the inertia gate existed): the first EQP
// solve returns x* = (0,0) -- the regularized KKT system is nonsingular and
// happily solves to the saddle -- so the step is negligible, nothing may be
// dropped, and the engine reported kOptimal at x = (0,0) with objective 0,
// both variables kFree, in 1 factorization / 1 minor iteration. A silently
// wrong answer: no throw, no kNumericalError, and the reported point is not
// even a local minimizer.
TEST(QpEngineIndefinite, IndefiniteStartRepairsToVertex) {
    const QpProblem qp = saddle_box_qp();
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);
        const std::string tag =
            algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize";
        SCOPED_TRACE(tag);

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        // x0 sits at the bottom of its positive-curvature direction; x1 is
        // driven to one of the two bounds (either is a true minimizer).
        EXPECT_NEAR(sol.x(0), 0.0, 1e-8);
        EXPECT_NEAR(std::abs(sol.x(1)), 1.0, 1e-8);
        EXPECT_NEAR(objective(qp, sol.x), -0.5, 1e-8);

        // The negative-curvature variable must be REPORTED active, not merely
        // parked at a bound while claiming to be free.
        EXPECT_NE(sol.bound_state[1], BoundState::kFree);
        EXPECT_EQ(sol.bound_state[0], BoundState::kFree);
        expect_bound_multiplier_signs(qp, sol, 1e-8);

        // Second-order necessity: the reduced Hessian over the free variables
        // is [1] here, so PSD. (At the pre-fix answer both variables were free
        // and the reduced Hessian was diag(1, -1) -- indefinite, i.e. the old
        // answer fails this check outright.)
        EXPECT_GE(smallest_free_eigenvalue(qp, sol), -1e-10);
    }
}

// (b) A factorization pardiso had to perturb must never be trusted, not even
// when its reported inertia happens to MATCH the expectation.
//
// h11 = -1e-8 puts K0's (1,1) entry at exactly zero. Measured: pardiso reports
// (pos, neg) = (3, 1) -- precisely the expected inertia for this system -- with
// a perturbed-pivot count of 1 and no error raised. A gate reading only the sign
// counts would conclude "second-order consistent, nothing to do" on a
// factorization one of whose pivot signs was fabricated.
//
// The observable here is that the engine does not settle on that factorization:
// it refactorizes (factorizations >= 2 -- one K0 rebuild per iteration while the
// perturbation persists) and still returns the true minimizer. Note the gate's
// kSuspect verdict deliberately does NOT trigger the temporary-vertex repair --
// acting on a fabricated inertia is exactly what the findings doc forbids --
// so this fixture also pins that a suspect verdict cannot pin variables at
// bounds and corrupt an otherwise-correct answer.
//
// ACCELERATE CONTRAST -- FORMERLY KNOWN DEFECT D9, now the fix's re-measured
// recovery. See finding D9 in
// docs/notes/2026-07-29-accelerate-audit-results.md for the original
// mechanism and its "D9 re-measurement" addendum for the flip below.
//
// TASK 3b UPDATE (Linux side, engine fix landed): the decision was taken and
// both candidates were implemented as SECTION 4b's SUSPECT-STALL GATE -- see
// SuspectStallIsNotCertifiedOptimal / SuspectLoopStillCertifiesAStationaryPoint
// below. THIS fixture is deliberately unchanged on the MKL side, because the
// MKL trajectory is unchanged: the point it certifies IS stationary (the pin
// removed x1 from the free block), so the gate passes it and no escalation
// rung is spent. The Accelerate arm below was that fix's tripwire.
//
// MAC RE-MEASUREMENT (2026-07-29, merge bdf64da): the tripwire FIRED --
// sol.x(1) came back 1, not the pinned defect's 0, in BOTH ws_algebra modes,
// Release and Debug -- and the arm below now pins the post-fix observation.
TEST(QpEngineIndefinite, InertiaGateRefusesPerturbedFactorization) {
    const QpProblem qp = near_singular_indefinite_qp(-1e-8);

    // Precondition of the fixture: the engine's own K0 for the seed working
    // set really is the perturbed case (this is what makes the test a test).
    {
        QpOptions opts;
        WorkingSet ws(3, 0);
        const KktAssembly a = assemble_kkt_full(qp, ws, opts);
        detail::KktFactor kkt;
        const hven::linear::InertiaEvidence e = detail::factorize_checked(kkt, a.K).inertia;
        ASSERT_EQ(e.state, hven::linear::InertiaEvidence::State::kObserved);
// OBSERVED (results note §(a)): Accelerate reports (2, 1) plus one zero
// pivot -- through InertiaEvidence, an honestly-absent perturbed_pivots and
// a natively-measured n_zero == 1 -- so pos + neg falls short of the
// dimension and inertia_verdict lands on kSuspect through the short-sum
// rule; on MKL only the perturbed-pivot count flags it while the sign
// counts impersonate a healthy (3, 1).
#ifdef USE_ACCELERATE_SPARSE
        ASSERT_EQ(e.n_pos, 2); // honest: the zero pivot is in the zero bucket
        ASSERT_EQ(e.n_neg, 1);
        ASSERT_FALSE(e.zero_is_derived);
        ASSERT_EQ(e.n_zero, 1);
#else
        ASSERT_TRUE(e.perturbed_pivots.has_value());
        ASSERT_EQ(*e.perturbed_pivots, 1);
        ASSERT_EQ(e.n_pos, 3); // looks exactly like a healthy factorization
        ASSERT_EQ(e.n_neg, 1);
#endif
        // ... and the gate refuses it anyway, precisely because of the
        // perturbed pivot: the expectation it is handed here is the one it
        // "matches".
        EXPECT_EQ(detail::inertia_verdict(e, 3, 1), detail::InertiaVerdict::kSuspect);
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 0.0, 1e-7);
#ifdef USE_ACCELERATE_SPARSE
        // D9 RE-MEASUREMENT (post-fix, merge bdf64da). HISTORY, kept as the
        // defect's record -- the KNOWN-DEFECT pin that stood here (audit
        // finding D9; the HS26 pin-the-bug precedent) was: kOptimal at
        // x = (1e-16, 0, 1), free-x1 reduced gradient -1, BOTH ws_algebra
        // modes. The seed K0's x1 row is exactly zero (h11 == -primal_delta),
        // Accelerate's zero-pivot solves are subspace-clean, so the iterate
        // never moved along the singular direction and the suspect-rebuild
        // loop certified a non-KKT point; Pardiso only escaped because its
        // perturbed solves emit garbage-but-nonzero values the ratio test
        // clips to the bound -- an accidental rescue, not a policy.
        //
        // OBSERVED NOW (both modes, Release and Debug): the suspect-stall
        // gate refuses that certification, ONE rung de-singularizes K0
        // (primal_delta 1e-8 -> 1e-7, so the (1,1) entry is 9e-8 rather than
        // 0), the de-singularized solve overshoots x1's box and is clipped
        // onto the bound, and the engine RECOVERS to the true minimizer:
        // kOptimal at x = (1e-16, 1, 1), free-block stationarity 1.0e-15
        // (hand-recomputed via free_block_stationarity_of), minor_iters 4,
        // factorizations 4 (border) / 6 (refactorize). NOT kNumericalError:
        // unlike the wide-box stall fixture, the [-1, 1] box turns the
        // escalated solve into exactly the MKL rescue shape. The rung count
        // is the observable that still distinguishes the backends -- MKL
        // spends 0 (its accident happens before the gate is consulted) -- so
        // it is pinned per-backend here.
        EXPECT_NEAR(sol.x(1), 1.0, 1e-7);
        EXPECT_EQ(sol.counters.suspect_escalations, 1);
#else
        EXPECT_NEAR(sol.x(1), 1.0, 1e-7);
#endif
        EXPECT_NEAR(sol.x(2), 1.0, 1e-7);
        EXPECT_GE(sol.counters.factorizations, 2);
        expect_bound_multiplier_signs(qp, sol, 1e-7);
    }
}

// SECTION 4b's SUSPECT-STALL GATE, positive arm -- and the MKL-REACHABLE
// costume of audit finding D9.
//
// The defect the gate exists for: the engine's response to a suspect
// factorization is to rebuild K0 and try again, but on an EXACTLY singular K0
// the rebuild reproduces the same matrix, so the loop can make zero progress
// and then certify anyway. D9 caught that on Accelerate, whose zero-pivot
// solves leave the singular direction untouched. It was believed to be
// Accelerate-only because Pardiso's perturbed solves emit garbage-but-nonzero
// values that USUALLY kick the iterate onto a bound. `suspect_stall_qp` removes
// that accident (see the fixture): with x1's box wide enough that neither the
// ratio test nor is_runaway intercepts the garbage, MKL certified kOptimal at
// x = (1e-16, 4.04e8, 1) -- free-block reduced gradient 1.04 -- in BOTH
// algebra modes.
//
// What the gate does about it: a would-be-kOptimal exit whose verdict is
// kSuspect must pass a free-block stationarity check on the QP model first.
// This point fails it by a factor of 1e9, so instead of certifying the engine
// escalates primal_delta one decade and resumes; at 1e-9 the (1,1) entry is
// 9e-10 rather than 0, K0 is nonsingular, and the true unbounded-artifact
// character of the direction becomes visible to is_runaway -- kNumericalError.
//
// The assertions are deliberately backend-agnostic: what is pinned is the
// STATUS and the ladder rung, not the trajectory. Accelerate reaches the same
// verdict by a different route (its iterate never moves at all, so the check
// fails at x1 == 0 rather than at 4e8) and the counters differ; the observable
// contract -- never kOptimal off a stalled suspect loop -- does not.
TEST(QpEngineIndefinite, SuspectStallIsNotCertifiedOptimal) {
    constexpr double kDelta = 1e-10;
    const QpProblem qp = suspect_stall_qp(kDelta, /*x1_bound=*/1e12);

    // Precondition: the seed K0 really is the exactly-singular case, on
    // either backend (a perturbed pivot on MKL; a natively-measured zero
    // eigenvalue on Accelerate -- the same trust signal through
    // InertiaEvidence's honest channel, docs/retarget-design-sqp.md §4.1).
    {
        QpOptions opts;
        opts.primal_delta = kDelta;
        WorkingSet ws(3, 0);
        const KktAssembly a = assemble_kkt_full(qp, ws, opts);
        detail::KktFactor kkt;
        const hven::linear::InertiaEvidence e = detail::factorize_checked(kkt, a.K).inertia;
#ifdef USE_ACCELERATE_SPARSE
        ASSERT_EQ(e.n_zero, 1);
#else
        ASSERT_TRUE(e.perturbed_pivots.has_value());
        ASSERT_EQ(*e.perturbed_pivots, 1);
#endif
        ASSERT_EQ(detail::inertia_verdict(e, 3, 1), detail::InertiaVerdict::kSuspect);
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.primal_delta = kDelta;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_NE(sol.status, QpStatus::kOptimal)
            << "certified a point whose free-block reduced gradient is "
            << free_block_stationarity_of(qp, sol);
        EXPECT_EQ(sol.status, QpStatus::kNumericalError);
        EXPECT_GE(sol.counters.suspect_escalations, 1);
        EXPECT_LE(sol.counters.suspect_escalations, detail::kMaxSuspectEscalations);
        // The kNumericalError exit's existing semantics: the iterate is
        // reported as evidence, the multipliers are not.
        EXPECT_EQ(sol.lambda_e.lpNorm<Eigen::Infinity>(), 0.0);
        EXPECT_EQ(sol.z.lpNorm<Eigen::Infinity>(), 0.0);
    }
}

// SECTION 4b's SUSPECT-STALL GATE, negative arm: the gate must not turn a
// PERSISTENT kSuspect into a refusal when the point it certifies is genuinely
// stationary. That case is not exotic -- it is the common one. In border mode
// K0 spans all n variables, so a singular direction that the border stack
// constrains (here: x1 pinned at its bound) leaves K0 perturbed for the rest of
// the solve while every bordered solve through it is exact.
//
// Same fixture, same delta, ONE lever changed: x1's box is [-1, 1] again, so
// the ratio test clips the singular direction onto the bound. The engine must
// still return the true minimizer (0, 1, 1), still certify it, and must NOT
// have spent a single rung of the escalation ladder.
//
// This is the guard that keeps the gate from being tightened into a blunt
// "refuse on kSuspect" rule -- which would cost correct answers the engine
// currently gets right, including the MKL arm of
// InertiaGateRefusesPerturbedFactorization above.
TEST(QpEngineIndefinite, SuspectLoopStillCertifiesAStationaryPoint) {
    constexpr double kDelta = 1e-10;
    const QpProblem qp = suspect_stall_qp(kDelta, /*x1_bound=*/1.0);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        opts.primal_delta = kDelta;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 0.0, 1e-7);
        EXPECT_NEAR(sol.x(1), 1.0, 1e-7);
        EXPECT_NEAR(sol.x(2), 1.0, 1e-7);
#ifdef USE_ACCELERATE_SPARSE
        // OBSERVED ON ACCELERATE (D9 re-measurement, merge bdf64da): ONE
        // rung, not zero -- and it is the gate WORKING, not the blunt refusal
        // this arm guards against. The subspace-clean zero-pivot solve never
        // moves x1, so the ratio-test rescue the MKL story above relies on
        // never happens; the loop instead reaches the certification branch at
        // the D9 point (x1 FREE at 0, reduced gradient -1), the gate refuses,
        // and the single rung de-singularizes K0 so the very next solve is
        // clipped onto the bound. The certification that finally fires is of
        // a genuinely stationary point -- residual 1.1e-16, final point and
        // status identical to MKL, both modes, Release and Debug. The
        // negative-arm claim "a stationary point certifies without further
        // rungs" holds on both backends; what differs is that MKL's accident
        // makes the gate unnecessary here and Accelerate's honest solves make
        // it necessary, exactly once.
        EXPECT_EQ(sol.counters.suspect_escalations, 1)
            << "spent other than the one observed rung; free-block reduced gradient is "
            << free_block_stationarity_of(qp, sol);
#else
        EXPECT_EQ(sol.counters.suspect_escalations, 0)
            << "escalated on a point whose free-block reduced gradient is "
            << free_block_stationarity_of(qp, sol);
#endif
        EXPECT_LE(free_block_stationarity_of(qp, sol), 1e-9);
        expect_bound_multiplier_signs(qp, sol, 1e-7);
    }
}

// SECTION 4b's SUSPECT-STALL GATE, NaN arm -- CALL-LEVEL, and deliberately so.
//
// `std::max(worst, NaN)` returns `worst`, so the obvious accumulation would
// DROP a NaN residual component, and `NaN > tol` is false, so the obvious
// comparison would then certify `kOptimal`. Together that is the Phase-3
// NaN-certified-optimal defect class reached from the other side: a
// factorization degenerate enough to emit NaN would get its answer stamped
// optimal by the very gate added to stop exactly that. Both halves are
// therefore NaN-aware -- `!(ri <= worst)` here, `!(free_stat <= tol)` at the
// call site in run().
//
// WHY THERE IS NO END-TO-END ARM. A NaN in the primal poisons the step long
// before the certification branch: `p.lpNorm<Infinity>() <= step_tol` is FALSE
// for a NaN p, so the loop never takes the negligible-step branch the gate
// lives in, and the ratio test disposes of the iterate instead. The only
// shape that reaches the gate with a NaN is a clean primal and a NaN
// MULTIPLIER, which no backend-portable fixture can force. So the primitive is
// tested where it can be tested -- which is why it lives in `detail`, next to
// inertia_verdict, rather than on QpEngine.
TEST(QpEngineIndefinite, FreeBlockStationarityTreatsNanAsAFailure) {
    const QpProblem qp = suspect_stall_qp(1e-10, /*x1_bound=*/1e12);
    WorkingSet ws(3, 0); // all free
    const Vec lambda_i = Vec(0);
    const double nan = std::numeric_limits<double>::quiet_NaN();

    // CONTROL: a finite residual reads finite, and reads the value the gate
    // would compare. At x = 0 with lambda_e = 0 the residual is just g.
    {
        double scale = 0.0;
        const double r =
            detail::free_block_stationarity(qp, Vec::Zero(3), ws, Vec::Zero(1), lambda_i, scale);
        EXPECT_DOUBLE_EQ(r, 1.0); // |g|inf
        EXPECT_DOUBLE_EQ(scale, 1.0);
        EXPECT_FALSE(std::isnan(r));
    }

    // A NaN on a FREE coordinate must survive the accumulation. It enters
    // through lambda_e here -- the cheapest seam, and the one shape that could
    // reach the gate for real (a clean primal with a poisoned multiplier).
    {
        double scale = 0.0;
        const double r = detail::free_block_stationarity(qp, Vec::Zero(3), ws,
                                                         Vec::Constant(1, nan), lambda_i, scale);
        EXPECT_TRUE(std::isnan(r)) << "a NaN residual component was silently dropped -- "
                                      "std::max(worst, NaN) returns worst";
    }

    // THE VERDICT HALF, on the same function run() calls -- not a
    // transcription of it. A NaN residual must read NOT stationary, so the
    // call site's `!stationary` routes it to the escalation ladder.
    {
        QpOptions opts; // opt_tol = 1e-9
        EXPECT_FALSE(detail::free_block_is_stationary(nan, 1.0, opts))
            << "a NaN residual must never read as stationary";
        EXPECT_FALSE(detail::free_block_is_stationary(nan, 1e6, opts)) << "...at any scale";
        // The ordinary readings, so the predicate is pinned in both
        // directions and cannot be satisfied by a constant.
        EXPECT_TRUE(detail::free_block_is_stationary(1e-12, 1.0, opts));
        EXPECT_FALSE(detail::free_block_is_stationary(1.04, 1.0, opts)); // the D9 defect
        EXPECT_TRUE(detail::free_block_is_stationary(1e-4, 1e6, opts))   // scaling is live
            << "the threshold must scale with the largest term in r";
    }

    // A NaN on a PINNED coordinate is NOT this gate's business: r(i) there is
    // the bound multiplier z(i), which the drop rule judges. Ordering matters
    // for the accumulation, so the NaN is placed on the coordinate the loop
    // visits FIRST among the pinned ones.
    {
        WorkingSet pinned(3, 0);
        pinned.bound_state()[0] = BoundState::kAtLower;
        Eigen::MatrixXd aed(1, 3);
        aed << 1, 0, 0;
        QpProblem q2 = qp;
        q2.Ae = aed.sparseView(); // Ae^T*NaN lands on x0 alone, which is pinned
        double scale = 0.0;
        const double r = detail::free_block_stationarity(q2, Vec::Zero(3), pinned,
                                                         Vec::Constant(1, nan), lambda_i, scale);
        EXPECT_FALSE(std::isnan(r)) << "a NaN on a pinned coordinate must not reach the free-block "
                                       "verdict";
        EXPECT_DOUBLE_EQ(r, 1.0);
    }
}

// The gate's perturbed-pivot policy, pinned directly against the findings
// doc's own matrix (docs/notes/2026-07-27-pardiso-inertia-findings.md): K =
// [[1,0,1],[0,0,0],[1,0,0]] factorizes with no error, reports (2, 1) -- which
// is the EXACT analytic inertia of every eps > 0 neighbor of this matrix, and
// so looks entirely healthy -- and sets the perturbed-pivot count to 1. Whatever
// expectation it is compared against, the verdict must be kSuspect: never kOk
// (which would trust a fabricated sign) and never kWrong (which would act on
// one as evidence).
TEST(QpEngineIndefinite, InertiaVerdictNeverTrustsAPerturbedFactorization) {
    Eigen::MatrixXd D(3, 3);
    D << 1, 0, 1, 0, 0, 0, 1, 0, 0;
    SpMatRM K = upper_with_structural_diag(D);
    detail::KktFactor kkt;
    const hven::linear::InertiaEvidence e = detail::factorize_checked(kkt, K).inertia;
#ifdef USE_ACCELERATE_SPARSE
    ASSERT_EQ(e.n_zero, 1);
#else
    ASSERT_TRUE(e.perturbed_pivots.has_value());
    ASSERT_EQ(*e.perturbed_pivots, 1);
#endif

    EXPECT_EQ(detail::inertia_verdict(e, 2, 1), detail::InertiaVerdict::kSuspect);
    EXPECT_EQ(detail::inertia_verdict(e, 1, 2), detail::InertiaVerdict::kSuspect);

    // Control: the same matrix with eps = 1e-12 factorizes unperturbed, and
    // there the same two calls are decided on the numbers themselves.
    Eigen::MatrixXd De(3, 3);
    De << 1, 0, 1, 0, 1e-12, 0, 1, 0, 0;
    SpMatRM Ke = upper_with_structural_diag(De);
    detail::KktFactor kkt_e;
    const hven::linear::InertiaEvidence e_ctrl = detail::factorize_checked(kkt_e, Ke).inertia;
    if (e_ctrl.perturbed_pivots.has_value()) {
        ASSERT_EQ(*e_ctrl.perturbed_pivots, 0);
    }
    EXPECT_EQ(detail::inertia_verdict(e_ctrl, 2, 1), detail::InertiaVerdict::kOk);
    EXPECT_EQ(detail::inertia_verdict(e_ctrl, 1, 2), detail::InertiaVerdict::kWrong);
    // A short sum (an expectation of a different dimension) is unknowable,
    // not wrong.
    EXPECT_EQ(detail::inertia_verdict(e_ctrl, 2, 2), detail::InertiaVerdict::kSuspect);
}

// A TRUSTWORTHY wrong inertia -- the case the repair exists for -- on the same
// near-singular pattern: h11 = -2e-8 leaves K0's (1,1) entry at -1e-8, so
// pardiso reports (2, 2) against an expected (3, 1) with zero perturbed pivots.
//
// PRE-FIX BEHAVIOR (measured): the engine returned x = (0, 0, 1), objective
// -0.5, status kOptimal, with x1 reported kFree and z1 == 0 -- a point that is
// not even stationary (the objective's x1-gradient there is -1). The true
// minimizer x = (0, 1, 1) has objective -1.5.
TEST(QpEngineIndefinite, TrustworthyWrongInertiaRepairsToVertex) {
    const QpProblem qp = near_singular_indefinite_qp(-2e-8);
    {
        QpOptions opts;
        WorkingSet ws(3, 0);
        const KktAssembly a = assemble_kkt_full(qp, ws, opts);
        detail::KktFactor kkt;
        const hven::linear::InertiaEvidence e = detail::factorize_checked(kkt, a.K).inertia;
        if (e.perturbed_pivots.has_value()) {
            ASSERT_EQ(*e.perturbed_pivots, 0);
        }
        ASSERT_EQ(detail::inertia_verdict(e, /*expected_pos=*/3, /*expected_neg=*/1),
                  detail::InertiaVerdict::kWrong);
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        QpEngine eng{opts};
        const QpSolution sol = eng.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 0.0, 1e-7);
        EXPECT_NEAR(sol.x(1), 1.0, 1e-7);
        EXPECT_NEAR(sol.x(2), 1.0, 1e-7);
        EXPECT_NEAR(objective(qp, sol.x), -1.5, 1e-6);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtUpper);
        expect_bound_multiplier_signs(qp, sol, 1e-7);
        EXPECT_GE(smallest_free_eigenvalue(qp, sol), -1e-10);
    }
}

// An UNPINNABLE free variable defeats the repair -- and the engine must then
// refuse to certify, not shrug and stamp kOptimal.
//
// H = diag(-1, -1) with x0 boxed in [-1,1] and x1 unbounded: the repair walks
// the most-negative-diagonal order, finds x1 has no finite bound to pin to,
// and since it is all-or-nothing (the working set can never reach kOk while
// x1 carries negative curvature in the free space) it unwinds completely. The
// loop then converges to the stationary point x = (0,0) -- which is the
// MAXIMIZER of this objective -- with the gate still reporting a TRUSTED
// kWrong. Before the classification branch existed, that came back kOptimal.
//
// Note this is not the "no bounds at all" corner: x0 is boxed exactly as in
// every other fixture here. One unbounded variable among boxed ones is
// enough.
//
// This also pre-delivers Task 7 fixture (c)'s expectation for the no-blocker
// case: an indefinite direction with nothing to ride to reports
// kNumericalError with multipliers cleared. Task 7 will additionally ride the
// direction when a blocker DOES exist.
TEST(QpEngineIndefinite, UnpinnableNegativeCurvatureIsNotCertifiedOptimal) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(2, 2);
    Hd(0, 0) = -1.0;
    Hd(1, 1) = -1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec(2);
    qp.lower << -1.0, -1e20;
    qp.upper = Vec(2);
    qp.upper << 1.0, 1e20;

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kNumericalError);
        EXPECT_NE(sol.status, QpStatus::kOptimal);
        // kNumericalError's existing semantics: the final iterate is reported,
        // the multipliers are cleared.
        EXPECT_EQ(sol.z.cwiseAbs().maxCoeff(), 0.0);
    }
}

// The repair's pins are ordinary bound pins in the working set, so a warm
// re-solve seeded from the repaired answer starts ALREADY second-order
// consistent: the gate returns kOk on the seed working set, no repair runs a
// second time, and the hot-start reuse fast path still applies (zero
// factorizations, zero border work). This is what makes the temporary vertex
// cheap in the SQP driver's warm-solve loop, and it is only true because the
// repair pins at bounds rather than at arbitrary interior values.
TEST(QpEngineIndefinite, RepairedVertexWarmStartsWithoutRepeatingTheRepair) {
    const QpProblem qp = saddle_box_qp();
    QpEngine eng{QpOptions{}}; // border mode (default), same engine instance
    const QpSolution cold = eng.solve(qp);
    ASSERT_EQ(cold.status, QpStatus::kOptimal);
    ASSERT_NEAR(std::abs(cold.x(1)), 1.0, 1e-8);

    const QpSolution warm = eng.solve(qp, cold);
    EXPECT_EQ(warm.status, QpStatus::kOptimal);
    EXPECT_NEAR(warm.x(0), cold.x(0), 1e-10);
    EXPECT_NEAR(warm.x(1), cold.x(1), 1e-10);
    EXPECT_EQ(warm.bound_state[1], cold.bound_state[1]);
    EXPECT_EQ(warm.counters.factorizations, 0);
    EXPECT_EQ(warm.counters.schur_updates, 0);
    EXPECT_EQ(warm.counters.minor_iters, 1);
}

// The gate must be INVISIBLE on convex problems: H PSD makes H + delta*I
// positive definite, so the KKT matrix is quasi-definite and its inertia is
// (#variables, #constraint rows, 0) for every working set the loop can reach --
// kOk always, no repair, no extra factorization.
//
// This is a TRIPWIRE, not the evidence for that claim: it watches one fixture
// in this file so a future change to the gate cannot silently start charging
// convex solves without a local test noticing. The actual evidence is (a) the
// 84 pre-existing tests, several of which assert exact factorization/
// schur_update counts and none of which were touched by this feature, and
// (b) a counter diff of the whole convex battery taken against the
// pre-feature engine (see the Task 6 report).
//
// PHASE 3 TASK 1 NOTE. This fixture's optimum is DEGENERATE: at x = (0, 1) the
// gradient is (-1, -1), the active row prices at lambda_i = 1, and that leaves
// z0 = -1 + 1 = 0 exactly -- x0's lower bound is WEAKLY active. So section 4b's
// new zero-multiplier probe does fire here, and this is the one fixture in the
// suite that PAYS for it, so both modes' costs are pinned below rather than
// just refactorize's: one extra factorization in refactorize mode (3 -> 4), and
// one extra schur_update in border mode (2 -> 3, with the factorization count
// unchanged at 1, since a border-mode probe re-syncs the stack instead of
// re-factorizing). Both measured against the engine with this feature reverted.
// The probe reads kOk -- H + delta*I is positive definite on all of R^n for a
// convex H, so it stays positive definite on the enlarged null space a drop
// produces -- so the ANSWER, the working set and the iteration count are all
// exactly what they were. The claim that the probe does not even RUN when no
// multiplier is near zero is carried by
// ZeroMultiplierProbeIsANoOpOnStrictComplementarity below.
TEST(QpEngineIndefinite, GateIsANoOpOnAConvexFixture) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
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

    QpOptions border_opts;
    border_opts.ws_algebra = WorkingSetLinearAlgebra::kSchurBorder;
    const QpSolution bor = QpEngine{border_opts}.solve(qp);
    EXPECT_EQ(bor.status, QpStatus::kOptimal);
    EXPECT_NEAR(bor.x(0), 0.0, 1e-8);
    EXPECT_NEAR(bor.x(1), 1.0, 1e-8);
    EXPECT_NEAR(bor.z(0), 0.0, 1e-12);         // the weakly active bound (see above)
    EXPECT_EQ(bor.counters.factorizations, 1); // one K0, no repair rebuilds
    // Two borders for the solve itself, plus the one the probe's tentative drop
    // costs -- the whole of what the probe charges in border mode.
    EXPECT_EQ(bor.counters.schur_updates, 3);
    EXPECT_EQ(bor.counters.minor_iters, 3);
    EXPECT_EQ(bor.bound_state[0], BoundState::kAtLower); // restored, not left dropped

    QpOptions refac_opts;
    refac_opts.ws_algebra = WorkingSetLinearAlgebra::kRefactorize;
    const QpSolution ref = QpEngine{refac_opts}.solve(qp);
    EXPECT_EQ(ref.status, QpStatus::kOptimal);
    EXPECT_NEAR(ref.z(0), 0.0, 1e-12);
    // Three major iterations plus the zero-multiplier probe's single tentative
    // drop of that weakly active bound -- which comes back kOk and changes
    // nothing else.
    EXPECT_EQ(ref.counters.factorizations, 4);
    EXPECT_EQ(ref.counters.minor_iters, 3);
    EXPECT_EQ(ref.bound_state[0], BoundState::kAtLower); // restored, not left dropped
}

// --- Section 4c: negative-curvature rides ---------------------------------

// (a) A drop exposes negative curvature and the ride runs to a variable BOUND.
// See ride_to_bound_qp() above for the full hand trace, the pre-fix cycling
// behavior, and why -37.5 is the global (not merely local) minimum.
TEST(QpEngineIndefinite, NegativeCurvatureRideToBound) {
    const QpProblem qp = ride_to_bound_qp();
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), -10.0, 1e-6);
        EXPECT_NEAR(sol.x(1), 5.0, 1e-6);
        EXPECT_NEAR(objective(qp, sol.x), -37.5, 1e-6);

        // The ride's blocker is REPORTED active, and the row the drop rule
        // released stays out.
        EXPECT_EQ(sol.bound_state[0], BoundState::kFree);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtUpper);
        ASSERT_EQ(sol.ineq_active.size(), 1u);
        EXPECT_FALSE(sol.ineq_active[0]);
        EXPECT_NEAR(sol.lambda_i(0), 0.0, 1e-9);

        // z1 <= 0 at an upper bound, and strictly so -- strict
        // complementarity is what makes the reduced-Hessian check below
        // equivalent to second-order necessity.
        expect_bound_multiplier_signs(qp, sol, 1e-7);
        EXPECT_LT(sol.z(1), -1.0);
        EXPECT_NEAR(sol.z(1), -15.0, 1e-6);

        // Second-order necessity, computed in the test: the free subspace is
        // span{e0} and H00 = 1. (The pre-fix stall point x = (-1, 2) left BOTH
        // variables free, where the reduced Hessian is H itself -- indefinite.)
        EXPECT_GE(smallest_reduced_eigenvalue(qp, sol), -1e-10);
    }
}

// (b) A drop exposes negative curvature and the ride runs to a DIFFERENT
// general row. See ride_to_row_qp() above for the full hand trace.
TEST(QpEngineIndefinite, SaddleWithGeneralConstraint) {
    const QpProblem qp = ride_to_row_qp();
    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), -12.0, 1e-6);
        EXPECT_NEAR(sol.x(1), -1.0, 1e-6);
        EXPECT_NEAR(sol.x(2), 6.0, 1e-6);
        EXPECT_NEAR(objective(qp, sol.x), -53.5, 1e-6);

        // Final active set: row 0 was dropped and stays out; row 1 survived the
        // drop rule; ROW 2 is the one the ride ran into. No variable is pinned.
        ASSERT_EQ(sol.ineq_active.size(), 3u);
        EXPECT_FALSE(sol.ineq_active[0]);
        EXPECT_TRUE(sol.ineq_active[1]);
        EXPECT_TRUE(sol.ineq_active[2]);
        for (Index i = 0; i < qp.n(); ++i) {
            EXPECT_EQ(sol.bound_state[static_cast<std::size_t>(i)], BoundState::kFree)
                << "variable " << i;
        }

        // Multipliers: nonnegative on the working rows (strictly, so
        // complementarity is strict), zero on the released row.
        EXPECT_NEAR(sol.lambda_i(0), 0.0, 1e-9);
        EXPECT_NEAR(sol.lambda_i(1), 1.0, 1e-6);
        EXPECT_NEAR(sol.lambda_i(2), 18.0, 1e-6);
        EXPECT_GT(sol.lambda_i(1), 1e-6);
        EXPECT_GT(sol.lambda_i(2), 1e-6);

        // Second-order necessity over the full active set: the critical cone is
        // span{e0} and H00 = 1 > 0. At the pre-fix stall point (-1,-1,2) the
        // active set was {row 1} alone, whose reduced Hessian [[1,2],[2,1]] has
        // eigenvalue -1 -- the check fails there outright.
        EXPECT_GE(smallest_reduced_eigenvalue(qp, sol), -1e-10);
    }
}

// (c) A negative-curvature direction with NOTHING to ride to: H = diag(-1),
// g = (0.1), no bounds and no rows. The QP is unbounded below.
//
// WHICH MECHANISM FIRES. Not the ride: with an empty working set from start to
// finish there is never a drop, so section 4c's curvature check is never armed.
// This is section 4b's SECOND-ORDER CERTIFICATION branch -- the loop converges
// to the stationary point x = 0.1 (the MAXIMIZER) with the gate reporting a
// trusted kWrong, and the classification refuses to stamp kOptimal on it. The
// two mechanisms agree on the verdict (kNumericalError, multipliers cleared)
// and are mutually exclusive by construction: the ride's no-blocker branch
// needs a preceding drop, the classification branch needs a point nothing can
// be dropped from. Delivered by Task 6; asserted here because it is the
// contract Task 7 owes for the no-blocker case.
TEST(QpEngineIndefinite, UnboundedIndefiniteReportsNumericalError) {
    QpProblem qp;
    Eigen::MatrixXd Hd(1, 1);
    Hd(0, 0) = -1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(1, 0.1);
    qp.Ae.resize(0, 1);
    qp.be = Vec(0);
    qp.Ai.resize(0, 1);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(1, -1e20);
    qp.upper = Vec::Constant(1, 1e20);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kNumericalError);
        EXPECT_EQ(sol.z.cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(sol.lambda_e.size(), 0);
        EXPECT_EQ(sol.lambda_i.size(), 0);
    }
}

// The brief's diagonal fixture: H = diag(-1, 1), g = (0.1, 0) on [-2, 2]^2,
// whose minimizer is x = (-2, 0) at objective -2.2.
//
// HONESTY NOTE: this fixture does NOT exercise the ride, and measurement says
// so. The start point (0,0) is interior with an EMPTY working set, so the gate
// comes back kWrong at iteration 0 and section 4b's temporary-vertex repair --
// which is exactly where "move off an interior saddle onto a bound" already
// lives -- pins x0 at its lower bound and the loop finishes from there. No drop
// ever happens, so section 4c's curvature check is never armed. Verified
// against the pre-Task-7 engine: identical answer, identical counters.
// It is kept as the brief named it, as a regression guard on the interaction
// between the two sections.
TEST(QpEngineIndefinite, DiagonalIndefiniteBoxReachesTheNegativeCurvatureBound) {
    const QpProblem qp = diagonal_indefinite_box_qp();

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), -2.0, 1e-8);
        EXPECT_NEAR(sol.x(1), 0.0, 1e-8);
        EXPECT_NEAR(objective(qp, sol.x), -2.2, 1e-8);
        EXPECT_EQ(sol.bound_state[0], BoundState::kAtLower);
        EXPECT_EQ(sol.bound_state[1], BoundState::kFree);
        expect_bound_multiplier_signs(qp, sol, 1e-8);
        EXPECT_NEAR(sol.z(0), 2.1, 1e-7); // grad_0 = -1*(-2) + 0.1
        EXPECT_GE(smallest_free_eigenvalue(qp, sol), -1e-10);
    }
}

// STRICTLY CONVEX half of section 4c's convex story: with H positive definite
// the ride branch is UNREACHABLE and the engine is bit-for-bit unchanged.
// (The PSD-singular half, where the branch IS reachable and behavior does
// change, is covered by RideOnAZeroHessianLpFindsTheVertex and
// RideOnAWideBoxLpReachesTheTrueMinimizer below -- this test says nothing
// about it.)
//
// The curvature check is armed only on the iteration immediately after a drop,
// and this fixture is chosen precisely because it DOES drop: the seed working
// set (both rows violated at x = 0, so both enter via the homotopy) is not the
// optimal one, row 1 prices negative and is released, and the loop finishes on
// row 0 alone. The post-drop curvature is p'Hp/p'p >= lambda_min(H) > 0, so
// the ride branch is not entered and the drop is followed by an ORDINARY EQP
// step.
//
// Exact counters in both modes, so a future change that starts riding (or
// merely re-solving) on the strictly convex path cannot land silently.
TEST(QpEngineIndefinite, RideIsANoOpOnAConvexDrop) {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Zero(2);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 0, 1, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -1.0, -3.0;
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), -1.5, 1e-8);
        EXPECT_NEAR(sol.x(1), -1.5, 1e-8);
        // Row 0 is the one the drop rule released (lambda_0 = -1 at the seed
        // point (-1,-2)); row 1 survives. If the drop had NOT happened the
        // answer would be (-1, -2) at objective 2.5 rather than 2.25.
        EXPECT_FALSE(sol.ineq_active[0]);
        EXPECT_TRUE(sol.ineq_active[1]);
        EXPECT_NEAR(objective(qp, sol.x), 2.25, 1e-8);
        // Four major iterations: seed EQP + full step; the KKT pass that drops
        // row 0; the post-drop EQP + full step; the final KKT/classification
        // pass. The third is the one whose curvature check is armed; p =
        // (-0.5, 0.5) has p'Hp/p'p = 1 > 0, so the ORDINARY step is taken and
        // the ride branch is not entered.
        EXPECT_EQ(sol.counters.minor_iters, 4);
    }
}

// The RIDE's own no-blocker branch -- the other half of the contract fixture
// (c) states, and the one fixture (c) itself does not reach.
//
// Same H, row and objective as ride_to_bound_qp(), but with the box removed.
// The walk is identical through iteration 2: the seed working set {row 0} is
// second-order consistent, row 0 prices at -3 and is released, and the
// post-drop step p = (1, -2) has p'Hp/p'p = -0.6 < 0. This time the ratio test
// along dir = (-1, 2) finds NOTHING: row 0 recedes and there is no bound to
// reach, so the QP really is unbounded below along the ride.
//
// WHICH MECHANISM FIRES, and the ordering. Here it is the ride, from INSIDE
// the loop, on the very iteration the curvature was measured -- the engine
// never reaches a classifiable point at all (3 minor iterations, not
// max_iter). Section 4b's certification branch cannot fire on this problem
// because it is only consulted where nothing may be dropped, and here
// something was. The two are mutually exclusive by construction and agree on
// the verdict: kNumericalError, final iterate reported, multipliers cleared.
TEST(QpEngineIndefinite, RideWithNoBlockerReportsNumericalError) {
    QpProblem qp = ride_to_bound_qp();
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kNumericalError);
        // Reported PROMPTLY, from inside the loop -- not by running out of
        // iterations (which is what the pre-ride engine did here).
        EXPECT_EQ(sol.counters.minor_iters, 3);
        // MARKED CORRECTION, PHASE-6 TASK 4 (M6): the comparison is now
        // against the EFFECTIVE cap rather than against opts.max_iter, whose
        // default is the size-derived SENTINEL (0) and not a cap at all. The
        // assertion's intent -- "this exit came from inside the loop, not from
        // running out of iterations" -- is unchanged, and so is the observed 3.
        EXPECT_LT(sol.counters.minor_iters, detail::effective_qp_max_iter(qp, opts.max_iter));
        // kNumericalError semantics: final iterate, multipliers cleared.
        EXPECT_NEAR(sol.x(0), -1.0, 1e-8);
        EXPECT_NEAR(sol.x(1), 2.0, 1e-8);
        EXPECT_EQ(sol.z.cwiseAbs().maxCoeff(), 0.0);
        EXPECT_EQ(sol.lambda_i.cwiseAbs().maxCoeff(), 0.0);
    }
}

// A ride direction that does NOT lie in the null space of the working
// constraints must be declined, not ridden. Regression guard for
// ride_stays_in_working_set (section 4c), against a case found by a
// randomized indefinite battery rather than constructed.
//
//     H = [[-3,-3,-1], [-3,2,0], [-1,0,0]],  g = (-2,-2,-1)
//     row 0: -2x0 + 2x1 + 3x2 <=  2
//     row 1:  2x0 - 3x1 - 3x2 <= -2
//     x0 unbounded, x1, x2 in [-2, 2].
//
// The two rows bracket x0 from BOTH sides for every (x1, x2) -- row 0 gives
// x0 >= (2x1+3x2-2)/2 and row 1 gives x0 <= (3x1+3x2-2)/2 -- so the feasible
// set is compact and the QP is bounded below however indefinite H is. The
// answer is x = (5, 2, 2) at objective -89.5, a STRICT local minimizer: row 1,
// x1's upper bound and x2's upper bound are active with multipliers
// (12.5, -50.5, -43.5), all strictly signed, and their gradients span R^3.
//
// The engine reaches a working set of {row 1, x1@upper, x2@upper} at
// x = (4.5, 2, 2), where row 1 is NOT tight (2*4.5 - 12 = -3, slack 1): four
// active constraints in three dimensions, so the regularized solve satisfies
// them only in a least-squares sense. Row 0 then prices at a
// regularization-artifact -2e8 and is dropped, and the post-drop step
// p = (0.5, 0, 0) has curvature H00 = -3 < 0 -- but a0 . p = 1 != 0, i.e. p
// walks straight THROUGH working row 1, which the ratio test does not watch
// because it is in the working set. Riding it reported this bounded QP as
// kNumericalError ("unbounded"). The null-space precondition declines it and
// the ordinary capped step -- which stops at alpha = 1, exactly on row 1 --
// finishes the solve.
TEST(QpEngineIndefinite, RideDeclinesADirectionThatLeavesTheWorkingSet) {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    Hd << -3, -3, -1, -3, 2, 0, -1, 0, 0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << -2, -2, -1;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 3);
    Aid << -2, 2, 3, 2, -3, -3;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 2.0, -2.0;
    qp.lower = Vec(3);
    qp.lower << -1e20, -2.0, -2.0;
    qp.upper = Vec(3);
    qp.upper << 1e20, 2.0, 2.0;

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NE(sol.status, QpStatus::kNumericalError); // the pre-guard answer
        EXPECT_NEAR(sol.x(0), 5.0, 1e-6);
        EXPECT_NEAR(sol.x(1), 2.0, 1e-6);
        EXPECT_NEAR(sol.x(2), 2.0, 1e-6);
        EXPECT_NEAR(objective(qp, sol.x), -89.5, 1e-6);

        EXPECT_FALSE(sol.ineq_active[0]);
        EXPECT_TRUE(sol.ineq_active[1]);
        EXPECT_NEAR(sol.lambda_i(1), 12.5, 1e-6);
        EXPECT_EQ(sol.bound_state[0], BoundState::kFree);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtUpper);
        EXPECT_EQ(sol.bound_state[2], BoundState::kAtUpper);
        expect_bound_multiplier_signs(qp, sol, 1e-7);
        EXPECT_GE(smallest_reduced_eigenvalue(qp, sol), -1e-10);
    }
}

// --- Section 4c on CONVEX problems: the PSD-singular half ------------------
//
// RideIsANoOpOnAConvexDrop above covers the strictly convex case, where the
// ride branch is unreachable. These two cover the case where it IS reachable:
// an exactly-PSD H whose curvature along the post-drop direction is zero.

// A FLAT direction must DECLINE, not ride. Regression guard for the
// strict-descent requirement in ride_sign (fix round 1).
//
// Exactly-PSD rank-1 H = m m' with m = (1, 1, -2, -3, 0) (eigenvalues
// 0,0,0,0,15), g = (-1, 1, -2, -3, 0), two general rows, box +/-10. Convex, so
// any KKT point is a global minimum.
//
// The optimum is -20.5 and that is PROVABLE rather than merely observed. With
// s = m'x, and writing g = m + d where d = (-2, 0, 0, 0, 0):
//     f(x) = g'x + 0.5 (m'x)^2 = s + 0.5 s^2 - 2 x0
//          >= min_s (s + 0.5 s^2)  -  2 * max(x0)
//          =  -0.5 - 20  =  -20.5,
// attained (the engine's own answer has s = -1 exactly and x0 = 10). Note H's
// rank deficiency leaves a large flat optimal FACE, so the two ws_algebra
// modes legitimately report different x with the same objective; only the
// objective and the status are asserted.
//
// PRE-FIX BEHAVIOR (measured, border mode, the ride's first revision): the
// post-drop direction was flat (slope within slope_tol), the sign was then
// taken from the freed constraint alone, and the resulting ride bought no
// objective decrease at all while pinning row 1 into the working set. The next
// EQP left a ~1e-6 residual on that row, which step 5's classifier read as a
// STRUCTURAL violation: kInfeasible, on a feasible problem whose optimum the
// pre-ride engine returned correctly. Requiring strict descent declines the
// direction and restores the pre-ride answer exactly.
TEST(QpEngineIndefinite, RideDeclinesAFlatDirectionOnAPsdSingularQp) {
    QpProblem qp;
    Eigen::MatrixXd Hd(5, 5);
    Hd << 1, 1, -2, -3, 0, 1, 1, -2, -3, 0, -2, -2, 4, 6, 0, -3, -3, 6, 9, 0, 0, 0, 0, 0, 0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(5);
    qp.g << -1, 1, -2, -3, 0;
    qp.Ae.resize(0, 5);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 5);
    Aid << -1, 3, 3, 0, -1, -1, -1, -3, -2, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -3.0, 2.0;
    qp.lower = Vec::Constant(5, -10.0);
    qp.upper = Vec::Constant(5, 10.0);

    // The fixture is exactly PSD (this is a convex problem, not an indefinite
    // one) -- if a future edit perturbs H the test must not quietly become a
    // different test.
    {
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hd);
        ASSERT_GE(es.eigenvalues().minCoeff(), -1e-12);
        ASSERT_LE(es.eigenvalues()(3), 1e-12); // rank 1: only the top eigenvalue is nonzero
        ASSERT_NEAR(es.eigenvalues()(4), 15.0, 1e-9);
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NE(sol.status, QpStatus::kInfeasible); // the pre-fix border answer
        EXPECT_NEAR(objective(qp, sol.x), -20.5, 1e-6);

        // ... and the answer is actually feasible, which is what makes the
        // pre-fix kInfeasible a wrong verdict rather than a hard problem.
        const Vec resid = Eigen::MatrixXd(qp.Ai) * sol.x - qp.bi;
        EXPECT_LE(resid.maxCoeff(), 1e-7);
        EXPECT_LE(sol.x.maxCoeff(), 10.0 + 1e-9);
        EXPECT_GE(sol.x.minCoeff(), -10.0 - 1e-9);
    }
}

// The ride branch RUNNING on a convex problem, and getting the right answer.
// H == 0 exactly (a pure LP), so the post-drop curvature is exactly zero and
// the branch is entered -- this is the live-path counterpart to
// RideIsANoOpOnAConvexDrop, which only guards the unreachable half.
//
//     min  -3*x0 + 2*x1   s.t.  -2*x0 + 2*x1 <= -1,   x in [-10, 10]^2
//
// Both objective coefficients pull to a box corner: x0 up to 10, x1 down to
// -10. The row is slack there (-20 - 20 = -40 <= -1), so x = (10, -10) at
// objective -50 is the unconstrained-over-the-box minimum and therefore the
// global one. Measured identical to the pre-ride engine (statuses, x,
// multipliers, active set), which is the expected common case: the
// regularized EQP step for H == 0 sits ~1e8 along the ride's own ray, so the
// box blocks long before the cap would have mattered.
TEST(QpEngineIndefinite, RideOnAZeroHessianLpFindsTheVertex) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Zero(2, 2).sparseView();
    qp.g = Vec(2);
    qp.g << -3.0, 2.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << -2, 2;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(2, -10.0);
    qp.upper = Vec::Constant(2, 10.0);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NEAR(sol.x(0), 10.0, 1e-8);
        EXPECT_NEAR(sol.x(1), -10.0, 1e-8);
        EXPECT_NEAR(objective(qp, sol.x), -50.0, 1e-8);
        EXPECT_EQ(sol.bound_state[0], BoundState::kAtUpper);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtLower);
        EXPECT_FALSE(sol.ineq_active[0]);
        expect_bound_multiplier_signs(qp, sol, 1e-8);
        // Pinned so a DEAD curvature gate cannot pass this test silently: the
        // right answer is reachable by more than one route, and only the walk
        // length distinguishes "the ride ran" from "the ride was skipped".
        EXPECT_EQ(sol.counters.minor_iters, 5);
    }
}

// Where the ride's UNCAPPED ratio makes a convex answer BETTER, documented as
// intended behavior rather than tolerated drift.
//
//     min  x0 + 3*x1   s.t.  -2*x0 + 2*x1 <= -3,   -x0 <= 2,   x in [-1e9, 1e9]^2
//
// Optimum by inspection: both coefficients are positive, so x1 goes to its
// lower bound -1e9 and x0 to the tightest of its two lower limits, max(-2,
// x1 + 1.5) = -2. So x = (-2, -1e9) at objective -3000000002.
//
// The box is deliberately WIDER than the regularized EQP step
// (~|g|/primal_delta ~ 1e8), which is exactly the regime where the ordinary
// capped path and the ride part company: the capped step cannot cross the box
// in one go, walks out to the artifact scale and trips the unbounded-artifact
// guard. Measured on the pre-ride engine: kNumericalError at objective
// -1800000002 in BOTH modes. The ride takes the raw ratio and lands on the
// blocking bound directly.
//
// This is one of 47 such blocks in a 1600-block randomized H == 0 battery at
// this box size (see section 4c's convex note); no kOptimal was lost or
// worsened anywhere in that battery.
TEST(QpEngineIndefinite, RideOnAWideBoxLpReachesTheTrueMinimizer) {
    QpProblem qp;
    qp.H = Eigen::MatrixXd::Zero(2, 2).sparseView();
    qp.g = Vec(2);
    qp.g << 1.0, 3.0;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -2, 2, -1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -3.0, 2.0;
    qp.lower = Vec::Constant(2, -1e9);
    qp.upper = Vec::Constant(2, 1e9);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_NE(sol.status, QpStatus::kNumericalError); // the pre-ride answer
        EXPECT_NEAR(sol.x(0), -2.0, 1e-3);
        EXPECT_NEAR(sol.x(1), -1e9, 1.0);
        EXPECT_NEAR(objective(qp, sol.x), -3000000002.0, 10.0);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtLower);
        // As above: pin the walk length, not just the destination.
        EXPECT_EQ(sol.counters.minor_iters, 5);
    }
}

// === Task 8: the local-minimizer oracle ===================================
//
// tests/sqp/support/dense_oracle.h's enumerate_local_minimizers() runs the SAME
// exhaustive active-set enumeration solve_dense_oracle() does, but instead of
// returning the single lowest-objective KKT point it returns EVERY enumerated
// KKT point whose reduced Hessian (H projected onto the null space of the
// active constraint gradients) is PSD -- i.e. every second-order-necessary
// point, which for these fixtures is exactly the set of local minimizers.
//
// The three tests below check it against fixtures whose minimizer sets are
// derived by hand in the comments, BEFORE it is used as the reference for the
// randomized battery further down. An oracle is only worth as much as its own
// tests (see the Task 5 me-offset bug, which a hand-derived oracle self-test is
// what caught).

// saddle_box_qp: min 1/2(x0^2 - x1^2) over [-1,1]^2.
//
// HAND DERIVATION. The objective is separable. x0's term 1/2 x0^2 is convex
// with its minimum at the interior point x0 = 0, so every local minimizer has
// x0 = 0 (any x0 != 0 admits a strictly decreasing feasible move toward 0).
// x1's term -1/2 x1^2 is concave on [-1, 1], so it is minimized at the two
// endpoints and BOTH are local minimizers (from x1 = 1 the only feasible moves
// are x1 = 1 - t, which raise the objective by t - t^2/2 > 0 for small t; the
// same at x1 = -1 by symmetry).
//
// So the minimizer set is exactly {(0, 1), (0, -1)}, both at objective -0.5 --
// which is also what IndefiniteStartRepairsToVertex asserts the engine reaches
// one of.
TEST(QpEngineIndefinite, OracleEnumeratesTheSaddleBoxMinimizers) {
    const QpProblem qp = saddle_box_qp();
    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);

    ASSERT_EQ(mins.size(), 2u);
    EXPECT_TRUE(contains_point(mins, vec2(0.0, 1.0), 1e-8));
    EXPECT_TRUE(contains_point(mins, vec2(0.0, -1.0), 1e-8));
    for (const auto &m : mins) {
        EXPECT_EQ(m.status, QpStatus::kOptimal);
        EXPECT_NEAR(objective(qp, m.x), -0.5, 1e-10);
        // The negative-curvature variable is pinned and the other is free.
        EXPECT_EQ(m.bound_state[0], BoundState::kFree);
        EXPECT_NE(m.bound_state[1], BoundState::kFree);
        // |z1| = 1 with the sign the bound side demands.
        EXPECT_NEAR(std::abs(m.z(1)), 1.0, 1e-10);
        expect_bound_multiplier_signs(qp, m, 1e-10);
    }
}

// ORACLE-QUALITY GUARD (mutation resistance for the Z' H Z check).
//
// The origin of saddle_box_qp is a KKT point with an EMPTY active set: the
// gradient Hx is zero there, every bound is slack, and there is no multiplier
// to get the sign of wrong. So the shared enumeration core accepts it, and the
// ONLY thing that keeps it out of enumerate_local_minimizers' output is the
// reduced-Hessian test -- over the empty active set that reduced Hessian is H
// itself, diag(1, -1), whose smallest eigenvalue is -1.
//
// This test therefore asserts both halves, which is what makes it a mutation
// test rather than a coincidence: the origin IS produced by the enumeration
// core (so a stray filter elsewhere cannot be what excludes it), and it is NOT
// in the minimizer list. Deleting the eigenvalue check -- or flipping its
// comparison -- fails the second half; a bug that dropped the origin from the
// enumeration entirely fails the first.
TEST(QpEngineIndefinite, OracleExcludesTheSaddleFromTheMinimizerSet) {
    const QpProblem qp = saddle_box_qp();

    std::vector<Vec> kkt_points;
    detail::enumerate_kkt_candidates(
        qp, [&](const detail::OracleCandidate &cand) { kkt_points.push_back(cand.solution.x); });
    const bool saddle_is_enumerated =
        std::any_of(kkt_points.begin(), kkt_points.end(),
                    [](const Vec &x) { return x.lpNorm<Eigen::Infinity>() <= 1e-10; });
    ASSERT_TRUE(saddle_is_enumerated) << "the enumeration core must produce the origin as a KKT "
                                         "point for this test to mean anything";

    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);
    EXPECT_FALSE(contains_point(mins, Vec::Zero(2), 1e-8));
}

// diagonal_indefinite_box_qp: min 0.1*x0 - 1/2 x0^2 + 1/2 x1^2 over [-2,2]^2.
//
// HAND DERIVATION, and the point of this fixture is that BOTH x0 = +/-2
// candidates must be checked rather than only the global one. Separable again:
// x1 = 0 at every local minimizer (convex term, interior minimum). x0's term
// 0.1*x0 - 1/2 x0^2 is CONCAVE, so on [-2, 2] both endpoints are local minima
// and the interior stationary point x0 = 0.1 is the MAXIMIZER.
//
//   x0 = -2: objective -0.2 - 2 = -2.2, the global minimum. Stationarity gives
//            z0 = grad_0 = -x0 + 0.1 = 2.1 > 0, correct at a LOWER bound.
//   x0 = +2: objective  0.2 - 2 = -1.8. z0 = -2 + 0.1 = -1.9 < 0, correct at an
//            UPPER bound -- so it is a genuine KKT point, not merely a corner.
//   Both have reduced Hessian [H11] = [1] > 0 over the one free direction.
//
// Set: exactly {(-2, 0), (2, 0)}. The interior stationary point (0.1, 0) is a
// KKT point of the empty active set and must be excluded (reduced Hessian
// diag(-1, 1)).
TEST(QpEngineIndefinite, OracleEnumeratesBothDiagonalBoxMinimizers) {
    const QpProblem qp = diagonal_indefinite_box_qp();
    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);

    ASSERT_EQ(mins.size(), 2u);
    const Index lo = index_of_point(mins, vec2(-2.0, 0.0), 1e-8);
    const Index hi = index_of_point(mins, vec2(2.0, 0.0), 1e-8);
    ASSERT_GE(lo, 0);
    ASSERT_GE(hi, 0);
    const QpSolution &at_lower = mins[static_cast<std::size_t>(lo)];
    const QpSolution &at_upper = mins[static_cast<std::size_t>(hi)];

    EXPECT_EQ(at_lower.bound_state[0], BoundState::kAtLower);
    EXPECT_EQ(at_lower.bound_state[1], BoundState::kFree);
    EXPECT_NEAR(at_lower.z(0), 2.1, 1e-10);
    EXPECT_NEAR(objective(qp, at_lower.x), -2.2, 1e-10);

    EXPECT_EQ(at_upper.bound_state[0], BoundState::kAtUpper);
    EXPECT_EQ(at_upper.bound_state[1], BoundState::kFree);
    EXPECT_NEAR(at_upper.z(0), -1.9, 1e-10);
    EXPECT_NEAR(objective(qp, at_upper.x), -1.8, 1e-10);

    // The interior stationary point is the maximizer, not a minimizer.
    EXPECT_FALSE(contains_point(mins, vec2(0.1, 0.0), 1e-8));
}

// ride_to_row_qp (Task 7 fixture (b)): n = 3,
//   H = [[1,0,2],[0,1,0],[2,0,1]],  g = 0,
//   rows x0 <= -1, x1 <= -1, x2 <= 6, box [-20,20]^3.
//
// HAND DERIVATION. x1 separates: its only objective term is 1/2 x1^2 and its
// only constraints are x1 <= -1 and x1 in [-20,20], so x1 = -1 at every local
// minimizer (the term is convex and increasing away from 0, and x1 = -1 is the
// closest feasible point to 0; x1 at a box bound is not stationary --
// z1 = grad_1 = x1 = -20 < 0 at a lower bound is the wrong sign).
//
// What remains is q(x0, x2) = 1/2 x0^2 + 1/2 x2^2 + 2 x0 x2 over the RECTANGLE
// x0 in [-20, -1], x2 in [-20, 6]. The form [[1,2],[2,1]] has eigenvalues 3 and
// -1, so there is no interior minimum (its only stationary point is the saddle
// at the origin, which is infeasible anyway). Every candidate is on an edge or
// a corner, and all of them are checked:
//
//   x0 = -1 edge: min over x2 at x2 = 2, q = -1.5. But the row-0 multiplier is
//                 -(x0 + 2x2) = -3 < 0 -- NOT a KKT point. (This is exactly the
//                 point the engine reaches at iteration 1 and drops row 0 from.)
//   x2 =  6 edge: min over x0 at x0 = -12 (interior to [-20,-1]), q = -54, and
//                 the row-2 multiplier is -(x2 + 2x0) = 18 > 0. KKT, and the
//                 reduced Hessian over the remaining free direction span{e0} is
//                 [1] > 0. LOCAL MINIMIZER.
//   x0 = -20 edge: q is strictly decreasing in x2 there (dq/dx2 = x2 - 40 < 0),
//                 so its minimum is the corner (-20, 6).
//   x2 = -20 edge: q is strictly decreasing in x0 (dq/dx0 = x0 - 40 < 0), so
//                 its minimum is the corner (-1, -20).
//   corners: (-1,-20) has z2 = grad_2 = -22 < 0 at a LOWER bound (wrong sign);
//            (-20, 6) has z0 = grad_0 = -8 < 0 at a LOWER bound (wrong sign);
//            (-20,-20) has both gradient components -60 < 0 (wrong sign);
//            (-1, 6) prices row 0 at -11 < 0 (wrong sign).
//            None is a KKT point.
//
// So the set is the SINGLE point x = (-12, -1, 6) at objective 1/2 - 54 =
// -53.5 -- the answer SaddleWithGeneralConstraint asserts the engine reaches,
// now shown to be the ONLY local minimizer rather than merely a good one.
TEST(QpEngineIndefinite, OracleEnumeratesTheSoleRideToRowMinimizer) {
    const QpProblem qp = ride_to_row_qp();
    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);

    ASSERT_EQ(mins.size(), 1u);
    EXPECT_LT((mins[0].x - vec3(-12.0, -1.0, 6.0)).lpNorm<Eigen::Infinity>(), 1e-8);
    EXPECT_NEAR(objective(qp, mins[0].x), -53.5, 1e-9);
    EXPECT_FALSE(mins[0].ineq_active[0]);
    EXPECT_TRUE(mins[0].ineq_active[1]);
    EXPECT_TRUE(mins[0].ineq_active[2]);
    EXPECT_NEAR(mins[0].lambda_i(1), 1.0, 1e-9);
    EXPECT_NEAR(mins[0].lambda_i(2), 18.0, 1e-9);

    // ... and the single global optimum the convex-path oracle reports agrees
    // with it, which it must when the local-minimizer set is a singleton.
    const QpSolution best = solve_dense_oracle(qp);
    EXPECT_LT((best.x - mins[0].x).lpNorm<Eigen::Infinity>(), 1e-8);
}

// === Task 8 (b): the engine lands on a local minimizer ====================

namespace {

// --- Four more hand-built indefinite fixtures for the battery -------------
// (the other four it uses -- saddle_box_qp, diagonal_indefinite_box_qp,
// ride_to_bound_qp and ride_to_row_qp -- are defined at the top of this file
// and already carry their own hand traces.)

// NEGATIVE-DIAGONAL BOXED, all three curvatures negative:
//   min -1/2(x0^2 + 2 x1^2 + 3 x2^2) + 0.5 x0 - 0.25 x1   over [-1, 1]^3.
//
// Separable and CONCAVE in every coordinate, so no local minimizer can leave a
// coordinate free and every candidate is a box corner. A corner is a local
// minimizer iff its multipliers are correctly signed, and here BOTH endpoints
// of every coordinate qualify: z_i = grad_i = -c_i x_i + g_i with c =
// (1, 2, 3), so at x_i = -1 it is c_i + g_i and at x_i = +1 it is -c_i + g_i,
// which have the signs the two bound sides demand exactly because
// |g_i| < c_i for all three (0.5 < 1, 0.25 < 2, 0 < 3). Concavity alone would
// NOT be enough -- a large enough linear term disqualifies one endpoint -- so
// this fixture is built with the margin. The minimizer set is therefore ALL
// EIGHT CORNERS, with z = (1.5, 1.75, 3) at the all-lower one and
// (-0.5, -2.25, -3) at the all-upper one. Every variable is pinned at each
// corner, so the active gradients already span R^3 and the second-order test is
// vacuous (reduced_curvature short-circuits to +infinity without forming a
// reduced Hessian at all).
//
// The GLOBAL minimum is -3.75 at x0 = -1, x1 = +1 (the linear term's best
// corner; every corner contributes the same -3 from the quadratic part) with
// x2 = +/-1 either way -- so this fixture also has a two-point global optimum,
// which is exactly why solve_dense_oracle cannot be the reference here.
QpProblem negative_diagonal_box_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = -1.0;
    Hd(1, 1) = -2.0;
    Hd(2, 2) = -3.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 0.5, -0.25, 0.0;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1.0);
    qp.upper = Vec::Constant(3, 1.0);
    return qp;
}

// INDEFINITE WITH AN EQUALITY ROW:
//   min 1/2(x0^2 - x1^2 - 2 x2^2) + 0.5 x0 - 0.5 x2
//   s.t. x0 + x1 + x2 = 1,  x in [-2, 2]^3.
//
// On the equality's null space (spanned by (1,-1,0) and (1,0,-1)) the form is
// 2 b c - c^2 in the coordinates d = (-(b+c), b, c) -- indefinite -- so no
// local minimizer can leave two directions free, and by the same arithmetic no
// SINGLE free direction survives either except the flat one, which turns out
// not to be attained. Two variables must therefore be pinned, and of the four
// (x1, x2) corner pairs only two put x0 = 1 - x1 - x2 back inside the box:
//
//   x = (1, -2,  2)  objective -6   (x1 at lower, x2 at upper), lambda_e = -1.5
//   x = (1,  2, -2)  objective -4   (x1 at upper, x2 at lower), lambda_e = -1.5
//
// Every (x0, x1) and (x0, x2) pinned pair that stays in the box fails its
// multiplier signs (e.g. (-2, 2, 1) needs z1 = 0.5 at an UPPER bound), and so
// does every single-pin and no-pin candidate. Set: exactly those two.
QpProblem indefinite_equality_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 0) = 1.0;
    Hd(1, 1) = -1.0;
    Hd(2, 2) = -2.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 0.5, 0.0, -0.5;
    Eigen::MatrixXd Aed(1, 3);
    Aed << 1, 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1.0);
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -2.0);
    qp.upper = Vec::Constant(3, 2.0);
    return qp;
}

// INDEFINITE WITH AN EQUALITY *AND* AN ACTIVE GENERAL ROW:
//   min x0*x1 + 1/2 x2^2 - x2
//   s.t. x0 + x2 = 0.5,  x0 + x1 <= -1,  x in [-2, 2]^3.
//
// H = [[0,1,0],[1,0,0],[0,0,1]] has eigenvalues (-1, 1, 1): the x0*x1 term is
// the canonical saddle. The general row is ACTIVE at BOTH minimizers, which is
// what this fixture is for.
//
//   x = (-1.5, 0.5,  2)   objective -0.75   x2 at UPPER, row 0 active
//        grad = (0.5, -1.5, 1); x1 free gives lambda_i = 1.5 >= 0, then
//        lambda_e = -2 and z2 = 1 + lambda_e = -1 <= 0 at an upper bound.
//   x = ( 1,  -2, -0.5)   objective -1.375  x1 at LOWER, row 0 active
//        grad = (-2, 1, -1.5); x2 free gives lambda_e = 1.5, then
//        lambda_i = 0.5 >= 0 and z1 = 1 + lambda_i = 1.5 >= 0 at a lower bound.
//
// Both have three independent active gradients in R^3, so their null spaces are
// trivial; both also have STRICT complementarity, so the critical cone really
// is {0} and each is a strict local minimizer. Set: exactly those two.
QpProblem indefinite_equality_and_row_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(3, 3);
    Hd(0, 1) = 1.0;
    Hd(2, 2) = 1.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 0.0, 0.0, -1.0;
    Eigen::MatrixXd Aed(1, 3);
    Aed << 1, 0, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 0.5);
    Eigen::MatrixXd Aid(1, 3);
    Aid << 1, 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, -1.0);
    qp.lower = Vec::Constant(3, -2.0);
    qp.upper = Vec::Constant(3, 2.0);
    return qp;
}

// TWO NEGATIVE EIGENVALUES plus an active general row:
//   min 1/2(-2 x0^2 + 2 x0 x1 - 2 x1^2 + x2^2) + 0.3 x0 - 0.2 x1 + 0.1 x2
//   s.t. -x0 + x1 <= 1,  x in [-2, 2]^3.
//
// H = [[-2,1,0],[1,-2,0],[0,0,1]] has eigenvalues (-3, -1, 1) -- TWO negative,
// unlike every other fixture in this file. x2 decouples (1/2 x2^2 + 0.1 x2,
// minimized at the interior point x2 = -0.1 everywhere), and the (x0, x1) block
// is concave, so every local minimizer is a VERTEX of the feasible pentagon
// {x0, x1 in [-2,2], x1 - x0 <= 1}. That pentagon has five vertices, and the
// enumeration finds all five correctly signed (a vertex of a concave problem is
// a local minimizer only when its multipliers work out, so this is a property
// of these numbers, not of concavity). The row is active at two of them:
//
//   (-2, -1, -0.1) obj -3.405   x0 at lower, ROW ACTIVE (lambda_i = 0.2)
//   ( 1,  2, -0.1) obj -3.105   x1 at upper, ROW ACTIVE (lambda_i = 0.3)
//   (-2, -2, -0.1) obj -4.205   both at lower, row slack
//   ( 2, -2, -0.1) obj -11.005  the global minimum, row slack
//   ( 2,  2, -0.1) obj -3.805   both at upper, row slack
//
// MEASURED (not asserted -- the battery deliberately requires only that the
// answer be SOME local minimizer): the engine walks to (-2, -1, -0.1) in both
// modes, a LOCAL and NOT global minimizer with the general row active. That is
// precisely the outcome this battery exists to accept and that
// solve_dense_oracle would reject, which is why the fixture is here.
QpProblem two_negative_eigenvalue_row_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    Hd << -2, 1, 0, 1, -2, 0, 0, 0, 1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 0.3, -0.2, 0.1;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 3);
    Aid << -1, 1, 0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec::Constant(1, 1.0);
    qp.lower = Vec::Constant(3, -2.0);
    qp.upper = Vec::Constant(3, 2.0);
    return qp;
}

// Worst primal violation of `x` against the FULL problem.
double primal_violation(const QpProblem &qp, const Vec &x) {
    double worst = 0.0;
    if (qp.me() > 0) {
        worst = std::max(worst, (qp.Ae * x - qp.be).lpNorm<Eigen::Infinity>());
    }
    if (qp.mi() > 0) {
        worst = std::max(worst, (qp.Ai * x - qp.bi).maxCoeff());
    }
    for (Index i = 0; i < qp.n(); ++i) {
        worst = std::max(worst, qp.lower(i) - x(i));
        worst = std::max(worst, x(i) - qp.upper(i));
    }
    return worst;
}

// MULTIPLIER CONSISTENCY at the engine's own answer: stationarity, sign
// conventions and complementarity, all checked against qp_problem.h's
// convention grad(f) + Ae^T lambda_e + Ai^T lambda_i - z = 0.
//
// This is checked instead of comparing the multipliers to the matched
// enumerated point's, because at a degenerate minimizer (dependent active
// gradients) the per-constraint split is not unique and the two would
// legitimately disagree. What must hold either way is that the reported
// multipliers certify the reported point.
void expect_multipliers_consistent(const QpProblem &qp, const QpSolution &sol, double tol) {
    Vec resid = qp.H.selfadjointView<Eigen::Upper>() * sol.x + qp.g - sol.z;
    if (qp.me() > 0) {
        resid += Eigen::MatrixXd(qp.Ae).transpose() * sol.lambda_e;
    }
    if (qp.mi() > 0) {
        resid += Eigen::MatrixXd(qp.Ai).transpose() * sol.lambda_i;
    }
    EXPECT_LT(resid.lpNorm<Eigen::Infinity>(), tol) << "stationarity residual";

    for (Index j = 0; j < qp.mi(); ++j) {
        if (sol.ineq_active[static_cast<std::size_t>(j)]) {
            EXPECT_GE(sol.lambda_i(j), -tol) << "row " << j;
        } else {
            EXPECT_NEAR(sol.lambda_i(j), 0.0, tol) << "inactive row " << j;
        }
    }
    expect_bound_multiplier_signs(qp, sol, tol);
    for (Index i = 0; i < qp.n(); ++i) {
        if (sol.bound_state[static_cast<std::size_t>(i)] == BoundState::kFree) {
            EXPECT_NEAR(sol.z(i), 0.0, tol) << "free variable " << i;
        }
    }
}

struct IndefiniteCase {
    std::string name;
    QpProblem qp;
    std::size_t expected_minimizers;
};

} // namespace

// The battery of eight HAND-BUILT indefinite QPs. For each, in BOTH ws_algebra
// modes, the engine's answer must be one of the enumerated local minimizers --
// not "the" optimum, which for a nonconvex QP is not what an active-set method
// promises and which the two modes may legitimately disagree about (see this
// file's header note on cross-mode equality).
//
// The expected minimizer COUNTS are pinned too, so a regression in the oracle's
// second-order filter cannot make the match trivially easy by admitting extra
// points. Each fixture's comment lists the set.
TEST(QpEngineIndefinite, EngineLandsOnALocalMinimizer) {
    const std::vector<IndefiniteCase> cases = {
        {"saddle_box_qp", saddle_box_qp(), 2},
        {"diagonal_indefinite_box_qp", diagonal_indefinite_box_qp(), 2},
        {"negative_diagonal_box_qp", negative_diagonal_box_qp(), 8},
        {"ride_to_bound_qp", ride_to_bound_qp(), 1},
        {"ride_to_row_qp", ride_to_row_qp(), 1},
        {"indefinite_equality_qp", indefinite_equality_qp(), 2},
        {"indefinite_equality_and_row_qp", indefinite_equality_and_row_qp(), 2},
        {"two_negative_eigenvalue_row_qp", two_negative_eigenvalue_row_qp(), 5},
    };

    for (const auto &c : cases) {
        SCOPED_TRACE(c.name);
        const std::vector<QpSolution> mins = enumerate_local_minimizers(c.qp);
        ASSERT_EQ(mins.size(), c.expected_minimizers);

        // Every fixture here is genuinely indefinite -- if an edit made one
        // convex the test would still pass while covering nothing.
        {
            const Eigen::MatrixXd Hu(c.qp.H);
            const Eigen::MatrixXd Hd = Hu.selfadjointView<Eigen::Upper>();
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hd);
            EXPECT_LT(es.eigenvalues().minCoeff(), -1e-9) << "fixture is not indefinite";
        }

        for (const auto algebra :
             {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
            QpOptions opts;
            opts.ws_algebra = algebra;
            const QpSolution sol = QpEngine{opts}.solve(c.qp);
            SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border"
                                                                          : "refactorize");

            ASSERT_EQ(sol.status, QpStatus::kOptimal);
            EXPECT_LT(primal_violation(c.qp, sol.x), 1e-8);
            // Note there is deliberately NO objective comparison against the
            // matched point: `matched` is defined by |x - x_min|_inf <= 1e-7,
            // so objective agreement follows from it rather than corroborating
            // it -- and on a fixture whose gradient reaches 18 (ride_to_row_qp)
            // a match at the edge of that radius would move the objective by
            // ~5e-6, i.e. the "corroboration" would be the stricter test and
            // would fail first. What the match is worth is exactly what it
            // says; the second-order check below is the independent evidence.
            const Index matched = index_of_point(mins, sol.x, 1e-7);
            ASSERT_GE(matched, 0) << "engine answer is not any enumerated local minimizer: x = "
                                  << sol.x.transpose();
            expect_multipliers_consistent(c.qp, sol, 1e-6);
            EXPECT_GE(smallest_reduced_eigenvalue(c.qp, sol), -1e-8);
        }
    }
}

namespace {

// The randomized half of the battery. H = M^T M - s*I with s placed in a GAP of
// M^T M's spectrum, which is what buys control over how many eigenvalues end up
// negative: putting s midway between the k-th and (k+1)-th smallest eigenvalues
// of M^T M makes exactly k of H's eigenvalues negative. k alternates 1, 2, 1, 2
// with the seed's parity, so the battery covers both "one negative curvature
// direction" and "two".
//
// The draw is REJECTED and repeated (from the same rng stream, so each seed
// stays reproducible) unless the result has the intended 1 or 2 negative
// eigenvalues AND every eigenvalue is at least 1e-2 from zero. The margin
// matters: an H with an eigenvalue at 1e-12 is a PSD-singular problem wearing
// an indefinite label, where the oracle's own 1e-10 curvature threshold and the
// engine's regularization are both in play and a disagreement would say nothing
// about the indefinite path.
//
// bi is drawn in [-0.25, 0.25] rather than kept positive, so roughly half the
// rows are VIOLATED at the cold start x = clamp(0, l, u) = 0 and the homotopy
// runs. That is deliberate: with strictly feasible rows the general constraints
// were almost never active at a minimizer (measured), and the battery would
// have been a box-constrained battery with two decorative rows.
QpProblem random_indefinite(int n, int mi, unsigned seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    Eigen::MatrixXd Hd;
    for (int attempt = 0; attempt < 200; ++attempt) {
        Eigen::MatrixXd M(n, n);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                M(i, j) = unit(rng);
            }
        }
        const Eigen::MatrixXd G = M.transpose() * M;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(G); // eigenvalues ascending
        const int k = 1 + static_cast<int>(seed % 2);
        Hd = G - 0.5 * (es.eigenvalues()(k - 1) + es.eigenvalues()(k)) *
                     Eigen::MatrixXd::Identity(n, n);

        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> esH(Hd);
        int negatives = 0;
        double closest_to_zero = std::numeric_limits<double>::infinity();
        for (int i = 0; i < n; ++i) {
            negatives += esH.eigenvalues()(i) < 0.0 ? 1 : 0;
            closest_to_zero = std::min(closest_to_zero, std::abs(esH.eigenvalues()(i)));
        }
        // negatives == k holds for every non-degenerate draw (the shift moves
        // exactly the k smallest eigenvalues below zero), so what this loop is
        // really rejecting is a draw whose spectrum crowds the shift -- either
        // a tied eigenvalue pair, which would leave the count wrong, or a small
        // gap, which would leave an eigenvalue near zero.
        if (negatives == k && closest_to_zero > 1e-2) {
            break;
        }
    }

    QpProblem qp;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (int i = 0; i < n; ++i) {
        qp.g(i) = unit(rng);
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
        qp.bi(r) = 0.25 * unit(rng);
    }
    qp.lower = Vec::Constant(n, -1.5);
    qp.upper = Vec::Constant(n, 1.5);
    return qp;
}

} // namespace

// 25 randomized indefinite QPs (n = 4, mi = 2, box [-1.5, 1.5]^4, seeds 0..24
// -- every seed documented by being the loop index) x both ws_algebra modes.
//
// WHAT IS ASSERTED, and why it is not "the engine returns kOptimal every time".
// Every kOptimal answer must coincide with an enumerated local minimizer and
// carry multipliers that certify it. Non-kOptimal outcomes are COUNTED and
// bounded rather than forbidden, because a kInfeasible on a feasible problem is
// a known, rare engine limitation rather than a test bug: a ride that pins a
// blocker can leave the next regularized EQP with a row residual that
// violation_is_structural reads as structural (Task 7 fix round 2, seed 777
// case 1613, landing residual 7.98e-6). kMaxIter is likewise reachable on a
// degenerate working set. Demanding zero would make this battery a flake
// detector for a defect it is not the place to fix.
//
// MEASURED ON THIS BATTERY: 50 of 50 blocks kOptimal, all 50 matched an
// enumerated minimizer, and every seed's minimizer set is non-empty (sizes
// 1..4; 21 of the 25 sets contain at least one point with a general row
// active). The budget below is therefore slack that was never spent -- it
// exists so the known class cannot turn a real regression into an unrelated
// argument, not to excuse one. A regression that pushes more than two blocks
// off kOptimal still fails.
TEST(QpEngineIndefinite, RandomizedIndefiniteBatteryLandsOnLocalMinimizers) {
    constexpr int kSeeds = 25;
    constexpr int kBlocks = 2 * kSeeds;
    // Non-kOptimal budget. Observed: 0 (see above).
    constexpr int kNonOptimalBudget = 2;

    int non_optimal = 0;
    int max_iter = 0;
    int infeasible = 0;
    int numerical_error = 0;

    for (unsigned seed = 0; seed < kSeeds; ++seed) {
        SCOPED_TRACE("seed " + std::to_string(seed));
        const QpProblem qp = random_indefinite(4, 2, seed);

        // The generator's coverage claim, PINNED rather than trusted: seed
        // parity is supposed to alternate between one and two negative
        // eigenvalues, and nothing else in this test can tell the two apart --
        // so replacing the generator's `k` with a constant would silently halve
        // the coverage while every other assertion still passed. The
        // eigenvalue-margin check is the one that can fire if the rejection
        // loop ever exhausts its 200 draws (which leaves a near-singular
        // indefinite H, not a convex one).
        {
            const Eigen::MatrixXd Hu(qp.H);
            const Eigen::MatrixXd Hd = Hu.selfadjointView<Eigen::Upper>();
            Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(Hd);
            ASSERT_EQ((es.eigenvalues().array() < 0.0).count(), 1 + static_cast<Index>(seed % 2));
            ASSERT_GT(es.eigenvalues().cwiseAbs().minCoeff(), 1e-2);
        }

        const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);
        // A seed whose minimizer set came back empty would make every kOptimal
        // below fail for the wrong reason; catch that here instead.
        ASSERT_FALSE(mins.empty()) << "no local minimizer enumerated for seed " << seed;

        for (const auto algebra :
             {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
            QpOptions opts;
            opts.ws_algebra = algebra;
            const QpSolution sol = QpEngine{opts}.solve(qp);
            SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border"
                                                                          : "refactorize");

            if (sol.status != QpStatus::kOptimal) {
                ++non_optimal;
                max_iter += sol.status == QpStatus::kMaxIter ? 1 : 0;
                infeasible += sol.status == QpStatus::kInfeasible ? 1 : 0;
                numerical_error += sol.status == QpStatus::kNumericalError ? 1 : 0;
                continue;
            }

            EXPECT_LT(primal_violation(qp, sol.x), 1e-8);
            EXPECT_GE(index_of_point(mins, sol.x, 1e-7), 0)
                << "kOptimal answer is not any of the " << mins.size()
                << " enumerated local minimizers: x = " << sol.x.transpose();
            expect_multipliers_consistent(qp, sol, 1e-6);
        }
    }

    EXPECT_LE(non_optimal, kNonOptimalBudget)
        << non_optimal << " of " << kBlocks << " blocks were not kOptimal (" << max_iter
        << " kMaxIter, " << infeasible << " kInfeasible, " << numerical_error
        << " kNumericalError); the budget is " << kNonOptimalBudget << " and the pinned "
        << "observation is 0";
}

// THE WEAKLY-ACTIVE-BOUND DEFECT, NOW FIXED (Phase 3 Task 1). This test was
// written in Phase 2 to pin the DEFECT -- it asserted the engine certified the
// saddle -- with the standing instruction that the day it failed was the day
// the fix landed. It did; the assertions below are the flipped ones.
//
// RENAMED in Task 1, since the old name asserted the opposite of what the body
// now checks. For the ledger trail: this was
// QpEngineIndefinite.EngineCertifiesANonMinimizerAtAWeaklyActiveBound
// throughout Phase 2, and that is the name the Phase-2 reports, the Task-1
// brief and (historically) qp_engine.h section 4b refer to.
//
//   min x0*x1 + 1/2 x2^2 - x2
//   s.t. x0 + x2 = 0.5,  x0 + x1 <= -2,  x in [-2, 2]^3
//
// (indefinite_equality_and_row_qp with the row tightened from -1 to -2.)
//
// THE SADDLE. x = (-1.5, -0.5, 2) really is a KKT point: with row 0 active and
// x2 at its upper bound, the gradient (-0.5, -1.5, 1) gives lambda_i = 1.5,
// lambda_e = -1 and z2 = 0. But z2 = 0 is a WEAKLY active bound, so the
// critical cone is not {0}: it is the ray {t*(1, -1, -1) : t >= 0} -- the
// direction that stays on the equality and on row 0 while moving x2 DOWN, away
// from its bound. Along it
//   grad . d = -0.5 + 1.5 - 1 = 0    and    d^T H d = -1,
// so f(x + t d) = f(x) - t^2/2: a STRICT DESCENT direction, feasible for every
// small t > 0. The point is not a local minimizer, and the pre-fix engine's
// second-order machinery did not see it because with the bound counted active
// the reduced space is empty.
//
// The oracle sees it because it groups the two active-set labelings of this one
// point -- {equality, row 0, x2@upper} and {equality, row 0} (which produces
// the SAME x, since z2 = 0) -- and requires both to pass. The second has null
// space span{(1,-1,-1)} and curvature -1.
//
// WHAT THE FIX DOES HERE (section 4b's ZERO-MULTIPLIER PROBE). At the
// would-be-kOptimal exit the engine finds |z2| <= opt_tol, tentatively drops
// that bound, re-runs the inertia gate on {equality, row 0} and reads kWrong --
// the negative curvature the full labeling hid. The drop is made REAL and the
// loop resumes, so the saddle is never certified.
//
// WHAT TASK 1 LEFT ON THE TABLE, AND WHAT TASK 6b COLLECTED. Section 4c's
// ride, which would take the exposed direction to x1's lower bound and land
// exactly on the minimizer, cannot be armed off this drop. Its arming direction
// is the post-drop EQP step p, and section 4c's own formula gives
// p = (-lambda_c / sigma) * q -- so a drop whose multiplier lambda_c is ZERO
// produces p == 0 identically. That is not an accident of this fixture but the
// defining property of a WEAKLY active constraint, so it is the general case for
// every drop this probe makes. MEASURED in Task 1: |p|_inf = 4.4e-16 (border) /
// 6.7e-16 (refactorize) against a step_tol of 2e-9, i.e. the loop's
// negligible-step branch fires, section 4c explicitly declines to judge a
// rounding-noise direction, and the point fell to section 4b's certification
// branch -- which read the reduced set's TRUSTED kWrong and reported
// kNumericalError AT THE SADDLE.
//
// Task 6b closed that with section 4b's POST-PROBE RESTART: at exactly that
// dead end (a probe-driven drop has been made, the loop cannot move, and the
// reduced labeling reads a trusted kWrong) the engine spends a ONE-SHOT
// temporary-vertex repair instead of reporting failure. That is the mechanism
// Task 1's own review measured as recovering this fixture from a warm re-solve;
// the restart just reaches it without the caller having to re-enter. The loop
// resumes from the repaired vertex and walks to the true minimizer.
//
// So the ANSWER is now right, not merely uncertified. The first block of
// assertions states the property that must hold however the loop gets there;
// the second pins the exit actually achieved. Do not weaken the oracle.
//
// SUCCESSOR INSTRUCTION EXECUTED (Task 6b). The second block used to pin
// kNumericalError at the saddle, with the standing instruction to flip it to
// kOptimal at (0, -2, 0.5), objective -0.375, the day the follow-up landed.
// That is what it now asserts, and the first block is untouched, exactly as
// that instruction prescribed.
TEST(QpEngineIndefinite, WeaklyActiveBoundIsNotCertifiedOptimal) {
    QpProblem qp = indefinite_equality_and_row_qp();
    qp.bi = Vec::Constant(1, -2.0);

    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);
    ASSERT_EQ(mins.size(), 1u);
    EXPECT_LT((mins[0].x - vec3(0.0, -2.0, 0.5)).lpNorm<Eigen::Infinity>(), 1e-9);
    EXPECT_NEAR(objective(qp, mins[0].x), -0.375, 1e-9);
    EXPECT_EQ(mins[0].bound_state[1], BoundState::kAtLower);
    EXPECT_NEAR(mins[0].z(1), 1.5, 1e-9);

    // The saddle's descent direction, evaluated here rather than asserted from
    // the comment: feasible, flat-sloped, negatively curved, strictly better.
    const Vec saddle = vec3(-1.5, -0.5, 2.0);
    {
        const Vec d = vec3(1.0, -1.0, -1.0);
        const Vec grad = qp.H.selfadjointView<Eigen::Upper>() * saddle + qp.g;
        EXPECT_LT(primal_violation(qp, saddle), 1e-8);
        EXPECT_NEAR(grad.dot(d), 0.0, 1e-9);
        EXPECT_LT(d.dot(qp.H.selfadjointView<Eigen::Upper>() * d), -0.5);
        const Vec stepped = saddle + 0.1 * d;
        EXPECT_LT(primal_violation(qp, stepped), 1e-8);                  // still feasible ...
        EXPECT_LT(objective(qp, stepped), objective(qp, saddle) - 1e-4); // ... and better
        EXPECT_LT(index_of_point(mins, saddle, 1e-7), 0); // ... so not a local minimizer
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        // THE FIX, stated as the property that must hold however the loop gets
        // there: the saddle is never certified.
        const bool certified_the_saddle =
            sol.status == QpStatus::kOptimal && (sol.x - saddle).lpNorm<Eigen::Infinity>() < 1e-7;
        EXPECT_FALSE(certified_the_saddle) << "x = " << sol.x.transpose();

        // Any kOptimal exit must be AT an enumerated local minimizer -- here
        // there is exactly one -- with consistent multipliers.
        if (sol.status == QpStatus::kOptimal) {
            EXPECT_LT(primal_violation(qp, sol.x), 1e-8);
            EXPECT_GE(index_of_point(mins, sol.x, 1e-7), 0)
                << "kOptimal answer is not the sole local minimizer: x = " << sol.x.transpose();
            expect_multipliers_consistent(qp, sol, 1e-6);
        }

        // THE EXIT ACTUALLY ACHIEVED, in both modes: the probe's real drop
        // leaves the loop at a KKT point of the REDUCED working set whose
        // inertia is a trusted kWrong, section 4b's POST-PROBE RESTART spends
        // its one temporary-vertex repair there, and the resumed loop walks to
        // the sole local minimizer.
        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_LT((sol.x - vec3(0.0, -2.0, 0.5)).lpNorm<Eigen::Infinity>(), 1e-8);
        EXPECT_NEAR(objective(qp, sol.x), -0.375, 1e-9);

        // The multipliers that come with it, hand-derived at (0, -2, 0.5):
        // grad = (x1, x0, x2 - 1) = (-2, 0, -0.5), and
        // grad + Ae' le + Ai' li - z = 0 reads
        //   [2] -0.5 + le        = 0  =>  le   = 0.5
        //   [0] -2   + le  + li  = 0  =>  li   = 1.5   (>= 0)
        //   [1]  0   + li  - z1  = 0  =>  z1   = 1.5   (>= 0 at a LOWER bound)
        EXPECT_NEAR(sol.lambda_e(0), 0.5, 1e-8);
        EXPECT_NEAR(sol.lambda_i(0), 1.5, 1e-8);
        EXPECT_NEAR(sol.z(1), 1.5, 1e-8);

        // The labeling at the answer: x1 at its LOWER bound, row 0 active, and
        // the weakly active bound the probe released still released.
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtLower);
        EXPECT_EQ(sol.bound_state[2], BoundState::kFree);
        EXPECT_TRUE(sol.ineq_active[0]);
    }
}

// THE CONVEX NO-OP GUARD for section 4b's zero-multiplier probe. The probe is
// gated on a working-set multiplier being within opt_tol of zero, and its
// candidate list is built from multipliers the loop already priced -- so under
// STRICT COMPLEMENTARITY the list is empty and the probe returns before any
// linear algebra runs at all. That is a stronger claim than "the probe changes
// no convex answer" (which is also true, and trivially: H + delta*I is positive
// definite on all of R^n for a convex H, so it stays positive definite on the
// enlarged null space a drop produces, and the probe can only ever read kOk),
// and it is the one worth pinning, because it is what keeps the feature off the
// cost path of every non-degenerate solve.
//
// Two spot-checks, one per kind of active constraint, both with every
// multiplier bounded far away from opt_tol (1e-9), and both asserted
// COUNTER-IDENTICAL to the pre-Task-1 engine: the numbers below were measured
// against the engine with this feature reverted and are unchanged by it.
// Contrast GateIsANoOpOnAConvexFixture above, whose optimum IS degenerate and
// which does pay one probe.
TEST(QpEngineIndefinite, ZeroMultiplierProbeIsANoOpOnStrictComplementarity) {
    // (a) BOTH BOUNDS strictly active. min 1/2||x||^2 + (1, -3).x on [0, 1]^2:
    // the unconstrained minimizer (-1, 3) is outside the box on both sides, so
    // x* = (0, 1) with z = (1, -2) -- nine orders of magnitude off opt_tol.
    QpProblem bounds_qp;
    bounds_qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    bounds_qp.g = Vec(2);
    bounds_qp.g << 1.0, -3.0;
    bounds_qp.Ae.resize(0, 2);
    bounds_qp.be = Vec(0);
    bounds_qp.Ai.resize(0, 2);
    bounds_qp.bi = Vec(0);
    bounds_qp.lower = Vec::Zero(2);
    bounds_qp.upper = Vec::Constant(2, 1.0);

    // (b) A GENERAL ROW strictly active, no bound active. min 1/2||x||^2 s.t.
    // -x0 - x1 <= -2 on [-10, 10]^2: x* = (1, 1) with lambda_i = 1.
    QpProblem row_qp;
    row_qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    row_qp.g = Vec::Zero(2);
    row_qp.Ae.resize(0, 2);
    row_qp.be = Vec(0);
    Eigen::MatrixXd Aid(1, 2);
    Aid << -1, -1;
    row_qp.Ai = Aid.sparseView();
    row_qp.bi = Vec::Constant(1, -2.0);
    row_qp.lower = Vec::Constant(2, -10.0);
    row_qp.upper = Vec::Constant(2, 10.0);

    struct Case {
        const char *name;
        const QpProblem &qp;
        Vec x_star;
        // Counters, per mode: {factorizations, schur_updates, minor_iters}.
        std::array<Index, 3> border;
        std::array<Index, 3> refactorize;
    };
    const Case cases[] = {
        {"bounds", bounds_qp, vec2(0.0, 1.0), {1, 2, 3}, {2, 0, 3}},
        {"row", row_qp, vec2(1.0, 1.0), {1, 0, 2}, {2, 0, 2}},
    };

    for (const auto &c : cases) {
        for (const auto algebra :
             {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
            const bool border = algebra == WorkingSetLinearAlgebra::kSchurBorder;
            SCOPED_TRACE(std::string(c.name) + (border ? "/border" : "/refactorize"));
            QpOptions opts;
            opts.ws_algebra = algebra;
            const QpSolution sol = QpEngine{opts}.solve(c.qp);

            EXPECT_EQ(sol.status, QpStatus::kOptimal);
            EXPECT_LT((sol.x - c.x_star).lpNorm<Eigen::Infinity>(), 1e-8);

            // STRICT COMPLEMENTARITY, asserted rather than assumed: every
            // multiplier the probe would look at is enormous next to opt_tol,
            // so no candidate exists and the probe cannot run.
            for (Index i = 0; i < c.qp.n(); ++i) {
                if (sol.bound_state[static_cast<std::size_t>(i)] != BoundState::kFree) {
                    EXPECT_GT(std::abs(sol.z(i)), 1e-3) << "bound " << i;
                }
            }
            for (Index j = 0; j < c.qp.mi(); ++j) {
                if (sol.ineq_active[static_cast<std::size_t>(j)]) {
                    EXPECT_GT(std::abs(sol.lambda_i(j)), 1e-3) << "row " << j;
                }
            }

            const std::array<Index, 3> &want = border ? c.border : c.refactorize;
            EXPECT_EQ(sol.counters.factorizations, want[0]);
            EXPECT_EQ(sol.counters.schur_updates, want[1]);
            EXPECT_EQ(sol.counters.minor_iters, want[2]);
        }
    }
}

// THE SAME DEFECT CLASS THROUGH A GENERAL ROW, so the fix is not a
// bounds-only patch. This is WeaklyActiveBoundIsNotCertifiedOptimal's
// fixture with the weakly active constraint re-expressed as a row of Ai:
//
//   min x0*x1 + 1/2 x2^2 - x2
//   s.t. x0 + x2 = 0.5,  x0 + x1 <= -2,  x2 <= 2,  x in [-2,2] x [-2,2] x [-3,3]
//
// x2's own box is WIDENED to [-3, 3] and the new row 1 (x2 <= 2) takes over the
// job its upper bound used to do. The FEASIBLE SET IS UNCHANGED by that swap:
// the equality forces x0 = 0.5 - x2, so x0 >= -2 already caps x2 at 2.5 and
// x0 <= 2 already floors it at -1.5, leaving x2 in [-1.5, 2] both before and
// after -- and the new lower bound -3 is slack throughout. So the sole local
// minimizer is still (0, -2, 0.5) at objective -0.375.
//
// THE HAND ALGEBRA at the saddle x = (-1.5, -0.5, 2), which is feasible (row 1
// holds with equality, row 0 holds with equality, the equality holds):
//   grad = H x + g = (x1, x0, x2 - 1) = (-0.5, -1.5, 1),
//   Ae = (1, 0, 1),  Ai row 0 = (1, 1, 0),  Ai row 1 = (0, 0, 1),
// and stationarity grad + Ae' le + Ai' li = 0 reads, component by component,
//   [1] -1.5 + li_0        = 0  =>  li_0 = 1.5   (> 0, so row 0 is strict)
//   [0] -0.5 + le  + li_0  = 0  =>  le   = -1
//   [2]  1   + le  + li_1  = 0  =>  li_1 = 0     (<- WEAKLY ACTIVE, the defect)
// With row 1 counted active the working set spans all of R^3 and the reduced
// space is empty, so the inertia gate sees nothing wrong. Drop row 1 and the
// critical cone is null{(1,0,1), (1,1,0)} = span{(1,-1,-1)}, on which
//   grad . d = -0.5 + 1.5 - 1 = 0    and    d' H d = -1,
// the same strict quadratic descent as in the bound version. The zero-
// multiplier probe drops row 1, reads kWrong, the certificate is refused, and
// (Task 6b) the post-probe restart recovers the minimizer from there.
TEST(QpEngineIndefinite, WeaklyActiveGeneralRowIsNotCertifiedEither) {
    QpProblem qp = indefinite_equality_and_row_qp();
    qp.bi = Vec::Constant(1, -2.0);
    // Row 1: x2 <= 2, replacing x2's upper bound.
    Eigen::MatrixXd Aid(2, 3);
    Aid << 1, 1, 0, 0, 0, 1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -2.0, 2.0;
    qp.lower(2) = -3.0;
    qp.upper(2) = 3.0;

    const std::vector<QpSolution> mins = enumerate_local_minimizers(qp);
    ASSERT_EQ(mins.size(), 1u);
    EXPECT_LT((mins[0].x - vec3(0.0, -2.0, 0.5)).lpNorm<Eigen::Infinity>(), 1e-9);
    EXPECT_NEAR(objective(qp, mins[0].x), -0.375, 1e-9);

    // The saddle and its hidden descent direction, evaluated here rather than
    // taken from the comment above.
    const Vec saddle = vec3(-1.5, -0.5, 2.0);
    {
        const Vec d = vec3(1.0, -1.0, -1.0);
        const Vec grad = qp.H.selfadjointView<Eigen::Upper>() * saddle + qp.g;
        EXPECT_LT(primal_violation(qp, saddle), 1e-8);
        EXPECT_NEAR((qp.Ai * saddle)(1) - qp.bi(1), 0.0, 1e-12); // row 1 is ACTIVE ...
        EXPECT_GT(saddle(2) - qp.lower(2), 1.0);                 // ... and no bound is
        EXPECT_LT(qp.upper(2) - saddle(2), 1.0 + 1e-12);
        EXPECT_NEAR(grad.dot(d), 0.0, 1e-9);
        EXPECT_LT(d.dot(qp.H.selfadjointView<Eigen::Upper>() * d), -0.5);
        const Vec stepped = saddle + 0.1 * d;
        EXPECT_LT(primal_violation(qp, stepped), 1e-8);
        EXPECT_LT(objective(qp, stepped), objective(qp, saddle) - 1e-4);
        EXPECT_LT(index_of_point(mins, saddle, 1e-7), 0);
    }

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        const bool certified_the_saddle =
            sol.status == QpStatus::kOptimal && (sol.x - saddle).lpNorm<Eigen::Infinity>() < 1e-7;
        EXPECT_FALSE(certified_the_saddle) << "x = " << sol.x.transpose();

        if (sol.status == QpStatus::kOptimal) {
            EXPECT_LT(primal_violation(qp, sol.x), 1e-8);
            EXPECT_GE(index_of_point(mins, sol.x, 1e-7), 0)
                << "kOptimal answer is not the sole local minimizer: x = " << sol.x.transpose();
            expect_multipliers_consistent(qp, sol, 1e-6);
        }

        // The exit actually achieved, identical in shape to the bound version:
        // the probe drops row 1 for real, the resulting KKT point of the
        // reduced working set reads a trusted kWrong, section 4b's POST-PROBE
        // RESTART spends its one repair there, and the resumed loop reaches the
        // sole local minimizer. (Pre-Task-1 this fixture returned kOptimal at
        // the SADDLE in both modes -- measured while the test was written, which
        // is what makes it a regression test rather than a restatement of the
        // bound case; Task 1 turned that into kNumericalError at the saddle and
        // Task 6b into the answer below. The geometry recovers exactly as the
        // bound version does, which is the point: the restart is not a
        // bounds-only patch either.)
        EXPECT_EQ(sol.status, QpStatus::kOptimal);
        EXPECT_LT((sol.x - vec3(0.0, -2.0, 0.5)).lpNorm<Eigen::Infinity>(), 1e-8);
        EXPECT_NEAR(objective(qp, sol.x), -0.375, 1e-9);
        EXPECT_EQ(sol.bound_state[1], BoundState::kAtLower);
        EXPECT_TRUE(sol.ineq_active[0]);
        EXPECT_FALSE(sol.ineq_active[1]); // the weakly active row was released

        // THE MULTIPLIERS, hand-derived here as in the bound version rather
        // than left to expect_multipliers_consistent above -- that helper
        // checks the reported quadruple against ITSELF (stationarity, signs,
        // complementarity), so it would pass on any consistent labeling,
        // including a different one. These are the VALUES. At (0, -2, 0.5):
        // grad = (x1, x0, x2 - 1) = (-2, 0, -0.5), row 1 (x2 <= 2) is SLACK
        // (0.5 < 2) so li_1 = 0, and grad + Ae' le + Ai' li - z = 0 reads
        //   [2] -0.5 + le  + li_1 = 0  =>  le   = 0.5
        //   [0] -2   + le  + li_0 = 0  =>  li_0 = 1.5   (>= 0)
        //   [1]  0   + li_0 - z1  = 0  =>  z1   = 1.5   (>= 0 at a LOWER bound)
        // i.e. the same prices as the bound version, as they must be: the two
        // fixtures have the same feasible set and the same objective.
        EXPECT_NEAR(sol.lambda_e(0), 0.5, 1e-8);
        EXPECT_NEAR(sol.lambda_i(0), 1.5, 1e-8);
        EXPECT_NEAR(sol.lambda_i(1), 0.0, 1e-8);
        EXPECT_NEAR(sol.z(1), 1.5, 1e-8);
    }
}

// THE POST-PROBE RESTART IS ONE-SHOT PER SOLVE, and this is the fixture that
// makes the budget observable rather than theoretical.
//
// Section 4b's restart lifts the `iter == 0` gate on repair_temporary_vertex
// for exactly one trigger -- a probe-driven drop that leaves the loop unable
// to move at a trusted-kWrong KKT point -- and exactly ONCE per solve. The
// bound applies whether or not it is enough: the repair is not a proof
// procedure, and a solve that could consume it repeatedly would be an
// unbounded amount of work bought with an anti-cycling argument the probe's
// exemption set makes only for DROPS.
//
// TWO INDEPENDENT COPIES of WeaklyActiveBoundIsNotCertifiedOptimal's geometry,
// on disjoint variable triples with disjoint constraint rows. Nothing couples
// them, so each block reaches its own weakly active bound at its own saddle and
// each needs its own probe drop; the second drop's dead end finds the restart
// already spent and falls through to the certification branch. MEASURED: block
// 1 (the one the restart repaired) lands on its minimizer while block 0 is left
// at its saddle, and the solve exits kNumericalError with the multipliers
// cleared, in both algebra modes.
//
// This is also the shape SqpDriverQpFailure's fixtures are built on: a
// reproducible kNumericalError WITH a finite, bound-feasible iterate is
// precisely what the driver's shrink-retry routing needs to be exercised
// against, and replication is the only way to get one now that a single copy
// recovers.
TEST(QpEngineIndefinite, PostProbeRestartIsOneShotPerSolve) {
    // Two copies of indefinite_equality_and_row_qp (row tightened to -2) on
    // variables 0..2 and 3..5.
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(6, 6);
    Eigen::MatrixXd Aed = Eigen::MatrixXd::Zero(2, 6);
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(2, 6);
    qp.g = Vec::Zero(6);
    qp.be = Vec::Constant(2, 0.5);
    qp.bi = Vec::Constant(2, -2.0);
    for (Index b = 0; b < 2; ++b) {
        const Index o = 3 * b;
        Hd(o + 0, o + 1) = 1.0;
        Hd(o + 2, o + 2) = 1.0;
        qp.g(o + 2) = -1.0;
        Aed(b, o + 0) = 1.0;
        Aed(b, o + 2) = 1.0;
        Aid(b, o + 0) = 1.0;
        Aid(b, o + 1) = 1.0;
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.Ae = Aed.sparseView();
    qp.Ai = Aid.sparseView();
    qp.lower = Vec::Constant(6, -2.0);
    qp.upper = Vec::Constant(6, 2.0);

    const Vec saddle = vec3(-1.5, -0.5, 2.0);
    const Vec minimizer = vec3(0.0, -2.0, 0.5);

    for (const auto algebra :
         {WorkingSetLinearAlgebra::kSchurBorder, WorkingSetLinearAlgebra::kRefactorize}) {
        QpOptions opts;
        opts.ws_algebra = algebra;
        const QpSolution sol = QpEngine{opts}.solve(qp);
        SCOPED_TRACE(algebra == WorkingSetLinearAlgebra::kSchurBorder ? "border" : "refactorize");

        // The budget: one block repaired, one block not, so no certificate.
        EXPECT_EQ(sol.status, QpStatus::kNumericalError);
        EXPECT_LT((sol.x.segment(0, 3) - saddle).lpNorm<Eigen::Infinity>(), 1e-7)
            << "block 0 = " << sol.x.segment(0, 3).transpose();
        EXPECT_LT((sol.x.segment(3, 3) - minimizer).lpNorm<Eigen::Infinity>(), 1e-7)
            << "block 1 = " << sol.x.segment(3, 3).transpose();

        // kNumericalError's standing semantics: final iterate reported,
        // multipliers cleared. The driver's routing depends on the FIRST half
        // of that (a usable iterate) and is indifferent to the second, which is
        // why it seeds from the activity only.
        EXPECT_EQ(sol.z.lpNorm<Eigen::Infinity>(), 0.0);
        EXPECT_EQ(sol.lambda_e.lpNorm<Eigen::Infinity>(), 0.0);
        EXPECT_EQ(sol.lambda_i.lpNorm<Eigen::Infinity>(), 0.0);

        // ... and that iterate is finite and inside the box, which is exactly
        // what sqp_driver.h's qp_failure_is_retryable tests.
        EXPECT_TRUE(sol.x.allFinite());
        EXPECT_LE(sol.x.lpNorm<Eigen::Infinity>(), 2.0 + 1e-12);
    }
}
