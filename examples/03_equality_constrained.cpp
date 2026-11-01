// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 3: an equality-constrained problem, and reading its multiplier.
//
//     min  (x0-1)^2 + (x1-2)^2 + (x2-3)^2      s.t.  x0^2 + x1^2 + x2^2 = 4
//
// The nearest point of the sphere of radius 2 to p = (1, 2, 3) is 2*p/|p|, so
//     f*      = (|p| - 2)^2,
//     lambda* = |p|/2 - 1      (hven's Lagrangian is f + lambda*g, g = |x|^2 - 4).
// Both are checked below; the sign of the multiplier is the point of the check.
//
// An equality row is a row whose lower and upper bounds coincide.

#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/make_nlp_program.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/nlp_triplet_model.h"

namespace {

using namespace hven::solvers;

struct Sphere : NlpTripletModel {
    int num_vars() const override { return 3; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 3; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        // Free variables are +/-infinity, NOT a large finite number such as 1e20.
        xl.setConstant(-std::numeric_limits<double>::infinity());
        xu.setConstant(std::numeric_limits<double>::infinity());
        gl << 4.0;
        gu << 4.0; // gl == gu: an equality
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = (x - Eigen::Vector3d(1, 2, 3)).squaredNorm();
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g = 2.0 * (x - Eigen::Vector3d(1, 2, 3));
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g << x.squaredNorm();
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v = 2.0 * x;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 2; // diagonal only
        c << 0, 1, 2;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
};

bool check(const char *engine, const SolveResult &r) {
    const double p_norm = std::sqrt(14.0);
    const double f_star = (p_norm - 2.0) * (p_norm - 2.0);
    const double lambda_star = 0.5 * p_norm - 1.0;
    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.f - f_star) < 1e-6 &&
                    std::abs(r.lambda_e[0] - lambda_star) < 1e-5 && r.feasibility_e < 1e-6;
    std::printf("%-10s status=%-8s iterations=%3lld  f=%.8f (exact %.8f)  lambda=%.6f "
                "(exact %.6f)  |ce|=%.1e  %s\n",
                engine, to_string(r.status), static_cast<long long>(r.iterations), r.f, f_star,
                r.lambda_e[0], lambda_star, r.feasibility_e, ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    const auto problem = std::make_shared<Sphere>();
    Eigen::VectorXd x0(3);
    x0 << 1.0, 1.0, 1.0;

    IpmOptions ipm_options;
    ipm_options.common.print_level = 3;
    IpmSolver ipm(ipm_options);
    const auto program = make_nlp_program(problem);
    const bool ok_ipm = check("IpmSolver", ipm.solve(*program, x0));

    NlpProblemModel model(problem);
    SqpSolver sqp{SqpOptions{}};
    const bool ok_sqp = check("SqpSolver", sqp.solve(model, x0));

    return (ok_ipm && ok_sqp) ? 0 : 1;
}
