// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// nlp_aggregate.h — the provider-side contract: a partitioned collection of
// pieces plus its layout, consumable by an engine and by an engine-independent
// scorer alike.
//
// It sits BESIDE NlpModel, never over it. NlpModel is one problem's callbacks;
// NlpAggregate is a collection of pieces and the arenas they claim out of. A
// bridge implementing this interface over an NlpModel is how a single-model
// problem reaches this level, and neither NlpModel nor NLPProblem gains a
// method, a base class or an obligation from anything in this header.
//
// CONCURRENCY POSTURE, stated here because it is part of the contract rather
// than an implementation note: an aggregate is inside at most ONE evaluation at
// a time. The evaluation entry points are non-const and share whatever
// per-partition scratch a provider keeps; the concurrency a provider is
// designed for is the fan-out WITHIN one call, over disjoint partitions, not
// two calls at once. Calling any evaluation entry -- assemble, either candidate
// evaluation, or probe_identity -- concurrently with another is a caller error.
// A consumer that must evaluate alongside an in-flight solve holds its OWN
// aggregate rather than sharing the solve's.
//
// Engine-independent by construction: nothing here includes from the
// interior-point machinery.

#include <cstdint>

#include "hven/core/types.h"
#include "hven/model/aggregate_declaration.h"
#include "hven/model/candidate_point.h"
#include "hven/model/claim_space.h"
#include "hven/model/structure_identity.h"

namespace hven::solvers {

/// What a provider DECLARES about how it does its work -- never about what it
/// is allowed to return.
///
/// Both declarations have the same shape: "this provider genuinely does the
/// fast thing; assume nothing if it does not say so." They are two bits of one
/// enum rather than two unrelated predicates because a consumer reads them at
/// the same moment, once per solve.
///
/// Declared per aggregate, as the WEAKEST claim over its pieces. A mixed
/// provider -- most pieces scattering directly, one holding an intermediate --
/// declares the weaker answer, because every decision a consumer makes from
/// these bits (where to place a probe, what to assume about the fill path) is
/// made once per solve rather than once per piece.
enum class AggregateCapability : std::uint32_t {
    kNone = 0,

    /// The fill path writes the consumer's storage in place: no provider-owned
    /// intermediate on the per-minor hot path. Absent it, a consumer must
    /// assume one intermediate exists and must not build a measurement or a
    /// budget that assumes otherwise.
    kDirectScatter = 1u << 0,

    /// evaluate_candidate_values genuinely skips derivative work rather than
    /// computing everything and returning a slice of it. Absent it, a consumer
    /// must assume a values evaluation costs a full one -- affordable at a
    /// subproblem exit under the flag, and not, in general, without it.
    kValuesFastPath = 1u << 1,
};

constexpr AggregateCapability operator|(AggregateCapability left,
                                        AggregateCapability right) noexcept {
    return static_cast<AggregateCapability>(static_cast<std::uint32_t>(left) |
                                            static_cast<std::uint32_t>(right));
}

constexpr AggregateCapability operator&(AggregateCapability left,
                                        AggregateCapability right) noexcept {
    return static_cast<AggregateCapability>(static_cast<std::uint32_t>(left) &
                                            static_cast<std::uint32_t>(right));
}

constexpr AggregateCapability &operator|=(AggregateCapability &left,
                                          AggregateCapability right) noexcept {
    left = left | right;
    return left;
}

/// True iff EVERY capability in `probe` is declared by `declared`. Probing for
/// kNone is vacuously true.
constexpr bool has_capability(AggregateCapability declared, AggregateCapability probe) noexcept {
    return (declared & probe) == probe;
}

/// The Level 2 provider interface: a partitioned collection of pieces plus its
/// layout.
///
/// An abstract base class rather than a type-erased handle. Its virtuals are
/// per structural call and per evaluation fan-out -- never per element -- so
/// the dispatch cost is immaterial, and one declaration reads as one contract.
/// The type-erasure seam stays one level down, at the pieces, where a
/// per-element dispatch budget is the thing being defended.
///
/// PARTITION INVARIANCE, a contract sentence every implementation owes:
/// **global (variable, row) identities are partition-invariant.** A variable's
/// global index and a constraint's global row are properties of the declaration
/// alone. No renegotiation of the partition count and no re-partitioning of the
/// piece lists may renumber either. Partitions decide which thread evaluates a
/// piece and in what order claims are handed out within the arenas; they never
/// decide what a claim NAMES. Claim ORDER being partition-dependent is exactly
/// why the adopted partition count is a conjunct of the structural key.
class NlpAggregate {
  public:
    virtual ~NlpAggregate() = default;

    NlpAggregate() = default;
    NlpAggregate(const NlpAggregate &) = delete;
    NlpAggregate &operator=(const NlpAggregate &) = delete;

    virtual const AggregateDeclaration &declaration() const = 0;

    /// Adopts a partition count and returns the count ACTUALLY adopted, which
    /// may be smaller than the request -- a provider with fewer pieces than
    /// partitions caps it. Returning the adopted count is what makes
    /// renegotiation honest: a consumer that assumed its request was honoured
    /// would mis-key the structural key, whose partition conjunct must be the
    /// ADOPTED count.
    ///
    /// A renegotiation re-lays the arenas, so it is a structural event and
    /// bumps the structure epoch -- even when it adopts the count already in
    /// force and leaves the claim structure identical. Claim order moved, and a
    /// consumer holding claim-slot-indexed state is stale.
    ///
    /// Engine-side threading (a subproblem solver's own thread count) is not
    /// this interface's business and never routes through it.
    virtual int negotiate_partition_count(int requested) = 0;

