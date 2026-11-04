// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AP-2 B6: the pieces layer.
//
//   * The local-block layer (model/local_blocks.h) against a dense reference
//     assembled straight from the same block functions: values, gradients,
//     residuals, adjoint gradients and the KKT matrix's Hessian and Jacobian
//     blocks, on one partition and on several. The dense reference's blocks are
//     checked against central differences, and the same problem as an Nlp solves
//     to the same point.
//   * Example 4 in both forms -- the raw protocol (the expert route, kept here)
//     and the local-block layer: the same status, objective and point, and the
//     same claim and slot multiset.
//   * The builder: derived row counts and overcounts, and every refusal by name.
//   * A pieces problem's row space (lambda and g), its warm-start round trip, the
//     evaluation-failure contract (core/errors.h) on a piece, and the SQP's
//     refusal.

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/core/errors.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/sqp_solver.h"
#include "hven/model/nlp.h"
#include "hven/model/pieces.h"
#include "hven/warmstart/warm_start_data.h"

namespace pieces_test {

using hven::ConstVecRef;
using hven::Index;
using hven::MatRef;
using hven::Vec;
using hven::VecRef;
using hven::solvers::AssemblyDeclaration;
using hven::solvers::BlockEntry;
using hven::solvers::IpmOptions;
using hven::solvers::IpmResult;
using hven::solvers::IpmSolver;
using hven::solvers::NonLinearProgram;
using hven::solvers::Problem;
using hven::solvers::SolveStatus;
using hven::solvers::Verbosity;
using hven::solvers::detail::ProblemAccess;
using Vx = Eigen::VectorXd;
using Vi = Eigen::VectorXi;
using Mi = Eigen::MatrixXi;
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;
constexpr double kInf = std::numeric_limits<double>::infinity();

IpmSolver quiet_ipm() {
    IpmOptions options;
    options.common.verbosity = Verbosity::kSilent;
    return IpmSolver(options);
}

// ===========================================================================
// Example 4, the raw protocol: the expert route. Hand-written claims, scatter
// and lock protocol. The objective pieces carry no constraint surface and no
// piece type is registered anywhere: the default adapter stores each as itself.
// ===========================================================================

namespace raw {

using hven::solvers::kkt_canonical_lock_col;
using hven::solvers::SolverIndexingData;

void kkt_add(SpMatRM &kkt, const Eigen::Ref<Vi> &locations, const Eigen::Ref<Vi> &clashes,
             std::vector<std::mutex> &locks, int slot, int lock_col, double value,
             bool check_clash) {
    const bool lock = check_clash && clashes[lock_col] != -1;
    if (lock) {
        locks[clashes[lock_col]].lock();
    }
    kkt.valuePtr()[locations[slot]] += value;
    if (lock) {
        locks[clashes[lock_col]].unlock();
    }
}

// weight * (z - ref)^2 on one variable per application.
struct QuadraticTerm {
    std::string label; // any movable type is a legal piece member
    double weight, ref;
    std::string name() const { return label; }
    int input_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void objective(double scale, const Eigen::Ref<const Vx> &X, double &val,
                   const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double e = X[d.v_loc(0, V)] - ref;
            val += scale * weight * e * e;
        }
    }
    void objective_gradient(double scale, const Eigen::Ref<const Vx> &X, double &val,
                            Eigen::Ref<Vx> GX, const SolverIndexingData &d) const {
        this->objective(scale, X, val, d);
        for (int V = 0; V < d.num_appl(); V++) {
            GX[d.inner_gradient_starts_[V]] = scale * 2.0 * weight * (X[d.v_loc(0, V)] - ref);
        }
    }
    void objective_gradient_hessian(double scale, const Eigen::Ref<const Vx> &X, double &val,
                                    Eigen::Ref<Vx> GX, SpMatRM &kkt, Eigen::Ref<Vi> locations,
                                    Eigen::Ref<Vi> clashes, std::vector<std::mutex> &locks,
                                    const SolverIndexingData &d) const {
        this->objective_gradient(scale, X, val, GX, d);
        for (int V = 0; V < d.num_appl(); V++) {
            const int r = d.v_scatter_loc(0, V);
            if (r < 0) {
                continue;
            }
            kkt_add(kkt, locations, clashes, locks, d.inner_kkt_starts_[V],
                    kkt_canonical_lock_col(r, r), scale * 2.0 * weight, true);
        }
    }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free_slot, int, bool,
                       bool do_hess, SolverIndexingData &d) {
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_hess) {
                rows[free_slot] = cols[free_slot] = d.v_scatter_loc(0, V);
                free_slot++;
            }
        }
    }
    int num_kkt_elements(bool, bool do_hess) const { return do_hess ? 1 : 0; }
};

// y1 - y0 - h*(u - y0^2): inputs (y_i, y_{i+1}, u_i), one row.
struct Defect {
    double h;
    std::string name() const { return "defect"; }
    int input_rows() const { return 3; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void constraints(const Eigen::Ref<const Vx> &X, Eigen::Ref<Vx> FX,
                     const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double y0 = X[d.v_loc(0, V)], y1 = X[d.v_loc(1, V)], u = X[d.v_loc(2, V)];
            FX[d.inner_constraint_starts_[V]] = y1 - y0 - h * (u - y0 * y0);
        }
    }
    void adjoint_gradient(const Eigen::Ref<const Vx> &X, const Eigen::Ref<const Vx> &L,
                          Eigen::Ref<Vx> AGX, const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double lam = L[d.c_loc(0, V)], y0 = X[d.v_loc(0, V)];
            const int g = d.inner_gradient_starts_[V];
            AGX[g + 0] += lam * (-1.0 + 2.0 * h * y0);
            AGX[g + 1] += lam * 1.0;
            AGX[g + 2] += lam * -h;
        }
    }
    void scatter_jacobian(const Eigen::Ref<const Vx> &X, SpMatRM &kkt, Eigen::Ref<Vi> locations,
                          Eigen::Ref<Vi> clashes, std::vector<std::mutex> &locks,
                          const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double jac[3] = {-1.0 + 2.0 * h * X[d.v_loc(0, V)], 1.0, -h};
            for (int k = 0; k < 3; k++) {
                const int col = d.v_scatter_loc(k, V);
                if (col < 0) {
                    continue;
                }
                kkt_add(kkt, locations, clashes, locks, d.inner_kkt_starts_[V] + k, col, jac[k],
                        !d.unique_constraints_);
            }
        }
    }
    void scatter_hessian(const Eigen::Ref<const Vx> &L, SpMatRM &kkt, Eigen::Ref<Vi> locations,
                         Eigen::Ref<Vi> clashes, std::vector<std::mutex> &locks,
                         const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const int r = d.v_scatter_loc(0, V);
            if (r < 0) {
                continue;
            }
            kkt_add(kkt, locations, clashes, locks, d.inner_kkt_starts_[V] + 3,
                    kkt_canonical_lock_col(r, r), 2.0 * h * L[d.c_loc(0, V)], true);
        }
    }
    void constraints_adjointgradient(const Eigen::Ref<const Vx> &X, const Eigen::Ref<const Vx> &L,
                                     Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
                                     const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        this->adjoint_gradient(X, L, AGX, d);
    }
    void constraints_jacobian(const Eigen::Ref<const Vx> &X, Eigen::Ref<Vx> FX, SpMatRM &kkt,
                              Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
                              std::vector<std::mutex> &locks, const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        this->scatter_jacobian(X, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient(const Eigen::Ref<const Vx> &X,
                                              const Eigen::Ref<const Vx> &L, Eigen::Ref<Vx> FX,
                                              Eigen::Ref<Vx> AGX, SpMatRM &kkt, Eigen::Ref<Vi> loc,
                                              Eigen::Ref<Vi> clash, std::vector<std::mutex> &locks,
                                              const SolverIndexingData &d) const {
        this->constraints_adjointgradient(X, L, FX, AGX, d);
        this->scatter_jacobian(X, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Vx> &X, const Eigen::Ref<const Vx> &L, Eigen::Ref<Vx> FX,
        Eigen::Ref<Vx> AGX, SpMatRM &kkt, Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
        std::vector<std::mutex> &locks, const SolverIndexingData &d) const {
        this->constraints_jacobian_adjointgradient(X, L, FX, AGX, kkt, loc, clash, locks, d);
        this->scatter_hessian(L, kkt, loc, clash, locks, d);
    }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free_slot, int con_offset,
                       bool do_jac, bool do_hess, SolverIndexingData &d) {
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_jac) {
                for (int k = 0; k < 3; k++, free_slot++) {
                    const int col = d.v_scatter_loc(k, V);
                    rows[free_slot] = (col < 0) ? -1 : d.c_loc(0, V) + con_offset;
                    cols[free_slot] = col;
                }
            }
            if (do_hess) {
                rows[free_slot] = cols[free_slot] = d.v_scatter_loc(0, V);
                free_slot++;
            }
        }
    }
    int num_kkt_elements(bool do_jac, bool do_hess) const {
        return (do_jac ? 3 : 0) + (do_hess ? 1 : 0);
    }
};

