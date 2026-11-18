// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// Fixture function types for the SolverInterfaceAdapter seam
// (include/hven/model/solver_interface_adapter.h). Shared by the positive test
// (test_solver_interface_adapter.cpp) and the compile-fail probes under
// compile_fail/, so the two agree on exactly which type is registered and
// which is not.
//
// The surfaces here are structural only: enough members for
// ConstraintModel<T> / ObjectiveModel<T> to instantiate, no numerics. What is
// under test is the ROUTING decision, which is settled entirely at compile
// time.

#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/model/piece_interfaces.h"
#include "hven/model/solver_interface_adapter.h"

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
                     const hven::SolverIndexingData &) const {}
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &,
                                     const Eigen::Ref<const Eigen::VectorXd> &,
                                     Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
                                     const hven::SolverIndexingData &) const {}
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &,
                              Eigen::Ref<Eigen::VectorXd>,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                              Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                              std::vector<std::mutex> &, const hven::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::SolverIndexingData &) const {}
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &, const Eigen::Ref<const Eigen::VectorXd> &,
        Eigen::Ref<Eigen::VectorXd>, Eigen::Ref<Eigen::VectorXd>,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &, Eigen::Ref<Eigen::VectorXi>,
        Eigen::Ref<Eigen::VectorXi>, std::vector<std::mutex> &,
        const hven::SolverIndexingData &) const {}
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &, int, bool,
                       bool, hven::SolverIndexingData &) {}
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
                   const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                            Eigen::Ref<Eigen::VectorXd>, const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                                    Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
};

/// Registered for direct storage, but has no objective surface: legal as a
/// constraint, a diagnosed error as an objective.
struct StubConstraintOnly : StubConstraintSurface {};

/// Registered for direct storage with a PARTIAL objective surface: it has
/// objective() but neither objective_gradient nor objective_gradient_hessian.
/// A guard that probed only objective() would wave this through and let
/// ObjectiveModel produce the raw diagnostics for the other two.
struct StubPartialObjective : StubConstraintSurface {
    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                   const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
};

/// Registered by a hand-written adapter that declares the type constraint-only
/// through the ObjectiveUnsupported mixin -- the sanctioned way to register
/// one direction.
struct StubMixinConstraintOnly : StubConstraintSurface {};

/// Registered by a hand-written adapter that declares the type objective-only
/// through the ConstraintUnsupported mixin -- the mirror image of
/// StubMixinConstraintOnly, and the constraint-side half of the seam's
/// half-policy coverage. It carries the scalar objective surface (inherited
/// from StubScalarObjective) because the direction its adapter DOES support
/// has to actually work; what makes it objective-only is the adapter's
/// declaration, not a missing member. That distinction is the point: the
/// refusal on the constraint route must be the mixin's authored answer, not a
/// lookup failure that happens to look like one.
struct StubMixinObjectiveOnly : StubScalarObjective {};

/// Registered by a hand-written adapter that provides install_constraint and
/// simply OMITS install_objective, without the mixin. Specialization lookup
/// does not fall back to the primary template, so only the interface's own
/// backstop can diagnose this one.
struct StubAdapterMissingObjective : StubConstraintSurface {};

/// Never registered anywhere: the default adapter stores it as itself.
struct StubUnregistered : StubConstraintSurface {};

/// An objective with the piece surface and the three scalar members only -- no
/// output count, no constraint surface -- and no registration.
struct StubPlainObjective {
    std::string name() const { return "adapter_fixture_plain_objective"; }
    int input_rows() const { return 1; }
    bool thread_safe() const { return true; }
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>, int &, int, bool,
                       bool, hven::SolverIndexingData &) {}
    int num_kkt_elements(bool, bool) const { return 0; }
    void objective(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                   const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                            Eigen::Ref<Eigen::VectorXd>, const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
    void objective_gradient_hessian(double, const Eigen::Ref<const Eigen::VectorXd> &, double &Val,
                                    Eigen::Ref<Eigen::VectorXd>,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &,
                                    Eigen::Ref<Eigen::VectorXi>, Eigen::Ref<Eigen::VectorXi>,
                                    std::vector<std::mutex> &,
                                    const hven::SolverIndexingData &) const {
        Val = 0.0;
    }
};