    virtual int evaluation_threads() const = 0;
    virtual void set_evaluation_threads(int n) = 0;

    /// The structural key of the declared model as currently laid.
    virtual ModelStructureKey model_structure_key() const = 0;

    /// How many times this aggregate has laid its structures.
    ///
    /// THE ONLY READ PATH, and there is deliberately no write path: the counter
    /// is owned here, bumped only from inside the routines that re-lay, and
    /// never by a caller. An engine must not be able to fake a structural
    /// event, and must never infer one from a digest that drifted -- nothing in
    /// this contract offers it a way to do either.
    ///
    /// WHAT BUMPS IT: any change to the assembled claim structure, AND any
    /// partition-count renegotiation, including one that leaves the claim
    /// structure identical.
    ///
    /// THE ORDERING GUARANTEE: the bump is observable before any evaluation
    /// performed against the new structures is observable. The bump is the last
    /// thing a re-lay does before returning, and it is a release store paired
    /// with the acquire load here. There is no window in which an evaluation of
    /// the new structures is visible under the old epoch.
    ///
    /// THE FAILURE-RESTORE RULE: a rejected reconfiguration that re-laid
    /// structures bumps too. If a re-lay happens and is then rejected -- a
    /// validation failure, an exception out of a piece, a treatment switch the
    /// caller abandons -- the structures on hand are not the structures the
    /// consumer last saw, whether or not they are the ones it saw before.
    /// Restoring them is itself a structural event and bumps under the same
    /// guarantee. What this closes is the failure where a consumer's cached
    /// claim-slot state survives a rejected reconfiguration because the epoch
    /// went out and came back.
    ///
    /// Virtual so that a provider wrapping another aggregate can forward the
    /// inner one's epoch; the default is the counter this base owns, which is
    /// what keeps monotonicity out of each provider's discipline.
    virtual StructureEpoch structure_epoch() const { return structure_epoch_counter_.current(); }

    /// What this provider declares about how it does its work. Declaring
    /// nothing is the default and is always a legal answer.
    virtual AggregateCapability capabilities() const { return AggregateCapability::kNone; }

    /// The hot path: fan out over the partitions, each piece scattering its own
    /// claims into the consumer's storage through the tables the consumer
    /// published.
    ///
    /// `request` selects what is EVALUATED this call. It replaces the eight
    /// evaluation shapes the partitioned engine grew, bijectively and with no
    /// over-evaluation -- see the mapping table in model/candidate_point.h,
    /// which is the contract text for that replacement. Views for arenas a
    /// request does not name may be empty, and the scalar objective value has
    /// an out slot of its own.
    ///
    /// Request masking never alters layout, structural key or structure epoch,
    /// and every legal request subset carries the full path's determinism
    /// guarantee.
    virtual void assemble(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                          RhsScatterView rhs) = 0;

    /// Off the hot path, under the same discipline: an aggregate evaluation at
    /// an arbitrary candidate point, into caller-owned split storage. No engine
    /// cooperation, and no access to whatever model sits behind the provider.
    ///
    /// Deliberately NOT spelled `eval_values`: the model-level method of that
    /// name has its own pinned wording that this contract does not re-word, and
    /// a reader who sees two different spellings in one call stack is being
    /// told, correctly, that they are two different levels.
    virtual void evaluate_candidate_values(const CandidatePoint &point, CandidateValues out) = 0;

    virtual void evaluate_candidate_first_order(const CandidatePoint &point,
                                                CandidateFirstOrder out) = 0;

    /// The cheap identity probe: the current structure epoch paired with a
    /// digest of the candidate values at `x`.
    ///
    /// It performs no factorization and no full derivative evaluation: it is
    /// evaluate_candidate_values plus a hash, which is why a provider declaring
    /// kValuesFastPath gets a genuinely cheap probe for free.
    virtual IdentityProbe probe_identity(ConstVecRef x) = 0;

  protected:
    /// Records a structural event. Called by an implementation from INSIDE the
    /// routine that re-lays structures, as the last thing that routine does --
    /// that program order is the substance of the ordering guarantee, and the
    /// release store here is only its cross-thread half.
    StructureEpoch bump_structure_epoch() noexcept { return structure_epoch_counter_.bump(); }

  private:
    StructureEpochCounter structure_epoch_counter_;
};

/// The consumption-seam spelling of an aggregate's structural key.
///
/// Only the provider can compute the key -- it is the one that knows its own
/// claim stream and the count it laid at -- so this delegates rather than
/// recomputing. It exists so consumer code reads `model_structure_key(agg)`,
/// which cannot be confused with a digest taken over assembled matrices, and so
/// the key has one free-function spelling for consumers regardless of how a
/// provider chose to store it.
inline ModelStructureKey model_structure_key(const NlpAggregate &aggregate) {
    return aggregate.model_structure_key();
}

} // namespace hven::solvers
