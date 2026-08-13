#pragma once

// qp_problem.h — the quadratic program solved by the SQP inner loop:
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be
//           Ai x <= bi
//           l <= x <= u
//
// H is stored as the upper triangle of a symmetric n x n matrix (SpMatU).
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

#include <hven/detail/sqp/types.h>

namespace hven::solvers {

struct QpProblem {
    SpMatU H;                                            // n x n, upper triangle
    Vec g;                                               // n
    Eigen::SparseMatrix<double, Eigen::RowMajor> Ae, Ai; // me x n, mi x n
    Vec be, bi;                                          // me, mi
    Vec lower, upper;                                    // n (+/-inf allowed, e.g. +/-1e20)

    Index n() const { return g.size(); }
    Index me() const { return Ae.rows(); }
    Index mi() const { return Ai.rows(); }

    // Throws std::invalid_argument if any block's dimensions are inconsistent
    // with n(), me(), or mi().
    void validate() const {
        const Index nn = n();
        if (H.rows() != nn || H.cols() != nn) {
            throw std::invalid_argument(fmt::format(
                "QpProblem::validate: H is {}x{}, expected {}x{}", H.rows(), H.cols(), nn, nn));
        }
        // H must store ONLY its upper triangle (row <= col): every consumer
        // (kkt_assembly.h's Hessian block, qp_engine.h's price()) reads H via
        // selfadjointView<Upper>()/symmetrizes off of that convention alone,
        // so a caller who instead populates the lower triangle gets an H that
        // is silently interpreted as a DIFFERENT (transposed-contribution)
        // matrix rather than rejected -- the dense oracle would not even
        // notice, since it symmetrizes explicitly and so agrees with the
        // wrong answer. Reject it here instead.
        for (Index i = 0; i < nn; ++i) {
            for (SpMatU::InnerIterator it(H, i); it; ++it) {
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

struct QpSolution {
    QpStatus status = QpStatus::kOptimal;
    Vec x, lambda_e, lambda_i, z;        // z = bound multipliers (>=0 at lower, <=0 at upper)
    std::vector<BoundState> bound_state; // n
    std::vector<bool> ineq_active;       // mi
    QpCounters counters;

    // Trust-region activity set (qp_engine.h's "Section 6"), size n. True at
    // index i iff variable i is held by a TR-tight effective bound rather
    // than a real one: bound_state[i] reports kFree for such a variable
    // (TR pins are deliberately EXCLUDED from bound_state, which is a
    // real-bound-only view) and z(i) is forced to 0 (TR duals are internal
    // to the ratio test/drop rule and are never exposed). A caller -- the
    // Phase-3 SQP driver -- reads tr_active to detect a binding radius, not
    // z. Warm-start seed ingestion never reads this field (see qp_engine.h):
    // a TR pin from a previous solve does not carry into the next one, which
    // may use a different radius.
    //
    // STATIONARITY CAVEAT: at a TR-pinned index, the reported quantities do
    // NOT satisfy stationarity -- grad(f) + Ae^T lambda_e + Ai^T lambda_i -
    // z(i) != 0 in general, because z(i) here is the forced 0, not the
    // internal (nonzero) TR dual that actually balanced the residual during
    // the solve. A kFree entry at such an index is therefore evidence that
    // the coordinate is unconstrained by any REAL bound, NOT evidence that
    // the point is stationary in that coordinate treated as free. Read
    // tr_active, never z or bound_state, to detect a binding radius.
    std::vector<bool> tr_active;
};

} // namespace hven::solvers
