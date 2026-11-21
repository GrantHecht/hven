// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The dense arm behind SymmetricFactor: LAPACK Bunch-Kaufman (dsytrf/dsytrs,
// uplo 'U'; rook, dsytrf_rook/dsytrs_rook, under DenseSingularPolicy::kReport)
// on the densified upper triangle, selected per analyzed pattern by
// SymmetricFactor::Options::dense_factorization. Internal: the platform
// adapters (symmetric_factor_mkl.cpp, symmetric_factor_accelerate.cpp) drive
// it; no public header includes this one.
//
// Every buffer is sized at analyze(); factorize() and solve() allocate
// nothing. The epoch is proposed by the adapter (engine-wide, monotone across
// the dense arm and the platform fallback session) and committed by the
// session as the larger of that proposal and its own epoch + 1, so a session
// that share()/adopt() co-owners advance in turn never rewinds and never
// repeats an epoch: every (session_id, epoch) names one set of numerics.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

#include <hven/core/types.h>
#include <hven/linear/symmetric_factor.h>

namespace hven::linear::detail {

// Throws std::invalid_argument unless n >= 1, n fits LAPACK's integer, and
// 8*n*n bytes fit both Index and std::size_t. Checked by division, so the
// check cannot itself overflow.
void validate_dense_dimension(Index n);

// The inertia read off a completed dsytrf factorization (uplo 'U'): a 1x1
// block d counts by its sign; a 2x2 block by its determinant and trace
// (decode_2x2). `singular` is set on a zero or non-finite 1x1 pivot, a 2x2
// block whose computed determinant is exactly 0.0, or a non-finite entry; the
// counts are then meaningless. Throws std::logic_error on malformed pivot
// data. `a` is column-major with leading dimension n.
struct DenseInertiaCount {
    Index positive = 0;
    Index negative = 0;
    bool singular = false;
};
DenseInertiaCount dense_block_walk(const double *a, Index n, const int *ipiv);

// The rook block walk (dsytrf_rook, uplo 'U'): inertia as dense_block_walk,
// a 2x2 block marked by two negative ipiv entries, plus the D-ratio
// min|lambda(D)| / max|lambda(D)| over the blocks (0 when singular).
struct DenseRookCount {
    DenseInertiaCount counts;
    double d_ratio = 0.0;
};
DenseRookCount dense_rook_walk(const double *a, Index n, const int *ipiv);

// DenseSingularPolicy::kReport's detector. A factorization whose rook
// D-ratio is below kDenseSingularScreen gets a dsycon_rook estimate; below
// kDenseConditionSingular (LAPACK xSYSVX's "singular to working precision",
// 2^-53) or NaN, an UNSHIFTED factorization is judged again on its
// equilibrated matrix, D A D with D = ruiz_symmetric_scale(A): flagged iff
// that rcond is below 2^-53 too (or NaN). Row and column scaling alone never
// flag. A shifted factorization is already equilibrated: its one estimate
// decides.
inline constexpr double kDenseSingularScreen = 1e-15;
inline constexpr double kDenseConditionSingular = 0.5 * std::numeric_limits<double>::epsilon();

// Symmetric Ruiz equilibration of an upper-CSR A (infinity norm), the one
// scale both the detector and the KKT layer's shift response
// (hven::detail::equilibration_scale) use: d = 1, then at most 8 sweeps of
// d_i /= sqrt(r_i), r_i = max_j |A_ij| d_i d_j (rows with r_i zero or
// non-finite keep d_i), stopping once max|1 - r_i| <= 0.05. `d` and `r` hold
// A.rows() entries each; `r` is scratch.
void ruiz_symmetric_scale(const SpMatRM &A, double *d, double *r);

// Higham's 1-norm estimator (LAPACK xLACN2's algorithm; Higham, ACM TOMS 14
// (1988); Accuracy and Stability of Numerical Algorithms, 2nd ed., Alg. 15.4)
// for a SYMMETRIC B of order n, given apply(x): x <- B x in place. A lower
// bound on ||B||_1, almost always within a factor 3. `x` and `v` hold n
// doubles, `sgn` n ints; all are scratch.
template <typename Apply>
double estimate_symmetric_one_norm(Index n, Apply &&apply, double *x, double *v, int *sgn);

// Every MKL LAPACK call the dense arm makes runs at this thread count, under
// every dense policy (threaded dsytrf inertia is not reproducible on undecidable
// matrices). Accelerate runs at its default threading. Options::threads is stored (num_threads())
// and configures the platform session only.
inline constexpr int kDenseArmThreads = 1;

// What DenseSession::factorize() did with a matrix.
enum class DenseFactorizeResult {
    kTaken,              // numerics committed
    kSingular,           // exactly singular: a zero pivot, a zero 2x2 det, a non-finite D
    kNumericallySingular // kReport: numerics committed, flagged by the detector
};

// The equilibrated shift: the session factors D A D + sigma S,
// D = diag(scale) (dim entries, positive), S = +1 on the first n_primal
// diagonals and -1 on the rest; solves then answer A-space systems.
struct DenseShift {
    Index n_primal = 0;
    const double *scale = nullptr;
    double sigma = 0.0;
};

class DenseSession {
  public:
    // `opts` is stored whole: adopt() takes an adopter's Options from it, and
    // a fallback session is built from it.
    DenseSession(const SymmetricFactor::Options &opts, std::uint64_t initial_epoch);

