// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "hven/detail/qp/ipqp_trace.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h"

namespace {
constexpr double kSolverInf = std::numeric_limits<double>::infinity();
} // namespace

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
using hven::solvers::NLPSolver;

// The canonical Ipopt HS071 example: n=4, one lower-bounded product row, one
// equality sphere row, dense Jacobian and Hessian.
struct Hs071Problem : NLPProblem {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, 1.0, 1.0, 1.0;
        xu << 5.0, 5.0, 5.0, 5.0;
        gl << 25.0, 40.0;
        gu << kSolverInf, 40.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[3] * (2.0 * x[0] + x[1] + x[2]);
        g[1] = x[0] * x[3];
        g[2] = x[0] * x[3] + 1.0;
        g[3] = x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] * x[1] * x[2] * x[3];
        g[1] = x[0] * x[0] + x[1] * x[1] + x[2] * x[2] + x[3] * x[3];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = x[1] * x[2] * x[3];
        v[1] = x[0] * x[2] * x[3];
        v[2] = x[0] * x[1] * x[3];
        v[3] = x[0] * x[1] * x[2];
        v[4] = 2.0 * x[0];
        v[5] = 2.0 * x[1];
        v[6] = 2.0 * x[2];
        v[7] = 2.0 * x[3];
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        const double x0 = x[0], x1 = x[1], x2 = x[2], x3 = x[3];
        v[0] = obj_factor * 2 * x3 + lambda[1] * 2;
        v[1] = obj_factor * x3 + lambda[0] * x2 * x3;
        v[2] = lambda[1] * 2;
        v[3] = obj_factor * x3 + lambda[0] * x1 * x3;
        v[4] = lambda[0] * x0 * x3;
        v[5] = lambda[1] * 2;
        v[6] = obj_factor * (2 * x0 + x1 + x2) + lambda[0] * x1 * x2;
        v[7] = obj_factor * x0 + lambda[0] * x0 * x2;
        v[8] = obj_factor * x0 + lambda[0] * x0 * x1;
        v[9] = lambda[1] * 2;
    }
    std::string name() const override { return "Hs071Problem"; }
};

TEST(NLPSolverTest, Hs071ConvergesToKnownOptimum) {
    hven::solvers::NLPSolver solver(std::make_shared<Hs071Problem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    auto flag = solver.optimize(x0);
    EXPECT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
    Eigen::VectorXd x = solver.return_x();
    Eigen::VectorXd expect(4);
    expect << 1.00000000, 4.74299963, 3.82114998, 1.37940829;
    EXPECT_LT((x - expect).lpNorm<Eigen::Infinity>(), 1e-5);
}

// SolveResult::bound_lmults_ pin: HS071's x[0] sits exactly on its active
// lower bound (xl[0] == 1.0 == x*[0]) at the known optimum above, and the
// other three variables are strictly interior. The exposed z must therefore
// be the right dimension (one entry per solver primal, since HS071 has no
// eliminated variables) and follow the sign convention nlp_model.h pins:
// >= 0 at an active lower bound, ~0 when free.
TEST(NLPSolverTest, Hs071BoundDualsMatchActiveLowerBound) {
    hven::solvers::NLPSolver solver(std::make_shared<Hs071Problem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    const Eigen::VectorXd &z = solver.optimizer_->result().bound_lmults_;
    ASSERT_EQ(z.size(), 4);
    EXPECT_GT(z[0], 1e-6);        // active lower bound: z >= 0, and strictly so here
    EXPECT_NEAR(z[1], 0.0, 1e-6); // free
    EXPECT_NEAR(z[2], 0.0, 1e-6); // free
    EXPECT_NEAR(z[3], 0.0, 1e-6); // free
}

// Unconstrained Rosenbrock: exercises the objective-owned Hessian path (no
// constraint rows at all).
struct RosenbrockProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double a = 1.0 - x[0];
        const double b = x[1] - x[0] * x[0];
        f = a * a + 100.0 * b * b;
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        const double b = x[1] - x[0] * x[0];
        g[0] = -2.0 * (1.0 - x[0]) - 400.0 * x[0] * b;
        g[1] = 200.0 * b;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1;
        c << 0, 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        const double b = x[1] - x[0] * x[0];
        v[0] = obj_factor * (2.0 - 400.0 * b + 800.0 * x[0] * x[0]);
        v[1] = obj_factor * (-400.0 * x[0]);
        v[2] = obj_factor * 200.0;
    }
    std::string name() const override { return "RosenbrockProblem"; }
};

TEST(NLPSolverTest, RosenbrockConvergesToKnownOptimum) {
    hven::solvers::NLPSolver solver(std::make_shared<RosenbrockProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(2);
    x0 << -1.2, 1.0;
    auto flag = solver.optimize(x0);
    EXPECT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
    Eigen::VectorXd x = solver.return_x();
    Eigen::VectorXd expect(2);
    expect << 1.0, 1.0;
    EXPECT_LT((x - expect).lpNorm<Eigen::Infinity>(), 1e-6);
}

// f = x0^2 + x1^2 subject to x0 + x1 = 2 -- optimum (1, 1); the Ipopt-sign
// multiplier satisfies 2*x_i + lambda = 0, so lambda = -2.
struct EqOnlyProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf, -kSolverInf;
        xu << kSolverInf, kSolverInf;
        gl << 2.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[0] + x[1] * x[1];
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
        g[1] = 2.0 * x[1];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
        v[1] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "EqOnlyProblem"; }
};

TEST(NLPSolverTest, EqualityMultiplierHasIpoptSign) {
    hven::solvers::NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_NEAR(solver.return_multipliers()[0], -2.0, 1e-5);
}

