#pragma once

// =============================================================================
// ssn_fixtures.h -- THE ANALYTIC QP FIXTURES OF PHASE-7 TASKS 3 AND 4
// =============================================================================
//
// **SHARED, NOT COPIED** (review fix round 1, I5). These builders were defined
// in tests/test_ssn_engine.cpp's anonymous namespace, which made them
// unreachable from a measurement binary -- so the safeguards note's trajectory
// figures (docs/notes/2026-08-07-ssn-safeguards.md sections 3.4, 6 and 10.1)
// had no committed vehicle and could only be re-derived by rebuilding the
// fixtures somewhere else, which is exactly the drift a second copy
// guarantees. They live here now, and BOTH consumers include this one file:
// tests/test_ssn_engine.cpp (the behavioural record) and
// bench/ssn_safeguard_probe.cpp (the note's reproduction vehicle). This is the
// pattern bench/corpus_cells.h and tests/sqp/support/hs_sweeps.h already follow.
//
// Test-support only, and deliberately gtest-free so a bench target can link
// it. Nothing here is shipped.

#include <cmath>

#include <Eigen/Dense>
#include <Eigen/SparseCore>

#include <hven/detail/sqp/qp_problem.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::test_support {

// =============================================================================
// THE ANALYTIC FIXTURES (Phase-7 Task 3)
// =============================================================================
//
// BENIGN BY CONSTRUCTION, and deliberately so: ssn_engine.h implements the
// LOCAL semismooth-Newton method only -- full undamped steps, no line search,
// no proximal term, no uncertain set. Every convergence assertion below is
// therefore scoped to a strictly convex Hessian, a non-degenerate solution and
// a start point in its neighbourhood. Task 4 adds the safeguards and Task 6
// scores the safeguarded engine on the committed corpus; NOTHING in this file
// should be read as evidence about either.
//
// EVERY ITERATION COUNT PINNED HERE IS AN OBSERVED VALUE, measured on this
// development machine with clang++ against MKL Pardiso (the project's
// authoritative configuration -- CLAUDE.md's COMPILER note). ============
// MKL-OBSERVED. NOT YET RE-VERIFIED ON APPLE ACCELERATE. ============ The
// counts are properties of the Newton trajectory, so a backend whose
// factorization rounds differently could in principle land on a different
// count near a tolerance boundary; the assertions are written as inequalities
// (<=) wherever the brief allows one for exactly that reason, and the two
// EXACT counts (1 iteration) are the ones a linear system settles in closed
// form rather than by convergence.

