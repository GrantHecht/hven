// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include <hven/detail/drivers/equality_basis.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <Eigen/Dense>
#include <fmt/format.h>

#include <hven/linear/symmetric_factor.h>

namespace hven::solvers::detail {

namespace {

constexpr double kEps = std::numeric_limits<double>::epsilon();
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();
// The certificate's shift floor: t_lo >= this * ||G||_inf (see factor_row_dependence()).
constexpr double kShiftFloor = 1e-10;
// A certificate factorization's probe solve: normwise backward error <= this * m * eps.
constexpr double kProbeBackwardFactor = 100.0;
// The null-basis iteration: oversampling, the step range, and the largest m x width block.
constexpr Index kOversample = 8;
constexpr int kSubspaceMinSteps = 2;
constexpr int kSubspaceMaxSteps = 4;
constexpr double kSubspaceCap = 2.5e7;
// The selection's near-tie band: candidates within (1 - this) of the largest remaining norm.
constexpr double kTieRatio = 1e-6;
// The fill guard: sum_f c_f (c_f + 1) / 2 <= max(this factor * nnz(M), the floor).
constexpr double kFillFactor = 8.0;
constexpr double kFillFloor = 1048576.0;
// Refinement steps of the normal-equations coefficients.
constexpr int kRefineSteps = 2;
// Explicit refinement steps of a lift or projection solve with K.
constexpr int kLiftRefineSteps = 2;
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

void fnv_mix(std::uint64_t &h, const void *data, std::size_t len) {
    const auto *bytes = static_cast<const unsigned char *>(data);
    for (std::size_t i = 0; i < len; ++i) {
        h ^= bytes[i];
        h *= kFnvPrime;
    }
}

using SpMatCM = Eigen::SparseMatrix<double>;

// sqrt(sum v_i^2) formed as amax * sqrt(sum (v_i / amax)^2): no overflow or
// underflow for finite entries; NaN when an entry is not finite.
template <class Values> double scaled_norm(const Values &v) {
    double amax = 0.0;
    for (Index i = 0; i < static_cast<Index>(v.size()); ++i) {
        if (!std::isfinite(v(i))) {
            return kNaN;
        }
        amax = std::max(amax, std::abs(v(i)));
    }
    if (amax == 0.0) {
        return 0.0;
    }
    double s = 0.0;
    for (Index i = 0; i < static_cast<Index>(v.size()); ++i) {
        const double q = v(i) / amax;
        s += q * q;
    }
    return amax * std::sqrt(s);
}

// The seam's answer (factor_row_dependence).
struct RowDependence {
    bool undecidable = false;
    EqualityBasisRefusal refusal = EqualityBasisRefusal::kNone; // why, when undecidable
    std::vector<Index> retained;                                // columns of M, increasing
    std::vector<Index> dropped;                                 // columns of M, increasing
    Eigen::MatrixXd coef;    // |retained| x |dropped|, equilibrated
    double min_pivot = kInf; // sqrt(t_hi): the certified floor on retained sigma
    double max_residual = 0.0;
    double max_coef = 0.0;
    double shift_lo = kNaN;
    double shift_hi = kNaN;
    Index k_hi = -1;
    Index k_lo = -1;
    Index gram_factorizations = 0;
};

// A deterministic uniform value in [-1, 1] of (i, j): splitmix64, identical on
// every platform and standard library.
double hash_uniform(std::uint64_t i, std::uint64_t j) {
    std::uint64_t z = i * 0x9E3779B97F4A7C15ULL + j + 0x632BE59BD9B4E019ULL;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    z ^= z >> 31;
    return static_cast<double>(z >> 11) * (2.0 / 9007199254740992.0) - 1.0;
}

// The equilibrated Gram G = M^T M and the shifted, optionally masked, operators
// the backend factors on one analyzed pattern:
//   A(t)      = G - t I
//   A_D(t)    = G with row and column j in D replaced by e_j, minus t I on B only.
class GramOperator {
  public:
    explicit GramOperator(const SpMatCM &M) : m_(M.cols()) {
        full_ = SpMatCM(M.transpose()) * M;
        full_.makeCompressed();
        upper_ = full_.triangularView<Eigen::Upper>();
        upper_.makeCompressed();
        base_.assign(upper_.valuePtr(), upper_.valuePtr() + upper_.nonZeros());
        row_of_.resize(static_cast<std::size_t>(upper_.nonZeros()));
        for (Index r = 0; r < m_; ++r) {
            for (Index e = upper_.outerIndexPtr()[r]; e < upper_.outerIndexPtr()[r + 1]; ++e) {
                row_of_[static_cast<std::size_t>(e)] = r;
            }
        }
        omega_ = 0.0;
        Vec rs = Vec::Zero(m_);
        for (Index c = 0; c < m_; ++c) {
            for (SpMatCM::InnerIterator it(full_, c); it; ++it) {
                rs(it.row()) += std::abs(it.value());
            }
        }
        omega_ = m_ > 0 ? rs.maxCoeff() : 0.0;
    }

    // Every diagonal entry is in the pattern (unit-norm columns give G_jj > 0).
    bool diagonal_present() const {
        for (Index r = 0; r < m_; ++r) {
            const Index e = upper_.outerIndexPtr()[r];
            if (e >= upper_.outerIndexPtr()[r + 1] || upper_.innerIndexPtr()[e] != r) {
                return false;
            }
        }
        return true;
    }

    double omega() const { return omega_; }
    const SpMatRM &upper() const { return upper_; }

    // Writes A(t) (mask empty) or A_D(t) into upper_'s values; returns ||A||_inf.
    double set(double t, const std::vector<char> &mask) {
        const bool masked = !mask.empty();
        Vec rs = Vec::Zero(m_);
        for (Index e = 0; e < upper_.nonZeros(); ++e) {
            const Index i = row_of_[static_cast<std::size_t>(e)];
            const Index j = upper_.innerIndexPtr()[e];
            double v = base_[static_cast<std::size_t>(e)];
            if (masked && (mask[static_cast<std::size_t>(i)] != 0 ||
                           mask[static_cast<std::size_t>(j)] != 0)) {
                v = i == j ? 1.0 : 0.0;
            } else if (i == j) {
                v -= t;
            }
            upper_.valuePtr()[e] = v;
            rs(i) += std::abs(v);
            if (i != j) {
                rs(j) += std::abs(v);
            }
        }
        return m_ > 0 ? rs.maxCoeff() : 0.0;
    }

    // y = A(t) x or A_D(t) x, formed from the full Gram.
    Vec apply(const Vec &x, double t, const std::vector<char> &mask) const {
        if (mask.empty()) {
            return Vec(full_ * x - t * x);
        }
        Vec xb = x;
        for (Index j = 0; j < m_; ++j) {
            if (mask[static_cast<std::size_t>(j)] != 0) {
                xb(j) = 0.0;
            }
        }
        Vec y = full_ * xb - t * xb;
        for (Index j = 0; j < m_; ++j) {
            if (mask[static_cast<std::size_t>(j)] != 0) {
                y(j) = x(j);
            }
        }
        return y;
    }

