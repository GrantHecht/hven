// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// Example 4: a sparse, partitioned problem built from PIECES.
//
// This is the "provider" way into the interior-point engine: instead of one
// triplet model, the problem is declared as several functions (pieces), each
// applied many times over index maps, and each writing its own derivatives
// straight into the KKT matrix.
//
// The problem is a small direct-transcription optimal-control chain, N steps:
//
//     min   sum_i  h*u_i^2  +  h*(y_i - 0.3)^2
//     s.t.  y_{i+1} - y_i - h*(u_i - y_i^2) = 0,   i = 0..N-1   (dynamics)
//           y_0 = 0,  y_N = 0.5                                  (boundary)
//           -2 <= u_i <= 2
//
// with variables z = (y_0..y_N, u_0..u_{N-1}). Four pieces:
//     ControlEffort  (objective, N applications of a 1-input function)
//     Tracking       (objective, N applications of a 1-input function)
//     Defect         (equality,  N applications of a 3-input, 1-row function)
//     Boundary       (equality,  1 application of a 2-input, 2-row function)
//
// Each piece answers: its sizes; its evaluations (value, gradient, Jacobian,
// adjoint-Hessian); and where its derivatives go (get_kkt_space claims KKT
// slots, the scatter writes through them under the shared lock protocol).
// The problem is then assembled as an AssemblyDeclaration, adopted by a
// NonLinearProgram and solved by IpmSolver -- once on one partition and once
// asking for four, and the answers must agree.

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "hven/detail/interior/constraint_function.h" // detail/: no public path to ConstraintFunction
#include "hven/detail/interior/objective_function.h" // detail/: ... or ObjectiveFunction
#include "hven/detail/solvers/solver_interface_specs.h" // detail/: ConstraintInterface, ObjectiveInterface
#include "hven/drivers/ipm_solver.h"
#include "hven/model/assembly_declaration.h"
#include "hven/model/non_linear_program.h"
#include "hven/solver_interface_adapter.h"

