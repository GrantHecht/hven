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
// C is kept assembled and dense (c_) and refactored on every add_border/
// drop_border via hven::linear::DenseSymmetricFactor's symmetric indefinite
// Bunch-Kaufman factorization (LAPACK dsytrf on the LOWER triangle) rather
// than Eigen::LDLT -- deliberately: Eigen::LDLT has no pivoting strategy for
// genuinely indefinite dense matrices, and on e.g. C = [[0,1],[1,0]] it
// silently returns a wrong answer with no exception. The engine's active-set
// updates can legitimately produce such a C (mixed-sign multipliers mid-
// update), and Bunch-Kaufman's 2x2-pivot fallback is designed for exactly
// that case.
//
// INVARIANT (c_): its top-left dim() x dim() block holds, bit for bit, what a
// from-scratch assembly of the current arrays would -- -v_[i].dot(k0inv_v_[j])
// for every (i, j), plus d_[i] on the diagonal -- on EVERY path, including
// after a throw. An add computes only its new row and column (each entry by
// that same expression); a drop deletes a row and column. The factorization
// is of that block, so it is also bit-identical to a from-scratch one. The
// O(dim()^2 * n0) assembly is gone from the per-border cost; what remains is
// O(dim() * n0) for the new row/column and O(dim()^3) for the factorization.
//
// NOTE dim() <= schur_cap is NOT an invariant this class enforces --
// add_border always succeeds and this class never refuses one. schur_cap is
// advisory: exceeding it makes needs_refactorization() true, and it is the
// CALLER's job to stop bordering at that point (qp_engine.h rebuilds K0 when
// the offending borders can be folded into it, and latches onto the
// elimination path when they cannot). Either way dim() peaks at
// schur_cap + 1: the cap is detected by the add that crosses it, so one
// border beyond the cap is always paid for. A caller that ignores
// needs_refactorization() will simply grow C without bound and pay the
// O(dim()^3) factorization for it.
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
// re-indexed after each drop) and deletes its row and column of C.
//
// BATCHING. begin_batch()/end_batch() bracket a run of add_border calls whose
// intermediate factorizations nobody reads (sync_borders): each add then only
// extends C, and end_batch() factorizes once. Inside a batch the evidence is
// absent (every reader refuses, as after a failed factorization). end_batch() keeps
// the per-add contract under a throw: if the one factorization throws, the
// batch's adds are replayed one at a time with their own factorizations, so
// the throw and the stack it leaves are exactly the per-add ones.

#include <algorithm>
#include <cmath>
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

// Steps near_null_direction() (inverse iteration on C's own factor) and dominant_direction()
// (power iteration on C) take: each damps a direction whose |eigenvalue| is r times nearer the
// other end of C's spectrum by 1/r. Few on purpose -- the reader wants every border the two
// extreme directions could live on, and an unconverged direction only widens that set.
constexpr int kConditionDirectionSteps = 2;

} // namespace detail

class SchurComplement {
  public:
    /// @param kkt     The sparse K0 factor this complement borders.
    /// @param opts     The effective QP options (schur_cap / schur_cond_max).
    /// @param threads  The thread count in force for the DENSE border factor's
    ///                 own LAPACK calls (M6 W5 T8.8) -- the same
    ///                 SqpOptions::common.threads the sparse factors take,
    ///                 handed down by whoever builds this complement. A plain
    ///                 parameter and not a QpOptions field, exactly as the
    ///                 parametric predictor takes it: QpOptions deliberately
    ///                 carries no thread field. 0, the default and what every
    ///                 pre-T8.8 construction site passed, leaves the backend's
    ///                 own default alone.
    SchurComplement(detail::KktFactor &kkt, const QpOptions &opts, int threads = 0)
        : kkt_(kkt), opts_(opts) {
        factor_.set_num_threads(threads);
    }

    /// The DENSE border factor's live thread count -- a boundary observation
    /// of the one factor path the sparse surface does not cover.
    int num_threads() const noexcept { return factor_.num_threads(); }