  private:
    Index m_;
    SpMatCM full_;
    SpMatRM upper_;
    std::vector<double> base_;
    std::vector<Index> row_of_;
    double omega_ = 0.0;
};

// One certificate factorization: its inertia is used only on CLEAN evidence
// (status kOk, inertia observed, no zero pivot, no perturbed pivot where the
// backend counts them, and a probe solve at the backward-error bound). Returns
// n_neg, or -1 when the evidence is not clean.
Index certified_negatives(linear::SymmetricFactor &factor, GramOperator &gram, double t,
                          const std::vector<char> &mask, Index &factorizations) {
    const Index m = gram.upper().rows();
    const double anorm = gram.set(t, mask);
    linear::FactorizeOutcome out;
    try {
        out = factor.factorize(gram.upper());
    } catch (const std::runtime_error &) {
        return -1;
    }
    ++factorizations;
    const linear::InertiaEvidence &in = out.inertia;
    if (out.status != linear::FactorizeOutcome::Status::kOk ||
        in.state != linear::InertiaEvidence::State::kObserved || in.n_zero != 0 || in.n_pos < 0 ||
        in.n_neg < 0 || in.n_pos + in.n_neg != m ||
        (in.perturbed_pivots.has_value() && *in.perturbed_pivots != 0)) {
        return -1;
    }
    Vec b(m);
    for (Index i = 0; i < m; ++i) {
        b(i) = 1.0 + 0.5 * hash_uniform(static_cast<std::uint64_t>(i), 0x5EED);
    }
    Vec x = Vec::Zero(m);
    try {
        factor.solve(b, x);
    } catch (const std::runtime_error &) {
        return -1;
    }
    const Vec r = gram.apply(x, t, mask) - b;
    const double rn = r.lpNorm<Eigen::Infinity>();
    const double scale = anorm * x.lpNorm<Eigen::Infinity>() + b.lpNorm<Eigen::Infinity>();
    if (!x.allFinite() || !std::isfinite(rn) ||
        rn > kProbeBackwardFactor * static_cast<double>(m) * kEps * scale) {
        return -1;
    }
    return in.n_neg;
}

// THE FACTORIZATION SEAM: the only place the split's linear algebra lives.
// M is n_free x m_ne with unit-norm columns (the equilibrated nonempty rows'
// transposes). On success, M(:, dropped) = M(:, retained) coef; `undecidable`
// means drop nothing. Production linear algebra: every sparse factorization is
// the hven::linear backend's; dense Eigen acts only on the m x (k + 8) blocks
// derived from it and can only confirm k or refuse.
//
// 1. THE CERTIFICATE. G = M^T M (unit diagonal, PSD). The backend factors
//    G - t I on one analyze. By Sylvester's law of inertia and Weyl's
//    inequality, n_neg(G - t I) counts the eigenvalues of G below t to within
//    the factor's backward error eta. t_lo = max(pivot_factor m eps, 1e-10) ||G||_inf
//    keeps t_lo above eta; t_hi = gap_ratio t_lo. n_neg = 0 at t_hi on clean
//    evidence is the full-rank certificate and the fast path (ONE factorization);
//    otherwise n_neg = 0 at t_lo certifies full rank with the smaller margin t_lo
//    (nothing is dropped, so no gap is needed), and a count k > 0 must agree at t_lo
//    (a gap), or the split is undecidable. G - t_lo I factors without growth although indefinite:
//    the last pivot of a dependent group is ~ -t_lo (1 + ||c||^2), far above roundoff and the
//    perturbation threshold, so its update is O(t_lo).
// 2. THE NULL BASIS. Block inverse iteration with (G - t_lo I)^-1 from a
//    hash-seeded start of width k + 8, orthonormalized each step; Rayleigh-Ritz on
//    (MQ)^T (MQ) must show exactly k Ritz values <= 2 t_lo and the next >= t_hi / 2.
// 3. THE CHOICE OF D. Column-pivoted Gram-Schmidt on N^T; the candidates of a step
//    are the columns within (1 - 1e-6) of the largest remaining norm, and the
//    HIGHEST row index wins (rotation-invariant, so independent of the basis the
//    iteration returned and of the backend).
// 4. THE RETAINED SET. The masked Gram A_D(t_hi) must show n_neg = 0 on clean
//    evidence (G_BB has the gap). C~ solves G_BB C~ = M_B^T M_D on A_D(0) with two
//    steps of refinement against residuals on M.
// 5. THE ARBITER. E1's reconstruction test on M: every dropped column must satisfy
//    ||M_D(:,j) - M_B C~(:,j)|| <= tau_lo (1 + ||c~_j||_1) and ||c~_j||_inf <= coef_cap,
//    all or nothing. The Gram's squared conditioning costs decidability only.
// Every doubt (backend error, unclean evidence, count mismatch, unconverged
// subspace, a bad retained set, a dense Gram) refuses: nothing is dropped.
RowDependence factor_row_dependence(const SpMatCM &M, double tau_lo, double coef_cap,
                                    const EqualityBasisTolerances &tol) {
    RowDependence out;
    const Index n_free = M.rows();
    const Index m = M.cols();
    auto refuse = [&](EqualityBasisRefusal why) {
        out.undecidable = true;
        out.refusal = why;
        out.retained.clear();
        out.dropped.clear();
        out.coef.resize(0, 0);
        out.min_pivot = kInf;
        out.max_residual = 0.0;
        out.max_coef = 0.0;
        return out;
    };
    if (m == 0) {
        return out;
    }
    if (n_free == 0) {
        // No free column: every nonempty row is empty on F. Unreachable by
        // construction (nu > 0 needs a free entry); kept for completeness.
        return refuse(EqualityBasisRefusal::kSpectrum);
    }

    // Fill guard before any numeric work: sum_f c_f (c_f + 1) / 2 bounds nnz(triu G).
    {
        std::vector<Index> count(static_cast<std::size_t>(n_free), 0);
        for (Index c = 0; c < m; ++c) {
            for (SpMatCM::InnerIterator it(M, c); it; ++it) {
                ++count[static_cast<std::size_t>(it.row())];
            }
        }
        double bound = 0.0;
        for (Index c : count) {
            bound += 0.5 * static_cast<double>(c) * static_cast<double>(c + 1);
        }
        const double limit = std::max(kFillFactor * static_cast<double>(M.nonZeros()), kFillFloor);
        if (bound > limit) {
            return refuse(EqualityBasisRefusal::kFill);
        }
    }

    GramOperator gram(M);
    if (!std::isfinite(gram.omega()) || !(gram.omega() > 0.0)) {
        return refuse(EqualityBasisRefusal::kNonFinite);
    }
    if (!gram.diagonal_present()) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    const double t_lo =
        std::max(tol.pivot_factor * static_cast<double>(m) * kEps, kShiftFloor) * gram.omega();
    const double t_hi = tol.gap_ratio * t_lo;
    out.shift_lo = t_lo;
    out.shift_hi = t_hi;

    linear::SymmetricFactor::Options opts;
    opts.num_threads = 1;
    opts.pivot_perturb_exp = tol.gram_pivot_perturb_exp;
    linear::SymmetricFactor factor(opts);
    try {
        factor.analyze(gram.upper());
    } catch (const std::runtime_error &) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    const std::vector<char> no_mask;

    // 1. The certificate.
    const Index k_hi = certified_negatives(factor, gram, t_hi, no_mask, out.gram_factorizations);
    out.k_hi = k_hi;
    if (k_hi < 0) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    if (k_hi == 0) {
        out.min_pivot = std::sqrt(t_hi);
        for (Index j = 0; j < m; ++j) {
            out.retained.push_back(j);
        }
        return out;
    }
    const Index k_lo = certified_negatives(factor, gram, t_lo, no_mask, out.gram_factorizations);
    out.k_lo = k_lo;
    if (k_lo < 0) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    if (k_lo == 0) {
        // Full rank at the lower shift: lambda_min(G) >= t_lo - eta. Nothing is dropped,
        // so no gap is needed (E1b deviation from the cleared design, declared: a long
        // full-rank collocation chain has eigenvalues in [t_lo, t_hi)).
        out.min_pivot = std::sqrt(t_lo);
        for (Index j = 0; j < m; ++j) {
            out.retained.push_back(j);
        }
        return out;
    }
    if (k_lo != k_hi || k_lo >= m) {
        return refuse(EqualityBasisRefusal::kSpectrum);
    }
    const Index k = k_lo;

    // 2. The null basis.
    const Index width = std::min(k + kOversample, m);
    if (static_cast<double>(m) * static_cast<double>(width) > kSubspaceCap) {
        return refuse(EqualityBasisRefusal::kSubspace);
    }
    Eigen::MatrixXd Q(m, width);
    for (Index c = 0; c < width; ++c) {
        for (Index r = 0; r < m; ++r) {
            Q(r, c) = hash_uniform(static_cast<std::uint64_t>(r), static_cast<std::uint64_t>(c));
        }
    }
    // The factor holds G - t_lo I from the second certificate factorization.
    Eigen::MatrixXd N;
    bool converged = false;
    for (int step = 1; step <= kSubspaceMaxSteps && !converged; ++step) {
        Eigen::MatrixXd Y(m, width);
        try {
            factor.solve(Q, Y);
        } catch (const std::runtime_error &) {
            return refuse(EqualityBasisRefusal::kCertificate);
        }
        if (!Y.allFinite()) {
            return refuse(EqualityBasisRefusal::kSubspace);
        }
        Eigen::HouseholderQR<Eigen::MatrixXd> qr(Y);
        Q = qr.householderQ() * Eigen::MatrixXd::Identity(m, width);
        if (step < kSubspaceMinSteps) {
            continue;
        }
        const Eigen::MatrixXd MQ = M * Q;
        const Eigen::MatrixXd H = MQ.transpose() * MQ;
        Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> eig(H);
        if (eig.info() != Eigen::Success || !eig.eigenvalues().allFinite()) {
            return refuse(EqualityBasisRefusal::kSubspace);
        }
        const Vec &theta = eig.eigenvalues(); // ascending
        converged = theta(k - 1) <= 2.0 * t_lo && (width == k || theta(k) >= 0.5 * t_hi);
        if (converged) {
            N = Q * eig.eigenvectors().leftCols(k);
        }
    }
    if (!converged) {
        return refuse(EqualityBasisRefusal::kSubspace);
    }

    // 3. The choice of D: pivoted Gram-Schmidt on N^T with the declared tie rule.
    Eigen::MatrixXd W = N.transpose(); // k x m
    std::vector<char> is_d(static_cast<std::size_t>(m), 0);
    for (Index step = 0; step < k; ++step) {
        double best = 0.0;
        Vec norms(m);
        for (Index j = 0; j < m; ++j) {
            norms(j) = is_d[static_cast<std::size_t>(j)] != 0 ? -1.0 : W.col(j).norm();
            best = std::max(best, norms(j));
        }
        if (!std::isfinite(best) || !(best > 0.0)) {
            return refuse(EqualityBasisRefusal::kSubspace);
        }
        Index pick = -1;
        for (Index j = m - 1; j >= 0; --j) {
            if (norms(j) >= (1.0 - kTieRatio) * best) {
                pick = j;
                break;
            }
        }
        is_d[static_cast<std::size_t>(pick)] = 1;
        const Vec q = W.col(pick) / norms(pick);
        W -= q * (q.transpose() * W);
    }
    for (Index j = 0; j < m; ++j) {
        (is_d[static_cast<std::size_t>(j)] != 0 ? out.dropped : out.retained).push_back(j);
    }

    // 4. The retained set's certificate, then the coefficients.
    const Index a_hi = certified_negatives(factor, gram, t_hi, is_d, out.gram_factorizations);
    if (a_hi < 0) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    if (a_hi != 0) {
        return refuse(EqualityBasisRefusal::kSpectrum);
    }
    const Index a_0 = certified_negatives(factor, gram, 0.0, is_d, out.gram_factorizations);
    if (a_0 < 0) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    if (a_0 != 0) {
        return refuse(EqualityBasisRefusal::kSpectrum);
    }
    const Index nd = k;
    Eigen::MatrixXd MD(n_free, nd);
    for (Index j = 0; j < nd; ++j) {
        MD.col(j) = M.col(out.dropped[static_cast<std::size_t>(j)]).toDense();
    }
    auto normal_solve = [&](const Eigen::MatrixXd &R, Eigen::MatrixXd &X) {
        Eigen::MatrixXd rhs = M.transpose() * R;
        for (Index d : out.dropped) {
            rhs.row(d).setZero();
        }
        X.resize(m, nd);
        factor.solve(rhs, X);
        for (Index d : out.dropped) {
            X.row(d).setZero();
        }
    };
    Eigen::MatrixXd X;
    Eigen::MatrixXd R = MD;
    try {
        normal_solve(R, X);
        for (int step = 0; step < kRefineSteps; ++step) {
            R = MD - M * X;
            Eigen::MatrixXd dX;
            normal_solve(R, dX);
            X += dX;
        }
    } catch (const std::runtime_error &) {
        return refuse(EqualityBasisRefusal::kCertificate);
    }
    R = MD - M * X;
    if (!X.allFinite() || !R.allFinite()) {
        return refuse(EqualityBasisRefusal::kNonFinite);
    }

    // 5. The arbiter.
    const Index nb = static_cast<Index>(out.retained.size());
    out.coef.resize(nb, nd);
    for (Index i = 0; i < nb; ++i) {
        out.coef.row(i) = X.row(out.retained[static_cast<std::size_t>(i)]);
    }
    for (Index j = 0; j < nd; ++j) {
        const double rn = scaled_norm(R.col(j));
        const double c1 = out.coef.col(j).lpNorm<1>();
        const double cinf = out.coef.col(j).lpNorm<Eigen::Infinity>();
        if (!std::isfinite(rn) || !std::isfinite(c1)) {
            return refuse(EqualityBasisRefusal::kNonFinite);
        }
        out.max_residual = std::max(out.max_residual, rn);
        out.max_coef = std::max(out.max_coef, cinf);
        if (rn > tau_lo * (1.0 + c1) || cinf > coef_cap) {
            return refuse(EqualityBasisRefusal::kSpectrum);
        }
    }
    out.min_pivot = std::sqrt(t_hi);
    return out;
}

} // namespace

struct EqualityBasis::Impl {
    EqualityBasisVerdict verdict = EqualityBasisVerdict::kNotComputed;
    EqualityBasisRefusal refusal = EqualityBasisRefusal::kNone;
    Index me = 0;
    Index n = 0;
    std::vector<Index> retained;
    std::vector<Index> dropped;
    std::vector<Index> fixed;   // S, increasing
    Vec fixed_lower;            // l on S
    std::vector<char> is_fixed; // n
    SpMatCM C;                  // |B| x |D|
    SpMatCM C_abs;              // |C|
    // The lift and projection system K = [I_B C; C^T -I_D] (rows B then D): its
    // upper triangle for the backend, the backend's factor, gamma_{w+1} of its
    // residual's rounding bound (w = K's largest row count), and the acceptance.
    SpMatRM k_upper;
    std::unique_ptr<linear::SymmetricFactor> k_factor;
    double k_gamma = 0.0;
    double lift_forward_tol = 0.0;
    double tau_lo = 0.0;
    double tau_hi = 0.0;
    double min_pivot = kInf;
    Index gram_factorizations = 0;
    Index k_hi = -1;
    Index k_lo = -1;
    double shift_lo = kNaN;
    double shift_hi = kNaN;
    double max_residual = 0.0;
    double max_coef = 0.0;
    std::uint64_t hash = 0;

