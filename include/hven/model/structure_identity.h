// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// structure_identity.h — the three answers a consumer of the model contract
// needs about identity, and the one thing they are deliberately NOT.
//
//   * ModelStructureKey  -- "is this the same declared structure?"
//   * StructureEpoch     -- "have the structures been re-laid since I looked?"
//   * IdentityProbe      -- "same structure AND same point?", in one comparison
//
// What they are not: the engine's own structural digest. That one is a function
// of ASSEMBLED matrices and answers a factorization-reuse question; this one is
// a function of the DECLARATION and answers a layout question. The two must
// never be compared, which is why the model key is a distinct type rather than
// a bare unsigned integer, and why the free function that computes it is
// spelled `model_structure_key` rather than `structure_key`.
//
// Engine-independent by construction: nothing here includes from the
// interior-point machinery.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/core/pattern_hash.h"
#include "hven/model/aggregate_declaration.h"

namespace hven::solvers {

/// The structural key of a declared model: the digest of its claim structure,
/// the partition count that structure was laid at, and the digest of its
/// variable-bound structure.
///
/// The partition count is an explicit conjunct even though the claim stream is
/// itself partition-sensitive (claims are handed out in partition order). The
/// redundancy is deliberate: an explicit field is inspectable in a diagnostic
/// where an order-sensitivity is not, and a consumer comparing keys should not
/// have to know that the count is implicitly folded in.
/// Each conjunct has one public builder and no other way in:
/// `claim_digest_` is computed by claim_stream_digest, `bound_digest_` by
/// materialized_bound_digest, and `partition_count_` is the count
/// NlpAggregate::negotiate_partition_count actually adopted. Filling a field
/// from anything else is how a key stops answering the question it exists for.
struct ModelStructureKey {
    std::uint64_t claim_digest_ = 0;
    int partition_count_ = 0;
    std::uint64_t bound_digest_ = 0;

    friend bool operator==(const ModelStructureKey &, const ModelStructureKey &) = default;