// f = x0^2 subject to x0 >= 1 -- optimum x0 = 1, active lower bound, Ipopt
// multiplier is negative there (lambda = -2).
struct LowerBoundRowProblem : NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 1; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf;
        xu << kSolverInf;
        gl << 1.0;
        gu << kSolverInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override { f = x[0] * x[0]; }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * x[0];
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
    }
    std::string name() const override { return "LowerBoundRowProblem"; }
};

TEST(NLPSolverTest, LowerBoundedRowActiveWithNegativeIpoptMultiplier) {
    hven::solvers::NLPSolver solver(std::make_shared<LowerBoundRowProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(1);
    x0 << 3.0;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_NEAR(solver.return_x()[0], 1.0, 1e-5);
    EXPECT_NEAR(solver.return_multipliers()[0], -2.0, 1e-5);
}

// f = (x0-3)^2 subject to 1 <= x0 <= 2 (a Range row) -- optimum x0 = 2, active
// upper end, positive Ipopt multiplier (lambda = +2).
struct RangeRowProblem : NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 1; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf;
        xu << kSolverInf;
        gl << 1.0;
        gu << 2.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double d = x[0] - 3.0;
        f = d * d;
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * (x[0] - 3.0);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
    }
    std::string name() const override { return "RangeRowProblem"; }
};

TEST(NLPSolverTest, RangeRowActiveAtUpperWithPositiveIpoptMultiplier) {
    hven::solvers::NLPSolver solver(std::make_shared<RangeRowProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(1);
    x0 << 1.5;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_NEAR(solver.return_x()[0], 2.0, 1e-5);
    EXPECT_NEAR(solver.return_multipliers()[0], 2.0, 1e-5);
}

// f = (x0-5)^2, n=1, m=1 with the single row unbounded on both sides (a Free
// row the classification drops from the transcription entirely). Solves
// unconstrained to x0 = 5; the dropped row's multiplier reads back as 0.
struct FreeRowProblem : NLPProblem {
    int num_vars() const override { return 1; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 1; }
    int num_hess_nonzeros() const override { return 1; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << -kSolverInf;
        xu << kSolverInf;
        gl << -kSolverInf;
        gu << kSolverInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double d = x[0] - 5.0;
        f = d * d;
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * (x[0] - 5.0);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0];
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0;
        c << 0;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 1.0;
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
    }
    std::string name() const override { return "FreeRowProblem"; }
};

TEST(NLPSolverTest, FreeRowDroppedFromTranscriptionReadsZeroMultiplier) {
    hven::solvers::NLPSolver solver(std::make_shared<FreeRowProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(1);
    x0 << 0.0;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_NEAR(solver.return_x()[0], 5.0, 1e-5);
    EXPECT_NEAR(solver.return_multipliers()[0], 0.0, 1e-14);
}

// f = (x0-1)^2 + (x1-1)^2, no constraint rows, x1 fixed via x_lower == x_upper.
// Exercises the adapter's native fixed-variable treatment.
struct FixedVarProblem : NLPProblem {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 2; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -kSolverInf, 3.0;
        xu << kSolverInf, 3.0;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        const double a = x[0] - 1.0, b = x[1] - 1.0;
        f = a * a + b * b;
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = 2.0 * (x[0] - 1.0);
        g[1] = 2.0 * (x[1] - 1.0);
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v[0] = 2.0 * obj_factor;
        v[1] = 2.0 * obj_factor;
    }
    std::string name() const override { return "FixedVarProblem"; }
};

TEST(NLPSolverTest, FixedVariableSolvesExactlyAtItsFixedValue) {
    hven::solvers::NLPSolver solver(std::make_shared<FixedVarProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(2);
    x0 << 0.0, 3.0;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    Eigen::VectorXd x = solver.return_x();
    EXPECT_NEAR(x[0], 1.0, 1e-6);
    EXPECT_NEAR(x[1], 3.0, 1e-12);
}

// SolveResult::fixed_variable_treatment_ pin, using the same fixture: under
// the default MakeParameter treatment the fixed variable is eliminated (no
// row added), so eq_lmults_ stays empty; switched to MakeConstraint, the one
// fixed variable becomes one internal fixing row, so eq_lmults_ grows to
// exactly one entry -- pinning both the recorded treatment and the
// treatment-dependent shape documented on eq_lmults_.
TEST(NLPSolverTest, FixedVariableTreatmentIsRecordedOnSolveResult) {
    {
        hven::solvers::NLPSolver solver(std::make_shared<FixedVarProblem>());
        solver.optimizer_->set_print_level(10);
        Eigen::VectorXd x0(2);
        x0 << 0.0, 3.0;
        ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
        EXPECT_EQ(solver.optimizer_->result().fixed_variable_treatment_,
                  hven::solvers::FixedVariableTreatments::MakeParameter);
        EXPECT_EQ(solver.optimizer_->result().eq_lmults_.size(), 0);
    }
    {
        hven::solvers::NLPSolver solver(std::make_shared<FixedVarProblem>());
        solver.optimizer_->set_print_level(10);
        solver.optimizer_->set_fixed_variable_treatment(
            hven::solvers::FixedVariableTreatments::MakeConstraint);
        Eigen::VectorXd x0(2);
        x0 << 0.0, 3.0;
        ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
        EXPECT_EQ(solver.optimizer_->result().fixed_variable_treatment_,
                  hven::solvers::FixedVariableTreatments::MakeConstraint);
        EXPECT_EQ(solver.optimizer_->result().eq_lmults_.size(), 1);
    }
}

// SolveResult::bound_lmults_ pin against the RelaxBounds -> MakeParameter
// treatment switch: under RelaxBounds the fixed variable is NOT eliminated,
// so it reaches the solver as a widened two-sided bound and bounds_lmults_
// comes back non-empty; switched to MakeParameter on the SAME solver
// instance (no intervening set_nlp()), the variable is eliminated instead,
// bounds_ goes back to null, and bound_lmults_ must not still be reporting
// the first solve's z at the old, now-mismatched primal_vars_ width.
TEST(NLPSolverTest, BoundLmultsClearedAfterATreatmentSwitchDropsTheBoundSet) {
    hven::solvers::NLPSolver solver(std::make_shared<FixedVarProblem>());
    solver.optimizer_->set_print_level(10);
    solver.optimizer_->set_fixed_variable_treatment(
        hven::solvers::FixedVariableTreatments::RelaxBounds);
    Eigen::VectorXd x0(2);
    x0 << 0.0, 3.0;

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_GT(solver.optimizer_->result().bound_lmults_.size(), 0);

    solver.optimizer_->set_fixed_variable_treatment(
        hven::solvers::FixedVariableTreatments::MakeParameter);
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(solver.optimizer_->result().bound_lmults_.size(), 0);
}

// EqOnlyProblem plus a starting_multipliers() override that returns true and
// seeds the exact solution multiplier (lambda = -2, see
// EqualityMultiplierHasIpoptSign above) -- proves the seed reaches InteriorPointSolver and
// still converges to the same optimum as the unseeded solve.
struct SeededEqOnlyProblem : EqOnlyProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = -2.0;
        return true;
    }
    std::string name() const override { return "SeededEqOnlyProblem"; }
};

