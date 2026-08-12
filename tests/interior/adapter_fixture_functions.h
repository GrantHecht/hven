#pragma once

// Fixture function types for the SolverInterfaceAdapter seam
// (include/hven/solver_interface_adapter.h). Shared by the positive test
// (test_solver_interface_adapter.cpp) and the compile-fail probes under
// compile_fail/, so the two agree on exactly which type is registered and
// which is not.
//
// The surfaces here are structural only: enough members for
// ConstraintModel<T> / ObjectiveModel<T> to instantiate, no numerics. What is
// under test is the ROUTING decision, which is settled entirely at compile
// time.

#include <mutex>
#include <string>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/detail/interior/solver_interface_specs.h"
#include "hven/solver_interface_adapter.h"

namespace adapter_fixture {

/// The member set ConstraintModel<T> forwards to. Every fixture type below
/// derives from it; none is registered by inheriting it, which is the point --
/// registration is an affirmative act elsewhere.
struct StubConstraintSurface {
    std::string name() const { return "adapter_fixture_stub"; }
    int input_rows() const { return 1; }
    int output_rows() const { return 1; }
    bool thread_safe() const { return true; }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &, Eigen::Ref<Eigen::VectorXd>,
                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &,
                                     const Eigen::Ref<const Eigen::VectorXd> &,
                                     Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                     const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &,
                              Eigen::Ref<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                              std::vector<std::mutex> &,
                              const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::solvers::SolverIndexingData &) const {}
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &, int, bool,
                       bool, hven::solvers::SolverIndexingData &) {}
    int num_kkt_elements(bool, bool) const { return 0; }
};

/// A FAMILY of constraint functions, the shape a consumer registers once for
/// every member of the family. Its adapter below is a CONSTRAINED partial
/// specialization -- the C++20 form the three CI frontends must all accept.
template <int OutRows> struct StubDefectFamily : StubConstraintSurface {
    int output_rows() const { return OutRows; }
};

/// Registered, and carries the scalar objective surface: legal in both
/// interfaces.
struct StubScalarObjective : StubConstraintSurface {
    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                   const hven::solvers::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                            Eigen::Ref<Eigen::VectorXd>,
                            const hven::solvers::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                                    Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const hven::solvers::SolverIndexingData &) const {
        Val = 0.0;
    }
};

/// Registered for direct storage, but has no objective surface: legal as a
/// constraint, a diagnosed error as an objective.
struct StubConstraintOnly : StubConstraintSurface {};

/// Never registered anywhere. Reaching either interface with it is a
/// diagnosed error.
struct StubUnregistered : StubConstraintSurface {};

} // namespace adapter_fixture

// Registrations live beside the definitions, per the placement rule in
// hven/solver_interface_adapter.h.
namespace hven::solvers {

template <int OutRows>
    requires(OutRows > 0)
struct SolverInterfaceAdapter<adapter_fixture::StubDefectFamily<OutRows>>
    : DirectFunctionModel<adapter_fixture::StubDefectFamily<OutRows>> {};

template <>
struct SolverInterfaceAdapter<adapter_fixture::StubScalarObjective>
    : DirectFunctionModel<adapter_fixture::StubScalarObjective> {};

template <>
struct SolverInterfaceAdapter<adapter_fixture::StubConstraintOnly>
    : DirectFunctionModel<adapter_fixture::StubConstraintOnly> {};

} // namespace hven::solvers
