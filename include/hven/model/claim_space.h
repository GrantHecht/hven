// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// claim_space.h — the claim arenas of the provider-side model contract, and the
// tables a claim pass publishes.
//
// A provider's pieces claim their slots ONCE, at layout time, out of two
// arenas: the KKT arena (one slot per stored matrix element the piece will sum
// into) and the right-hand-side arena (one slot per gradient row and per
// residual row the piece will sum into). The claim pass turns those claims into
// a sparsity pattern and a LOCATION TABLE, and every later evaluation addresses
// its destinations only through that table.
//
// Three types per arena, and they are three because the arena has three
// distinct moments:
//
//   * a CLAIM SPACE is the setup-time cursor a piece claims out of;
//   * a LOCATION TABLE is the published, read-only result of that pass;
//   * a SCATTER VIEW is a consumer-owned destination plus the table that says
//     where in it each claim slot lands.
//
// Nothing here allocates a destination, and nothing here owns one. The consumer
// publishes storage and a table; the provider fills locations someone else
// published. That is one fill path for both consumers of these tables -- an
// engine scattering on its per-minor hot path, and an engine-independent
// scorer evaluating at an arbitrary point off it.
//
// This header is engine-independent by construction: it includes nothing from
// the interior-point machinery and names no engine type.

#include <cstdint>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <Eigen/Core>

#include <fmt/format.h>

namespace hven::solvers {

/// The four kinds of structure a piece may claim slots for. Naming them is what
/// makes "which domains does this piece claim?" a question with a checkable
/// answer, rather than something inferred from which list a piece came out of
/// and from a pair of booleans passed alongside it.
///
/// NOT a request vocabulary. A claim domain names what is CLAIMED at layout
/// time; what is EVALUATED per call is named by EvalRequest
/// (model/candidate_point.h). An objective VALUE, for instance, is evaluable
/// but claims no matrix structure at all, so it has no claim domain.
enum class ClaimDomain { kHessian, kEqualityJacobian, kInequalityJacobian, kVariableBound };

/// A combined selection of claim domains.
class ClaimDomainSet {
  public:
    constexpr ClaimDomainSet() noexcept = default;

    /// A single domain converts to the set containing just it, so
    /// `kHessian | kEqualityJacobian` reads as the selection it is.
    constexpr ClaimDomainSet(ClaimDomain domain) noexcept : bits_(mask(domain)) {}

    constexpr bool contains(ClaimDomain domain) const noexcept {
        return (bits_ & mask(domain)) != 0u;
    }
    constexpr bool empty() const noexcept { return bits_ == 0u; }
    constexpr std::uint32_t bits() const noexcept { return bits_; }

    static constexpr ClaimDomainSet all() noexcept {
        return ClaimDomainSet(mask(ClaimDomain::kHessian) | mask(ClaimDomain::kEqualityJacobian) |
                              mask(ClaimDomain::kInequalityJacobian) |
                              mask(ClaimDomain::kVariableBound));
    }

    constexpr ClaimDomainSet &operator|=(ClaimDomainSet other) noexcept {
        bits_ |= other.bits_;
        return *this;
    }
    constexpr ClaimDomainSet &operator&=(ClaimDomainSet other) noexcept {
        bits_ &= other.bits_;
        return *this;
    }

    friend constexpr ClaimDomainSet operator|(ClaimDomainSet left, ClaimDomainSet right) noexcept {
        return ClaimDomainSet(left.bits_ | right.bits_);
    }
    friend constexpr ClaimDomainSet operator&(ClaimDomainSet left, ClaimDomainSet right) noexcept {
        return ClaimDomainSet(left.bits_ & right.bits_);
    }
    friend constexpr bool operator==(ClaimDomainSet, ClaimDomainSet) noexcept = default;

  private:
    explicit constexpr ClaimDomainSet(std::uint32_t bits) noexcept : bits_(bits) {}

    static constexpr std::uint32_t mask(ClaimDomain domain) noexcept {
        return 1u << static_cast<std::uint32_t>(domain);
    }

