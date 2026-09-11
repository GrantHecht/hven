// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <type_traits>
#include <vector>

#include "hven/detail/model/nlp_adapter.h"
#include "hven/drivers/interior_point_solver.h"
#include "hven/drivers/solve_status.h"
#include "hven/drivers/trace.h"
#include "hven/model/nlp_problem.h"
#include "hven/model/non_linear_program.h"

#include "declared_route.h" // NOLINT(build/include_subdir)

namespace {
constexpr double kSolverInf = std::numeric_limits<double>::infinity();
} // namespace

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;

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

TEST(IpmSolverEntry, Hs071ConvergesToKnownOptimum) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    result = solver.solve(*program, x0);
    auto flag = result.status;
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd x = result.x;
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
TEST(IpmSolverEntry, Hs071BoundDualsMatchActiveLowerBound) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

    const Eigen::VectorXd &z = result.z;
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

TEST(IpmSolverEntry, RosenbrockConvergesToKnownOptimum) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<RosenbrockProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(2);
    x0 << -1.2, 1.0;
    result = solver.solve(*program, x0);
    auto flag = result.status;
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd x = result.x;
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

TEST(IpmSolverEntry, EqualityMultiplierHasIpoptSign) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    result = solver.solve(*route.program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_NEAR(route.model->compose_user_multipliers(result.lambda_e, result.lambda_i)[0], -2.0,
                1e-5);
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

TEST(IpmSolverEntry, LowerBoundedRowActiveWithNegativeIpoptMultiplier) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<LowerBoundRowProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(1);
    x0 << 3.0;
    result = solver.solve(*route.program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_NEAR(result.x[0], 1.0, 1e-5);
    EXPECT_NEAR(route.model->compose_user_multipliers(result.lambda_e, result.lambda_i)[0], -2.0,
                1e-5);
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

TEST(IpmSolverEntry, RangeRowActiveAtUpperWithPositiveIpoptMultiplier) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<RangeRowProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(1);
    x0 << 1.5;
    result = solver.solve(*route.program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_NEAR(result.x[0], 2.0, 1e-5);
    EXPECT_NEAR(route.model->compose_user_multipliers(result.lambda_e, result.lambda_i)[0], 2.0,
                1e-5);
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

TEST(IpmSolverEntry, FreeRowDroppedFromTranscriptionReadsZeroMultiplier) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<FreeRowProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(1);
    x0 << 0.0;
    result = solver.solve(*route.program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_NEAR(result.x[0], 5.0, 1e-5);
    EXPECT_NEAR(route.model->compose_user_multipliers(result.lambda_e, result.lambda_i)[0], 0.0,
                1e-14);
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

TEST(IpmSolverEntry, FixedVariableSolvesExactlyAtItsFixedValue) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<FixedVarProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(2);
    x0 << 0.0, 3.0;
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd x = result.x;
    EXPECT_NEAR(x[0], 1.0, 1e-6);
    EXPECT_NEAR(x[1], 3.0, 1e-12);
}

// SolveResult::fixed_variable_treatment_ pin, using the same fixture: under
// the default MakeParameter treatment the fixed variable is eliminated (no
// row added), so eq_lmults_ stays empty; switched to MakeConstraint, the one
// fixed variable becomes one internal fixing row, so eq_lmults_ grows to
// exactly one entry -- pinning both the recorded treatment and the
// treatment-dependent shape documented on eq_lmults_.
TEST(IpmSolverEntry, FixedVariableTreatmentIsRecordedOnSolveResult) {
    {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<FixedVarProblem>());
        hven::solvers::InteriorPointSolver solver;
        hven::solvers::IpmResult result;
        {
            auto o = solver.options();
            o.common.print_level = 10;
            solver.set_options(std::move(o));
        }
        Eigen::VectorXd x0(2);
        x0 << 0.0, 3.0;
        result = solver.solve(*program, x0);
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
        EXPECT_EQ(result.fixed_variable_treatment,
                  hven::solvers::FixedVariableTreatments::MakeParameter);
        EXPECT_EQ(result.lambda_e.size(), 0);
    }
    {
        const auto program = hven::solvers::make_nlp_program(std::make_shared<FixedVarProblem>());
        hven::solvers::InteriorPointSolver solver;
        hven::solvers::IpmResult result;
        {
            auto o = solver.options();
            o.common.print_level = 10;
            o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::MakeConstraint;
            solver.set_options(std::move(o));
        }
        Eigen::VectorXd x0(2);
        x0 << 0.0, 3.0;
        result = solver.solve(*program, x0);
        ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
        EXPECT_EQ(result.fixed_variable_treatment,
                  hven::solvers::FixedVariableTreatments::MakeConstraint);
        // THE DECLARED BLOCK IS STILL EMPTY (M6 W5 T8.4): the problem declares
        // no equality row, and the one row MakeConstraint adds is the
        // TREATMENT's -- reported beside the base, not inside it.
        EXPECT_EQ(result.lambda_e.size(), 0);
        EXPECT_EQ(result.internal_fixed_lambda_e.size(), 1);
        EXPECT_EQ(result.internal_fixed_ce.size(), 1);
    }
}

// IpmResult::z pin against the RelaxBounds -> MakeParameter treatment switch.
// Under RelaxBounds the fixed variable is NOT eliminated, so it reaches the
// solver as a widened two-sided bound and carries a price; switched to
// MakeParameter on the SAME solver instance, the variable is eliminated
// instead, the bound set goes back to null, and the second solve must not
// report the first solve's price.
//
// WHAT THE PIN ASSERTS CHANGED IN M6 W5 T8.4, and for the better: the base's z
// is DECLARED-WIDTH unconditionally -- an eliminated coordinate has a 0 there,
// which is what "the reduced problem has no row for it" means in the caller's
// space -- so the failure mode this guarded (a stale REDUCED-width block
// standing at the wrong length) is impossible by construction. What is left to
// assert, and what this now asserts, is the value: zero at the eliminated
// coordinate, from a solve that never priced it.
TEST(IpmSolverEntry, ZIsDeclaredWidthAndZeroAtAnEliminatedCoordinate) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<FixedVarProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::RelaxBounds;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(2);
    x0 << 0.0, 3.0;

    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    ASSERT_EQ(result.z.size(), 2) << "declared width, both treatments";
    const double relaxed_price = result.z[1];

    {
        auto o = solver.options();
        o.fixed_variable_treatment = hven::solvers::FixedVariableTreatments::MakeParameter;
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    ASSERT_EQ(result.z.size(), 2);
    EXPECT_EQ(result.z[1], 0.0)
        << "an eliminated coordinate is not priced, and the previous solve's price for it "
           "("
        << relaxed_price << ") must not survive";
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

TEST(IpmSolverEntry, SeededSolveConverges) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<SeededEqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    const hven::solvers::IpmResult result = hven_interior_tests::solve_declared(solver, route, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    Eigen::VectorXd expect(2);
    expect << 1.0, 1.0;
    EXPECT_LT((result.x - expect).lpNorm<Eigen::Infinity>(), 1e-6);
    // return_multipliers()'s replacement, named in the migration guide.
    EXPECT_NEAR(route.model->compose_user_multipliers(result.lambda_e, result.lambda_i)[0], -2.0,
                1e-5);
}

// THE NO-ARG ENTRY POINTS ARE GONE WITH THE WRAPPER (M6 W5 T8.9). They reused
// a start point the wrapper kept between calls; the engine keeps nothing and
// every solve names its own. What survives, and what the second half of the
// retired pair really pinned, is the SIZE REFUSAL: a start point that is not
// the program's variable count is refused rather than solved from.
TEST(IpmSolverEntry, TwoSolvesFromTheSameStartPointAgree) {
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    const auto first_program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver first;
    hven::solvers::IpmResult first_result;
    {
        auto o = first.options();
        o.common.print_level = 10;
        first.set_options(std::move(o));
    }
    const hven::solvers::IpmResult a = first.solve(*first_program, x0);
    ASSERT_EQ(a.status, hven::solvers::SolveStatus::kOptimal);

    const auto second_program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver second;
    hven::solvers::IpmResult second_result;
    {
        auto o = second.options();
        o.common.print_level = 10;
        second.set_options(std::move(o));
    }
    const hven::solvers::IpmResult b = second.solve(*second_program, x0);
    ASSERT_EQ(b.status, hven::solvers::SolveStatus::kOptimal);

    EXPECT_LT((b.x - a.x).lpNorm<Eigen::Infinity>(), 1e-8);
}

TEST(IpmSolverEntry, AStartPointOfTheWrongSizeIsRefused) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    EXPECT_THROW((void)solver.solve(*program, Eigen::VectorXd()), std::invalid_argument);
    EXPECT_THROW((void)solver.solve(*program, Eigen::VectorXd::Zero(3)), std::invalid_argument);
}

// THE WORKER PREPARATION, in place of the jet lifecycle (M6 W5 T8.9). The
// retired jet_initialize() set exactly one backend thread and print level 10,
// and forced a single partition; ipm_worker_options is the first two, and the
// partition count is the PROGRAM's -- which is why the test reads it off the
// program rather than off the preset.
TEST(WorkerOptions, JetPreparationEquivalent) {
    // (1) THE PRESET carries the two settings and nothing else: every other
    //     field of the value handed in survives.
    hven::solvers::IpmOptions base;
    base.common.threads = 7;
    base.common.print_level = 0;
    base.max_iters = 123;
    base.obj_scale = 2.5;
    const hven::solvers::IpmOptions worker = hven::solvers::ipm_worker_options(base);
    EXPECT_EQ(worker.common.threads, 1);
    EXPECT_EQ(worker.common.print_level, 10);
    EXPECT_EQ(worker.max_iters, 123);
    EXPECT_EQ(worker.obj_scale, 2.5);
    EXPECT_EQ(worker.common.start_level, base.common.start_level);

    // (2) THE SETTINGS IN FORCE, read off a CONFIGURED SOLVER rather than off
    //     the value: the preset is only worth anything if set_options accepts
    //     it and the solver reports it.
    const auto program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    solver.set_options(hven::solvers::ipm_worker_options(solver.options()));
    EXPECT_EQ(solver.options().common.threads, 1);
    EXPECT_EQ(solver.options().common.print_level, 10);

    // (3) THE THIRD SETTING IS THE PROGRAM'S, and its ADOPTED value is what
    //     negotiate_partition_count reports.
    EXPECT_EQ(program->negotiate_partition_count(1), 1);
    EXPECT_EQ(program->num_partitions_, 1);

    // ... and a worker-configured solve of the worker's own program still runs.
    const hven::solvers::IpmResult result = solver.solve(*program, Eigen::VectorXd::Zero(2));
    EXPECT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
}

// starting_multipliers() returning a non-finite entry must fail the
// allFinite() guard the seed route applies -- before the payload ever reaches
// the engine, and with a distinct message. Since M6 W5 T8.9 that route is the
// caller's (declared_route.h's starting_multiplier_seed).
struct NonFiniteSeedProblem : EqOnlyProblem {
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda[0] = std::numeric_limits<double>::quiet_NaN();
        return true;
    }
    std::string name() const override { return "NonFiniteSeedProblem"; }
};

TEST(IpmSolverEntry, NonFiniteStartingMultipliersThrow) {
    const auto route = hven_interior_tests::transcribe(std::make_shared<NonFiniteSeedProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    try {
        (void)hven_interior_tests::solve_declared(solver, route, x0);
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("non-finite"), std::string::npos);
    }
}

// Partition count and QP thread count are independent settings on two different
// OBJECTS -- the partition count on the PROGRAM, the thread count in the
// solver's own options value -- and neither touches the other's state. That was
// true of the retired wrapper's two fields and is true of the two objects that
// replaced them, which is what this pins.
//
// AND THE PARTITION COUNT IS CLAMPED, NOT REFUSED (M6 W5 T8.9): make_nlp
// caps it at num_user_kkt_elems_ / kMinKktElementsPerPartition, so a problem
// this small ADOPTS 1 whatever is asked for. The adopted count is read off the
// program; the REQUEST is not an answer. A non-positive request is a different
// thing and is refused by name.
TEST(IpmSolverEntry, PartitionCountAndQpThreadCountAreSetIndependently) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>(), 2);
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    // EqOnlyProblem is far below the 1000-element-per-partition floor, so the
    // adopted count is 1 however many were asked for.
    EXPECT_EQ(program->num_partitions_, 1);
    EXPECT_EQ(program->declaration().partition_count_, program->num_partitions_);

    {
        auto o = solver.options();
        o.common.threads = 3;
        solver.set_options(std::move(o));
    }
    EXPECT_EQ(program->negotiate_partition_count(2), 1) << "clamped, and reported as adopted";
    EXPECT_EQ(solver.options().common.threads, 3); // partitions left it alone

    {
        auto o = solver.options();
        o.common.threads = 1;
        solver.set_options(std::move(o));
    }
    EXPECT_EQ(solver.options().common.threads, 1);
    EXPECT_EQ(program->num_partitions_, 1); // and the QP setter left partitions alone

    EXPECT_THROW(program->negotiate_partition_count(0), std::invalid_argument);
    EXPECT_THROW(hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>(), 0),
                 std::invalid_argument);
    {
        auto o = solver.options();
        o.common.threads = 0;
        EXPECT_THROW(solver.set_options(o), std::invalid_argument);
    }

    // The worker preset puts the solver on one QP thread; the program's own
    // single partition is negotiated beside it, not by it.
    {
        auto o = solver.options();
        o.common.threads = 4;
        solver.set_options(std::move(o));
    }
    solver.set_options(hven::solvers::ipm_worker_options(solver.options()));
    EXPECT_EQ(program->negotiate_partition_count(1), 1);
    EXPECT_EQ(solver.options().common.threads, 1);
    EXPECT_EQ(solver.options().common.print_level, 10);
}

