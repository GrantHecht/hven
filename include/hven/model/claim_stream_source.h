// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// claim_stream_source.h — the claim-stream half of the provider surface: an
// NlpAggregate that also publishes, per claim slot, the assembled coordinate
// that slot names.
//
// COORDINATE CONVENTION. Claims are stated in the square assembled space the
// declaration describes -- n + me + mi on a side, laid
// [primal | equality rows | inequality rows]. A Hessian claim names (i, j) with
// i <= j, the upper triangle; an equality Jacobian claim names (n + r, c); an
// inequality Jacobian claim names (n + me + r, c). Objective-gradient claims
// name a row of the primal block alone. The three dimensions come from the
// declaration, so this interface adds no accessor for them.

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
///        consumer can lay a destination for. Abstract, like the base.
///
/// STREAM SHAPE. Claims are issued serially by the provider, in partition-index
/// order, and never from worker threads; each domain's claims -- Lagrangian
/// Hessian, equality Jacobian, inequality Jacobian -- occupy one contiguous
/// slot range. The claim stream is therefore a pure function of the declaration
/// and the adopted partition count, and a consumer may address it as three runs
/// rather than as a scatter of slots.
///
/// VIEW VALIDITY, AND THE EPOCH IT KEYS ON. Everything published here describes
/// the structures as last laid, and the epoch that governs it is
/// claim_stream_epoch(), NOT the base's structure epoch. The two answer
/// different questions: structure_epoch() moves on every re-lay, while
/// claim_stream_epoch() moves only when the published claim stream itself stops
/// describing the layout. A consumer re-reads these accessors whenever the
/// claim-stream epoch it read against has moved, and holds no view across one.
/// Pointer identity is NOT the contract: a provider may republish the same
/// storage or different storage under a moved epoch, and a consumer may not
/// infer validity from an address that happens not to have changed.
///
/// THE GAP BETWEEN THE TWO EPOCHS IS DELIBERATE, and it is what an
/// ELIMINATION-ONLY RE-LAY buys. A provider that eliminates bound-fixed
/// variables from the system it factorizes re-lays its raw structures without
/// changing the DECLARED claim structure this stream is stated in -- same
/// declaration, same claim counts, same partition count. The published stream
/// still describes that structure exactly, so it is RETAINED: the claim-stream
/// epoch does not move even though the structure epoch does, and a view held
/// across such a re-lay stays valid. A consumer keyed on the structure epoch
/// would re-read for nothing; one keyed on the claim-stream epoch holds what it
/// already has.
///
/// A provider whose claim stream is rebuilt at every re-lay satisfies this term
/// by returning its structure epoch -- which is what the default below does, and
/// what makes the term no new obligation for such a provider. A provider that
/// cannot restate its stream for the layout it now holds refuses at the
/// accessor rather than publishing a stream naming coordinates the layout does
/// not have.
class ClaimStreamSource : public NlpAggregate {
  public:
    /// @brief The epoch the views published here are valid under.
    ///
    /// The claim-stream half of the epoch pair described above: it moves when
    /// the published stream stops describing the layout, and NOT on a re-lay
    /// that leaves the declared claim structure standing. It never moves
    /// backwards and it is equality-comparable and nothing else, exactly like
    /// the structure epoch.
    ///
    /// The DEFAULT is the structure epoch, and that identity is the honest
    /// answer for any provider that rebuilds its claim stream at every lay: such
    /// a provider's stream is superseded exactly when its structures are, so the
    /// two epochs carry the same information and returning one for the other
    /// states a fact rather than hiding one. Only a provider that RETAINS a
    /// published stream across a re-lay has anything to override this with.
    virtual StructureEpoch claim_stream_epoch() const { return this->structure_epoch(); }

    /// @brief Claim slot to assembled KKT row, in claim order.
    ///
    /// One entry per claim slot, in the serial partition-index order the stream
    /// shape fixes. The returned view is valid until the claim-stream epoch
    /// moves, which is not every re-lay -- see VIEW VALIDITY above.
    virtual Eigen::Ref<const Eigen::VectorXi> kkt_claim_rows() const = 0;

    /// @brief Claim slot to assembled KKT column, in claim order.
    ///
    /// Paired element for element with kkt_claim_rows(), in the same serial
    /// partition-index order. The returned view is valid until the claim-stream
    /// epoch moves, which is not every re-lay -- see VIEW VALIDITY above.
    virtual Eigen::Ref<const Eigen::VectorXi> kkt_claim_cols() const = 0;

    /// @brief The KKT claim slots the Lagrangian Hessian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded when the
    /// claim-stream epoch moves, which is not every re-lay -- see VIEW VALIDITY
    /// above.
    virtual ClaimBlock hessian_claims() const = 0;

    /// @brief The KKT claim slots the equality Jacobian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded when the
    /// claim-stream epoch moves, which is not every re-lay -- see VIEW VALIDITY
    /// above.
    virtual ClaimBlock equality_jacobian_claims() const = 0;

    /// @brief The KKT claim slots the inequality Jacobian scatters through.
    ///
    /// One contiguous slot range, as the stream shape fixes. The block
    /// describes the structures as last laid and is superseded when the
    /// claim-stream epoch moves, which is not every re-lay -- see VIEW VALIDITY
    /// above.
    virtual ClaimBlock inequality_jacobian_claims() const = 0;

    /// @brief Claim slot to row of the objective-gradient arena.
    ///
    /// One entry per claim slot of that arena, in the same serial
    /// partition-index order the KKT stream carries. The returned view is valid
    /// until the claim-stream epoch moves, which is not every re-lay -- see VIEW
    /// VALIDITY above.
    virtual Eigen::Ref<const Eigen::VectorXi> objective_gradient_claim_rows() const = 0;
};

} // namespace hven::solvers