    std::uint32_t bits_ = 0u;
};

/// Combines two bare domains. The set-valued operators above are hidden
/// friends, reachable by argument-dependent lookup on ClaimDomainSet; two bare
/// domains name no set, so their union needs an overload of its own.
constexpr ClaimDomainSet operator|(ClaimDomain left, ClaimDomain right) noexcept {
    return ClaimDomainSet(left) | ClaimDomainSet(right);
}

/// Which triangle of the KKT matrix the assembled storage holds.
///
/// Resolved ONCE, here, at claim time -- never on a per-element scatter. The
/// location table a claim pass publishes already accounts for the storage
/// state, so a scatter writes `values[table.location(slot)]` and never branches
/// on which triangle that offset names.
enum class KktStorage { kUpperTriangle, kFull };

/// The setup-time claim cursor handed to a piece: the two writable index
/// arrays, the next free slot, the row base for this piece's kind, the domains
/// being claimed, and the storage state the assembled matrix will be in.
///
/// A claiming piece writes `rows_[next_free_]` / `cols_[next_free_]` for each
/// slot it takes and advances `next_free_` by exactly the count it reported it
/// would take.
struct KktClaimSpace {
    Eigen::Ref<Eigen::VectorXi> rows_; ///< claim slot -> assembled row
    Eigen::Ref<Eigen::VectorXi> cols_; ///< claim slot -> assembled column
    int next_free_ = 0;                ///< advanced by each claiming piece
    int constraint_row_offset_ = 0;    ///< row base for this piece's kind
    ClaimDomainSet domains_;           ///< which domains are being claimed now
    KktStorage storage_ = KktStorage::kUpperTriangle;
};

/// The published, read-only result of the KKT claim pass: claim slot -> offset
/// into the assembled value array, plus the per-column clash marks and the
/// mutex vector those marks index.
///
/// The three are published as ONE type because they are never used apart: every
/// scatter site needs the offset, needs to know whether the column it is
/// writing is contested, and must key its lock the same way every other
/// claimant of that slot does. Publishing them together is what makes the
/// shared keying structural rather than a convention each site re-implements.
///
/// A non-owning view. The arrays and the mutex vector belong to whoever laid
/// the arena; a table must not outlive them.
///
/// The three element accessors are unchecked, deliberately: they sit on a
/// per-element scatter loop, and the invariants they would re-check are
/// established ONCE here, at construction. What construction validates is that
/// the published arrays are consistent with each other -- so a clash mark can
/// never index past the mutex vector, and `lock(clash_lock(col))` is
/// well-defined for every column the table describes. A claim slot handed to
/// `location()` is the CALLER's own claim range, which the caller validates at
/// its own boundary.
class KktLocationTable {
  public:
    KktLocationTable() noexcept = default;

    KktLocationTable(const int *locations, int size, const int *clashes, int clash_size,
                     std::vector<std::mutex> *locks)
        : locations_(locations), size_(size), clashes_(clashes), clash_size_(clash_size),
          locks_(locks) {
        if (size_ < 0 || clash_size_ < 0) {
            throw std::invalid_argument(
                fmt::format("KktLocationTable: negative array size (locations {0}, clash marks "
                            "{1})",
                            size_, clash_size_));
        }
        if (size_ > 0 && locations_ == nullptr) {
            throw std::invalid_argument(fmt::format(
                "KktLocationTable: {0} claim slots were published with no location array", size_));
        }
        if (clash_size_ > 0 && clashes_ == nullptr) {
            throw std::invalid_argument(
                fmt::format("KktLocationTable: {0} columns were published with no clash-mark array",
                            clash_size_));
        }
        const int lock_count = locks_ == nullptr ? 0 : static_cast<int>(locks_->size());
        for (int column = 0; column < clash_size_; ++column) {
            const int mark = clashes_[column];
            // -1 is the ONLY legal negative -- it is the uncontested sentinel.
            // Any other negative reaches lock() through a scatter that tested
            // `mark != -1`, and indexing the mutex vector with it is undefined
            // behaviour: exactly what this constructor exists to make
            // unreachable, and what an upper-bound check alone lets through.
            if (mark < -1) {
                throw std::invalid_argument(fmt::format(
                    "KktLocationTable: column {0} carries clash mark {1}; the only negative mark "
                    "is -1, meaning uncontested",
                    column, mark));
            }
            if (mark >= lock_count) {
                throw std::invalid_argument(fmt::format(
                    "KktLocationTable: column {0} is marked contested with lock index {1}, but "
                    "only {2} locks were published",
                    column, mark, lock_count));
            }
        }
    }

    /// Offset of a claim slot into the assembled value array.
    int location(int claim_slot) const noexcept { return locations_[claim_slot]; }

    /// Lock index for a canonical column, or -1 when the column is uncontested.
    int clash_lock(int canonical_col) const noexcept { return clashes_[canonical_col]; }

    /// The mutex a clash mark indexes.
    std::mutex &lock(int lock_index) const noexcept {
        return (*locks_)[static_cast<std::size_t>(lock_index)];
    }

    int size() const noexcept { return size_; }
    int clash_size() const noexcept { return clash_size_; }
    bool empty() const noexcept { return size_ == 0; }

