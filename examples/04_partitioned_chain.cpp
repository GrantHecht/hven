// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 4: a sparse, partitioned problem built from PIECES.
//
// Instead of one callback model, the problem is declared as a few small
// functions (pieces), each applied many times over index maps. A piece states
// its sizes and evaluates small dense blocks over its own inputs; the library
// writes the KKT claims, the scatter, the locks and the partitioning
// (model/local_blocks.h).
//
// A direct-transcription optimal-control chain, N steps:
//
//     min   sum_i  h*u_i^2  +  h*(y_i - 0.3)^2
//     s.t.  y_{i+1} - y_i - h*(u_i - y_i^2) = 0,   i = 0..N-1   (dynamics)
//           y_0 = 0,  y_N = 0.5                                  (boundary)
//           -2 <= u_i <= 2
//
// with variables z = (y_0..y_N, u_0..u_{N-1}). Solved by IpmSolver once on one
// partition and once asking for four, and the answers must agree. (The SQP engine
// does not solve a problem built from pieces.)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <stdexcept>

#include <hven/hven.h>

namespace {

using hven::BlockEntry;
using hven::ConstVecRef;
using hven::Index;
using hven::MatRef;
using hven::VecRef;

// y_{i+1} - y_i - h*(u_i - y_i^2): inputs (y_i, y_{i+1}, u_i), one row.
struct Defect {
    double h;
    static constexpr Index kInputs = 3, kOutputs = 1;
    // Only d2/dy_i^2 is nonzero: claim that one Hessian entry, not the whole triangle.
    static constexpr std::array<BlockEntry, 1> kHessianPattern{{{0, 0}}};
    void value(ConstVecRef v, VecRef out) const { out[0] = v[1] - v[0] - h * (v[2] - v[0] * v[0]); }
    void jacobian(ConstVecRef v, MatRef J) const { J << -1.0 + 2.0 * h * v[0], 1.0, -h; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(0, 0) = 2.0 * h * lambda[0]; }
};

// weight * (v - ref)^2 on one variable.
struct Quadratic {
    double weight, ref;
    static constexpr Index kInputs = 1;
    double value(ConstVecRef v) const { return weight * (v[0] - ref) * (v[0] - ref); }
    void gradient(ConstVecRef v, VecRef g) const { g[0] = 2.0 * weight * (v[0] - ref); }
    void hessian(ConstVecRef, MatRef H) const { H(0, 0) = 2.0 * weight; }
};

// Two linear rows: y_0 - a and y_N - b. Diagonal Jacobian, no Hessian.
struct Boundary {
    double a, b;
    static constexpr Index kInputs = 2, kOutputs = 2;
    static constexpr std::array<BlockEntry, 2> kJacobianPattern{{{0, 0}, {1, 1}}};
    static constexpr auto kHessianPattern = hven::kNoHessian;
    void value(ConstVecRef v, VecRef out) const { out << v[0] - a, v[1] - b; }
    void jacobian(ConstVecRef, MatRef J) const { J(0, 0) = J(1, 1) = 1.0; }
};

hven::Problem make_chain(int steps, int partitions) {
    const double h = 1.0 / steps;
    const int u0 = steps + 1; // y_0..y_N, then u_0..u_{N-1}
    Eigen::MatrixXi y_idx(1, steps), u_idx(1, steps), defect_vars(3, steps), defect_rows(1, steps);
    for (int i = 0; i < steps; i++) {
        y_idx(0, i) = i;
        u_idx(0, i) = u0 + i;
        defect_vars.col(i) << i, i + 1, u0 + i;
        defect_rows(0, i) = i;
    }
    const Eigen::MatrixXi boundary_vars = (Eigen::MatrixXi(2, 1) << 0, steps).finished();
    const Eigen::MatrixXi boundary_rows = (Eigen::MatrixXi(2, 1) << steps, steps + 1).finished();

    hven::AssemblyDeclaration d(2 * steps + 1);
    d.add_objective(Quadratic{h, 0.0}, u_idx);
    d.add_objective(Quadratic{h, 0.3}, y_idx);
    d.add_equality(Defect{h}, defect_vars, defect_rows);
    d.add_equality(Boundary{0.0, 0.5}, boundary_vars, boundary_rows);
    d.set_bounds(u0, u0 + steps - 1, -2.0, 2.0);
    d.set_partitions(partitions);
    return hven::Problem(std::move(d));
}

} // namespace

int main() {
    const int steps = 500;
    const double h = 1.0 / steps;
    hven::Vec x0 = hven::Vec::Zero(2 * steps + 1);
    for (int i = 0; i <= steps; i++) {
        x0[i] = 0.5 * i / steps; // straight line from y_0 = 0 to y_N = 0.5
    }

    hven::IpmResult results[2];
    Index adopted[2] = {0, 0};
    for (int k = 0; k < 2; k++) {
        hven::Problem chain = make_chain(steps, k == 0 ? 1 : 4);
        results[k] = hven::IpmSolver{}.solve(chain, x0);
        adopted[k] = chain.num_partitions();
        const auto &r = results[k];
        std::printf("%d partition(s) requested: adopted=%lld status=%-8s iterations=%3lld "
                    "f=%.9f |c|=%.1e\n",
                    k == 0 ? 1 : 4, static_cast<long long>(adopted[k]), to_string(r.status),
                    static_cast<long long>(r.iterations), r.f, r.feasibility);
    }

    // An independent check of the pieces: the objective and the worst dynamics
    // residual, recomputed from the returned point.
    const hven::Vec &x = results[0].x;
    double f_check = 0.0, worst_defect = 0.0;
    for (int i = 0; i < steps; i++) {
        const double y = x[i], yn = x[i + 1], u = x[steps + 1 + i];
        f_check += h * u * u + h * (y - 0.3) * (y - 0.3);
        worst_defect = std::max(worst_defect, std::abs(yn - y - h * (u - y * y)));
    }
    const bool ok = results[0].status == hven::SolveStatus::kOptimal &&
                    results[1].status == hven::SolveStatus::kOptimal && adopted[1] > 1 &&
                    std::abs(f_check - results[0].f) < 1e-9 && worst_defect < 1e-8 &&
                    std::abs(results[0].f - results[1].f) < 1e-8 &&
                    (results[0].x - results[1].x).lpNorm<Eigen::Infinity>() < 1e-6;
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