// --- Fixture (a)/(b): 2-variable QP with a known 1-of-2 active set ---------
//
//     min  1/2 (x0^2 + x1^2) - 2 x0 - 2 x1
//     s.t. x0 + x1 <= 2      (row 0)
//          x0 - x1 <= 5      (row 1)
//
// Unconstrained minimizer (2, 2) violates row 0 only. On the active set
// {row 0}: x - 2*1 + lambda0 * (1,1) = 0 and x0 + x1 = 2 give lambda0 = 1 and
// x = (1, 1); row 1 then reads 1 - 1 = 0 <= 5, strictly inactive with
// lambda1 = 0. So the exact primal-dual solution is
//
//     x* = (1, 1),  lambda_i* = (1, 0),  active set {row 0}
//
// which is non-degenerate (the active multiplier is 1, the inactive slack 5).
inline QpProblem two_row_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec::Constant(2, -2.0);
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1, 1, 1, -1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 2.0, 5.0;
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// --- Fixture (c): bounds-only box QP, n = 50 -------------------------------
//
// H is symmetric, tridiagonal, strictly diagonally dominant and has
// NON-POSITIVE off-diagonals (diagonal 2 + j/100, off-diagonal -0.3), so it is
// a symmetric positive definite M-matrix -- deliberately, because that is the
// exact setting Hintermueller-Ito-Kunisch's PDAS convergence theory addresses
// and therefore the fairest bench for a kernel that claims to be PDAS. g is a
// deterministic cosine sweep chosen so the unconstrained minimizer leaves the
// box [-1, 1]^n on most coordinates. No Ae, no Ai: the whole active-set
// question is in the 2n bound rows, which is the shape the walk pays most for
// (at n = 50 the walk needs 30 minor iterations against this kernel's 8 Newton
// steps -- observed, and not a benchmark: see BoxQpCountGrowsSlowlyInN for the
// measured grid and for what it does and does not say about n-scaling).
inline QpProblem box_qp(Index n) {
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(n, n);
    for (Index j = 0; j < n; ++j) {
        Hd(j, j) = 2.0 + 0.01 * static_cast<double>(j);
        if (j + 1 < n) {
            Hd(j, j + 1) = -0.3;
            Hd(j + 1, j) = -0.3;
        }
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (Index j = 0; j < n; ++j) {
        qp.g(j) = -3.0 * std::cos(0.7 * static_cast<double>(j));
    }
    qp.Ae.resize(0, n);
    qp.be = Vec(0);
    qp.Ai.resize(0, n);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(n, -1.0);
    qp.upper = Vec::Constant(n, 1.0);
    return qp;
}

// --- Fixture (d): equality-only QP -----------------------------------------
//
//     min 1/2||x||^2 + g^T x  s.t. x0 + x1 + x2 = 3, x0 - x2 = 1
//
// No FB pair anywhere, so the residual is affine in w and ONE Newton step is
// the exact solve (up to the delta/mu regularization).
inline QpProblem equality_only_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(3, 3).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << -1.0, 0.5, 2.0;
    Eigen::MatrixXd Aed(2, 3);
    Aed << 1, 1, 1, 1, 0, -1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec(2);
    qp.be << 3.0, 1.0;
    qp.Ai.resize(0, 3);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(3, -1e20);
    qp.upper = Vec::Constant(3, 1e20);
    return qp;
}

// --- The MIXED-BLOCK fixture (review fix round 1, I1) -----------------------
//
// **EVERY OTHER FIXTURE IN THIS FILE HAS AN EMPTY CONSTRAINT BLOCK**, and that
// was a real coverage hole rather than an aesthetic one: (a)/(b)/M6/M12 and the
// non-finite probe have me = 0; box_qp has me = 0; equality_only_qp has
// mi = mb = 0, so its diagonal-refresh loop body never executes at all. The
// `+ me` term in ssn_engine.h's positional diagonal write --
// `vals[outer[n + me + k]]` -- was therefore never exercised by any test, and
// the reviewer's planted mutant (`outer[n + me + k]` -> `outer[n + k]`, which
// aims the FB diagonals at the EQUALITY rows) passed the whole 574-test suite
// while diverging to ||F|| = 7.8e57 on a mixed QP.
//
// **THIS SHAPE IS EVERY TASK-5 SUBPROBLEM**: an SQP linearization has
// equalities AND inequalities AND bounds simultaneously. n = 8, me = 1, mi = 3,
// mb = 16 (two-sided bounds on all eight variables), strictly convex tridiagonal
// H, with the data chosen so the solution has a genuinely mixed active set --
// some inequality rows active, some bounds active, some of both slack.
inline QpProblem mixed_block_qp() {
    const Index n = 8;
    QpProblem qp;
    Eigen::MatrixXd Hd = Eigen::MatrixXd::Zero(n, n);
    for (Index j = 0; j < n; ++j) {
        Hd(j, j) = 2.0 + 0.05 * static_cast<double>(j);
        if (j + 1 < n) {
            Hd(j, j + 1) = -0.4;
            Hd(j + 1, j) = -0.4;
        }
    }
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(n);
    for (Index j = 0; j < n; ++j) {
        qp.g(j) = -2.5 * std::cos(0.9 * static_cast<double>(j)) - 0.4;
    }

    // One equality: the coordinates must sum to 1.5.
    Eigen::MatrixXd Aed = Eigen::MatrixXd::Ones(1, n);
    qp.Ae = Aed.sparseView();
    qp.be = Vec::Constant(1, 1.5);

    // Three inequalities, each touching a different overlapping window.
    Eigen::MatrixXd Aid = Eigen::MatrixXd::Zero(3, n);
    Aid(0, 0) = 1.0;
    Aid(0, 1) = 1.0;
    Aid(0, 2) = 1.0;
    Aid(1, 3) = 1.0;
    Aid(1, 4) = -1.0;
    Aid(1, 5) = 1.0;
    Aid(2, 5) = 2.0;
    Aid(2, 6) = 1.0;
    Aid(2, 7) = -1.0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(3);
    qp.bi << 0.6, 0.9, 0.3;

    qp.lower = Vec::Constant(n, -0.8);
    qp.upper = Vec::Constant(n, 0.7);
    return qp;
}