    void set_identity(Index rows) {
        me = rows;
        retained.resize(static_cast<std::size_t>(rows));
        for (Index r = 0; r < rows; ++r) {
            retained[static_cast<std::size_t>(r)] = r;
        }
        dropped.clear();
        C = SpMatCM(rows, 0);
    }

    // The scaled two-norm of row r on F; NaN when an entry on F is not finite.
    double free_norm(const SpMatRM &Je, Index r) const {
        double amax = 0.0;
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            if (is_fixed[static_cast<std::size_t>(it.col())] == 0) {
                if (!std::isfinite(it.value())) {
                    return kNaN;
                }
                amax = std::max(amax, std::abs(it.value()));
            }
        }
        if (amax == 0.0) {
            return 0.0;
        }
        double s = 0.0;
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            if (is_fixed[static_cast<std::size_t>(it.col())] == 0) {
                const double q = it.value() / amax;
                s += q * q;
            }
        }
        return amax * std::sqrt(s);
    }

    // y = K x, or |K| |x| when `absolute`.
    Vec apply_k(const Vec &x, bool absolute) const {
        const Index nb = static_cast<Index>(retained.size());
        const Index nd = static_cast<Index>(dropped.size());
        Vec y(nb + nd);
        if (absolute) {
            const Vec ax = x.cwiseAbs();
            y.head(nb) = ax.head(nb) + C_abs * ax.tail(nd);
            y.tail(nd) = C_abs.transpose() * ax.head(nb) + ax.tail(nd);
        } else {
            y.head(nb) = x.head(nb) + C * x.tail(nd);
            y.tail(nd) = C.transpose() * x.head(nb) - x.tail(nd);
        }
        return y;
    }

    // K x = b through the backend's factor with up to kLiftRefineSteps steps of
    // explicit refinement. K^2 = diag(I + C C^T, I + C^T C) >= I, so ||K^-1||_2 <= 1 and
    //   ||x^ - x||_2 <= ||r^||_2 + rho,   rho = gamma_{w+1} || |K| |x^| + |b| ||_2,
    // where r^ = fl(b - K x^) and rho bounds the rounding in forming it. Accepted iff
    // that bound is <= lift_forward_tol ||b||_2; `bound` receives it (+inf on a
    // backend error or a non-finite value).
    bool solve_k(const Vec &b, Vec &x, double &bound) const {
        bound = kInf;
        const double bn = scaled_norm(b);
        if (!std::isfinite(bn) || k_factor == nullptr) {
            return false;
        }
        x = Vec::Zero(b.size());
        Vec dx = Vec::Zero(b.size());
        try {
            k_factor->solve(b, x);
            for (int step = 0;; ++step) {
                if (!x.allFinite()) {
                    return false;
                }
                const Vec r = b - apply_k(x, false);
                const Vec a = apply_k(x, true) + b.cwiseAbs();
                const double est = scaled_norm(r) + k_gamma * scaled_norm(a);
                if (!std::isfinite(est)) {
                    return false;
                }
                bound = est;
                if (bound <= lift_forward_tol * bn || step == kLiftRefineSteps) {
                    break;
                }
                dx.setZero();
                k_factor->solve(r, dx);
                x += dx;
            }
        } catch (const std::runtime_error &) {
            bound = kInf;
            return false;
        }
        return bound <= lift_forward_tol * bn;
    }
};

