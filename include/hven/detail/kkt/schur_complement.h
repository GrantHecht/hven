// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// schur_complement.h -- dense Schur-complement border over a fixed KKT
// factorization, letting working-set changes (qp_engine.h's border mode)
// add/drop constraint rows without a full Pardiso refactorization.
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
// C is maintained dense and factored from scratch on every add_border/
// drop_border via hven::linear::DenseSymmetricFactor's symmetric indefinite
// Bunch-Kaufman factorization (LAPACK dsytrf on the LOWER triangle) rather
// than Eigen::LDLT -- deliberately: Eigen::LDLT has no pivoting strategy for
// genuinely indefinite dense matrices, and on e.g. C = [[0,1],[1,0]] it
// silently returns a wrong answer with no exception. The engine's active-set
// updates can legitimately produce such a C (mixed-sign multipliers mid-
// update), and Bunch-Kaufman's 2x2-pivot fallback is designed for exactly
// that case. Recomputing from scratch is O(dim()^2 * n0) to rebuild C from
// the cached K0^-1v columns plus O(dim()^3) to factorize; both are cheap
// while dim() stays around schur_cap (128).
//
// NOTE dim() <= schur_cap is NOT an invariant this class enforces --
// add_border always succeeds and this class never refuses one. schur_cap is
// advisory: exceeding it makes needs_refactorization() true, and it is the
// CALLER's job to stop bordering at that point (qp_engine.h rebuilds K0 when
// the offending borders can be folded into it, and latches onto the
// elimination path when they cannot). Either way dim() peaks at
// schur_cap + 1: the cap is detected by the add that crosses it.
//
// K0^-1 v_i is cached at add_border time (one K0 solve per border) so
// `solve` costs exactly two K0 solves (for w and for x0), independent of
// dim(). The partial-solve fast path (composing
// SymmetricFactor::solve_partial's phases to shave one of those two solves
// where kkt.factor.supports_partial_solve()) is not implemented -- no
// current caller needs the saving. supports_partial_solve() is constant for
// the lifetime of an instance (K0 is held fixed; a fresh SchurComplement is
// built whenever K0 is refactorized), so caching that gate would be safe.
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
// cond_estimate() is a pure RATIO and is structurally blind at dim() == 1:
// a 1x1 Schur complement of 1e-300 has max_abs == min_abs and reports a
// condition number of exactly 1.0. Measuring the smallest block against the
// largest (floored at 1.0, so a uniformly tiny C is judged on absolute
// grounds rather than certifying itself well conditioned) closes that gap
// without disturbing the ratio test at higher dimensions, where the two
// agree.
constexpr double kSchurSingularEigFrac = 1e-12;

} // namespace detail

class SchurComplement {
  public:
    SchurComplement(detail::KktFactor &kkt, const QpOptions &opts) : kkt_(kkt), opts_(opts) {}

    // Borders K0 with column v (size K0.rows()) and diagonal entry d. Costs
    // one K0 solve (to cache K0^-1 v) plus an O(dim()^2 * n0 +
    // dim()^3) dense rebuild-and-factorize of C.
    //
    // ALL-OR-NOTHING ON THE BORDER STACK. If anything below throws -- the K0
    // solve, an allocation, or rebuild_schur()'s own factorization -- this
    // call leaves v_/k0inv_v_/d_ EXACTLY as it found them, so dim() never
    // counts a border the call did not finish adding. The caller
    // (qp_engine.h sync_borders) records each successful add in a parallel
    // ledger AFTER this returns, and drop_border() indexes by add-order
    // position; a border left behind by a throwing add would be invisible to
    // that ledger forever and would shift every later drop onto the WRONG
    // border -- a silent wrong bordered solve.
    //
    // The cached C factorization is NOT restored -- it cannot be, short of
    // re-running the rebuild that just failed. It is left DISCARDED instead,
    // which makes needs_refactorization() true and every evidence reader
    // refuse; the caller must rebuild K0 rather than keep bordering.
    void add_border(const Vec &v, double d) {
        Vec k0inv_v = detail::solve_vec(kkt_, v);

        // Every allocation the three pushes below could need is taken HERE,
        // before any of them mutates anything, so the torn state of one
        // push_back succeeding and a later one throwing bad_alloc cannot
        // form. Growing GEOMETRICALLY, not to the exact size needed:
        // reserving base + 1 every time would turn push_back's amortized
        // growth into a reallocation per add_border.
        reserve_all(v_.size() + 1);

        const std::size_t base = v_.size();
        try {
            v_.push_back(v);
            k0inv_v_.push_back(std::move(k0inv_v));
            d_.push_back(d);
            rebuild_schur();
        } catch (...) {
            // Shrinking three vectors to a size they already held: no
            // allocation, no reallocation, nothing here can throw.
            v_.resize(base);
            k0inv_v_.resize(base);
            d_.resize(base);
            throw;
        }
    }