TEST(NLPSolverTest, SeededSolveConverges) {
    hven::solvers::NLPSolver solver(std::make_shared<SeededEqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    Eigen::VectorXd x = solver.return_x();
    Eigen::VectorXd expect(2);
    expect << 1.0, 1.0;
    EXPECT_LT((x - expect).lpNorm<Eigen::Infinity>(), 1e-6);
    EXPECT_NEAR(solver.return_multipliers()[0], -2.0, 1e-5);
}

// The no-arg optimize() override reuses whatever is already in
// active_variables_ as the input iterate -- must match the x0-arg path
// solving from the same starting point.
TEST(NLPSolverTest, NoArgOptimizeUsesActiveVariables) {
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    hven::solvers::NLPSolver x0_solver(std::make_shared<EqOnlyProblem>());
    x0_solver.optimizer_->set_print_level(10);
    ASSERT_EQ(x0_solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    hven::solvers::NLPSolver noarg_solver(std::make_shared<EqOnlyProblem>());
    noarg_solver.optimizer_->set_print_level(10);
    noarg_solver.active_variables_ = x0;
    ASSERT_EQ(noarg_solver.optimize(), hven::ConvergenceFlags::CONVERGED);

    EXPECT_LT((noarg_solver.return_x() - x0_solver.return_x()).lpNorm<Eigen::Infinity>(), 1e-8);
}

// A fresh solver's active_variables_ is a 0-element vector; the no-arg
// optimize() override must hit the size-mismatch check in run(), not attempt
// a solve from an empty iterate.
TEST(NLPSolverTest, NoArgOptimizeOnFreshSolverThrows) {
    hven::solvers::NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    EXPECT_THROW(solver.optimize(), std::invalid_argument);
}

// jet_initialize() transcribes once; a subsequent no-arg solve must not
// re-transcribe; jet_release() resets do_transcription_ so the next x0-arg
// solve builds a fresh NonLinearProgram from scratch.
TEST(NLPSolverTest, JetLifecycleRoundTrip) {
    hven::solvers::NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);

    solver.jet_initialize();
    EXPECT_FALSE(solver.do_transcription_);

    solver.active_variables_ = Eigen::VectorXd::Zero(2);
    ASSERT_EQ(solver.optimize(), hven::ConvergenceFlags::CONVERGED);
    EXPECT_FALSE(solver.do_transcription_); // no re-transcription happened

    solver.jet_release();
    EXPECT_TRUE(solver.do_transcription_);
    solver.optimizer_->set_print_level(10); // jet_release() resets print level; re-silence

    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    EXPECT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED); // fresh transcription works
}

// starting_multipliers() returning a non-finite entry must fail the
// allFinite() guard in apply_starting_multipliers -- before ever reaching
// InteriorPointSolver::set_initial_multipliers, and with a distinct message.
struct NonFiniteSeedProblem : EqOnlyProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    std::string name() const override { return "NonFiniteSeedProblem"; }
};

TEST(NLPSolverTest, NonFiniteStartingMultipliersThrow) {
    hven::solvers::NLPSolver solver(std::make_shared<NonFiniteSeedProblem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    try {
        solver.optimize(x0);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("non-finite"), std::string::npos);
    }
}

// Partition count and QP thread count are independent settings, each reached
// through its own setter, and neither setter touches the other's state. The
// concepts below pin that surface: the only partition setter takes the
// partition count alone, so no call site can silently reset the QP thread
// count while asking for a partition count.
template <class T>
concept SetsPartitionsAlone = requires(T &t) { t.set_num_partitions(1); };
template <class T>
concept SetsPartitionsAndQpThreads = requires(T &t) { t.set_num_partitions(1, 1); };

static_assert(SetsPartitionsAlone<NLPSolver>);
static_assert(!SetsPartitionsAndQpThreads<NLPSolver>);

// M6 W5 T1's DECLARED BREAK, pinned where a future reader will look for it:
// NLPSolver is not a base class and is not polymorphic. The folded base's
// virtuals existed for a derived-class contract with no second member.
static_assert(std::is_final_v<NLPSolver>);
static_assert(!std::is_polymorphic_v<NLPSolver>);