  private:
    const int *locations_ = nullptr;
    int size_ = 0;
    const int *clashes_ = nullptr;
    int clash_size_ = 0;
    std::vector<std::mutex> *locks_ = nullptr;
};

/// The consumer-owned KKT destination a provider fills: a span the consumer
/// already allocated, plus the table saying where in it each claim slot lands.
/// No matrix type and no ownership -- the provider allocates nothing on the
/// fill path.
///
/// Legally empty for a call whose request names no KKT-bearing output.
struct KktScatterView {
    double *values_ = nullptr;
    int size_ = 0;
    const KktLocationTable *locations_ = nullptr;

    bool empty() const noexcept {
        return values_ == nullptr || size_ == 0 || locations_ == nullptr;
    }
};

/// The four right-hand-side arrays, each its own claim arena with its own
/// cursor. They are a separate space from the KKT arena and carry no locking:
/// folding them into the KKT types would put a lock surface on a space that has
/// no contention.
enum class RhsArena {
    kObjectiveGradient,
    kConstraintAdjointGradient,
    kEqualityResiduals,
    kInequalityResiduals
};

/// The setup-time claim cursor for one right-hand-side arena.
struct RhsClaimSpace {
    Eigen::Ref<Eigen::VectorXi> rows_; ///< claim slot -> row of the assembled array
    int next_free_ = 0;                ///< advanced by each claiming piece
    RhsArena arena_ = RhsArena::kObjectiveGradient;
};

/// The published, read-only result of one arena's claim pass: claim slot -> row
/// of the assembled array.
///
/// A location of -1 means the row was DROPPED: an eliminated variable's
/// stationarity row is not part of the reduced problem's residual, so the claim
/// survives a variable's elimination while its destination does not. A filler
/// skips those slots rather than writing them anywhere.
///
/// Non-owning, and unchecked per element for the same reason KktLocationTable
/// is; construction validates the published array.
class RhsLocationTable {
  public:
    RhsLocationTable() noexcept = default;

    RhsLocationTable(const int *rows, int size) : rows_(rows), size_(size) {
        if (size_ < 0) {
            throw std::invalid_argument(
                fmt::format("RhsLocationTable: negative array size ({0})", size_));
        }
        if (size_ > 0 && rows_ == nullptr) {
            throw std::invalid_argument(fmt::format(
                "RhsLocationTable: {0} claim slots were published with no row array", size_));
        }
        for (int slot = 0; slot < size_; ++slot) {
            // Same sentinel discipline as the KKT table's clash marks, though
            // NOT for the same reason -- the reason here is the weaker
            // protocol, not the stronger one. A filler that tests `row >= 0`
            // skips -1 and -3 alike and is safe either way; the filler that
            // does NOT test, and indexes the destination directly, is unsafe on
            // -1 exactly as it is on -3. So there is no arithmetic that makes
            // one negative worse than another. What this check buys is the
            // guarantee the unchecked protocol needs: every published mark is
            // either the one sentinel a skipping filler recognizes, or an index
            // that is genuinely in range. A stray negative belongs to neither
            // set, and the only place to say so is here, once, where the table
            // is published.
            if (rows_[slot] < -1) {
                throw std::invalid_argument(
                    fmt::format("RhsLocationTable: claim slot {0} maps to row {1}; the only "
                                "negative row is -1, meaning dropped",
                                slot, rows_[slot]));
            }
        }
    }

    /// Row of the assembled array a claim slot lands in, or -1 when the row was
    /// dropped.
    int location(int claim_slot) const noexcept { return rows_[claim_slot]; }

    int size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

  private:
    const int *rows_ = nullptr;
    int size_ = 0;
};

/// One arena's half of a right-hand-side scatter destination: the consumer's
/// storage plus the table addressing it. Legally empty for an arena the request
/// does not name.
struct RhsArenaView {
    double *values_ = nullptr;
    int size_ = 0;
    const RhsLocationTable *locations_ = nullptr;

    bool empty() const noexcept {
        return values_ == nullptr || size_ == 0 || locations_ == nullptr;
    }
};

/// The consumer-owned right-hand-side destination a provider fills: the four
/// arenas, plus an explicit out slot for the scalar objective value.
///
/// The scalar gets a slot of its own rather than a repurposed arena, because a
/// value is not a vector of claims and pretending otherwise would put a
/// one-element arena on every objective evaluation.
struct RhsScatterView {
    RhsArenaView objective_gradient_;
    RhsArenaView constraint_adjoint_gradient_;
    RhsArenaView equality_residuals_;
    RhsArenaView inequality_residuals_;
    double *objective_ = nullptr;
};

} // namespace hven::solvers
