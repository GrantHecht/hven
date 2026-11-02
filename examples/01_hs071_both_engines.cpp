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
// The problem is stated ONCE, as an Nlp (the Ipopt TNLP shape: callbacks plus
// sparsity triplets), wrapped ONCE in a Problem, and handed to both engines.

#include <cmath>
#include <cstdio>
#include <memory>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::VecRef;
constexpr double kOptimum = 17.0140173;

struct Hs071 : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0; // row 0: x0*x1*x2*x3 >= 25 ; row 1: sum x^2 == 40
        gu << kInfinity, 40.0;
    }
    double eval_f(ConstVecRef x) const override {
        return x[0] * x[3] * (x[0] + x[1] + x[2]) + x[2];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << x[3] * (2 * x[0] + x[1] + x[2]), x[0] * x[3], x[0] * x[3] + 1,
            x[0] * (x[0] + x[1] + x[2]);
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << x[0] * x[1] * x[2] * x[3], x.squaredNorm();
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0, 0, 1, 1, 1, 1;
        c << 0, 1, 2, 3, 0, 1, 2, 3;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v << x[1] * x[2] * x[3], x[0] * x[2] * x[3], x[0] * x[1] * x[3], x[0] * x[1] * x[2],
            2 * x[0], 2 * x[1], 2 * x[2], 2 * x[3];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3; // lower triangle
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef v) const override {
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
    std::printf("%-12s status=%-9s iterations=%3lld  f=%.7f  x=(%.4f, %.4f, %.4f, %.4f)  "
                "lambda=(%.4f, %.4f)  %s\n",
                engine, to_string(r.status), static_cast<long long>(r.iterations), r.f, r.x[0],
                r.x[1], r.x[2], r.x[3], r.lambda[0], r.lambda[1], ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    Problem problem(std::make_shared<Hs071>());
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    // Both engines are silent by default. One line turns the console on:
    //     IpmOptions options;
    //     options.common.verbosity = Verbosity::kIterations;
    //     IpmSolver ipm(options);
    IpmSolver ipm;
    const IpmResult ipm_result = ipm.solve(problem, x0);

    SqpSolver sqp;
    const SqpResult sqp_result = sqp.solve(problem, x0);

    const bool ok_ipm = report("IpmSolver", ipm_result);
    const bool ok_sqp = report("SqpSolver", sqp_result);
    return (ok_ipm && ok_sqp) ? 0 : 1;
}