    DenseSession(const DenseSession &) = delete;
    DenseSession &operator=(const DenseSession &) = delete;

    // Sizes and zeroes every buffer for A's pattern (A is a validated upper
    // CSR). Throws std::invalid_argument per validate_dense_dimension().
    void analyze(const SpMatRM &A);

    // Densify A (equilibrated and shifted per `shift` when given; the scale is
    // kept for solve() until the next analyze()/factorize()) and factorize: Bunch-Kaufman under
    // kSparseFallback, rook under kReport. Returns kTaken and commits
    // max(epoch() + 1, next_epoch) when the dense arm takes the factorization
    // (an engine on its fallback session while a co-owner advanced this one
    // proposes a `next_epoch` at or below epoch()); kNumericallySingular, with
    // numerics committed the same way, when kReport's detector flags it;
    // kSingular, with no numerics, when it is exactly singular (the walk's
    // `singular`, or info > 0). Throws std::invalid_argument when A's dimension
    // or entry count is not the analyzed pattern's, hven::BackendError on an
    // illegal LAPACK argument.
    DenseFactorizeResult factorize(const SpMatRM &A, std::uint64_t next_epoch,
                                   const DenseShift *shift = nullptr);

    // In-place solve of nrhs columns of b (leading dimension ldb >= dim());
    // under an equilibrated shift b <- D (D A D + sigma S)^-1 D b.
    // Requires has_numerics(). Throws hven::BackendError on a LAPACK error.
    void solve(double *b, Index nrhs, Index ldb) const;

    // Forget the current numerics (the epoch stays): solves then throw until a
    // factorize() commits again.
    void drop_numerics() noexcept;

    Index dim() const noexcept { return n_; }
    bool has_numerics() const noexcept { return has_numerics_; }
    std::uint64_t epoch() const noexcept { return epoch_; }
    std::uint64_t session_id() const noexcept { return session_id_; }
    Index positive() const noexcept { return positive_; }
    Index negative() const noexcept { return negative_; }
    // Of the current numerics: kReport's flag, and the diagonals a DenseShift
    // moved (0 without one).
    bool numerically_singular() const noexcept { return numerically_singular_; }
    Index shifted_pivots() const noexcept { return shifted_pivots_; }
    // dsycon_rook calls made (kReport's screened factorizations).
    Index condition_estimates() const noexcept { return condition_estimates_; }
    // Equilibrated re-estimates made (unshifted factorizations whose raw
    // estimate fell below kDenseConditionSingular).
    Index equilibrated_estimates() const noexcept { return equilibrated_estimates_; }
    // n(n+1)/2: the entries of L and D stored.
    Index factor_entries() const noexcept { return n_ * (n_ + 1) / 2; }

    const SymmetricFactor::Options &options() const noexcept { return opts_; }
    // The configured count, stored for the platform session; the dense arm's own
    // MKL calls run at kDenseArmThreads.
    int num_threads() const noexcept { return opts_.threads; }
    void set_num_threads(int num_threads) noexcept { opts_.threads = num_threads; }

  private:
    bool report() const noexcept {
        return opts_.dense_singular_policy == DenseSingularPolicy::kReport;
    }
    // ||A||_1 from A's stored upper triangle (kReport, unshifted).
    double csr_one_norm(const SpMatRM &A);
    // ||.||_1 of the densified upper triangle in a_ (before dsytrf).
    double dense_one_norm();
    // kReport's dsycon_rook estimate of the factorization in a_.
    double rcond_estimate(double anorm);
    // kReport's estimate of rcond_1(D A D), D = ruiz_symmetric_scale(A), from
    // the factorization of the unshifted A in a_: ||D A D||_1 from A's CSR and
    // ||(D A D)^-1||_1 = ||D^-1 A^-1 D^-1||_1 by estimate_symmetric_one_norm
    // through dsytrs_rook. The numerics are not touched.
    double equilibrated_rcond_estimate(const SpMatRM &A);

