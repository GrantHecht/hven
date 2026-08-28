// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// structure_identity.h — identity for consumers of the model contract:
//
//   * DeclarationKey     -- "is this the same declared PROBLEM?"
//   * ModelStructureKey  -- "is this the same LAID structure?"
//   * StructureEpoch     -- "have the structures been re-laid since I looked?"
//   * IdentityProbe      -- "same structure AND same point?", in one comparison
//
// None of these is the engine's own structural digest: that one is a function
// of ASSEMBLED matrices and answers a factorization-reuse question, while these
// are functions of the DECLARATION. Engine-independent by construction: nothing
// here includes from the interior-point machinery.
//
// DeclarationKey and ModelStructureKey answer different questions, and must
// never be compared against one another or substituted for one another:
//
//   * ModelStructureKey answers "may I reuse this factorization?". It is taken
//     over the provider's laid claim slots in EMISSION order, plus the adopted
//     partition count, so it moves when the LAYOUT moves. It is therefore
//     ENGINE-SPECIFIC and TREATMENT-SPECIFIC.
//
//     EMISSION order, said that way deliberately: it is the order the pieces
//     were handed their slots in as the layout was laid, which for a partitioned
//     provider is partition-major and within a partition piece-major. A provider
//     may ALSO publish its claims restated into some other order -- a
//     domain-contiguous claim stream, say -- and that restatement is a different
//     sequence over the same slots. This digest is taken over the laid slots, in
//     the order they were laid, and over nothing else. The VALUE is unchanged by
//     this sentence; what changes is that the sentence still says which order it
//     means once a second one exists.
//
//   * DeclarationKey answers "does this value describe the problem I am about
//     to solve?", and is what the warm-start currency stamps with. It is taken
//     over the DECLARATION alone, so it is the same value on both engines and
//     under every fixed-variable treatment. Staleness under it means the caller
//     transcribed a different problem.
//
// DeclarationKey covers less than "the declared problem" suggests; what it
// deliberately excludes is listed at declaration_identity_digest in
// src/model/aggregate_declaration.cpp.

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

/// @brief The structural key of a declared model: the digest of its claim
///        structure, the partition count that structure was laid at, and the
///        digest of its variable-bound structure.
///
/// Each conjunct has one public builder and no other way in:
/// `claim_digest_` is computed by claim_stream_digest, `bound_digest_` by
/// materialized_bound_digest, and `partition_count_` is the count
/// NlpAggregate::negotiate_partition_count actually adopted. Filling a field
/// from anything else is how a key stops answering the question it exists for.
///
/// `partition_count_` is deliberately explicit though the claim stream is
/// already partition-sensitive: an inspectable field where an order-sensitivity
/// is not.
struct ModelStructureKey {
    std::uint64_t claim_digest_ = 0;
    int partition_count_ = 0;
    std::uint64_t bound_digest_ = 0;

    friend bool operator==(const ModelStructureKey &, const ModelStructureKey &) = default;

    /// @brief The three conjuncts folded into one value, for diagnostics and
    ///        for consumers that want a single number to carry. Comparing
    ///        folded digests is weaker than comparing keys -- prefer `==`.
    std::uint64_t digest() const noexcept {
        Fnv1a hash;
        hash.feed_index(static_cast<std::int64_t>(claim_digest_));
        hash.feed_index(static_cast<std::int64_t>(partition_count_));
        hash.feed_index(static_cast<std::int64_t>(bound_digest_));
        return hash.value();
    }
};

/// @brief The structural key of a declared PROBLEM: a digest of the
///        declaration's mathematical identity, and a digest of its variable
///        bound structure.
///
/// The warm-start currency's stamp. Two conjuncts rather than one so that a
/// caller reading a refusal can tell whether the ROWS moved or the BOX did.
///
/// Engine- and treatment-independent: both conjuncts read only what the CALLER
/// declared, never what a provider or a policy decided.
struct DeclarationKey {
    /// The declared problem's mathematical identity: declaration_identity_digest.
    std::uint64_t declaration_digest_ = 0;
    /// The declared bound structure: materialized_bound_digest, the same
    /// builder ModelStructureKey's own bound conjunct uses.
    std::uint64_t bound_digest_ = 0;

