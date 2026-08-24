// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The objective scale is internal to the solve, and these are the two
// boundaries that keep it there.
//
// The solver minimizes obj_scale * f, so its Lagrangian is
// L = s*f + lambda_e^T cE + lambda_i^T cI - z and every multiplier it carries
// is the scaled problem's -- as is every objective value it evaluates. The
// caller's problem is the unscaled one, and nlp_model.h's stationarity
// convention is stated at s = 1, so the scale is divided out of what the solve
// REPORTS and multiplied into what a caller SEEDS. The tests below hold both
// directions: nothing a caller reads or writes moves when the scale does.
//
// The minimizer itself is scale-invariant for a positive scale, so the primals
// are the control: they were already the same at every scale and stay so.

#include <gtest/gtest.h>

#include <limits>
#include <memory>
#include <string>

#include <Eigen/Core>

#include "hven/drivers/interior_point_solver.h"
#include "hven/model/nlp_solver.h"

using hven::ConstEigenRef;
using hven::solvers::NLPProblem;
using hven::solvers::NLPSolver;

// min 0.5*|x|^2 subject to sum(x) == 3, with a two-sided box on every
// variable. The optimum is x_i = 0.75 with f = 1.125, and the equality
// multiplier is -0.75 under the caller's convention -- all three known by hand,
// so a scale leaking into a report is a difference from a NUMBER rather than a
// difference between two runs.
struct ObjScaleBoxedProblem : NLPProblem {
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
    /// The seed the installation test relies on: a multiplier on the CALLER's
    /// convention, handed over through the problem's own seed hook (which is
    /// what NLPSolver consults -- a seed staged directly on the optimizer is
    /// discarded when this returns false).
    bool starting_multipliers(Eigen::Ref<Eigen::VectorXd> lambda) const override {
        lambda.setConstant(kSeed);
        return true;
    }

    static constexpr double kSeed = -0.75;

    std::string name() const override { return "ObjScaleBoxed"; }
};

// A variable pinned below by its own bound, so the bound multiplier at the
// solution is a real active one (1.0 on the caller's scale) rather than a
// vanishing residual.
struct ObjScaleActiveBoundProblem : NLPProblem {
    static constexpr int kN = 2;
    static constexpr double kInf = std::numeric_limits<double>::infinity();

    int num_vars() const override { return kN; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return kN; }
    int num_hess_nonzeros() const override { return kN; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl << 1.0, -kInf;
        xu << kInf, kInf;
        gl << 0.5;
        gu << kInf;
    }
    void eval_f(ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 0.5 * x.squaredNorm();
    }
    void eval_grad_f(ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = x;
    }
    void eval_g(ConstEigenRef<Eigen::VectorXd> x, Eigen::Ref<Eigen::VectorXd> g) const override {
        g[0] = x[0] + x[1];
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
    std::string name() const override { return "ObjScaleActiveBound"; }
};

namespace {

constexpr double kObjScaleTol = 1e-5;

} // namespace

TEST(ObjectiveScaleReporting, TheReportedObjectiveAndMultipliersDoNotMoveWithTheScale) {
    for (double scale : {1.0, 2.0, 10.0, 0.125}) {
        NLPSolver solver(std::make_shared<ObjScaleBoxedProblem>());
        solver.optimizer_->set_print_level(3);
        solver.optimizer_->set_obj_scale(scale);

        const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleBoxedProblem::kN, 0.6);
        ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED) << "scale " << scale;

        const auto &result = solver.optimizer_->result();
        EXPECT_NEAR(result.obj_val_, 1.125, kObjScaleTol) << "scale " << scale;
        ASSERT_EQ(result.eq_lmults_.size(), 1);
        EXPECT_NEAR(result.eq_lmults_[0], -0.75, kObjScaleTol) << "scale " << scale;

        // The control: the minimizer never depended on the scale, and still
        // does not.
        for (int i = 0; i < ObjScaleBoxedProblem::kN; i++) {
            EXPECT_NEAR(solver.return_x()[i], 0.75, kObjScaleTol) << "scale " << scale;
        }
    }
}

TEST(ObjectiveScaleReporting, AnActiveBoundMultiplierDoesNotMoveWithTheScale) {
    for (double scale : {1.0, 2.0, 10.0}) {
        NLPSolver solver(std::make_shared<ObjScaleActiveBoundProblem>());
        solver.optimizer_->set_print_level(3);
        solver.optimizer_->set_obj_scale(scale);

        const Eigen::VectorXd x0 = Eigen::VectorXd::Constant(ObjScaleActiveBoundProblem::kN, 0.6);
        ASSERT_EQ(solver.optimize(x0), hven::ConvergenceFlags::CONVERGED) << "scale " << scale;

        const auto &result = solver.optimizer_->result();
        ASSERT_EQ(result.bound_lmults_.size(), ObjScaleActiveBoundProblem::kN);
        // x0 sits on its lower bound, and the bound multiplier that holds it
        // there balances the objective gradient: 1.0 on the caller's scale.
        EXPECT_NEAR(result.bound_lmults_[0], 1.0, kObjScaleTol) << "scale " << scale;
        EXPECT_NEAR(solver.return_x()[0], 1.0, kObjScaleTol) << "scale " << scale;
    }
}

namespace {

/// Runs one seeded solve and reports the equality multiplier as it stood in
/// the iterate at the first callback -- the last moment before any algorithmic
/// step has moved it, and therefore the installed seed itself.
double obj_scale_installed_seed(double scale) {
    NLPSolver solver(std::make_shared<ObjScaleBoxedProblem>());
    solver.optimizer_->set_print_level(3);
    solver.optimizer_->set_obj_scale(scale);

    double installed = 0.0;
    bool seen = false;
    const int primal_vars = ObjScaleBoxedProblem::kN;
    solver.optimizer_->set_early_callback(
        [&](int iteration, double, hven::EigenRef<Eigen::VectorXd> xsl, double,
            hven::EigenRef<Eigen::VectorXd>, hven::EigenRef<Eigen::VectorXd>,
            Eigen::SparseMatrix<double, Eigen::RowMajor> &) {
            if (iteration == 0 && !seen) {
                // No slack variables on this problem, so the equality
                // multiplier block follows the primals directly.
                installed = xsl[primal_vars];
                seen = true;
            }
            return 0;
        });

    EXPECT_EQ(solver.optimize(Eigen::VectorXd::Constant(primal_vars, 0.6)),
              hven::ConvergenceFlags::CONVERGED)
        << "scale " << scale;
    EXPECT_TRUE(seen) << "the early callback never ran, so nothing was observed";
    return installed;
}

} // namespace

// The other direction of the same boundary: a seed arrives on the caller's
// convention and is installed on the solver's, which is the caller's
// multiplied by the scale.
TEST(ObjectiveScaleReporting, ASeededMultiplierIsInstalledOnTheSolversScale) {
    const double unit = obj_scale_installed_seed(1.0);

    // The unit-scale case is the identity, so what lands is the seed itself.
    EXPECT_DOUBLE_EQ(unit, ObjScaleBoxedProblem::kSeed);

    // And every other scale multiplies it, exactly.
    EXPECT_DOUBLE_EQ(obj_scale_installed_seed(4.0), ObjScaleBoxedProblem::kSeed * 4.0);
    EXPECT_DOUBLE_EQ(obj_scale_installed_seed(0.25), ObjScaleBoxedProblem::kSeed * 0.25);
}
