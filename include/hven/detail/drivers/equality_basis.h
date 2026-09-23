// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// equality_basis.h — an independent row basis of the equality Jacobian.
//
// compute() eliminates the fixed columns S = {i : l_i == u_i}, equilibrates the
// rows on the free columns F, and splits them into a retained set B and a dropped
// set D with a_r(F) = sum_k C(k, r) a_k(F) for r in D. The rank decision is the
// hven::linear backend's inertia of the shifted equilibrated Gram matrix G - t I
// at two declared shifts (a certificate up to the factor's backward error; see
// factor_row_dependence() in the .cpp); D is chosen from a Rayleigh-Ritz null
// basis by pivoted Gram-Schmidt with a declared tie rule, the retained set is
// certified by a masked Gram factorization, and a split is accepted only when every
// dropped row reconstructs from B to tau_lo with |C~| <= coef_cap, every intermediate
// is finite, and the lift system K below factors with inertia (|B|, |D|, 0) on clean
// evidence and passes a probe solve; otherwise nothing is dropped (kUndecidable,
// refusal() says why). Rows empty on F are dropped with a zero coefficient column.
//
// AMENDMENT 1 (settler S1, 2026-09-23). n_neg(G - t_lo I) = 0 on clean evidence
// certifies lambda_min(G) >= t_lo (Weyl) and gives kFullRank even when the t_hi count
// is positive: a certificate with the narrower margin t_lo, not an estimate. Measured
// t_hi counts on the full-rank collocation chain: 0/0/2/3/3 at 400/700/1000/1500/2000
// intervals (t_lo counts 0). A chain WITH a real dependence then reads t_hi = k + 3
// against t_lo = k and is undecidable (registered E1b-L1; fails safe).
//
// With N = [-C; I] (rows B then D), Je(:,F)^T N = 0 and
//   lift(lambda_B)  = the minimum-norm lambda with Je(:,F)^T lambda = Je(B,F)^T lambda_B
//   fold(lambda)    = lambda(B) + C lambda(D)          (fold(lift(y)) == y)
//   reduce(qp)      : Ae <- Ae(B,:), be <- be(B) + t,  t = C (I + C^T C)^-1 gamma,
//                     gamma = N^T (be - Ae d),  d = qp.lower on S and 0 elsewhere.
// Every (I + C^T C)^-1 is a solve with the quasidefinite K = [I_B C; C^T -I_D]
// through the hven::linear backend, with up to two steps of explicit refinement and
// the a-posteriori forward-error bound ||x^ - x||_2 <= ||r^||_2 + gamma_{w+1}
// || |K| |x^| + |b| ||_2 (||K^-1||_2 <= 1), accepted iff <= lift_forward_tol ||b||_2.
// C is in the units of the Je it was computed from. With nothing dropped every
// operation is the identity.

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
    /// A lift or projection solve with K is accepted iff its forward-error bound is
    /// <= this * ||rhs||_2 (the header note).
    double lift_forward_tol = 1e-8;
    /// The certificate factorizations' static pivot perturbation exponent
    /// (SymmetricFactor::Options::pivot_perturb_exp; Pardiso iparm[9]). Any perturbed
    /// or zero pivot refuses the split, so this sets only how small a pivot is refused.
    int gram_pivot_perturb_exp = 13;
};

enum class EqualityBasisVerdict {
    kNotComputed, ///< compute() has not run; the basis is the identity.
    kFullRank,    ///< Nothing dropped; every retained pivot clears the gap.
    kReduced,     ///< At least one row dropped.
    kUndecidable, ///< A check failed (see EqualityBasisRefusal); nothing dropped.
};

/// @brief Why a split is kUndecidable.
enum class EqualityBasisRefusal {
    kNone,
    kSpectrum,    ///< No gap (the counts at the two shifts differ), the retained set
                  ///< failed its certificate, or a reconstruction or coefficient-cap failure.
    kNonFinite,   ///< A norm, scaled entry, coefficient or Gram value was not finite.
    kLift,        ///< K = [I C; C^T -I] failed to factor with inertia (|B|, |D|, 0) on
                  ///< clean evidence, or a probe solve failed its forward-error bound.
    kCertificate, ///< A certificate factorization's evidence was not clean: a backend
                  ///< error, an unobserved inertia, a zero or perturbed pivot, or a
                  ///< probe solve above its backward-error bound.
    kSubspace,    ///< The null-basis iteration did not confirm the certified count, or
                  ///< its dense block would exceed the size cap.
    kFill,        ///< The Gram matrix would be too dense to form.
};

