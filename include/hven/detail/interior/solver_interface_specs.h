// =============================================================================
// Originally from ASSET (AlabamaASRL/asset_asrl)
// Copyright 2020-present The University of Alabama-Astrodynamics and Space
//   Research Lab. Licensed under the Apache License, Version 2.0
// License: notices/asset-apache2.txt.
// Source: https://github.com/AlabamaASRL/asset_asrl
// Original Developer: James B. Pezent
//
// Defines the type erasure specs (SolverConstraintSpec, SolverObjectiveSpec)
// and concrete type erasure (ConstraintInterface, ObjectiveInterface) that
// enable vector functions to interface with PSIOPT and NonLinearProgram.
//
// Modifications in Tycho (Copyright 2026-present Grant R. Hecht,
//   Apache 2.0 — see LICENSE.txt):
//   - Namespace renamed: asset -> tycho (with sub-namespaces tycho::vf, tycho::oc, etc.)
//   - Python binding methods moved to src/bindings/ (nanobind)
//   - PR 9: Replaced rubber_types with TypeStorage; deleted dead
//     Model<>/ExternalInterface<> boilerplate and SolverInterfaceSelector.
//   - Function entry routed through hven/solver_interface_adapter.h: the
//     consumer-type forward declaration and its preferred constructors are
//     gone; both interfaces now delegate to SolverInterfaceAdapter<T>, so
//     what gets stored is declared by the type's owner rather than inferred
//     here.
// =============================================================================

#pragma once

#include "hven/detail/interior/indexing_data.h"
#include "hven/detail/interior/sizing_specs.h"
#include "hven/solver_interface_adapter.h"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Geometry>
#include <Eigen/Sparse>

#include "hven/detail/interior/typedefs/eigen_types.h"
#include "hven/detail/interior/utils/flat_map.h"
#include "hven/detail/interior/utils/function_return_type.h"
#include "hven/detail/interior/utils/get_core_count.h"
#include "hven/detail/interior/utils/math_functions.h"
#include "hven/detail/interior/utils/sizing_helpers.h"
#include "hven/detail/interior/utils/std_extensions.h"
#include "hven/detail/interior/utils/thread_pool.h"
#include "hven/detail/interior/utils/type_name.h"
#include "hven/detail/interior/utils/type_storage.h"

namespace hven::solvers {

// Import cross-namespace types used throughout the solver layer.
using utils::TypeStorage;

/*
 * Spec for vector function that can be used as a constraint inside of PSIOPT.
 */
struct SolverConstraintSpec {
    struct Concept {
        virtual ~Concept() = default;

        virtual void constraints(const Eigen::Ref<const Eigen::VectorXd> &X,
                                 Eigen::Ref<Eigen::VectorXd> FX,
                                 const SolverIndexingData &data) const = 0;

        virtual void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                                 const Eigen::Ref<const Eigen::VectorXd> &L,
                                                 Eigen::Ref<Eigen::VectorXd> FX,
                                                 Eigen::Ref<Eigen::VectorXd> AGX,
                                                 const SolverIndexingData &data) const = 0;

        virtual void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                                          Eigen::Ref<Eigen::VectorXd> FX,
                                          Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                          Eigen::Ref<Eigen::VectorXi> KKTLocations,
                                          Eigen::Ref<Eigen::VectorXi> KKTClashes,
                                          std::vector<std::mutex> &KKTLocks,
                                          const SolverIndexingData &data) const = 0;

        virtual void constraints_jacobian_adjointgradient(
            const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
            Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
            Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
            Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
            std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const = 0;

        virtual void constraints_jacobian_adjointgradient_adjointhessian(
            const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
            Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
            Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
            Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
            std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const = 0;

        virtual void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows,
                                   Eigen::Ref<Eigen::VectorXi> KKTcols, int &freeloc, int conoffset,
                                   bool dojac, bool dohess, SolverIndexingData &data) = 0;

        virtual int num_kkt_elements(bool dojac, bool dohess) const = 0;
    };
};

/*
 * Spec for scalar vector functions that can be used as an objective inside of PSIOPT.
 */
struct SolverObjectiveSpec {
    struct Concept {
        virtual ~Concept() = default;

        virtual void objective(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                               double &Val, const SolverIndexingData &data) const = 0;

        virtual void objective_gradient(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                        double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                                        const SolverIndexingData &data) const = 0;

