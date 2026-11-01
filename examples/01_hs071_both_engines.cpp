// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 1: Hock-Schittkowski 071 on both engines.
//
//     min   x0*x3*(x0 + x1 + x2) + x2
//     s.t.  x0*x1*x2*x3            >= 25
//           x0^2 + x1^2 + x2^2 + x3^2 = 40
//           1 <= x <= 5
//
// The known optimum is f* = 17.0140173 at x* = (1, 4.743, 3.821, 1.379).
//
// The problem is stated ONCE, as an NlpTripletModel (the Ipopt TNLP shape:
// callbacks plus sparsity triplets), and handed to both engines:
//   * the interior-point engine consumes the NonLinearProgram that
//     make_nlp_program() transcribes;
//   * the SQP engine consumes the NlpModel that NlpProblemModel converts.

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
constexpr double kOptimum = 17.0140173;

struct Hs071 : NlpTripletModel {
    int num_vars() const override { return 4; }
    int num_cons() const override { return 2; }
    int num_jac_nonzeros() const override { return 8; }
    int num_hess_nonzeros() const override { return 10; }

    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0; // row 0: x0*x1*x2*x3 >= 25 ; row 1: sum x^2 == 40
        gu << kInf, 40.0;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << x[3] * (2 * x[0] + x[1] + x[2]), x[0] * x[3], x[0] * x[3] + 1,
            x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g << x[0] * x[1] * x[2] * x[3], x.squaredNorm();
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3; // lower triangle
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v << x[1] * x[2] * x[3], x[0] * x[2] * x[3], x[0] * x[1] * x[3], x[0] * x[1] * x[2],
            2 * x[0], 2 * x[1], 2 * x[2], 2 * x[3];
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd> x, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
        v << obj_factor * 2 * x[3] + lambda[1] * 2,                          // (0,0)
            obj_factor * x[3] + lambda[0] * x[2] * x[3],                     // (1,0)
            lambda[1] * 2,                                                   // (1,1)
            obj_factor * x[3] + lambda[0] * x[1] * x[3],                     // (2,0)
            lambda[0] * x[0] * x[3],                                         // (2,1)
            lambda[1] * 2,                                                   // (2,2)
            obj_factor * (2 * x[0] + x[1] + x[2]) + lambda[0] * x[1] * x[2], // (3,0)
            obj_factor * x[0] + lambda[0] * x[0] * x[2],                     // (3,1)
            obj_factor * x[0] + lambda[0] * x[0] * x[1],                     // (3,2)
            lambda[1] * 2;                                                   // (3,3)
    }
};

bool report(const char *engine, const SolveResult &r) {
    const bool ok = (r.status == SolveStatus::kOptimal) && std::abs(r.f - kOptimum) < 1e-5;
    std::printf("%-12s status=%-9s iterations=%3lld  f=%.7f  x=(%.4f, %.4f, %.4f, %.4f)  %s\n",
                engine, to_string(r.status), static_cast<long long>(r.iterations), r.f, r.x[0],
                r.x[1], r.x[2], r.x[3], ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    const auto problem = std::make_shared<Hs071>();
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    // Interior-point engine: transcribe, then solve the program.
    const auto program = make_nlp_program(problem);
    IpmOptions ipm_options;
    ipm_options.common.print_level = 3; // the IPM default (0) prints a banner and a table
    IpmSolver ipm(ipm_options);
    const IpmResult ipm_result = ipm.solve(*program, x0);

    // SQP engine: convert to the driver-facing model, then solve it.
    NlpProblemModel model(problem);
    SqpSolver sqp{SqpOptions{}};
    const SqpResult sqp_result = sqp.solve(model, x0);

    const bool ok_ipm = report("IpmSolver", ipm_result);
    const bool ok_sqp = report("SqpSolver", sqp_result);
    return (ok_ipm && ok_sqp) ? 0 : 1;
}
