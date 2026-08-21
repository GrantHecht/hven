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
// than an implementation note: an aggregate is inside at most ONE operation at
// a time, and STRUCTURAL MUTATION IS AN OPERATION. Each of the following
// overlapping any other is a caller error:
//
//   * two evaluations at once -- assemble, either candidate evaluation, or
//     probe_identity. They are non-const and share whatever per-partition
//     scratch a provider keeps; the concurrency a provider is designed for is
//     the fan-out WITHIN one call, over disjoint partitions.
//   * any structural mutation overlapping ANY evaluation, in either order, and
//     including a mutation that changes nothing: negotiate_partition_count, a
//     re-layout, or a mutation of the declaration the aggregate was built
//     from. A re-lay moves the arenas an in-flight evaluation is scattering
//     into and renumbers the claim slots it is addressing them by. The epoch's
//     ordering guarantee does not soften this: it says a re-lay is visible
//     before any evaluation OF THE NEW structures, which is a statement about
//     successive operations and says nothing about one already running.
//   * two structural mutations at once.
//
// The counter behind structure_epoch() is atomic, so reading the epoch is safe
// from anywhere. That is the one exception, and deliberately the only thing a
// consumer may do to a busy aggregate. A consumer that must evaluate alongside
// an in-flight solve holds its OWN aggregate rather than sharing the solve's.
//
// Engine-independent by construction: nothing here includes from the
// interior-point machinery.

#include <cstdint>
#include <stdexcept>

#include <fmt/format.h>

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

/// In-place masking, so narrowing a declared set down to a subset compiles the
/// same way widening one does.
constexpr AggregateCapability &operator&=(AggregateCapability &left,
                                          AggregateCapability right) noexcept {
    left = left & right;
    return left;
}

/// True iff EVERY capability in `probe` is declared by `declared` -- ALL-BITS
/// semantics, not any-bit: `has_capability(set, a | b)` asks whether the set
/// declares both, which is the reduction a consumer wants when it is deciding
/// what it may assume. Probing for kNone is vacuously true. To ask whether a
/// set declares ANY of a group, compare the intersection:
/// `(declared & group) != AggregateCapability::kNone`.
constexpr bool has_capability(AggregateCapability declared, AggregateCapability probe) noexcept {
    return (declared & probe) == probe;
}

namespace detail {

/// One arena's presence check, written once so all four arena rows below refuse
/// the same way.
inline void require_arena_view(const RhsArenaView &view, EvalRequest flag, const char *arena) {
    if (view.empty()) {
        throw std::invalid_argument(
            fmt::format("assemble: the request names the {0} (flag 0x{1:x}), but that arena's "
                        "scatter view is empty. A request may leave empty only the arenas it "
                        "does NOT name; see the mapping table in model/candidate_point.h",
                        arena, static_cast<std::uint32_t>(flag)));
    }
}

} // namespace detail