        virtual void objective_gradient_hessian(
            double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X, double &Val,
            Eigen::Ref<Eigen::VectorXd> GX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
            Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
            std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const = 0;
    };
};

// ==========================================================================
// ConstraintBase / ConstraintModel<T> / ConstraintInterface
// ==========================================================================

struct ConstraintBase : SolverConstraintSpec::Concept, SizableSpec::Concept {
    virtual void clone_into(TypeStorage<ConstraintBase> &) const = 0;
};

template <typename T> struct ConstraintModel final : ConstraintBase {
    T data_;
    explicit ConstraintModel(T t) : data_(std::move(t)) {}

    // ---- SizableSpec::Concept ----
    std::string name() const override { return data_.name(); }
    int input_rows() const override { return data_.input_rows(); }
    int output_rows() const override { return data_.output_rows(); }
    bool thread_safe() const override { return data_.thread_safe(); }

    // ---- SolverConstraintSpec::Concept ----
    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const override {
        data_.constraints(X, FX, data);
    }
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const override {
        data_.constraints_adjointgradient(X, L, FX, AGX, data);
    }
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks,
                              const SolverIndexingData &data) const override {
        data_.constraints_jacobian(X, FX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const override {
        data_.constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes,
                                                   KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const override {
        data_.constraints_jacobian_adjointgradient_adjointhessian(
            X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows, Eigen::Ref<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) override {
        data_.get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess, data);
    }
    int num_kkt_elements(bool dojac, bool dohess) const override {
        return data_.num_kkt_elements(dojac, dohess);
    }

    void clone_into(TypeStorage<ConstraintBase> &s) const override {
        s.emplace<ConstraintModel<T>>(data_);
    }
};

struct ConstraintInterface;
struct ObjectiveInterface;

struct ConstraintInterface {
    TypeStorage<ConstraintBase> storage_;

    ConstraintInterface() = default;

    template <class T, std::enable_if_t<
                           !std::is_base_of_v<Eigen::EigenBase<std::decay_t<T>>, std::decay_t<T>>,
                           bool> = true>
    ConstraintInterface(const T &t) {
        SolverInterfaceAdapter<std::decay_t<T>>::install_constraint(t, *this);
    }

    // ---- Forwarding methods ----
    std::string name() const { return storage_.get().name(); }
    int input_rows() const { return storage_.get().input_rows(); }
    int output_rows() const { return storage_.get().output_rows(); }
    bool thread_safe() const { return storage_.get().thread_safe(); }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const {
        storage_.get().constraints(X, FX, data);
    }
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const {
        storage_.get().constraints_adjointgradient(X, L, FX, AGX, data);
    }
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks,
                              const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian(X, FX, KKTmat, KKTLocations, KKTClashes, KKTLocks,
                                            data);
    }
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations,
                                                            KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian_adjointgradient_adjointhessian(
            X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows, Eigen::Ref<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) {
        storage_.get().get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess, data);
    }
    int num_kkt_elements(bool dojac, bool dohess) const {
        return storage_.get().num_kkt_elements(dojac, dohess);
    }
};

// ==========================================================================
// ObjectiveBase / ObjectiveModel<T> / ObjectiveInterface
// ==========================================================================

struct ObjectiveBase : SolverConstraintSpec::Concept,
                       SolverObjectiveSpec::Concept,
                       SizableSpec::Concept {
    virtual void clone_into(TypeStorage<ObjectiveBase> &) const = 0;
};

template <typename T> struct ObjectiveModel final : ObjectiveBase {
    T data_;
    explicit ObjectiveModel(T t) : data_(std::move(t)) {}

    // ---- SizableSpec::Concept ----
    std::string name() const override { return data_.name(); }
    int input_rows() const override { return data_.input_rows(); }
    int output_rows() const override { return data_.output_rows(); }
    bool thread_safe() const override { return data_.thread_safe(); }

    // ---- SolverConstraintSpec::Concept ----
    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const override {
        data_.constraints(X, FX, data);
    }
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const override {
        data_.constraints_adjointgradient(X, L, FX, AGX, data);
    }
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks,
                              const SolverIndexingData &data) const override {
        data_.constraints_jacobian(X, FX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const override {
        data_.constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes,
                                                   KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const override {
        data_.constraints_jacobian_adjointgradient_adjointhessian(
            X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows, Eigen::Ref<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) override {
        data_.get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess, data);
    }
    int num_kkt_elements(bool dojac, bool dohess) const override {
        return data_.num_kkt_elements(dojac, dohess);
    }

    // ---- SolverObjectiveSpec::Concept ----
    void objective(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X, double &Val,
                   const SolverIndexingData &data) const override {
        data_.objective(ObjScale, X, Val, data);
    }
    void objective_gradient(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                            double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                            const SolverIndexingData &data) const override {
        data_.objective_gradient(ObjScale, X, Val, GX, data);
    }
    void objective_gradient_hessian(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                    double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                    Eigen::Ref<Eigen::VectorXi> KKTLocations,
                                    Eigen::Ref<Eigen::VectorXi> KKTClashes,
                                    std::vector<std::mutex> &KKTLocks,
                                    const SolverIndexingData &data) const override {
        data_.objective_gradient_hessian(ObjScale, X, Val, GX, KKTmat, KKTLocations, KKTClashes,
                                         KKTLocks, data);
    }

    void clone_into(TypeStorage<ObjectiveBase> &s) const override {
        s.emplace<ObjectiveModel<T>>(data_);
    }
};

struct ObjectiveInterface {
    TypeStorage<ObjectiveBase> storage_;

    ObjectiveInterface() = default;

    template <class T, std::enable_if_t<
                           !std::is_base_of_v<Eigen::EigenBase<std::decay_t<T>>, std::decay_t<T>>,
                           bool> = true>
    ObjectiveInterface(const T &t) {
        SolverInterfaceAdapter<std::decay_t<T>>::install_objective(t, *this);
    }

    // ---- Forwarding methods ----
    std::string name() const { return storage_.get().name(); }
    int input_rows() const { return storage_.get().input_rows(); }
    int output_rows() const { return storage_.get().output_rows(); }
    bool thread_safe() const { return storage_.get().thread_safe(); }

    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X, Eigen::Ref<Eigen::VectorXd> FX,
                     const SolverIndexingData &data) const {
        storage_.get().constraints(X, FX, data);
    }
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX,
                                     const SolverIndexingData &data) const {
        storage_.get().constraints_adjointgradient(X, L, FX, AGX, data);
    }
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks,
                              const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian(X, FX, KKTmat, KKTLocations, KKTClashes, KKTLocks,
                                            data);
    }
    void constraints_jacobian_adjointgradient(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations,
                                                            KKTClashes, KKTLocks, data);
    }
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks, const SolverIndexingData &data) const {
        storage_.get().constraints_jacobian_adjointgradient_adjointhessian(
            X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes, KKTLocks, data);
    }
    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows, Eigen::Ref<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) {
        storage_.get().get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess, data);
    }
    int num_kkt_elements(bool dojac, bool dohess) const {
        return storage_.get().num_kkt_elements(dojac, dohess);
    }

    void objective(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X, double &Val,
                   const SolverIndexingData &data) const {
        storage_.get().objective(ObjScale, X, Val, data);
    }
    void objective_gradient(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                            double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                            const SolverIndexingData &data) const {
        storage_.get().objective_gradient(ObjScale, X, Val, GX, data);
    }
    void objective_gradient_hessian(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                    double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                    Eigen::Ref<Eigen::VectorXi> KKTLocations,
                                    Eigen::Ref<Eigen::VectorXi> KKTClashes,
                                    std::vector<std::mutex> &KKTLocks,
                                    const SolverIndexingData &data) const {
        storage_.get().objective_gradient_hessian(ObjScale, X, Val, GX, KKTmat, KKTLocations,
                                                  KKTClashes, KKTLocks, data);
    }
};