TEST(NLPSolverTest, PartitionCountAndQpThreadCountAreSetIndependently) {
    NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);

    solver.optimizer_->set_qp_threads(3);
    solver.set_num_partitions(2);
    EXPECT_EQ(solver.num_partitions_, 2);
    EXPECT_EQ(solver.optimizer_->settings().qp_threads_, 3); // partitions left it alone

    solver.optimizer_->set_qp_threads(1);
    EXPECT_EQ(solver.optimizer_->settings().qp_threads_, 1);
    EXPECT_EQ(solver.num_partitions_, 2); // and the QP setter left partitions alone

    EXPECT_THROW(solver.set_num_partitions(0), std::invalid_argument);
    EXPECT_THROW(solver.optimizer_->set_qp_threads(0), std::invalid_argument);

    // Both jet entry points put the solver on one partition and one QP thread.
    solver.set_num_partitions(4);
    solver.optimizer_->set_qp_threads(4);
    solver.jet_initialize();
    EXPECT_EQ(solver.num_partitions_, 1);
    EXPECT_EQ(solver.optimizer_->settings().qp_threads_, 1);

    solver.set_num_partitions(4);
    solver.optimizer_->set_qp_threads(4);
    solver.jet_release();
    EXPECT_EQ(solver.num_partitions_, 1);
    EXPECT_EQ(solver.optimizer_->settings().qp_threads_, 1);
    solver.optimizer_->set_print_level(10); // jet_release() resets the print level
}