// =============================================================================
// THE TASK-4 FIXTURES
// =============================================================================
//
// **THE CYCLING COUNTER-EXAMPLES WERE SEARCHED FOR, NOT ASSERTED**, and the
// search is the evidence that they are hard to come by. Curtis-Han-Robinson
// (2015)'s framework exists because the plain primal-dual active-set iteration
// -- pick a partition, solve its equality-constrained KKT system, re-read the
// partition off the answer -- has no global convergence guarantee for a general
// convex QP (it does for an M-matrix or diagonally dominant H; Hintermueller-
// Ito-Kunisch's original theory), and can cycle. Ben Gharbia & Gilbert (2012)
// pin the boundary for the min-map form: the plain Newton-min iteration
// converges for every P-matrix LCP of dimension <= 2 and there are 3-dimensional
// P-matrices for which it cycles.
//
// **NEITHER PAPER'S EXAMPLE IS QUOTED HERE**, because neither is an example for
// THIS iteration: this kernel is FB-based, not min-map, and the FB residual is
// smooth away from one ray. So the failing partition family was reproduced by
// exhaustive random search over its defining shape -- symmetric positive
// definite but NOT M-matrix H, small n, feasible (the walk agrees), general
// inequality rows -- and the two instances below are the smallest found. The
// full search record, including two families in which the bare iteration never
// failed at all, is docs/notes/2026-08-07-ssn-safeguards.md section 3.

// --- Task-4 fixture (a1): n = 2, cycles from a WRONG-HINT start ------------
//
//     min 1/2 x' [2.7 2.7; 2.7 7.2] x + [1.4, -2.2]' x
//     s.t. [1.3 -0.7; -0.6 -0.2] x <= [1.8, -2.2],  -3 <= x <= 3
//
// H's eigenvalues are 1.435 and 8.465 -- strictly convex and WELL conditioned,
// so nothing here is a conditioning artefact -- and it is emphatically not an
// M-matrix (the off-diagonal is +2.7). The solution is x* = (2.794118,
// 2.617647), which the walk reaches in 3 minor iterations.
inline QpProblem cycling_qp_2var() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 2.7, 2.7, 2.7, 7.2;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 1.4, -2.2;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 1.3, -0.7, -0.6, -0.2;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1.8, -2.2;
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);
    return qp;
}

// The stated start for cycling_qp_2var: a wrong activity hint. Stated as a
// function so the test and the cycle detector cannot drift apart.
SsnStart cycling_start_2var() {
    SsnStart s;
    s.activity_hint.ineq = {false, true};
    s.activity_hint.bounds = {BoundState::kAtLower, BoundState::kAtUpper};
    return s;
}