// validate()'s checks for bound_push, alpha_red, delta_h, incr_h,
// bound_fraction and decr_h are written as negated comparisons so that a NaN,
// which compares false against every ordinary relational operator, is
// refused rather than silently accepted and stored. Before M6 W5 T8.3 these
// were the site-named set_*() methods; the checks and the messages are the
// same ones, reached now through set_options().
TEST(InteriorPointSolverSettingsTest, NaNRejectedBySiteNamedSetters) {
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    const double nan = std::numeric_limits<double>::quiet_NaN();

    try {
        {
            auto o = solver.options();
            o.bound_push = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.alpha_red = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("alpha_red"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.delta_h = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.incr_h = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.bound_fraction = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_fraction"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.decr_h = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("decr_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.bound_interval_push = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_interval_push"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.bound_relax_factor = nan;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_relax_factor"), std::string::npos);
    }

    // set_hpert_params delegates to the three setters above, so a NaN in any
    // one argument is refused too -- naming that argument's own setter site,
    // since it is set_delta_h/set_incr_h/set_decr_h that actually throws.
    try {
        {
            auto o = solver.options();
            o.delta_h = nan;
            o.incr_h = 8.0;
            o.decr_h = 0.1;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.delta_h = 1e-4;
            o.incr_h = nan;
            o.decr_h = 0.1;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.delta_h = 1e-4;
            o.incr_h = 8.0;
            o.decr_h = nan;
            solver.set_options(std::move(o));
        }
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
    hven::solvers::IpmResult result;
    const double inf = std::numeric_limits<double>::infinity();

    try {
        {
            auto o = solver.options();
            o.bound_push = inf;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.alpha_red = inf;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("alpha_red"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.delta_h = inf;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("delta_h"), std::string::npos);
    }
    try {
        {
            auto o = solver.options();
            o.incr_h = inf;
            solver.set_options(std::move(o));
        }
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        EXPECT_NE(std::string(e.what()).find("incr_h"), std::string::npos);
    }
}

// validate() refuses a NaN for one representative field per helper family that
// has a numeric double-valued invariant. Before M6 W5 T8.3 this was "the other
// half of the twice-checked pairing" -- the half that caught a NaN written
// directly through the mutable settings() reference, bypassing the per-field
// setter. There is no such reference and no such setter any more: validate() is
// the single door, run from set_options() and again at solve entry, and this
// test calls it directly.
TEST(InteriorPointSolverSettingsTest, NaNRejectedByValidateForEveryHelperFamily) {
    const double nan = std::numeric_limits<double>::quiet_NaN();

    {
        hven::solvers::IpmOptions o; // pos_finite
        o.kkt_tol = nan;
        try {
            hven::solvers::validate(o);
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("kkt_tol"), std::string::npos);
        }
    }
    {
        hven::solvers::IpmOptions o; // in_open_unit
        o.bound_fraction = nan;
        try {
            hven::solvers::validate(o);
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_fraction"), std::string::npos);
        }
    }
    {
        hven::solvers::IpmOptions o; // greater_than
        o.bound_push = nan;
        try {
            hven::solvers::validate(o);
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_push"), std::string::npos);
        }
    }
    {
        hven::solvers::IpmOptions o; // in_open_interval
        o.bound_interval_push = nan;
        try {
            hven::solvers::validate(o);
            FAIL() << "expected std::invalid_argument";
        } catch (const std::invalid_argument &e) {
            EXPECT_NE(std::string(e.what()).find("bound_interval_push"), std::string::npos);
        }
    }
    {
        hven::solvers::IpmOptions o; // in_closed_interval
        o.bound_relax_factor = nan;
        try {
            hven::solvers::validate(o);
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

TEST(IpmSolverEntry, AFaultedTranscriptionCommitsNothingAndRetriesCleanly) {
    auto problem = std::make_shared<FaultingSetupProblem>();

    // A fault on the very first transcription commits nothing at all: the call
    // that builds the program is the call that throws, and it hands back
    // nothing (M6 W5 T8.9 -- the wrapper's three members and its retry flag are
    // gone, and with them the whole class of half-replaced state they guarded).
    EXPECT_THROW((void)hven::solvers::make_nlp_program(problem), std::runtime_error);

    // Clearing the fault and retrying succeeds; nothing had to be reset by hand.
    problem->armed_ = false;
    const auto program = hven::solvers::make_nlp_program(problem);
    ASSERT_NE(program, nullptr);

    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    ASSERT_EQ(solver.solve(*program, x0).status, hven::solvers::SolveStatus::kOptimal);

    // A fault on a LATER transcription leaves the standing program whole -- the
    // caller still holds the same object, and the failed call produced no
    // second one to be confused with it.
    problem->armed_ = true;
    EXPECT_THROW((void)hven::solvers::make_nlp_program(problem), std::runtime_error);
    ASSERT_NE(program, nullptr);

    // ... and a solve against the standing program still runs, because nothing
    // about the failed attempt reached it.
    problem->armed_ = false;
    EXPECT_EQ(solver.solve(*program, x0).status, hven::solvers::SolveStatus::kOptimal);

    // A fresh transcription is a fresh object; the old one is untouched.
    const auto replacement = hven::solvers::make_nlp_program(problem);
    EXPECT_NE(replacement, program);
    EXPECT_EQ(solver.solve(*replacement, x0).status, hven::solvers::SolveStatus::kOptimal);
}

// Counts the setup-only queries a transcription makes of the problem. bounds,
// jac_structure and hess_structure are asked exactly once per transcription
// and never during a solve, so their totals are a direct count of how many
// transcriptions have happened.
struct TranscriptionCountingProblem : EqOnlyProblem {
    mutable int n_bounds_ = 0, n_jac_structure_ = 0, n_hess_structure_ = 0;
    mutable int n_eval_jac_ = 0, n_eval_hess_ = 0;

    /// Set by the partition pin only. When it is set, every eval_jac call
    /// records the PROGRAM's partition count AT THAT MOMENT -- an observation
    /// made DURING the solve, which is where the count the evaluations actually
    /// run under is visible.
    const hven::solvers::NonLinearProgram *watch_ = nullptr;
    mutable std::vector<int> partitions_during_eval_;

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
        if (watch_ != nullptr) {
            partitions_during_eval_.push_back(watch_->num_partitions_);
        }
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

TEST(IpmSolverEntry, ASecondSolveTranscribesNothingAndSpendsNoFurtherSetupEvaluation) {
    auto problem = std::make_shared<TranscriptionCountingProblem>();
    const auto program = hven::solvers::make_nlp_program(problem);
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);

    // Exactly one transcription, made by the CALLER before either solve: one
    // bounds query, one of each structure.
    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);

    ASSERT_EQ(solver.solve(*program, x0).status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(problem->n_bounds_, 1);
    const int jac_after_first = problem->n_eval_jac_;
    const int hess_after_first = problem->n_eval_hess_;

    ASSERT_EQ(solver.solve(*program, x0).status, hven::solvers::SolveStatus::kOptimal);
    // Nothing was transcribed a second time, so no setup evaluation happened a
    // second time either: the counts that a transcription and only a
    // transcription moves are unchanged.
    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);
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
TEST(IpmSolverEntry, TheReportedKktResidualsAreTheOnesTheConvergenceTestGated) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

    const auto &settings = solver.options();

    EXPECT_TRUE(std::isfinite(result.kkt_inf));
    EXPECT_TRUE(std::isfinite(result.barr_inf));
    EXPECT_TRUE(std::isfinite(result.econ_inf));
    EXPECT_TRUE(std::isfinite(result.icon_inf));

    EXPECT_LT(result.kkt_inf, settings.kkt_tol);
    EXPECT_LT(result.barr_inf, settings.bar_tol);
    EXPECT_LT(result.econ_inf, settings.econ_tol);
    EXPECT_LT(result.icon_inf, settings.icon_tol);

    // A residual is a norm: never negative, whatever the exit.
    EXPECT_GE(result.kkt_inf, 0.0);
    EXPECT_GE(result.barr_inf, 0.0);
    EXPECT_GE(result.econ_inf, 0.0);
    EXPECT_GE(result.icon_inf, 0.0);

    // HS071 has both row kinds, so neither constraint residual is the vacuous
    // "no rows, therefore zero" reading.
    ASSERT_EQ(result.ce.size(), 1);
    ASSERT_EQ(result.ci.size(), 1);

    // A SECOND CALL RE-REPORTS. reset_accumulators() clears the four at solve
    // entry, so a stale reading cannot survive into a call that never wrote
    // them; here the second call writes them again and lands inside the same
    // gates.
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_LT(result.kkt_inf, settings.kkt_tol);
    EXPECT_LT(result.barr_inf, settings.bar_tol);
    EXPECT_LT(result.econ_inf, settings.econ_tol);
    EXPECT_LT(result.icon_inf, settings.icon_tol);
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

// A sink that keeps every `ipm.iter` record (M6 W5 T8.6). One event per
// iterate, in order -- the same rows the per-iteration callback is shown, and
// the only stream that still carries this engine's OWN residual columns.
namespace {
class IterateRecordingSink : public hven::solvers::TraceSink {
  public:
    std::vector<hven::solvers::IterateInfo> rows;
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &e) override {
        rows.push_back(e.iterate);
    }
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override {}
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override {}
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override {}
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override {}
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override {}
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override {}
    void on_qp_mode(const hven::solvers::QpModeTraceEvent &) override {}
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {}
};
} // namespace

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
TEST(IpmSolverEntry, TheReportedKktResidualsDescribeTheIterateTheResultDescribes) {
    const auto program =
        hven::solvers::make_nlp_program(std::make_shared<BestIterateRisingObjectiveProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    {
        auto o = solver.options();
        o.return_best = true;
        solver.set_options(std::move(o));
    }
    {
        auto o = solver.options();
        o.best_criteria = hven::solvers::InteriorPointSolver::BestCriteriaModes::OBJ;
        solver.set_options(std::move(o));
    }

    // M6 W5 T8.6: THE ITERATE STREAM COMES FROM THE TRACE NOW, not from the
    // late callback this test used to install. The four residuals it reads are
    // this ENGINE's own (kkt_inf and friends), and the shared iteration
    // callback that replaced the late one deliberately does not carry them --
    // it carries the four DECLARED diagnostics instead. The `ipm.iter` event
    // does carry them, is emitted one per iterate exactly as the callback
    // fires, and hands out the record itself, so the pin below is unchanged in
    // substance and reads a stream that still has the fields it names.
    IterateRecordingSink rows_sink;
    solver.attach_trace(&rows_sink);
    const std::vector<hven::solvers::IterateInfo> &rows = rows_sink.rows;

    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(BestIterateRisingObjectiveProblem::kN);
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);
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

    // THE PIN: the four residuals are the LAST iterate's -- the one the result
    // describes on a converged exit -- and not the best-scoring one's.
    EXPECT_EQ(result.kkt_inf, rows[last].kkt_inf_);
    EXPECT_EQ(result.barr_inf, rows[last].barr_inf_);
    EXPECT_EQ(result.econ_inf, rows[last].econ_inf_);
    EXPECT_EQ(result.icon_inf, rows[last].icon_inf_);
    EXPECT_NE(result.kkt_inf, rows[best].kkt_inf_);

    // AND THE OBJECTIVE AGREES WITH THEM: one point, one set of numbers. The
    // scale is 1 here, so obj_val_ is prim_obj_ unmodified.
    EXPECT_EQ(result.f, rows[last].prim_obj_);
}

// NaN IS THE UNMEASURED SENTINEL, not 0.0, matching the SQP side so one outcome
// record reads both engines the same way: a zeroed residual on an engine that
// never measured one reads as a converged solve.
//
// M6 W5 T8.9: the engine returns its result by value and holds none, so what is
// read here is the value a solve has not written -- which is where the wrapper's
// retired result() read from too, before its first solve.
TEST(IpmSolverEntry, TheKktResidualsOfAResultNoSolveHasWrittenAreUnmeasured) {
    const hven::solvers::IpmResult result;
    EXPECT_TRUE(std::isnan(result.kkt_inf));
    EXPECT_TRUE(std::isnan(result.barr_inf));
    EXPECT_TRUE(std::isnan(result.econ_inf));
    EXPECT_TRUE(std::isnan(result.icon_inf));
}

///////////////////////////////////////////////////////////////////////////////
// WHAT THE RETIRED WRAPPER'S JOB-MODE SURFACE BECAME (M6 W5 T8.9).
//
// M6 W5 T1 folded OptimizationProblemBase into NLPSolver and pinned that
// surface for the first time: a job-mode vocabulary of five spellings plus two
// that dispatched to nothing, a jet lifecycle, and the five modes' semantics.
// T8.4 had already made the semantics an option (IpmOptions::phases) and T8.9
// deletes the wrapper, so the vocabulary and the lifecycle go with it. What
// survives is what they MEANT, and it is pinned below on the option: the five
// spellings are five phase sequences, the two non-dispatching ones are the
// EMPTY sequence validate() refuses, and the mode semantics -- which phase
// runs, which is conditional -- are asserted against the sequences themselves.
///////////////////////////////////////////////////////////////////////////////

using hven::solvers::IpmPhase;

// (1) THE VOCABULARY, as sequences.
TEST(IpmPhases, TheFiveRetiredJobModesAreFivePhaseSequences) {
    // THE MAPPING, WHICH IS THE WHOLE OF WHAT THE FIVE NAMES WERE (M6 W5 T8.4,
    // documented on IpmOptions::phases; M6 W5 T8.9 retired the names). Each
    // retired spelling is one sequence value, and a caller writes the sequence.
    using hven::solvers::IpmPhase;
    const std::vector<std::pair<const char *, std::vector<IpmPhase>>> table = {
        {"solve", {IpmPhase::kSolve}},
        {"optimize", {IpmPhase::kOptimize}},
        {"solve_optimize", {IpmPhase::kSolve, IpmPhase::kOptimize}},
        {"solve_optimize_solve", {IpmPhase::kSolve, IpmPhase::kOptimize, IpmPhase::kSolve}},
        {"optimize_solve", {IpmPhase::kOptimize, IpmPhase::kSolve}}};
    for (const auto &entry : table) {
        hven::solvers::IpmOptions o;
        o.phases = entry.second;
        EXPECT_NO_THROW(hven::solvers::validate(o)) << entry.first;
        EXPECT_EQ(o.phases.size(), entry.second.size()) << entry.first;
    }
    // `optimize` is the DEFAULT, which is what made it the wrapper's commonest
    // entry point.
    EXPECT_EQ(hven::solvers::IpmOptions{}.phases, (std::vector<IpmPhase>{IpmPhase::kOptimize}));
}

// The two spellings that parsed but dispatched to nothing -- `DoNothing` and
// `NotSet` -- have exactly one successor: the EMPTY sequence, which validate()
// refuses rather than running a solve that reports nothing.
TEST(IpmPhases, TheEmptySequenceIsRefusedByValidate) {
    hven::solvers::IpmOptions o;
    o.phases.clear();
    EXPECT_THROW(hven::solvers::validate(o), std::invalid_argument);
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    EXPECT_THROW(solver.set_options(o), std::invalid_argument);
}

// (3) THE WORKER PREPARATION'S OWN OBSERVABLE (M6 W5 T8.9, in place of the jet
// lifecycle's transcription count). The retired jet_run() was
// initialize-mode-release, and what separated it from a bare mode call was the
// PARTITION COUNT seen DURING the evaluations. That count now belongs to the
// program, is fixed before the solve begins, and is therefore visible from
// inside an evaluation exactly as before -- with the difference that the
// caller, not a lifecycle, decides it.
TEST(IpmSolverEntry, TheProgramsPartitionCountIsWhatEvaluationsSee) {
    auto problem = std::make_shared<TranscriptionCountingProblem>();
    const auto program = hven::solvers::make_nlp_program(problem, 7);
    problem->watch_ = program.get();
    hven::solvers::InteriorPointSolver solver;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }

    const hven::solvers::IpmResult result = solver.solve(*program, Eigen::VectorXd::Zero(2));
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kOptimal);

    // ONE TRANSCRIPTION, and it happened before the solve: the caller made it.
    EXPECT_EQ(problem->n_bounds_, 1);
    EXPECT_EQ(problem->n_jac_structure_, 1);
    EXPECT_EQ(problem->n_hess_structure_, 1);

    // EVERY EVALUATION SAW THE ADOPTED COUNT, which on a problem this small is
    // the CLAMPED 1 rather than the 7 requested -- the clamp is the reason the
    // request is never the answer.
    ASSERT_EQ(program->num_partitions_, 1);
    ASSERT_FALSE(problem->partitions_during_eval_.empty());
    for (int p : problem->partitions_during_eval_) {
        EXPECT_EQ(p, program->num_partitions_);
    }
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
struct IpmPhaseRecordingSink : hven::solvers::TraceSink {
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

/// Runs one PHASE SEQUENCE on a fresh EqOnlyProblem solver with a
/// phase-recording sink attached, optionally capped at @p max_iters so the OPT
/// phase cannot report CONVERGED, and returns the sink.
IpmPhaseRecordingSink run_with_phase_sink(const std::vector<hven::solvers::IpmPhase> &phases,
                                          int max_iters, hven::solvers::SolveStatus *flag_out) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<EqOnlyProblem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.phases = phases;
        solver.set_options(std::move(o));
    }
    if (max_iters > 0) {
        auto o = solver.options();
        o.max_iters = max_iters;
        solver.set_options(std::move(o));
    }
    IpmPhaseRecordingSink sink;
    solver.attach_trace(&sink);
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    *flag_out = solver.solve(*program, x0).status;
    // The sink outlives every solve made while it is attached, which is the
    // documented contract -- but it does not outlive this function, so detach it
    // rather than leave the solver holding a pointer to a dead object.
    solver.attach_trace(nullptr);
    return sink;
}

} // namespace