// The set_*() validators for bound_push, alpha_red, delta_h, incr_h,
// bound_fraction and decr_h are written as negated comparisons so that a NaN,
// which compares false against every ordinary relational operator, is
// refused rather than silently accepted and stored.
TEST(InteriorPointSolverSettingsTest, NaNRejectedBySiteNamedSetters) {
    hven::solvers::InteriorPointSolver solver;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    try {
        solver.set_bound_push(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
    }
    try {
        solver.set_alpha_red(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("alpha_red"), std::string::npos);
    }
    try {
        solver.set_delta_h(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        solver.set_incr_h(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
    try {
        solver.set_bound_fraction(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_fraction"), std::string::npos);
    }
    try {
        solver.set_decr_h(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("decr_h"), std::string::npos);
    }
    try {
        solver.set_bound_interval_push(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_interval_push"), std::string::npos);
    }
    try {
        solver.set_bound_relax_factor(nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_relax_factor"), std::string::npos);
    }

    // set_hpert_params delegates to the three setters above, so a NaN in any
    // one argument is refused too -- naming that argument's own setter site,
    // since it is set_delta_h/set_incr_h/set_decr_h that actually throws.
    try {
        solver.set_hpert_params(nan, 8.0, 0.1);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        solver.set_hpert_params(1e-4, nan, 0.1);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
    try {
        solver.set_hpert_params(1e-4, 8.0, nan);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("decr_h"), std::string::npos);
    }
}

// greater_than's four callers (bound_push, alpha_red, delta_h, incr_h) all
// feed a magnitude or rate that downstream arithmetic uses directly, with no
// "infinity means disabled" reading anywhere in the solver -- so +inf is
// refused right alongside NaN, not just accepted as "greater than the bound".
TEST(InteriorPointSolverSettingsTest, InfRejectedByGreaterThanSetters) {
    hven::solvers::InteriorPointSolver solver;
    const double inf = std::numeric_limits<double>::infinity();

    try {
        solver.set_bound_push(inf);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
    }
    try {
        solver.set_alpha_red(inf);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("alpha_red"), std::string::npos);
    }
    try {
        solver.set_delta_h(inf);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        solver.set_incr_h(inf);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
}

// The other half of the twice-checked pairing: a NaN written directly through
// the mutable settings() reference (bypassing the setter entirely) must still
// be caught by Settings::validate(), one representative field per helper
// family that has a numeric double-valued invariant.
TEST(InteriorPointSolverSettingsTest, NaNRejectedByValidateForEveryHelperFamily) {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    {
        hven::solvers::InteriorPointSolver solver; // pos_finite
        solver.settings().kkt_tol_ = nan;
        try {
            solver.settings().validate();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("kkt_tol"), std::string::npos);
        }
    }
    {
        hven::solvers::InteriorPointSolver solver; // in_open_unit
        solver.settings().bound_fraction_ = nan;
        try {
            solver.settings().validate();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_fraction"), std::string::npos);
        }
    }
    {
        hven::solvers::InteriorPointSolver solver; // greater_than
        solver.settings().bound_push_ = nan;
        try {
            solver.settings().validate();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
        }
    }
    {
        hven::solvers::InteriorPointSolver solver; // in_open_interval
        solver.settings().bound_interval_push_ = nan;
        try {
            solver.settings().validate();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_interval_push"), std::string::npos);
        }
    }
    {
        hven::solvers::InteriorPointSolver solver; // in_closed_interval
        solver.settings().bound_relax_factor_ = nan;
        try {
            solver.settings().validate();
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_relax_factor"), std::string::npos);
        }
    }
}

// A problem whose Hessian callback throws while armed. Transcription runs that
// callback at the model's start point, so arming it makes transcribe() fault
// partway through.
struct FaultingSetupProblem : EqOnlyProblem {
    bool armed_ = true;

    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        if (armed_) {
            throw std::runtime_error("FaultingSetupProblem: eval_hess armed to throw");
        }
        EqOnlyProblem::eval_hess(x, obj_factor, lambda, v);
    }
    std::string name() const override { return "FaultingSetupProblem"; }
};

TEST(NLPSolverTest, AFaultedTranscriptionCommitsNothingAndRetriesCleanly) {
    auto problem = std::make_shared<FaultingSetupProblem>();
    NLPSolver solver(problem);
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    // A fault on the very first transcription commits nothing at all.
    EXPECT_THROW(solver.optimize(x0), std::runtime_error);
    EXPECT_EQ(solver.model_, nullptr);
    EXPECT_EQ(solver.core_, nullptr);
    EXPECT_EQ(solver.nlp_, nullptr);
    EXPECT_TRUE(solver.do_transcription_);
    EXPECT_THROW(solver.return_multipliers(), std::runtime_error); // nothing was solved

    // Clearing the fault and retrying succeeds; nothing had to be reset by hand.
    problem->armed_ = false;
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_FALSE(solver.do_transcription_);
    const auto model_after = solver.model_;
    const auto core_after = solver.core_;
    const auto nlp_after = solver.nlp_;
    ASSERT_NE(model_after, nullptr);

    // A fault on a later transcription leaves the standing one whole -- same
    // three objects, still consistent with each other -- and leaves the retry
    // flag set rather than half-replacing the solver.
    problem->armed_ = true;
    solver.do_transcription_ = true;
    EXPECT_THROW(solver.optimize(x0), std::runtime_error);
    EXPECT_EQ(solver.model_, model_after);
    EXPECT_EQ(solver.core_, core_after);
    EXPECT_EQ(solver.nlp_, nlp_after);
    EXPECT_TRUE(solver.do_transcription_);

    // The optimizer kept the standing transcription too, not just this
    // solver's members: with the fault cleared and re-transcription
    // suppressed, a solve still runs against the program the optimizer was
    // given before the fault. Nothing reached set_nlp on the faulted attempt,
    // so there was nothing there to replace.
    problem->armed_ = false;
    solver.do_transcription_ = false;
    EXPECT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(solver.model_, model_after);
    EXPECT_EQ(solver.nlp_, nlp_after);

    solver.do_transcription_ = true;
    EXPECT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
}

// Counts the setup-only queries a transcription makes of the problem. bounds,
// jac_structure and hess_structure are asked exactly once per transcription
// and never during a solve, so their totals are a direct count of how many
// transcriptions have happened.
struct TranscriptionCountingProblem : EqOnlyProblem {
    mutable int n_bounds_ = 0, n_jac_structure_ = 0, n_hess_structure_ = 0;
    mutable int n_eval_jac_ = 0, n_eval_hess_ = 0;

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        n_bounds_++;
        EqOnlyProblem::bounds(xl, xu, gl, gu);
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        n_jac_structure_++;
        EqOnlyProblem::jac_structure(r, c);
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        n_hess_structure_++;
        EqOnlyProblem::hess_structure(r, c);
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> v) const override {
        n_eval_jac_++;
        EqOnlyProblem::eval_jac(x, v);
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        n_eval_hess_++;
        EqOnlyProblem::eval_hess(x, obj_factor, lambda, v);
    }
    std::string name() const override { return "TranscriptionCountingProblem"; }
};

TEST(NLPSolverTest, ASecondSolveTranscribesNothingAndSpendsNoFurtherSetupEvaluation) {
    auto problem = std::make_shared<TranscriptionCountingProblem>();
    NLPSolver solver(problem);
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    // Exactly one transcription: one bounds query, one of each structure.
    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);
    EXPECT_FALSE(solver.do_transcription_);
    const auto model_after_first = solver.model_;
    const auto nlp_after_first = solver.nlp_;

    const int jac_after_first = problem->n_eval_jac_;
    const int hess_after_first = problem->n_eval_hess_;

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    // Nothing was transcribed a second time, so no setup evaluation happened a
    // second time either: the counts that a transcription and only a
    // transcription moves are unchanged, and the model and program are the
    // same objects.
    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);
    EXPECT_EQ(solver.model_, model_after_first);
    EXPECT_EQ(solver.nlp_, nlp_after_first);
    // The second solve did evaluate -- it is a solve -- so the derivative
    // counts moved; the point is that none of that movement was setup.
    EXPECT_GT(problem->n_eval_jac_, jac_after_first);
    EXPECT_GT(problem->n_eval_hess_, hess_after_first);
}

// THE TERMINAL KKT RESIDUALS ON SolveResult. Four scalars a consumer's outcome
// record needs and could not otherwise obtain: eq_cons_/iq_cons_ carry the
// PRIMAL residuals as vectors, but stationarity and complementarity are not
// reconstructible from a SolveResult at all.
//
// They are the very numbers converge_check gated on, so this pins them against
// the four Settings tolerances that produced the CONVERGED verdict rather than
// against hand-copied constants. A residual reported from some other iterate
// fails the gates; one left at its reset value fails too, because that value is
// NaN and NaN passes neither the finiteness checks nor the < comparisons.
//
// Nothing here reads a clock, and nothing asserts a residual VALUE against a
// literal: the assertion is the relation to the gate.
TEST(NLPSolverTest, TheReportedKktResidualsAreTheOnesTheConvergenceTestGated) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    solver.optimizer_->set_print_level(10);
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);

    const auto &result = solver.optimizer_->result();
    const auto &settings = solver.optimizer_->settings();

    EXPECT_TRUE(std::isfinite(result.kkt_inf_));
    EXPECT_TRUE(std::isfinite(result.barr_inf_));
    EXPECT_TRUE(std::isfinite(result.econ_inf_));
    EXPECT_TRUE(std::isfinite(result.icon_inf_));

    EXPECT_LT(result.kkt_inf_, settings.kkt_tol_);
    EXPECT_LT(result.barr_inf_, settings.bar_tol_);
    EXPECT_LT(result.econ_inf_, settings.econ_tol_);
    EXPECT_LT(result.icon_inf_, settings.icon_tol_);

    // A residual is a norm: never negative, whatever the exit.
    EXPECT_GE(result.kkt_inf_, 0.0);
    EXPECT_GE(result.barr_inf_, 0.0);
    EXPECT_GE(result.econ_inf_, 0.0);
    EXPECT_GE(result.icon_inf_, 0.0);

    // HS071 has both row kinds, so neither constraint residual is the vacuous
    // "no rows, therefore zero" reading.
    ASSERT_EQ(result.eq_cons_.size(), 1);
    ASSERT_EQ(result.iq_cons_.size(), 1);

    // A SECOND CALL RE-REPORTS. reset_accumulators() clears the four at solve
    // entry, so a stale reading cannot survive into a call that never wrote
    // them; here the second call writes them again and lands inside the same
    // gates.
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    EXPECT_LT(result.kkt_inf_, settings.kkt_tol_);
    EXPECT_LT(result.barr_inf_, settings.bar_tol_);
    EXPECT_LT(result.econ_inf_, settings.econ_tol_);
    EXPECT_LT(result.icon_inf_, settings.icon_tol_);
}

