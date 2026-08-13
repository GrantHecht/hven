#pragma once

// tests/support/hs_problems.h — test-support only, NOT part of the public
// library surface. Twenty-seven problems of the Hock-Schittkowski (HS) test
// collection, transcribed into NlpModel (nlp_model.h) form.
//
// TASK 3 shipped the first six: HS1, HS3, HS5 (bound-constrained only), HS6,
// HS7 (one equality constraint each), and HS76 (three linear inequality
// constraints, QP-like since every constraint and, in HS76's case, the
// objective's Hessian are exactly what eval_hess reports with no
// x-dependence).
//
// TASK 11 added twenty-one more for the whole-solver battery -- see the
// "TASK 11 ADDITIONS" banner further down for the class breakdown, the
// once-and-for-all g(x) >= 0 -> cI(x) <= 0 sign conversion, and the
// two-independent-ports cross-check policy. hs_numbers() is the canonical
// list; tests/test_hs_battery.cpp iterates it.
//
// PRIMARY SOURCE (every problem, cited again per-class below): Hock, W.,
// Schittkowski, K. (1981). "Test Examples for Nonlinear Programming Codes."
// Lecture Notes in Economics and Mathematical Systems 187. Springer-Verlag.
//
// CROSS-CHECKS. Because sign conventions for inequality constraints differ
// across secondary sources (the 1981 book and Schittkowski's later "306 Test
// Problems" collection both state general inequalities as g(x) >= 0; this
// codebase's convention, per nlp_model.h, is cI(x) <= 0 -- every conversion
// below is called out where it applies), each problem's objective/constraint
// formulas were additionally cross-checked against the AMPL model
// transcriptions at vanderbei.princeton.edu/ampl/nlmodels/hs_new/
// (tp001.mod, tp003.mod, tp005.mod, tp006.mod, tp007.mod) and, for HS76,
// github.com/ampl/global-optimization/blob/master/cute/hs076.mod. Each f*
// below was additionally cross-checked against the FEX column of Appendix
// "Individual Results" in Schittkowski, K. (2008), "306 Test Problems for
// Nonlinear Programming with Optimal Solutions -- User's Guide"
// (klaus-schittkowski.de/test_problems.pdf), which lists NLPQLP's converged
// objective against the exact known optimum for every problem, TP column
// keyed to the same 1981 numbering used here. The derivative checker
// (derivative_check.h) is the transcription guard on top of both: every
// model below must pass assert_gradient/assert_jacobians/assert_hessian at
// two points before it is trusted (test_nlp_model.cpp).

#include <cmath>
#include <memory>
#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>
#include <fmt/ranges.h>

#include <hven/detail/sqp/nlp_model.h>

