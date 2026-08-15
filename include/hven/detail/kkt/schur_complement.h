#pragma once

// schur_complement.h -- dense Schur-complement border over a fixed KKT
// factorization, letting working-set changes (the engine's active-set
// working-set updates, see qp_engine.h border mode) add/drop constraint
// rows without a full Pardiso refactorization.
//
// Given a factorized K0 (owned by `kkt`, held fixed across add_border/
// drop_border calls) and a sequence of border columns v_1..v_m with scalar
// diagonal entries d_1..d_m, this class solves the bordered system
//     [ K0   V ] [x0]   [rhs0]
//     [ Vt   D ] [y ] = [rhs1]
// via the Schur complement C = D - Vt K0^-1 V (D = diag(d_1..d_m)):
//     w  = K0^-1 rhs0
//     y  = C^-1 (rhs1 - Vt w)
//     x0 = K0^-1 (rhs0 - V y)
//
// C is maintained dense and factored from scratch on
// every add_border/drop_border via hven::linear::DenseSymmetricFactor's
// symmetric indefinite (Bunch-Kaufman) factorization -- LAPACK dsytrf on the
// LOWER triangle, C = L*D*L^T with D a block diagonal matrix of 1x1 and 2x2
// blocks -- rather than Eigen::LDLT.
// Eigen::LDLT was tried first and rejected: it has no pivoting strategy for
// genuinely indefinite dense matrices (it always produces 1x1 diagonal
// blocks), so on e.g. C = [[0,1],[1,0]] it reports info()==NumericalIssue,
// its vectorD() is garbage, and solve() silently returns a wrong answer with
// no exception -- exactly the failure mode this class must not have, since
// the engine's active-set working-set updates (see qp_engine.h border mode)
// can legitimately produce such C (a working set with mixed-sign multipliers
// mid-update). Bunch-Kaufman's 2x2-pivot fallback
// is designed for precisely this case. Recomputing the factorization from
// scratch is O(dim()^2 * n0) to rebuild C from the cached K0^-1v columns
// (n0 = K0.rows()) plus O(dim()^3) to factorize it; both are cheap while
// dim() stays around schur_cap (128).
//
// NOTE dim() <= schur_cap is NOT an invariant this class enforces --
// add_border always succeeds and this class never refuses one. schur_cap is
// advisory: exceeding it makes needs_refactorization() true, and it is the
// CALLER's job to stop bordering at that point. QpEngine (qp_engine.h) does
// so in two ways -- it rebuilds K0 when the offending borders can be folded
// into it, and it LATCHES onto the elimination path (ceasing to add borders
// at all) when they cannot, which is the pins-only case, since a pin is a
// border against every possible K0. Either way dim() peaks at schur_cap + 1:
// the cap is detected by the add that crosses it, so one border beyond the
// cap is always paid for. A caller that ignores needs_refactorization() will
// simply grow C without bound and pay the O(dim()^3) rebuild for it.
//
// LAPACK has no public rank-one update/downdate for
// symmetric indefinite factorizations, so a genuine incremental update is a
// later optimization if profiling ever shows the O(dim()^3) rebuild
// mattering -- it does not at this scale.
//
// K0^-1 v_i is cached at add_border time (one K0 solve per border) so
// `solve` costs exactly two K0 solves (for w and for x0), independent of
// dim(). This is the plain two-solve variant the task brief asks for first;
// the partial-solve fast path (composing SymmetricFactor::solve_partial's
// forward/diagonal/backward phases to shave one of those two solves when
// kkt.factor.supports_partial_solve() is true) is NOT implemented here. Note
// supports_partial_solve() is in fact CONSTANT for the lifetime of a given
// SchurComplement instance -- K0 (owned by `kkt`) is held fixed across
// add_border/drop_border by this class's own contract, and a fresh
// SchurComplement is constructed whenever K0 is refactorized (see
// needs_refactorization()) -- so caching the gate would be safe. It is
// skipped simply because no current caller needs the saving; the simpler,
// always-obviously-correct form is kept until one does.
//
// drop_border(k) removes the k-th added border (by add_border call order,
// re-indexed after each drop) and recomputes C from the remaining cached
// K0^-1v columns -- cheap and safe, and avoids having to reason about
// deleting a row/column from an existing factorization.

