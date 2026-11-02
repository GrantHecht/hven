// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 2: a bound-constrained problem (no constraint rows at all).
//
//     min  100*(y - x^2)^2 + (1 - x)^2      s.t.  -2 <= x <= 0.5,   y free
//
// Unconstrained, Rosenbrock's minimum is (1, 1). The upper bound x <= 0.5 cuts
// it off, so the solution sits ON the bound: x* = 0.5, y* = 0.25, f* = 0.25,
// with a bound multiplier on x (z <= 0 at an active upper bound). An Nlp with m == 0
// needs no constraint callbacks at all; "no bound" on y is written as
// +/-kInfinity.

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

struct BoundedRosenbrock : Nlp {
    NlpDimensions dimensions() const override { return {.n = 2, .m = 0, .hessian_nonzeros = 3}; }
    void bounds(VecRef xl, VecRef xu, VecRef, VecRef) const override {
        xl << -2.0, -kInfinity;
        xu << 0.5, kInfinity;
    }
    double eval_f(ConstVecRef x) const override {
        return 100.0 * std::pow(x[1] - x[0] * x[0], 2) + std::pow(1.0 - x[0], 2);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << -400.0 * x[0] * (x[1] - x[0] * x[0]) - 2.0 * (1.0 - x[0]),
            200.0 * (x[1] - x[0] * x[0]);
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1; // lower triangle: (0,0), (1,0), (1,1)
        c << 0, 0, 1;
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef, VecRef v) const override {
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
    Problem problem(std::make_shared<BoundedRosenbrock>());
    Eigen::VectorXd x0(2);
    x0 << -1.2, 1.0; // the classic starting point

    IpmSolver ipm;
    const bool ok_ipm = check("IpmSolver", ipm.solve(problem, x0));

    SqpSolver sqp;
    const bool ok_sqp = check("SqpSolver", sqp.solve(problem, x0));

    return (ok_ipm && ok_sqp) ? 0 : 1;
}
