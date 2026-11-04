// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// The raw piece protocol's two type-erased handles, ConstraintInterface and
// ObjectiveInterface, and the surface probes their adapters check a piece
// against. The protocol is specified in docs/assembly-protocol.md; the
// local-block layer (model/local_blocks.h) is the default way to write a piece
// and produces these handles itself.

#pragma once

#include <concepts>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/type_storage.h"
#include "hven/model/indexing_data.h"
#include "hven/model/sizing_specs.h"
#include "hven/solver_interface_adapter.h"

namespace hven::solvers {

// Import cross-namespace types used throughout the solver layer.
using utils::TypeStorage;

// Function-surface probes
//
// These mirror, expression for expression, what ConstraintModel<T> and
// ObjectiveModel<T> below actually call on their stored T. That correspondence
// is the whole point: an adapter can then check a type BEFORE emplacing it, so
// a type that would have failed somewhere inside the Model's instantiation
// gets an authored message instead of a raw one. Keep these in step with the
// Models -- a member added to a Model's forwarding set belongs here too, or
// the raw failures come back.
//
// They are expression probes, not signature matches: anything a Model can call
// satisfies them, and nothing else does.

/// What every piece reports about itself: its name, its input count and its
/// threading posture. An objective piece needs nothing more of this kind.
template <class T>
concept PieceIdentity = requires(const T &f) {
    { f.name() } -> std::convertible_to<std::string>;
    { f.input_rows() } -> std::convertible_to<int>;
    { f.thread_safe() } -> std::convertible_to<bool>;
};

/// The identity plus the output count a constraint piece reports.
template <class T>
concept SizableFunction = PieceIdentity<T> && requires(const T &f) {
    { f.output_rows() } -> std::convertible_to<int>;
};

/// The claim pass: how many KKT slots one application claims, and claiming them.
/// Both kinds of piece carry it.
template <class T>
concept PieceClaimSurface =
    requires(const T &f, T &mf, Eigen::Ref<Eigen::VectorXi> KKTIdx, SolverIndexingData &mdata,
             int &freeloc, int offset, bool flag) {
        mf.get_kkt_space(KKTIdx, KKTIdx, freeloc, offset, flag, flag, mdata);
        { f.num_kkt_elements(flag, flag) } -> std::convertible_to<int>;
    };

template <class T>
concept SolverConstraintFunction =
    SizableFunction<T> &&
    requires(const T &f, T &mf, const Eigen::Ref<const Eigen::VectorXd> &X,
             Eigen::Ref<Eigen::VectorXd> FX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
             Eigen::Ref<Eigen::VectorXi> KKTIdx, std::vector<std::mutex> &KKTLocks,
             const SolverIndexingData &data, SolverIndexingData &mdata, int &freeloc, int offset,
             bool flag) {
        f.constraints(X, FX, data);
        f.constraints_adjointgradient(X, X, FX, FX, data);
        f.constraints_jacobian(X, FX, KKTmat, KKTIdx, KKTIdx, KKTLocks, data);
        f.constraints_jacobian_adjointgradient(X, X, FX, FX, KKTmat, KKTIdx, KKTIdx, KKTLocks,
                                               data);
        f.constraints_jacobian_adjointgradient_adjointhessian(X, X, FX, FX, KKTmat, KKTIdx, KKTIdx,
                                                              KKTLocks, data);
        mf.get_kkt_space(KKTIdx, KKTIdx, freeloc, offset, flag, flag, mdata);
        { f.num_kkt_elements(flag, flag) } -> std::convertible_to<int>;
    };

/// The three scalar-objective members ONLY. Deliberately independent of the
/// identity and claim probes: keeping the groups apart is what lets a type
/// missing exactly one of them get exactly one authored message.
template <class T>
concept SolverObjectiveSurface =
    requires(const T &f, double s, const Eigen::Ref<const Eigen::VectorXd> &X, double &Val,
             Eigen::Ref<Eigen::VectorXd> GX, Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
             Eigen::Ref<Eigen::VectorXi> KKTIdx, std::vector<std::mutex> &KKTLocks,
             const SolverIndexingData &data) {
        f.objective(s, X, Val, data);
        f.objective_gradient(s, X, Val, GX, data);
        f.objective_gradient_hessian(s, X, Val, GX, KKTmat, KKTIdx, KKTIdx, KKTLocks, data);
    };

/// What ObjectiveModel<T> needs in full: the identity, the claim pass and the
/// three objective members. An objective piece carries no constraint surface
/// and no output count; the handle reports one output, the scalar.
template <class T>
concept SolverObjectiveFunction =
    PieceIdentity<T> && PieceClaimSurface<T> && SolverObjectiveSurface<T>;

/// @brief Type-erasure spec for a vector function usable as a constraint
/// inside IpmSolver: the pure-virtual Concept the stored model
/// implements.
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

/// @brief Type-erasure spec for a scalar function usable as an objective
/// inside IpmSolver: the pure-virtual Concept the stored model
/// implements.
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

// ConstraintBase / ConstraintModel<T> / ConstraintInterface

struct ConstraintBase : SolverConstraintSpec::Concept, SizableSpec::Concept {
    virtual void clone_into(TypeStorage<ConstraintBase> &) const = 0;
};

template <typename T> struct ConstraintModel final : ConstraintBase {
    T data_;
    explicit ConstraintModel(T t) : data_(std::move(t)) {}

    // SizableSpec::Concept
    std::string name() const override { return data_.name(); }
    int input_rows() const override { return data_.input_rows(); }
    int output_rows() const override { return data_.output_rows(); }
    bool thread_safe() const override { return data_.thread_safe(); }

    // SolverConstraintSpec::Concept
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
        // Specialization lookup does not fall back: once SolverInterfaceAdapter<T>
        // is specialized, an omitted install_constraint is a raw member-lookup
        // failure and the primary template's authored message never gets a
        // chance to fire. Check for the operation here so that omission gets an
        // authored message too, and guard the call with the same condition so
        // nothing raw is reached behind a failed check.
        constexpr bool adapter_installs_constraints =
            requires(const std::decay_t<T> &f, ConstraintInterface &ci) {
                SolverInterfaceAdapter<std::decay_t<T>>::install_constraint(f, ci);
            };
        static_assert(adapter_installs_constraints,
                      "hven constraint adapter: the SolverInterfaceAdapter specialization for "
                      "this type does not provide install_constraint. A specialization must "
                      "provide BOTH installs, because it is selected by type and is therefore "
                      "reachable from both interfaces. Add install_constraint, or inherit "
                      "hven::solvers::ConstraintUnsupported<T> to declare the type "
                      "objective-only and get an authored refusal on this route.");
        if constexpr (adapter_installs_constraints) {
            SolverInterfaceAdapter<std::decay_t<T>>::install_constraint(t, *this);
        }
    }