    // Borders K0 with column v (size K0.rows()) and diagonal entry d. Costs
    // one K0 solve (to cache K0^-1 v), the O(dim() * n0) dots of the new row
    // and column of C, and an O(dim()^3) dense factorization of C (deferred to
    // end_batch() inside a batch).
    //
    // ALL-OR-NOTHING ON THE BORDER STACK. If anything below throws -- the K0
    // solve, an allocation, or the dense factorization -- this call leaves
    // v_/k0inv_v_/d_ EXACTLY as it found them, so dim() never counts a border
    // the call did not finish adding. The caller (qp_engine.h sync_borders)
    // records each successful add in a parallel ledger AFTER this returns, and
    // drop_border() indexes by add-order position; a border left behind by a
    // throwing add would be invisible to that ledger forever and would shift
    // every later drop onto the WRONG border -- a silent wrong bordered solve.
    // c_ needs no rollback: only its top-left dim() block is meaningful, and
    // the shrink takes the half-written new row and column out of it.
    //
    // The cached C factorization is NOT restored -- it cannot be, short of
    // re-running the factorization that just failed. It is left DISCARDED
    // instead, which makes needs_refactorization() true and every evidence
    // reader refuse; the caller must rebuild K0 rather than keep bordering.
    void add_border(const Vec &v, double d) {
        Vec k0inv_v = detail::solve_vec(kkt_, v);

        // Every allocation the pushes below and the growth of c_ could need is
        // taken HERE, before anything mutates, so the torn state of one
        // push_back succeeding and a later one throwing bad_alloc cannot form.
        // (drop_border's per-array erase and the C assembly both index all
        // three at dim(), so a length skew is an unguarded out-of-bounds read
        // in Release, not a degraded answer.) Growing GEOMETRICALLY, not to
        // the exact size needed: reserving base + 1 every time would turn
        // push_back's amortized growth into a reallocation per add_border.
        reserve_all(v_.size() + 1);

        const std::size_t base = v_.size();
        try {
            append_border(v, std::move(k0inv_v), d);
            if (batching_) {
                // Factorization deferred: nothing may read C's old one.
                singular_ = false;
                evidence_.reset();
                factor_pending_ = true;
            } else {
                factor_c();
            }
        } catch (...) {
            // Shrinking three vectors to a size they already held: no
            // allocation, no reallocation, nothing here can throw.
            v_.resize(base);
            k0inv_v_.resize(base);
            d_.resize(base);
            throw;
        }
    }

    // Opens a batch: until end_batch(), add_border only extends C. No
    // drop_border inside a batch. Not nestable.
    void begin_batch() {
        if (batching_) {
            throw std::logic_error("SchurComplement::begin_batch: a batch is already open");
        }
        batching_ = true;
        factor_pending_ = false;
        batch_base_ = v_.size();
    }

    // Closes the batch and factorizes C once if it grew. The result is the
    // state the same adds made one at a time would have left, bit for bit.
    // If the single factorization throws, the batch's adds are replayed with a
    // factorization each: the throw is then the one the per-add path raises
    // (the first add whose C cannot be factorized), and the stack is left as
    // that add's rollback leaves it -- the caller's ledger keeps only its first
    // dim() entries. If the replay completes, the replay's state stands.
    void end_batch() {
        if (!batching_) {
            return;
        }
        batching_ = false;
        if (!factor_pending_) {
            return;
        }
        factor_pending_ = false;
        try {
            factor_c();
        } catch (...) {
            replay_batch();
        }
    }

