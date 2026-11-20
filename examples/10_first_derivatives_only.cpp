// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 10: first derivatives only, the SNOPT-style way (damped L-BFGS).
//
// Same HS071 as example 1, but the model supplies only the objective gradient and the
// constraint Jacobian: has_hessian() is false and no Hessian structure is declared.
// SqpOptions::hessian_mode = kLbfgsDamped makes the SQP engine build its own limited-
// memory curvature, so eval_hessian is never called. Example 7 isolates the mode on a
// toy; this one is the full configuration of docs/guides/snopt-like-configuration.md
// on a problem with bounds, an equality and an inequality, and reads the result the
// way that guide's counts table does.
//
// The interior-point engine needs second derivatives, so it refuses the same problem at
// solve entry; the refusal is shown at the end.

#include <cmath>
#include <cstdio>
#include <memory>
#include <stdexcept>

#include <hven/hven.h>

namespace {

using namespace hven;

constexpr double kOptimum = 17.0140173;

struct Hs071FirstOrder : Nlp {
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 0};
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
    bool has_hessian() const override { return false; } // first derivatives only
};

} // namespace

int main() {
    Problem problem(std::make_shared<Hs071FirstOrder>());
    const Vec x0{{1.0, 5.0, 5.0, 1.0}};

    SqpOptions o;
    o.hessian_mode = SqpHessianMode::kLbfgsDamped; // curvature from gradient differences
    o.lbfgs_memory = 10;                           // the window; the default, shown for the knob
    o.advanced.lbfgs_nonlinear_only = true;        // curvature on the variables the Jacobian
                                                   // pattern says are nonlinear (all, here)
    o.qp_mode = QpMode::kWalk;                     // primal active set: the SNOPT-like QP tier
    o.common.optimality_tol = 1e-6;
    o.common.feasibility_tol = 1e-6;
    o.common.max_iterations = 200;      // counts subproblems solved
    o.common.time_limit_seconds = 60.0; // wall clock; a safety net, not reproducible
    SqpSolver solver(o);                // validates; throws naming the bad field

    const SqpResult r = solver.solve(problem, x0);

    std::printf("status=%s  %s\n", to_string(r.status), r.message.c_str());
    std::printf("majors=%lld  minors=%lld  f evals=%lld  grad evals=%lld  hessian evals=%lld\n",
                static_cast<long long>(r.counters.major_iters),
                static_cast<long long>(r.counters.qp_minor_iters),
                static_cast<long long>(r.work.objective_evals),
                static_cast<long long>(r.work.gradient_evals),
                static_cast<long long>(r.work.hessian_evals));
    std::printf("f=%.7f  x=(%.4f, %.4f, %.4f, %.4f)  lambda=(%.4f, %.4f)\n", r.f, r.x[0], r.x[1],
                r.x[2], r.x[3], r.lambda[0], r.lambda[1]);

    bool ipm_refused = false;
    try {
        IpmSolver{}.solve(problem, x0);
    } catch (const std::invalid_argument &e) {
        ipm_refused = true;
        std::printf("IpmSolver refuses it: %s\n", e.what());
    }

    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.f - kOptimum) < 1e-5 &&
                    r.work.hessian_evals == 0 && ipm_refused;
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
