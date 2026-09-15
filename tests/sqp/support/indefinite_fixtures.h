// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/indefinite_fixtures.h -- test-support only, NOT part of
// the public library surface.
//
// THE THREE ROW-CARRYING INDEFINITE QPs the SQP suite already owns, factored
// out of tests/sqp/test_qp_engine_indefinite.cpp (where they were written, and
// where their battery still consumes them) so the IPQP tier's A11 cell can
// assert against THE SAME PROBLEMS rather than a second hand-built copy.
//
// WHY THEY MOVED RATHER THAN BEING COPIED. A11 (plan section 8) asks for "HS
// indefinite rows + the parametric IndefiniteBoxModel family", and the row half
// has to be a CONSTRAINED indefinite KKT system -- one whose inertia signature
// counts equality and inequality rows, not just a box. Two copies of a
// hand-derived fixture whose comment carries a page of multiplier arithmetic is
// exactly the drift hazard this tree's one-implementation rule exists for (the
// same reasoning tests/sqp/CMakeLists.txt gives for corpus_cells.h being shared
// between the measurement binary and the assertion that guards it).
//
// Each fixture's own derivation travelled with it verbatim; nothing below was
// re-derived, re-numbered or re-tuned in the move. They are `inline` because a
// header shared by two translation units in the same binary needs them to be;
// nothing else about them changed.

#include <Eigen/Core>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>

namespace hven::solvers::test_support {

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
inline QpProblem indefinite_equality_qp() {
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
inline QpProblem indefinite_equality_and_row_qp() {
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
inline QpProblem two_negative_eigenvalue_row_qp() {
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

} // namespace hven::solvers::test_support