EqualityBasis::EqualityBasis() : impl_(std::make_unique<Impl>()) {}
EqualityBasis::~EqualityBasis() = default;
EqualityBasis::EqualityBasis(EqualityBasis &&) noexcept = default;
EqualityBasis &EqualityBasis::operator=(EqualityBasis &&) noexcept = default;

void EqualityBasis::reset(Index me) {
    impl_ = std::make_unique<Impl>();
    impl_->set_identity(me);
}

EqualityBasisVerdict EqualityBasis::verdict() const { return impl_->verdict; }
EqualityBasisRefusal EqualityBasis::refusal() const { return impl_->refusal; }
bool EqualityBasis::reduces() const { return !impl_->dropped.empty(); }
Index EqualityBasis::me() const { return impl_->me; }
const std::vector<Index> &EqualityBasis::retained() const { return impl_->retained; }
const std::vector<Index> &EqualityBasis::dropped() const { return impl_->dropped; }
const Eigen::SparseMatrix<double> &EqualityBasis::coefficients() const { return impl_->C; }
double EqualityBasis::tau_lo() const { return impl_->tau_lo; }
double EqualityBasis::tau_hi() const { return impl_->tau_hi; }
double EqualityBasis::min_retained_pivot() const { return impl_->min_pivot; }
Index EqualityBasis::gram_factorizations() const { return impl_->gram_factorizations; }
Index EqualityBasis::deficiency_hi() const { return impl_->k_hi; }
Index EqualityBasis::deficiency_lo() const { return impl_->k_lo; }
double EqualityBasis::shift_lo() const { return impl_->shift_lo; }
double EqualityBasis::shift_hi() const { return impl_->shift_hi; }
double EqualityBasis::max_backward_residual() const { return impl_->max_residual; }
double EqualityBasis::max_coefficient() const { return impl_->max_coef; }
std::uint64_t EqualityBasis::jacobian_hash() const { return impl_->hash; }

