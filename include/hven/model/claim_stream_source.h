// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// claim_stream_source.h — the claim-stream half of the provider surface: an
// NlpAggregate that also publishes, per claim slot, the assembled coordinate
// that slot names.
//
// WHY IT IS ITS OWN INTERFACE. NlpAggregate (model/nlp_aggregate.h) publishes a
// declaration, a structure epoch and the evaluation entries -- everything a
// consumer needs in order to ASK for a fill, given a destination and a location
// table it already has. It does not publish WHERE the fill lands. A consumer
// that lays its own destination needs the second half too: the per-slot
// (row, column) pairs, and the per-domain slot ranges, from which it builds the
// destination and the location table the provider is then scattered through.
// Those are what this interface adds, and nothing else. A provider whose
// consumers always hand it a destination they laid elsewhere keeps deriving
// from the base.
//
// THE COORDINATE CONVENTION. Claims are stated in the square assembled space
// the declaration describes -- n + me + mi on a side, laid
// [primal | equality rows | inequality rows]. A Hessian claim names (i, j) with
// i <= j, the upper triangle; an equality Jacobian claim names (n + r, c); an
// inequality Jacobian claim names (n + me + r, c). Objective-gradient claims
// name a row of the primal block alone. The three dimensions come from the
// declaration, so this interface adds no accessor for them.
//
// WHAT IT IS NOT. Not a second contract: every obligation an implementation
// carries -- the concurrency posture, the epoch's ordering guarantee, partition
// invariance -- is the base's, unchanged. This interface adds accessors and no
// rule of its own.

#include <Eigen/Core>

#include "hven/model/nlp_aggregate.h"

namespace hven::solvers {

/// @brief One contiguous run of claim slots, as [start, start + count).
struct ClaimBlock {
    int start_ = 0;
    int count_ = 0;

    friend bool operator==(const ClaimBlock &, const ClaimBlock &) = default;
};

/// @brief A provider that publishes its claim stream: an NlpAggregate a
///        consumer can lay a destination for.
///
/// Abstract, and abstract for the same reason the base is: these accessors are
/// read once per lay, never per element, so the dispatch cost is immaterial and
/// one declaration reads as one contract.
///
/// STREAM SHAPE. Claims are issued serially by the provider, in partition-index
/// order, and never from worker threads; each domain's claims -- Lagrangian
/// Hessian, equality Jacobian, inequality Jacobian -- occupy one contiguous
/// slot range. The claim stream is therefore a pure function of the declaration
/// and the adopted partition count, and a consumer may address it as three runs
/// rather than as a scatter of slots.
///
/// VIEW VALIDITY. Everything published here describes the structures as last
/// laid. A re-lay replaces them, and announces itself through the base's
/// structure epoch: a consumer re-reads these accessors whenever the epoch it
/// laid against has moved, and holds no view across one.
class ClaimStreamSource : public NlpAggregate {
  public:
    /// @brief Claim slot to assembled KKT row, in claim order.
    ///
    /// One entry per claim slot, in the serial partition-index order the stream
    /// shape fixes. The returned view is valid until the next re-lay, which the
    /// structure epoch announces.
    virtual Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const = 0;

    /// @brief Claim slot to assembled KKT column, in claim order.
    ///
    /// Paired element for element with kkt_claim_rows(), in the same serial
    /// partition-index order. The returned view is valid until the next re-lay,
    /// which the structure epoch announces.
    virtual Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const = 0;

    /// @brief The KKT claim slots the Lagrangian Hessian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded by the next
    /// re-lay, which the structure epoch announces.
    virtual ClaimBlock hessian_claims() const = 0;

    /// @brief The KKT claim slots the equality Jacobian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded by the next
    /// re-lay, which the structure epoch announces.
    virtual ClaimBlock equality_jacobian_claims() const = 0;

    /// @brief The KKT claim slots the inequality Jacobian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded by the next
    /// re-lay, which the structure epoch announces.
    virtual ClaimBlock inequality_jacobian_claims() const = 0;

    /// @brief Claim slot to row of the objective-gradient arena.
    ///
    /// One entry per claim slot of that arena, in the same serial
    /// partition-index order the KKT stream carries. The returned view is valid
    /// until the next re-lay, which the structure epoch announces.
    virtual Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const = 0;
};

} // namespace hven::solvers