// --- Task-4 fixture (a2): n = 3, cycles from the COLD start ----------------
//
// The stronger of the two, because no hint is involved at all: the bare
// iteration cycles from the default SsnStart{}. H's eigenvalues are 3.14, 5.42
// and 8.14 (strictly convex, well conditioned, positive off-diagonals). The
// solution is x* = (-0.730769, 3, -2.961538) -- a mixed active set with one
// inequality row and two bounds live -- which the walk reaches in 4 minors.
inline QpProblem cycling_qp_3var() {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    Hd << 4.9, 1.3, -1.7, 1.3, 5.2, 1.8, -1.7, 1.8, 6.6;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 2.8, 0.2, 4.7;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(3, 3);
    Aid << 0.0, 1.7, 2.6, -0.1, -1.9, -1.9, 1.3, 1.2, 3.0;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(3);
    qp.bi << -2.6, 0.0, 0.5;
    qp.lower = Vec::Constant(3, -3.0);
    qp.upper = Vec::Constant(3, 3.0);
    return qp;
}

// --- Task-4 fixture (b): a WEAKLY ACTIVE row (lambda* = 0 AND c* = 0) ------
//
// two_row_qp with row 1's right-hand side moved to 0, so at the solution
// x* = (1, 1) the row x0 - x1 <= 0 is EXACTLY TIGHT with multiplier EXACTLY
// zero. Strict complementarity fails there, which is precisely the
// configuration whose binary classification is a coin flip -- and the whole
// reason CHR's partition has a third set.
inline QpProblem weakly_active_qp() {
    QpProblem qp = two_row_qp();
    qp.bi(1) = 0.0;
    return qp;
}

// --- Task-4 fixture (c): CONTRADICTORY ROWS -------------------------------
//
// With u = -1.7 x0 + 1.9 x1 the two rows read u <= -1.4 and -u <= 0.6, i.e.
// u <= -1.4 AND u >= -0.6: infeasible by a gap of 0.8, which the two rows split
// evenly, so ||F||inf flattens onto 0.4 while both multipliers run away
// together. The walk reports QpStatus::kInfeasible on it independently.
inline QpProblem contradictory_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 1.3, 3.3;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1.7, 1.9, 1.7, -1.9;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -1.4, 0.6;
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// --- Task-4 fixture (c2): an INCONSISTENT EQUALITY BLOCK ------------------
//
//     min 1/2||x||^2 - x0 + 0.5 x1   s.t.  x0 + x1 = 1  AND  x0 + x1 = 2
//
// The commonest infeasibility an SQP linearization produces, and structurally
// unlike (c): there is no FB row anywhere, so F is AFFINE and the whole
// complementarity mechanism is out of the picture. What diverges is the
// EQUALITY multiplier, in the direction the two rows disagree along -- the
// dual-mu block makes K nonsingular, so one Newton step lands at
// lambda = -(Ae Ae^T + mu I)^{-1} be, whose component in Ae Ae^T's null space
// is O(1/mu) = 5e7, and the residual it leaves is exactly the half-gap 0.5.
inline QpProblem inconsistent_equality_qp() {
    QpProblem qp;
    qp.H =
        Eigen::MatrixXd::Identity(2, 2).triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1.0, 0.5;
    Eigen::MatrixXd Aed(2, 2);
    Aed << 1, 1, 1, 1;
    qp.Ae = Aed.sparseView();
    qp.be = Vec(2);
    qp.be << 1.0, 2.0;
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1e20);
    qp.upper = Vec::Constant(2, 1e20);
    return qp;
}