    /// The three conjuncts folded into one value, for diagnostics and for
    /// consumers that want a single number to carry. Comparing folded digests
    /// is weaker than comparing keys -- prefer `operator==`.
    std::uint64_t digest() const noexcept {
        Fnv1a hash;
        hash.feed_index(static_cast<std::int64_t>(claim_digest_));
        hash.feed_index(static_cast<std::int64_t>(partition_count_));
        hash.feed_index(static_cast<std::int64_t>(bound_digest_));
        return hash.value();
    }
};

// THE STREAM PRIMITIVES ARE INTERNAL, and that placement is the contract
// rather than an accident of organization. Each key component has exactly ONE
// public way to compute it -- claim_stream_digest below for the claim
// conjunct, materialized_bound_digest for the bound conjunct -- and no
// assemble-it-yourself path beside it. The primitives are separable, and every
// separation is a way to get the answer wrong: a stream built without the
// dimension preamble keys two different problems identically, and a bound
// stream built from the DECLARED records rather than their intersection keys
// two different layouts identically. Both were real defects in this header's
// first cut. Keeping the pieces reachable but not public is what makes the
// safe path the only path a consumer can take.
namespace detail {

/// Opens a claim stream with the declared dimensions -- primal variables,
/// equality rows, inequality rows -- before the first claim is fed.
///
/// A PREAMBLE, on this library's own hashing precedent: a matrix's pattern
/// stream opens with its rows/cols/nnz triple, and that leading triple is what
/// makes a continued accumulation self-delimiting rather than needing a
/// separator (docs/pattern-hash.md). The same shape applies here for a stronger
/// reason than symmetry: two declarations can hand out an identical claim
/// stream at different dimensions -- a claim names rows and columns that exist,
/// never the size of the space they live in -- and without the preamble those
/// would key identically while laying different systems.
constexpr void feed_dimensions(Fnv1a &hash, int primal_vars, int equality_rows,
                               int inequality_rows) noexcept {
    hash.feed_index(primal_vars);
    hash.feed_index(equality_rows);
    hash.feed_index(inequality_rows);
}

/// @brief Feeds THE WHOLE CLAIM STREAM into a running accumulator, in claim
///        order, after the dimension preamble: claim i is the pair
///        (rows[i], cols[i]), and the pairs are fed interleaved.
/// @param hash   The accumulator, already carrying the dimension preamble.
/// @param rows   The claim rows, in claim order.
/// @param cols   The claim columns, in claim order.
/// @param count  How many claims the stream carries.
///
/// The claim-structure digest hashes the DECLARED (row, column) claim stream
/// rather than the assembled pattern: the stream is available at layout time
/// without assembling anything, it is exactly what a layout-determinism check
/// asserts on, and it keeps this key structurally independent of any digest
/// taken over assembled matrices. Order is significant -- a re-partitioned
/// declaration hands its claims out in a different order and hashes
/// differently, which is the point.
///
/// Fed as ONE PASS OVER THE TWO CONTIGUOUS ARRAYS rather than a call per claim:
/// the stream is the same stream, so the value is the same value, and the
/// arrays a claim arena already holds are read as arrays.
constexpr void feed_claims(Fnv1a &hash, const int *rows, const int *cols,
                           std::size_t count) noexcept {
    hash.feed_index_pairs(rows, cols, count);
}

/// Feeds ONE variable's MATERIALIZED bound structure into a running
/// accumulator, in variable order.
///
/// MATERIALIZED, and that is the substance of the rule rather than a detail of
/// where the loop runs: a variable's bound is DECLARED as a history of records
/// that are intersected tightest-wins, and it is the INTERSECTION that decides
/// the layout. Two different declaration histories intersecting to the same
/// per-variable structure lay the same system and must key the same; a history
/// whose intersection fixes a variable lays a different system from one whose
/// intersection does not, however similar the records look.
///
/// What is fed, per variable: which sides are finite, and whether the two sides
/// coincide -- both read off the intersection result. Bound VALUES are not fed.
/// The digest is value-independent GIVEN the materialized structure: values
/// enter only through their structural consequences, so a value change that
/// alters no variable's materialized finiteness or fixedness never re-keys the
/// problem, and a bound-value nudge cannot kill warm reuse. The two changes
/// that DO move the layout still move the key -- an intersection making a
/// variable fixed (it can then be eliminated from the solved system entirely),
/// and an intersection making a previously infinite side finite (it changes
/// which barrier terms exist).
///
/// A NaN bound is not finite by these comparisons and hashes as an unbounded
/// side. It is rejected at the declaration's own boundary
/// (AggregateDeclaration::validate) rather than given a meaning here.
constexpr void feed_variable_bound(Fnv1a &hash, const VariableBound &bound) noexcept {
    constexpr double kInf = std::numeric_limits<double>::infinity();
    std::int64_t structure = 0;
    if (bound.lower_ > -kInf && bound.lower_ < kInf) {
        structure |= 1;
    }
    if (bound.upper_ > -kInf && bound.upper_ < kInf) {
        structure |= 2;
    }
    if (bound.lower_ == bound.upper_) {
        structure |= 4;
    }
    hash.feed_index(bound.index_);
    hash.feed_index(structure);
}

} // namespace detail

/// THE claim-structure conjunct of a declaration's structural key, and the only
/// public way to compute one: the dimension preamble, then the claim stream in
/// claim order.
///
/// The claims are taken as the two index arrays a claim arena already holds --
/// the shape KktClaimSpace publishes, and the shape the arrays keep once a
/// claim pass has filled them -- so no caller has to interleave them into
/// something else first. Repacking would be exactly the assemble-it-yourself
/// step this entry exists to remove.
///
/// Throws std::invalid_argument if the two arrays disagree in length: a claim
/// is a (row, column) pair, and a stream missing half of one is not a stream.
inline std::uint64_t claim_stream_digest(const AggregateDeclaration &declaration,
                                         Eigen::Ref<const Eigen::VectorXi> claim_rows,
                                         Eigen::Ref<const Eigen::VectorXi> claim_cols) {
    if (claim_rows.size() != claim_cols.size()) {
        throw std::invalid_argument(
            fmt::format("claim_stream_digest: the claim stream has {0} rows and {1} columns; a "
                        "claim is a (row, column) pair, so the two arrays must be the same length",
                        claim_rows.size(), claim_cols.size()));
    }

    Fnv1a hash;
    detail::feed_dimensions(hash, declaration.primal_vars_, declaration.equality_rows_,
                            declaration.inequality_rows_);
    detail::feed_claims(hash, claim_rows.data(), claim_cols.data(),
                        static_cast<std::size_t>(claim_rows.size()));
    return hash.value();
}

/// THE bound-structure conjunct of a declaration's structural key, and the only
/// public way to compute one: the materialized per-variable structure, in
/// variable order.
///
/// Materializing here rather than hashing the declared records is the whole of
/// the rule above. It throws whatever the materialization throws -- an
/// out-of-range index, a NaN bound, an empty intersection -- so a key can never
/// be taken over a bound set that does not describe a problem.
inline std::uint64_t materialized_bound_digest(const AggregateDeclaration &declaration) {
    Fnv1a hash;
    for (const VariableBound &bound : declaration.materialize_variable_bounds()) {
        detail::feed_variable_bound(hash, bound);
    }
    return hash.value();
}

/// How many times the structures behind an aggregate have been laid.
///
/// A strong type rather than a bare unsigned integer, because the linear layer
/// already carries a NUMERIC epoch of its own as a bare unsigned integer and
/// the two appear in the same diagnostics. Mixing them is a compile error here
/// and would be a silent misreading there.
///
/// Default 0 means "nothing laid yet"; the first laid layout is 1. Equality
/// comparable and nothing else: an epoch answers "the same?", never "how much
/// later?" -- ordering would invite arithmetic on a value whose only guarantee
/// is that it changes.
class StructureEpoch {
  public:
    constexpr StructureEpoch() noexcept = default;
    constexpr explicit StructureEpoch(std::uint64_t value) noexcept : value_(value) {}