// Linear boundary rows: row j = input j - target_j.
struct Boundary {
    double target0, target1;
    std::string name() const { return "boundary"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 2; }
    bool thread_safe() const { return true; }

    void constraints(const Eigen::Ref<const Vx> &X, Eigen::Ref<Vx> FX,
                     const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            FX[d.inner_constraint_starts_[V] + 0] = X[d.v_loc(0, V)] - target0;
            FX[d.inner_constraint_starts_[V] + 1] = X[d.v_loc(1, V)] - target1;
        }
    }
    void constraints_adjointgradient(const Eigen::Ref<const Vx> &X, const Eigen::Ref<const Vx> &L,
                                     Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
                                     const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        for (int V = 0; V < d.num_appl(); V++) {
            AGX[d.inner_gradient_starts_[V] + 0] += L[d.c_loc(0, V)];
            AGX[d.inner_gradient_starts_[V] + 1] += L[d.c_loc(1, V)];
        }
    }
    void constraints_jacobian(const Eigen::Ref<const Vx> &X, Eigen::Ref<Vx> FX, SpMatRM &kkt,
                              Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
                              std::vector<std::mutex> &locks, const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        for (int V = 0; V < d.num_appl(); V++) {
            for (int k = 0; k < 2; k++) {
                const int col = d.v_scatter_loc(k, V);
                if (col >= 0) {
                    kkt_add(kkt, loc, clash, locks, d.inner_kkt_starts_[V] + k, col, 1.0,
                            !d.unique_constraints_);
                }
            }
        }
    }
    void constraints_jacobian_adjointgradient(const Eigen::Ref<const Vx> &X,
                                              const Eigen::Ref<const Vx> &L, Eigen::Ref<Vx> FX,
                                              Eigen::Ref<Vx> AGX, SpMatRM &kkt, Eigen::Ref<Vi> loc,
                                              Eigen::Ref<Vi> clash, std::vector<std::mutex> &locks,
                                              const SolverIndexingData &d) const {
        this->constraints_adjointgradient(X, L, FX, AGX, d);
        this->constraints_jacobian(X, FX, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Vx> &X, const Eigen::Ref<const Vx> &L, Eigen::Ref<Vx> FX,
        Eigen::Ref<Vx> AGX, SpMatRM &kkt, Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
        std::vector<std::mutex> &locks, const SolverIndexingData &d) const {
        this->constraints_jacobian_adjointgradient(X, L, FX, AGX, kkt, loc, clash, locks, d);
    }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free_slot, int con_offset,
                       bool do_jac, bool, SolverIndexingData &d) {
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_jac) {
                for (int k = 0; k < 2; k++, free_slot++) {
                    const int col = d.v_scatter_loc(k, V);
                    rows[free_slot] = (col < 0) ? -1 : d.c_loc(k, V) + con_offset;
                    cols[free_slot] = col;
                }
            }
        }
    }
    int num_kkt_elements(bool do_jac, bool) const { return do_jac ? 2 : 0; }
};

} // namespace raw

// ===========================================================================
// Example 4, the local-block layer (examples/04_partitioned_chain.cpp).
// ===========================================================================

namespace local {

struct Defect {
    double h;
    static constexpr Index kInputs = 3, kOutputs = 1;
    static constexpr std::array<BlockEntry, 1> kHessianPattern{{{0, 0}}};
    std::string name() const { return "defect"; }
    void value(ConstVecRef v, VecRef out) const { out[0] = v[1] - v[0] - h * (v[2] - v[0] * v[0]); }
    void jacobian(ConstVecRef v, MatRef J) const { J << -1.0 + 2.0 * h * v[0], 1.0, -h; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(0, 0) = 2.0 * h * lambda[0]; }
};

struct Quadratic {
    double weight, ref;
    static constexpr Index kInputs = 1;
    double value(ConstVecRef v) const { return weight * (v[0] - ref) * (v[0] - ref); }
    void gradient(ConstVecRef v, VecRef g) const { g[0] = 2.0 * weight * (v[0] - ref); }
    void hessian(ConstVecRef, MatRef H) const { H(0, 0) = 2.0 * weight; }
};

struct Boundary {
    double a, b;
    static constexpr Index kInputs = 2, kOutputs = 2;
    static constexpr std::array<BlockEntry, 2> kJacobianPattern{{{0, 0}, {1, 1}}};
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
    void value(ConstVecRef v, VecRef out) const { out << v[0] - a, v[1] - b; }
    void jacobian(ConstVecRef, MatRef J) const { J(0, 0) = J(1, 1) = 1.0; }
};

} // namespace local

struct ChainMaps {
    Mi y_idx, u_idx, defect_vars, defect_rows, boundary_vars, boundary_rows;
    int n = 0, u0 = 0;
};

ChainMaps chain_maps(int steps) {
    ChainMaps m;
    m.n = 2 * steps + 1;
    m.u0 = steps + 1;
    m.y_idx.resize(1, steps);
    m.u_idx.resize(1, steps);
    m.defect_vars.resize(3, steps);
    m.defect_rows.resize(1, steps);
    for (int i = 0; i < steps; i++) {
        m.y_idx(0, i) = i;
        m.u_idx(0, i) = m.u0 + i;
        m.defect_vars.col(i) << i, i + 1, m.u0 + i;
        m.defect_rows(0, i) = i;
    }
    m.boundary_vars = (Mi(2, 1) << 0, steps).finished();
    m.boundary_rows = (Mi(2, 1) << steps, steps + 1).finished();
    return m;
}

Problem raw_chain(int steps, int partitions) {
    const double h = 1.0 / steps;
    const ChainMaps m = chain_maps(steps);
    AssemblyDeclaration d;
    d.primal_vars_ = m.n;
    d.equality_rows_ = steps + 2;
    d.inequality_rows_ = 0;
    d.partition_count_ = partitions;
    d.objectives_.emplace_back(
        hven::solvers::ObjectiveInterface(raw::QuadraticTerm{"effort", h, 0.0}), m.u_idx);
    d.objectives_.emplace_back(
        hven::solvers::ObjectiveInterface(raw::QuadraticTerm{"tracking", h, 0.3}), m.y_idx);
    d.equality_constraints_.emplace_back(hven::solvers::ConstraintInterface(raw::Defect{h}),
                                         m.defect_vars, m.defect_rows);
    d.equality_constraints_.emplace_back(
        hven::solvers::ConstraintInterface(raw::Boundary{0.0, 0.5}), m.boundary_vars,
        m.boundary_rows);
    for (int i = 0; i < steps; i++) {
        d.variable_bounds_.push_back(hven::solvers::VariableBound{m.u0 + i, -2.0, 2.0});
    }
    return Problem(std::move(d));
}