#include <algorithm>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include <Eigen/Dense>

#include <fmt/format.h>

#include <hven/detail/kkt/kkt_calls.h>
#include <hven/linear/dense_symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// Relative floor beneath which C's smallest |block eigenvalue| counts as
// numerically singular, even though the dense factorization completed
// without reporting an exactly zero pivot.
//
// This exists because cond_estimate() is a pure RATIO and is therefore
// structurally blind at dim() == 1: a 1x1 Schur complement of 1e-300 has
// max_abs == min_abs and reports a condition number of exactly 1.0, so
// schur_cond_max can never fire on it however small it gets. The border it
// represents is nonetheless one rounding error away from the exactly-singular
// case that makes solve() throw, and its solve is already meaningless well
// before that. Measuring the smallest block against the largest (floored at
// 1.0, so a uniformly tiny C is judged on absolute grounds rather than
// certifying itself well conditioned) closes that gap without disturbing the
// ratio test at higher dimensions, where the two agree.
constexpr double kSchurSingularEigFrac = 1e-12;

} // namespace detail

class SchurComplement {
  public:
    SchurComplement(detail::KktFactor &kkt, const QpOptions &opts) : kkt_(kkt), opts_(opts) {}

    // Borders K0 with column v (size K0.rows()) and diagonal entry d. Costs
    // one K0 solve (to cache K0^-1 v) plus an O(dim()^2 * n0 +
    // dim()^3) dense rebuild-and-factorize of C.
    void add_border(const Vec &v, double d) {
        Vec k0inv_v = detail::solve_vec(kkt_, v);
        v_.push_back(v);
        k0inv_v_.push_back(std::move(k0inv_v));
        d_.push_back(d);
        rebuild_schur();
    }

    // Removes the k-th added border (0-indexed in add_border call order,
    // shifting later indices down by one). Throws std::out_of_range if k is
    // outside [0, dim()).
    void drop_border(Index k) {
        if (k < 0 || k >= dim()) {
            throw std::out_of_range(fmt::format(
                "SchurComplement::drop_border: index {} out of range [0, {})", k, dim()));
        }
        const auto idx = static_cast<std::size_t>(k);
        v_.erase(v_.begin() + static_cast<std::ptrdiff_t>(idx));
        k0inv_v_.erase(k0inv_v_.begin() + static_cast<std::ptrdiff_t>(idx));
        d_.erase(d_.begin() + static_cast<std::ptrdiff_t>(idx));
        rebuild_schur();
    }

    // rhs_full sized K0.rows() + dim(); returns [x0; y] of the same size.
    // Throws std::runtime_error if C's last factorization found an exact
    // zero pivot (see rebuild_schur()) -- needs_refactorization() would
    // already be true in that case, so this is a belt-and-suspenders guard
    // against a caller ignoring it.
    Vec solve(const Vec &rhs_full) const {
        const Index m = dim();
        const Index n0 = rhs_full.size() - m;
        if (n0 < 0) {
            throw std::invalid_argument(
                fmt::format("SchurComplement::solve: rhs size {} smaller than border dim {}",
                            rhs_full.size(), m));
        }

        const Vec rhs0 = rhs_full.head(n0);
        const Vec w = detail::solve_vec(kkt_, rhs0); // K0^-1 rhs0

        Vec y(m);
        if (m > 0) {
            if (singular_) {
                throw std::runtime_error(fmt::format(
                    "SchurComplement::solve: Schur complement (dim={}) is exactly singular "
                    "(DenseSymmetricFactor reported kExactlySingular); refactorize instead "
                    "of solving",
                    m));
            }
            const Vec rhs1 = rhs_full.tail(m);
            Vec vtw(m);
            for (Index i = 0; i < m; ++i) {
                vtw(i) = v_[static_cast<std::size_t>(i)].dot(w);
            }
            const Vec cy = rhs1 - vtw;
            factor_.solve(cy, y); // dsytrs against the cached lower-triangle factorization
        }

        Vec rhs0_adj = rhs0;
        for (Index i = 0; i < m; ++i) {
            rhs0_adj -= y(i) * v_[static_cast<std::size_t>(i)];
        }
        const Vec x0 = detail::solve_vec(kkt_, rhs0_adj); // K0^-1 (rhs0 - V y)

        Vec x(n0 + m);
        x.head(n0) = x0;
        x.tail(m) = y;
        return x;
    }

