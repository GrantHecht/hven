// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_problem.h — the quadratic program solved by the SQP inner loop:
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be
//           Ai x <= bi
//           l <= x <= u
//
// H is stored as the upper triangle of a symmetric n x n matrix (SpMatRM).
//
// Multiplier sign convention (stationarity of the KKT system):
//
//     grad(f) + Ae^T lambda_e + Ai^T lambda_i - z = 0,   lambda_i >= 0
//
// where grad(f) = Hx + g. The bound multiplier z is >= 0 when the variable
// sits at its active lower bound, <= 0 when it sits at its active upper
// bound, and 0 when the variable is free.

#include <stdexcept>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/qp/qp_types.h>

namespace hven::solvers {

/// @brief The quadratic program solved by the SQP inner loop (see the
/// formulation and multiplier sign convention in the file header).
struct QpProblem {
    SpMatRM H;                                           ///< n x n, upper triangle only.
    Vec g;                                               ///< n.
    Eigen::SparseMatrix<double, Eigen::RowMajor> Ae, Ai; ///< me x n, mi x n.
    Vec be, bi;                                          ///< me, mi.
    Vec lower, upper;                                    ///< n (+/-inf allowed, e.g. +/-1e20).

    Index n() const { return g.size(); }
    Index me() const { return Ae.rows(); }
    Index mi() const { return Ai.rows(); }

    /// @brief Checks dimensional consistency of every block against
    /// n()/me()/mi() and the upper-triangle convention for H.
    /// @throws std::invalid_argument On any inconsistency, including a
    ///   below-diagonal entry in H.
    void validate() const {
        const Index nn = n();
        if (H.rows() != nn || H.cols() != nn) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: H is {}x{}, expected {}x{}", H.rows(), H.cols(), nn, nn));
        }
        // Callout: H must store ONLY its upper triangle (row <= col). Every
        // consumer reads H via selfadjointView<Upper>()/symmetrizes off that
        // convention alone, so a lower-triangle population would be silently
        // interpreted as a DIFFERENT (transposed-contribution) matrix rather
        // than rejected -- the dense oracle symmetrizes explicitly and would
        // agree with the wrong answer. Rejected here instead.
        for (Index i = 0; i < nn; ++i) {
            for (SpMatRM::InnerIterator it(H, i); it; ++it) {
                if (it.row() > it.col()) {
                    throw std::invalid_argument(
                        fmt::format("QpProblem::validate: H has a lower-triangle entry at "
                                    "(row={}, col={}); H must store only its upper triangle "
                                    "(row <= col)",
                                    it.row(), it.col()));
                }
            }
        }
        if (Ae.cols() != nn) {
            throw std::invalid_argument(
                fmt::format("QpProblem::validate: Ae has {} columns, expected {}", Ae.cols(), nn));
        }
        if (be.size() != me()) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: be has size {}, expected {} (= Ae.rows())", be.size(), me()));
        }
        if (Ai.cols() != nn) {
            throw std::invalid_argument(
                fmt::format("QpProblem::validate: Ai has {} columns, expected {}", Ai.cols(), nn));
        }
        if (bi.size() != mi()) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: bi has size {}, expected {} (= Ai.rows())", bi.size(), mi()));
        }
        if (lower.size() != nn) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: lower has size {}, expected {}", lower.size(), nn));
        }
        if (upper.size() != nn) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: upper has size {}, expected {}", upper.size(), nn));
        }
    }
};

/// @brief The solution of one QpProblem: primal/dual vectors, working-set
/// state, counters, and the trust-region activity flags.
struct QpSolution {
    QpStatus status = QpStatus::kOptimal;
    Vec x, lambda_e, lambda_i, z;        ///< z: bound multipliers (>=0 at lower, <=0 at upper).
    std::vector<BoundState> bound_state; ///< Size n.
    std::vector<bool> ineq_active;       ///< Size mi.
    QpCounters counters;

    /// Trust-region activity set (qp_engine.h's "Section 6"), size n. True at
    /// index i iff variable i is held by a TR-tight effective bound rather
    /// than a real one: bound_state[i] reports kFree for such a variable (TR
    /// pins are deliberately EXCLUDED from bound_state, a real-bound-only
    /// view) and z(i) is forced to 0 (TR duals are internal to the ratio
    /// test/drop rule, never exposed). Read tr_active to detect a binding
    /// radius; warm-start seed ingestion never reads this field (a TR pin
    /// from a previous solve does not carry into one with a different
    /// radius).
    ///
    /// STATIONARITY CAVEAT: at a TR-pinned index the reported quantities do
    /// NOT satisfy stationarity — grad(f) + Ae^T lambda_e + Ai^T lambda_i -
    /// z(i) != 0 in general, because z(i) here is the forced 0, not the
    /// internal (nonzero) TR dual that actually balanced the residual. A
    /// kFree entry there means "unconstrained by any REAL bound", NOT
    /// "stationary treated as free".
    std::vector<bool> tr_active;
};

} // namespace hven::solvers
