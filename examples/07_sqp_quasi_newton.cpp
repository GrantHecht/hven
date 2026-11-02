// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 7: the SQP engine in quasi-Newton mode (SqpOptions::hessian_mode).
//
//     min  sum_i (x_i - i)^2,  i = 1..4      s.t.  x0^2 + x1^2 + x2^2 + x3^2 <= 4
//
// The nearest point of the ball of radius 2 to p = (1, 2, 3, 4) is 2*p/|p|:
//     f* = (|p| - 2)^2 = (sqrt(30) - 2)^2,   row multiplier = |p|/2 - 1.
//
// With hessian_mode = kLbfgsDamped the engine builds its own damped L-BFGS
// curvature and never asks for the Lagrangian Hessian
// (counters.eval_hess_calls stays 0), so the problem needs first derivatives
// only: BallFirstOrder declares has_hessian() == false and no Hessian
// structure. The exact-Hessian solve is run next to it, on a problem that does
// provide the Hessian, for comparison.

#include <cmath>
#include <cstdio>
#include <memory>

#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::Vec;
using hven::VecRef;

// First derivatives only.
struct BallFirstOrder : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 1, .jacobian_nonzeros = 4, .hessian_nonzeros = 0};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << 4.0; // |x|^2 <= 4
    }
    double eval_f(ConstVecRef x) const override { return (x - target()).squaredNorm(); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g = 2.0 * (x - target()); }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0, 0;
        c << 0, 1, 2, 3;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    bool has_hessian() const override { return false; }

    static Vec target() { return Vec{{1.0, 2.0, 3.0, 4.0}}; }
};

// The same problem with its Lagrangian Hessian (diagonal).
struct Ball : BallFirstOrder {
    NlpDimensions dimensions() const override {
        NlpDimensions d = BallFirstOrder::dimensions();
        d.hessian_nonzeros = 4;
        return d;
    }
    bool has_hessian() const override { return true; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 2, 3;
        c << 0, 1, 2, 3;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
};

bool report(const char *label, const SqpResult &r) {
    const double f_star = std::pow(std::sqrt(30.0) - 2.0, 2);
    const double lambda_star = 0.5 * std::sqrt(30.0) - 1.0;
    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.f - f_star) < 1e-6 &&
                    std::abs(r.lambda[0] - lambda_star) < 1e-4;
    std::printf(
        "%-12s status=%-8s iterations=%2lld  f=%.8f (exact %.8f)  eval_hess calls=%lld  %s\n",
        label, to_string(r.status), static_cast<long long>(r.iterations), r.f, f_star,
        static_cast<long long>(r.counters.eval_hess_calls), ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    const Vec x0 = Vec::Zero(4);

    Problem with_hessian(std::make_shared<Ball>());
    SqpOptions exact;
    SqpSolver exact_solver(exact);
    const SqpResult r_exact = exact_solver.solve(with_hessian, x0);

    Problem first_order(std::make_shared<BallFirstOrder>());
    SqpOptions qn;
    qn.hessian_mode = SqpHessianMode::kLbfgsDamped;
    qn.qn_memory = 10; // L-BFGS window (default 10; shown for the knob)
    SqpSolver qn_solver(qn);
    const SqpResult r_qn = qn_solver.solve(first_order, x0);

    const bool ok_exact = report("exact", r_exact);
    const bool ok_qn = report("L-BFGS", r_qn) && r_qn.counters.eval_hess_calls == 0;
    return (ok_exact && ok_qn) ? 0 : 1;
}