    // Removes the k-th added border (0-indexed in add_border call order,
    // shifting later indices down by one). Throws std::out_of_range if k is
    // outside [0, dim()).
    //
    // ALL-OR-NOTHING ON THE BORDER STACK, the exact mirror of add_border's
    // guarantee. The caller drops the border FIRST and erases the matching
    // ledger entry SECOND, so a throw out of the factorization below -- after
    // the arrays and c_ are already shortened -- must not survive: it would
    // leave a ledger entry naming a border that is gone, and every later drop
    // would index one position too high, removing the WRONG border -- a silent
    // wrong bordered solve rather than a reported failure.
    //
    // THE FALLIBLE PART CANNOT BE HOISTED here the way add_border hoists its
    // allocations: the factorization is of C as it stands AFTER the deletion,
    // so rollback is the available shape. Re-inserting into three vectors that
    // were just erased from needs no allocation (erase does not shrink
    // capacity) and moves only Eigen dynamic vectors, whose move assignment
    // steals a pointer; c_'s deleted row and column are written back into
    // storage c_ already has. Nothing in the recovery can itself throw. (The
    // two saved copies of that row and column allocate, and are taken before
    // anything mutates.)
    //
    // The cached C factorization is NOT restored, for add_border's reason:
    // needs_refactorization() is true after a failed factorization and every
    // evidence reader refuses. What the rollback preserves is the STACK's
    // agreement with the caller's ledger, not the ability to keep bordering.
    void drop_border(Index k) {
        if (k < 0 || k >= dim()) {
            throw std::out_of_range(fmt::format(
                "SchurComplement::drop_border: index {} out of range [0, {})", k, dim()));
        }
        if (batching_) {
            throw std::logic_error("SchurComplement::drop_border: called inside a batch");
        }
        const Index m = dim();
        const auto idx = static_cast<std::size_t>(k);
        const auto at = static_cast<std::ptrdiff_t>(idx);
        const Vec saved_col = c_.col(k).head(m);
        const Vec saved_row = c_.row(k).head(m).transpose();
        Vec saved_v = std::move(v_[idx]);
        Vec saved_k0inv_v = std::move(k0inv_v_[idx]);
        const double saved_d = d_[idx];
        v_.erase(v_.begin() + at);
        k0inv_v_.erase(k0inv_v_.begin() + at);
        d_.erase(d_.begin() + at);
        delete_c_row_col(k, m);
        try {
            factor_c();
        } catch (...) {
            v_.insert(v_.begin() + at, std::move(saved_v));
            k0inv_v_.insert(k0inv_v_.begin() + at, std::move(saved_k0inv_v));
            d_.insert(d_.begin() + at, saved_d);
            restore_c_row_col(k, m, saved_col, saved_row);
            throw;
        }
    }

    // rhs_full sized K0.rows() + dim(); returns [x0; y] of the same size.
    // Throws std::runtime_error if C's last factorization found an exact
    // zero pivot (see factor_c()) -- needs_refactorization() would
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

    // True iff C's last factorization is usable and its inertia is one-signed
    // (every eigenvalue negative, or every one positive). Read by the walk's
    // conditioning latch: a border ADDED to a definite C pushes its extreme
    // eigenvalues outward (Cauchy interlacing) while the bordered C stays
    // definite.
    bool definite() const {
        return dim() > 0 && evidence_usable() &&
               (evidence_->neg_eigs == 0 || evidence_->neg_eigs == dim());
    }

    // C's two extreme directions, the ends cond_estimate() measures: the one
    // nearest its null space (kConditionDirectionSteps steps of inverse
    // iteration on the cached factor) and its dominant one (as many steps of
    // power iteration on C), each from a fixed start and scaled to a largest
    // |entry| of 1. Entry i belongs to the i-th border (add order). EMPTY when
    // the direction cannot be read: dim() == 0, a step that is not finite or
    // vanishes, and -- for the near-null one -- no usable factorization (an
    // exactly singular C, or a rebuild that threw). Read by the walk's
    // conditioning latch (qp_engine.h, section 4) for the borders that made C
    // ill-conditioned; they change nothing here.
    Vec near_null_direction() const {
        if (dim() == 0 || !evidence_usable()) {
            return {};
        }
        return iterate_direction([this](const Vec &y, Vec &z) { factor_.solve(y, z); });
    }
    Vec dominant_direction() const {
        if (dim() == 0) {
            return {};
        }
        const Index m = dim();
        return iterate_direction(
            [this, m](const Vec &y, Vec &z) { z.noalias() = c_.topLeftCorner(m, m) * y; });
    }

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
    // engaged exactly when the last factor_c() ran to completion on a
    // nonempty C and got a usable factorization out of it. Two states leave
    // it disengaged, and neither may be read through:
    //
    //   * singular_ -- try_factorize reported kExactlySingular, so the
    //     factor completed but carries nothing decodable.
    //   * A throw out of factor_c, which is a real path:
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