std::uint64_t EqualityBasis::hash_jacobian(const SpMatRM &Je_in) {
    SpMatRM tmp;
    const SpMatRM *p = &Je_in;
    if (!Je_in.isCompressed()) {
        tmp = Je_in;
        tmp.makeCompressed();
        p = &tmp;
    }
    const SpMatRM &m = *p;
    std::uint64_t h = kFnvOffset;
    const Index rows = m.rows();
    const Index cols = m.cols();
    const Index nnz = m.nonZeros();
    fnv_mix(h, &rows, sizeof(rows));
    fnv_mix(h, &cols, sizeof(cols));
    fnv_mix(h, &nnz, sizeof(nnz));
    fnv_mix(h, m.outerIndexPtr(), sizeof(int) * static_cast<std::size_t>(rows + 1));
    fnv_mix(h, m.innerIndexPtr(), sizeof(int) * static_cast<std::size_t>(nnz));
    fnv_mix(h, m.valuePtr(), sizeof(double) * static_cast<std::size_t>(nnz));
    return h;
}

void EqualityBasis::compute(const SpMatRM &Je, const Vec &lower, const Vec &upper,
                            const EqualityBasisTolerances &tol) {
    const Index me = Je.rows();
    const Index n = Je.cols();
    if (lower.size() != n || upper.size() != n) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::compute: Je is {}x{} but lower/upper have sizes {}/{}", me,
                        n, lower.size(), upper.size()));
    }
    if (!(tol.pivot_factor > 0.0) || !(tol.gap_ratio >= 1.0) || !(tol.coef_cap > 0.0) ||
        !(tol.lift_forward_tol > 0.0) || tol.gram_pivot_perturb_exp < 0) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::compute: tolerances must satisfy pivot_factor > 0, gap_ratio >= 1, "
            "coef_cap > 0, lift_forward_tol > 0, gram_pivot_perturb_exp >= 0 (got {}, {}, {}, "
            "{}, {})",
            tol.pivot_factor, tol.gap_ratio, tol.coef_cap, tol.lift_forward_tol,
            tol.gram_pivot_perturb_exp));
    }

    auto next = std::make_unique<Impl>();
    Impl &b = *next;
    b.n = n;
    b.hash = hash_jacobian(Je);
    b.is_fixed.assign(static_cast<std::size_t>(n), 0);
    std::vector<Index> free_pos(static_cast<std::size_t>(n), -1);
    Index n_free = 0;
    for (Index i = 0; i < n; ++i) {
        if (lower(i) == upper(i) && std::isfinite(lower(i))) {
            b.is_fixed[static_cast<std::size_t>(i)] = 1;
            b.fixed.push_back(i);
        } else {
            free_pos[static_cast<std::size_t>(i)] = n_free++;
        }
    }
    b.fixed_lower.resize(static_cast<Index>(b.fixed.size()));
    for (std::size_t s = 0; s < b.fixed.size(); ++s) {
        b.fixed_lower(static_cast<Index>(s)) = lower(b.fixed[s]);
    }
    b.tau_lo = tol.pivot_factor * static_cast<double>(n_free + me) * kEps;
    b.tau_hi = tol.gap_ratio * b.tau_lo;
    b.lift_forward_tol = tol.lift_forward_tol;
    b.set_identity(me);

    // Every refusal leaves the identity basis in force.
    auto refuse = [&](EqualityBasisRefusal why) {
        b.set_identity(me);
        b.verdict = EqualityBasisVerdict::kUndecidable;
        b.refusal = why;
        b.max_coef = 0.0;
        b.max_residual = 0.0;
        b.C_abs = SpMatCM();
        b.k_upper = SpMatRM();
        b.k_factor.reset();
        impl_ = std::move(next);
    };

    // Row norms on F; empty rows are dropped outright.
    Vec nu(me);
    std::vector<Index> nonempty;
    std::vector<Index> empty_rows;
    for (Index r = 0; r < me; ++r) {
        nu(r) = b.free_norm(Je, r);
        if (!std::isfinite(nu(r))) {
            refuse(EqualityBasisRefusal::kNonFinite);
            return;
        }
        if (nu(r) > 0.0) {
            nonempty.push_back(r);
        } else {
            empty_rows.push_back(r);
        }
    }
    const Index m_ne = static_cast<Index>(nonempty.size());

    // M = Je(nonempty, F)^T diag(1/nu), n_free x m_ne.
    SpMatCM M(n_free, m_ne);
    if (m_ne > 0) {
        std::vector<Eigen::Triplet<double>> trip;
        trip.reserve(static_cast<std::size_t>(Je.nonZeros()));
        for (Index j = 0; j < m_ne; ++j) {
            const Index r = nonempty[static_cast<std::size_t>(j)];
            for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
                const Index f = free_pos[static_cast<std::size_t>(it.col())];
                if (f >= 0 && it.value() != 0.0) {
                    const double v = it.value() / nu(r);
                    if (!std::isfinite(v)) {
                        refuse(EqualityBasisRefusal::kNonFinite);
                        return;
                    }
                    trip.emplace_back(static_cast<int>(f), static_cast<int>(j), v);
                }
            }
        }
        M.setFromTriplets(trip.begin(), trip.end());
        M.makeCompressed();
    }

    RowDependence dep = factor_row_dependence(M, b.tau_lo, tol.coef_cap, tol);
    b.gram_factorizations = dep.gram_factorizations;
    b.k_hi = dep.k_hi;
    b.k_lo = dep.k_lo;
    b.shift_lo = dep.shift_lo;
    b.shift_hi = dep.shift_hi;
    b.min_pivot = dep.min_pivot;
    b.max_residual = dep.max_residual;
    b.max_coef = dep.max_coef;
    const std::vector<Index> &dropped_ne = dep.dropped; // indices into `nonempty`
    const std::vector<Index> &retained_ne = dep.retained;
    const Eigen::MatrixXd &Ct = dep.coef;

    if (dep.undecidable) {
        refuse(dep.refusal);
        return;
    }

    // Assemble B (increasing), D (increasing) and C in Je's units.
    std::vector<Index> dropped_rows;
    for (Index j : dropped_ne) {
        dropped_rows.push_back(nonempty[static_cast<std::size_t>(j)]);
    }
    for (Index r : empty_rows) {
        dropped_rows.push_back(r);
    }
    if (dropped_rows.empty()) {
        b.verdict = EqualityBasisVerdict::kFullRank;
        impl_ = std::move(next);
        return;
    }
    std::vector<Index> order(dropped_rows.size());
    for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<Index>(i);
    }
    std::sort(order.begin(), order.end(), [&](Index a, Index c) {
        return dropped_rows[static_cast<std::size_t>(a)] <
               dropped_rows[static_cast<std::size_t>(c)];
    });
    std::vector<char> is_dropped(static_cast<std::size_t>(me), 0);
    for (Index r : dropped_rows) {
        is_dropped[static_cast<std::size_t>(r)] = 1;
    }
    b.retained.clear();
    std::vector<Index> b_pos(static_cast<std::size_t>(me), -1);
    for (Index r = 0; r < me; ++r) {
        if (is_dropped[static_cast<std::size_t>(r)] == 0) {
            b_pos[static_cast<std::size_t>(r)] = static_cast<Index>(b.retained.size());
            b.retained.push_back(r);
        }
    }
    const Index nb = static_cast<Index>(b.retained.size());
    const Index nd = static_cast<Index>(dropped_rows.size());
    b.dropped.resize(static_cast<std::size_t>(nd));
    std::vector<Eigen::Triplet<double>> trip;
    for (Index jj = 0; jj < nd; ++jj) {
        const Index src = order[static_cast<std::size_t>(jj)];
        const Index r = dropped_rows[static_cast<std::size_t>(src)];
        b.dropped[static_cast<std::size_t>(jj)] = r;
        if (src >= static_cast<Index>(dropped_ne.size())) {
            continue; // an empty row: zero column
        }
        for (Index k = 0; k < Ct.rows(); ++k) {
            const double ct = Ct(k, src);
            if (ct == 0.0) {
                continue;
            }
            const Index rk =
                nonempty[static_cast<std::size_t>(retained_ne[static_cast<std::size_t>(k)])];
            const double c = ct * (nu(r) / nu(rk));
            if (!std::isfinite(c)) {
                refuse(EqualityBasisRefusal::kNonFinite);
                return;
            }
            trip.emplace_back(static_cast<int>(b_pos[static_cast<std::size_t>(rk)]),
                              static_cast<int>(jj), c);
        }
    }
    b.C = SpMatCM(nb, nd);
    b.C.setFromTriplets(trip.begin(), trip.end());
    b.C.makeCompressed();

    for (Index k = 0; k < b.C.nonZeros(); ++k) {
        if (!std::isfinite(b.C.valuePtr()[k])) {
            refuse(EqualityBasisRefusal::kNonFinite);
            return;
        }
    }
    b.C_abs = b.C.cwiseAbs();

    // K = [I_B C; C^T -I_D] through the backend: symmetric quasidefinite, so its
    // inertia is exactly (|B|, |D|, 0) and every symmetric ordering factors it
    // stably. Clean evidence (as the certificate's) and that inertia, then a
    // probe solve through the same bound lift() and project_rhs() apply;
    // anything else refuses with kLift.
    {
        const Index nk = nb + nd;
        std::vector<Eigen::Triplet<double>> kt;
        kt.reserve(static_cast<std::size_t>(nk + b.C.nonZeros()));
        std::vector<Index> row_count(static_cast<std::size_t>(nk), 1);
        for (Index k = 0; k < nb; ++k) {
            kt.emplace_back(static_cast<int>(k), static_cast<int>(k), 1.0);
        }
        for (Index j = 0; j < nd; ++j) {
            kt.emplace_back(static_cast<int>(nb + j), static_cast<int>(nb + j), -1.0);
            for (SpMatCM::InnerIterator it(b.C, j); it; ++it) {
                kt.emplace_back(static_cast<int>(it.row()), static_cast<int>(nb + j), it.value());
                ++row_count[static_cast<std::size_t>(it.row())];
                ++row_count[static_cast<std::size_t>(nb + j)];
            }
        }
        b.k_upper = SpMatRM(nk, nk);
        b.k_upper.setFromTriplets(kt.begin(), kt.end());
        b.k_upper.makeCompressed();
        const Index w = *std::max_element(row_count.begin(), row_count.end());
        const double nu_w = static_cast<double>(w + 1) * (0.5 * kEps);
        b.k_gamma = nu_w / (1.0 - nu_w);
        linear::SymmetricFactor::Options kopts;
        kopts.num_threads = 1;
        b.k_factor = std::make_unique<linear::SymmetricFactor>(kopts);
        bool clean = false;
        try {
            b.k_factor->analyze(b.k_upper);
            const linear::FactorizeOutcome fo = b.k_factor->factorize(b.k_upper);
            const linear::InertiaEvidence &in = fo.inertia;
            clean = fo.status == linear::FactorizeOutcome::Status::kOk &&
                    in.state == linear::InertiaEvidence::State::kObserved && in.n_zero == 0 &&
                    in.n_pos == nb && in.n_neg == nd &&
                    (!in.perturbed_pivots.has_value() || *in.perturbed_pivots == 0);
        } catch (const std::runtime_error &) {
            clean = false;
        }
        Vec probe_b(nk);
        for (Index i = 0; i < nk; ++i) {
            probe_b(i) = 1.0 + 0.5 * hash_uniform(static_cast<std::uint64_t>(i), 0x11F7);
        }
        Vec probe_x;
        double probe_bound = kInf;
        if (!clean || !b.solve_k(probe_b, probe_x, probe_bound)) {
            refuse(EqualityBasisRefusal::kLift);
            return;
        }
    }
    b.verdict = EqualityBasisVerdict::kReduced;
    impl_ = std::move(next);
}