// --- Task-4 fixture (c3): infeasible, and diagnosed by the OTHER route -----
//
// The telemetry has two entry points, and they are not the same test:
//
//   * the STANDING detector, at the top of every attempt -- the stall window
//     filling while the duals diverge, which is the picture of an iteration
//     that keeps ACCEPTING steps and getting nowhere;
//   * the EXHAUSTION route, when the Armijo schedule finds no descent at all
//     while the duals have already diverged.
//
// Fixtures (c) and (c2) both take the exhaustion route. This one takes the
// STANDING route: the line search keeps finding just enough decrease to accept
// (89 backtracks over 13 accepted steps), the residual stalls at 1.33 after
// ten consecutive attempts without a 1% improvement, and the multipliers reach
// 1.3e6. Found by search over infeasible QPs (29 903 of them; 29% take this
// route), which is also how it is known that neither route is decorative.
inline QpProblem slow_infeasible_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1.6, 0.8, 0.8, 4.1;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 2.1, 2.8;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(3, 2);
    Aid << -1.4, 0.9, 0.2, -1.2, -0.2, 2.8;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(3);
    qp.bi << 1.3, -1.5, -0.5;
    qp.lower = Vec::Constant(2, -2.0);
    qp.upper = Vec::Constant(2, 2.0);
    return qp;
}

// --- Task-4 fixture (e): an INDEFINITE Hessian ----------------------------
//
//     min 1/2 x0^2 - x1^2 - 0.5 x0 + 0.25 x1   s.t.  -1 <= x <= 1
//
// Separable, and deliberately so, because it makes the trap arithmetic:
// x0 = 0.5 is the interior minimum of the convex half, while the CONCAVE half
// is minimized only at a bound, at x1 = -1. So (0.5, -1) is the solution (the
// walk finds it in 3 minors, riding negative curvature to the bound) and
// (0.5, 0.125) is an interior stationary point of the SAME KKT system that is
// not a minimizer of anything -- a saddle, at objective -0.109 against the
// solution's -1.375.
//
// **A ROOT-FINDING METHOD CANNOT TELL THEM APART FROM THE RESIDUAL**, which is
// exactly why the inertia gate exists: F vanishes at both.
inline QpProblem indefinite_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1.0, 0.0, 0.0, -2.0;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.5, 0.25;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    qp.Ai.resize(0, 2);
    qp.bi = Vec(0);
    qp.lower = Vec::Constant(2, -1.0);
    qp.upper = Vec::Constant(2, 1.0);
    return qp;
}

// --- Fix-round-1 fixture (f): FEASIBLE, BADLY SCALED, AND FORMERLY REPORTED
// --- INFEASIBLE (review fix round 1, I2; Fable review F2)
//
//     min 1/2 x' [1.4 -0.8; -0.8 1.3] x + [6e5, -2.4e6]' x
//     s.t. [-1.1 0.7; 0.9 1.1] x <= [-0.2, -2.1],   -3 <= x <= 3
//
// **THIS QP HAS A SOLUTION AND THE WALK FINDS IT IN TWO MINOR ITERATIONS**:
// x* = (-0.6793478262, -1.353260869), with multipliers 1.53e6 and 1.21e6.
// Those multipliers are the whole point -- they are two orders above the
// telemetry's kSsnDualGrowthFactor = 1e4, which they reach on the way to their
// own values and never come back from, so the growth conjunct measured against
// the START POINT is satisfied PERMANENTLY on a perfectly feasible subproblem.
// The first implementation therefore reported QpStatus::kInfeasible /
// SsnEscape::kInfeasibleSuspect here, at 4 attempts, on the exhaustion route --
// a WRONG ANSWER of the class the walk reserves for a certificate, and the
// class the Fable review's F2 predicted from the code.
//
// It was found by search rather than constructed: 16 636 instances of exactly
// this shape (feasible, pre-fix false kInfeasible, post-fix kOptimal) exist in
// 60 000 draws of the generator bench/ssn_safeguard_probe.cpp's `routes`/
// false-positive census uses, and this is the smallest and best conditioned of
// them (H's eigenvalues 0.548 and 2.152). Under the reworked conjuncts the
// solve CONVERGES -- 8 accepted steps, agreeing with the walk to 3e-8 -- and
// the reason it converges is that the false diagnosis used to pre-empt the
// proximal ladder that fixes it: the shipped solve takes two rungs.
inline QpProblem badly_scaled_feasible_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1.4, -0.8, -0.8, 1.3;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << 6.0e5, -2.4e6;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1.1, 0.7, 0.9, 1.1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -0.2, -2.1;
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);
    return qp;
}
// --- Fix-round-1 fixture (g): THE CERTIFIED POINT RECLASSIFIES ------------
//
//     min 1/2 x' [2.4 -1 0.6; -1 2.5 1.3; 0.6 1.3 2.5] x + [6e5, -5e5, 9e5]' x
//     s.t. [-0.1 -0.4 -0.8; 0.6 -1.2 -1.1] x <= [1.5, 1.9],  -3 <= x <= 3
//
// Benign: strictly convex, well conditioned, and the walk solves it in 5 minor
// iterations at the vertex x* = (-3, 3, -3). Its ONE interesting property is
// that the partition the second-order verification classifies AT THE CERTIFIED
// POINT differs from the one the last accepted step was taken under -- so it is
// the fixture that can see whether the verification's classification is counted
// as a bulk flip. It must not be (the verification is not a step): shipped
// reports 4 flips over 9 accepted steps, and counting the verification would
// report 5. Found by search over 20 000 draws of
// bench/ssn_safeguard_probe.cpp's generator with the objective scaled 1e6 --
// 16 of them discriminate, and this is the smallest.
inline QpProblem late_reclassification_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(3, 3);
    Hd << 2.4, -1.0, 0.6, -1.0, 2.5, 1.3, 0.6, 1.3, 2.5;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(3);
    qp.g << 6.0e5, -5.0e5, 9.0e5;
    qp.Ae.resize(0, 3);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 3);
    Aid << -0.1, -0.4, -0.8, 0.6, -1.2, -1.1;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1.5, 1.9;
    qp.lower = Vec::Constant(3, -3.0);
    qp.upper = Vec::Constant(3, 3.0);
    return qp;
}