// ---- DirectFunctionModel bodies ----
//
// Declared in hven/solver_interface_adapter.h; defined here because they
// emplace into the interfaces above, which that header only forward-declares.
// The emplacement is the same call the interfaces made inline before the
// adapter seam existed, so the stored object and the per-call dispatch count
// are unchanged.

template <class T>
void DirectFunctionModel<T>::install_constraint(const T &t, ConstraintInterface &ci) {
    ci.storage_.emplace<ConstraintModel<T>>(t);
}

template <class T>
void DirectFunctionModel<T>::install_objective(const T &t, ObjectiveInterface &oi) {
    // Registration is per type, not per interface: a family registered for
    // direct storage is registered for BOTH interfaces, and a constraint-only
    // family reaching this one would otherwise fail deep inside
    // ObjectiveModel<T>'s instantiation with no statement of what is wrong.
    static_assert(
        requires(const T &f, double s, const Eigen::Ref<const Eigen::VectorXd> &x, double &v,
                 const SolverIndexingData &d) { f.objective(s, x, v, d); },
        "hven objective adapter: this registered type does not provide the scalar "
        "objective interface (objective / objective_gradient / "
        "objective_gradient_hessian; OR == 1). It can enter ConstraintInterface only.");
    oi.storage_.emplace<ObjectiveModel<T>>(t);
}

} // namespace hven::solvers
