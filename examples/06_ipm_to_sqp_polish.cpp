// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 6: the explicit interior-point -> SQP polish composition.
//
// A common pattern: let the interior-point engine find the neighbourhood of a
// solution cheaply (loose tolerances), then hand its answer to the SQP engine,
// whose active-set steps finish the job to a tight tolerance.
//
//   1. IpmSolver solves HS071 to 1e-3 and exports its state as a
//      WarmStartData payload (result.export_warm_start()). The payload carries
//      the primal point, the multipliers and, as an extension, the
//      interior-point's bound duals ("hven.ipm.polish.v1").
//   2. to_sqp_warm_start() turns that payload into the SQP engine's own
//      SqpWarmStart (the crossover: it infers the active set).
//   3. SqpSolver::solve(problem, x0, SqpWarmStart) polishes to 1e-10.
//
// The same payload can also be given to SqpSolver directly,
// solve(problem, x0, payload), which runs the crossover internally; both are
// shown, next to a cold SQP solve to the same tolerance for comparison. Both
// engines solve the one Problem.

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/problem.h"
#include "hven/warmstart/ipm_polish_extension.h"

namespace {

using namespace hven::solvers;
using hven::ConstVecRef;
using hven::IndexVecRef;
using hven::kInfinity;
using hven::VecRef;

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

} // namespace

int main() {
    const auto hs071 = std::make_shared<Hs071>();
    Problem problem(hs071);
    Eigen::VectorXd x0(4);
    x0 << 1.0, 5.0, 5.0, 1.0;

    IpmOptions ipm_options;
    ipm_options.common.optimality_tol = 1e-3;
    ipm_options.common.feasibility_tol = 1e-3;
    IpmSolver ipm(ipm_options);
    const IpmResult rough = ipm.solve(problem, x0);
    const std::optional<WarmStartData> payload = rough.export_warm_start();
    if (rough.status != SolveStatus::kOptimal || !payload) {
        std::printf("the interior-point stage failed\n");
        return 1;
    }

    SqpOptions sqp_options;
    sqp_options.common.optimality_tol = 1e-10;
    sqp_options.common.feasibility_tol = 1e-10;
    SqpSolver sqp(sqp_options);

    // The staged crossover reads the variable bounds and stamps the problem's
    // declaration key, which the payload already carries.
    Eigen::VectorXd xl(4), xu(4), gl(2), gu(2);
    hs071->bounds(xl, xu, gl, gu);
    const SqpWarmStart staged = to_sqp_warm_start(*payload, xl, xu, payload->structure_key_);

    const SqpResult polished = sqp.solve(problem, rough.x, staged);
    const SqpResult direct = sqp.solve(problem, rough.x, *payload);
    const SqpResult cold = sqp.solve(problem, x0);

    const double f_star = 17.01401725; // HS071 optimum
    auto show = [&](const char *label, const SqpResult &r) {
        std::printf(
            "%-18s status=%-8s iterations=%2lld  f=%.10f  stationarity=%.1e  start level=%s\n",
            label, to_string(r.status), static_cast<long long>(r.iterations), r.f, r.stationarity,
            to_string(r.counters.start_level_used));
    };
    std::printf("%-18s status=%-8s iterations=%2lld  f=%.10f (tol 1e-3)\n", "IPM rough",
                to_string(rough.status), static_cast<long long>(rough.iterations), rough.f);
    show("SQP polish staged", polished);
    show("SQP polish payload", direct);
    show("SQP cold", cold);

    const bool ok = polished.status == SolveStatus::kOptimal &&
                    direct.status == SolveStatus::kOptimal &&
                    std::abs(polished.f - f_star) < 1e-6 && std::abs(direct.f - f_star) < 1e-6 &&
                    polished.stationarity < 1e-9 && polished.iterations <= cold.iterations;
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