    // The shared loop of near_null_direction() and dominant_direction(): `apply`
    // maps y to the next unnormalized iterate z.
    template <class Apply> Vec iterate_direction(const Apply &apply) const {
        const Index m = dim();
        Vec y(m);
        for (Index i = 0; i < m; ++i) {
            // A fixed start with no symmetry for an eigenvector to be orthogonal to.
            y(i) = 1.0 + static_cast<double>(i) / static_cast<double>(m);
        }
        Vec z(m);
        for (int step = 0; step < detail::kConditionDirectionSteps; ++step) {
            apply(y, z);
            const double s = z.lpNorm<Eigen::Infinity>();
            if (!std::isfinite(s) || s == 0.0) {
                return {};
            }
            y = z / s;
        }
        return y;
    }

    // Grows all three border arrays, and c_, to hold at least `want` entries,
    // doing EVERY allocation before any of them is mutated -- see add_border.
    void reserve_all(std::size_t want) {
        const auto cap_c = static_cast<std::size_t>(c_.rows());
        if (v_.capacity() < want || k0inv_v_.capacity() < want || d_.capacity() < want) {
            const std::size_t grown = std::max(want, 2 * v_.size());
            v_.reserve(grown);
            k0inv_v_.reserve(grown);
            d_.reserve(grown);
        }
        if (cap_c < want) {
            const std::size_t grown = std::max(want, cap_c + cap_c / 2 + 8);
            const auto m = static_cast<Index>(v_.size());
            Eigen::MatrixXd bigger(static_cast<Index>(grown), static_cast<Index>(grown));
            bigger.topLeftCorner(m, m) = c_.topLeftCorner(m, m);
            c_.swap(bigger);
        }
    }

    // Pushes the border onto the three arrays (capacity already reserved, so
    // nothing here throws) and writes its row and column of C. Every entry is
    // the expression a from-scratch assembly would evaluate for it, with its
    // operands in the same order: c(i, j) = -v_i . k0inv_v_j for ALL i, j (C is
    // symmetric only up to rounding, and the factorization reads one
    // triangle), c(k, k) additionally + d_k.
    void append_border(const Vec &v, Vec &&k0inv_v, double d) {
        v_.push_back(v);
        k0inv_v_.push_back(std::move(k0inv_v));
        d_.push_back(d);
        const Index k = dim() - 1;
        const auto sk = static_cast<std::size_t>(k);
        for (Index j = 0; j < k; ++j) {
            const auto sj = static_cast<std::size_t>(j);
            c_(k, j) = -v_[sk].dot(k0inv_v_[sj]);
            c_(j, k) = -v_[sj].dot(k0inv_v_[sk]);
        }
        c_(k, k) = -v_[sk].dot(k0inv_v_[sk]);
        c_(k, k) += d_[sk];
    }

    // Deletes row and column k of the m x m block, closing the gap. Plain
    // loops: no temporary, so no allocation.
    void delete_c_row_col(Index k, Index m) {
        for (Index j = 0; j < m; ++j) {
            for (Index i = k; i + 1 < m; ++i) {
                c_(i, j) = c_(i + 1, j);
            }
        }
        for (Index j = k; j + 1 < m; ++j) {
            for (Index i = 0; i + 1 < m; ++i) {
                c_(i, j) = c_(i, j + 1);
            }
        }
    }

