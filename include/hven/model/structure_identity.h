// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

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
#include <cstdint>
#include <limits>

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

/// Feeds ONE declared claim into a running accumulator, in claim order.
///
/// The claim-structure digest hashes the DECLARED (row, column) claim stream
/// rather than the assembled pattern: the stream is available at layout time
/// without assembling anything, it is exactly what a layout-determinism check
/// asserts on, and it keeps this key structurally independent of any digest
/// taken over assembled matrices. Order is significant -- a re-partitioned
/// declaration hands its claims out in a different order and hashes
/// differently, which is the point.
constexpr void feed_claim(Fnv1a &hash, int row, int col) noexcept {
    hash.feed_index(row);
    hash.feed_index(col);
}

/// Feeds ONE variable bound's STRUCTURE into a running accumulator, in
/// declaration order.
///
/// Structure, not value: which variable, which sides are finite, and whether
/// the two sides coincide. Bound VALUES are deliberately not fed. A finite
/// bound moving to another finite value does not move the layout -- the same
/// variable is bounded on the same sides and the same system is factorized --
/// so it must not move a key whose whole job is to say whether the layout can
/// be reused. The one value change that DOES move the layout is a variable
/// becoming fixed (its two bounds coinciding), because a fixed variable can be
/// eliminated from the solved system entirely, and that is exactly what the
/// third bit records.
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