// --- Fix-round-1 fixture (h): FEASIBLE, BRUTALLY SCALED, AND THE TELEMETRY'S
// --- TWO SECOND-LINE GUARDS ARE WHAT KEEP IT FROM BEING CALLED INFEASIBLE
//
//     min 1/2 x' [1.9e-4 -2.18e-2; -2.18e-2 12.56] x + [-2.9e9, 5e8]' x
//     s.t. [-1.2 0.5; -0.3 -1.2] x <= [-1, 1.3],  -3 <= x <= 3
//
// Objective scaled 1e9 AND a Hessian whose eigenvalues span 1.9e-4 to 12.6 --
// the two stresses combined, which is where the fix-round-1 telemetry's
// remaining exposure lives. The walk solves it (8 minor iterations,
// x* = (3, -1.8333)), so it is FEASIBLE; the shipped engine does not solve it
// either, but it says so honestly -- SsnEscape::kNoContraction at 5 accepted
// steps, which Task 5 routes to the walk.
//
// **WHAT IT DISCRIMINATES**: both of the stall window's second-line guards --
// that the window advances on ACCEPTED STEPS rather than attempts, and that a
// window in which sigma moved is DISCARDED. Remove either and this feasible QP
// is reported QpStatus::kInfeasible / SsnEscape::kInfeasibleSuspect. They are
// second-line because the windowed improvement demand and the re-armed growth
// reference already remove the false positives on every gentler population
// measured (docs/notes/2026-08-07-ssn-safeguards.md section 12.2); this cell is
// what shows they are not therefore decorative. 18 of 20 000 draws discriminate
// them at this scaling; this is the smallest.
inline QpProblem brutally_scaled_feasible_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1.9e-4, -2.18e-2, -2.18e-2, 12.56;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -2.9e9, 5.0e8;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -1.2, 0.5, -0.3, -1.2;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -1.0, 1.3;
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);
    return qp;
}

