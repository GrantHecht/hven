// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// Implements the ObjectiveFunction class.
// Holds an ObjectiveInterface type erasure class and SolverIndexingData struct.
// Interfaces directly with NonLinearProgram and IpmSolver.

#pragma once

#include <mutex>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/model/solver_function_base.h"

namespace hven {

struct ObjectiveFunction : SolverFunctionBase<ObjectiveInterface> {
    ObjectiveFunction() {}

    ObjectiveFunction(const ObjectiveInterface &f, const MatrixXi &vindex) {
        this->function = f;
        this->index_data = SolverIndexingData(f.input_rows(), vindex);
    }
    ObjectiveFunction(const ObjectiveInterface &f, const SolverIndexingData &data) {
        this->function = f;
        this->index_data = data;
    }

    /// @brief Partitions multiple calls to this function into separate
    /// ObjectiveFunction instances that will be called on multiple threads.
    std::vector<ObjectiveFunction> thread_split(int Thr) const {
        std::vector<SolverIndexingData> idat = this->index_data.thread_split(Thr);
        std::vector<ObjectiveFunction> split(idat.size());
        for (int i = 0; i < idat.size(); i++) {
            split[i] = ObjectiveFunction(this->function, idat[i]);
        }
        return split;
    }

    /// @brief Calls the type-erased function's .objective method, passing
    /// through the solver's arguments and the indexing data.
    void objective(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X, double &Val) const {
        this->function.objective(ObjScale, X, Val, this->index_data);
    }

    /// @brief Calls the type-erased function's .objective_gradient method,
    /// passing through the solver's arguments and the indexing data.
    void objective_gradient(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                            double &Val, Eigen::Ref<Eigen::VectorXd> GX) const {
        this->function.objective_gradient(ObjScale, X, Val, GX, this->index_data);
    }

    /// @brief Calls the type-erased function's .objective_gradient_hessian
    /// method, passing through the solver's arguments and the indexing data.
    void objective_gradient_hessian(double ObjScale, const Eigen::Ref<const Eigen::VectorXd> &X,
                                    double &Val, Eigen::Ref<Eigen::VectorXd> GX,
                                    Eigen::SparseMatrix<double, Eigen::RowMajor> &KKTmat,
                                    Eigen::Ref<Eigen::VectorXi> KKTLocations,
                                    Eigen::Ref<Eigen::VectorXi> KKTClashes,
                                    std::vector<std::mutex> &KKTLocks) {
        this->function.objective_gradient_hessian(ObjScale, X, Val, GX, KKTmat, KKTLocations,
                                                  KKTClashes, KKTLocks, this->index_data);
    }
};

} // namespace hven