    SymmetricFactor::Options opts_;
    Index n_ = 0;
    std::vector<double> a_;      // n x n, column-major; dsytrf's factors
    std::vector<int> ipiv_;      // n
    std::vector<double> work_;   // dsytrf's optimal workspace
    std::vector<Index> scatter_; // one dense offset per stored CSR entry
    // kReport only (empty otherwise): column sums for ||A||_1, dsycon_rook's
    // 2n workspace and n integers.
    std::vector<double> colsum_;
    std::vector<double> con_work_;
    std::vector<int> con_iwork_;
    // kReport only (empty otherwise): the equilibrated re-estimate's scale,
    // Ruiz scratch, the estimator's x and v (n each, in that order), and its
    // n sign integers.
    std::vector<double> eq_work_;
    std::vector<int> eq_sgn_;
    std::vector<double> scale_; // the current numerics' equilibration (empty: none)
    bool has_numerics_ = false;
    Index positive_ = 0;
    Index negative_ = 0;
    bool numerically_singular_ = false;
    Index shifted_pivots_ = 0;
    Index condition_estimates_ = 0;
    Index equilibrated_estimates_ = 0;
    std::uint64_t epoch_ = 0;
    std::uint64_t session_id_ = 0;
};

// The adapters' shared readings of a dense session.
InertiaEvidence dense_evidence_of(const DenseSession &session);
FactorEvidence dense_factor_evidence_of(const DenseSession &session);

// Full solves against a dense session's current numerics, with the same
// boundary validation as the platform paths. SolveInfo::refinement_steps is
// absent: the dense arm has no refinement.
SolveInfo run_dense_solve(const DenseSession &session, ConstMatRef RHS, MatRef X, const char *what);
SolveInfo run_dense_solve(const DenseSession &session, ConstVecRef rhs, VecRef x, const char *what);

// Throws std::invalid_argument for the dense-policy option conflicts that do
// not depend on the backend: kAuto without a threshold >= 1, a threshold
// under kNever/kAlways, and conditional_reproducibility_threads > 0 with a
// policy other than kNever.
void validate_dense_options(const SymmetricFactor::Options &opts);

// Whether a pattern of dimension n takes the dense arm under `opts`.
bool dense_selected(const SymmetricFactor::Options &opts, Index n) noexcept;

template <typename Apply>
double estimate_symmetric_one_norm(Index n, Apply &&apply, double *x, double *v, int *sgn) {
    constexpr int kMaxIter = 5; // xLACN2's ITMAX
    const auto sum_abs = [n](const double *p) {
        double s = 0.0;
        for (Index i = 0; i < n; ++i) {
            s += std::abs(p[i]);
        }
        return s;
    };
    const auto argmax_abs = [n](const double *p) {
        Index j = 0;
        for (Index i = 1; i < n; ++i) {
            if (std::abs(p[i]) > std::abs(p[j])) {
                j = i;
            }
        }
        return j;
    };
    const auto sign_of = [](double t) { return t >= 0.0 ? 1 : -1; };
    for (Index i = 0; i < n; ++i) {
        x[i] = 1.0 / static_cast<double>(n);
    }
    apply(x);
    if (n == 1) {
        return std::abs(x[0]);
    }
    double est = sum_abs(x);
    for (Index i = 0; i < n; ++i) {
        sgn[i] = sign_of(x[i]);
        x[i] = static_cast<double>(sgn[i]);
    }
    apply(x); // B^T = B
    Index j = argmax_abs(x);
    for (int iter = 2;; ++iter) {
        for (Index i = 0; i < n; ++i) {
            x[i] = 0.0;
        }
        x[j] = 1.0;
        apply(x);
        for (Index i = 0; i < n; ++i) {
            v[i] = x[i];
        }
        const double est_old = est;
        est = sum_abs(v);
        bool repeated = true;
        for (Index i = 0; i < n && repeated; ++i) {
            repeated = sign_of(x[i]) == sgn[i];
        }
        // A repeated sign vector, or no growth (NaN included): converged. On no
        // growth the larger of the two estimates is kept (each is ||B x||_1 for a
        // unit-1-norm x, so both are lower bounds); a NaN propagates.
        if (repeated) {
            break;
        }
        if (!(est > est_old)) {
            est = std::isnan(est) ? est : std::max(est, est_old);
            break;
        }
        for (Index i = 0; i < n; ++i) {
            sgn[i] = sign_of(x[i]);
            x[i] = static_cast<double>(sgn[i]);
        }
        apply(x);
        const Index j_last = j;
        j = argmax_abs(x);
        if (!(x[j_last] != std::abs(x[j]) && iter < kMaxIter)) {
            break;
        }
    }
    // The alternating-sign probe guards against the estimator's known failure
    // cases (Higham 1988, section 4).
    double alt = 1.0;
    for (Index i = 0; i < n; ++i) {
        x[i] = alt * (1.0 + static_cast<double>(i) / static_cast<double>(n - 1));
        alt = -alt;
    }
    apply(x);
    const double probe = 2.0 * sum_abs(x) / (3.0 * static_cast<double>(n));
    return probe > est ? probe : est;
}

} // namespace hven::linear::detail
