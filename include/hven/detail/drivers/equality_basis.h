// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// equality_basis.h — an independent row basis of the equality Jacobian.
//
// Given Je (me x n) and the box, the fixed columns S = {i : l_i == u_i} are
// eliminated, the rows are equilibrated on the free columns F, and a threshold
// sparse QR of the transpose splits the rows into a retained set B (increasing
// model-row order) and a dropped set D with a_r(F) = sum_k C(k, r) a_k(F) for
// r in D. A split is accepted only when (i) every dropped pivot is below
// tau_lo, (ii) every retained pivot is at least tau_hi = gap_ratio * tau_lo,
// and (iii) every dropped row reconstructs from B to tau_lo with a bounded
// equilibrated coefficient; otherwise nothing is dropped (kUndecidable).
// Rows empty on F are dropped with a zero coefficient column. A sparse Cholesky
// of the equilibrated Gram matrix runs first: when every pivot clears
// max(1e-5, 10 tau_hi) the rows are independent and the QR is skipped (the
// pre-check can only drop fewer rows, never more).
//
// The Eigen factorizations (SimplicialLLT, SparseQR) are the SCAFFOLD oracle,
// behind one internal seam in equality_basis.cpp. The production oracle is
// registered as E1b: a pivoted LDL^T of the equilibrated Gram matrix through
// hven::linear, null vectors naming the dependent rows, verify() as the guarantee.
//
// C is stored in the units of the Je it was computed from. With
// N = [-C; I] (rows B then D), Je(:,F)^T N = 0 and
//   lift(lambda_B)  = the minimum-norm lambda with Je(:,F)^T lambda = Je(B,F)^T lambda_B
//   fold(lambda)    = lambda(B) + C lambda(D)          (fold(lift(y)) == y)
//   reduce(qp)      : Ae <- Ae(B,:), be <- be(B) + t,  t = C (I + C^T C)^-1 gamma,
//                     gamma = N^T (be - Ae d),  d = qp.lower on S and 0 elsewhere,
// so t is the minimum-norm rhs correction that makes the dropped rows' linearization
// consistent with the retained rows'. With nothing dropped every operation is the
// identity and reduce() does not touch the problem.

#include <cstdint>
#include <memory>
#include <vector>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/model/nlp_model.h>

namespace hven::solvers::detail {

/// @brief Acceptance thresholds of an EqualityBasis split.
struct EqualityBasisTolerances {
    /// tau_lo = pivot_factor * (|F| + me) * eps, on unit-norm columns.
    double pivot_factor = 20.0;
    /// tau_hi = gap_ratio * tau_lo; a retained pivot in (tau_lo, tau_hi) is undecidable.
    double gap_ratio = 1e4;
    /// Largest accepted |C~| (equilibrated units) of a dropped row's reconstruction.
    double coef_cap = 1e8;
};

enum class EqualityBasisVerdict {
    kNotComputed, ///< compute() has not run; the basis is the identity.
    kFullRank,    ///< Nothing dropped; every retained pivot clears the gap.
    kReduced,     ///< At least one row dropped.
    kUndecidable, ///< The spectrum has no gap or a reconstruction failed; nothing dropped.
};

/// @brief Where a dropped row's combination gap sits.
enum class EqualityGapBand {
    kNoise,        ///< |g| <= 10 eps (|w_r| + sum |C| |w_k|).
    kFeasible,     ///< |g| <= feas_tol (1 + ||C(:,r)||_1).
    kInconsistent, ///< Neither: no point satisfies the group to feas_tol.
};

class EqualityBasis {
  public:
    EqualityBasis();
    ~EqualityBasis();
    EqualityBasis(EqualityBasis &&) noexcept;
    EqualityBasis &operator=(EqualityBasis &&) noexcept;
    EqualityBasis(const EqualityBasis &) = delete;
    EqualityBasis &operator=(const EqualityBasis &) = delete;

    /// @brief Computes the split of Je's rows.
    /// @throws std::invalid_argument On a size mismatch among Je, lower and upper.
    void compute(const SpMatRM &Je, const Vec &lower, const Vec &upper,
                 const EqualityBasisTolerances &tol = {});

    /// @brief Returns to the identity basis over `me` rows (verdict kNotComputed).
    void reset(Index me);