Problem local_chain(int steps, int partitions) {
    const double h = 1.0 / steps;
    const ChainMaps m = chain_maps(steps);
    AssemblyDeclaration d(m.n);
    d.add_objective(local::Quadratic{h, 0.0}, m.u_idx);
    d.add_objective(local::Quadratic{h, 0.3}, m.y_idx);
    d.add_equality(local::Defect{h}, m.defect_vars, m.defect_rows);
    d.add_equality(local::Boundary{0.0, 0.5}, m.boundary_vars, m.boundary_rows);
    d.set_bounds(m.u0, m.u0 + steps - 1, -2.0, 2.0);
    d.set_partitions(partitions);
    return Problem(std::move(d));
}

Vx chain_start(int steps) {
    Vx x0 = Vx::Zero(2 * steps + 1);
    for (int i = 0; i <= steps; i++) {
        x0[i] = 0.5 * i / steps;
    }
    return x0;
}

/// The (row, col) claims of a laid program, sorted: the claim multiset.
std::vector<std::pair<int, int>> claim_multiset(Problem &problem) {
    NonLinearProgram &program = ProblemAccess::ipm_program(problem);
    const auto rows = program.kkt_claim_rows();
    const auto cols = program.kkt_claim_cols();
    std::vector<std::pair<int, int>> claims;
    for (Index k = 0; k < rows.size(); k++) {
        claims.emplace_back(rows[k], cols[k]);
    }
    std::sort(claims.begin(), claims.end());
    return claims;
}

std::vector<int> sorted(Eigen::Ref<Vi> v) {
    std::vector<int> out(v.data(), v.data() + v.size());
    std::sort(out.begin(), out.end());
    return out;
}

// Example 4's two forms agree, on one partition and on several: the same status,
// the objective to 1e-10 relative, x to 1e-8, the same adopted partition count,
// and the same claim and slot multiset (KKT claims, gradient and residual slots).
// The iteration count is pinned equal too.
TEST(PiecesExample4, BothFormsAgree) {
    constexpr int kSteps = 500;
    for (const int partitions : {1, 4}) {
        SCOPED_TRACE(partitions);
        Problem raw_problem = raw_chain(kSteps, partitions);
        Problem local_problem = local_chain(kSteps, partitions);
        IpmSolver raw_ipm = quiet_ipm();
        IpmSolver local_ipm = quiet_ipm();
        const IpmResult a = raw_ipm.solve(raw_problem, chain_start(kSteps));
        const IpmResult b = local_ipm.solve(local_problem, chain_start(kSteps));
        ASSERT_EQ(a.status, SolveStatus::kOptimal);
        ASSERT_EQ(b.status, SolveStatus::kOptimal);
        EXPECT_LE(std::abs(a.f - b.f), 1e-10 * std::max(1.0, std::abs(a.f)));
        EXPECT_LE((a.x - b.x).lpNorm<Eigen::Infinity>(), 1e-8);
        EXPECT_EQ(a.iterations, b.iterations);
        EXPECT_EQ(raw_problem.partitions(), local_problem.partitions());
        if (partitions > 1) {
            EXPECT_GT(local_problem.partitions(), 1);
        }

        EXPECT_EQ(claim_multiset(raw_problem), claim_multiset(local_problem));
        NonLinearProgram &pa = ProblemAccess::ipm_program(raw_problem);
        NonLinearProgram &pb = ProblemAccess::ipm_program(local_problem);
        EXPECT_EQ(pa.num_user_kkt_elems_, pb.num_user_kkt_elems_);
        EXPECT_EQ(sorted(pa.pgx_coeff_rows()), sorted(pb.pgx_coeff_rows()));
        EXPECT_EQ(sorted(pa.agx_coeff_rows()), sorted(pb.agx_coeff_rows()));
        EXPECT_EQ(sorted(pa.econ_coeff_rows()), sorted(pb.econ_coeff_rows()));
    }
}

// ===========================================================================
// The local-block layer against a dense reference.
//
// A chain with every shape the adapter handles: an objective block with an
// off-diagonal Hessian entry, a linear objective (kNoHessian), a constraint
// block with the dense default patterns, a sparse Jacobian pattern, a block
// writing a row another piece also writes (a shared row), an inequality block
// with a diagonal Hessian pattern, and one application that names a variable
// twice.
// ===========================================================================

struct Smooth { // w*(v1 - v0)^2 + 0.1*v0*v1
    double w;
    static constexpr Index kInputs = 2;
    double value(ConstVecRef v) const {
        return w * (v[1] - v[0]) * (v[1] - v[0]) + 0.1 * v[0] * v[1];
    }
    void gradient(ConstVecRef v, VecRef g) const {
        g[0] = -2.0 * w * (v[1] - v[0]) + 0.1 * v[1];
        g[1] = 2.0 * w * (v[1] - v[0]) + 0.1 * v[0];
    }
    void hessian(ConstVecRef, MatRef H) const {
        H(0, 0) = 2.0 * w;
        H(1, 0) = -2.0 * w + 0.1;
        H(1, 1) = 2.0 * w;
    }
};

struct Linear { // c * v0
    double c;
    static constexpr Index kInputs = 1;
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
    double value(ConstVecRef v) const { return c * v[0]; }
    void gradient(ConstVecRef, VecRef g) const { g[0] = c; }
};

struct DenseDefect { // y1 - y0 - h*(u - y0^2) + 0.05*sin(y0*u): dense patterns
    double h;
    static constexpr Index kInputs = 3, kOutputs = 1;
    std::string name() const { return "dense defect"; }
    void value(ConstVecRef v, VecRef out) const {
        out[0] = v[1] - v[0] - h * (v[2] - v[0] * v[0]) + 0.05 * std::sin(v[0] * v[2]);
    }
    void jacobian(ConstVecRef v, MatRef J) const {
        const double c = 0.05 * std::cos(v[0] * v[2]);
        J(0, 0) = -1.0 + 2.0 * h * v[0] + c * v[2];
        J(0, 1) = 1.0;
        J(0, 2) = -h + c * v[0];
    }
    void hessian(ConstVecRef v, ConstVecRef lambda, MatRef H) const {
        const double s = 0.05 * std::sin(v[0] * v[2]), c = 0.05 * std::cos(v[0] * v[2]);
        H(0, 0) = lambda[0] * (2.0 * h - s * v[2] * v[2]);
        H(2, 0) = lambda[0] * (c - s * v[0] * v[2]);
        H(2, 2) = lambda[0] * (-s * v[0] * v[0]);
    }
};

struct Nudge { // 1e-3 * v0^2, into a row another piece also writes
    static constexpr Index kInputs = 1, kOutputs = 1;
    void value(ConstVecRef v, VecRef out) const { out[0] = 1e-3 * v[0] * v[0]; }
    void jacobian(ConstVecRef v, MatRef J) const { J(0, 0) = 2e-3 * v[0]; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(0, 0) = 2e-3 * lambda[0]; }
};

struct Ends { // v0 - a, v1 - b: diagonal Jacobian pattern, linear
    double a, b;
    static constexpr Index kInputs = 2, kOutputs = 2;
    static constexpr std::array<BlockEntry, 2> kJacobianPattern{{{0, 0}, {1, 1}}};
    static constexpr auto kHessianPattern = hven::solvers::kNoHessian;
    void value(ConstVecRef v, VecRef out) const { out << v[0] - a, v[1] - b; }
    void jacobian(ConstVecRef, MatRef J) const { J(0, 0) = J(1, 1) = 1.0; }
};