/// No piece surface at all, and no registration and no hook: the default
/// adapter refuses it in words.
struct StubNoSurface {
    std::string name() const { return "adapter_fixture_no_surface"; }
};

/// An ERASED HANDLE with the payload-emplacement hooks: it carries a concrete
/// piece behind its own vtable and exposes nothing of the piece surface itself.
/// The default adapter detects the hooks, so what the interface stores is the
/// concrete payload's model and no call ever goes through this handle.
struct StubHandle {
    struct Concept {
        virtual ~Concept() = default;
        virtual void pack_into_constraint_interface(hven::ConstraintInterface &) const = 0;
        virtual void pack_into_objective_interface(hven::ObjectiveInterface &) const = 0;
    };
    template <class T> struct Model final : Concept {
        T payload;
        explicit Model(T t) : payload(std::move(t)) {}
        void pack_into_constraint_interface(hven::ConstraintInterface &ci) const override {
            ci.storage.emplace<hven::ConstraintModel<T>>(payload);
        }
        void pack_into_objective_interface(hven::ObjectiveInterface &oi) const override {
            if constexpr (hven::SolverObjectiveFunction<T>) {
                oi.storage.emplace<hven::ObjectiveModel<T>>(payload);
            } else {
                throw std::logic_error("StubHandle: this payload is not an objective");
            }
        }
    };
    std::shared_ptr<const Concept> impl;

    void pack_into_constraint_interface(hven::ConstraintInterface &ci) const {
        impl->pack_into_constraint_interface(ci);
    }
    void pack_into_objective_interface(hven::ObjectiveInterface &oi) const {
        impl->pack_into_objective_interface(oi);
    }
};

/// A handle with the constraint hook only: it cannot be an objective.
struct StubConstraintHookOnly {
    StubConstraintOnly payload;
    void pack_into_constraint_interface(hven::ConstraintInterface &ci) const {
        ci.storage.emplace<hven::ConstraintModel<StubConstraintOnly>>(payload);
    }
};

} // namespace adapter_fixture

// Registrations live beside the definitions, per the placement rule in
// hven/model/solver_interface_adapter.h.
namespace hven {

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

template <>
struct SolverInterfaceAdapter<adapter_fixture::StubPartialObjective>
    : DirectFunctionModel<adapter_fixture::StubPartialObjective> {};

// The composition pattern the mixin exists for: one install written out, the
// other inherited from the mixin that refuses it in words. Note that the two
// cannot simply be multiply-inherited -- DirectFunctionModel supplies an
// install_objective of its own, so inheriting both would make the name
// ambiguous, which is a raw error and exactly what the mixin is here to avoid.
template <>
struct SolverInterfaceAdapter<adapter_fixture::StubMixinConstraintOnly>
    : ObjectiveUnsupported<adapter_fixture::StubMixinConstraintOnly> {
    static void install_constraint(const adapter_fixture::StubMixinConstraintOnly &t,
                                   ConstraintInterface &ci) {
        DirectFunctionModel<adapter_fixture::StubMixinConstraintOnly>::install_constraint(t, ci);
    }
};

// The same composition pattern in the other direction: install_objective
// written out, install_constraint inherited from the mixin that refuses it in
// words. Multiple inheritance is unavailable here for the same reason as above
// -- DirectFunctionModel supplies an install_constraint of its own, so
// inheriting both would make the name ambiguous.
template <>
struct SolverInterfaceAdapter<adapter_fixture::StubMixinObjectiveOnly>
    : ConstraintUnsupported<adapter_fixture::StubMixinObjectiveOnly> {
    static void install_objective(const adapter_fixture::StubMixinObjectiveOnly &t,
                                  ObjectiveInterface &oi) {
        DirectFunctionModel<adapter_fixture::StubMixinObjectiveOnly>::install_objective(t, oi);
    }
};

// Deliberately malformed: install_objective is missing and the mixin is not
// inherited. Only the interface constructor's backstop can catch this.
template <> struct SolverInterfaceAdapter<adapter_fixture::StubAdapterMissingObjective> {
    static constexpr bool registered = true;

    static void install_constraint(const adapter_fixture::StubAdapterMissingObjective &t,
                                   ConstraintInterface &ci) {
        DirectFunctionModel<adapter_fixture::StubAdapterMissingObjective>::install_constraint(t,
                                                                                              ci);
    }
};

} // namespace hven