bool EqualityBasis::verify(const SpMatRM &Je) const {
    const Impl &b = *impl_;
    if (b.dropped.empty()) {
        return true;
    }
    if (Je.rows() != b.me || Je.cols() != b.n) {
        return false;
    }
    Vec acc = Vec::Zero(b.n);
    const Index nd = static_cast<Index>(b.dropped.size());
    for (Index j = 0; j < nd; ++j) {
        const Index r = b.dropped[static_cast<std::size_t>(j)];
        acc.setZero();
        double bound = b.free_norm(Je, r);
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            acc(it.col()) += it.value();
        }
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            const Index rk = b.retained[static_cast<std::size_t>(ct.row())];
            bound += std::abs(ct.value()) * b.free_norm(Je, rk);
            for (SpMatRM::InnerIterator it(Je, rk); it; ++it) {
                acc(it.col()) -= ct.value() * it.value();
            }
        }
        for (Index s : b.fixed) {
            acc(s) = 0.0;
        }
        const double res = scaled_norm(acc);
        if (!std::isfinite(res) || !std::isfinite(bound) || res > b.tau_lo * bound) {
            return false;
        }
    }
    return true;
}

void EqualityBasis::read_gaps(const Vec &ce, const SpMatRM &Je, const Vec &x, double feas_tol,
                              Vec &gap, std::vector<EqualityGapBand> &band) const {
    const Impl &b = *impl_;
    const Index nd = static_cast<Index>(b.dropped.size());
    gap.resize(nd);
    band.assign(static_cast<std::size_t>(nd), EqualityGapBand::kNoise);
    if (nd == 0) {
        return;
    }
    if (ce.size() != b.me || Je.rows() != b.me || Je.cols() != b.n || x.size() != b.n) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::read_gaps: ce {} / Je {}x{} / x {} disagree with the basis ({} x {})",
            ce.size(), Je.rows(), Je.cols(), x.size(), b.me, b.n));
    }
    Vec w = ce;
    if (!b.fixed.empty()) {
        Vec d = Vec::Zero(b.n);
        for (std::size_t s = 0; s < b.fixed.size(); ++s) {
            d(b.fixed[s]) = b.fixed_lower(static_cast<Index>(s)) - x(b.fixed[s]);
        }
        w += Je * d;
    }
    for (Index j = 0; j < nd; ++j) {
        const Index r = b.dropped[static_cast<std::size_t>(j)];
        double g = w(r);
        double scale = std::abs(w(r));
        double c1 = 0.0;
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            const double wk = w(b.retained[static_cast<std::size_t>(ct.row())]);
            g -= ct.value() * wk;
            scale += std::abs(ct.value()) * std::abs(wk);
            c1 += std::abs(ct.value());
        }
        gap(j) = g;
        const double ag = std::abs(g);
        if (!std::isfinite(ag) || !std::isfinite(scale) || !std::isfinite(c1)) {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kNonFinite;
        } else if (ag <= 10.0 * kEps * scale) {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kNoise;
        } else if (ag <= feas_tol * (1.0 + c1)) {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kFeasible;
        } else {
            band[static_cast<std::size_t>(j)] = EqualityGapBand::kInconsistent;
        }
    }
}