// --- TASK 6b PHASE B fixture (i): A FEASIBLE QP THE SHIPPED SYMPTOM CONJUNCTS
// --- CALL INFEASIBLE, AND R4's FARKAS GATE DOES NOT
//
//     min 1/2 x' [1.4228e6 -1.0272e6; -1.0272e6 1.6433e6] x
//              + [-1.0909e6, -2.9328e6]' x
//     s.t. [0.13517 0.59677; -0.92170 -1.09432] x <= [-0.28016, 2.41144],
//          -3 <= x <= 3
//
// FEASIBLE BY CONSTRUCTION (the right-hand sides were priced against a point
// strictly inside the box) and confirmed so by the WALK, which certifies
// x* = (-0.11554, -0.44154). Drawn from the committed probe's own
// `census`/`lever-farkas` population "feasible, objective x 1e6" -- the 428th
// draw of the shipped seed, and the FIRST draw in that stream on which the
// shipped conjuncts fire. It is the smallest instance (n = 2) of the 0.43%
// false-positive residue docs/notes/2026-08-07-ssn-safeguards.md section 13.6
// carries as a watch item, made into a fixture rather than a rate.
//
// **WHAT IT DISCRIMINATES.** Under the shipped SsnInfeasibilityRule::kSymptoms
// this QP exits QpStatus::kInfeasible / SsnEscape::kInfeasibleSuspect -- a
// WRONG ANSWER of the one class a driver responds to with restoration rather
// than with a bigger budget. Under kFarkasGated the same symptoms arm the
// check, the sign-cone-projected dual increment is NOT an approximate Farkas
// direction, and the exit falls through to the honest budget route. So this
// fixture is the R4 lever's mechanism, executable: it is the cell on which the
// two rules DISAGREE, and it is what a mutant that neuters either half of the
// certificate has to survive.
inline QpProblem false_infeasible_trap_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 1422828.8753147568, -1027172.8141073986, -1027172.8141073986, 1643314.8856351578;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -1090891.26781786, -2932836.8318028217;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << 0.1351674730494965, 0.59676878613923101, -0.92170250531674447, -1.0943153919454645;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -0.280161707865765, 2.4114365738244077;
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);
    return qp;
}

// --- TASK 6b PHASE B fixture (j): A FEASIBLE NARROW STRIP -- THE CELL THAT
// --- SEPARATES THE FARKAS CERTIFICATE'S TWO HALVES
//
// Two inequality rows that are EXACT NEGATIVES of each other,
//
//     a' x <= -1.30229,   -a' x <= 1.67753
//
// i.e. -1.67753 <= a' x <= -1.30229: a FEASIBLE strip of width 0.37524, and
// the walk certifies a point inside it. The objective is scaled 1e6, which is
// what makes the shipped symptom conjuncts fire.
//
// **WHY THIS SHAPE AND NOT ANOTHER.** A Farkas direction needs BOTH
// A' y = 0 (with y >= 0 on the inequality rows) and <b, y> < 0. On a
// near-dependent pair the FIRST is nearly free -- y = (1, 1) gives
// A' y = a - a = 0 EXACTLY -- so the residual conjunct alone cannot tell this
// QP from an infeasible one. What separates them is the second conjunct:
// <b, y> = -1.30229 + 1.67753 = +0.37524, the strip's own width, which is
// POSITIVE exactly because the strip is non-empty. Negate the width and the
// same construction is the `routes` generator's infeasible pair.
//
// So this fixture is the <b, y> < 0 conjunct's reason for existing, executable.
// It was found by search over 400 000 draws of that construction at
// obj x 1e6 (draw 7 of the shipped seed, the first that discriminates), and
// without it a mutant that drops the objective conjunct and keeps only the
// residual one SURVIVES the full unfiltered suite -- measured.
inline QpProblem narrow_strip_feasible_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(4, 4);
    Hd << 4098157.6575607248, 835805.94776384661, 546624.27110317862, 1364517.9085298716,
        835805.94776384661, 3229351.4285035697, 933268.09968722553, -1569845.9854165167,
        546624.27110317862, 933268.09968722553, 4801502.0557977837, 1660392.9217718928,
        1364517.9085298716, -1569845.9854165167, 1660392.9217718928, 3485686.5562725845;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(4);
    qp.g << -31800.509227158269, 1884577.9543742002, -2719733.9696929543, 1488750.9348827912;
    qp.Ae.resize(0, 4);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 4);
    Aid << -1.0027928781626101, -1.4986692248804232, 0.14616229327313013, 0.51021690092071781,
        1.0027928781626101, 1.4986692248804232, -0.14616229327313013, -0.51021690092071781;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << -1.3022941181204235, 1.6775317677930879;
    qp.lower = Vec::Constant(4, -3.0);
    qp.upper = Vec::Constant(4, 3.0);
    return qp;
}

