// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#include "hven/detail/qp/ipqp_kkt_layout.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include "hven/core/pattern_hash.h"

namespace hven::solvers {

namespace {

// The position of (row, col) in a COMPRESSED row-major matrix's value array.
// Every row's inner indices are sorted after makeCompressed(), so this is a
// binary search per entry -- the same lookup SsnEngine::sync_matrix does when
// it records its own position map.
std::size_t locate(const SpMatRM &k, Index row, Index col) {
    using StorageIndex = SpMatRM::StorageIndex;
    const StorageIndex *const outer = k.outerIndexPtr();
    const StorageIndex *const inner = k.innerIndexPtr();
    const StorageIndex *const begin = inner + outer[row];
    const StorageIndex *const end = inner + outer[row + 1];
    const StorageIndex *const hit = std::lower_bound(begin, end, static_cast<StorageIndex>(col));
    if (hit == end || *hit != static_cast<StorageIndex>(col)) {
        // Unreachable for a matrix just built from these very triplets;
        // checked rather than asserted because Release compiles an assert out
        // entirely and a wrong position would corrupt the KKT matrix silently
        // on every later scatter.
        throw std::runtime_error(
            fmt::format("IpqpKktLayout: internal error -- entry ({}, {}) is missing from the "
                        "matrix just assembled from it",
                        row, col));
    }
    return static_cast<std::size_t>(hit - inner);
}

} // namespace

template <typename Emit>
void IpqpKktLayout::for_each_entry(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n,
                                   Index me, Index mi, Emit emit) {
    const Index eq_off = n + mi;
    const Index iq_off = n + mi + me;

    // (1,1): H's stored upper triangle. sync() has already refused a
    // below-diagonal entry, so every emission here is on or above the
    // diagonal.
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(H, i); it; ++it) {
            emit(i, it.col(), it.value());
        }
    }
    // (1,3): Ae', emitted by walking Ae's rows -- row r of Ae, column c,
    // becomes the upper-triangle entry (c, eq_off + r). c < n <= eq_off + r,
    // so it is always above the diagonal.
    for (Index r = 0; r < me; ++r) {
        for (SpMatRM::InnerIterator it(Ae, r); it; ++it) {
            emit(it.col(), eq_off + r, it.value());
        }
    }
    // (1,4): Ai', the same way.
    for (Index j = 0; j < mi; ++j) {
        for (SpMatRM::InnerIterator it(Ai, j); it; ++it) {
            emit(it.col(), iq_off + j, it.value());
        }
    }
}

std::uint64_t IpqpKktLayout::structure_hash(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai,
                                            Index n, Index me, Index mi) const {
    Fnv1a h;
    h.feed_index(n);
    h.feed_index(me);
    h.feed_index(mi);
    feed_pattern(h, H);
    feed_pattern(h, Ae);
    feed_pattern(h, Ai);
    return h.value();
}

bool IpqpKktLayout::matches(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n,
                            Index me, Index mi) const {
    if (!has_structure_) {
        return false;
    }
    if (n != n_ || me != me_ || mi != mi_) {
        return false;
    }
    return structure_hash(H, Ae, Ai, n, me, mi) == structure_key_;
}

void IpqpKktLayout::require_structure(const char *what) const {
    if (!has_structure_) {
        // Not merely defensive. The slot accessors range-check against
        // n_/me_/mi_, so "no plan yet" and "a plan for other dimensions" would
        // both pass that check while the tables behind it were empty or stale,
        // and the value returned is used directly as an index into
        // k.valuePtr(). Release compiles asserts out, so this is a throw.
        throw std::logic_error(fmt::format(
            "IpqpKktLayout::{}: no pattern has been laid out yet -- call sync() first", what));
    }
}

std::size_t IpqpKktLayout::diag_slot(Index base, Index offset, Index count) const {
    require_structure("diag_slot");
    if (offset < 0 || offset >= count) {
        throw std::out_of_range(fmt::format(
            "IpqpKktLayout: diagonal index {} is outside [0, {}) for this block", offset, count));
    }
    return diag_pos_[static_cast<std::size_t>(base + offset)];
}

std::size_t IpqpKktLayout::slack_coupling_slot(Index j) const {
    require_structure("slack_coupling_slot");
    if (j < 0 || j >= mi_) {
        throw std::out_of_range(
            fmt::format("IpqpKktLayout: coupling index {} is outside [0, {})", j, mi_));
    }
    return coupling_pos_[static_cast<std::size_t>(j)];
}