struct Ring { // v0*v1 + v0^2 - r2 <= 0: dense Jacobian, full lower Hessian
    double r2;
    static constexpr Index kInputs = 2, kOutputs = 1;
    void value(ConstVecRef v, VecRef out) const { out[0] = v[0] * v[1] + v[0] * v[0] - r2; }
    void jacobian(ConstVecRef v, MatRef J) const {
        J(0, 0) = v[1] + 2.0 * v[0];
        J(0, 1) = v[0];
    }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const {
        H(0, 0) = 2.0 * lambda[0];
        H(1, 0) = lambda[0];
    }
};

struct Disc { // v0^2 + v1^2 - r2 <= 0: diagonal Hessian pattern
    double r2;
    static constexpr Index kInputs = 2, kOutputs = 1;
    static constexpr std::array<BlockEntry, 2> kHessianPattern{{{0, 0}, {1, 1}}};
    void value(ConstVecRef v, VecRef out) const { out[0] = v[0] * v[0] + v[1] * v[1] - r2; }
    void jacobian(ConstVecRef v, MatRef J) const {
        J(0, 0) = 2.0 * v[0];
        J(0, 1) = 2.0 * v[1];
    }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const {
        H(0, 0) = H(1, 1) = 2.0 * lambda[0];
    }
};

/// The test chain's index maps, and a dense reference assembled from the same
/// block functions with global indices -- independent of the adapter's claims,
/// scatter and locks.
struct TestChain {
    int steps = 0, n = 0, u0 = 0, me = 0, mi = 0;
    double h = 0.0;
    Mi y_pairs, u_single, y_single, defect_vars, defect_rows, nudge_vars, nudge_rows, end_vars,
        end_rows, ring_vars, ring_rows, disc_vars, disc_rows;

    explicit TestChain(int steps_) : steps(steps_) {
        n = 2 * steps + 1;
        u0 = steps + 1;
        h = 1.0 / steps;
        y_pairs.resize(2, steps);
        u_single.resize(1, steps);
        y_single.resize(1, steps);
        defect_vars.resize(3, steps);
        defect_rows.resize(1, steps);
        for (int i = 0; i < steps; i++) {
            y_pairs.col(i) << i, i + 1;
            u_single(0, i) = u0 + i;
            y_single(0, i) = i;
            defect_vars.col(i) << i, i + 1, u0 + i;
            defect_rows(0, i) = i;
        }
        // Row i also gets 1e-3*u_{i+1}^2: a shared row, on a variable the defect
        // of row i does not read.
        nudge_vars.resize(1, steps - 1);
        nudge_rows.resize(1, steps - 1);
        for (int i = 0; i + 1 < steps; i++) {
            nudge_vars(0, i) = u0 + i + 1;
            nudge_rows(0, i) = i;
        }
        end_vars = (Mi(2, 1) << 0, steps).finished();
        end_rows = (Mi(2, 1) << steps, steps + 1).finished();
        me = steps + 2;
        // Inequalities: ring on (y_i, u_i), plus one application on (y_1, y_1);
        // disc on (y_i, y_{i+1}).
        ring_vars.resize(2, steps + 1);
        ring_rows.resize(1, steps + 1);
        for (int i = 0; i < steps; i++) {
            ring_vars.col(i) << i, u0 + i;
            ring_rows(0, i) = i;
        }
        ring_vars.col(steps) << 1, 1;
        ring_rows(0, steps) = steps;
        disc_vars = y_pairs;
        disc_rows.resize(1, steps);
        for (int i = 0; i < steps; i++) {
            disc_rows(0, i) = steps + 1 + i;
        }
        mi = 2 * steps + 1;
    }

    AssemblyDeclaration declaration(int partitions) const {
        AssemblyDeclaration d(n);
        d.add_objective(local::Quadratic{h, 0.0}, u_single);
        d.add_objective(Smooth{0.5}, y_pairs);
        d.add_objective(Linear{0.01}, u_single);
        d.add_equality(DenseDefect{h}, defect_vars, defect_rows);
        d.add_equality(Nudge{}, nudge_vars, nudge_rows);
        d.add_equality(Ends{0.0, 0.5}, end_vars, end_rows);
        d.add_inequality(Ring{3.0}, ring_vars, ring_rows);
        d.add_inequality(Disc{2.0}, disc_vars, disc_rows);
        d.set_bounds(u0, u0 + steps - 1, -2.0, 2.0);
        d.set_partitions(partitions);
        return d;
    }

    // ---- the dense reference ----
    template <class P> static Vx gather(const P &, const Vx &x, const Mi &vars, Index V) {
        Vx v(P::kInputs);
        for (Index i = 0; i < P::kInputs; i++) {
            v[i] = x[vars(i, V)];
        }
        return v;
    }
    template <class P>
    static void add_objective(const P &p, const Mi &vars, const Vx &x, double &f, Vx *grad,
                              Eigen::MatrixXd *H) {
        for (Index V = 0; V < vars.cols(); V++) {
            const Vx v = gather(p, x, vars, V);
            f += p.value(v);
            if (grad != nullptr) {
                Vx g = Vx::Zero(P::kInputs);
                p.gradient(v, g);
                for (Index i = 0; i < P::kInputs; i++) {
                    (*grad)[vars(i, V)] += g[i];
                }
            }
            if constexpr (!hven::solvers::detail::DeclaresNoHessian<P>) {
                if (H != nullptr) {
                    Eigen::MatrixXd hb = Eigen::MatrixXd::Zero(P::kInputs, P::kInputs);
                    p.hessian(v, hb);
                    add_symmetric(hb, vars, V, *H);
                }
            }
        }
    }
    template <class P>
    static void add_constraint(const P &p, const Mi &vars, const Mi &rows, const Vx &x,
                               const Vx *lambda, Vx &c, Eigen::MatrixXd *J, Eigen::MatrixXd *H) {
        for (Index V = 0; V < vars.cols(); V++) {
            const Vx v = gather(p, x, vars, V);
            Vx out = Vx::Zero(P::kOutputs);
            p.value(v, out);
            Eigen::MatrixXd jb = Eigen::MatrixXd::Zero(P::kOutputs, P::kInputs);
            p.jacobian(v, jb);
            for (Index j = 0; j < P::kOutputs; j++) {
                c[rows(j, V)] += out[j];
                if (J != nullptr) {
                    for (Index i = 0; i < P::kInputs; i++) {
                        (*J)(rows(j, V), vars(i, V)) += jb(j, i);
                    }
                }
            }
            if constexpr (!hven::solvers::detail::DeclaresNoHessian<P>) {
                if (H != nullptr) {
                    Vx lam(P::kOutputs);
                    for (Index j = 0; j < P::kOutputs; j++) {
                        lam[j] = (*lambda)[rows(j, V)];
                    }
                    Eigen::MatrixXd hb = Eigen::MatrixXd::Zero(P::kInputs, P::kInputs);
                    p.hessian(v, lam, hb);
                    add_symmetric(hb, vars, V, *H);
                }
            }
        }
    }
    /// Sums a block's lower triangle into the full symmetric global matrix.
    static void add_symmetric(const Eigen::MatrixXd &hb, const Mi &vars, Index V,
                              Eigen::MatrixXd &H) {
        for (Index r = 0; r < hb.rows(); r++) {
            for (Index c = 0; c <= r; c++) {
                const Index a = vars(r, V), b = vars(c, V);
                H(a, b) += hb(r, c);
                if (r != c) {
                    H(b, a) += hb(r, c);
                }
            }
        }
    }

    struct Eval {
        double f = 0.0;
        Vx grad, ce, ci;
        Eigen::MatrixXd je, ji, hess;
    };