std::vector<unsigned char> EqualityBasis::group_bytes(const SpMatRM &Je, Index j) const {
    const Impl &b = *impl_;
    if (j < 0 || j >= static_cast<Index>(b.dropped.size()) || Je.rows() != b.me) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::group_bytes: dropped index {} out of range [0, {}) or Je "
                        "has {} rows (expected {})",
                        j, b.dropped.size(), Je.rows(), b.me));
    }
    std::vector<unsigned char> out;
    auto put = [&](const void *data, std::size_t len) {
        const auto *p = static_cast<const unsigned char *>(data);
        out.insert(out.end(), p, p + len);
    };
    auto put_row = [&](Index r) {
        put(&r, sizeof(r));
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            const Index c = it.col();
            const double v = it.value();
            put(&c, sizeof(c));
            put(&v, sizeof(v));
        }
    };
    put_row(b.dropped[static_cast<std::size_t>(j)]);
    for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
        put_row(b.retained[static_cast<std::size_t>(ct.row())]);
    }
    return out;
}

std::vector<Index> EqualityBasis::group_free_columns(const SpMatRM &Je, Index j) const {
    const Impl &b = *impl_;
    if (j < 0 || j >= static_cast<Index>(b.dropped.size()) || Je.rows() != b.me ||
        Je.cols() != b.n) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::group_free_columns: dropped index {} out of range [0, {}) or Je is "
            "{}x{} (expected {}x{})",
            j, b.dropped.size(), Je.rows(), Je.cols(), b.me, b.n));
    }
    std::vector<Index> cols;
    auto add_row = [&](Index r) {
        for (SpMatRM::InnerIterator it(Je, r); it; ++it) {
            if (b.is_fixed[static_cast<std::size_t>(it.col())] == 0) {
                cols.push_back(it.col());
            }
        }
    };
    add_row(b.dropped[static_cast<std::size_t>(j)]);
    for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
        add_row(b.retained[static_cast<std::size_t>(ct.row())]);
    }
    std::sort(cols.begin(), cols.end());
    cols.erase(std::unique(cols.begin(), cols.end()), cols.end());
    return cols;
}

std::uint64_t EqualityBasis::group_hash(const SpMatRM &Je, Index j) const {
    const std::vector<unsigned char> bytes = group_bytes(Je, j);
    std::uint64_t h = kFnvOffset;
    fnv_mix(h, bytes.data(), bytes.size());
    return h;
}

bool EqualityBasis::project_rhs(const Vec &gamma, Vec &t, double *forward_bound) const {
    const Impl &b = *impl_;
    if (gamma.size() != static_cast<Index>(b.dropped.size())) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::project_rhs: gamma has size {}, expected {} (= |D|)",
                        gamma.size(), b.dropped.size()));
    }
    if (b.dropped.empty()) {
        t = Vec::Zero(static_cast<Index>(b.retained.size()));
        if (forward_bound != nullptr) {
            *forward_bound = 0.0;
        }
        return true;
    }
    const Index nb = static_cast<Index>(b.retained.size());
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec rhs(nb + nd);
    rhs << Vec::Zero(nb), -gamma;
    Vec x;
    double bound = kInf;
    const bool ok = b.solve_k(rhs, x, bound);
    if (forward_bound != nullptr) {
        *forward_bound = bound;
    }
    t = b.C * Vec(x.tail(nd));
    return ok && t.allFinite();
}

bool EqualityBasis::reduce_rhs(const Vec &be, const SpMatRM &Ae, const Vec &lower, Vec &out) const {
    const Impl &b = *impl_;
    if (be.size() != b.me || Ae.rows() != b.me || Ae.cols() != b.n || lower.size() != b.n) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::reduce_rhs: be {} / Ae {}x{} / lower {} disagree with the basis "
            "({} x {})",
            be.size(), Ae.rows(), Ae.cols(), lower.size(), b.me, b.n));
    }
    Vec w = be;
    if (!b.fixed.empty()) {
        Vec d = Vec::Zero(b.n);
        for (Index s : b.fixed) {
            d(s) = lower(s);
        }
        w -= Ae * d;
    }
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec gamma(nd);
    for (Index j = 0; j < nd; ++j) {
        double g = w(b.dropped[static_cast<std::size_t>(j)]);
        for (SpMatCM::InnerIterator ct(b.C, j); ct; ++ct) {
            g -= ct.value() * w(b.retained[static_cast<std::size_t>(ct.row())]);
        }
        gamma(j) = g;
    }
    Vec t;
    const bool ok = project_rhs(gamma, t);
    const Index nb = static_cast<Index>(b.retained.size());
    out.resize(nb);
    for (Index k = 0; k < nb; ++k) {
        out(k) = be(b.retained[static_cast<std::size_t>(k)]) + t(k);
    }
    return ok && out.allFinite();
}

bool EqualityBasis::reduce(QpProblem &qp) const {
    const Impl &b = *impl_;
    if (b.dropped.empty()) {
        return true;
    }
    if (qp.me() != b.me || qp.n() != b.n) {
        throw std::invalid_argument(
            fmt::format("EqualityBasis::reduce: QP has me={} n={}, basis expects me={} n={}",
                        qp.me(), qp.n(), b.me, b.n));
    }
    if (!qp.Ae.isCompressed()) {
        qp.Ae.makeCompressed();
    }
    Vec be;
    if (!reduce_rhs(qp.be, qp.Ae, qp.lower, be)) {
        return false;
    }
    const Index nb = static_cast<Index>(b.retained.size());
    SpMatRM Ae(nb, b.n);
    Index nnz = 0;
    for (Index r : b.retained) {
        nnz += qp.Ae.outerIndexPtr()[r + 1] - qp.Ae.outerIndexPtr()[r];
    }
    Ae.reserve(nnz);
    for (Index k = 0; k < nb; ++k) {
        Ae.startVec(k);
        for (SpMatRM::InnerIterator it(qp.Ae, b.retained[static_cast<std::size_t>(k)]); it; ++it) {
            Ae.insertBack(k, it.col()) = it.value();
        }
    }
    Ae.finalize();
    Ae.makeCompressed();
    qp.Ae = std::move(Ae);
    qp.be = std::move(be);
    return true;
}