// A problem whose OBJECTIVE RISES along the solve: min 0.5*|x|^2 subject to
// sum(x) == 3 on a [-2, 2] box, started at the origin. f(x0) = 0 and
// f(x*) = 1.125, so under BestCriteriaModes::OBJ the best-scoring iterate is an
// EARLY one while the solve still exits CONVERGED. That pairing -- BestIter !=
// last on a converged exit -- is what the pin below needs, and it is not
// reachable with the default ECONS criterion.
struct BestIterateRisingObjectiveProblem : NLPProblem {
    static constexpr int kN = 4;

    int num_vars() const override { return kN; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return kN; }
    int num_hess_nonzeros() const override { return kN; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-2.0);
        xu.setConstant(2.0);
        gl.setConstant(3.0);
        gu.setConstant(3.0);
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm();
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x.sum();
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = 0;
            c[i] = i;
        }
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        for (int i = 0; i < kN; i++) {
            r[i] = i;
            c[i] = i;
        }
    }
    void eval_jac(ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(1.0);
    }
    void eval_hess(ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(obj_factor);
    }
    std::string name() const override { return "BestIterateRisingObjective"; }
};

// THE REPORTED ITERATE IS ONE ITERATE. alg_impl's return_best_ substitution --
// the one that makes primals_, obj_val_ and the multiplier blocks describe
// BestIter -- is guarded on the exit NOT being converged, so on a CONVERGED
// exit the result describes the LAST iterate even with return_best_ on, and the
// four residuals must come from that same row. Selecting them unconditionally
// on the setting would report an objective measured at one point beside
// residuals measured at another.
//
// THE FIXTURE PREMISE IS ASSERTED, NOT ASSUMED: the test re-derives BestIter
// from the iterate stream under track_best_iterate's own rule (minimum, ties to
// the LATEST, since it updates on <=) and refuses to proceed unless that pick
// really differs from the last row and really carries different residuals.
TEST(NLPSolverTest, TheReportedKktResidualsDescribeTheIterateTheResultDescribes) {
    NLPSolver solver(std::make_shared<BestIterateRisingObjectiveProblem>());
    solver.optimizer_->set_print_level(10);
    solver.optimizer_->settings().return_best_ = true;
    solver.optimizer_->set_best_criteria(
        hven::solvers::InteriorPointSolver::BestCriteriaModes::OBJ);

    std::vector<hven::solvers::IterateInfo> rows;
    solver.optimizer_->set_late_callback([&rows](const hven::solvers::IterateInfo &info,
                                                 hven::ConstEigenRef<Eigen::VectorXd>,
                                                 hven::ConstEigenRef<Eigen::VectorXd>) {
        rows.push_back(info);
        return 0;
    });

    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(BestIterateRisingObjectiveProblem::kN);
    ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
    ASSERT_GE(rows.size(), 2u);

    std::size_t best = 0;
    for (std::size_t i = 1; i < rows.size(); i++) {
        if (rows[i].prim_obj_ <= rows[best].prim_obj_) {
            best = i;
        }
    }
    const std::size_t last = rows.size() - 1;
    ASSERT_NE(best, last) << "fixture premise: the OBJ criterion must pick an EARLIER iterate "
                             "than the converged one, or this pin proves nothing";
    ASSERT_NE(rows[best].kkt_inf_, rows[last].kkt_inf_)
        << "fixture premise: the two candidate rows must carry DIFFERENT residuals";

    const auto &result = solver.optimizer_->result();

    // THE PIN: the four residuals are the LAST iterate's -- the one the result
    // describes on a converged exit -- and not the best-scoring one's.
    EXPECT_EQ(result.kkt_inf_, rows[last].kkt_inf_);
    EXPECT_EQ(result.barr_inf_, rows[last].barr_inf_);
    EXPECT_EQ(result.econ_inf_, rows[last].econ_inf_);
    EXPECT_EQ(result.icon_inf_, rows[last].icon_inf_);
    EXPECT_NE(result.kkt_inf_, rows[best].kkt_inf_);

    // AND THE OBJECTIVE AGREES WITH THEM: one point, one set of numbers. The
    // scale is 1 here, so obj_val_ is prim_obj_ unmodified.
    EXPECT_EQ(result.obj_val_, rows[last].prim_obj_);
}

// NaN IS THE UNMEASURED SENTINEL, not 0.0, matching the SQP side so one outcome
// record reads both engines the same way: a zeroed residual on an engine that
// never measured one reads as a converged solve.
TEST(NLPSolverTest, TheKktResidualsOfASolverThatHasNotSolvedAreUnmeasured) {
    NLPSolver solver(std::make_shared<Hs071Problem>());
    solver.optimizer_->set_print_level(10);

    const auto &result = solver.optimizer_->result();
    EXPECT_TRUE(std::isnan(result.kkt_inf_));
    EXPECT_TRUE(std::isnan(result.barr_inf_));
    EXPECT_TRUE(std::isnan(result.econ_inf_));
    EXPECT_TRUE(std::isnan(result.icon_inf_));
}