bool IpqpKktLayout::sync(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, Index n, Index me,
                         Index mi, SpMatRM &k) {
    // Boundary validation. Eigen's own asserts are compiled out under NDEBUG
    // and are never the only guard here (CLAUDE.md section 4).
    if (n < 0 || me < 0 || mi < 0) {
        throw std::invalid_argument(
            fmt::format("IpqpKktLayout::sync: dimensions must be non-negative (n={}, me={}, mi={})",
                        n, me, mi));
    }
    if (H.rows() != n || H.cols() != n) {
        throw std::invalid_argument(fmt::format("IpqpKktLayout::sync: H is {}x{}, expected {}x{}",
                                                H.rows(), H.cols(), n, n));
    }
    if (Ae.rows() != me || Ae.cols() != n) {
        throw std::invalid_argument(fmt::format("IpqpKktLayout::sync: Ae is {}x{}, expected {}x{}",
                                                Ae.rows(), Ae.cols(), me, n));
    }
    if (Ai.rows() != mi || Ai.cols() != n) {
        throw std::invalid_argument(fmt::format("IpqpKktLayout::sync: Ai is {}x{}, expected {}x{}",
                                                Ai.rows(), Ai.cols(), mi, n));
    }
    // H must store ONLY its upper triangle, the convention QpProblem::validate
    // enforces and every consumer reads H under. A below-diagonal entry would
    // be emitted below the KKT diagonal and rejected by the linear layer as a
    // malformed buffer -- a diagnostic pointing at the wrong layer -- so it is
    // named here instead.
    for (Index i = 0; i < n; ++i) {
        for (SpMatRM::InnerIterator it(H, i); it; ++it) {
            if (it.row() > it.col()) {
                throw std::invalid_argument(fmt::format(
                    "IpqpKktLayout::sync: H has a lower-triangle entry at (row={}, col={}); H "
                    "must store only its upper triangle (row <= col)",
                    it.row(), it.col()));
            }
        }
    }

    if (matches(H, Ae, Ai, n, me, mi) && k.rows() == dim_ && k.cols() == dim_ &&
        k.nonZeros() == nnz_ && k.isCompressed()) {
        scatter(H, Ae, Ai, k);
        return false;
    }

    // ---------------------------------------------------------------------
    // Structure changed (or this is the first sync): full layout.
    //
    // EVERYTHING below is built into LOCALS and committed in one step at the
    // end. Three sites here can throw -- reserve() and setFromTriplets() may
    // bad_alloc, and locate() raises on an entry the assembly does not carry
    // -- and a partial commit would be worse than a failed one: the slot
    // accessors bounds-check against n_/me_/mi_, so storing the NEW dimensions
    // beside the OLD tables would leave every in-range index reading past the
    // end of a shorter vector, in Release, silently, with the result used as
    // an index into k.valuePtr(). Committing at the end means a throw leaves
    // this object entirely on its previous plan, and leaves `k` untouched.
    //
    // AND the object is marked planless FIRST, so the protection does not rest
    // on the ordering below staying right forever: if anything here throws,
    // every slot accessor refuses with std::logic_error rather than
    // range-checking a stale table. The caller's recovery is to sync again;
    // the previous plan is not resumed, because from here nothing can tell a
    // caller who still wants it from one whose problem has moved.
    // ---------------------------------------------------------------------
    has_structure_ = false;

    const Index dim = n + 2 * mi + me;
    const Index slack_off = n;
    const Index eq_off = n + mi;
    const Index iq_off = n + mi + me;

    std::vector<Eigen::Triplet<double>> trips;
    trips.reserve(
        static_cast<std::size_t>(H.nonZeros() + n + Ae.nonZeros() + me + Ai.nonZeros() + 3 * mi));

    // The source nonzeros, in for_each_entry's order -- the order value_pos_
    // is indexed by.
    for_each_entry(H, Ae, Ai, n, me, mi,
                   [&trips](Index r, Index c, double v) { trips.emplace_back(r, c, v); });
    const std::size_t source_count = trips.size();

    // The structural entries the plan owns: a diagonal in every row (the
    // linear layer requires one, and all four families carry values the caller
    // writes per iteration) plus the (s, iq) coupling -I. Emitted with their
    // resting values -- 0 for the diagonals, -1 for the coupling -- so a
    // freshly laid-out matrix and a freshly scattered one agree.
    for (Index i = 0; i < n; ++i) {
        trips.emplace_back(i, i, 0.0);
    }
    for (Index j = 0; j < mi; ++j) {
        trips.emplace_back(slack_off + j, slack_off + j, 0.0);
        trips.emplace_back(slack_off + j, iq_off + j, -1.0);
    }
    for (Index r = 0; r < me; ++r) {
        trips.emplace_back(eq_off + r, eq_off + r, 0.0);
    }
    for (Index j = 0; j < mi; ++j) {
        trips.emplace_back(iq_off + j, iq_off + j, 0.0);
    }

    SpMatRM built(dim, dim);
    built.setFromTriplets(trips.begin(), trips.end());
    built.makeCompressed();

    std::vector<std::size_t> value_pos(source_count, 0);
    for (std::size_t t = 0; t < source_count; ++t) {
        value_pos[t] = locate(built, trips[t].row(), trips[t].col());
    }

    std::vector<std::size_t> diag_pos(static_cast<std::size_t>(dim), 0);
    for (Index d = 0; d < dim; ++d) {
        diag_pos[static_cast<std::size_t>(d)] = locate(built, d, d);
    }
    std::vector<std::size_t> coupling_pos(static_cast<std::size_t>(mi), 0);
    for (Index j = 0; j < mi; ++j) {
        coupling_pos[static_cast<std::size_t>(j)] = locate(built, slack_off + j, iq_off + j);
    }

    std::vector<double> primal_diag_source(static_cast<std::size_t>(n), 0.0);
    for (Index i = 0; i < n; ++i) {
        primal_diag_source[static_cast<std::size_t>(i)] =
            built.valuePtr()[diag_pos[static_cast<std::size_t>(i)]];
    }

    const std::uint64_t key = structure_hash(H, Ae, Ai, n, me, mi);

    // --- commit. Nothing below allocates or can throw: the vector moves are
    // pointer swaps and the rest are scalar stores.
    k = std::move(built);
    value_pos_ = std::move(value_pos);
    diag_pos_ = std::move(diag_pos);
    coupling_pos_ = std::move(coupling_pos);
    primal_diag_source_ = std::move(primal_diag_source);
    n_ = n;
    me_ = me;
    mi_ = mi;
    dim_ = dim;
    nnz_ = k.nonZeros();
    structure_key_ = key;
    has_structure_ = true;
    return true;
}