    // Removes the k-th added border (0-indexed in add_border call order,
    // shifting later indices down by one). Throws std::out_of_range if k is
    // outside [0, dim()).
    //
    // ALL-OR-NOTHING ON THE BORDER STACK, the exact mirror of add_border's
    // guarantee. The caller drops the border FIRST and erases the matching
    // ledger entry SECOND, so a throw out of the rebuild_schur() below --
    // after all three arrays are already shortened -- must not survive: it
    // would leave a ledger entry naming a border that is gone, and every
    // later drop would index one position too high, removing the WRONG
    // border -- a silent wrong bordered solve rather than a reported
    // failure.
    //
    // THE FALLIBLE PART CANNOT BE HOISTED here the way add_border hoists its
    // allocations: rebuild_schur() rebuilds C from the arrays as they stand
    // AFTER the erase, so rollback is the available shape. Re-inserting into
    // three vectors that were just erased from needs no allocation (erase
    // does not shrink capacity) and moves only Eigen dynamic vectors, whose
    // move assignment steals a pointer -- so nothing in the recovery can
    // itself throw.
    //
    // The cached C factorization is NOT restored, for add_border's reason:
    // needs_refactorization() is true after a failed rebuild and every
    // evidence reader refuses. What the rollback preserves is the STACK's
    // agreement with the caller's ledger, not the ability to keep bordering.
    void drop_border(Index k) {
        if (k < 0 || k >= dim()) {
            throw std::out_of_range(fmt::format(
                "SchurComplement::drop_border: index {} out of range [0, {})", k, dim()));
        }
        const auto idx = static_cast<std::size_t>(k);
        const auto at = static_cast<std::ptrdiff_t>(idx);
        Vec saved_v = std::move(v_[idx]);
        Vec saved_k0inv_v = std::move(k0inv_v_[idx]);
        const double saved_d = d_[idx];
        v_.erase(v_.begin() + at);
        k0inv_v_.erase(k0inv_v_.begin() + at);
        d_.erase(d_.begin() + at);
        try {
            rebuild_schur();
        } catch (...) {
            v_.insert(v_.begin() + at, std::move(saved_v));
            k0inv_v_.insert(k0inv_v_.begin() + at, std::move(saved_k0inv_v));
            d_.insert(d_.begin() + at, saved_d);
            throw;
        }
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
        if (!evidence_usable()) {
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
        return (dim() > 0 && !evidence_usable()) || nearly_singular() || dim() > opts_.schur_cap ||
               cond_estimate() > opts_.schur_cond_max;
    }

    // True iff C's smallest |block eigenvalue| has collapsed to a negligible
    // fraction of its largest (floored at 1.0). False when dim() == 0, and
    // false whenever the block evidence is not usable -- not because such a C
    // is well conditioned, but because the block eigenvalues are not populated
    // in that case and needs_refactorization() already reports it directly.
    bool nearly_singular() const {
        if (dim() == 0 || !evidence_usable()) {
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
    // Throws std::runtime_error whenever there is no usable block evidence to
    // read the inertia off -- C's last factorization found an exact zero pivot
    // (singular_), or the last rebuild threw before it produced any. Returning
    // 0 in either case would be a plausible-LOOKING but WRONG answer: a
    // singular C may still be indefinite, and this class has no basis for
    // claiming "no negative eigenvalues" about a matrix it could not factor.
    Index expected_neg_eigs_delta() const {
        if (dim() == 0) {
            return 0;
        }
        if (!evidence_usable()) {
            throw std::runtime_error(fmt::format(
                "SchurComplement::expected_neg_eigs_delta: Schur complement (dim={}) has no "
                "usable factorization ({}); refactorize instead of querying its inertia",
                dim(),
                singular_ ? "DenseSymmetricFactor reported kExactlySingular"
                          : "the last rebuild threw before producing block evidence"));
        }
        return static_cast<Index>(evidence_->neg_eigs);
    }

  private:
    // THE ONE GUARD BEHIND ALL THREE EVIDENCE READERS. `evidence_` is
    // engaged exactly when the last rebuild_schur() ran to completion on a
    // nonempty C and got a usable factorization out of it. Two states leave
    // it disengaged, and neither may be read through:
    //
    //   * singular_ -- try_factorize reported kExactlySingular, so the
    //     factor completed but carries nothing decodable.
    //   * A throw out of rebuild_schur, which is a real path:
    //     DenseSymmetricFactor::try_factorize throws on LAPACKE info < 0,
    //     and LAPACKE's own NaN screen returns info < 0 for a C carrying a
    //     non-finite entry -- which one non-finite border value is enough to
    //     produce. dim() and singular_ are BOTH the wrong thing to test:
    //     the stack is nonempty and the factorization was not singular, it
    //     simply never finished.
    //
    // The readers' contracts are unchanged for every state that could
    // already occur -- cond_estimate() reports infinity, nearly_singular()
    // declines to judge, expected_neg_eigs_delta() throws -- which is
    // exactly what each already did for singular_. solve() needs no guard
    // here: it reaches factor_.solve(), which refuses a factor that never
    // completed with its own std::runtime_error.
    bool evidence_usable() const noexcept { return !singular_ && evidence_.has_value(); }

    // Grows all three border arrays to hold at least `want` entries, doing
    // EVERY allocation before any of them is mutated -- see add_border.
    void reserve_all(std::size_t want) {
        if (v_.capacity() >= want && k0inv_v_.capacity() >= want && d_.capacity() >= want) {
            return;
        }
        const std::size_t grown = std::max(want, 2 * v_.size());
        v_.reserve(grown);
        k0inv_v_.reserve(grown);
        d_.reserve(grown);
    }

    // Rebuilds C = D - Vt K0^-1 V (dense, from the cached K0^-1v columns),
    // factorizes it via DenseSymmetricFactor::try_factorize on the LOWER
    // triangle (Bunch-Kaufman -- 'L' is float-load-bearing: 'U' eliminates
    // in a different order and produces different rounding), and caches the
    // factor's block evidence used by cond_estimate() and
    // expected_neg_eigs_delta(). See the header comment for why
    // Bunch-Kaufman (not Eigen::LDLT) and why recomputed from scratch.
    //
    // THIS CAN THROW, and its throw is caught nowhere inside this class
    // except by add_border's rollback. It clears the cached evidence FIRST,
    // so an escape leaves the object with the border arrays intact (or
    // restored) and no usable factorization -- the state evidence_usable()
    // exists to describe. Every reader is written against that; do not add
    // one that assumes evidence_ survives a failed rebuild.
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
            // C is exactly singular: the factorization ran to completion but
            // is not usable for solve() or for reading an inertia off of it.
            // Leave the evidence absent; needs_refactorization() reports the
            // singularity while solve() and expected_neg_eigs_delta() both
            // throw, rather than either silently returning a
            // plausible-looking but wrong answer.
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
    // the block evidence read off it. The evidence is absent when singular_
    // AND whenever a rebuild threw partway; evidence_usable() is the single
    // test for both, and no reader may dereference evidence_ without it.
    // The judgment calls consuming it -- kSchurSingularEigFrac,
    // needs_refactorization(), expected_neg_eigs_delta()'s throw -- stay
    // here: they are border-stack policy, not dense-factor facts.
    hven::linear::DenseSymmetricFactor factor_;
    bool singular_ = false; // true iff last try_factorize was kExactlySingular
    std::optional<hven::linear::BunchKaufmanBlockEvidence> evidence_;
};

} // namespace hven::solvers