///////////////////////////////////////////////////////////////////////////////
// THE FOLDED SURFACE (M6 W5 T1): what OptimizationProblemBase declared and
// NLPSolver now owns outright -- the job-mode vocabulary and its four refusal
// messages, the jet lifecycle's ORDER, and the five modes' semantics.
//
// None of it was pinned before the fold: a grep of the test tree for
// strto_jet_job_mode, jet_run, DoNothing or NotSet found nothing. These are
// new assertions about old behaviour, taken from the base's own contract.
//
// They land WITH the fold so that the two commits after it are checked by
// them, rather than by the suite those commits inherited.
///////////////////////////////////////////////////////////////////////////////

using JetJobModes = NLPSolver::JetJobModes;

// (1) THE VOCABULARY. Every spelling the parser's doc block lists, round-
// tripped; the refusal carries the spelling it refused.
TEST(NLPSolverJobModeTest, EveryAcceptedSpellingParsesToItsMode) {
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("solve"), JetJobModes::Solve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Solve"), JetJobModes::Solve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("optimize"), JetJobModes::Optimize);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Optimize"), JetJobModes::Optimize);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("solve_optimize"), JetJobModes::SolveOptimize);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("SolveOptimize"), JetJobModes::SolveOptimize);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Solve_Optimize"), JetJobModes::SolveOptimize);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("solve_optimize_solve"),
              JetJobModes::SolveOptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("SolveOptimizeSolve"), JetJobModes::SolveOptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Solve_Optimize_Solve"),
              JetJobModes::SolveOptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("optimize_solve"), JetJobModes::OptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("OptimizeSolve"), JetJobModes::OptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Optimize_Solve"), JetJobModes::OptimizeSolve);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("DoNothing"), JetJobModes::DoNothing);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("do_nothing"), JetJobModes::DoNothing);
    EXPECT_EQ(NLPSolver::strto_jet_job_mode("Do_Nothing"), JetJobModes::DoNothing);
}

TEST(NLPSolverJobModeTest, AnUnknownSpellingIsRefusedAndNamedInTheMessage) {
    try {
        NLPSolver::strto_jet_job_mode("Solve_Then_Give_Up");
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string what(e.what());
        EXPECT_NE(what.find("Unrecognized jet_job_mode: "), std::string::npos) << what;
        EXPECT_NE(what.find("Solve_Then_Give_Up"), std::string::npos) << what;
    }
}

// The string overload of the setter is the parser plus the enum setter, and
// nothing else: an accepted spelling lands on jet_job_mode_ and a refused one
// leaves it where it was.
TEST(NLPSolverJobModeTest, TheStringSetterParsesAndTheRefusalLeavesTheModeAlone) {
    NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    EXPECT_EQ(solver.jet_job_mode_, JetJobModes::NotSet);

    solver.set_jet_job_mode("Solve_Optimize");
    EXPECT_EQ(solver.jet_job_mode_, JetJobModes::SolveOptimize);

    EXPECT_THROW(solver.set_jet_job_mode("nonsense"), std::invalid_argument);
    EXPECT_EQ(solver.jet_job_mode_, JetJobModes::SolveOptimize);

    solver.set_jet_job_mode(JetJobModes::OptimizeSolve);
    EXPECT_EQ(solver.jet_job_mode_, JetJobModes::OptimizeSolve);
}

// (2) THE TWO REFUSALS THAT ARE NOT THE PARSER'S. DoNothing parses -- it is a
// named enumerator -- and is then dispatched by nothing, on two different
// paths with two different messages. NotSet is the third.
TEST(NLPSolverJobModeTest, JetRunRefusesNotSetAndDoNothingWithDistinctMessages) {
    NLPSolver notset(std::make_shared<EqOnlyProblem>());
    notset.optimizer_->set_print_level(10);
    try {
        notset.jet_run();
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_EQ(std::string(e.what()), "jet_job_mode_ not set");
    }

    NLPSolver donothing(std::make_shared<EqOnlyProblem>());
    donothing.optimizer_->set_print_level(10);
    donothing.set_jet_job_mode(JetJobModes::DoNothing);
    try {
        donothing.jet_run();
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_EQ(std::string(e.what()), "Unrecognized jet_job_mode");
    }
}

TEST(NLPSolverJobModeTest, RunNlpSolverRefusesDoNothingAndNotSetWithItsOwnMessage) {
    NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    for (JetJobModes mode : {JetJobModes::DoNothing, JetJobModes::NotSet}) {
        try {
            solver.run_nlp_solver(mode, x0);
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_EQ(std::string(e.what()), "Unrecognized NLP solve mode");
        }
    }
}

// (3) THE JET LIFECYCLE'S ORDER, made observable. num_partitions_ == 1 after
// jet_run() holds whichever order initialize and release ran in, so it is not
// the pin; the transcription COUNT is.
//
// jet_initialize() transcribes and clears do_transcription_; run() transcribes
// only when it is set; jet_release() sets it again and nulls nlp_.
//
// So exactly one transcription per jet_run() is initialize-mode-release and
// nothing else: a release that ran before the mode would show two.
TEST(NLPSolverJobModeTest, JetRunTranscribesExactlyOnceAndReleasesAfterTheMode) {
    auto problem = std::make_shared<TranscriptionCountingProblem>();
    NLPSolver solver(problem);
    solver.optimizer_->set_print_level(10);
    solver.set_jet_job_mode(JetJobModes::Optimize);
    solver.active_variables_ = Eigen::VectorXd::Zero(2);

    ASSERT_EQ(solver.jet_run(), hven::ConvergenceFlags::CONVERGED);

    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);
    EXPECT_TRUE(solver.do_transcription_);
    EXPECT_EQ(solver.nlp_, nullptr);
    EXPECT_EQ(solver.num_partitions_, 1);

    ASSERT_EQ(solver.jet_run(), hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(problem->n_bounds_, 2);
    EXPECT_TRUE(solver.do_transcription_);
    EXPECT_EQ(solver.nlp_, nullptr);
    solver.optimizer_->set_print_level(10);
}