    /// f, grad f, c_E, c_I, J_E, J_I, and sigma * hess f + sum lambda_E hess c_E +
    /// sum lambda_I hess c_I.
    Eval evaluate(const Vx &x, double sigma, const Vx &le, const Vx &li) const {
        Eval e;
        e.grad = Vx::Zero(n);
        e.ce = Vx::Zero(me);
        e.ci = Vx::Zero(mi);
        e.je = Eigen::MatrixXd::Zero(me, n);
        e.ji = Eigen::MatrixXd::Zero(mi, n);
        Eigen::MatrixXd hf = Eigen::MatrixXd::Zero(n, n);
        add_objective(local::Quadratic{h, 0.0}, u_single, x, e.f, &e.grad, &hf);
        add_objective(Smooth{0.5}, y_pairs, x, e.f, &e.grad, &hf);
        add_objective(Linear{0.01}, u_single, x, e.f, &e.grad, &hf);
        e.hess = sigma * hf;
        add_constraint(DenseDefect{h}, defect_vars, defect_rows, x, &le, e.ce, &e.je, &e.hess);
        add_constraint(Nudge{}, nudge_vars, nudge_rows, x, &le, e.ce, &e.je, &e.hess);
        add_constraint(Ends{0.0, 0.5}, end_vars, end_rows, x, &le, e.ce, &e.je, &e.hess);
        add_constraint(Ring{3.0}, ring_vars, ring_rows, x, &li, e.ci, &e.ji, &e.hess);
        add_constraint(Disc{2.0}, disc_vars, disc_rows, x, &li, e.ci, &e.ji, &e.hess);
        return e;
    }
};

Vx test_point(int n, unsigned seed) {
    Vx x(n);
    for (int i = 0; i < n; i++) {
        x[i] = 0.3 * std::sin(1.7 * i + seed) + 0.1 * seed;
    }
    return x;
}

// The dense reference's blocks against central differences, so the comparisons
// below compare the adapter against correct derivatives.
TEST(PiecesLocalBlocks, TheDenseReferenceMatchesCentralDifferences) {
    const TestChain chain(3);
    const Vx x = test_point(chain.n, 1);
    const Vx le = test_point(chain.me, 2), li = test_point(chain.mi, 3);
    const auto base = chain.evaluate(x, 1.0, le, li);
    const double step = 1e-6;
    for (int k = 0; k < chain.n; k++) {
        Vx xp = x, xm = x;
        xp[k] += step;
        xm[k] -= step;
        const auto p = chain.evaluate(xp, 1.0, le, li);
        const auto m = chain.evaluate(xm, 1.0, le, li);
        EXPECT_NEAR(base.grad[k], (p.f - m.f) / (2 * step), 1e-7) << k;
        EXPECT_LE((base.je.col(k) - (p.ce - m.ce) / (2 * step)).lpNorm<Eigen::Infinity>(), 1e-7);
        EXPECT_LE((base.ji.col(k) - (p.ci - m.ci) / (2 * step)).lpNorm<Eigen::Infinity>(), 1e-7);
        const Vx lag_p = p.grad + p.je.transpose() * le + p.ji.transpose() * li;
        const Vx lag_m = m.grad + m.je.transpose() * le + m.ji.transpose() * li;
        EXPECT_LE((base.hess.col(k) - (lag_p - lag_m) / (2 * step)).lpNorm<Eigen::Infinity>(), 1e-6)
            << k;
    }
}

/// Evaluates a pieces problem's laid program at (x, sigma, le, li) and compares
/// every value it assembles against the dense reference.
void expect_program_matches(Problem &problem, const TestChain &chain, unsigned seed) {
    NonLinearProgram &nlp = ProblemAccess::ipm_program(problem);
    ASSERT_EQ(nlp.primal_vars_, chain.n);
    ASSERT_EQ(nlp.equal_cons_, chain.me);
    ASSERT_EQ(nlp.inequal_cons_, chain.mi);
    SpMatRM kkt(nlp.kkt_dim_, nlp.kkt_dim_);
    nlp.analyze_sparsity(kkt);
    Eigen::Map<Vx>(kkt.valuePtr(), kkt.nonZeros()).setZero();

    const Vx x = test_point(chain.n, seed);
    const Vx le = test_point(chain.me, seed + 1), li = test_point(chain.mi, seed + 2);
    const double sigma = 0.7;
    double val = 0.0;
    Vx PGX = Vx::Zero(nlp.primal_vars_), AGX = Vx::Zero(nlp.primal_vars_);
    Vx FXE = Vx::Zero(nlp.equal_cons_), FXI = Vx::Zero(nlp.inequal_cons_);
    nlp.eval_kkt(sigma, x, le, li, val, PGX, AGX, FXE, FXI, kkt);

    const auto ref = chain.evaluate(x, sigma, le, li);
    constexpr double kTol = 1e-12;
    EXPECT_NEAR(val, sigma * ref.f, kTol * std::max(1.0, std::abs(ref.f)));
    EXPECT_LE((PGX - sigma * ref.grad).lpNorm<Eigen::Infinity>(), kTol);
    EXPECT_LE((FXE - ref.ce).lpNorm<Eigen::Infinity>(), kTol);
    EXPECT_LE((FXI - ref.ci).lpNorm<Eigen::Infinity>(), kTol);
    EXPECT_LE((AGX - (ref.je.transpose() * le + ref.ji.transpose() * li)).lpNorm<Eigen::Infinity>(),
              kTol);

    // The KKT matrix holds the upper triangle: the Hessian block at (i, j), i <= j
    // < n, and a Jacobian entry (row r, variable c) at (c, offset + r).
    const int eq_offset = nlp.reduced_primal_vars_count_ + nlp.slack_vars_;
    const int iq_offset = eq_offset + nlp.equal_cons_;
    Eigen::MatrixXd hk = Eigen::MatrixXd::Zero(chain.n, chain.n);
    Eigen::MatrixXd jek = Eigen::MatrixXd::Zero(chain.me, chain.n);
    Eigen::MatrixXd jik = Eigen::MatrixXd::Zero(chain.mi, chain.n);
    for (int row = 0; row < kkt.outerSize(); row++) {
        for (SpMatRM::InnerIterator it(kkt, row); it; ++it) {
            const int col = static_cast<int>(it.col());
            if (row >= chain.n) {
                continue; // solver blocks: slacks, pivots
            }
            if (col < chain.n) {
                hk(row, col) = it.value();
            } else if (col >= eq_offset && col < iq_offset) {
                jek(col - eq_offset, row) = it.value();
            } else if (col >= iq_offset) {
                jik(col - iq_offset, row) = it.value();
            }
        }
    }
    const Eigen::MatrixXd upper_ref = ref.hess.triangularView<Eigen::Upper>();
    EXPECT_LE((hk - upper_ref).lpNorm<Eigen::Infinity>(), kTol);
    EXPECT_LE((jek - ref.je).lpNorm<Eigen::Infinity>(), kTol);
    EXPECT_LE((jik - ref.ji).lpNorm<Eigen::Infinity>(), kTol);
}

TEST(PiecesLocalBlocks, AssemblyMatchesTheDenseReferenceOnOnePartition) {
    const TestChain chain(6);
    Problem problem(chain.declaration(1));
    EXPECT_EQ(problem.partitions(), 1);
    for (unsigned seed : {1u, 4u, 9u}) {
        SCOPED_TRACE(seed);
        expect_program_matches(problem, chain, seed);
    }
}

