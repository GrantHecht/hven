// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// tests/sqp/support/qp_as_nlp_model.h -- test-support only, NOT part of the public library
// surface. One engine-agnostic adapter, deliberately gtest-free so a measurement binary can
// include it without pulling GoogleTest in. Requirement: `.superpowers/w5-t5-report.md`.

#include <utility>

#include <Eigen/SparseCore>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers::test_support {

/// @brief A `QpProblem` presented as an `NlpModel`, so a QP fixture's OWN
///        active set is what the driver's first subproblem reports.
///
/// EXACT BY CONSTRUCTION: f is the QP's objective and the rows are affine, so
/// the subproblem the driver builds at x is this QP shifted to p = x* - x. One
/// major solves it, and its `ineq_active`/`bound_state` are the QP's own.
class QpAsNlpModel final : public NlpModel {
  public:
    QpAsNlpModel(QpProblem qp, Vec x0) : qp_(std::move(qp)), x0_(std::move(x0)) {
        // The eval boundary refuses an uncompressed matrix return by name
        // (nlp_adapter.h), and `resize` alone leaves one.
        qp_.H.makeCompressed();
        qp_.Ae.makeCompressed();
        qp_.Ai.makeCompressed();
    }

    Index n() const override { return qp_.n(); }
    Index me() const override { return qp_.me(); }
    Index mi() const override { return qp_.mi(); }

    double eval_f(const Vec &x) const override {
        return 0.5 * x.dot(qp_.H.selfadjointView<Eigen::Upper>() * x) + qp_.g.dot(x);
    }
    Vec eval_grad(const Vec &x) const override {
        return Vec(qp_.H.selfadjointView<Eigen::Upper>() * x) + qp_.g;
    }
    Vec eval_ce(const Vec &x) const override { return Vec(qp_.Ae * x) - qp_.be; }
    Vec eval_ci(const Vec &x) const override { return Vec(qp_.Ai * x) - qp_.bi; }
    SpMatRM eval_hess(const Vec &, double obj_scale, const Vec &, const Vec &) const override {
        return SpMatRM(qp_.H * obj_scale);
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_e(const Vec &) const override {
        return qp_.Ae;
    }
    Eigen::SparseMatrix<double, Eigen::RowMajor> eval_jac_i(const Vec &) const override {
        return qp_.Ai;
    }
    const Vec &lower() const override { return qp_.lower; }
    const Vec &upper() const override { return qp_.upper; }
    Vec start_point() const override { return x0_; }

  private:
    QpProblem qp_;
    Vec x0_;
};

} // namespace hven::solvers::test_support