void IpqpKktLayout::scatter(const SpMatRM &H, const SpMatRM &Ae, const SpMatRM &Ai, SpMatRM &k) {
    double *const values = k.valuePtr();
    std::fill(values, values + k.nonZeros(), 0.0);

    std::size_t t = 0;
    const std::size_t expected = value_pos_.size();
    for_each_entry(H, Ae, Ai, n_, me_, mi_, [&](Index, Index, double v) {
        if (t >= expected) {
            // The structure-key collision guard, in SsnEngine::sync_matrix's
            // exact shape: a hash match is a probabilistic claim, and writing
            // past the cached position map on a collision would corrupt the
            // KKT matrix silently. Both halves are needed -- this one catches
            // a colliding pattern that emits MORE entries, the count check
            // below one that emits fewer.
            throw std::runtime_error(fmt::format(
                "IpqpKktLayout: structure-key collision detected -- the reused pattern expects "
                "{} entries but this problem emits more; refusing to write past the cached "
                "position map",
                expected));
        }
        values[value_pos_[t++]] += v;
    });
    if (t != expected) {
        throw std::runtime_error(fmt::format(
            "IpqpKktLayout: structure-key collision detected -- the reused pattern expects {} "
            "entries but this problem emitted {}",
            expected, t));
    }

    // The coupling block's resting value, re-established after the zero-fill.
    // The diagonals are deliberately left at zero (the primal family at
    // whatever H's own diagonal contributed): they are the caller's per-
    // iteration write.
    for (std::size_t j = 0; j < coupling_pos_.size(); ++j) {
        values[coupling_pos_[j]] = -1.0;
    }

    // H's diagonal contribution, read back out of the slot the scatter just
    // filled -- nothing else writes there, so this IS H(i,i).
    for (std::size_t i = 0; i < primal_diag_source_.size(); ++i) {
        primal_diag_source_[i] = values[diag_pos_[i]];
    }
}

} // namespace hven::solvers