    constexpr std::uint64_t value() const noexcept { return value_; }

    friend constexpr bool operator==(StructureEpoch, StructureEpoch) noexcept = default;

  private:
    std::uint64_t value_ = 0;
};

/// The monotone source of structure epochs, held by whatever owns the layout.
///
/// Monotonicity lives here rather than in each provider's discipline: a
/// provider re-lays structures and calls `bump()`, and there is no way to move
/// the counter anywhere but forward.
///
/// `bump()` is a release store and `current()` an acquire load. The pairing is
/// the cross-thread half of the ordering guarantee; the program-order half is
/// an obligation on the provider, which bumps INSIDE the routine that re-lays
/// rather than at the next evaluation's entry.
///
/// The counter is 64-bit and DOES wrap, arithmetically, after 2^64 bumps -- at
/// which point two different structures could compare equal. The guarantee
/// rests on that being unreachable rather than on a check: a structural event
/// is a re-layout, not an iteration, and a process would have to re-lay a
/// problem's structures once a nanosecond for roughly six hundred years to get
/// there. Stated rather than left implicit, so nobody re-derives it as a bug.
class StructureEpochCounter {
  public:
    StructureEpochCounter() noexcept = default;

    StructureEpochCounter(const StructureEpochCounter &) = delete;
    StructureEpochCounter &operator=(const StructureEpochCounter &) = delete;

    StructureEpoch current() const noexcept {
        return StructureEpoch(value_.load(std::memory_order_acquire));
    }

    /// Advances to the next epoch and returns it.
    StructureEpoch bump() noexcept {
        return StructureEpoch(value_.fetch_add(1, std::memory_order_release) + 1);
    }

  private:
    std::atomic<std::uint64_t> value_{0};
};

/// A cheap two-part answer: is this the same structure, and is this the same
/// point?
///
/// The pair rather than either half alone, because a consumer deciding whether
/// it may reuse anything needs both, and comparing one pair is both cheaper and
/// harder to misuse than remembering to compare two values.
///
/// `value_digest_` is a digest of the candidate VALUES at the probed point (see
/// candidate_value_digest in model/candidate_point.h). It is bitwise over the
/// value bytes, and it is only ever compared against another probe taken in the
/// same process by the same binary: it is not a portable pin, and it is never
/// compared against a digest taken over assembled matrices.
struct IdentityProbe {
    StructureEpoch epoch_;
    std::uint64_t value_digest_ = 0;

    friend bool operator==(const IdentityProbe &, const IdentityProbe &) = default;
};

} // namespace hven::solvers