// --- TASK 6b PHASE B fixture (k): AN INFEASIBLE PAIR ON WHICH THE SIGN-CONE
// --- PROJECTION IS LOAD-BEARING -- AND COSTS RECALL
//
//     min 1/2 x' [2.2322 -0.0419; -0.0419 0.5010] x + [-0.2062, 1.2767]' x
//     s.t.  a' x <= 1.46313,  -a' x <= -1.85461,  -3 <= x <= 3
//
// with a = (-0.0708051, -0.534744): the two rows say a' x <= 1.46313 AND
// a' x >= 1.85461, so the QP is INFEASIBLE by a gap of 0.39147. It is the
// probe's own `routes` construction, draw 6633 of the shipped seed at
// obj x 1 -- the first draw on which the projection changes the verdict.
//
// **WHAT IT PINS, AND WHY IT LOOKS LIKE A FAILURE.** Farkas' lemma requires
// y >= 0 on the inequality rows: a "certificate" read off a sign-indefinite
// combination is not a certificate, it is an arbitrary linear combination that
// happens to annihilate A'. Under R4's shipped projection the dual increment
// here leaves the cone, the certificate is REFUSED, and the solve exits on the
// honest numerical route instead of reporting kInfeasible -- so this cell is a
// RECALL LOSS, and it is one of the 8-in-12 248 the arm's 56.9% -> 55.6% trade
// is made of. Drop the projection and the same cell reports kInfeasible again,
// on evidence that is not evidence.
//
// **WHICH READING THOSE THREE NUMBERS ARE** (Phase-B review, minor 3): the
// 20 000-QP `routes` run, which is where this cell was FOUND. They are not the
// committed arm's: `docs/notes/data/2026-08-10-task6b-phaseB/r4_farkas.txt`
// reads 30 000 QPs and 56.5% -> 55.2% (16 940 -> 16 560 diagnosed). Same
// population and same trade, different draw count -- and the artifact's is the
// one to quote.
//
// A LATER RULE WITH BETTER RECALL SHOULD RE-DERIVE THIS CELL, not read its
// assertion as a regression: what is pinned is that the projection is READ, not
// that refusing here is desirable.
inline QpProblem sign_cone_discriminating_infeasible_qp() {
    QpProblem qp;
    Eigen::MatrixXd Hd(2, 2);
    Hd << 2.232240664985274, -0.04187509863132588, -0.04187509863132588, 0.50101228652624785;
    qp.H = Hd.triangularView<Eigen::Upper>().toDenseMatrix().sparseView();
    qp.g = Vec(2);
    qp.g << -0.2061869684716755, 1.2767013916138845;
    qp.Ae.resize(0, 2);
    qp.be = Vec(0);
    Eigen::MatrixXd Aid(2, 2);
    Aid << -0.070805081909206535, -0.5347444481748127, 0.070805081909206535, 0.5347444481748127;
    qp.Ai = Aid.sparseView();
    qp.bi = Vec(2);
    qp.bi << 1.4631335686093674, -1.8546081817609288;
    qp.lower = Vec::Constant(2, -3.0);
    qp.upper = Vec::Constant(2, 3.0);
    return qp;
}
} // namespace hven::solvers::test_support