// Several partitions: the problem is large enough that the layout adopts more
// than one, so the scatter runs concurrently under the lock protocol.
TEST(PiecesLocalBlocks, AssemblyMatchesTheDenseReferenceOnSeveralPartitions) {
    const TestChain chain(400);
    Problem problem(chain.declaration(3));
    ProblemAccess::ipm_program(problem);
    EXPECT_GT(problem.partitions(), 1);
    for (unsigned seed : {2u, 7u}) {
        SCOPED_TRACE(seed);
        expect_program_matches(problem, chain, seed);
    }
}

// The builder's derived counts: the equality block's row space is 0..me-1 with
// the nudge rows claimed twice (overcount = steps - 1), the inequality block's
// 0..mi-1 with no overcount.
TEST(PiecesLocalBlocks, TheBuilderDerivesTheRowCounts) {
    const TestChain chain(6);
    const AssemblyDeclaration d = chain.declaration(1);
    EXPECT_EQ(d.primal_vars_, chain.n);
    EXPECT_EQ(d.equality_rows_, chain.me);
    EXPECT_EQ(d.inequality_rows_, chain.mi);
    EXPECT_EQ(d.equality_shared_row_overcount_, chain.steps - 1);
    EXPECT_EQ(d.inequality_shared_row_overcount_, 0);
    EXPECT_EQ(d.partition_count_, 1);
    EXPECT_EQ(d.variable_bounds_.size(), static_cast<std::size_t>(chain.steps));
    EXPECT_NO_THROW(d.validate());
    Problem problem{chain.declaration(1)};
    EXPECT_EQ(problem.num_variables(), chain.n);
    EXPECT_EQ(problem.num_constraints(), chain.me + chain.mi);
    EXPECT_EQ(problem.statistics().equality_rows, chain.me);
    EXPECT_EQ(problem.statistics().upper_bounded_rows, chain.mi);
    EXPECT_EQ(problem.statistics().boxed_variables, chain.steps);
}

// A large block needs no pattern: the dense default (every Hessian entry of a
// 40-input block's lower triangle, 820 slots) is generated, not checked, so it
// costs no compile-time budget, and it assembles correctly.
// f(v) = sum_i (v_i - c_i)^2 + w (sum_i v_i)^2, minimized at v = c - w S 1 with
// S = sum(c) / (1 + 40 w).
struct CoupledSquares {
    static constexpr Index kInputs = 40;
    double w;
    static double target(Index i) { return 0.1 * static_cast<double>(i) - 1.0; }
    double value(ConstVecRef v) const {
        double f = 0.0;
        for (Index i = 0; i < kInputs; i++) {
            f += (v[i] - target(i)) * (v[i] - target(i));
        }
        const double sum = v.sum();
        return f + w * sum * sum;
    }
    void gradient(ConstVecRef v, VecRef g) const {
        const double sum = v.sum();
        for (Index i = 0; i < kInputs; i++) {
            g[i] = 2.0 * (v[i] - target(i)) + 2.0 * w * sum;
        }
    }
    void hessian(ConstVecRef, MatRef H) const {
        H.setConstant(2.0 * w);
        H.diagonal().array() += 2.0;
    }
};

TEST(PiecesLocalBlocks, ALargeDenseBlockNeedsNoPattern) {
    constexpr double kW = 0.25;
    AssemblyDeclaration d(CoupledSquares::kInputs);
    Mi all(CoupledSquares::kInputs, 1);
    for (Index i = 0; i < CoupledSquares::kInputs; i++) {
        all(i, 0) = static_cast<int>(i);
    }
    d.add_objective(CoupledSquares{kW}, all);
    Problem problem(std::move(d));
    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(problem, Vx::Zero(CoupledSquares::kInputs));
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    double sum_c = 0.0;
    for (Index i = 0; i < CoupledSquares::kInputs; i++) {
        sum_c += CoupledSquares::target(i);
    }
    const double s = sum_c / (1.0 + CoupledSquares::kInputs * kW);
    for (Index i = 0; i < CoupledSquares::kInputs; i++) {
        EXPECT_NEAR(r.x[i], CoupledSquares::target(i) - kW * s, 1e-7) << i;
    }
    EXPECT_EQ(ProblemAccess::ipm_program(problem).num_user_kkt_elems_, 40 * 41 / 2);
}

// ---------------------------------------------------------------------------
// The same problem as an Nlp: dense callbacks over the reference.
// ---------------------------------------------------------------------------

struct TestChainNlp final : hven::solvers::Nlp {
    TestChain chain;
    explicit TestChainNlp(int steps) : chain(steps) {}

    hven::solvers::NlpDimensions dimensions() const override {
        const Index m = chain.me + chain.mi;
        return {.n = chain.n,
                .m = m,
                .jacobian_nonzeros = m * chain.n,
                .hessian_nonzeros = chain.n * (chain.n + 1) / 2};
    }
    void bounds(VecRef xl, VecRef xu, VecRef gl, VecRef gu) const override {
        xl.setConstant(-kInf);
        xu.setConstant(kInf);
        for (int i = 0; i < chain.steps; i++) {
            xl[chain.u0 + i] = -2.0;
            xu[chain.u0 + i] = 2.0;
        }
        gl.head(chain.me).setZero();
        gu.head(chain.me).setZero();
        gl.tail(chain.mi).setConstant(-kInf);
        gu.tail(chain.mi).setZero();
    }
    Vx zeros_e() const { return Vx::Zero(chain.me); }
    Vx zeros_i() const { return Vx::Zero(chain.mi); }
    double eval_f(ConstVecRef x) const override {
        return chain.evaluate(x, 1.0, zeros_e(), zeros_i()).f;
    }
    void eval_grad_f(ConstVecRef x, VecRef g) const override {
        g = chain.evaluate(x, 1.0, zeros_e(), zeros_i()).grad;
    }
    void eval_g(ConstVecRef x, VecRef g) const override {
        const auto e = chain.evaluate(x, 1.0, zeros_e(), zeros_i());
        g << e.ce, e.ci;
    }
    void jacobian_structure(hven::IndexVecRef rows, hven::IndexVecRef cols) const override {
        Index k = 0;
        for (Index r = 0; r < chain.me + chain.mi; r++) {
            for (Index c = 0; c < chain.n; c++, k++) {
                rows[k] = r;
                cols[k] = c;
            }
        }
    }
    void eval_jacobian(ConstVecRef x, VecRef values) const override {
        const auto e = chain.evaluate(x, 1.0, zeros_e(), zeros_i());
        Index k = 0;
        for (Index r = 0; r < chain.me + chain.mi; r++) {
            for (Index c = 0; c < chain.n; c++, k++) {
                values[k] = r < chain.me ? e.je(r, c) : e.ji(r - chain.me, c);
            }
        }
    }
    void hessian_structure(hven::IndexVecRef rows, hven::IndexVecRef cols) const override {
        Index k = 0;
        for (Index r = 0; r < chain.n; r++) {
            for (Index c = 0; c <= r; c++, k++) {
                rows[k] = r;
                cols[k] = c;
            }
        }
    }
    void eval_hessian(ConstVecRef x, double obj_factor, ConstVecRef lambda,
                      VecRef values) const override {
        const auto e = chain.evaluate(x, obj_factor, lambda.head(chain.me), lambda.tail(chain.mi));
        Index k = 0;
        for (Index r = 0; r < chain.n; r++) {
            for (Index c = 0; c <= r; c++, k++) {
                values[k] = e.hess(r, c);
            }
        }
    }
    std::string name() const override { return "test chain (dense Nlp)"; }
};

Vx test_chain_start(const TestChain &chain) {
    Vx x0 = Vx::Zero(chain.n);
    for (int i = 0; i <= chain.steps; i++) {
        x0[i] = 0.5 * i / chain.steps;
    }
    return x0;
}

