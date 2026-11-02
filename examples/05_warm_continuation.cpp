// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 5: warm-started continuation along a parameter.
//
//     min  (x0 - 2p)^2 + (x1 - p)^2     s.t.  x0^2 + x1^2 <= 1 + p/2
//
// swept over p = 0.2, 0.4, ..., 1.0. For small p the constraint is inactive
// (f* = 0); once the unconstrained minimiser (2p, p) leaves the disc it is
// active and f* = (|p|*sqrt(5) - sqrt(1 + p/2))^2. The sweep crosses that
// switch, which is where a warm start is tested.
//
// The currency is WarmStartData: every solve hands one back through
// result.export_warm_start(), and the next solve takes it as a third argument,
// on either engine. Cold = each p solved from scratch; warm = each p solved
// from the previous p's payload.
//
// The parameter is a member of the Nlp, changed IN PLACE between solves on ONE
// Problem. Every solve re-reads the bounds (here the row bound 1 + p/2) and
// clears the engines' per-iterate caches, so a value or bound change is legal;
// only the dimensions, the structures and the row kinds are fixed. The warm-start
// payload stays valid across p because the problem's declared structure is
// unchanged.

#include <cmath>
#include <cstdio>
#include <memory>
#include <optional>

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

struct Disc : Nlp {
    double p = 0.0; // the parameter, changed between solves

    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 2};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << 1.0 + 0.5 * p;
    }
    double eval_f(ConstVecRef x) const override {
        return std::pow(x[0] - 2 * p, 2) + std::pow(x[1] - p, 2);
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << 2 * (x[0] - 2 * p), 2 * (x[1] - p);
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x.squaredNorm(); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v = 2.0 * x; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hessian(ConstVecRef, double obj_factor, ConstVecRef lambda, VecRef v) const override {
        v.setConstant(2.0 * obj_factor + 2.0 * lambda[0]);
    }
};

double exact_objective(double p) {
    const double gap = p * std::sqrt(5.0) - std::sqrt(1.0 + 0.5 * p);
    return gap > 0 ? gap * gap : 0.0;
}

// One sweep. `solve_at(p, warm)` returns the engine's result for parameter p.
template <class SolveAt>
bool sweep(const char *label, bool use_warm, SolveAt solve_at, long long &total) {
    std::optional<WarmStartData> warm;
    total = 0;
    bool ok = true;
    std::printf("%-6s %-5s:", label, use_warm ? "warm" : "cold");
    for (double p : {0.2, 0.4, 0.6, 0.8, 1.0}) {
        const SolveResult r = solve_at(p, use_warm ? warm : std::nullopt);
        ok = ok && r.status == SolveStatus::kOptimal && std::abs(r.f - exact_objective(p)) < 1e-6;
        total += r.iterations;
        std::printf(" %lld", static_cast<long long>(r.iterations));
        warm = r.export_warm_start();
    }
    std::printf("   total=%lld  %s\n", total, ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    const Eigen::VectorXd x0 = Eigen::VectorXd::Zero(2);
    bool ok = true;
    long long cold = 0, warm = 0;

    const auto disc = std::make_shared<Disc>();
    Problem problem(disc);

    IpmOptions ipm_options;
    ipm_options.common.print_level = 3;
    IpmSolver ipm(ipm_options);
    auto ipm_at = [&](double p, const std::optional<WarmStartData> &w) -> SolveResult {
        disc->p = p;
        return w ? ipm.solve(problem, x0, *w) : ipm.solve(problem, x0);
    };
    ok &= sweep("IPM", false, ipm_at, cold);
    ok &= sweep("IPM", true, ipm_at, warm);

    SqpSolver sqp{SqpOptions{}};
    auto sqp_at = [&](double p, const std::optional<WarmStartData> &w) -> SolveResult {
        disc->p = p;
        return w ? sqp.solve(problem, x0, *w) : sqp.solve(problem, x0);
    };
    long long sqp_cold = 0, sqp_warm = 0;
    ok &= sweep("SQP", false, sqp_at, sqp_cold);
    ok &= sweep("SQP", true, sqp_at, sqp_warm);

    ok &= (warm <= cold) && (sqp_warm <= sqp_cold);
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
