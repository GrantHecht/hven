// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Implements the ConstraintFunction class.
// Holds an ConstraintInterface type erasure class and SolverIndexingData struct.
// Interfaces directly with NonLinearProgram and IpmSolver.

#pragma once

#include <mutex>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/model/solver_function_base.h"

namespace hven {

struct ConstraintFunction : SolverFunctionBase<ConstraintInterface> {
    using Base = SolverFunctionBase<ConstraintInterface>;
    using Base::function;
    using Base::index_data;
    using MatrixXi = Eigen::MatrixXi;
    using VectorXi = Eigen::VectorXi;

    ConstraintFunction() {}

    ConstraintFunction(const ConstraintInterface &f, const MatrixXi &vindex,
                       const MatrixXi &cindex) {
        this->function = f;
        this->index_data = SolverIndexingData(f.input_rows(), f.output_rows(), vindex, cindex);
    }

    ConstraintFunction(const ConstraintInterface &f, const SolverIndexingData &data) {
        this->function = f;
        this->index_data = data;
    }

    /// @brief Partitions multiple calls to this function into separate
    /// ConstraintFunction instances that will be called on multiple threads.
    std::vector<ConstraintFunction> thread_split(int Thr) const {
        std::vector<SolverIndexingData> idat = this->index_data.thread_split(Thr);
        std::vector<ConstraintFunction> split(idat.size());
        for (int i = 0; i < idat.size(); i++) {
            split[i] = ConstraintFunction(this->function, idat[i]);
        }
        return split;
    }

    /// @brief Calls the type-erased function's .constraints method, passing
    /// through the solver's arguments and the indexing data.
    void constraints(const Eigen::Ref<const Eigen::VectorXd> &X,
                     Eigen::Ref<Eigen::VectorXd> FX) const {
        this->function.constraints(X, FX, this->index_data);
    }

    /// @brief Calls the type-erased function's .constraints_adjointgradient
    /// method, passing through the solver's arguments and the indexing data.
    void constraints_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                     const Eigen::Ref<const Eigen::VectorXd> &L,
                                     Eigen::Ref<Eigen::VectorXd> FX,
                                     Eigen::Ref<Eigen::VectorXd> AGX) const {
        this->function.constraints_adjointgradient(X, L, FX, AGX, this->index_data);
    }

    /// @brief Calls the type-erased function's .constraints_jacobian method,
    /// passing through the solver's arguments and the indexing data.
    void constraints_jacobian(const Eigen::Ref<const Eigen::VectorXd> &X,
                              Eigen::Ref<Eigen::VectorXd> FX,
                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                              std::vector<std::mutex> &KKTLocks) const {
        this->function.constraints_jacobian(X, FX, KKTmat, KKTLocations, KKTClashes, KKTLocks,
                                            this->index_data);
    }

    /// @brief Calls the type-erased function's
    /// .constraints_jacobian_adjointgradient method, passing through the
    /// solver's arguments and the indexing data.
    void constraints_jacobian_adjointgradient(const Eigen::Ref<const Eigen::VectorXd> &X,
                                              const Eigen::Ref<const Eigen::VectorXd> &L,
                                              Eigen::Ref<Eigen::VectorXd> FX,
                                              Eigen::Ref<Eigen::VectorXd> AGX,
                                              Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                              Eigen::Ref<Eigen::VectorXi> KKTLocations,
                                              Eigen::Ref<Eigen::VectorXi> KKTClashes,
                                              std::vector<std::mutex> &KKTLocks) const {
        this->function.constraints_jacobian_adjointgradient(X, L, FX, AGX, KKTmat, KKTLocations,
                                                            KKTClashes, KKTLocks, this->index_data);
    }

    /// @brief Calls the type-erased function's
    /// .constraints_jacobian_adjointgradient_adjointhessian method, passing
    /// through the solver's arguments and the indexing data.
    void constraints_jacobian_adjointgradient_adjointhessian(
        const Eigen::Ref<const Eigen::VectorXd> &X, const Eigen::Ref<const Eigen::VectorXd> &L,
        Eigen::Ref<Eigen::VectorXd> FX, Eigen::Ref<Eigen::VectorXd> AGX,
        Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
        Eigen::Ref<Eigen::VectorXi> KKTLocations, Eigen::Ref<Eigen::VectorXi> KKTClashes,
        std::vector<std::mutex> &KKTLocks) const {
        this->function.constraints_jacobian_adjointgradient_adjointhessian(
            X, L, FX, AGX, KKTmat, KKTLocations, KKTClashes, KKTLocks, this->index_data);
    }
};

} // namespace hven