bool EqualityBasis::lift(const Vec &lambda_B, Vec &out, double *forward_bound) const {
    const Impl &b = *impl_;
    const Index nb = static_cast<Index>(b.retained.size());
    if (lambda_B.size() != nb) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::lift: lambda_B has size {}, expected {} (= |B|)", lambda_B.size(), nb));
    }
    if (b.dropped.empty()) {
        out = lambda_B;
        if (forward_bound != nullptr) {
            *forward_bound = 0.0;
        }
        return true;
    }
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec rhs(nb + nd);
    rhs << lambda_B, Vec::Zero(nd);
    Vec x;
    double bound = kInf;
    bool ok = b.solve_k(rhs, x, bound);
    if (forward_bound != nullptr) {
        *forward_bound = bound;
    }
    const Vec w = ok ? Vec(x.tail(nd)) : Vec::Zero(nd);
    const Vec lb = ok ? Vec(lambda_B - b.C * w) : lambda_B;
    ok = ok && lb.allFinite();
    out = Vec::Zero(b.me);
    for (Index k = 0; k < nb; ++k) {
        out(b.retained[static_cast<std::size_t>(k)]) = ok ? lb(k) : lambda_B(k);
    }
    if (ok) {
        for (std::size_t j = 0; j < b.dropped.size(); ++j) {
            out(b.dropped[j]) = w(static_cast<Index>(j));
        }
    }
    return ok;
}

Vec EqualityBasis::fold(const Vec &lambda) const {
    const Impl &b = *impl_;
    if (lambda.size() != b.me) {
        throw std::invalid_argument(fmt::format(
            "EqualityBasis::fold: lambda has size {}, expected {} (= me)", lambda.size(), b.me));
    }
    if (b.dropped.empty()) {
        return lambda;
    }
    const Index nb = static_cast<Index>(b.retained.size());
    const Index nd = static_cast<Index>(b.dropped.size());
    Vec ld(nd);
    for (Index j = 0; j < nd; ++j) {
        ld(j) = lambda(b.dropped[static_cast<std::size_t>(j)]);
    }
    Vec out = b.C * ld;
    for (Index k = 0; k < nb; ++k) {
        out(k) += lambda(b.retained[static_cast<std::size_t>(k)]);
    }
    return out;
}

EqualityRowSubsetModel::EqualityRowSubsetModel(const NlpModel &model, std::vector<Index> rows)
    : model_(model), rows_(std::move(rows)) {
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        if (rows_[k] < 0 || rows_[k] >= model.me() || (k > 0 && rows_[k] <= rows_[k - 1])) {
            throw std::invalid_argument(fmt::format(
                "EqualityRowSubsetModel: row {} at position {} is out of [0, {}) or not "
                "strictly increasing",
                rows_[k], k, model.me()));
        }
    }
}

Index EqualityRowSubsetModel::n() const { return model_.n(); }
Index EqualityRowSubsetModel::me() const { return static_cast<Index>(rows_.size()); }
Index EqualityRowSubsetModel::mi() const { return model_.mi(); }
double EqualityRowSubsetModel::eval_f(const Vec &x) const { return model_.eval_f(x); }

Vec EqualityRowSubsetModel::restrict(const Vec &full) const {
    Vec out(me());
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        out(static_cast<Index>(k)) = full(rows_[k]);
    }
    return out;
}

SpMatRM EqualityRowSubsetModel::restrict(const SpMatRM &full) const {
    SpMatRM out(me(), full.cols());
    Index nnz = 0;
    for (Index r : rows_) {
        for (SpMatRM::InnerIterator it(full, r); it; ++it) {
            ++nnz;
        }
    }
    out.reserve(nnz);
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        out.startVec(static_cast<Index>(k));
        for (SpMatRM::InnerIterator it(full, rows_[k]); it; ++it) {
            out.insertBack(static_cast<Index>(k), it.col()) = it.value();
        }
    }
    out.finalize();
    out.makeCompressed();
    return out;
}

Vec EqualityRowSubsetModel::eval_grad(const Vec &x) const {
    Vec g;
    model_.eval_grad_in_place(x, g);
    return g;
}
Vec EqualityRowSubsetModel::eval_ce(const Vec &x) const {
    Vec c;
    eval_ce_in_place(x, c);
    return c;
}
Vec EqualityRowSubsetModel::eval_ci(const Vec &x) const {
    Vec c;
    model_.eval_ci_in_place(x, c);
    return c;
}
SpMatRM EqualityRowSubsetModel::eval_hess(const Vec &x, double obj_scale, const Vec &lambda_e,
                                          const Vec &lambda_i) const {
    SpMatRM H;
    eval_hess_in_place(x, obj_scale, lambda_e, lambda_i, H);
    return H;
}
SpMatRM EqualityRowSubsetModel::eval_jac_e(const Vec &x) const {
    SpMatRM J;
    eval_jac_e_in_place(x, J);
    return J;
}
SpMatRM EqualityRowSubsetModel::eval_jac_i(const Vec &x) const {
    SpMatRM J;
    model_.eval_jac_i_in_place(x, J);
    return J;
}
void EqualityRowSubsetModel::eval_grad_in_place(const Vec &x, Vec &out) const {
    model_.eval_grad_in_place(x, out);
}
void EqualityRowSubsetModel::eval_ce_in_place(const Vec &x, Vec &out) const {
    Vec full;
    model_.eval_ce_in_place(x, full);
    out = restrict(full);
}
void EqualityRowSubsetModel::eval_ci_in_place(const Vec &x, Vec &out) const {
    model_.eval_ci_in_place(x, out);
}
void EqualityRowSubsetModel::eval_jac_e_in_place(const Vec &x, SpMatRM &out) const {
    SpMatRM full;
    model_.eval_jac_e_in_place(x, full);
    out = restrict(full);
}
void EqualityRowSubsetModel::eval_jac_i_in_place(const Vec &x, SpMatRM &out) const {
    model_.eval_jac_i_in_place(x, out);
}
void EqualityRowSubsetModel::eval_hess_in_place(const Vec &x, double obj_scale, const Vec &lambda_e,
                                                const Vec &lambda_i, SpMatRM &out) const {
    if (lambda_e.size() != me()) {
        throw std::invalid_argument(
            fmt::format("EqualityRowSubsetModel::eval_hess: lambda_e has size {}, expected {}",
                        lambda_e.size(), me()));
    }
    Vec full = Vec::Zero(model_.me());
    for (std::size_t k = 0; k < rows_.size(); ++k) {
        full(rows_[k]) = lambda_e(static_cast<Index>(k));
    }
    model_.eval_hess_in_place(x, obj_scale, full, lambda_i, out);
}
const Vec &EqualityRowSubsetModel::lower() const { return model_.lower(); }
const Vec &EqualityRowSubsetModel::upper() const { return model_.upper(); }
Vec EqualityRowSubsetModel::start_point() const { return model_.start_point(); }

} // namespace hven::solvers::detail
