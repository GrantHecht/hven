// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// A user piece with a std::string member, declared and solved through the
// public assembly path (the pattern of examples/04_partitioned_chain.cpp).
// The erased handle used to relocate such a piece with memcpy, which broke
// the string's small-string buffer and corrupted the heap.

#include <gtest/gtest.h>

#include <cmath>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/objective_function.h"
#include "hven/detail/model/non_linear_program.h"
#include "hven/detail/model/problem_access.h"
#include "hven/detail/solvers/solver_interface_specs.h"
#include "hven/drivers/ipm_solver.h"
#include "hven/drivers/solve_status.h"
#include "hven/model/assembly_declaration.h"
#include "hven/solver_interface_adapter.h"

namespace string_piece_test {

using namespace hven;
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

// Constraint surface an objective piece must still carry; never called.
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

// weight * (z - ref)^2 on one variable per application, labelled by a std::string.
struct StringLabelledQuadratic : NoConstraintSurface {
    std::string label; // SSO for a short label: not trivially relocatable
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

} // namespace string_piece_test

template <>
struct hven::solvers::SolverInterfaceAdapter<string_piece_test::StringLabelledQuadratic>
    : DirectFunctionModel<string_piece_test::StringLabelledQuadratic> {};

namespace string_piece_test {

TEST(StringPiece, DeclaredAndSolvedThroughTheAssemblyPath) {
    const int n = 2;
    Eigen::MatrixXi x0_idx(1, 1), x1_idx(1, 1);
    x0_idx(0, 0) = 0;
    x1_idx(0, 0) = 1;

    AssemblyDeclaration declaration;
    declaration.primal_vars_ = n;
    declaration.equality_rows_ = 0;
    declaration.inequality_rows_ = 0;
    declaration.partition_count_ = 1;
    declaration.objectives_.emplace_back(
        ObjectiveInterface(StringLabelledQuadratic{{}, "q0", 1.0, 0.25}), x0_idx);
    declaration.objectives_.emplace_back(
        ObjectiveInterface(StringLabelledQuadratic{
            {}, "a label long enough to defeat the small-string optimisation", 2.0, -0.5}),
        x1_idx);
    declaration.variable_bounds_.push_back(VariableBound{0, -1.0, 1.0});
    declaration.variable_bounds_.push_back(VariableBound{1, -1.0, 1.0});

    NonLinearProgram program(1);
    program.adopt_declaration(std::move(declaration));

    IpmOptions options;
    options.common.print_level = 0;
    IpmSolver solver(options);
    const IpmResult result = solver.solve(hven::solvers::detail::problem_ref(program), Vx::Zero(n));

    EXPECT_EQ(result.status, SolveStatus::kOptimal);
    EXPECT_NEAR(result.x[0], 0.25, 1e-6);
    EXPECT_NEAR(result.x[1], -0.5, 1e-6);
    EXPECT_NEAR(result.f, 0.0, 1e-9);
}

} // namespace string_piece_test
