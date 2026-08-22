#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>

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

static_assert(SetsPartitionsAlone<hven::solvers::OptimizationProblemBase>);
static_assert(!SetsPartitionsAndQpThreads<hven::solvers::OptimizationProblemBase>);
static_assert(SetsPartitionsAlone<NLPSolver>);
static_assert(!SetsPartitionsAndQpThreads<NLPSolver>);

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

    // A fault on a LATER transcription leaves the standing one whole -- same
    // three objects, still consistent with each other -- and leaves the retry
    // flag set rather than half-replacing the solver.
    problem->armed_ = true;
    solver.do_transcription_ = true;
    EXPECT_THROW(solver.optimize(x0), std::runtime_error);
    EXPECT_EQ(solver.model_, model_after);
    EXPECT_EQ(solver.core_, core_after);
    EXPECT_EQ(solver.nlp_, nlp_after);
    EXPECT_TRUE(solver.do_transcription_);

    problem->armed_ = false;
    EXPECT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED);
}
