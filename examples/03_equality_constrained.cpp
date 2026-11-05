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
#include <memory>

#include <hven/hven.h>

namespace {

using namespace hven;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::VecRef;

struct Sphere : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 1, .jacobian_nonzeros = 3, .hessian_nonzeros = 3};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        // Free variables are +/-kInfinity. A finite value at or beyond
        // common.infinite_bound (1e20 by default) is read the same way: absent.
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 4.0;
        gu << 4.0; // gl == gu: an equality
    }
    double eval_f(ConstVecRef x) const override {
        return (x - Eigen::Vector3d(1, 2, 3)).squaredNorm();
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = 2.0 * (x - Eigen::Vector3d(1, 2, 3));
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 2; // diagonal only
        c << 0, 1, 2;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
};

bool check(const char *engine, const SolveResult &r) {
    const double p_norm = std::sqrt(14.0);
    const double f_star = (p_norm - 2.0) * (p_norm - 2.0);
    const double lambda_star = 0.5 * p_norm - 1.0;
    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.f - f_star) < 1e-6 &&
                    std::abs(r.lambda[0] - lambda_star) < 1e-5 && r.feasibility < 1e-6;
    std::printf("%-10s status=%-8s iterations=%3lld  f=%.8f (exact %.8f)  lambda=%.6f "
                "(exact %.6f)  g=%.6f  |ce|=%.1e  %s\n",
                engine, to_string(r.status), static_cast<long long>(r.iterations), r.f, f_star,
                r.lambda[0], lambda_star, r.g[0], r.feasibility, ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    Problem problem(std::make_shared<Sphere>());
    Eigen::VectorXd x0(3);
    x0 << 1.0, 1.0, 1.0;

    IpmSolver ipm;
    const bool ok_ipm = check("IpmSolver", ipm.solve(problem, x0));

    SqpSolver sqp;
    const bool ok_sqp = check("SqpSolver", sqp.solve(problem, x0));

    return (ok_ipm && ok_sqp) ? 0 : 1;
}
