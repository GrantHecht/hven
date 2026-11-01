// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 2: a bound-constrained problem (no constraint rows at all).
//
//     min  100*(y - x^2)^2 + (1 - x)^2      s.t.  -2 <= x <= 0.5,   y free
//
// Unconstrained, Rosenbrock's minimum is (1, 1). The upper bound x <= 0.5 cuts
// it off, so the solution sits ON the bound: x* = 0.5, y* = 0.25, f* = 0.25,
// with a bound multiplier on x (z <= 0 at an active upper bound). An NlpTripletModel with
// num_cons() == 0 is legal; "no bound" on y is written as +/-infinity.

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
constexpr double kInf = std::numeric_limits<double>::infinity();

struct BoundedRosenbrock : NlpTripletModel {
    int num_vars() const override { return 2; }
    int num_cons() const override { return 0; }
    int num_jac_nonzeros() const override { return 0; }
    int num_hess_nonzeros() const override { return 3; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {
        xl << -2.0, -kInf;
        xu << 0.5, kInf;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = 100.0 * std::pow(x[1] - x[0] * x[0], 2) + std::pow(1.0 - x[0], 2);
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << -400.0 * x[0] * (x[1] - x[0] * x[0]) - 2.0 * (1.0 - x[0]),
            200.0 * (x[1] - x[0] * x[0]);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>) const override {}
    void jac_structure(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>) const override {}
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd>,
                  Eigen::Ref<Eigen::VectorXd>) const override {}
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1; // lower triangle: (0,0), (1,0), (1,1)
        c << 0, 0, 1;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd>,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << obj_factor * (1200.0 * x[0] * x[0] - 400.0 * x[1] + 2.0), obj_factor * -400.0 * x[0],
            obj_factor * 200.0;
    }
};

bool check(const char *engine, const SolveResult &r) {
    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.x[0] - 0.5) < 1e-5 &&
                    std::abs(r.x[1] - 0.25) < 1e-5 && std::abs(r.f - 0.25) < 1e-6 && r.z[0] < 0.0;
    std::printf("%-10s status=%-8s iterations=%3lld  x=(%.6f, %.6f)  f=%.8f  z_x=%+.4f  %s\n",
                engine, to_string(r.status), static_cast<long long>(r.iterations), r.x[0], r.x[1],
                r.f, r.z[0], ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    const auto problem = std::make_shared<BoundedRosenbrock>();
    Eigen::VectorXd x0(2);
    x0 << -1.2, 1.0; // the classic starting point

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