TEST(IpmPhaseSemantics, SolveOptimizeSolveSkipsTheTrailingSoeOnlyWhenOptConverged) {
    hven::solvers::SolveStatus flag = hven::solvers::SolveStatus::kMaxIter;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink({IpmPhase::kSolve, IpmPhase::kOptimize, IpmPhase::kSolve}, 0, &flag);
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(converged.begins_, 1);
    EXPECT_EQ(converged.ends_, 1);
    EXPECT_EQ(converged.begin_phases_, 3); // three REQUESTED, one of them skipped
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0, 1}));

    const IpmPhaseRecordingSink capped =
        run_with_phase_sink({IpmPhase::kSolve, IpmPhase::kOptimize, IpmPhase::kSolve}, 1, &flag);
    ASSERT_NE(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(capped.begin_phases_, 3);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1, 2}));
}

TEST(IpmPhaseSemantics, OptimizeSolveSkipsTheTrailingSoeOnlyWhenOptConverged) {
    hven::solvers::SolveStatus flag = hven::solvers::SolveStatus::kMaxIter;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink({IpmPhase::kOptimize, IpmPhase::kSolve}, 0, &flag);
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(converged.begin_phases_, 2);
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0}));

    const IpmPhaseRecordingSink capped =
        run_with_phase_sink({IpmPhase::kOptimize, IpmPhase::kSolve}, 1, &flag);
    ASSERT_NE(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(capped.begin_phases_, 2);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1}));
}