    // Forwarding methods
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

// ObjectiveBase / ObjectiveModel<T> / ObjectiveInterface

/// The objective handle's erased surface: the identity, the claim pass and the
/// objective members, and nothing of the constraint kind.
struct ObjectiveBase : SolverObjectiveSpec::Concept {
    virtual std::string name() const = 0;
    virtual int input_rows() const = 0;
    virtual bool thread_safe() const = 0;
    virtual void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows,
                               Eigen::Ref<Eigen::VectorXi> KKTcols, int &freeloc, int conoffset,
                               bool dojac, bool dohess, SolverIndexingData &data) = 0;
    virtual int num_kkt_elements(bool dojac, bool dohess) const = 0;
    virtual void clone_into(TypeStorage<ObjectiveBase> &) const = 0;
};

template <typename T> struct ObjectiveModel final : ObjectiveBase {
    T data_;
    explicit ObjectiveModel(T t) : data_(std::move(t)) {}

    std::string name() const override { return data_.name(); }
    int input_rows() const override { return data_.input_rows(); }
    bool thread_safe() const override { return data_.thread_safe(); }

    void get_kkt_space(Eigen::Ref<Eigen::VectorXi> KKTrows, Eigen::Ref<Eigen::VectorXi> KKTcols,
                       int &freeloc, int conoffset, bool dojac, bool dohess,
                       SolverIndexingData &data) override {
        data_.get_kkt_space(KKTrows, KKTcols, freeloc, conoffset, dojac, dohess, data);
    }
    int num_kkt_elements(bool dojac, bool dohess) const override {
        return data_.num_kkt_elements(dojac, dohess);
    }

    // SolverObjectiveSpec::Concept
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
        // See ConstraintInterface's constructor: same backstop, other route.
        // This is the likelier of the two to be hit, since a constraint-only
        // adapter that simply omits install_objective is the natural mistake.
        constexpr bool adapter_installs_objectives =
            requires(const std::decay_t<T> &f, ObjectiveInterface &oi) {
                SolverInterfaceAdapter<std::decay_t<T>>::install_objective(f, oi);
            };
        static_assert(adapter_installs_objectives,
                      "hven objective adapter: the SolverInterfaceAdapter specialization for "
                      "this type does not provide install_objective. A specialization must "
                      "provide BOTH installs, because it is selected by type and is therefore "
                      "reachable from both interfaces. Add install_objective, or inherit "
                      "hven::solvers::ObjectiveUnsupported<T> to declare the type "
                      "constraint-only and get an authored refusal on this route.");
        if constexpr (adapter_installs_objectives) {
            SolverInterfaceAdapter<std::decay_t<T>>::install_objective(t, *this);
        }
    }

    // Forwarding methods
    std::string name() const { return storage_.get().name(); }
    int input_rows() const { return storage_.get().input_rows(); }
    /// An objective is scalar: one output, reported here rather than asked of
    /// the piece.
    int output_rows() const { return 1; }
    bool thread_safe() const { return storage_.get().thread_safe(); }

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