// The same solve: a pieces problem and the equivalent Nlp reach the same status,
// point, objective and row-space multipliers. (Their KKT layouts differ -- the
// Nlp's is dense -- so iteration counts are not asserted equal.)
TEST(PiecesLocalBlocks, APiecesProblemSolvesAsTheEquivalentNlpDoes) {
    constexpr int kSteps = 12;
    const TestChain chain(kSteps);
    Problem pieces(chain.declaration(1));
    Problem nlp(std::make_shared<TestChainNlp>(kSteps));
    IpmSolver a = quiet_ipm(), b = quiet_ipm();
    const IpmResult ra = a.solve(pieces, test_chain_start(chain));
    const IpmResult rb = b.solve(nlp, test_chain_start(chain));
    ASSERT_EQ(ra.status, SolveStatus::kOptimal);
    ASSERT_EQ(rb.status, SolveStatus::kOptimal);
    EXPECT_NEAR(ra.f, rb.f, 1e-8 * std::max(1.0, std::abs(rb.f)));
    EXPECT_LE((ra.x - rb.x).lpNorm<Eigen::Infinity>(), 1e-6);
    ASSERT_EQ(ra.lambda.size(), rb.lambda.size());
    EXPECT_LE((ra.lambda - rb.lambda).lpNorm<Eigen::Infinity>(), 1e-5);
    ASSERT_EQ(ra.g.size(), rb.g.size());
    EXPECT_LE((ra.g - rb.g).lpNorm<Eigen::Infinity>(), 1e-6);
}

// A pieces problem's row space: lambda and g are [equality rows | inequality
// rows], g is [c_E(x); c_I(x)] at the returned point, and the multipliers are
// the Lagrangian's (L = f + lambda^T [c_E; c_I] - z^T x): stationarity holds.
// A fixed variable (eliminated under the default treatment) is in the problem
// too, so its -1 slots are taken.
TEST(PiecesLocalBlocks, ARowSpaceResultOnAPiecesProblem) {
    const TestChain chain(12);
    AssemblyDeclaration d = chain.declaration(1);
    d.set_bounds(chain.u0 + 3, 0.1, 0.1); // fixes u_3
    Problem problem(std::move(d));
    IpmSolver ipm = quiet_ipm();
    const IpmResult r = ipm.solve(problem, test_chain_start(chain));
    ASSERT_EQ(r.status, SolveStatus::kOptimal);
    EXPECT_EQ(r.x[chain.u0 + 3], 0.1);
    ASSERT_EQ(r.lambda.size(), chain.me + chain.mi);
    ASSERT_EQ(r.g.size(), chain.me + chain.mi);
    const Vx le = r.lambda.head(chain.me), li = r.lambda.tail(chain.mi);
    const auto e = chain.evaluate(r.x, 1.0, le, li);
    EXPECT_LE((r.g.head(chain.me) - e.ce).lpNorm<Eigen::Infinity>(), 1e-9);
    EXPECT_LE((r.g.tail(chain.mi) - e.ci).lpNorm<Eigen::Infinity>(), 1e-9);
    EXPECT_GE(li.minCoeff(), -1e-8); // c_I(x) <= 0 rows carry lambda >= 0
    Vx stationarity = e.grad + e.je.transpose() * le + e.ji.transpose() * li - r.z;
    stationarity[chain.u0 + 3] = 0.0; // the eliminated coordinate
    EXPECT_LE(stationarity.lpNorm<Eigen::Infinity>(), 1e-6);
}

// A warm start round trip on a pieces problem: the result's warm start re-solves
// the same Problem to the same point, and a seed built in the problem's row space
// from the result is accepted the same way.
TEST(PiecesLocalBlocks, AWarmStartRoundTripsOnAPiecesProblem) {
    const TestChain chain(12);
    Problem problem(chain.declaration(1));
    IpmSolver ipm = quiet_ipm();
    const IpmResult cold = ipm.solve(problem, test_chain_start(chain));
    ASSERT_EQ(cold.status, SolveStatus::kOptimal);
    ASSERT_FALSE(cold.warm_start.empty());

    const IpmResult warm = ipm.solve(problem, cold.x, cold.warm_start);
    ASSERT_EQ(warm.status, SolveStatus::kOptimal);
    EXPECT_LE((warm.x - cold.x).lpNorm<Eigen::Infinity>(), 1e-5);
    EXPECT_LT(warm.iterations, cold.iterations);

    const auto seed = hven::solvers::WarmStart::seed(problem, cold.x, cold.lambda, cold.z);
    const IpmResult seeded = ipm.solve(problem, cold.x, seed);
    ASSERT_EQ(seeded.status, SolveStatus::kOptimal);
    EXPECT_LE((seeded.x - cold.x).lpNorm<Eigen::Infinity>(), 1e-5);
    EXPECT_LE((seed.lambda(problem) - cold.lambda).lpNorm<Eigen::Infinity>(), 0.0);
}

// ---------------------------------------------------------------------------
// The evaluation-failure contract on a piece (core/errors.h).
// ---------------------------------------------------------------------------

enum class PieceFault { kEvaluationError, kNaN, kForeign };

/// The chain's defect, failing once at the first point whose control input
/// differs from the start's (u = 0): a trial point. Shared state so the copies
/// the layout makes (partitions) fire once between them.
struct FaultyDefect {
    double h;
    PieceFault fault;
    std::shared_ptr<std::atomic<int>> fired;
    static constexpr Index kInputs = 3, kOutputs = 1;
    static constexpr std::array<BlockEntry, 1> kHessianPattern{{{0, 0}}};
    void value(ConstVecRef v, VecRef out) const {
        out[0] = v[1] - v[0] - h * (v[2] - v[0] * v[0]);
        if (v[2] != 0.0 && fired->exchange(1) == 0) {
            switch (fault) {
            case PieceFault::kEvaluationError:
                throw hven::solvers::EvaluationError("defect: cannot evaluate here");
            case PieceFault::kNaN:
                out[0] = std::numeric_limits<double>::quiet_NaN();
                break;
            case PieceFault::kForeign:
                throw std::runtime_error("defect: a foreign failure");
            }
        }
    }
    void jacobian(ConstVecRef v, MatRef J) const { J << -1.0 + 2.0 * h * v[0], 1.0, -h; }
    void hessian(ConstVecRef, ConstVecRef lambda, MatRef H) const { H(0, 0) = 2.0 * h * lambda[0]; }
};

Problem faulty_chain(int steps, int partitions, PieceFault fault,
                     const std::shared_ptr<std::atomic<int>> &fired) {
    const double h = 1.0 / steps;
    const ChainMaps m = chain_maps(steps);
    AssemblyDeclaration d(m.n);
    d.add_objective(local::Quadratic{h, 0.0}, m.u_idx);
    d.add_objective(local::Quadratic{h, 0.3}, m.y_idx);
    d.add_equality(FaultyDefect{h, fault, fired}, m.defect_vars, m.defect_rows);
    d.add_equality(local::Boundary{0.0, 0.5}, m.boundary_vars, m.boundary_rows);
    d.set_bounds(m.u0, m.u0 + steps - 1, -2.0, 2.0);
    d.set_partitions(partitions);
    return Problem(std::move(d));
}