// solve_optimize has no conditional step at all: both phases run whatever the
// OPT phase reported, which is what separates it from the two above.
TEST(IpmPhaseSemantics, SolveOptimizeAlwaysRunsBothPhases) {
    hven::solvers::SolveStatus flag = hven::solvers::SolveStatus::kMaxIter;
    const IpmPhaseRecordingSink converged =
        run_with_phase_sink({IpmPhase::kSolve, IpmPhase::kOptimize}, 0, &flag);
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(converged.begin_phases_, 2);
    EXPECT_EQ(converged.distinct_phases(), (std::vector<int>{0, 1}));

    const IpmPhaseRecordingSink capped =
        run_with_phase_sink({IpmPhase::kSolve, IpmPhase::kOptimize}, 1, &flag);
    ASSERT_NE(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(capped.begin_phases_, 2);
    EXPECT_EQ(capped.distinct_phases(), (std::vector<int>{0, 1}));
}

// The two single-phase modes, for the contrast: one requested phase, one
// executed, and nothing conditional to skip.
TEST(IpmPhaseSemantics, SolveAndOptimizeEachRunExactlyOnePhase) {
    hven::solvers::SolveStatus flag = hven::solvers::SolveStatus::kMaxIter;
    for (const hven::solvers::IpmPhase phase :
         {hven::solvers::IpmPhase::kSolve, hven::solvers::IpmPhase::kOptimize}) {
        const IpmPhaseRecordingSink sink = run_with_phase_sink({phase}, 0, &flag);
        ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
        EXPECT_EQ(sink.begins_, 1);
        EXPECT_EQ(sink.ends_, 1);
        EXPECT_EQ(sink.begin_phases_, 1);
        EXPECT_EQ(sink.distinct_phases(), (std::vector<int>{0}));
    }
}

// The LIVE iteration-cap pin on last_stop_reason() (M6 W5 T8.2). HS071 takes
// nine iterations, so one is a cap exhaustion and nothing else; the two labelled
// doors are pinned in test_ipm_stop_reason.cpp.
TEST(IpmSolverEntry, TheIterationCapIsTheRecordedStopReason) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.max_iters = 1;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    result = solver.solve(*program, x0);
    const hven::solvers::SolveStatus flag = result.status;
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_EQ(solver.last_stop_reason(), hven::solvers::IpmStopReason::kIterationCap);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.last_stop_reason()),
              hven::solvers::SolveStatus::kMaxIter);
}

