// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Implements the ObjectiveFunction class.
// Holds an ObjectiveInterface type erasure class and SolverIndexingData struct.
// Interfaces directly with NonLinearProgram and InteriorPointSolver.

#pragma once

#include <mutex>
#include <vector>

#include <Eigen/Core>
#include <Eigen/Sparse>

#include "hven/detail/interior/solver_function_base.h"
#include "hven/detail/interior/typedefs/eigen_types.h"

namespace hven::solvers {

struct ObjectiveFunction : SolverFunctionBase<ObjectiveInterface> {
    ObjectiveFunction() {}

    ObjectiveFunction(const ObjectiveInterface &f, const MatrixXi &vindex) {
        this->function_ = f;
        this->index_data_ = SolverIndexingData(f.input_rows(), vindex);
    }
    ObjectiveFunction(const ObjectiveInterface &f, const SolverIndexingData &data) {
        this->function_ = f;
        this->index_data_ = data;
    }

    /// @brief Partitions multiple calls to this function into separate
    /// ObjectiveFunction instances that will be called on multiple threads.
    std::vector<ObjectiveFunction> thread_split(int Thr) const {
        std::vector<SolverIndexingData> idat = this->index_data_.thread_split(Thr);
        std::vector<ObjectiveFunction> split(idat.size());
        for (int i = 0; i < idat.size(); i++) {
            split[i] = ObjectiveFunction(this->function_, idat[i]);
        }
        return split;
    }

    /// @brief Calls the type-erased function's .objective method, passing
    /// through the solver's arguments and the indexing data.
    void objective(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val) const {
        this->function_.objective(ObjScale, X, Val, this->index_data_);
    }

    /// @brief Calls the type-erased function's .objective_gradient method,
    /// passing through the solver's arguments and the indexing data.
    void objective_gradient(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val,
                            EigenRef<Eigen::VectorXd> GX) const {
        this->function_.objective_gradient(ObjScale, X, Val, GX, this->index_data_);
    }

    /// @brief Calls the type-erased function's .objective_gradient_hessian
    /// method, scattering the Hessian into the KKT matrix through the
    /// location/clash tables under per-row locks, and passing through the
    /// indexing data.
    void objective_gradient_hessian(double ObjScale, ConstEigenRef<Eigen::VectorXd> X, double &Val,
                                    EigenRef<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                    EigenRef<Eigen::VectorXi> KKTLocations,
                                    EigenRef<Eigen::VectorXi> KKTClashes,
                                    std::vector<std::mutex> &KKTLocks) {
        this->function_.objective_gradient_hessian(ObjScale, X, Val, GX, KKTmat, KKTLocations,
                                                   KKTClashes, KKTLocks, this->index_data_);
    }
};

} // namespace hven::solvers
