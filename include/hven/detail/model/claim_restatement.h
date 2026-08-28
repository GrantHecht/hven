// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// claim_restatement.h — the pure index work behind a laid provider's
// domain-contiguous claim stream, and the arena that stream lives in.
//
// WHAT THIS IS FOR. A partitioned provider hands its claim slots out in
// EMISSION order: partition-major, and within a partition objective pieces,
// then equality pieces, then inequality pieces, with each piece's Jacobian and
// Hessian slots interleaved as that piece chose to claim them. The claim-stream
// contract (model/claim_stream_source.h) wants something else -- one contiguous
// run per DOMAIN, stated in the square assembled space the declaration
// describes, n + me + mi on a side, with no slack block and Hessian pairs in the
// upper triangle. The two orders are a permutation of one another plus two
// per-slot restatements, and this is where that permutation is taken.
//
// WHY IT IS A FREE FUNCTION OVER PLAIN VIEWS rather than a method reading a
// layout's members: the refusals below are the whole reason a consumer can trust
// the published stream, and a refusal that can only be reached by corrupting a
// laid layout's members is a refusal no test can exercise. Stated over its
// inputs, every one of them is a case a test can construct directly.
//
// The implementation lives in src/model/non_linear_program.cpp, beside the one
// layout that drives it.

#include <utility>

#include <Eigen/Core>

#include "hven/model/claim_stream_source.h"

namespace hven::solvers::detail {

class ClaimArena;
struct ClaimDomainCounts;
struct RawClaimLayout;

ClaimArena restate_claim_stream(const RawClaimLayout &raw, const ClaimDomainCounts &counts);

/// @brief How many claim slots each domain takes, as the layout's own element
///        counts report them.
///
/// The three are the sizing input to a restatement AND the assertion it is
/// checked against: the classification pass below counts its own slots per
/// domain and refuses if the two disagree, which is how a piece whose
/// `num_kkt_elements` is not additive in its two flags is caught rather than
/// silently overrunning a domain run.
struct ClaimDomainCounts {
    int hessian_ = 0;
    int equality_jacobian_ = 0;
    int inequality_jacobian_ = 0;

    int total() const { return hessian_ + equality_jacobian_ + inequality_jacobian_; }

    friend bool operator==(const ClaimDomainCounts &, const ClaimDomainCounts &) = default;
};

/// @brief Everything a restatement reads, as non-owning views over the arrays a
///        lay has just written, plus the dimensions the claim convention is
///        stated in.
///
/// `segment_marks_` is the raw-slot cursor recorded at each intra-partition
/// boundary: for P partitions it holds 3P + 1 entries, where entry 3p opens
/// partition p, 3p+1 closes its objective pieces, 3p+2 closes its equality
/// pieces, and 3p+3 closes the partition. A MARK IS NOT AN OFFSET -- it is an
/// absolute cursor into the RAW emission order, and the published offsets are
/// claim-order and domain-relative; the restatement derives the latter from its
/// own output cursors, never from these.
///
/// The dimensions are the DECLARATION's: `primal_vars_` is the full primal
/// width, not a reduced one. A reduced layout cannot be restated into
/// declaration space at all and never reaches here (see the caller).
struct RawClaimLayout {
    Eigen::Ref<const Eigen::VectorXi> raw_rows_;
    Eigen::Ref<const Eigen::VectorXi> raw_cols_;
    Eigen::Ref<const Eigen::VectorXi> segment_marks_;
    Eigen::Ref<const Eigen::VectorXi> gradient_rows_;

    int primal_vars_ = 0;
    int slack_vars_ = 0;
    int equality_rows_ = 0;
    int inequality_rows_ = 0;
    int partitions_ = 0;
};

/// @brief ONE owned integer arena holding a whole restated claim stream, cut
///        into the six views the published surface hands out.
///
/// One allocation, not six: every accessor below is a view into `storage_`, so a
/// rebuild is a single allocation swapped in and a retained stream is a single
/// block held. The cut is
///
///     [ rows (N) | cols (N) | gradient rows (G) | offsets_H | offsets_Ae | offsets_Ai ]
///
/// with each offset table `partitions_ + 1` long.
///
/// PARTITION OFFSETS, precisely: `offsets_d[p] .. offsets_d[p + 1]` is partition
/// p's run WITHIN domain d, stated relative to that domain's own
/// `ClaimBlock::start_`. An empty partition gives equal adjacent entries;
/// `offsets_d[0]` is 0 and `offsets_d[P]` is the domain's run length. The
/// encoding is lossless because each domain's run is partition-major by
/// construction -- the partition loop is the outer one at the lay.
class ClaimArena {
  public:
    /// Whether anything is published. False on a default-constructed arena and
    /// on one a caller has dropped.
    bool empty() const { return storage_.size() == 0; }