// A converged solve takes no labelled door, and the reason is reset per call:
// the capped run above does not leave its label behind on the next one.
TEST(IpmSolverEntry, AConvergedSolveRecordsNoStopReason) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        solver.set_options(std::move(o));
    }
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    {
        auto o = solver.options();
        o.max_iters = 1;
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, x0);
    ASSERT_EQ(result.status, hven::solvers::SolveStatus::kMaxIter);
    ASSERT_EQ(solver.last_stop_reason(), hven::solvers::IpmStopReason::kIterationCap);

    {
        auto o = solver.options();
        o.max_iters = 200;
        solver.set_options(std::move(o));
    }
    result = solver.solve(*program, x0);
    const hven::solvers::SolveStatus flag = result.status;
    ASSERT_EQ(flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(solver.last_stop_reason(), hven::solvers::IpmStopReason::kNone);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.last_stop_reason()),
              hven::solvers::SolveStatus::kOptimal);
}

// The MULTI-PHASE cap pin (M6 W5 T8.2 fix round 1). solve_optimize runs the
// feasibility phase and then the optimality phase, both unconditionally. At
// max_iters = 10 the feasibility phase converges inside its budget and the
// optimality phase runs out of iterations: the label belongs to the phase that
// hit the cap, because it is reset per phase and stored without consulting a
// verdict the EARLIER phase wrote. The per-phase terminal loop indices are read
// through the late callback (IterateInfo::iter_ restarts at 0 in each phase),
// which is what makes "phase 1 ended early, phase 2 ended on its cap" an
// assertion rather than a story.
//
// RECORDED, not asserted as correct: the call reports NOTCONVERGED, because the
// capped phase leaves through the terminal conjunction, which assigns the
// verdict on its way out. The one path on which the REPORTED verdict would be
// the earlier phase's stale CONVERGED is a later phase leaving by EXHAUSTION
// after one of the loop's five `continue`s -- all five are restoration
// transitions, and every fixture that enters restoration in an optimality phase
// does so because the problem is infeasible, which is exactly what stops the
// feasibility phase converging first. No live case was built for it inside the
// fix round's search budget; the verdict's per-CALL lifetime is registered for
// T8.4's per-phase results.
TEST(IpmSolverEntry, AMultiPhaseCapIsLabelledByThePhaseThatHitIt) {
    constexpr int kCap = 10;
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.max_iters = kCap;
        o.phases = {IpmPhase::kSolve, IpmPhase::kOptimize}; // what solve_optimize() ran
        solver.set_options(std::move(o));
    }
    std::vector<int> phase_terminal;
    // M6 W5 T8.6: the shared iteration callback. The per-phase iterate index is
    // the same number the late callback's IterateInfo carried, and the first
    // and last events of each phase are the same events -- one per `ipm.iter`
    // row, in order.
    solver.set_iteration_callback([&phase_terminal](const hven::solvers::IterationEvent &ev) {
        const int idx = static_cast<int>(ev.iteration);
        if (idx == 0)
            phase_terminal.push_back(idx);
        else if (!phase_terminal.empty())
            phase_terminal.back() = idx;
        return hven::solvers::CallbackAction::kContinue;
    });
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    result = solver.solve(*program, x0);
    const hven::solvers::SolveStatus flag = result.status;

    ASSERT_EQ(phase_terminal.size(), 2u);
    EXPECT_LT(phase_terminal[0], kCap - 1) << "the feasibility phase was expected to end on its "
                                              "own verdict, not on the cap";
    EXPECT_EQ(phase_terminal[1], kCap - 1);
    EXPECT_EQ(solver.last_stop_reason(), hven::solvers::IpmStopReason::kIterationCap);
    EXPECT_EQ(flag, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(flag, solver.last_stop_reason()),
              hven::solvers::SolveStatus::kMaxIter);

    // THE PER-PHASE ACCOUNT, ADDED BESIDE THE ABOVE RATHER THAN REPLACING IT
    // (M6 W5 T8.4). The pin above was written when the verdict's lifetime was
    // the CALL and the reason's was the PHASE, and design §2.3 registered that
    // mismatch as a defect for this task. It did NOT record a stale verdict:
    // this solve leaves through the terminal CONJUNCTION, which assigns the
    // verdict on the way out, so the reported kMaxIter was always phase 1's own
    // answer. Nothing above flips; what the fix adds is that the same is now
    // true BY CONSTRUCTION rather than by which door this fixture happens to
    // take, and that phase 0's own verdict survives beside it.
    const hven::solvers::IpmResult &r = result;
    ASSERT_EQ(r.phases.size(), 2u);
    EXPECT_EQ(r.phases[0].phase, hven::solvers::IpmPhase::kSolve);
    EXPECT_EQ(r.phases[1].phase, hven::solvers::IpmPhase::kOptimize);
    EXPECT_TRUE(r.phases[0].ran);
    EXPECT_TRUE(r.phases[1].ran);
    // Phase 1 hit the cap and says so; phase 0 did not and says that.
    EXPECT_EQ(r.phases[1].status, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_EQ(r.phases[1].stop_reason, hven::solvers::IpmStopReason::kIterationCap);
    EXPECT_EQ(r.phases[0].stop_reason, hven::solvers::IpmStopReason::kNone);
    // The call reports the LAST RAN phase, and the solve-level stop reason is
    // that phase's -- the same value, read two ways.
    EXPECT_EQ(r.status, r.phases[1].status);
    EXPECT_EQ(solver.last_stop_reason(), r.phases[1].stop_reason);
    // And the per-phase iteration counts sum to the call's.
    EXPECT_EQ(r.iterations, r.phases[0].iterations + r.phases[1].iterations);
}