    friend bool operator==(const DeclarationKey &, const DeclarationKey &) = default;

    /// @brief The two conjuncts folded into one value, for diagnostics and for
    ///        consumers that want a single number to carry. Comparing folded
    ///        digests is weaker than comparing keys -- prefer `==`.
    std::uint64_t digest() const noexcept {
        Fnv1a hash;
        hash.feed_index(static_cast<std::int64_t>(declaration_digest_));
        hash.feed_index(static_cast<std::int64_t>(bound_digest_));
        return hash.value();
    }
};

namespace detail {

/// @brief Opens a claim stream with the declared dimensions -- primal
///        variables, equality rows, inequality rows -- before the first claim
///        is fed.
///
/// The preamble makes a continued accumulation self-delimiting: two
/// declarations can hand out an identical claim stream at different dimensions
/// -- a claim names rows and columns that exist, never the size of the space
/// they live in -- and without the preamble those would key identically while
/// laying different systems.
constexpr void feed_dimensions(Fnv1a &hash, int primal_vars, int equality_rows,
                               int inequality_rows) noexcept {
    hash.feed_index(primal_vars);
    hash.feed_index(equality_rows);
    hash.feed_index(inequality_rows);
}

/// @brief Feeds THE WHOLE CLAIM STREAM into a running accumulator, in EMISSION
///        order, after the dimension preamble: claim i is the pair
///        (rows[i], cols[i]), and the pairs are fed interleaved.
/// @param hash   The accumulator, already carrying the dimension preamble.
/// @param rows   The claim rows, in emission order.
/// @param cols   The claim columns, in emission order.
/// @param count  How many claims the stream carries.
///
/// The digest hashes the DECLARED (row, column) claim stream rather than the
/// assembled pattern: the stream is available at layout time without assembling
/// anything, and it keeps this key structurally independent of any digest taken
/// over assembled matrices. Order is significant -- a re-partitioned
/// declaration hands its claims out in a different order and hashes
/// differently, which is the point.
constexpr void feed_claims(Fnv1a &hash, const int *rows, const int *cols,
                           std::size_t count) noexcept {
    hash.feed_index_pairs(rows, cols, count);
}

/// @brief Feeds ONE variable's MATERIALIZED bound structure into a running
///        accumulator, in variable order.
///
/// A variable's bound is DECLARED as a history of records intersected
/// tightest-wins, and it is the INTERSECTION that decides the layout -- so two
/// histories intersecting to the same per-variable structure must key the same.
/// What is fed, per variable: which sides are finite, and whether the two sides
/// coincide -- both read off the intersection result. Bound VALUES are not fed:
/// values enter only through their structural consequences, so a value change
/// that alters no variable's materialized finiteness or fixedness never
/// re-keys the problem. The two changes that DO move the layout still move the
/// key: an intersection making a variable fixed (it can then be eliminated
/// from the solved system entirely), and an intersection making a previously
/// infinite side finite (it changes which barrier terms exist).
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

/// @brief THE claim-structure conjunct of a declaration's structural key, and
///        the only public way to compute one: the dimension preamble, then the
///        provider's laid claim slots in EMISSION order. The claims are taken as
///        the two index arrays a claim arena already holds.
///
/// EMISSION order, not any restated order a provider may also publish -- see the
/// ModelStructureKey banner at the top of this header for why the distinction
/// has to be spelled out. Feeding a restatement here would key a layout twice.
///
/// @throws std::invalid_argument if the two arrays disagree in length: a claim
///         is a (row, column) pair, and a stream missing half of one is not a
///         stream.
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

/// @brief THE bound-structure conjunct of a declaration's structural key, and
///        the only public way to compute one: the materialized per-variable
///        structure, in variable order.
///
/// @throws std::invalid_argument whatever materialize_variable_bounds throws --
///         an out-of-range index, a NaN bound, an empty intersection -- so a
///         key can never be taken over a bound set that does not describe a
///         problem.
inline std::uint64_t materialized_bound_digest(const AggregateDeclaration &declaration) {
    Fnv1a hash;
    for (const VariableBound &bound : declaration.materialize_variable_bounds()) {
        detail::feed_variable_bound(hash, bound);
    }
    return hash.value();
}

/// @brief THE declaration-identity conjunct of a DeclarationKey, and the only
///        public way to compute one: the DECLARED DIMENSIONS -- primal
///        variables, USER equality rows, and inequality rows -- through the
///        same self-delimiting preamble claim_stream_digest opens with.
///
/// The equality count fed is `equality_rows_ - fixing_rows_`: the internal rows
/// a MakeConstraint treatment appends are the TREATMENT's rows, not the
/// declaration's.
///
/// The claim stream, the partition count and thread modes, and the per-piece
/// row structure are deliberately NOT fed; the exclusions and the argument for
/// each are stated at the definition, in src/model/aggregate_declaration.cpp.
/// A caller must therefore read this digest as "the same declared SHAPE and
/// BOX", never as "the same functions": two declarations agreeing on dimensions
/// and on bound structure key the same even if their constraint functions
/// differ.
///
/// @param declaration the declared problem.
/// @return the digest.
/// @throws std::invalid_argument if `fixing_rows_` is negative or exceeds
///         `equality_rows_` -- a fixing-row count that is not a legal split of
///         the equality row space is one this function cannot read, and
///         guessing at the split would key two different problems the same.
std::uint64_t declaration_identity_digest(const AggregateDeclaration &declaration);

/// @brief The whole DeclarationKey of a declared problem: both conjuncts,
///        through their own builders.
///
/// @param declaration the declared problem.
/// @return the key.
/// @throws std::invalid_argument whatever either conjunct throws --
///         declaration_identity_digest's fixing-row refusal above, or
///         materialize_variable_bounds' out-of-range index, NaN bound or empty
///         intersection -- so a key can never be taken over a declaration that
///         does not describe a problem.
DeclarationKey declaration_key(const AggregateDeclaration &declaration);

/// @brief How many times the structures behind an aggregate have been laid.
///
/// A strong type rather than a bare unsigned integer, because the linear layer
/// already carries a NUMERIC epoch of its own as a bare unsigned integer and
/// the two appear in the same diagnostics. Default 0 means "nothing laid yet";
/// the first laid layout is 1. Equality comparable and nothing else: an epoch
/// answers "the same?", never "how much later?".
class StructureEpoch {
  public:
    constexpr StructureEpoch() noexcept = default;
    constexpr explicit StructureEpoch(std::uint64_t value) noexcept : value_(value) {}

    constexpr std::uint64_t value() const noexcept { return value_; }

    friend constexpr bool operator==(StructureEpoch, StructureEpoch) noexcept = default;

  private:
    std::uint64_t value_ = 0;
};

/// @brief The monotone source of structure epochs, held by whatever owns the
///        layout: a provider re-lays structures and calls bump(), and there is
///        no way to move the counter anywhere but forward.
///
/// `bump()` is a release store and `current()` an acquire load; that pairing is
/// the cross-thread half of the ordering guarantee, and the program-order half
/// is an obligation on the provider, which bumps INSIDE the routine that
/// re-lays rather than at the next evaluation's entry. The counter is 64-bit
/// and DOES wrap arithmetically after 2^64 bumps, at which point two different
/// structures could compare equal -- the guarantee rests on that being
/// unreachable (a structural event is a re-layout, not an iteration).
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

/// @brief A cheap two-part answer: is this the same structure, and is this the
///        same point?
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