namespace hven::solvers::test_support {

namespace detail {

constexpr double kInf = 1e20; // "effectively infinite" bound, matches
                              // qp_problem.h/dense_oracle.h's convention.

// Builds an n x n SpMatU (upper triangle only) from (row, col, value)
// triples with row <= col.
inline SpMatU make_upper(Index n, std::vector<Eigen::Triplet<double>> triplets) {
    SpMatU H(n, n);
    H.setFromTriplets(triplets.begin(), triplets.end());
    return H;
}

// Builds an m x n row-major Jacobian from (row, col, value) triples. Task 11
// helper: the Task-3 problems each spelled their Jacobian out with J.insert()
// calls, which is unreadable past a handful of entries and, worse, requires
// the inserts to be in row-major order (Eigen's insert() on a compressed-
// insertion path is order-sensitive). setFromTriplets is order-agnostic.
//
// EXPLICIT ZEROS ARE PRESERVED, and every model below relies on it: each
// eval_jac_* emits its FULL STRUCTURAL PATTERN every call -- including entries
// that happen to evaluate to 0.0 at a particular x (e.g. HS40's d(cE2)/dx1 =
// 2*x1*x4 at x1 = 0) -- so the sparsity pattern a solve sees is constant
// across iterates and the QP engine's hot-start reuse is never invalidated by
// a structural change that is really just a numeric coincidence.
inline Eigen::SparseMatrix<double, Eigen::RowMajor>
make_jac(Index m, Index n, std::vector<Eigen::Triplet<double>> triplets) {
    Eigen::SparseMatrix<double, Eigen::RowMajor> J(m, n);
    J.setFromTriplets(triplets.begin(), triplets.end());
    return J;
}

// An m x n all-structural-zeros Jacobian, for the me()==0 / mi()==0 side of a
// problem that only has one kind of general constraint.
inline Eigen::SparseMatrix<double, Eigen::RowMajor> no_jac(Index m, Index n) {
    return Eigen::SparseMatrix<double, Eigen::RowMajor>(m, n);
}

} // namespace detail

// ---------------------------------------------------------------------
// HS1 — Hock & Schittkowski (1981), problem 1.
//
//   f(x) = 100*(x2 - x1^2)^2 + (1 - x1)^2
//   bounds: x1 free, x2 >= -1.5
//   x0 = (-2, 1), f* = 0 at x* = (1, 1)
//
// No constraints beyond the bound on x2 (me = mi = 0): the classic
// Rosenbrock function with one variable's lower bound relaxed to -1.5,
// cross-checked against tp001.mod (`var x_2 >= -3/2`).
// ---------------------------------------------------------------------
class Hs1Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(1) - x(0) * x(0);
        const double b = 1.0 - x(0);
        return 100.0 * a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        const double u = x(1) - x(0) * x(0);
        g(0) = -400.0 * x(0) * u - 2.0 * (1.0 - x(0));
        g(1) = 200.0 * u;
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        // Standard Rosenbrock Hessian: [[1200 x1^2 - 400 x2 + 2, -400 x1], [-400 x1, 200]].
        const double h00 = 1200.0 * x(0) * x(0) - 400.0 * x(1) + 2.0;
        const double h01 = -400.0 * x(0);
        const double h11 = 200.0;
        return detail::make_upper(
            2, {{0, 0, obj_scale * h00}, {0, 1, obj_scale * h01}, {1, 1, obj_scale * h11}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }

    const Vec &lower() const override {
        static const Vec l = (Vec(2) << -detail::kInf, -1.5).finished();
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << -2.0, 1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS3 — Hock & Schittkowski (1981), problem 3.
//
//   f(x) = x2 + 1e-5*(x2 - x1)^2
//   bounds: x1 free, x2 >= 0
//   x0 = (10, 1), f* = 0 at x* = (0, 0)
//
// Cross-checked against tp003.mod.
// ---------------------------------------------------------------------
class Hs3Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double d = x(1) - x(0);
        return x(1) + 1e-5 * d * d;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        const double d = x(1) - x(0);
        g(0) = -2e-5 * d;
        g(1) = 1.0 + 2e-5 * d;
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return detail::make_upper(
            2, {{0, 0, obj_scale * 2e-5}, {0, 1, obj_scale * -2e-5}, {1, 1, obj_scale * 2e-5}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }

    const Vec &lower() const override {
        static const Vec l = (Vec(2) << -detail::kInf, 0.0).finished();
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << 10.0, 1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS5 — Hock & Schittkowski (1981), problem 5.
//
//   f(x) = sin(x1 + x2) + (x1 - x2)^2 - 1.5*x1 + 2.5*x2 + 1
//   bounds: -1.5 <= x1 <= 4, -3 <= x2 <= 3
//   x0 = (0, 0)
//   f* = -1.913222954981036 at x* = (-0.5471975511965977, -1.547197551196598)
//
// Cross-checked against tp005.mod (whose `display` epilogue states the same
// x*/f* to full precision).
// ---------------------------------------------------------------------
class Hs5Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double d = x(0) - x(1);
        return std::sin(x(0) + x(1)) + d * d - 1.5 * x(0) + 2.5 * x(1) + 1.0;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        const double c = std::cos(x(0) + x(1));
        const double d = x(0) - x(1);
        g(0) = c + 2.0 * d - 1.5;
        g(1) = c - 2.0 * d + 2.5;
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        const double s = std::sin(x(0) + x(1));
        const double h00 = -s + 2.0;
        const double h01 = -s - 2.0;
        const double h11 = -s + 2.0;
        return detail::make_upper(
            2, {{0, 0, obj_scale * h00}, {0, 1, obj_scale * h01}, {1, 1, obj_scale * h11}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }

    const Vec &lower() const override {
        static const Vec l = (Vec(2) << -1.5, -3.0).finished();
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(2) << 4.0, 3.0).finished();
        return u;
    }

    Vec start_point() const override { return Vec::Zero(2); }
};

// ---------------------------------------------------------------------
// HS6 — Hock & Schittkowski (1981), problem 6.
//
//   f(x)  = (1 - x1)^2
//   cE(x) = 10*(x2 - x1^2)         [already an equality; no sign conversion]
//   x0 = (-1.2, 1), f* = 0 at x* = (1, 1)
//
// Cross-checked against tp006.mod.
// ---------------------------------------------------------------------
class Hs6Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double b = 1.0 - x(0);
        return b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g(0) = -2.0 * (1.0 - x(0));
        g(1) = 0.0;
        return g;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = 10.0 * (x(1) - x(0) * x(0));
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        // hess(f) = [[2, 0], [0, 0]]; hess(cE) = [[-20, 0], [0, 0]].
        const double le = lambda_e(0);
        return detail::make_upper(2, {{0, 0, obj_scale * 2.0 + le * -20.0}, {1, 1, 0.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J(1, 2);
        J.insert(0, 0) = -20.0 * x(0);
        J.insert(0, 1) = 10.0;
        return J;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << -1.2, 1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS7 — Hock & Schittkowski (1981), problem 7.
//
//   f(x)  = ln(1 + x1^2) - x2
//   cE(x) = (1 + x1^2)^2 + x2^2 - 4      [already an equality]
//   x0 = (2, 2)
//   f* = -sqrt(3) = -1.732050807568877 at x* = (0, sqrt(3))
//
// Cross-checked against tp007.mod.
// ---------------------------------------------------------------------
class Hs7Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return std::log(1.0 + x(0) * x(0)) - x(1); }

    Vec eval_grad(const Vec &x) const override {
        Vec g(2);
        g(0) = 2.0 * x(0) / (1.0 + x(0) * x(0));
        g(1) = -1.0;
        return g;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        const double p = 1.0 + x(0) * x(0);
        c(0) = p * p + x(1) * x(1) - 4.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double x1 = x(0);
        const double p = 1.0 + x1 * x1;
        // hess(f) = [[(2 - 2 x1^2)/(1+x1^2)^2, 0], [0, 0]].
        const double hf00 = (2.0 - 2.0 * x1 * x1) / (p * p);
        // hess(cE) = [[4 + 12 x1^2, 0], [0, 2]].
        const double hc00 = 4.0 + 12.0 * x1 * x1;
        const double hc11 = 2.0;
        const double le = lambda_e(0);
        return detail::make_upper(
            2, {{0, 0, obj_scale * hf00 + le * hc00}, {1, 1, obj_scale * 0.0 + le * hc11}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J(1, 2);
        J.insert(0, 0) = 4.0 * x(0) * (1.0 + x(0) * x(0));
        J.insert(0, 1) = 2.0 * x(1);
        return J;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 2);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};

// ---------------------------------------------------------------------
// HS76 — Hock & Schittkowski (1981), problem 76.
//
//   f(x) = x1^2 + 0.5 x2^2 + x3^2 + 0.5 x4^2 - x1 x3 + x3 x4 - x1 - 3 x2 + x3 - x4
//   bounds: x >= 0 (all four variables), no upper bound
//   HS states the three general constraints as g(x) >= 0:
//     g1 = 5 - x1 - 2 x2 - x3 - x4        >= 0
//     g2 = 4 - 3 x1 - x2 - 2 x3 + x4      >= 0
//     g3 = x2 + 4 x3 - 1.5                >= 0
//   CONVERTED to this codebase's cI(x) <= 0 by negating each (ci = -gi):
//     ci1 = x1 + 2 x2 + x3 + x4 - 5       <= 0
//     ci2 = 3 x1 + x2 + 2 x3 - x4 - 4     <= 0
//     ci3 = 1.5 - x2 - 4 x3               <= 0
//   x0 = (0.5, 0.5, 0.5, 0.5)
//   f* = -4.681818181818182
//     at x* = (0.2727273, 2.090909, 0, 0.5454545) (approximately; the g3
//     constraint's slack variable degenerates to numerically-zero x3)
//
// QP-like: the objective is exactly quadratic and every constraint is
// exactly linear, so eval_hess is CONSTANT in x (equal to obj_scale times
// hess(f); the linear constraints contribute a zero Hessian regardless of
// lambda_i). Cross-checked against
// github.com/ampl/global-optimization/blob/master/cute/hs076.mod, whose
// `<=`-form constraints already match the sign convention derived above
// (that file states them directly as x1+2x2+x3+x4<=5, 3x1+x2+2x3-x4<=4,
// x2+4x3>=1.5 -- the third is left in g(x)>=0 form there and converted here
// exactly as above) and whose commented optimal point matches x* above.
// ---------------------------------------------------------------------
class Hs76Model : public NlpModel {
  public:
    Index n() const override { return 4; }
    Index me() const override { return 0; }
    Index mi() const override { return 3; }

    double eval_f(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2), x4 = x(3);
        return x1 * x1 + 0.5 * x2 * x2 + x3 * x3 + 0.5 * x4 * x4 - x1 * x3 + x3 * x4 - x1 -
               3.0 * x2 + x3 - x4;
    }

    Vec eval_grad(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2), x4 = x(3);
        Vec g(4);
        g(0) = 2.0 * x1 - x3 - 1.0;
        g(1) = x2 - 3.0;
        g(2) = 2.0 * x3 - x1 + x4 + 1.0;
        g(3) = x4 + x3 - 1.0;
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2), x4 = x(3);
        Vec c(3);
        c(0) = x1 + 2.0 * x2 + x3 + x4 - 5.0;
        c(1) = 3.0 * x1 + x2 + 2.0 * x3 - x4 - 4.0;
        c(2) = 1.5 - x2 - 4.0 * x3;
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        // hess(f): constant, all three linear constraints have zero Hessian.
        return detail::make_upper(4, {{0, 0, obj_scale * 2.0},
                                      {0, 2, obj_scale * -1.0},
                                      {1, 1, obj_scale * 1.0},
                                      {2, 2, obj_scale * 2.0},
                                      {2, 3, obj_scale * 1.0},
                                      {3, 3, obj_scale * 1.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return Eigen::SparseMatrix<double, Eigen::RowMajor>(0, 4);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        Eigen::SparseMatrix<double, Eigen::RowMajor> J(3, 4);
        J.insert(0, 0) = 1.0;
        J.insert(0, 1) = 2.0;
        J.insert(0, 2) = 1.0;
        J.insert(0, 3) = 1.0;
        J.insert(1, 0) = 3.0;
        J.insert(1, 1) = 1.0;
        J.insert(1, 2) = 2.0;
        J.insert(1, 3) = -1.0;
        J.insert(2, 1) = -1.0;
        J.insert(2, 2) = -4.0;
        return J;
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(4, 0.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(4, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(4, 0.5); }
};

// =====================================================================
// TASK 11 ADDITIONS -- the Hock-Schittkowski battery.
//
// Twenty-one further problems, spanning the three classes the battery's
// brief names: bounds-only (25, 38, 45), equality-constrained (26, 27, 28,
// 39, 40, 77, 79) and inequality-constrained (10, 11, 12, 14, 15, 22, 24,
// 30, 33, 35, 43). Together with the six above the set is 27 problems.
//
// SIGN CONVERSION, ONCE, FOR ALL OF THEM. Hock & Schittkowski state every
// general inequality as g(x) >= 0; this codebase's convention (nlp_model.h)
// is cI(x) <= 0. Every inequality below is therefore the NEGATION of the
// published row, ci = -g, and each model's comment gives BOTH forms so the
// conversion is checkable against the source without re-deriving it.
//
// TRANSCRIPTION SOURCES. Primary: Hock & Schittkowski (1981) as cited at the
// top of this file. Each problem's formulas were additionally cross-checked
// against a machine-readable transcription -- Robert Vanderbei's AMPL ports
// at vanderbei.princeton.edu/ampl/nlmodels/, in two collections: `hs_new/`
// (tpNNN.mod, "revised by Stephan Seidl") and `hs/` (hsNNN.mod).
//
// THE TWO PORTS AGREE ON EVERY FORMULA SHIPPED HERE. No transcription
// disagreement was found between them, and none of the problems below is
// annotated with one. Where they DO differ it is by DELIBERATE REVISION --
// the `r`/`v` suffixed files exist to make a problem easier or better posed
// than the published one -- and those differences are real, load-bearing, and
// called out per-problem, because taking a revised variant for the published
// problem would silently change what is under test:
//
//   HS26: tp026r.mod adds x3 >= -0.4, which suppresses the SECOND solution
//         branch. Not taken here -- the unrevised problem is the fixture.
//   HS33: tp033r.mod lifts x2's lower bound from 0 to 1e-5 and starts x2
//         there. Not taken; hs/hs033.mod's unrevised x2 >= 0 is used.
//   HS40: tp040r.mod adds x3 >= 0. Not taken.
//   HS45: tp045r.mod lifts every lower bound to 1e-4, and hs/hs045.mod
//         starts at the ORIGIN instead of the published (2,2,2,2,2) -- where
//         the gradient of a five-way product vanishes identically, so the
//         origin is an exact stationary point at f = 2. Neither taken.
//
// So the reason to read two ports is not that one of them is wrong; it is
// that "the AMPL port of HSnn" is not a single well-defined thing, and the
// f* self-check below is what confirms which variant a transcription
// actually implements.
//
// f* IS NEVER TRUSTED FROM THE CITATION ALONE. tests/test_hs_battery.cpp
// re-derives a full KKT quadruple from the MODEL at each returned point and
// asserts on that; the cited f* is a cross-check on top, not the guard.
// Where an f* has an exact closed form it is STORED in that form rather than
// as a hand-typed decimal (HS14: 9 - 2.875*sqrt(7); HS33: sqrt(2) - 6;
// HS35: 1/9; HS7: -sqrt(3)) -- not because the published decimals are wrong,
// but because a transcribed decimal is one more thing that can be got wrong
// and a closed form cannot.
// =====================================================================

// ---------------------------------------------------------------------
// HS10 — Hock & Schittkowski (1981), problem 10.
//
//   f(x)  = x1 - x2
//   g1(x) = -3 x1^2 + 2 x1 x2 - x2^2 + 1 >= 0
//     -> ci1 = 3 x1^2 - 2 x1 x2 + x2^2 - 1 <= 0
//   no bounds
//   x0 = (-10, 10), f* = -1 at x* = (0, 1)
//
// A LINEAR OBJECTIVE, so hess(f) == 0 and the subproblem's whole Hessian is
// lambda_i * hess(cI1) -- indefinite whenever lambda_i < 0 is priced during
// the iteration, which makes this the battery's cheapest test that the
// engine's indefinite path is reachable from the driver. Cross-checked
// against hs_new/tp010.mod.
// ---------------------------------------------------------------------
class Hs10Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return x(0) - x(1); }

    Vec eval_grad(const Vec &) const override { return (Vec(2) << 1.0, -1.0).finished(); }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = 3.0 * x(0) * x(0) - 2.0 * x(0) * x(1) + x(1) * x(1) - 1.0;
        return c;
    }

    SpMatU eval_hess(const Vec &, double, const Vec &, const Vec &lambda_i) const override {
        const double li = lambda_i(0);
        // hess(f) = 0; hess(cI1) = [[6, -2], [-2, 2]].
        return detail::make_upper(2, {{0, 0, li * 6.0}, {0, 1, li * -2.0}, {1, 1, li * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(
            1, 2, {{0, 0, 6.0 * x(0) - 2.0 * x(1)}, {0, 1, -2.0 * x(0) + 2.0 * x(1)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << -10.0, 10.0).finished(); }
};

// ---------------------------------------------------------------------
// HS11 — Hock & Schittkowski (1981), problem 11.
//
//   f(x)  = (x1 - 5)^2 + x2^2 - 25
//   g1(x) = -x1^2 + x2 >= 0   ->  ci1 = x1^2 - x2 <= 0
//   no bounds
//   x0 = (4.9, 0.1)
//   f* = -8.498464223 at x* = (1.234769, 1.524653)
//
// f* RE-DERIVED HERE (the citation's 10 digits are confirmed, not replaced):
// eliminating lambda from the KKT system leaves 2 x1^3 + x1 - 5 = 0, whose
// root is x1 = 1.2347728250533 with x2 = x1^2 = 1.5246639294901 and f =
// -8.4984642231547. The cited -8.498464223 agrees to all 10 quoted digits.
// Cross-checked against hs_new/tp011.mod.
// ---------------------------------------------------------------------
class Hs11Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        const double d = x(0) - 5.0;
        return d * d + x(1) * x(1) - 25.0;
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(2) << 2.0 * (x(0) - 5.0), 2.0 * x(1)).finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) * x(0) - x(1);
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        // hess(f) = diag(2, 2); hess(cI1) = diag(2, 0).
        return detail::make_upper(
            2, {{0, 0, obj_scale * 2.0 + lambda_i(0) * 2.0}, {1, 1, obj_scale * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(1, 2, {{0, 0, 2.0 * x(0)}, {0, 1, -1.0}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << 4.9, 0.1).finished(); }
};

// ---------------------------------------------------------------------
// HS12 — Hock & Schittkowski (1981), problem 12.
//
//   f(x)  = 0.5 x1^2 + x2^2 - x1 x2 - 7 x1 - 7 x2
//   g1(x) = 25 - 4 x1^2 - x2^2 >= 0  ->  ci1 = 4 x1^2 + x2^2 - 25 <= 0
//   no bounds
//   x0 = (0, 0), f* = -30 at x* = (2, 3)  [the constraint is ACTIVE there:
//   4*4 + 9 = 25]
//
// Cross-checked against hs_new/tp012.mod.
// ---------------------------------------------------------------------
class Hs12Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        return 0.5 * x(0) * x(0) + x(1) * x(1) - x(0) * x(1) - 7.0 * x(0) - 7.0 * x(1);
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(2) << x(0) - x(1) - 7.0, 2.0 * x(1) - x(0) - 7.0).finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = 4.0 * x(0) * x(0) + x(1) * x(1) - 25.0;
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        // hess(f) = [[1, -1], [-1, 2]]; hess(cI1) = diag(8, 2).
        const double li = lambda_i(0);
        return detail::make_upper(2, {{0, 0, obj_scale * 1.0 + li * 8.0},
                                      {0, 1, obj_scale * -1.0},
                                      {1, 1, obj_scale * 2.0 + li * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(1, 2, {{0, 0, 8.0 * x(0)}, {0, 1, 2.0 * x(1)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Zero(2); }
};

// ---------------------------------------------------------------------
// HS14 — Hock & Schittkowski (1981), problem 14.
//
//   f(x)  = (x1 - 2)^2 + (x2 - 1)^2
//   cE(x) = x1 - 2 x2 + 1                      [already an equality]
//   g1(x) = -x1^2/4 - x2^2 + 1 >= 0  ->  ci1 = x1^2/4 + x2^2 - 1 <= 0
//   no bounds
//   x0 = (2, 2)
//   f* = 9 - 2.875*sqrt(7) = 1.3934649806893 at
//     x* = ((sqrt(7) - 1)/2, (sqrt(7) + 1)/4) = (0.82287566, 0.91143783)
//
// f* IS STORED IN CLOSED FORM rather than as a decimal, and the closed form
// was re-derived here rather than copied: substituting the equality
// x1 = 2 x2 - 1 turns the inequality into 2 x2^2 - x2 - 0.75 <= 0, active at
// its upper root x2 = (1 + sqrt(7))/4, while the unconstrained minimum of the
// reduced objective 5 x2^2 - 14 x2 + 10 sits at x2 = 1.4 -- outside it -- so
// the inequality binds and f there is exactly 9 - 2.875*sqrt(7) =
// 1.393464980689302. hs_new/tp014.mod's epilogue carries the same value to
// all sixteen digits (`display obj - 1.393464980689302;`), so the closed form
// and the cited port agree exactly; storing the closed form just removes a
// hand-typed decimal as a thing that could be got wrong.
//
// BOTH CONSTRAINTS ARE ACTIVE at x*, so this is also the battery's smallest
// fixture with a fully determined (n = 2 active rows) active set.
// Cross-checked against hs_new/tp014.mod.
// ---------------------------------------------------------------------
class Hs14Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 1; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 2.0, b = x(1) - 1.0;
        return a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(2) << 2.0 * (x(0) - 2.0), 2.0 * (x(1) - 1.0)).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) - 2.0 * x(1) + 1.0;
        return c;
    }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = 0.25 * x(0) * x(0) + x(1) * x(1) - 1.0;
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        // hess(f) = diag(2, 2); hess(cE) = 0 (linear); hess(cI1) = diag(0.5, 2).
        const double li = lambda_i(0);
        return detail::make_upper(
            2, {{0, 0, obj_scale * 2.0 + li * 0.5}, {1, 1, obj_scale * 2.0 + li * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::make_jac(1, 2, {{0, 0, 1.0}, {0, 1, -2.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(1, 2, {{0, 0, 0.5 * x(0)}, {0, 1, 2.0 * x(1)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};

// ---------------------------------------------------------------------
// HS15 — Hock & Schittkowski (1981), problem 15.
//
//   f(x)  = 100 (x2 - x1^2)^2 + (1 - x1)^2       [Rosenbrock again]
//   g1(x) = x1 x2 - 1     >= 0  ->  ci1 = 1 - x1 x2      <= 0
//   g2(x) = x1 + x2^2     >= 0  ->  ci2 = -x1 - x2^2     <= 0
//   bounds: x1 <= 0.5, x2 free
//   x0 = (-2, 1), f* = 306.5 at x* = (0.5, 2)
//
// THE START POINT IS INFEASIBLE for ci1 (1 - (-2)(1) = 3 > 0), which is the
// point of the fixture: it is the battery's clearest test of the funnel's
// h-type acceptance and of the elastic tier, since the linearization at
// (-2, 1) has to make real progress on the violation before the objective
// matters at all. Cross-checked against hs_new/tp015.mod.
// ---------------------------------------------------------------------
class Hs15Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override {
        const double a = x(1) - x(0) * x(0), b = 1.0 - x(0);
        return 100.0 * a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        const double u = x(1) - x(0) * x(0);
        return (Vec(2) << -400.0 * x(0) * u - 2.0 * (1.0 - x(0)), 200.0 * u).finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c(0) = 1.0 - x(0) * x(1);
        c(1) = -x(0) - x(1) * x(1);
        return c;
    }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        // hess(f): Rosenbrock's. hess(cI1) = [[0, -1], [-1, 0]].
        // hess(cI2) = [[0, 0], [0, -2]].
        const double h00 = 1200.0 * x(0) * x(0) - 400.0 * x(1) + 2.0;
        const double h01 = -400.0 * x(0);
        return detail::make_upper(2, {{0, 0, obj_scale * h00},
                                      {0, 1, obj_scale * h01 + lambda_i(0) * -1.0},
                                      {1, 1, obj_scale * 200.0 + lambda_i(1) * -2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(2, 2,
                                {{0, 0, -x(1)}, {0, 1, -x(0)}, {1, 0, -1.0}, {1, 1, -2.0 * x(1)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(2) << 0.5, detail::kInf).finished();
        return u;
    }

    Vec start_point() const override { return (Vec(2) << -2.0, 1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS22 — Hock & Schittkowski (1981), problem 22.
//
//   f(x)  = (x1 - 2)^2 + (x2 - 1)^2
//   g1(x) = -x1 - x2 + 2 >= 0  ->  ci1 = x1 + x2 - 2 <= 0
//   g2(x) = -x1^2 + x2   >= 0  ->  ci2 = x1^2 - x2   <= 0
//   no bounds
//   x0 = (2, 2), f* = 1 at x* = (1, 1)   [BOTH constraints active]
//
// Cross-checked against hs_new/tp022.mod.
// ---------------------------------------------------------------------
class Hs22Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 2.0, b = x(1) - 1.0;
        return a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(2) << 2.0 * (x(0) - 2.0), 2.0 * (x(1) - 1.0)).finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c(0) = x(0) + x(1) - 2.0;
        c(1) = x(0) * x(0) - x(1);
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        // hess(cI1) = 0 (linear); hess(cI2) = diag(2, 0).
        return detail::make_upper(
            2, {{0, 0, obj_scale * 2.0 + lambda_i(1) * 2.0}, {1, 1, obj_scale * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(2, 2, {{0, 0, 1.0}, {0, 1, 1.0}, {1, 0, 2.0 * x(0)}, {1, 1, -1.0}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(2, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(2, 2.0); }
};

// ---------------------------------------------------------------------
// HS24 — Hock & Schittkowski (1981), problem 24.
//
//   f(x)  = ((x1 - 3)^2 - 9) x2^3 / (27 sqrt(3))
//   g1(x) = x1/sqrt(3) - x2      >= 0  ->  ci1 = x2 - x1/sqrt(3)      <= 0
//   g2(x) = x1 + sqrt(3) x2      >= 0  ->  ci2 = -x1 - sqrt(3) x2     <= 0
//   g3(x) = -x1 - sqrt(3) x2 + 6 >= 0  ->  ci3 = x1 + sqrt(3) x2 - 6  <= 0
//   bounds: x1 >= 0, x2 >= 0
//   x0 = (1, 0.5), f* = -1 at x* = (3, sqrt(3))
//
// ALL THREE GENERAL ROWS ARE LINEAR and the objective is a cubic in x2 times
// a quadratic in x1, so this is the battery's "polyhedral feasible set,
// genuinely nonconvex objective" case -- the QP subproblem's H is indefinite
// at the start point (d2f/dx2^2 = ((x1-3)^2 - 9) * 6 x2 / (27 sqrt 3) < 0
// there) with an exactly-linear feasible region, which is the combination
// the inertia probe is for. Cross-checked against hs_new/tp024.mod.
// ---------------------------------------------------------------------
class Hs24Model : public NlpModel {
  public:
    Index n() const override { return 2; }
    Index me() const override { return 0; }
    Index mi() const override { return 3; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 3.0;
        return kScale * (a * a - 9.0) * x(1) * x(1) * x(1);
    }

    Vec eval_grad(const Vec &x) const override {
        const double a = x(0) - 3.0;
        const double x2 = x(1);
        return (Vec(2) << kScale * 2.0 * a * x2 * x2 * x2, kScale * (a * a - 9.0) * 3.0 * x2 * x2)
            .finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(3);
        c(0) = x(1) - x(0) / kSqrt3;
        c(1) = -x(0) - kSqrt3 * x(1);
        c(2) = x(0) + kSqrt3 * x(1) - 6.0;
        return c;
    }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        // Every general row is LINEAR, so the multipliers contribute nothing.
        const double a = x(0) - 3.0;
        const double x2 = x(1);
        const double h00 = kScale * 2.0 * x2 * x2 * x2;
        const double h01 = kScale * 6.0 * a * x2 * x2;
        const double h11 = kScale * (a * a - 9.0) * 6.0 * x2;
        return detail::make_upper(
            2, {{0, 0, obj_scale * h00}, {0, 1, obj_scale * h01}, {1, 1, obj_scale * h11}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 2);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::make_jac(3, 2,
                                {{0, 0, -1.0 / kSqrt3},
                                 {0, 1, 1.0},
                                 {1, 0, -1.0},
                                 {1, 1, -kSqrt3},
                                 {2, 0, 1.0},
                                 {2, 1, kSqrt3}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Zero(2);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(2, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(2) << 1.0, 0.5).finished(); }

  private:
    // Not `static constexpr` members: std::sqrt is not constexpr before C++26.
    inline static const double kSqrt3 = std::sqrt(3.0);
    inline static const double kScale = 1.0 / (27.0 * kSqrt3);
};

// ---------------------------------------------------------------------
// HS25 — Hock & Schittkowski (1981), problem 25.
//
//   f(x) = sum_{i=1}^{99} ( -0.01 i + exp(-(u_i - x2)^x3 / x1) )^2
//   with u_i = 25 + (-50 ln(0.01 i))^(2/3)
//   bounds: 0.1 <= x1 <= 100, 0 <= x2 <= 25.6, 0 <= x3 <= 5
//   x0 = (100, 12.5, 3), f* = 0 at x* = (50, 25, 1.5)
//
// THE BATTERY'S DELIBERATE TRAP, and it is shipped BECAUSE it is one. Every
// exponential is numerically dead at the published start point: the smallest
// (u_i - 12.5)^3 / 100 over i is ~22.6, so every exp() term is <= 1.5e-10,
// the objective is a constant 32.8 to fourteen digits, and its gradient is
// ~1e-11 in every coordinate -- BELOW any sane kkt_tol. A first-order method
// starting there is at a numerically exact stationary point of a function
// whose true minimum is 0, and no globalization, trust region or correction
// can change that, because the information needed to escape is below the
// double-precision noise floor of f itself (eps * 32.8 / h ~ 3.6e-9 at
// h = 1e-6, i.e. two orders ABOVE the true gradient). Schittkowski's own
// commentary makes the same point -- codes that "master #25" do so via an
// internal scaling mechanism, not via the iteration.
//
// So the battery EXCUSES it, pins the observed kOptimal-at-the-start-point
// outcome, and documents it; see docs/notes/2026-07-29-hs-battery-results.md.
// Its value here is exactly that: a certified-KKT point that is NOT the cited
// f*, which is the case a battery must be able to tell apart from a bug.
//
// Cross-checked against JuliaSmoothOptimizers/OptimizationProblems.jl's
// ADNLPProblems/hs25.jl (which carries the same u_i, bounds and start point);
// hs_new/ ships only revised variants (tp025v1..v3) of this problem, never
// the unrevised form, which is itself corroboration that the unrevised form
// is a trap.
// ---------------------------------------------------------------------
class Hs25Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        double sum = 0.0;
        for (int i = 1; i <= kM; ++i) {
            const double r = residual(x, i);
            sum += r * r;
        }
        return sum;
    }

    Vec eval_grad(const Vec &x) const override {
        Vec g = Vec::Zero(3);
        for (int i = 1; i <= kM; ++i) {
            Terms t;
            const double r = terms(x, i, t);
            // df_i = e * dt (chain rule through t = -(u-x2)^x3 / x1), and
            // dF = sum 2 f_i df_i.
            for (int j = 0; j < 3; ++j) {
                g(j) += 2.0 * r * t.e * t.dt[j];
            }
        }
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        double h[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
        for (int i = 1; i <= kM; ++i) {
            Terms t;
            const double r = terms(x, i, t);
            for (int a = 0; a < 3; ++a) {
                for (int b = a; b < 3; ++b) {
                    // d2(f_i) = e * (dt dt^T + d2t); d2F = sum 2 (df df^T + f d2f).
                    const double d2fi = t.e * (t.dt[a] * t.dt[b] + t.d2t[a][b]);
                    const double dfa = t.e * t.dt[a];
                    const double dfb = t.e * t.dt[b];
                    h[a][b] += 2.0 * (dfa * dfb + r * d2fi);
                }
            }
        }
        std::vector<Eigen::Triplet<double>> trips;
        for (int a = 0; a < 3; ++a) {
            for (int b = a; b < 3; ++b) {
                trips.emplace_back(a, b, obj_scale * h[a][b]);
            }
        }
        return detail::make_upper(3, std::move(trips));
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 3);
    }

    const Vec &lower() const override {
        static const Vec l = (Vec(3) << 0.1, 0.0, 0.0).finished();
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(3) << 100.0, 25.6, 5.0).finished();
        return u;
    }

    Vec start_point() const override { return (Vec(3) << 100.0, 12.5, 3.0).finished(); }

  private:
    static constexpr int kM = 99;

    // u_i = 25 + (-50 ln(0.01 i))^(2/3), built once. Every u_i is >= 25.63,
    // so u_i - x2 > 0 over the whole box (x2 <= 25.6) and the fractional
    // power (u_i - x2)^x3 and its logarithm are both well defined.
    static const std::vector<double> &u_table() {
        static const std::vector<double> u = [] {
            std::vector<double> v(kM + 1, 0.0);
            for (int i = 1; i <= kM; ++i) {
                v[static_cast<std::size_t>(i)] =
                    25.0 + std::pow(-50.0 * std::log(0.01 * i), 2.0 / 3.0);
            }
            return v;
        }();
        return u;
    }

    // First and second derivatives of t(x) = -(u_i - x2)^x3 / x1, plus
    // e = exp(t). Indices: 0 = x1, 1 = x2, 2 = x3.
    struct Terms {
        double e = 0.0;
        double dt[3] = {0.0, 0.0, 0.0};
        double d2t[3][3] = {{0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}};
    };

    static double residual(const Vec &x, int i) {
        const double a = u_table()[static_cast<std::size_t>(i)] - x(1);
        return -0.01 * i + std::exp(-std::pow(a, x(2)) / x(0));
    }

    // Fills `out` and returns the residual f_i = -0.01 i + exp(t).
    static double terms(const Vec &x, int i, Terms &out) {
        const double x1 = x(0), x3 = x(2);
        const double a = u_table()[static_cast<std::size_t>(i)] - x(1);
        const double la = std::log(a);
        const double p = std::pow(a, x3); // (u - x2)^x3
        const double pm1 = std::pow(a, x3 - 1.0);
        const double pm2 = std::pow(a, x3 - 2.0);
        const double t = -p / x1;
        out.e = std::exp(t);

        out.dt[0] = p / (x1 * x1);
        out.dt[1] = x3 * pm1 / x1;
        out.dt[2] = -p * la / x1;

        out.d2t[0][0] = -2.0 * p / (x1 * x1 * x1);
        out.d2t[0][1] = -x3 * pm1 / (x1 * x1);
        out.d2t[0][2] = p * la / (x1 * x1);
        out.d2t[1][1] = -x3 * (x3 - 1.0) * pm2 / x1;
        out.d2t[1][2] = pm1 * (1.0 + x3 * la) / x1;
        out.d2t[2][2] = -p * la * la / x1;
        out.d2t[1][0] = out.d2t[0][1];
        out.d2t[2][0] = out.d2t[0][2];
        out.d2t[2][1] = out.d2t[1][2];

        return -0.01 * i + out.e;
    }
};

// ---------------------------------------------------------------------
// HS26 — Hock & Schittkowski (1981), problem 26.
//
//   f(x)  = (x1 - x2)^2 + (x2 - x3)^4
//   cE(x) = (1 + x2^2) x1 + x3^4 - 3           [already an equality]
//   no bounds
//   x0 = (-2.6, 2, 2), f* = 0 at x* = (1, 1, 1)
//
// DEGENERATE ON PURPOSE. f* = 0 needs x1 = x2 = x3 = a with
// a^4 + a^3 + a - 3 = 0, which factors as (a - 1)(a^3 + 2a^2 + 2a + 3) and so
// has a SECOND real root at a = -1.8105357138 -- i.e. the solution set is two
// isolated points, both with f = 0, and hs_new/tp026r.mod exists precisely to
// suppress the second one (it adds x3 >= -0.4, which this transcription does
// NOT: the unrevised problem is what is under test). On top of that the
// quartic (x2 - x3)^4 makes hess(f) SINGULAR at either solution, so the local
// convergence rate is linear rather than quadratic and the KKT residual tail
// is long -- which is the property the battery is buying here.
// Cross-checked against hs_new/tp026r.mod (formulas identical; only that
// file's added bound differs).
// ---------------------------------------------------------------------
class Hs26Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - x(1), b = x(1) - x(2);
        return a * a + b * b * b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        const double a = x(0) - x(1), b = x(1) - x(2);
        const double b3 = 4.0 * b * b * b;
        return (Vec(3) << 2.0 * a, -2.0 * a + b3, -b3).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = (1.0 + x(1) * x(1)) * x(0) + x(2) * x(2) * x(2) * x(2) - 3.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double b2 = 12.0 * (x(1) - x(2)) * (x(1) - x(2));
        const double le = lambda_e(0);
        // hess(cE): d2/dx1dx2 = 2 x2, d2/dx2^2 = 2 x1, d2/dx3^2 = 12 x3^2.
        return detail::make_upper(3, {{0, 0, obj_scale * 2.0},
                                      {0, 1, obj_scale * -2.0 + le * 2.0 * x(1)},
                                      {0, 2, 0.0},
                                      {1, 1, obj_scale * (2.0 + b2) + le * 2.0 * x(0)},
                                      {1, 2, obj_scale * -b2},
                                      {2, 2, obj_scale * b2 + le * 12.0 * x(2) * x(2)}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return detail::make_jac(1, 3,
                                {{0, 0, 1.0 + x(1) * x(1)},
                                 {0, 1, 2.0 * x(1) * x(0)},
                                 {0, 2, 4.0 * x(2) * x(2) * x(2)}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 3);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(3, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(3, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(3) << -2.6, 2.0, 2.0).finished(); }
};

// ---------------------------------------------------------------------
// HS27 — Hock & Schittkowski (1981), problem 27.
//
//   f(x)  = 0.01 (x1 - 1)^2 + (x2 - x1^2)^2
//   cE(x) = x1 + x3^2 + 1                      [already an equality]
//   no bounds
//   x0 = (2, 2, 2), f* = 0.04 at x* = (-1, 1, 0)
//
// x3 APPEARS ONLY SQUARED AND ONLY IN THE CONSTRAINT, and its value at the
// solution is 0 -- so the equality Jacobian's x3 column, 2 x3, VANISHES at
// x*. LICQ HOLDS regardless (the row's x1 entry is the constant 1, so this
// single-row Jacobian is nonzero everywhere, including at x*, and the
// equality multiplier is unique). What degenerates is narrower than a
// constraint qualification: the x3 direction drops out of the active
// Jacobian entirely, so nothing in cE's linearization prices a first-order
// move in x3 AT the solution, even though nothing else about the constraint
// qualification is at stake. That is the property under test: a constraint-
// Jacobian column that vanishes exactly at the solution while the row itself
// stays full rank throughout. Cross-checked against hs_new/tp027.mod.
// ---------------------------------------------------------------------
class Hs27Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 1.0, b = x(1) - x(0) * x(0);
        return 0.01 * a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        const double b = x(1) - x(0) * x(0);
        return (Vec(3) << 0.02 * (x(0) - 1.0) - 4.0 * x(0) * b, 2.0 * b, 0.0).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(2) * x(2) + 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double h00 = 0.02 + 12.0 * x(0) * x(0) - 4.0 * x(1);
        return detail::make_upper(3, {{0, 0, obj_scale * h00},
                                      {0, 1, obj_scale * -4.0 * x(0)},
                                      {1, 1, obj_scale * 2.0},
                                      {2, 2, lambda_e(0) * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return detail::make_jac(1, 3, {{0, 0, 1.0}, {0, 1, 0.0}, {0, 2, 2.0 * x(2)}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 3);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(3, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(3, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(3, 2.0); }
};

// ---------------------------------------------------------------------
// HS28 — Hock & Schittkowski (1981), problem 28.
//
//   f(x)  = (x1 + x2)^2 + (x2 + x3)^2
//   cE(x) = x1 + 2 x2 + 3 x3 - 1               [already an equality]
//   no bounds
//   x0 = (-4, 1, 1), f* = 0 at x* = (0.5, -0.5, 0.5)
//
// AN EXACT EQUALITY-CONSTRAINED QP: convex quadratic objective, one linear
// equality, no inequalities and no bounds. The linearization is EXACT, so
// SQP must solve it in ONE major from any start point -- which makes this the
// battery's control fixture, the one whose cost is a floor rather than a
// measurement. Cross-checked against hs_new/tp028.mod.
// ---------------------------------------------------------------------
class Hs28Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 1; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) + x(1), b = x(1) + x(2);
        return a * a + b * b;
    }

    Vec eval_grad(const Vec &x) const override {
        const double a = x(0) + x(1), b = x(1) + x(2);
        return (Vec(3) << 2.0 * a, 2.0 * a + 2.0 * b, 2.0 * b).finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + 2.0 * x(1) + 3.0 * x(2) - 1.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return detail::make_upper(3, {{0, 0, obj_scale * 2.0},
                                      {0, 1, obj_scale * 2.0},
                                      {1, 1, obj_scale * 4.0},
                                      {1, 2, obj_scale * 2.0},
                                      {2, 2, obj_scale * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::make_jac(1, 3, {{0, 0, 1.0}, {0, 1, 2.0}, {0, 2, 3.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 3);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(3, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(3, detail::kInf);
        return u;
    }

    Vec start_point() const override { return (Vec(3) << -4.0, 1.0, 1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS30 — Hock & Schittkowski (1981), problem 30.
//
//   f(x)  = x1^2 + x2^2 + x3^2
//   g1(x) = x1^2 + x2^2 - 1 >= 0  ->  ci1 = 1 - x1^2 - x2^2 <= 0
//   bounds: 1 <= x1 <= 10, -10 <= x2 <= 10, -10 <= x3 <= 10
//   x0 = (1, 1, 1), f* = 1 at x* = (1, 0, 0)
//
// THE GENERAL INEQUALITY IS REDUNDANT AT x*: x1 >= 1 already implies
// x1^2 + x2^2 >= 1, so at the solution the row is active with a multiplier
// that may legitimately be 0 -- weak complementarity, and a case where the
// active-set logic can pick either of two working sets and be right.
// Cross-checked against hs_new/tp030.mod.
// ---------------------------------------------------------------------
class Hs30Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return x.squaredNorm(); }

    Vec eval_grad(const Vec &x) const override { return 2.0 * x; }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = 1.0 - x(0) * x(0) - x(1) * x(1);
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        const double li = lambda_i(0);
        return detail::make_upper(3, {{0, 0, obj_scale * 2.0 + li * -2.0},
                                      {1, 1, obj_scale * 2.0 + li * -2.0},
                                      {2, 2, obj_scale * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(1, 3, {{0, 0, -2.0 * x(0)}, {0, 1, -2.0 * x(1)}, {0, 2, 0.0}});
    }

    const Vec &lower() const override {
        static const Vec l = (Vec(3) << 1.0, -10.0, -10.0).finished();
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(3) << 10.0, 10.0, 10.0).finished();
        return u;
    }

    Vec start_point() const override { return Vec::Constant(3, 1.0); }
};

// ---------------------------------------------------------------------
// HS33 — Hock & Schittkowski (1981), problem 33.
//
//   f(x)  = (x1 - 1)(x1 - 2)(x1 - 3) + x3
//   g1(x) = x3^2 - x2^2 - x1^2          >= 0
//     -> ci1 = x1^2 + x2^2 - x3^2       <= 0
//   g2(x) = x1^2 + x2^2 + x3^2 - 4      >= 0
//     -> ci2 = 4 - x1^2 - x2^2 - x3^2   <= 0
//   bounds: x1 >= 0, x2 >= 0, 0 <= x3 <= 5
//   x0 = (0, 0, 3), f* = sqrt(2) - 6 = -4.5857864376269 at
//     x* = (0, sqrt(2), sqrt(2))
//
// ci2 IS A REVERSE-CONVEX ROW (the feasible side is the OUTSIDE of a ball),
// so its Hessian contribution is negative definite whenever its multiplier is
// positive -- the battery's most reliable source of an indefinite subproblem
// Hessian with a nonzero multiplier, as opposed to HS10/HS24's indefiniteness
// which comes from the objective. Cross-checked against
// vanderbei .../hs/hs033.mod (which uses the unrevised x2 >= 0 bound and the
// published x0 = (0, 0, 3)); hs_new/tp033r.mod raises x2's lower bound to
// 1e-5 and starts x2 there instead, a revision this transcription does not
// take.
// ---------------------------------------------------------------------
class Hs33Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 0; }
    Index mi() const override { return 2; }

    double eval_f(const Vec &x) const override {
        const double a = x(0);
        return (a - 1.0) * (a - 2.0) * (a - 3.0) + x(2);
    }

    Vec eval_grad(const Vec &x) const override {
        const double a = x(0);
        // d/dx1 of a^3 - 6a^2 + 11a - 6.
        return (Vec(3) << 3.0 * a * a - 12.0 * a + 11.0, 0.0, 1.0).finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(2);
        c(0) = x(0) * x(0) + x(1) * x(1) - x(2) * x(2);
        c(1) = 4.0 - x(0) * x(0) - x(1) * x(1) - x(2) * x(2);
        return c;
    }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        const double l1 = lambda_i(0), l2 = lambda_i(1);
        // hess(cI1) = diag(2, 2, -2); hess(cI2) = diag(-2, -2, -2).
        return detail::make_upper(3,
                                  {{0, 0, obj_scale * (6.0 * x(0) - 12.0) + l1 * 2.0 + l2 * -2.0},
                                   {1, 1, l1 * 2.0 + l2 * -2.0},
                                   {2, 2, l1 * -2.0 + l2 * -2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(2, 3,
                                {{0, 0, 2.0 * x(0)},
                                 {0, 1, 2.0 * x(1)},
                                 {0, 2, -2.0 * x(2)},
                                 {1, 0, -2.0 * x(0)},
                                 {1, 1, -2.0 * x(1)},
                                 {1, 2, -2.0 * x(2)}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Zero(3);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(3) << detail::kInf, detail::kInf, 5.0).finished();
        return u;
    }

    Vec start_point() const override { return (Vec(3) << 0.0, 0.0, 3.0).finished(); }
};

// ---------------------------------------------------------------------
// HS35 — Hock & Schittkowski (1981), problem 35 (Beale's problem).
//
//   f(x)  = 9 - 8 x1 - 6 x2 - 4 x3 + 2 x1^2 + 2 x2^2 + x3^2
//             + 2 x1 x2 + 2 x1 x3
//   g1(x) = 3 - x1 - x2 - 2 x3 >= 0  ->  ci1 = x1 + x2 + 2 x3 - 3 <= 0
//   bounds: x >= 0
//   x0 = (0.5, 0.5, 0.5), f* = 1/9 at x* = (4/3, 7/9, 4/9)
//
// A STRICTLY CONVEX QP with one linear inequality and bounds: like HS28 an
// exact-linearization control, but on the INEQUALITY side, so one major is
// the floor here too. Cross-checked against hs_new/tp035.mod.
// ---------------------------------------------------------------------
class Hs35Model : public NlpModel {
  public:
    Index n() const override { return 3; }
    Index me() const override { return 0; }
    Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2);
        return 9.0 - 8.0 * x1 - 6.0 * x2 - 4.0 * x3 + 2.0 * x1 * x1 + 2.0 * x2 * x2 + x3 * x3 +
               2.0 * x1 * x2 + 2.0 * x1 * x3;
    }

    Vec eval_grad(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2);
        return (Vec(3) << -8.0 + 4.0 * x1 + 2.0 * x2 + 2.0 * x3, -6.0 + 4.0 * x2 + 2.0 * x1,
                -4.0 + 2.0 * x3 + 2.0 * x1)
            .finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        Vec c(1);
        c(0) = x(0) + x(1) + 2.0 * x(2) - 3.0;
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        // Constant: the objective is exactly quadratic, the row exactly linear.
        return detail::make_upper(3, {{0, 0, obj_scale * 4.0},
                                      {0, 1, obj_scale * 2.0},
                                      {0, 2, obj_scale * 2.0},
                                      {1, 1, obj_scale * 4.0},
                                      {1, 2, 0.0},
                                      {2, 2, obj_scale * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 3);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::make_jac(1, 3, {{0, 0, 1.0}, {0, 1, 1.0}, {0, 2, 2.0}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Zero(3);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(3, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(3, 0.5); }
};

// ---------------------------------------------------------------------
// HS38 — Hock & Schittkowski (1981), problem 38 (Colville #4 / a coupled
// pair of Rosenbrock valleys).
//
//   f(x) = 100 (x2 - x1^2)^2 + (1 - x1)^2 + 90 (x4 - x3^2)^2 + (1 - x3)^2
//          + 10.1 ((x2 - 1)^2 + (x4 - 1)^2) + 19.8 (x2 - 1)(x4 - 1)
//   bounds: -10 <= xi <= 10
//   x0 = (-3, -1, -3, -1), f* = 0 at x* = (1, 1, 1, 1)
//
// THE BATTERY'S DERIVATIVE-MAGNITUDE STRESS CASE, and the reason
// derivative_check.h's tolerance became relative in this task: at the
// published start point |grad f|inf ~ 1.2e4 and |hess L|inf ~ 1.1e4, which
// puts the central-difference roundoff floor at ~2.6e-6 -- above the absolute
// 1e-6 the checker used through Task 10. Cross-checked against
// hs_new/tp038.mod.
// ---------------------------------------------------------------------
class Hs38Model : public NlpModel {
  public:
    Index n() const override { return 4; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(1) - x(0) * x(0), b = 1.0 - x(0);
        const double c = x(3) - x(2) * x(2), d = 1.0 - x(2);
        const double e = x(1) - 1.0, g = x(3) - 1.0;
        return 100.0 * a * a + b * b + 90.0 * c * c + d * d + 10.1 * (e * e + g * g) + 19.8 * e * g;
    }

    Vec eval_grad(const Vec &x) const override {
        const double a = x(1) - x(0) * x(0);
        const double c = x(3) - x(2) * x(2);
        Vec g(4);
        g(0) = -400.0 * x(0) * a - 2.0 * (1.0 - x(0));
        g(1) = 200.0 * a + 20.2 * (x(1) - 1.0) + 19.8 * (x(3) - 1.0);
        g(2) = -360.0 * x(2) * c - 2.0 * (1.0 - x(2));
        g(3) = 180.0 * c + 20.2 * (x(3) - 1.0) + 19.8 * (x(1) - 1.0);
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        const double h00 = 1200.0 * x(0) * x(0) - 400.0 * x(1) + 2.0;
        const double h22 = 1080.0 * x(2) * x(2) - 360.0 * x(3) + 2.0;
        return detail::make_upper(4, {{0, 0, obj_scale * h00},
                                      {0, 1, obj_scale * -400.0 * x(0)},
                                      {1, 1, obj_scale * 220.2},
                                      {1, 3, obj_scale * 19.8},
                                      {2, 2, obj_scale * h22},
                                      {2, 3, obj_scale * -360.0 * x(2)},
                                      {3, 3, obj_scale * 200.2}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 4);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 4);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(4, -10.0);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(4, 10.0);
        return u;
    }

    Vec start_point() const override { return (Vec(4) << -3.0, -1.0, -3.0, -1.0).finished(); }
};

// ---------------------------------------------------------------------
// HS39 — Hock & Schittkowski (1981), problem 39.
//
//   f(x)   = -x1
//   cE1(x) = x2 - x1^3 - x3^2                  [already equalities]
//   cE2(x) = x1^2 - x2 - x4^2
//   no bounds
//   x0 = (2, 2, 2, 2), f* = -1 at x* = (1, 1, 0, 0)
//
// LICQ HOLDS AT THE SOLUTION -- the rank-2 check below is exactly what
// confirms it -- but the CONSTRAINT COLUMNS for x3/x4 degenerate badly there:
// at x* = (1, 1, 0, 0) the two Jacobian rows are (-3, 1, 0, 0) and
// (2, -1, 0, 0), and the x3/x4 columns (-2 x3, -2 x4) are both ZERO -- so the
// 2 x 4 Jacobian keeps its full rank 2 (LICQ intact) even though the squared
// slack variables x3, x4 have vanished from it entirely, exactly the
// "squared-slack" degeneracy that makes this problem a standard hard case.
// A LINEAR OBJECTIVE means hess(f) = 0, so the subproblem's whole Hessian is
// the multiplier-weighted constraint curvature. Cross-checked against
// hs_new/tp039.mod.
// ---------------------------------------------------------------------
class Hs39Model : public NlpModel {
  public:
    Index n() const override { return 4; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return -x(0); }

    Vec eval_grad(const Vec &) const override { return (Vec(4) << -1.0, 0.0, 0.0, 0.0).finished(); }

    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c(0) = x(1) - x(0) * x(0) * x(0) - x(2) * x(2);
        c(1) = x(0) * x(0) - x(1) - x(3) * x(3);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double, const Vec &lambda_e, const Vec &) const override {
        const double l1 = lambda_e(0), l2 = lambda_e(1);
        // hess(cE1): d2/dx1^2 = -6 x1, d2/dx3^2 = -2.
        // hess(cE2): d2/dx1^2 = 2,      d2/dx4^2 = -2.
        return detail::make_upper(4, {{0, 0, l1 * -6.0 * x(0) + l2 * 2.0},
                                      {1, 1, 0.0},
                                      {2, 2, l1 * -2.0},
                                      {3, 3, l2 * -2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return detail::make_jac(2, 4,
                                {{0, 0, -3.0 * x(0) * x(0)},
                                 {0, 1, 1.0},
                                 {0, 2, -2.0 * x(2)},
                                 {0, 3, 0.0},
                                 {1, 0, 2.0 * x(0)},
                                 {1, 1, -1.0},
                                 {1, 2, 0.0},
                                 {1, 3, -2.0 * x(3)}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 4);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(4, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(4, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(4, 2.0); }
};

// ---------------------------------------------------------------------
// HS40 — Hock & Schittkowski (1981), problem 40.
//
//   f(x)   = -x1 x2 x3 x4
//   cE1(x) = x1^3 + x2^2 - 1                   [already equalities]
//   cE2(x) = x1^2 x4 - x3
//   cE3(x) = x4^2 - x2
//   no bounds
//   x0 = (0.8, 0.8, 0.8, 0.8), f* = -0.25 at
//     x* = (2^(-1/3), 2^(-1/2), 0.52973157, 0.84089642)
//
// f* IS EXACT, not a decimal: eliminating through the three equalities gives
// x1 = 2^(-1/3), x2 = 2^(-1/2), x4 = x2^(1/2), x3 = x1^2 x4, and the product
// x1 x2 x3 x4 is exactly 1/4. THREE equalities in FOUR variables leaves a
// one-dimensional null space, so this is the battery's tightest
// equality-constrained fixture. Cross-checked against hs_new/tp040r.mod
// (formulas identical; only that file's added x3 >= 0 bound differs).
// ---------------------------------------------------------------------
class Hs40Model : public NlpModel {
  public:
    Index n() const override { return 4; }
    Index me() const override { return 3; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return -x(0) * x(1) * x(2) * x(3); }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(4) << -x(1) * x(2) * x(3), -x(0) * x(2) * x(3), -x(0) * x(1) * x(3),
                -x(0) * x(1) * x(2))
            .finished();
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(3);
        c(0) = x(0) * x(0) * x(0) + x(1) * x(1) - 1.0;
        c(1) = x(0) * x(0) * x(3) - x(2);
        c(2) = x(3) * x(3) - x(1);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double l1 = lambda_e(0), l2 = lambda_e(1), l3 = lambda_e(2);
        // hess(f): zero diagonal, off-diagonal (i,j) = -prod of the other two.
        // hess(cE1): d2/dx1^2 = 6 x1, d2/dx2^2 = 2.
        // hess(cE2): d2/dx1^2 = 2 x4, d2/dx1dx4 = 2 x1.
        // hess(cE3): d2/dx4^2 = 2.
        return detail::make_upper(4, {{0, 0, l1 * 6.0 * x(0) + l2 * 2.0 * x(3)},
                                      {0, 1, obj_scale * -x(2) * x(3)},
                                      {0, 2, obj_scale * -x(1) * x(3)},
                                      {0, 3, obj_scale * -x(1) * x(2) + l2 * 2.0 * x(0)},
                                      {1, 1, l1 * 2.0},
                                      {1, 2, obj_scale * -x(0) * x(3)},
                                      {1, 3, obj_scale * -x(0) * x(2)},
                                      {2, 2, 0.0},
                                      {2, 3, obj_scale * -x(0) * x(1)},
                                      {3, 3, l3 * 2.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return detail::make_jac(3, 4,
                                {{0, 0, 3.0 * x(0) * x(0)},
                                 {0, 1, 2.0 * x(1)},
                                 {0, 2, 0.0},
                                 {0, 3, 0.0},
                                 {1, 0, 2.0 * x(0) * x(3)},
                                 {1, 1, 0.0},
                                 {1, 2, -1.0},
                                 {1, 3, x(0) * x(0)},
                                 {2, 0, 0.0},
                                 {2, 1, -1.0},
                                 {2, 2, 0.0},
                                 {2, 3, 2.0 * x(3)}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 4);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(4, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(4, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(4, 0.8); }
};

// ---------------------------------------------------------------------
// HS43 — Hock & Schittkowski (1981), problem 43 (Rosen-Suzuki).
//
//   f(x)  = x1^2 + x2^2 + 2 x3^2 + x4^2 - 5 x1 - 5 x2 - 21 x3 + 7 x4
//   g1(x) = 8  - x1^2 - x2^2 - x3^2 - x4^2 - x1 + x2 - x3 + x4 >= 0
//   g2(x) = 10 - x1^2 - 2 x2^2 - x3^2 - 2 x4^2 + x1 + x4       >= 0
//   g3(x) = 5  - 2 x1^2 - x2^2 - x3^2 - 2 x1 + x2 + x4         >= 0
//     -> ci_k = -g_k, i.e. each is the same polynomial negated:
//        ci1 = x1^2 + x2^2 + x3^2 + x4^2 + x1 - x2 + x3 - x4 - 8  <= 0
//        ci2 = x1^2 + 2 x2^2 + x3^2 + 2 x4^2 - x1 - x4 - 10       <= 0
//        ci3 = 2 x1^2 + x2^2 + x3^2 + 2 x1 - x2 - x4 - 5          <= 0
//   no bounds
//   x0 = (0, 0, 0, 0), f* = -44 at x* = (0, 1, 2, -1)
//
// The battery's standard multi-inequality nonlinear fixture: rows 1 and 3 are
// ACTIVE at x* and row 2 is slack (g2 = 1 there), so the active set has to be
// IDENTIFIED rather than guessed, and every row is convex so the reformulated
// subproblem stays well posed. Cross-checked against hs_new/tp043.mod.
// ---------------------------------------------------------------------
class Hs43Model : public NlpModel {
  public:
    Index n() const override { return 4; }
    Index me() const override { return 0; }
    Index mi() const override { return 3; }

    double eval_f(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2), x4 = x(3);
        return x1 * x1 + x2 * x2 + 2.0 * x3 * x3 + x4 * x4 - 5.0 * x1 - 5.0 * x2 - 21.0 * x3 +
               7.0 * x4;
    }

    Vec eval_grad(const Vec &x) const override {
        return (Vec(4) << 2.0 * x(0) - 5.0, 2.0 * x(1) - 5.0, 4.0 * x(2) - 21.0, 2.0 * x(3) + 7.0)
            .finished();
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override {
        const double x1 = x(0), x2 = x(1), x3 = x(2), x4 = x(3);
        Vec c(3);
        c(0) = x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4 + x1 - x2 + x3 - x4 - 8.0;
        c(1) = x1 * x1 + 2.0 * x2 * x2 + x3 * x3 + 2.0 * x4 * x4 - x1 - x4 - 10.0;
        c(2) = 2.0 * x1 * x1 + x2 * x2 + x3 * x3 + 2.0 * x1 - x2 - x4 - 5.0;
        return c;
    }

    SpMatU eval_hess(const Vec &, double obj_scale, const Vec &,
                     const Vec &lambda_i) const override {
        const double l1 = lambda_i(0), l2 = lambda_i(1), l3 = lambda_i(2);
        // Every Hessian here is CONSTANT and diagonal.
        return detail::make_upper(4, {{0, 0, obj_scale * 2.0 + l1 * 2.0 + l2 * 2.0 + l3 * 4.0},
                                      {1, 1, obj_scale * 2.0 + l1 * 2.0 + l2 * 4.0 + l3 * 2.0},
                                      {2, 2, obj_scale * 4.0 + l1 * 2.0 + l2 * 2.0 + l3 * 2.0},
                                      {3, 3, obj_scale * 2.0 + l1 * 2.0 + l2 * 4.0}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 4);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &x) const override {
        return detail::make_jac(3, 4,
                                {{0, 0, 2.0 * x(0) + 1.0},
                                 {0, 1, 2.0 * x(1) - 1.0},
                                 {0, 2, 2.0 * x(2) + 1.0},
                                 {0, 3, 2.0 * x(3) - 1.0},
                                 {1, 0, 2.0 * x(0) - 1.0},
                                 {1, 1, 4.0 * x(1)},
                                 {1, 2, 2.0 * x(2)},
                                 {1, 3, 4.0 * x(3) - 1.0},
                                 {2, 0, 4.0 * x(0) + 2.0},
                                 {2, 1, 2.0 * x(1) - 1.0},
                                 {2, 2, 2.0 * x(2)},
                                 {2, 3, -1.0}});
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(4, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(4, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Zero(4); }
};

// ---------------------------------------------------------------------
// HS45 — Hock & Schittkowski (1981), problem 45.
//
//   f(x) = 2 - x1 x2 x3 x4 x5 / 120
//   bounds: 0 <= xi <= i  (i = 1..5)
//   x0 = (2, 2, 2, 2, 2), f* = 1 at x* = (1, 2, 3, 4, 5) -- EVERY variable at
//     its upper bound
//
// THE PUBLISHED START POINT IS BOUND-INFEASIBLE (x1 = 2 against x1 <= 1), and
// it is used unchanged: the driver's subproblem box is l - x .. u - x, so the
// first trial's p1 is pinned into [-2, -1] and the iterate is pulled inside
// the box by construction (sqp_driver.h's feasibility note). That is worth
// exercising rather than papering over.
//
// THE SOLUTION IS A PURE VERTEX -- all five bounds active, no general
// constraints at all -- so the entire KKT residual is carried by the bound
// multiplier z, which makes this the battery's sharpest test of the
// model-implied z that SqpSolution reports.
//
// vanderbei .../hs/hs045.mod starts instead at x = 0, which is a WORSE trap
// than the published point: at the origin every partial derivative of a
// five-way product is 0, so the gradient vanishes identically and the origin
// is an exact stationary point at f = 2. The published (2,2,2,2,2) is used
// here for exactly that reason. hs_new/tp045r.mod uses the published start
// point too, with lower bounds lifted to 1e-4.
// ---------------------------------------------------------------------
class Hs45Model : public NlpModel {
  public:
    Index n() const override { return 5; }
    Index me() const override { return 0; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override { return 2.0 - x.prod() / 120.0; }

    Vec eval_grad(const Vec &x) const override {
        Vec g(5);
        for (Index i = 0; i < 5; ++i) {
            double p = 1.0;
            for (Index k = 0; k < 5; ++k) {
                if (k != i) {
                    p *= x(k);
                }
            }
            g(i) = -p / 120.0;
        }
        return g;
    }

    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &, const Vec &) const override {
        std::vector<Eigen::Triplet<double>> trips;
        for (Index i = 0; i < 5; ++i) {
            for (Index j = i; j < 5; ++j) {
                double v = 0.0; // d2f/dxi^2 == 0: no variable appears squared
                if (j > i) {
                    double p = 1.0;
                    for (Index k = 0; k < 5; ++k) {
                        if (k != i && k != j) {
                            p *= x(k);
                        }
                    }
                    v = -p / 120.0;
                }
                trips.emplace_back(i, j, obj_scale * v);
            }
        }
        return detail::make_upper(5, std::move(trips));
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return detail::no_jac(0, 5);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 5);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Zero(5);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = (Vec(5) << 1.0, 2.0, 3.0, 4.0, 5.0).finished();
        return u;
    }

    Vec start_point() const override { return Vec::Constant(5, 2.0); }
};

// ---------------------------------------------------------------------
// HS77 — Hock & Schittkowski (1981), problem 77.
//
//   f(x)   = (x1 - 1)^2 + (x1 - x2)^2 + (x3 - 1)^2 + (x4 - 1)^4 + (x5 - 1)^6
//   cE1(x) = x1^2 x4 + sin(x4 - x5) - 2 sqrt(2)      [already equalities]
//   cE2(x) = x2 + x3^4 x4^2 - 8 - sqrt(2)
//   no bounds
//   x0 = (2, 2, 2, 2, 2), f* = 0.24150513 at
//     x* = (1.166172, 1.182111, 1.380257, 1.506036, 0.6109203)
//
// THE THIRD TERM'S EXPONENT WAS CHECKED AGAINST f*, NOT JUST READ. Two of
// this objective's five terms are even powers of (x_i - 1) and a third is
// (x3 - 1)^2 sitting between them, so a 2/3 slip in that exponent is exactly
// the kind of error the derivative checker CANNOT catch: (x3-1)^2 and
// (x3-1)^3 are each internally self-consistent, and a model built on either
// passes assert_grad/assert_hessian. What separates them is f* at the
// published x* = (1.166172, 1.182111, 1.380257, 1.506036, 0.6109203):
// squared gives 0.2415049, matching the published f* = 0.24150513 to seven
// digits; cubed would give 0.1519, matching nothing.
//
// THE SQUARED FORM IS WHAT BOTH CITED PORTS CARRY -- hs_new/tp077.mod and
// hs/hs077.mod agree, and so do OptimizationProblems.jl and CUTEst. There is
// no disagreement here to adjudicate; the arithmetic above is recorded
// because it is the check that makes the exponent VERIFIED rather than
// copied, and the same check independently confirms the port's own published
// x*: this battery converges to x3 = 1.38025704, which is tp077.mod's
// 1.38025704314546.
//
// (tp077.mod's epilogue also carries f* to sixteen digits,
// 0.2415051287901787, against the 0.24150513 make_hs stores. Harmless at the
// battery's 1e-6 relative bar; noted so a future tightening knows where the
// digits are.)
//
// (x5 - 1)^6 makes hess(f) SINGULAR in the x5 direction at the solution
// (30 (x5-1)^4 is still ~0.5 at x5* = 0.611, so in practice it is merely
// ill-conditioned rather than singular here -- the sixth power is what makes
// the fixture interesting, not degenerate).
// ---------------------------------------------------------------------
class Hs77Model : public NlpModel {
  public:
    Index n() const override { return 5; }
    Index me() const override { return 2; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 1.0, b = x(0) - x(1), c = x(2) - 1.0;
        const double d = x(3) - 1.0, e = x(4) - 1.0;
        const double d2 = d * d, e2 = e * e;
        return a * a + b * b + c * c + d2 * d2 + e2 * e2 * e2;
    }

    Vec eval_grad(const Vec &x) const override {
        const double b = x(0) - x(1), d = x(3) - 1.0, e = x(4) - 1.0;
        Vec g(5);
        g(0) = 2.0 * (x(0) - 1.0) + 2.0 * b;
        g(1) = -2.0 * b;
        g(2) = 2.0 * (x(2) - 1.0);
        g(3) = 4.0 * d * d * d;
        g(4) = 6.0 * e * e * e * e * e;
        return g;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(2);
        c(0) = x(0) * x(0) * x(3) + std::sin(x(3) - x(4)) - 2.0 * std::sqrt(2.0);
        c(1) = x(1) + x(2) * x(2) * x(2) * x(2) * x(3) * x(3) - 8.0 - std::sqrt(2.0);
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double d = x(3) - 1.0, e = x(4) - 1.0;
        const double s = std::sin(x(3) - x(4));
        const double l1 = lambda_e(0), l2 = lambda_e(1);
        const double x3sq = x(2) * x(2);
        const double x3cu = x3sq * x(2);
        const double x3q = x3sq * x3sq;
        return detail::make_upper(5, {{0, 0, obj_scale * 4.0 + l1 * 2.0 * x(3)},
                                      {0, 1, obj_scale * -2.0},
                                      {0, 3, l1 * 2.0 * x(0)},
                                      {1, 1, obj_scale * 2.0},
                                      {2, 2, obj_scale * 2.0 + l2 * 12.0 * x3sq * x(3) * x(3)},
                                      {2, 3, l2 * 8.0 * x3cu * x(3)},
                                      {3, 3, obj_scale * 12.0 * d * d + l1 * -s + l2 * 2.0 * x3q},
                                      {3, 4, l1 * s},
                                      {4, 4, obj_scale * 30.0 * e * e * e * e + l1 * -s}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        const double c = std::cos(x(3) - x(4));
        const double x3sq = x(2) * x(2);
        return detail::make_jac(2, 5,
                                {{0, 0, 2.0 * x(0) * x(3)},
                                 {0, 1, 0.0},
                                 {0, 2, 0.0},
                                 {0, 3, x(0) * x(0) + c},
                                 {0, 4, -c},
                                 {1, 0, 0.0},
                                 {1, 1, 1.0},
                                 {1, 2, 4.0 * x3sq * x(2) * x(3) * x(3)},
                                 {1, 3, 2.0 * x3sq * x3sq * x(3)},
                                 {1, 4, 0.0}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 5);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(5, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(5, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(5, 2.0); }
};

// ---------------------------------------------------------------------
// HS79 — Hock & Schittkowski (1981), problem 79.
//
//   f(x)   = (x1 - 1)^2 + (x1 - x2)^2 + (x2 - x3)^2 + (x3 - x4)^4
//              + (x4 - x5)^4
//   cE1(x) = x1 + x2^2 + x3^3 - 2 - 3 sqrt(2)       [already equalities]
//   cE2(x) = x2 - x3^2 + x4 + 2 - 2 sqrt(2)
//   cE3(x) = x1 x5 - 2
//   no bounds
//   x0 = (2, 2, 2, 2, 2), f* = 0.0787768209 at
//     x* = (1.191127, 1.362603, 1.472818, 1.635017, 1.679081)
//
// Three nonlinear equalities in five variables. Like HS77 the objective's
// quartic tail terms make hess(f) nearly singular in the (x3 - x4) and
// (x4 - x5) directions at the solution, so the reduced Hessian's smallest
// eigenvalue is small and the driver's regularization is genuinely exercised.
// Cross-checked against hs_new/tp079.mod.
// ---------------------------------------------------------------------
class Hs79Model : public NlpModel {
  public:
    Index n() const override { return 5; }
    Index me() const override { return 3; }
    Index mi() const override { return 0; }

    double eval_f(const Vec &x) const override {
        const double a = x(0) - 1.0, b = x(0) - x(1), c = x(1) - x(2);
        const double d = x(2) - x(3), e = x(3) - x(4);
        return a * a + b * b + c * c + d * d * d * d + e * e * e * e;
    }

    Vec eval_grad(const Vec &x) const override {
        const double b = x(0) - x(1), c = x(1) - x(2);
        const double d3 = 4.0 * std::pow(x(2) - x(3), 3);
        const double e3 = 4.0 * std::pow(x(3) - x(4), 3);
        Vec g(5);
        g(0) = 2.0 * (x(0) - 1.0) + 2.0 * b;
        g(1) = -2.0 * b + 2.0 * c;
        g(2) = -2.0 * c + d3;
        g(3) = -d3 + e3;
        g(4) = -e3;
        return g;
    }

    Vec eval_ce(const Vec &x) const override {
        Vec c(3);
        c(0) = x(0) + x(1) * x(1) + x(2) * x(2) * x(2) - 2.0 - 3.0 * std::sqrt(2.0);
        c(1) = x(1) - x(2) * x(2) + x(3) + 2.0 - 2.0 * std::sqrt(2.0);
        c(2) = x(0) * x(4) - 2.0;
        return c;
    }
    Vec eval_ci(const Vec &) const override { return Vec(0); }

    SpMatU eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                     const Vec &) const override {
        const double d2 = 12.0 * (x(2) - x(3)) * (x(2) - x(3));
        const double e2 = 12.0 * (x(3) - x(4)) * (x(3) - x(4));
        const double l1 = lambda_e(0), l2 = lambda_e(1), l3 = lambda_e(2);
        // hess(cE1): d2/dx2^2 = 2, d2/dx3^2 = 6 x3. hess(cE2): d2/dx3^2 = -2.
        // hess(cE3): d2/dx1dx5 = 1.
        return detail::make_upper(5, {{0, 0, obj_scale * 4.0},
                                      {0, 1, obj_scale * -2.0},
                                      {0, 4, l3 * 1.0},
                                      {1, 1, obj_scale * 4.0 + l1 * 2.0},
                                      {1, 2, obj_scale * -2.0},
                                      {2, 2, obj_scale * (2.0 + d2) + l1 * 6.0 * x(2) + l2 * -2.0},
                                      {2, 3, obj_scale * -d2},
                                      {3, 3, obj_scale * (d2 + e2)},
                                      {3, 4, obj_scale * -e2},
                                      {4, 4, obj_scale * e2}});
    }

    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &x) const override {
        return detail::make_jac(3, 5,
                                {{0, 0, 1.0},
                                 {0, 1, 2.0 * x(1)},
                                 {0, 2, 3.0 * x(2) * x(2)},
                                 {0, 3, 0.0},
                                 {0, 4, 0.0},
                                 {1, 0, 0.0},
                                 {1, 1, 1.0},
                                 {1, 2, -2.0 * x(2)},
                                 {1, 3, 1.0},
                                 {1, 4, 0.0},
                                 {2, 0, x(4)},
                                 {2, 1, 0.0},
                                 {2, 2, 0.0},
                                 {2, 3, 0.0},
                                 {2, 4, x(0)}});
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return detail::no_jac(0, 5);
    }

    const Vec &lower() const override {
        static const Vec l = Vec::Constant(5, -detail::kInf);
        return l;
    }
    const Vec &upper() const override {
        static const Vec u = Vec::Constant(5, detail::kInf);
        return u;
    }

    Vec start_point() const override { return Vec::Constant(5, 2.0); }
};

struct HsProblem {
    std::unique_ptr<NlpModel> model;
    double f_star;
    const char *source;
};

// Every problem number this header ships, in ascending order -- the battery
// (tests/test_hs_battery.cpp) iterates it rather than re-listing the numbers,
// so a problem added above and to make_hs below joins the battery by
// construction and cannot be silently left out of it.
inline const std::vector<int> &hs_numbers() {
    static const std::vector<int> kNumbers = {1,  3,  5,  6,  7,  10, 11, 12, 14,
                                              15, 22, 24, 25, 26, 27, 28, 30, 33,
                                              35, 38, 39, 40, 43, 45, 76, 77, 79};
    return kNumbers;
}

// Factory for every Hock-Schittkowski problem shipped here: Task 3's six
// (1, 3, 5, 6, 7, 76) plus Task 11's twenty-one. Throws std::invalid_argument
// for any other number.
//
// EVERY f* BELOW IS SOURCE-CITED AND SELF-CHECKED. The citation is `source`;
// the self-check is tests/test_hs_battery.cpp, which re-derives a KKT
// quadruple from the model at the returned point instead of trusting the
// number. An f* with an exact closed form is stored in that form (HS7, HS14,
// HS33, HS35) so that no hand-typed decimal sits between the source and the
// assertion.
inline HsProblem make_hs(int number) {
    static constexpr const char *kSource =
        "Hock, W., Schittkowski, K. (1981), Test Examples for Nonlinear "
        "Programming Codes, Lecture Notes in Economics and Mathematical "
        "Systems 187, Springer-Verlag";
    switch (number) {
    case 1:
        return HsProblem{std::make_unique<Hs1Model>(), 0.0, kSource};
    case 3:
        return HsProblem{std::make_unique<Hs3Model>(), 0.0, kSource};
    case 5:
        return HsProblem{std::make_unique<Hs5Model>(), -1.913222954981036, kSource};
    case 6:
        return HsProblem{std::make_unique<Hs6Model>(), 0.0, kSource};
    case 7:
        return HsProblem{std::make_unique<Hs7Model>(), -std::sqrt(3.0), kSource};
    case 10:
        return HsProblem{std::make_unique<Hs10Model>(), -1.0, kSource};
    case 11:
        return HsProblem{std::make_unique<Hs11Model>(), -8.498464223154677, kSource};
    case 12:
        return HsProblem{std::make_unique<Hs12Model>(), -30.0, kSource};
    case 14:
        // Exact closed form (= 1.393464980689302, which is also what
        // hs_new/tp014.mod's epilogue carries); see Hs14Model for the
        // derivation and for why the closed form is stored rather than a
        // hand-typed decimal.
        return HsProblem{std::make_unique<Hs14Model>(), 9.0 - 2.875 * std::sqrt(7.0), kSource};
    case 15:
        return HsProblem{std::make_unique<Hs15Model>(), 306.5, kSource};
    case 22:
        return HsProblem{std::make_unique<Hs22Model>(), 1.0, kSource};
    case 24:
        return HsProblem{std::make_unique<Hs24Model>(), -1.0, kSource};
    case 25:
        return HsProblem{std::make_unique<Hs25Model>(), 0.0, kSource};
    case 26:
        return HsProblem{std::make_unique<Hs26Model>(), 0.0, kSource};
    case 27:
        return HsProblem{std::make_unique<Hs27Model>(), 0.04, kSource};
    case 28:
        return HsProblem{std::make_unique<Hs28Model>(), 0.0, kSource};
    case 30:
        return HsProblem{std::make_unique<Hs30Model>(), 1.0, kSource};
    case 33:
        return HsProblem{std::make_unique<Hs33Model>(), std::sqrt(2.0) - 6.0, kSource};
    case 35:
        return HsProblem{std::make_unique<Hs35Model>(), 1.0 / 9.0, kSource};
    case 38:
        return HsProblem{std::make_unique<Hs38Model>(), 0.0, kSource};
    case 39:
        return HsProblem{std::make_unique<Hs39Model>(), -1.0, kSource};
    case 40:
        return HsProblem{std::make_unique<Hs40Model>(), -0.25, kSource};
    case 43:
        return HsProblem{std::make_unique<Hs43Model>(), -44.0, kSource};
    case 45:
        return HsProblem{std::make_unique<Hs45Model>(), 1.0, kSource};
    case 76:
        return HsProblem{std::make_unique<Hs76Model>(), -4.681818181818182, kSource};
    case 77:
        return HsProblem{std::make_unique<Hs77Model>(), 0.24150513, kSource};
    case 79:
        return HsProblem{std::make_unique<Hs79Model>(), 0.0787768209, kSource};
    default:
        throw std::invalid_argument(fmt::format("make_hs: problem {} is not shipped (shipped: {})",
                                                number, fmt::join(hs_numbers(), ", ")));
    }
}

} // namespace hven::solvers::test_support
