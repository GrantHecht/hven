// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 7: the SQP engine in quasi-Newton mode (SqpOptions::hessian_mode).
//
//     min  sum_i (x_i - i)^2,  i = 1..4      s.t.  x0^2 + x1^2 + x2^2 + x3^2 <= 4
//
// The nearest point of the ball of radius 2 to p = (1, 2, 3, 4) is 2*p/|p|:
//     f* = (|p| - 2)^2 = (sqrt(30) - 2)^2,   inequality multiplier = |p|/2 - 1.
//
// This example states the problem directly as an NlpModel, the SQP engine's
// native model contract (the other examples use the triplet model both engines
// accept): equality and inequality blocks are separate (cE = 0, cI <= 0), the
// derivative matrices are sparse (row-major) and the Hessian is the UPPER
// triangle of the Lagrangian Hessian.
//
// With hessian_mode = kLbfgsDamped the engine builds its own damped L-BFGS
// curvature and never calls eval_hess (counters.eval_hess_calls stays 0), so
// the model needs first derivatives only; the exact-Hessian solve is run next
// to it for comparison.

#include <cmath>
#include <cstdio>
#include <limits>

#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp_model.h"

namespace {

using namespace hven::solvers;
using hven::SpMatRM;
using hven::Vec;

class Ball : public NlpModel {
  public:
    Ball()
        : lower_(Vec::Constant(4, -std::numeric_limits<double>::infinity())),
          upper_(Vec::Constant(4, std::numeric_limits<double>::infinity())) {}

    hven::Index n() const override { return 4; }
    hven::Index me() const override { return 0; }
    hven::Index mi() const override { return 1; }

    double eval_f(const Vec &x) const override { return (x - target()).squaredNorm(); }
    const Vec &lower() const override { return lower_; }
    const Vec &upper() const override { return upper_; }
    Vec start_point() const override { return Vec::Zero(4); }

    // The by-value evaluations are the pure-virtual core of the contract (the
    // *_in_place forms have defaults that call them). They are marked
    // deprecated, so the overrides sit inside the library's suppression bracket.
    HVEN_SUPPRESS_DEPRECATED_BEGIN
    Vec eval_grad(const Vec &x) const override { return 2.0 * (x - target()); }
    Vec eval_ce(const Vec &) const override { return Vec(0); }
    Vec eval_ci(const Vec &x) const override { return Vec::Constant(1, x.squaredNorm() - 4.0); }
    SpMatRM eval_jac_e(const Vec &) const override { return SpMatRM(0, 4); }
    SpMatRM eval_jac_i(const Vec &x) const override {
        SpMatRM J(1, 4);
        for (int j = 0; j < 4; j++) {
            J.insert(0, j) = 2.0 * x[j];
        }
        J.makeCompressed();
        return J;
    }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &,
                      const Vec &lambda_i) const override {
        // Diagonal; every entry emitted as a structural nonzero whatever the
        // multiplier's value (the pattern must not depend on lambda).
        SpMatRM H(4, 4);
        for (int j = 0; j < 4; j++) {
            H.insert(j, j) = 2.0 * obj_scale + 2.0 * lambda_i[0];
        }
        H.makeCompressed();
        return H;
    }
    HVEN_SUPPRESS_DEPRECATED_END

  private:
    static Vec target() { return Vec{{1.0, 2.0, 3.0, 4.0}}; }
    Vec lower_, upper_;
};

bool report(const char *label, const SqpResult &r) {
    const double f_star = std::pow(std::sqrt(30.0) - 2.0, 2);
    const double lambda_star = 0.5 * std::sqrt(30.0) - 1.0;
    const bool ok = r.status == SolveStatus::kOptimal && std::abs(r.f - f_star) < 1e-6 &&
                    std::abs(r.lambda_i[0] - lambda_star) < 1e-4;
    std::printf(
        "%-12s status=%-8s iterations=%2lld  f=%.8f (exact %.8f)  eval_hess calls=%lld  %s\n",
        label, to_string(r.status), static_cast<long long>(r.iterations), r.f, f_star,
        static_cast<long long>(r.counters.eval_hess_calls), ok ? "OK" : "FAILED");
    return ok;
}

} // namespace

int main() {
    Ball model;
    const Vec x0 = Vec::Zero(4);

    SqpOptions exact;
    SqpSolver exact_solver(exact);
    const SqpResult r_exact = exact_solver.solve(model, x0);

    SqpOptions qn;
    qn.hessian_mode = SqpHessianMode::kLbfgsDamped;
    qn.qn_memory = 10; // L-BFGS window (default 10; shown for the knob)
    SqpSolver qn_solver(qn);
    const SqpResult r_qn = qn_solver.solve(model, x0);

    const bool ok_exact = report("exact", r_exact);
    const bool ok_qn = report("L-BFGS", r_qn) && r_qn.counters.eval_hess_calls == 0;
    return (ok_exact && ok_qn) ? 0 : 1;
}
