// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// nlp_eval.h -- NlpEval, the SQP engine's one-point model evaluation bundle. Its own header so
// the headers that only consume it (assembly_eval_seam.h, globalization/soc.h,
// globalization/restoration.h) can include it directly instead of depending on
// sqp_solver_impl.h, which includes them. The evaluation functions that produce one
// (eval_nlp and friends) stay in sqp_solver_impl.h.
//
// Why detail/sqp/: every consumer is SQP engine code; no model or QP header names it.

#include <Eigen/SparseCore>

#include <hven/core/types.h>

namespace hven::detail {

/// @brief One model evaluation at one point: the five quantities a major
///        iteration reads, taken together so nothing is evaluated twice.
struct NlpEval {
    /// @brief Objective value at the evaluation point.
    double f = 0.0;
    /// @brief Objective gradient, equality- and inequality-constraint values;
    ///        sizes n(), me() and mi().
    Vec grad, ce, ci;
    /// @brief Equality and inequality constraint Jacobians; me() x n() and mi() x n().
    Eigen::SparseMatrix<double, Eigen::RowMajor> Je, Ji;

    /// @brief False if any of f/grad/cE/cI is NaN or infinite. Checked
    ///        explicitly: a running maximum would swallow a NaN silently.
    bool all_finite = true;
};

} // namespace hven::detail