    EqualityBasisVerdict verdict() const;
    /// True iff at least one row is dropped.
    bool reduces() const;
    Index me() const;
    const std::vector<Index> &retained() const;
    const std::vector<Index> &dropped() const;
    /// |B| x |D|, column-major.
    const Eigen::SparseMatrix<double> &coefficients() const;
    double tau_lo() const;
    double tau_hi() const;
    /// Smallest retained |R_kk|, or the pre-check's smallest Cholesky pivot
    /// (+inf when no pivot was formed).
    double min_retained_pivot() const;
    /// Whether the last compute() ran the sparse QR (the pre-check did not settle it).
    bool ran_qr() const;
    /// Largest accepted backward residual, equilibrated units.
    double max_backward_residual() const;
    /// Largest |C~| over the dropped rows, equilibrated units.
    double max_coefficient() const;
    /// Hash of the pattern and values of the Je compute() read.
    std::uint64_t jacobian_hash() const;

    /// @brief FNV-1a over Je's shape, pattern and values.
    static std::uint64_t hash_jacobian(const SpMatRM &Je);

    /// @brief Whether every dropped row still reconstructs from B at Je's values
    ///        with the stored coefficients, to the compute-time tolerance.
    bool verify(const SpMatRM &Je) const;

    /// @brief The combination gaps g = N^T w, w = ce + Je d with d = (l - x) on S (l as
    ///        passed to compute()), and their bands at `feas_tol`. Both sized |D|.
    void read_gaps(const Vec &ce, const SpMatRM &Je, const Vec &x, double feas_tol, Vec &gap,
                   std::vector<EqualityGapBand> &band) const;

    /// @brief Hash of Je's values on dropped row j and its support rows in B.
    std::uint64_t group_hash(const SpMatRM &Je, Index j) const;

    /// @brief Restricts qp's equality block to B with the projected rhs (header).
    /// @throws std::invalid_argument When qp.me() or qp.n() disagree with the basis.
    void reduce(QpProblem &qp) const;

    /// @brief t = C (I + C^T C)^-1 gamma, |B|.
    Vec project_rhs(const Vec &gamma) const;
    /// @brief |B| -> me, minimum norm (header).
    Vec lift(const Vec &lambda_B) const;
    /// @brief me -> |B| (header).
    Vec fold(const Vec &lambda) const;

    /// @brief be(B) + t for a full-size rhs `be` of the system Ae d = be whose fixed
    ///        columns sit at d = `lower` (the same map reduce() applies).
    Vec reduce_rhs(const Vec &be, const SpMatRM &Ae, const Vec &lower) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

/// @brief The wrapped model with its equality block restricted to `rows`
///        (increasing); multipliers of the omitted rows are zero in the Hessian.
///        Holds a reference: the wrapped model must outlive it.
class EqualityRowSubsetModel final : public NlpModel {
  public:
    /// @throws std::invalid_argument On a row outside [0, model.me()) or out of order.
    EqualityRowSubsetModel(const NlpModel &model, std::vector<Index> rows);

    Index n() const override;
    Index me() const override;
    Index mi() const override;
    double eval_f(const Vec &x) const override;
    Vec eval_grad(const Vec &x) const override;
    Vec eval_ce(const Vec &x) const override;
    Vec eval_ci(const Vec &x) const override;
    SpMatRM eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                      const Vec &lambda_i) const override;
    SpMatRM eval_jac_e(const Vec &x) const override;
    SpMatRM eval_jac_i(const Vec &x) const override;
    void eval_grad_in_place(const Vec &x, Vec &out) const override;
    void eval_ce_in_place(const Vec &x, Vec &out) const override;
    void eval_ci_in_place(const Vec &x, Vec &out) const override;
    void eval_jac_e_in_place(const Vec &x, SpMatRM &out) const override;
    void eval_jac_i_in_place(const Vec &x, SpMatRM &out) const override;
    void eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                            const Vec &lambda_i, SpMatRM &out) const override;
    const Vec &lower() const override;
    const Vec &upper() const override;
    Vec start_point() const override;

    /// @brief The rows of a full-size vector or matrix that this model keeps.
    Vec restrict(const Vec &full) const;
    SpMatRM restrict(const SpMatRM &full) const;

  private:
    const NlpModel &model_;
    std::vector<Index> rows_;
};

} // namespace hven::solvers::detail
