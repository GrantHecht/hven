// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// M8 F1 probe: ONE interior-point solve (optionally a second) of a small Hock-Schittkowski
// problem in a fresh process. Prints the engine's answer and counters (asserted equal
// across arms) and the timings (informational).
//   usage: hs_probe <hs006|hs010|hs035|hs071> [twice]

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>

#include <hven/hven.h>

namespace {

using namespace hven;

struct Hs006 : Nlp { // min (1-x0)^2  s.t. 10(x1 - x0^2) = 0
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 1};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 0.0;
        gu << 0.0;
    }
    double eval_f(ConstVecRef x) const override { return (1 - x[0]) * (1 - x[0]); }
    void eval_grad_f(ConstVecRef x, VecRef g) const override { g << -2 * (1 - x[0]), 0.0; }
    void eval_g(ConstVecRef x, VecRef g) const override { g << 10 * (x[1] - x[0] * x[0]); }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override { v << -20 * x[0], 10.0; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0;
        c << 0;
    }
    void eval_hessian(ConstVecRef, double of, ConstVecRef l, VecRef v) const override {
        v << of * 2 - 20 * l[0];
    }
};

struct Hs010 : Nlp { // min x0 - x1  s.t. -3x0^2 + 2x0x1 - x1^2 + 1 >= 0
    NlpDimensions dimensions() const override {
        return {.n = 2, .m = 1, .jacobian_nonzeros = 2, .hessian_nonzeros = 3};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInfinity);
        xu.setConstant(kInfinity);
        gl << 0.0;
        gu << kInfinity;
    }
    double eval_f(ConstVecRef x) const override { return x[0] - x[1]; }
    void eval_grad_f(ConstVecRef, VecRef g) const override { g << 1.0, -1.0; }
    void eval_g(ConstVecRef x, VecRef g) const override {
        g << -3 * x[0] * x[0] + 2 * x[0] * x[1] - x[1] * x[1] + 1;
    }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0;
        c << 0, 1;
    }
    void eval_jacobian(ConstVecRef x, VecRef v) const override {
        v << -6 * x[0] + 2 * x[1], 2 * x[0] - 2 * x[1];
    }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1;
        c << 0, 0, 1;
    }
    void eval_hessian(ConstVecRef, double, ConstVecRef l, VecRef v) const override {
        v << -6 * l[0], 2 * l[0], -2 * l[0];
    }
};

struct Hs035 : Nlp { // quadratic, x0 + x1 + 2x2 <= 3, x >= 0
    NlpDimensions dimensions() const override {
        return {.n = 3, .m = 1, .jacobian_nonzeros = 3, .hessian_nonzeros = 5};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(0.0);
        xu.setConstant(kInfinity);
        gl << -kInfinity;
        gu << 3.0;
    }
    double eval_f(ConstVecRef x) const override {
        return 9 - 8 * x[0] - 6 * x[1] - 4 * x[2] + 2 * x[0] * x[0] + 2 * x[1] * x[1] +
               x[2] * x[2] + 2 * x[0] * x[1] + 2 * x[0] * x[2];
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g << -8 + 4 * x[0] + 2 * x[1] + 2 * x[2], -6 + 4 * x[1] + 2 * x[0], -4 + 2 * x[2] + 2 * x[0];
    }
    void eval_g(ConstVecRef x, VecRef g) const override { g << x[0] + x[1] + 2 * x[2]; }
    void jacobian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 0, 0;
        c << 0, 1, 2;
    }
    void eval_jacobian(ConstVecRef, VecRef v) const override { v << 1.0, 1.0, 2.0; }
    void hessian_structure(IndexVecRef r, IndexVecRef c) const override {
        r << 0, 1, 1, 2, 2;
        c << 0, 0, 1, 0, 2;
    }
    void eval_hessian(ConstVecRef, double of, ConstVecRef, VecRef v) const override {
        v << 4 * of, 2 * of, 4 * of, 2 * of, 2 * of;
    }
};

struct Hs071 : Nlp { // examples/01_hs071_both_engines.cpp's statement
    NlpDimensions dimensions() const override {
        return {.n = 4, .m = 2, .jacobian_nonzeros = 8, .hessian_nonzeros = 10};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(1.0);
        xu.setConstant(5.0);
        gl << 25.0, 40.0;
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
        r << 0, 1, 1, 2, 2, 2, 3, 3, 3, 3;
        c << 0, 0, 1, 0, 1, 2, 0, 1, 2, 3;
    }
    void eval_hessian(ConstVecRef x, double of, ConstVecRef l, VecRef v) const override {
        v << of * 2 * x[3] + l[1] * 2, of * x[3] + l[0] * x[2] * x[3], l[1] * 2,
            of * x[3] + l[0] * x[1] * x[3], l[0] * x[0] * x[3], l[1] * 2,
            of * (2 * x[0] + x[1] + x[2]) + l[0] * x[1] * x[2], of * x[0] + l[0] * x[0] * x[2],
            of * x[0] + l[0] * x[0] * x[1], l[1] * 2;
    }
};

void solve_once(const char *name, int k, Problem &p, const Eigen::VectorXd &x0) {
    IpmSolver ipm;
    const auto t0 = std::chrono::steady_clock::now();
    const IpmResult r = ipm.solve(p, x0);
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("%s solve=%d status=%s iterations=%lld factorizations=%lld f=%.17g "
                "init_s=%.6f total_s=%.6f kkt_s=%.6f solve_wall_s=%.6f\n",
                name, k, to_string(r.status), static_cast<long long>(r.iterations),
                static_cast<long long>(r.work.factorizations), r.f,
                r.timing.solver_initialization_seconds, r.timing.total_seconds,
                r.timing.kkt_seconds, wall);
}

} // namespace

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "hs071";
    std::shared_ptr<Nlp> nlp;
    Eigen::VectorXd x0;
    if (!std::strcmp(name, "hs006")) {
        nlp = std::make_shared<Hs006>();
        x0.resize(2);
        x0 << -1.2, 1.0;
    } else if (!std::strcmp(name, "hs010")) {
        nlp = std::make_shared<Hs010>();
        x0.resize(2);
        x0 << -10.0, 10.0;
    } else if (!std::strcmp(name, "hs035")) {
        nlp = std::make_shared<Hs035>();
        x0.resize(3);
        x0 << 0.5, 0.5, 0.5;
    } else {
        nlp = std::make_shared<Hs071>();
        x0.resize(4);
        x0 << 1.0, 5.0, 5.0, 1.0;
    }
    Problem problem(nlp);
    solve_once(name, 1, problem, x0);
    if (argc > 2 && !std::strcmp(argv[2], "twice")) {
        solve_once(name, 2, problem, x0);
    }
    return 0;
}