    // The inverse of delete_c_row_col: reopens row and column k (undoing the
    // two shifts in reverse order) and writes the saved ones back; the saved
    // column and row are full length m and carry the diagonal entry at k.
    void restore_c_row_col(Index k, Index m, const Vec &col, const Vec &row) {
        for (Index j = m - 1; j > k; --j) {
            for (Index i = 0; i + 1 < m; ++i) {
                c_(i, j) = c_(i, j - 1);
            }
        }
        for (Index j = 0; j < m; ++j) {
            for (Index i = m - 1; i > k; --i) {
                c_(i, j) = c_(i - 1, j);
            }
        }
        for (Index i = 0; i < m; ++i) {
            c_(i, k) = col(i);
            c_(k, i) = row(i);
        }
    }

    // After end_batch()'s one factorization threw: undoes the batch's adds and
    // redoes each with its own factorization, exactly as add_border does, so
    // the exception and the stack it leaves are the per-add path's. The cached
    // K0^-1 v columns are reused; no K0 solve is repeated.
    void replay_batch() {
        const std::size_t base = batch_base_;
        std::vector<Vec> tail_v(
            std::make_move_iterator(v_.begin() + static_cast<std::ptrdiff_t>(base)),
            std::make_move_iterator(v_.end()));
        std::vector<Vec> tail_k(
            std::make_move_iterator(k0inv_v_.begin() + static_cast<std::ptrdiff_t>(base)),
            std::make_move_iterator(k0inv_v_.end()));
        const std::vector<double> tail_d(d_.begin() + static_cast<std::ptrdiff_t>(base), d_.end());
        v_.resize(base);
        k0inv_v_.resize(base);
        d_.resize(base);
        for (std::size_t t = 0; t < tail_v.size(); ++t) {
            const std::size_t before = v_.size();
            try {
                append_border(tail_v[t], std::move(tail_k[t]), tail_d[t]);
                factor_c();
            } catch (...) {
                v_.resize(before);
                k0inv_v_.resize(before);
                d_.resize(before);
                throw;
            }
        }
    }

    // Factorizes the current top-left block of c_ via
    // DenseSymmetricFactor::try_factorize on the LOWER triangle
    // (Bunch-Kaufman -- 'L' is float-load-bearing: 'U' eliminates in a
    // different order and produces different rounding), and caches the
    // factor's block evidence used by cond_estimate() and
    // expected_neg_eigs_delta(). See the header comment for why Bunch-Kaufman
    // (not Eigen::LDLT).
    //
    // THIS CAN THROW, and its throw is caught nowhere inside this class except
    // by add_border's and drop_border's rollback (and end_batch's replay). It
    // clears the cached evidence FIRST, so an escape leaves the object with
    // the border arrays intact (or restored) and no usable factorization --
    // the state evidence_usable() exists to describe. Every reader is written
    // against that; do not add one that assumes evidence_ survives a failed
    // factorization.
    void factor_c() {
        const Index m = dim();
        singular_ = false;
        evidence_.reset();
        if (m == 0) {
            return;
        }

        // try_factorize, not factorize: an exact zero pivot is a reportable
        // STATE this class degrades on (needs_refactorization() reports it,
        // solve()/expected_neg_eigs_delta() throw), never an error to
        // propagate. An illegal-argument failure (info < 0) still throws
        // from inside the dense factor. The block is passed by reference
        // (column-major, outer stride = c_'s capacity); the factor copies it.
        const hven::linear::DenseFactorizeOutcome outcome =
            factor_.try_factorize(c_.topLeftCorner(m, m), hven::linear::Triangle::kLower);
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

    // The assembled C, column-major, in a square buffer of capacity >=
    // dim(); only the top-left dim() x dim() block is meaningful (see the
    // INVARIANT in the header comment).
    Eigen::MatrixXd c_;

    // An open batch (begin_batch): the stack size it opened at, and whether
    // an add since then left a factorization owed.
    bool batching_ = false;
    bool factor_pending_ = false;
    std::size_t batch_base_ = 0;

    // Bunch-Kaufman factor of C (lower triangle -- see factor_c) and
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