// Each body checks the surface it is about to require, and guards the
// emplacement with that same condition. The guard is what keeps the authored
// message the ONLY output: without it, a type that fails the check is still
// fed to ConstraintModel/ObjectiveModel, whose forwarding methods then add a
// raw "no member named" for every member the type is missing.

template <class T>
void DirectFunctionModel<T>::install_constraint(const T &t, ConstraintInterface &ci) {
    static_assert(SolverConstraintFunction<T>,
                  "hven constraint adapter: this type does not provide the piece constraint "
                  "surface (and exposes no pack_into_constraint_interface hook). A directly "
                  "stored constraint must supply name / "
                  "input_rows / output_rows / thread_safe, the constraints* evaluation family "
                  "(constraints, constraints_adjointgradient, constraints_jacobian, "
                  "constraints_jacobian_adjointgradient, and "
                  "constraints_jacobian_adjointgradient_adjointhessian), get_kkt_space, and "
                  "num_kkt_elements -- callable with the argument types ConstraintModel passes.");
    if constexpr (SolverConstraintFunction<T>) {
        ci.storage_.emplace<ConstraintModel<T>>(t);
    }
}

template <class T>
void DirectFunctionModel<T>::install_objective(const T &t, ObjectiveInterface &oi) {
    // Registration is per type, not per interface: a family stored directly
    // is reachable from BOTH interfaces, so a constraint-only family can reach
    // this one. The piece surface and the objective surface are asserted
    // separately, so a type missing exactly one is told which.
    static_assert(PieceIdentity<T> && PieceClaimSurface<T>,
                  "hven objective adapter: this type does not provide the piece surface an "
                  "objective needs: name / input_rows / thread_safe, get_kkt_space and "
                  "num_kkt_elements -- callable with the argument types ObjectiveModel passes. "
                  "An objective piece needs no output_rows and no constraints* family.");
    static_assert(SolverObjectiveSurface<T>,
                  "hven objective adapter: this type does not provide the scalar "
                  "objective interface (objective / objective_gradient / "
                  "objective_gradient_hessian; OR == 1) -- or provides only part of it. All "
                  "three are required, callable with the argument types ObjectiveModel passes. "
                  "A type without them can enter ConstraintInterface only; declare that by "
                  "inheriting hven::solvers::ObjectiveUnsupported<T> in its adapter.");
    if constexpr (SolverObjectiveFunction<T>) {
        oi.storage_.emplace<ObjectiveModel<T>>(t);
    }
}

} // namespace hven::solvers