// The CONVERGE-ON-THE-CAP pins (M6 W5 T8.2 fix round 2). The cap store in the
// terminal conjunction is guarded on alg_impl's own phase-LOCAL exit code, so
// the label can never contradict the verdict it is reported beside: a phase
// whose last iteration is BOTH the cap iteration and a converged one reads
// kNone, and only a phase that ran out of iterations with nothing better to say
// reads kIterationCap. The two pins below are the same solve one iteration
// apart, so together they pin the boundary rather than a single point on it.
//
// The converging iteration index is DERIVED, not hard-coded: an uncapped pilot
// reports its terminal loop index through the late callback (IterateInfo::iter_
// is alg_impl's own loop variable; result().iterations is the history size, which
// the restoration transitions pop, and is not the loop count). On this box the
// pilot reports 9; the tests assert the relationship, never the number.
namespace hs071_cap_boundary {
namespace {

// The uncapped run's terminal loop index -- the index of the iteration on which
// HS071 is found converged. Installing the callback moves no trajectory: its
// return value is discarded and it is handed read-only views.
int converging_loop_index() {
    const auto pilot_program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver pilot;
    hven::solvers::IpmResult pilot_result;
    {
        auto o = pilot.options();
        o.common.print_level = 10;
        o.max_iters = 200;
        pilot.set_options(std::move(o));
    }
    int last = -1;
    pilot.set_iteration_callback([&last](const hven::solvers::IterationEvent &ev) {
        last = static_cast<int>(ev.iteration);
        return hven::solvers::CallbackAction::kContinue;
    });
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    pilot_result = pilot.solve(*pilot_program, x0);
    EXPECT_EQ(pilot_result.status, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(pilot.last_stop_reason(), hven::solvers::IpmStopReason::kNone);
    return last;
}

// One capped run, reporting the verdict, the label and the terminal loop index.
struct CappedRun {
    hven::solvers::SolveStatus flag = hven::solvers::SolveStatus::kMaxIter;
    hven::solvers::IpmStopReason reason = hven::solvers::IpmStopReason::kNone;
    int terminal_index = -1;
};

CappedRun run_capped(int cap) {
    const auto program = hven::solvers::make_nlp_program(std::make_shared<Hs071Problem>());
    hven::solvers::InteriorPointSolver solver;
    hven::solvers::IpmResult result;
    {
        auto o = solver.options();
        o.common.print_level = 10;
        o.max_iters = cap;
        solver.set_options(std::move(o));
    }
    CappedRun out;
    solver.set_iteration_callback([&out](const hven::solvers::IterationEvent &ev) {
        out.terminal_index = static_cast<int>(ev.iteration);
        return hven::solvers::CallbackAction::kContinue;
    });
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;
    result = solver.solve(*program, x0);
    out.flag = result.status;
    out.reason = solver.last_stop_reason();
    return out;
}

} // namespace
} // namespace hs071_cap_boundary

TEST(IpmSolverEntry, AConvergenceOnTheCapIterationRecordsNoStopReason) {
    const int idx = hs071_cap_boundary::converging_loop_index();
    ASSERT_GT(idx, 0);

    // max_iters = idx + 1 makes the converging iteration the CAP iteration: both
    // disjuncts of "this is the last iteration" hold on it. The label must be
    // kNone -- the convergence stopped the phase, not the budget.
    const hs071_cap_boundary::CappedRun run = hs071_cap_boundary::run_capped(idx + 1);
    EXPECT_EQ(run.terminal_index, idx) << "the capped run was expected to end on the same "
                                          "iteration the uncapped pilot converged on";
    EXPECT_EQ(run.terminal_index, (idx + 1) - 1) << "and that iteration is the cap iteration";
    EXPECT_EQ(run.flag, hven::solvers::SolveStatus::kOptimal);
    EXPECT_EQ(run.reason, hven::solvers::IpmStopReason::kNone);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(run.flag, run.reason),
              hven::solvers::SolveStatus::kOptimal);
}

TEST(IpmSolverEntry, OneIterationShortOfConvergenceRecordsTheCap) {
    const int idx = hs071_cap_boundary::converging_loop_index();
    ASSERT_GT(idx, 1);

    // One iteration short: the same solve, stopped on the iteration BEFORE the
    // converging one. Nothing better to say, so the cap is the whole answer.
    const hs071_cap_boundary::CappedRun run = hs071_cap_boundary::run_capped(idx);
    EXPECT_EQ(run.terminal_index, idx - 1) << "the run was expected to end on its cap iteration";
    EXPECT_EQ(run.flag, hven::solvers::SolveStatus::kMaxIter);
    EXPECT_EQ(run.reason, hven::solvers::IpmStopReason::kIterationCap);
    EXPECT_EQ(hven::solvers::resolve_ipm_phase_status(run.flag, run.reason),
              hven::solvers::SolveStatus::kMaxIter);
}