/// @brief Where a dropped row's combination gap sits.
enum class EqualityGapBand {
    kNoise,        ///< |g| <= 10 eps (|w_r| + sum |C| |w_k|).
    kFeasible,     ///< |g| <= feas_tol (1 + ||C(:,r)||_1).
    kInconsistent, ///< Neither: no point satisfies the group to feas_tol.
    kNonFinite,    ///< The gap is not finite; it is evidence of nothing.
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
    EqualityBasisRefusal refusal() const;
    /// True iff at least one row is dropped.
    bool reduces() const;
    Index me() const;
    const std::vector<Index> &retained() const;
    const std::vector<Index> &dropped() const;
    /// |B| x |D|, column-major.
    const Eigen::SparseMatrix<double> &coefficients() const;
    double tau_lo() const;
    double tau_hi() const;
    /// sqrt(t_hi), or sqrt(t_lo) when full rank was certified only at t_lo: the certified
    /// floor (to the factor's backward error) on sigma_min of the retained equilibrated
    /// rows (+inf when no certificate was reached).
    double min_retained_pivot() const;
    /// Whether the last compute() entered the reducing path (n_neg(G - t_lo I) > 0 on
    /// clean evidence; the null-basis identification runs when it agrees with t_hi's).
    /// The name is the report's until its schema moves.
    bool ran_qr() const;
    /// Numeric factorizations of the Gram matrix the last compute() ran (1 on the fast path).
    Index gram_factorizations() const;
    /// n_neg(G - t_hi I) and n_neg(G - t_lo I) on clean evidence (-1 when not reached).
    Index deficiency_hi() const;
    Index deficiency_lo() const;
    /// The certificate's shifts t_lo and t_hi, eigenvalue units of G (NaN when not formed).
    double shift_lo() const;
    double shift_hi() const;
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

    /// @brief The row indices, column indices and value bytes of Je on dropped row j
    ///        and its support rows in B; equal bytes mean identical values.
    std::vector<unsigned char> group_bytes(const SpMatRM &Je, Index j) const;
    /// @brief The free columns Je's dropped row j and its support rows touch, increasing.
    std::vector<Index> group_free_columns(const SpMatRM &Je, Index j) const;
    /// @brief FNV-1a of group_bytes(Je, j).
    std::uint64_t group_hash(const SpMatRM &Je, Index j) const;

    /// @brief Restricts qp's equality block to B with the projected rhs (header).
    /// @return False, with qp untouched, when the projection solve fails its check.
    /// @throws std::invalid_argument When qp.me() or qp.n() disagree with the basis.
    bool reduce(QpProblem &qp) const;

    /// @brief t = C (I + C^T C)^-1 gamma, |B|, through K.
    /// @param forward_bound When non-null, receives the solve's forward-error bound.
    /// @return False when the solve fails its bound or is not finite.
    bool project_rhs(const Vec &gamma, Vec &t, double *forward_bound = nullptr) const;
    /// @brief |B| -> me, minimum norm (header), through K; lambda(B) = lambda_B - C w.
    /// @param forward_bound When non-null, receives the K solve's forward-error bound.
    /// @return False when the solve fails its bound; `out` is then the zero-scatter
    ///         lift (lambda_B on B, 0 on D), which satisfies the same identity.
    bool lift(const Vec &lambda_B, Vec &out, double *forward_bound = nullptr) const;
    /// @brief me -> |B| (header).
    Vec fold(const Vec &lambda) const;

    /// @brief be(B) + t for a full-size rhs `be` of the system Ae d = be whose fixed
    ///        columns sit at d = `lower` (the same map reduce() applies).
    /// @return False when project_rhs() does.
    bool reduce_rhs(const Vec &be, const SpMatRM &Ae, const Vec &lower, Vec &out) const;

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