/// Rejects an assemble call that names an output with nowhere to put it.
///
/// PRESENCE ONLY -- a null pointer, a zero size, a missing table -- and that
/// bound is the point rather than an economy. The entry's budget is a fixed
/// amount of work per call, so this asks whether a destination EXISTS and never
/// whether its contents agree with the layout: a per-slot scan of a location
/// table would re-derive at every evaluation what the claim pass established
/// once.
///
/// What it buys is the other half of the type-level guarantee the non-virtual
/// entry makes. "No output is written that the request did not name" was
/// already structural; this adds its converse -- a request the entry ACCEPTS
/// has a destination for everything it names -- so an implementation's hooks
/// are born needing no view checks of their own, and a consumer that forgot an
/// arena is told which one at the entry rather than discovering a silently
/// unwritten block.
///
/// The empty-view permission is unchanged and is exactly its complement: an
/// arena the request does not name may be empty, and shapes 5 and 7 of the
/// mapping table rely on that.
inline void validate_request_destinations(EvalRequest request, const KktScatterView &kkt,
                                          const RhsScatterView &rhs) {
    if (has_request(request, EvalRequest::kObjectiveValue) && rhs.objective_ == nullptr) {
        throw std::invalid_argument(
            fmt::format("assemble: the request names the objective value (flag 0x{0:x}), but no "
                        "out slot was supplied for it",
                        static_cast<std::uint32_t>(EvalRequest::kObjectiveValue)));
    }
    if (has_request(request, EvalRequest::kObjectiveGradient)) {
        detail::require_arena_view(rhs.objective_gradient_, EvalRequest::kObjectiveGradient,
                                   "objective gradient");
    }
    if (has_request(request, EvalRequest::kConstraintValues)) {
        // One flag, two arenas: no evaluation shape produces one residual block
        // without the other, so naming the flag names both destinations.
        detail::require_arena_view(rhs.equality_residuals_, EvalRequest::kConstraintValues,
                                   "equality residuals");
        detail::require_arena_view(rhs.inequality_residuals_, EvalRequest::kConstraintValues,
                                   "inequality residuals");
    }
    if (has_request(request, EvalRequest::kConstraintAdjointGradient)) {
        detail::require_arena_view(rhs.constraint_adjoint_gradient_,
                                   EvalRequest::kConstraintAdjointGradient,
                                   "constraint adjoint gradient");
    }
    // The three KKT-bearing flags share one destination, so they are tested as
    // a group: any of them names the KKT view.
    constexpr EvalRequest kKktBearing = EvalRequest::kObjectiveHessian |
                                        EvalRequest::kConstraintJacobian |
                                        EvalRequest::kConstraintAdjointHessian;
    if ((request & kKktBearing) != EvalRequest::kNone && kkt.empty()) {
        throw std::invalid_argument(fmt::format(
            "assemble: the request names KKT-bearing output (flags 0x{0:x} of the request), but "
            "the KKT scatter view is empty",
            static_cast<std::uint32_t>(request & kKktBearing)));
    }
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

    /// The declaration this aggregate was built from.
    ///
    /// The reference is to STORED STATE, and stays valid and unchanging for the
    /// object's lifetime except across a structural mutation. An implementation
    /// must not build a declaration per call and return a reference to it: the
    /// non-virtual entries above read this on EVERY evaluation, to check the
    /// caller's blocks against real dimensions, so a rebuilding implementation
    /// would put that work on the evaluation path -- and returning a reference
    /// to anything temporary would dangle.
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
    ///
    /// AN OVERRIDE TAKES OVER THE FULL SET OF EPOCH OBLIGATIONS -- monotonicity,
    /// the ordering guarantee, and the failure-restore rule -- and must justify
    /// itself in its own contract text. The door is open for a delegating
    /// aggregate that mirrors an inner aggregate's epoch, which is a genuine
    /// case; it is not open for a provider that would rather compute an epoch
    /// its own way, because the escape hatch must not erode the enforcement the
    /// default exists to provide.
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
    ///
    /// VALIDATED HERE, NOT BY THE IMPLEMENTATION, before anything is evaluated:
    ///   * validate_eval_request(request) -- exactly the eight named shapes are
    ///     legal, and a composed-but-unmapped combination is refused rather
    ///     than approximated.
    ///   * validate_candidate_point(point, ...) -- the primal block is sized to
    ///     the declaration; the multiplier blocks are empty or exact.
    ///   * validate_full_multipliers(point, ...) when
    ///     request_consumes_multipliers(request) -- a request naming a
    ///     constraint adjoint gradient or adjoint Hessian contracts a
    ///     derivative against the multipliers, so an empty multiplier block is
    ///     a missing input rather than a zero vector.
    ///   * validate_request_destinations(request, kkt, rhs) -- every output the
    ///     request names has somewhere to go. Presence only, O(1) per
    ///     destination; an arena the request does not name stays legally empty.
    ///
    /// THE ENTRY IS NON-VIRTUAL AND THE HOOK BELOW IT IS THE VIRTUAL ONE, which
    /// upgrades the guarantee at the top of this comment in BOTH directions and
    /// makes each half a property of the TYPE rather than a rule each provider
    /// is asked to keep: no output is written that the request did not name,
    /// and a request the entry accepts has a destination for everything it
    /// does. An implementation cannot skip the validation, forget it, or
    /// reorder it after its own work, because it never sees an unvalidated
    /// call. That is strictly stronger than the settled text required, and a
    /// consumer may rely on it: any NlpAggregate&, whatever is behind it, has
    /// refused an unmapped request, a short-blocked point and a missing
    /// destination before a single value moved.
    ///
    /// ACCUMULATION, NOT ASSIGNMENT. Every destination this call writes -- the
    /// KKT values through their location table, each named arena through
    /// theirs, and the scalar objective out slot -- is ACCUMULATED into. An
    /// implementation never assigns to one and never zeroes one: the consumer
    /// owns its storage AND its initial state, and zeroes what it wants clean
    /// before the call. That is what lets a fan-out over partitions, and
    /// successive requests against one destination, compose the same way.
    /// Provider-internal scratch is a separate matter and stays the provider's
    /// own to zero.
    ///
    /// WHAT THIS CALL DOES NOT DO: fill the consumer's own KKT coefficients --
    /// slack Jacobian, primal and slack diagonals, constraint-row pivots. Those
    /// are values the consumer set on the consumer's own storage; scattering
    /// them is a consumer-owned step outside provider evaluation. The mapping
    /// table records that transfer per row, because in the shapes this call
    /// replaces the two steps ran inside one entry.
    void assemble(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                  RhsScatterView rhs) {
        const AggregateDeclaration &declared = this->declaration();
        validate_eval_request(request);
        validate_candidate_point(point, declared.primal_vars_, declared.equality_rows_,
                                 declared.inequality_rows_);
        if (request_consumes_multipliers(request)) {
            validate_full_multipliers(point, declared.equality_rows_, declared.inequality_rows_);
        }
        validate_request_destinations(request, kkt, rhs);
        this->assemble_impl(point, request, kkt, rhs);
    }

    /// Off the hot path, under the same discipline: an aggregate evaluation at
    /// an arbitrary candidate point, into caller-owned split storage. No engine
    /// cooperation, and no access to whatever model sits behind the provider.
    ///
    /// Deliberately NOT spelled `eval_values`: the model-level method of that
    /// name has its own pinned wording that this contract does not re-word, and
    /// a reader who sees two different spellings in one call stack is being
    /// told, correctly, that they are two different levels.
    ///
    /// A VALUES path, so the point's multiplier blocks are legally empty: it
    /// reads none of them. The storage blocks are checked against the
    /// declaration's dimensions here, before the hook runs.
    void evaluate_candidate_values(const CandidatePoint &point, CandidateValues out) {
        const AggregateDeclaration &declared = this->declaration();
        validate_candidate_point(point, declared.primal_vars_, declared.equality_rows_,
                                 declared.inequality_rows_);
        validate_candidate_values(out, declared.equality_rows_, declared.inequality_rows_);
        this->evaluate_candidate_values_impl(point, out);
    }

    /// The first-order evaluation ALWAYS produces the constraint adjoint
    /// gradient, so it always reads the multipliers: full-length blocks are
    /// required here, unconditionally, and an empty multiplier block is a
    /// missing input rather than a zero vector.
    void evaluate_candidate_first_order(const CandidatePoint &point, CandidateFirstOrder out) {
        const AggregateDeclaration &declared = this->declaration();
        validate_candidate_point(point, declared.primal_vars_, declared.equality_rows_,
                                 declared.inequality_rows_);
        validate_candidate_first_order(out, declared.primal_vars_, declared.equality_rows_,
                                       declared.inequality_rows_);
        validate_full_multipliers(point, declared.equality_rows_, declared.inequality_rows_);
        this->evaluate_candidate_first_order_impl(point, out);
    }

    /// The cheap identity probe: the current structure epoch paired with a
    /// digest of the candidate values at `x`.
    ///
    /// It performs no factorization and no full derivative evaluation: it is
    /// evaluate_candidate_values plus a hash, which is why a provider declaring
    /// kValuesFastPath gets a genuinely cheap probe for free.
    ///
    /// Virtual rather than split like the three entries above, because a probe
    /// takes no caller-owned storage and no request: the only thing there would
    /// be to check at a non-virtual entry is the primal block, and an
    /// implementation that routes through evaluate_candidate_values -- which is
    /// what "values plus a hash" means -- has that checked already, at the
    /// entry it goes through.
    virtual IdentityProbe probe_identity(ConstVecRef x) = 0;

  protected:
    // THE EVALUATION HOOKS. Each public entry above validates and then forwards
    // to exactly one of these, so an implementation writes only the work and
    // never the checks -- and cannot omit them. A hook is born needing no view
    // checks of its own: by the time it runs, every destination its request
    // names is known to be present.
    //
    // What the entries check is deliberately bounded: flag tests and dimension
    // comparisons, a fixed amount of work per call. The hot-path budget belongs
    // to the fill, and a per-element scan at the entry would spend it on
    // re-deriving what the layout already established. A check that needs
    // element work is not an entry check; it belongs where the elements are
    // already being walked.

    virtual void assemble_impl(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                               RhsScatterView rhs) = 0;

    virtual void evaluate_candidate_values_impl(const CandidatePoint &point,
                                                CandidateValues out) = 0;

    virtual void evaluate_candidate_first_order_impl(const CandidatePoint &point,
                                                     CandidateFirstOrder out) = 0;

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