namespace {

using namespace hven::solvers;
using Vx = Eigen::VectorXd;
using Vi = Eigen::VectorXi;
using SpMatRM = Eigen::SparseMatrix<double, Eigen::RowMajor>;

// Adds `value` into KKT value slot `slot`, taking the per-column mutex when
// another partition may be writing the same slot (KKTClashes[col] != -1).
void kkt_add(SpMatRM &kkt, const Vi &locations, Eigen::Ref<Vi> clashes,
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

// The constraint surface an objective piece must still carry (the KKT sizing
// pass is shared between objectives and constraints), never called.
struct NoConstraintSurface {
    [[noreturn]] static void refuse() {
        throw std::logic_error("objective piece used as constraint");
    }
    void constraints(hven::ConstEigenRef<Vx>, Eigen::Ref<Vx>, const SolverIndexingData &) const {
        refuse();
    }
    void constraints_adjointgradient(hven::ConstEigenRef<Vx>, hven::ConstEigenRef<Vx>,
                                     Eigen::Ref<Vx>, Eigen::Ref<Vx>,
                                     const SolverIndexingData &) const {
        refuse();
    }
    void constraints_jacobian(hven::ConstEigenRef<Vx>, Eigen::Ref<Vx>, SpMatRM &, Eigen::Ref<Vi>,
                              Eigen::Ref<Vi>, std::vector<std::mutex> &,
                              const SolverIndexingData &) const {
        refuse();
    }
    void constraints_jacobian_adjointgradient(hven::ConstEigenRef<Vx>, hven::ConstEigenRef<Vx>,
                                              Eigen::Ref<Vx>, Eigen::Ref<Vx>, SpMatRM &,
                                              Eigen::Ref<Vi>, Eigen::Ref<Vi>,
                                              std::vector<std::mutex> &,
                                              const SolverIndexingData &) const {
        refuse();
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        hven::ConstEigenRef<Vx>, hven::ConstEigenRef<Vx>, Eigen::Ref<Vx>, Eigen::Ref<Vx>, SpMatRM &,
        Eigen::Ref<Vi>, Eigen::Ref<Vi>, std::vector<std::mutex> &,
        const SolverIndexingData &) const {
        refuse();
    }
};

// A scalar objective term  weight * (z - ref)^2  applied to one variable per
// application. ControlEffort is (ref = 0), Tracking is (ref = 0.3).
struct QuadraticTerm : NoConstraintSurface {
    const char *label;
    double weight, ref;
    std::string name() const { return label; }
    int input_rows() const { return 1; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void objective(double scale, hven::ConstEigenRef<Vx> X, double &val,
                   const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double e = X[d.v_loc(0, V)] - ref;
            val += scale * weight * e * e;
        }
    }
    void objective_gradient(double scale, hven::ConstEigenRef<Vx> X, double &val, Eigen::Ref<Vx> GX,
                            const SolverIndexingData &d) const {
        this->objective(scale, X, val, d);
        for (int V = 0; V < d.num_appl(); V++) {
            GX[d.inner_gradient_starts_[V]] = scale * 2.0 * weight * (X[d.v_loc(0, V)] - ref);
        }
    }
    void objective_gradient_hessian(double scale, hven::ConstEigenRef<Vx> X, double &val,
                                    Eigen::Ref<Vx> GX, SpMatRM &kkt, Eigen::Ref<Vi> locations,
                                    Eigen::Ref<Vi> clashes, std::vector<std::mutex> &locks,
                                    const SolverIndexingData &d) const {
        this->objective_gradient(scale, X, val, GX, d);
        for (int V = 0; V < d.num_appl(); V++) {
            const int r = d.v_scatter_loc(0, V);
            if (r < 0) {
                continue; // variable eliminated by the solver: slot claimed, nothing to write
            }
            kkt_add(kkt, locations, clashes, locks, d.inner_kkt_starts_[V],
                    kkt_canonical_lock_col(r, r), scale * 2.0 * weight, true);
        }
    }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free_slot, int /*con_offset*/,
                       bool /*do_jac*/, bool do_hess, SolverIndexingData &d) {
        d.inner_kkt_starts_.resize(d.num_appl());
        for (int V = 0; V < d.num_appl(); V++) {
            d.inner_kkt_starts_[V] = free_slot;
            if (do_hess) {
                rows[free_slot] = cols[free_slot] = d.v_scatter_loc(0, V); // -1 when eliminated
                free_slot++;
            }
        }
    }
    int num_kkt_elements(bool /*do_jac*/, bool do_hess) const { return do_hess ? 1 : 0; }
};

// d = y1 - y0 - h*(u - y0^2): inputs (y_i, y_{i+1}, u_i), one row.
struct Defect {
    double h;
    std::string name() const { return "defect"; }
    int input_rows() const { return 3; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void constraints(hven::ConstEigenRef<Vx> X, Eigen::Ref<Vx> FX,
                     const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double y0 = X[d.v_loc(0, V)], y1 = X[d.v_loc(1, V)], u = X[d.v_loc(2, V)];
            FX[d.inner_constraint_starts_[V]] = y1 - y0 - h * (u - y0 * y0);
        }
    }
    void adjoint_gradient(hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L, Eigen::Ref<Vx> AGX,
                          const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            const double lam = L[d.c_loc(0, V)], y0 = X[d.v_loc(0, V)];
            const int g = d.inner_gradient_starts_[V];
            AGX[g + 0] += lam * (-1.0 + 2.0 * h * y0);
            AGX[g + 1] += lam * 1.0;
            AGX[g + 2] += lam * -h;
        }
    }
    void scatter_jacobian(hven::ConstEigenRef<Vx> X, SpMatRM &kkt, Eigen::Ref<Vi> locations,
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
    void scatter_hessian(hven::ConstEigenRef<Vx> L, SpMatRM &kkt, Eigen::Ref<Vi> locations,
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

    void constraints_adjointgradient(hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L,
                                     Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
                                     const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        this->adjoint_gradient(X, L, AGX, d);
    }
    void constraints_jacobian(hven::ConstEigenRef<Vx> X, Eigen::Ref<Vx> FX, SpMatRM &kkt,
                              Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
                              std::vector<std::mutex> &locks, const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        this->scatter_jacobian(X, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient(hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L,
                                              Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX, SpMatRM &kkt,
                                              Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
                                              std::vector<std::mutex> &locks,
                                              const SolverIndexingData &d) const {
        this->constraints_adjointgradient(X, L, FX, AGX, d);
        this->scatter_jacobian(X, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L, Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
        SpMatRM &kkt, Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
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

// Linear boundary rows: row j = input j - target_j (inputs y_0 and y_N).
struct Boundary {
    double target0, target1;
    std::string name() const { return "boundary"; }
    int input_rows() const { return 2; }
    int output_rows() const { return 2; }
    bool thread_safe() const { return true; }

    void constraints(hven::ConstEigenRef<Vx> X, Eigen::Ref<Vx> FX,
                     const SolverIndexingData &d) const {
        for (int V = 0; V < d.num_appl(); V++) {
            FX[d.inner_constraint_starts_[V] + 0] = X[d.v_loc(0, V)] - target0;
            FX[d.inner_constraint_starts_[V] + 1] = X[d.v_loc(1, V)] - target1;
        }
    }
    void constraints_adjointgradient(hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L,
                                     Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
                                     const SolverIndexingData &d) const {
        this->constraints(X, FX, d);
        for (int V = 0; V < d.num_appl(); V++) {
            AGX[d.inner_gradient_starts_[V] + 0] += L[d.c_loc(0, V)];
            AGX[d.inner_gradient_starts_[V] + 1] += L[d.c_loc(1, V)];
        }
    }
    void constraints_jacobian(hven::ConstEigenRef<Vx> X, Eigen::Ref<Vx> FX, SpMatRM &kkt,
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
    void constraints_jacobian_adjointgradient(hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L,
                                              Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX, SpMatRM &kkt,
                                              Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash,
                                              std::vector<std::mutex> &locks,
                                              const SolverIndexingData &d) const {
        this->constraints_adjointgradient(X, L, FX, AGX, d);
        this->constraints_jacobian(X, FX, kkt, loc, clash, locks, d);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        hven::ConstEigenRef<Vx> X, hven::ConstEigenRef<Vx> L, Eigen::Ref<Vx> FX, Eigen::Ref<Vx> AGX,
        SpMatRM &kkt, Eigen::Ref<Vi> loc, Eigen::Ref<Vi> clash, std::vector<std::mutex> &locks,
        const SolverIndexingData &d) const {
        // Linear rows: no Hessian contribution.
        this->constraints_jacobian_adjointgradient(X, L, FX, AGX, kkt, loc, clash, locks, d);
    }
    void get_kkt_space(Eigen::Ref<Vi> rows, Eigen::Ref<Vi> cols, int &free_slot, int con_offset,
                       bool do_jac, bool /*do_hess*/, SolverIndexingData &d) {
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
    int num_kkt_elements(bool do_jac, bool /*do_hess*/) const { return do_jac ? 2 : 0; }
};

} // namespace

// A function type enters the solver only through an explicit registration,
// placed beside its definition (hven/solver_interface_adapter.h).
template <>
struct hven::solvers::SolverInterfaceAdapter<QuadraticTerm> : DirectFunctionModel<QuadraticTerm> {};
template <> struct hven::solvers::SolverInterfaceAdapter<Defect> : DirectFunctionModel<Defect> {};
template <>
struct hven::solvers::SolverInterfaceAdapter<Boundary> : DirectFunctionModel<Boundary> {};

namespace {

struct ChainSolution {
    SolveStatus status;
    double f;
    double feasibility_e;
    int adopted_partitions;
    long long iterations;
    Vx x;
};

ChainSolution solve_chain(int steps, int requested_partitions) {
    const double h = 1.0 / steps;
    const int n = 2 * steps + 1; // y_0..y_N then u_0..u_{N-1}
    const int u0 = steps + 1;

    // Index maps: column V lists the variables application V reads.
    Eigen::MatrixXi y_idx(1, steps), u_idx(1, steps), defect_v(3, steps), defect_c(1, steps);
    for (int i = 0; i < steps; i++) {
        y_idx(0, i) = i;
        u_idx(0, i) = u0 + i;
        defect_v.col(i) << i, i + 1, u0 + i;
        defect_c(0, i) = i; // constraint row of defect i
    }
    Eigen::MatrixXi boundary_v(2, 1), boundary_c(2, 1);
    boundary_v << 0, steps;
    boundary_c << steps, steps + 1; // rows after the defects

    AssemblyDeclaration declaration;
    declaration.primal_vars_ = n;
    declaration.equality_rows_ = steps + 2;
    declaration.inequality_rows_ = 0;
    declaration.partition_count_ = requested_partitions;
    declaration.objectives_.emplace_back(ObjectiveInterface(QuadraticTerm{{}, "effort", h, 0.0}),
                                         u_idx);
    declaration.objectives_.emplace_back(ObjectiveInterface(QuadraticTerm{{}, "tracking", h, 0.3}),
                                         y_idx);
    declaration.equality_constraints_.emplace_back(ConstraintInterface(Defect{h}), defect_v,
                                                   defect_c);
    declaration.equality_constraints_.emplace_back(ConstraintInterface(Boundary{0.0, 0.5}),
                                                   boundary_v, boundary_c);
    for (int i = 0; i < steps; i++) {
        declaration.variable_bounds_.push_back(VariableBound{u0 + i, -2.0, 2.0});
    }

    NonLinearProgram program(requested_partitions);
    program.adopt_declaration(std::move(declaration));

    IpmOptions options;
    options.common.print_level = 3;
    IpmSolver solver(options);

    Vx x0 = Vx::Zero(n);
    for (int i = 0; i <= steps; i++) {
        x0[i] = 0.5 * i / steps; // straight line from y_0 = 0 to y_N = 0.5
    }
    const IpmResult result = solver.solve(program, x0);

    // Independent check of the pieces: recompute the objective and the
    // dynamics residual from the returned point.
    double f_check = 0.0, worst_defect = 0.0;
    for (int i = 0; i < steps; i++) {
        const double y = result.x[i], yn = result.x[i + 1], u = result.x[u0 + i];
        f_check += h * u * u + h * (y - 0.3) * (y - 0.3);
        worst_defect = std::max(worst_defect, std::abs(yn - y - h * (u - y * y)));
    }
    if (std::abs(f_check - result.f) > 1e-9 || worst_defect > 1e-8) {
        throw std::runtime_error("the pieces disagree with the independent evaluation");
    }
    return {result.status,
            result.f,
            result.feasibility_e,
            program.num_partitions_,
            static_cast<long long>(result.iterations),
            result.x};
}

} // namespace

int main() {
    const int steps = 500;
    const ChainSolution one = solve_chain(steps, 1);
    const ChainSolution many = solve_chain(steps, 4);
    for (const auto &[label, s] :
         {std::pair{"1 partition requested", &one}, std::pair{"4 partitions requested", &many}}) {
        std::printf("%-22s adopted=%d status=%-8s iterations=%3lld f=%.9f |ce|=%.1e\n", label,
                    s->adopted_partitions, to_string(s->status), s->iterations, s->f,
                    s->feasibility_e);
    }
    const bool ok = one.status == SolveStatus::kOptimal && many.status == SolveStatus::kOptimal &&
                    many.adopted_partitions > 1 && std::abs(one.f - many.f) < 1e-8 &&
                    (one.x - many.x).lpNorm<Eigen::Infinity>() < 1e-6 && one.feasibility_e < 1e-6 &&
                    many.feasibility_e < 1e-6;
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