// An EvaluationError, and a NaN, thrown or produced by a piece at a trial point is
// a rejected trial: the solve recovers to the fault-free answer and counts it. On
// one partition and on several (the fault then surfaces on a worker thread).
TEST(PiecesEvaluationFailure, ARejectedTrialOnAPiece) {
    constexpr int kSteps = 500;
    Problem reference = local_chain(kSteps, 1);
    IpmSolver ref_ipm = quiet_ipm();
    const IpmResult clean = ref_ipm.solve(reference, chain_start(kSteps));
    ASSERT_EQ(clean.status, SolveStatus::kOptimal);
    for (const PieceFault fault : {PieceFault::kEvaluationError, PieceFault::kNaN}) {
        for (const int partitions : {1, 4}) {
            SCOPED_TRACE(::testing::Message() << static_cast<int>(fault) << "/" << partitions);
            const auto fired = std::make_shared<std::atomic<int>>(0);
            Problem problem = faulty_chain(kSteps, partitions, fault, fired);
            IpmSolver ipm = quiet_ipm();
            const IpmResult r = ipm.solve(problem, chain_start(kSteps));
            EXPECT_EQ(fired->load(), 1);
            ASSERT_EQ(r.status, SolveStatus::kOptimal);
            EXPECT_GE(r.evaluation_errors.count, 1);
            EXPECT_LE((r.x - clean.x).lpNorm<Eigen::Infinity>(), 1e-6);
        }
    }
}

// Any other exception a piece throws propagates with its own type, and
// last_result() then holds the interrupted solve.
TEST(PiecesEvaluationFailure, AForeignExceptionFromAPiecePropagates) {
    constexpr int kSteps = 500;
    for (const int partitions : {1, 4}) {
        SCOPED_TRACE(partitions);
        const auto fired = std::make_shared<std::atomic<int>>(0);
        Problem problem = faulty_chain(kSteps, partitions, PieceFault::kForeign, fired);
        IpmSolver ipm = quiet_ipm();
        EXPECT_THROW((void)ipm.solve(problem, chain_start(kSteps)), std::runtime_error);
        EXPECT_EQ(fired->load(), 1);
        EXPECT_EQ(ipm.last_result().status, SolveStatus::kInterrupted);
    }
}

// ---------------------------------------------------------------------------
// The SQP refuses a pieces problem by name.
// ---------------------------------------------------------------------------

TEST(PiecesProblem, TheSqpRefusesAPiecesProblemByName) {
    Problem problem = local_chain(20, 1);
    hven::solvers::SqpSolver sqp{hven::solvers::SqpOptions{}};
    try {
        (void)sqp.solve(problem, chain_start(20));
        FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        EXPECT_NE(message.find("built from pieces"), std::string::npos) << message;
        EXPECT_NE(message.find("solve it with IpmSolver"), std::string::npos) << message;
        EXPECT_NE(message.find("state the problem as an Nlp"), std::string::npos) << message;
    }
}

// ---------------------------------------------------------------------------
// The builder's refusals, each by name.
// ---------------------------------------------------------------------------

/// Runs @p f and expects std::invalid_argument whose message contains every
/// one of @p fragments.
template <class F> void expect_refusal(F &&f, std::initializer_list<const char *> fragments) {
    try {
        f();
        ADD_FAILURE() << "expected std::invalid_argument";
    } catch (const std::invalid_argument &e) {
        const std::string message(e.what());
        for (const char *fragment : fragments) {
            EXPECT_NE(message.find(fragment), std::string::npos)
                << "missing '" << fragment << "' in: " << message;
        }
    }
}

TEST(PiecesBuilder, RefusesAMapOfTheWrongShape) {
    AssemblyDeclaration d(5);
    expect_refusal([&] { d.add_objective(local::Quadratic{1.0, 0.0}, Mi::Zero(2, 3)); },
                   {"add_objective", "local objective block", "1 inputs", "variable map has 2"});
    expect_refusal([&] { d.add_equality(local::Defect{0.1}, Mi::Zero(2, 1), Mi::Zero(1, 1)); },
                   {"add_equality", "'defect'", "3 inputs", "variable map has 2"});
    expect_refusal([&] { d.add_equality(local::Defect{0.1}, Mi::Zero(3, 1), Mi::Zero(2, 1)); },
                   {"add_equality", "1 outputs", "row map has 2"});
    expect_refusal([&] { d.add_inequality(local::Defect{0.1}, Mi::Zero(3, 2), Mi::Zero(1, 3)); },
                   {"add_inequality", "2 applications in its variable map", "3 in its row map"});
    expect_refusal([&] { d.add_objective(local::Quadratic{1.0, 0.0}, Mi::Zero(1, 0)); },
                   {"add_objective", "no column", "names no application"});
    EXPECT_TRUE(d.objectives_.empty());
    EXPECT_TRUE(d.equality_constraints_.empty());
    EXPECT_TRUE(d.inequality_constraints_.empty());
}

TEST(PiecesBuilder, RefusesAnIndexOutsideTheDeclaration) {
    AssemblyDeclaration d(5);
    expect_refusal(
        [&] { d.add_objective(local::Quadratic{1.0, 0.0}, (Mi(1, 2) << 0, 5).finished()); },
        {"add_objective", "application 1", "variable 5", "has 5 variables"});
    expect_refusal(
        [&] {
            d.add_equality(local::Defect{0.1}, (Mi(3, 1) << 0, -1, 2).finished(), Mi::Zero(1, 1));
        },
        {"add_equality", "variable -1", "input 1"});
    expect_refusal(
        [&] {
            d.add_equality(local::Defect{0.1}, (Mi(3, 1) << 0, 1, 2).finished(),
                           (Mi(1, 1) << -2).finished());
        },
        {"add_equality", "row -2", "non-negative"});
}

// A row space derived by the builder has a piece behind every row: a gap is
// refused when the problem is built, naming the row.
TEST(PiecesBuilder, RefusesARowNoApplicationNames) {
    AssemblyDeclaration d(4);
    d.add_equality(Ends{0.0, 1.0}, (Mi(2, 1) << 0, 1).finished(), (Mi(2, 1) << 0, 2).finished());
    EXPECT_EQ(d.equality_rows_, 3);
    expect_refusal([&] { d.validate(); }, {"equality row 1", "named by no application"});
    expect_refusal([&] { Problem p{AssemblyDeclaration(d)}; }, {"equality row 1"});
}

TEST(PiecesBuilder, RefusesBadBoundsAndPartitionCounts) {
    AssemblyDeclaration d(4);
    expect_refusal([&] { d.set_bounds(1, 2.0, 1.0); }, {"set_bounds", "variable 1", "inverted"});
    expect_refusal([&] { d.set_bounds(0, std::nan(""), 1.0); }, {"set_bounds", "not a number"});
    expect_refusal([&] { d.set_bounds(2, 4, -1.0, 1.0); }, {"set_bounds", "2..4", "4 variables"});
    expect_refusal([&] { d.set_bounds(3, 2, -1.0, 1.0); }, {"set_bounds", "3..2", "empty"});
    expect_refusal([&] { d.set_bounds(-1, -1.0, 1.0); }, {"set_bounds", "-1..-1"});
    EXPECT_TRUE(d.variable_bounds_.empty());
    expect_refusal([&] { d.set_partitions(0); }, {"set_partitions", "0 partitions"});
    expect_refusal([&] { AssemblyDeclaration bad(-1); }, {"-1 variables"});
    d.set_bounds(0, 3, -kInf, 2.0);
    EXPECT_EQ(d.variable_bounds_.size(), 4u);
    d.set_partitions(3);
    EXPECT_EQ(d.partition_count_, 3);
}

// A raw-protocol piece goes through the builder too, and its counts are derived
// the same way.
TEST(PiecesBuilder, TakesRawProtocolPieces) {
    const double h = 0.1;
    AssemblyDeclaration d(5);
    d.add_objective(raw::QuadraticTerm{"effort", 1.0, 0.0}, (Mi(1, 2) << 3, 4).finished());
    d.add_equality(raw::Defect{h}, (Mi(3, 2) << 0, 1, 1, 2, 3, 4).finished(),
                   (Mi(1, 2) << 0, 1).finished());
    EXPECT_EQ(d.equality_rows_, 2);
    EXPECT_EQ(d.equality_shared_row_overcount_, 0);
    EXPECT_NO_THROW(d.validate());
}

} // namespace pieces_test