    int slots() const { return slots_; }
    int gradient_slots() const { return gradient_; }
    int partitions() const { return partitions_; }

    Eigen::Ref<const Eigen::VectorXi> rows() const { return storage_.segment(0, slots_); }
    Eigen::Ref<const Eigen::VectorXi> cols() const { return storage_.segment(slots_, slots_); }
    Eigen::Ref<const Eigen::VectorXi> gradient_rows() const {
        return storage_.segment(2 * slots_, gradient_);
    }
    Eigen::Ref<const Eigen::VectorXi> hessian_offsets() const {
        return storage_.segment(offsets_start(), partitions_ + 1);
    }
    Eigen::Ref<const Eigen::VectorXi> equality_offsets() const {
        return storage_.segment(offsets_start() + (partitions_ + 1), partitions_ + 1);
    }
    Eigen::Ref<const Eigen::VectorXi> inequality_offsets() const {
        return storage_.segment(offsets_start() + 2 * (partitions_ + 1), partitions_ + 1);
    }

    const ClaimBlock &hessian() const { return hessian_; }
    const ClaimBlock &equality_jacobian() const { return equality_jacobian_; }
    const ClaimBlock &inequality_jacobian() const { return inequality_jacobian_; }

    /// THE COMMIT. A freshly built arena is exchanged with a published one in
    /// one call that cannot throw, which is what lets a restatement be built as
    /// a local and committed only once it has succeeded.
    void swap(ClaimArena &other) noexcept {
        storage_.swap(other.storage_);
        std::swap(slots_, other.slots_);
        std::swap(gradient_, other.gradient_);
        std::swap(partitions_, other.partitions_);
        std::swap(hessian_, other.hessian_);
        std::swap(equality_jacobian_, other.equality_jacobian_);
        std::swap(inequality_jacobian_, other.inequality_jacobian_);
    }

    /// Releases the storage and reports nothing. Used where a stream cannot be
    /// restated for the layout now on hand: a dropped arena refuses at the
    /// accessor, where a retained one would answer with coordinates naming a
    /// layout that no longer exists.
    void clear() noexcept {
        ClaimArena empty_arena;
        this->swap(empty_arena);
    }

  private:
    friend ClaimArena restate_claim_stream(const RawClaimLayout &, const ClaimDomainCounts &);

    int offsets_start() const { return 2 * slots_ + gradient_; }

    Eigen::VectorXi storage_;
    int slots_ = 0;
    int gradient_ = 0;
    int partitions_ = 0;
    ClaimBlock hessian_;
    ClaimBlock equality_jacobian_;
    ClaimBlock inequality_jacobian_;
};

/// @brief Restates one laid, UNREDUCED emission-order claim stream into the
///        domain-contiguous claim convention, and returns the arena holding it.
///
/// Sort-free and single-pass: the domain runs are already partition-major by
/// construction, so the permutation is three output cursors walked in step with
/// one read cursor. Two per-slot restatements ride on it -- a constraint row
/// drops the slack offset the assembled space carries and the claim convention
/// does not, and a Hessian pair is ordered (min, max) into the upper triangle
/// rather than left in the walk order the piece claimed it in.
///
/// @param raw    the laid arrays and the declaration's dimensions.
/// @param counts the per-domain slot counts the layout's element counts report.
/// @return the restated arena, ready to be swapped into place.
///
/// @throws std::invalid_argument if the inputs do not describe one laid,
///         unreduced layout in the claim convention. Every refusal names the
///         slot and the coordinate:
///           * a mark table that is not 3P + 1 long, not monotone, or does not
///             open at 0 and close at the slot count;
///           * a NEGATIVE coordinate -- how a REDUCED layout records an
///             eliminated variable, and therefore a contradiction here, where
///             the caller has already established that nothing is eliminated;
///           * a claimed SLACK row -- a row between the primal block and the
///             equality block, which the assembled space has and the claim
///             convention does not, so no sound layout claims one;
///           * a coordinate outside the declared space, a constraint row
///             outside its own domain's row band, or an objective-gradient row
///             outside the declared variables.
/// @throws std::logic_error if the classification's own per-domain slot counts
///         disagree with @p counts -- the one way a piece whose
///         `num_kkt_elements` is not additive in its two flags shows up.
ClaimArena restate_claim_stream(const RawClaimLayout &raw, const ClaimDomainCounts &counts);

} // namespace hven::solvers::detail