    Index dim() const { return static_cast<Index>(v_.size()); }

    // Ratio of the largest to smallest |eigenvalue| among C's Bunch-Kaufman
    // diagonal blocks (1x1 blocks contribute |d_ii| directly; 2x2 blocks
    // contribute the two eigenvalues of that 2x2 block). 1.0 when dim()==0;
    // +inf when C's last factorization was exactly singular or otherwise
    // produced a zero eigenvalue.
    double cond_estimate() const {
        if (dim() == 0) {
            return 1.0;
        }
        if (singular_) {
            return std::numeric_limits<double>::infinity();
        }
        double max_abs = 0.0;
        double min_abs = std::numeric_limits<double>::infinity();
        for (const double e : evidence_->block_abs_eigs) {
            max_abs = std::max(max_abs, e);
            min_abs = std::min(min_abs, e);
        }
        if (min_abs == 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        return max_abs / min_abs;
    }

    // True iff C has grown past schur_cap, its condition estimate has passed
    // schur_cond_max, its smallest block eigenvalue has collapsed relative to
    // its largest (see kSchurSingularEigFrac -- the case cond_estimate() is
    // blind to at dim 1), or C's last factorization hit an exact zero pivot
    // (DenseFactorizeOutcome::kExactlySingular) -- any of which means the
    // caller must stop
    // bordering this factorization and take a different path.
    bool needs_refactorization() const {
        return singular_ || nearly_singular() || dim() > opts_.schur_cap ||
               cond_estimate() > opts_.schur_cond_max;
    }

    // True iff C's smallest |block eigenvalue| has collapsed to a negligible
    // fraction of its largest (floored at 1.0). False when dim() == 0, and
    // false when singular_ -- not because a singular C is well conditioned,
    // but because the block eigenvalues are not populated in that case and
    // needs_refactorization() already reports it directly.
    bool nearly_singular() const {
        if (dim() == 0 || singular_) {
            return false;
        }
        double max_abs = 0.0;
        double min_abs = std::numeric_limits<double>::infinity();
        for (const double e : evidence_->block_abs_eigs) {
            max_abs = std::max(max_abs, e);
            min_abs = std::min(min_abs, e);
        }
        return min_abs < detail::kSchurSingularEigFrac * std::max(1.0, max_abs);
    }

    // True inertia contribution of C: each 1x1 Bunch-Kaufman block
    // contributes sign(d_ii); each 2x2 block has a strictly negative
    // determinant by construction (Bunch-Kaufman only forms a 2x2 pivot when
    // no acceptable 1x1 pivot exists), so it always contributes exactly one
    // negative and one positive eigenvalue -- computed directly here (not
    // assumed) from the block's own two eigenvalues.
    //
    // Throws std::runtime_error if C's last factorization found an exact
    // zero pivot (singular_), matching solve()'s behavior. Returning 0 in
    // that case would be a plausible-LOOKING but WRONG answer: a singular C
    // may still be indefinite, and this class has no basis for claiming "no
    // negative eigenvalues" about a matrix it could not factor.
    Index expected_neg_eigs_delta() const {
        if (dim() == 0) {
            return 0;
        }
        if (singular_) {
            throw std::runtime_error(fmt::format(
                "SchurComplement::expected_neg_eigs_delta: Schur complement (dim={}) is exactly "
                "singular (DenseSymmetricFactor reported kExactlySingular); refactorize instead "
                "of querying its inertia",
                dim()));
        }
        return static_cast<Index>(evidence_->neg_eigs);
    }

  private:
    // Rebuilds C = D - Vt K0^-1 V (dense, from the cached K0^-1v columns),
    // factorizes it via DenseSymmetricFactor::try_factorize on the LOWER
    // triangle (Bunch-Kaufman -- 'L' is float-load-bearing: 'U' eliminates
    // in a different order and produces different rounding), and caches the
    // factor's block evidence, which carries the per-block eigenvalues used
    // by cond_estimate() and expected_neg_eigs_delta(). See the header
    // comment for why Bunch-Kaufman (not Eigen::LDLT) and why recomputed
    // from scratch.
    void rebuild_schur() {
        const Index m = dim();
        singular_ = false;
        evidence_.reset();
        if (m == 0) {
            return;
        }

        // Column-major to match DenseSymmetricFactor's LAPACK_COL_MAJOR call
        // / Eigen::MatrixXd's default storage directly (no transposition
        // needed). Only the lower triangle is read (Triangle::kLower), but
        // it's cheapest to just fill the whole symmetric matrix.
        Eigen::MatrixXd c = Eigen::MatrixXd::Zero(m, m);
        for (Index i = 0; i < m; ++i) {
            for (Index j = 0; j < m; ++j) {
                c(i, j) =
                    -v_[static_cast<std::size_t>(i)].dot(k0inv_v_[static_cast<std::size_t>(j)]);
            }
            c(i, i) += d_[static_cast<std::size_t>(i)];
        }

        // try_factorize, not factorize: an exact zero pivot is a reportable
        // STATE this class degrades on (needs_refactorization() reports it,
        // solve()/expected_neg_eigs_delta() throw), never an error to
        // propagate. An illegal-argument failure (info < 0) still throws
        // from inside the dense factor.
        const hven::linear::DenseFactorizeOutcome outcome =
            factor_.try_factorize(c, hven::linear::Triangle::kLower);
        if (outcome == hven::linear::DenseFactorizeOutcome::kExactlySingular) {
            // C is exactly singular. The factorization ran to completion but
            // is not usable for solve() or for reading an inertia off of it:
            // leave the evidence absent and let needs_refactorization()
            // report the singularity while solve() and
            // expected_neg_eigs_delta() both throw std::runtime_error,
            // rather than either of them silently returning a
            // plausible-looking but wrong answer (absent evidence read as "0
            // negative eigenvalues" would not follow from a factorization
            // that did not complete usably).
            singular_ = true;
            return;
        }
        evidence_ = factor_.block_evidence();
    }

    detail::KktFactor &kkt_;
    QpOptions opts_;
    std::vector<Vec> v_;
    std::vector<Vec> k0inv_v_;
    std::vector<double> d_;

    // Bunch-Kaufman factor of C (lower triangle -- see rebuild_schur) and
    // the block evidence read off it. The evidence is absent exactly when
    // singular_ (the factor's own leave-absent discipline); the judgment
    // calls consuming it -- kSchurSingularEigFrac, needs_refactorization(),
    // expected_neg_eigs_delta()'s throw-on-singular -- stay here: they are
    // border-stack policy, not dense-factor facts.
    hven::linear::DenseSymmetricFactor factor_;
    bool singular_ = false; // true iff last try_factorize was kExactlySingular
    std::optional<hven::linear::BunchKaufmanBlockEvidence> evidence_;
};

} // namespace hven::solvers
