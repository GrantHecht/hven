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
// NOTE the rebuild per p. A model's callbacks must be pure functions of x
// ("results are cached per iterate"), so changing p inside a live model is not
// allowed: the engines would serve values cached at the old p. Each p is a new
// model object; the warm-start payload stays valid across them because the
// problem's declared structure is unchanged.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/make_nlp_program.h"
#include "hven/model/nlp_problem_model.h"
#include "hven/model/nlp_triplet_model.h"

namespace {

using namespace hven::solvers;
constexpr double kInf = std::numeric_limits<double>::infinity();

struct Disc : NlpTripletModel {
    explicit Disc(double p) : p_(p) {}
    double p_;
    int num_vars() const override { return 2; }
    int num_cons() const override { return 1; }
    int num_jac_nonzeros() const override { return 2; }
    int num_hess_nonzeros() const override { return 2; }
    void bounds(Eigen::Ref<Eigen::VectorXd> xl, Eigen::Ref<Eigen::VectorXd> xu,
                Eigen::Ref<Eigen::VectorXd> gl, Eigen::Ref<Eigen::VectorXd> gu) const override {
        xl.setConstant(-kInf);
        xu.setConstant(kInf);
        gl << -kInf;
        gu << 1.0 + 0.5 * p_;
    }
    void eval_f(hven::ConstEigenRef<Eigen::VectorXd> x, double &f) const override {
        f = std::pow(x[0] - 2 * p_, 2) + std::pow(x[1] - p_, 2);
    }
    void eval_grad_f(hven::ConstEigenRef<Eigen::VectorXd> x,
                     Eigen::Ref<Eigen::VectorXd> g) const override {
        g << 2 * (x[0] - 2 * p_), 2 * (x[1] - p_);
    }
    void eval_g(hven::ConstEigenRef<Eigen::VectorXd> x,
                Eigen::Ref<Eigen::VectorXd> g) const override {
        g << x.squaredNorm();
    }
    void jac_structure(Eigen::Ref<Eigen::VectorXi> r,
                       Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jac(hven::ConstEigenRef<Eigen::VectorXd> x,
                  Eigen::Ref<Eigen::VectorXd> v) const override {
        v = 2.0 * x;
    }
    void hess_structure(Eigen::Ref<Eigen::VectorXi> r,
                        Eigen::Ref<Eigen::VectorXi> c) const override {
        r << 0, 1;
        c << 0, 1;
    }
    void eval_hess(hven::ConstEigenRef<Eigen::VectorXd>, double obj_factor,
                   hven::ConstEigenRef<Eigen::VectorXd> lambda,
                   Eigen::Ref<Eigen::VectorXd> v) const override {
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

    // Interior-point engine: re-transcribe at each p.
    IpmOptions ipm_options;
    ipm_options.common.print_level = 3;
    IpmSolver ipm(ipm_options);
    auto ipm_at = [&](double p, const std::optional<WarmStartData> &w) -> SolveResult {
        const auto program = make_nlp_program(std::make_shared<Disc>(p));
        return w ? ipm.solve(*program, x0, *w) : ipm.solve(*program, x0);
    };
    ok &= sweep("IPM", false, ipm_at, cold);
    ok &= sweep("IPM", true, ipm_at, warm);

    // SQP engine: convert at each p.
    SqpSolver sqp{SqpOptions{}};
    auto sqp_at = [&](double p, const std::optional<WarmStartData> &w) -> SolveResult {
        NlpProblemModel model(std::make_shared<Disc>(p));
        return w ? sqp.solve(model, x0, *w) : sqp.solve(model, x0);
    };
    long long sqp_cold = 0, sqp_warm = 0;
    ok &= sweep("SQP", false, sqp_at, sqp_cold);
    ok &= sweep("SQP", true, sqp_at, sqp_warm);

    // The warm sweep must not cost more iterations than the cold one.
    ok &= (warm <= cold) && (sqp_warm <= sqp_cold);
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