// (4) THE MODE SEMANTICS, as the folded base documented them.
//
// The observable is NOT the ipm.solve pair: one is written per entry-point
// call however many phases run, and its `phases` counts the phases REQUESTED,
// a conditional one later skipped included.
//
// It is `ipm.iter`'s `phase` key. The driver takes current_phase_idx BEFORE
// the conditional-skip check, so a skipped step leaves a GAP in the phases
// that reach the sink, and every executed phase writes an iteration line.
struct IpmPhaseRecordingSink : hven::solvers::IpqpTraceSink {
    std::vector<int> iter_phases_;
    int begin_phases_ = -1;
    int begins_ = 0;
    int ends_ = 0;

    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &event) override {
        this->iter_phases_.push_back(static_cast<int>(event.phase));
    }
    void on_ipm_solve_begin(const hven::solvers::IpmSolveBeginTraceEvent &event) override {
        this->begin_phases_ = static_cast<int>(event.phases);
        this->begins_++;
    }
    void on_ipm_solve_end(const hven::solvers::IpmSolveEndTraceEvent &) override { this->ends_++; }

    // The eight QP-side events are pure on the sink and unreachable from the
    // interior-point driver; they are stubbed, not recorded.
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}

    std::vector<int> distinct_phases() const {
        std::set<int> s(this->iter_phases_.begin(), this->iter_phases_.end());
        return std::vector<int>(s.begin(), s.end());
    }
};

namespace {

/// Runs one entry point on a fresh EqOnlyProblem solver with a phase-recording
/// sink attached, optionally capped at @p max_iters so the OPT phase cannot
/// report CONVERGED, and returns the sink.
IpmPhaseRecordingSink run_with_phase_sink(JetJobModes mode, int max_iters,
                                          hven::ConvergenceFlags *flag_out) {
    NLPSolver solver(std::make_shared<EqOnlyProblem>());
    solver.optimizer_->set_print_level(10);
    if (max_iters > 0) {
        solver.optimizer_->set_max_iters(max_iters);
    }
    // run_nlp_solver IS the dispatch point under test, and it is below the
    // lazy-transcription step run() performs, so the program is adopted here.
    solver.transcribe();
    IpmPhaseRecordingSink sink;
    solver.optimizer_->attach_trace(&sink);
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    *flag_out = solver.run_nlp_solver(mode, x0).flag_;
    return sink;
}

} // namespace

TEST(NLPSolverModeSemanticsTest, SolveOptimizeSolveSkipsTheTrailingSoeOnlyWhenOptConverged) {
    hven::ConvergenceFlags flag = hven::ConvergenceFlags::NOTCONVERGED;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink(JetJobModes::SolveOptimizeSolve, 0, &flag);
    ASSERT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(converged.begins_, 1);
    EXPECT_EQ(converged.ends_, 1);
    EXPECT_EQ(converged.begin_phases_, 3); // three REQUESTED, one of them skipped
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0, 1}));

    const IpmPhaseRecordingSink capped =
        run_with_phase_sink(JetJobModes::SolveOptimizeSolve, 1, &flag);
    ASSERT_NE(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(capped.begin_phases_, 3);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1, 2}));
}

TEST(NLPSolverModeSemanticsTest, OptimizeSolveSkipsTheTrailingSoeOnlyWhenOptConverged) {
    hven::ConvergenceFlags flag = hven::ConvergenceFlags::NOTCONVERGED;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink(JetJobModes::OptimizeSolve, 0, &flag);
    ASSERT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(converged.begin_phases_, 2);
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0}));

    const IpmPhaseRecordingSink capped = run_with_phase_sink(JetJobModes::OptimizeSolve, 1, &flag);
    ASSERT_NE(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(capped.begin_phases_, 2);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1}));
}

// solve_optimize has no conditional step at all: both phases run whatever the
// OPT phase reported, which is what separates it from the two above.
TEST(NLPSolverModeSemanticsTest, SolveOptimizeAlwaysRunsBothPhases) {
    hven::ConvergenceFlags flag = hven::ConvergenceFlags::NOTCONVERGED;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink(JetJobModes::SolveOptimize, 0, &flag);
    ASSERT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(converged.begin_phases_, 2);
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0, 1}));

    const IpmPhaseRecordingSink capped = run_with_phase_sink(JetJobModes::SolveOptimize, 1, &flag);
    ASSERT_NE(flag, hven::ConvergenceFlags::CONVERGED);
    EXPECT_EQ(capped.begin_phases_, 2);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1}));
}

// The two single-phase modes, for the contrast: one requested phase, one
// executed, and nothing conditional to skip.
TEST(NLPSolverModeSemanticsTest, SolveAndOptimizeEachRunExactlyOnePhase) {
    hven::ConvergenceFlags flag = hven::ConvergenceFlags::NOTCONVERGED;
    for (JetJobModes mode : {JetJobModes::Solve, JetJobModes::Optimize}) {
        const IpmPhaseRecordingSink sink = run_with_phase_sink(mode, 0, &flag);
        ASSERT_EQ(flag, hven::ConvergenceFlags::CONVERGED);
        EXPECT_EQ(sink.begins_, 1);
        EXPECT_EQ(sink.ends_, 1);
        EXPECT_EQ(sink.begin_phases_, 1);
        EXPECT_EQ(sink.distinct_phases(), (std::vector<int>{0}));
    }
}
