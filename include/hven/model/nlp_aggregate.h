// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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
// CONCURRENCY POSTURE, part of the contract: an aggregate is inside at most
// ONE operation at a time, and STRUCTURAL MUTATION IS AN OPERATION. Each of
// the following overlapping any other is a caller error:
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
// A consumer's own work on its own storage is a separate question, settled at
// assemble(): it may run concurrently with an evaluation when its writes touch
// no destination the scatter views name. This does not relax the
// one-evaluation-at-a-time rule for the aggregate's own entries.
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

/// A gradient arena's presence check: storage and a table, both there.
///
/// Presence and NOT a size match, deliberately: a residual arena has a
/// DECLARED length (the declaration's own row count), but a gradient arena's
/// length is the provider's own primal width, which this contract nowhere
/// fixes -- a provider that eliminates variables from the system it solves has
/// a narrower one, and checking against the declared variable count would
/// reject that provider's own storage.
///
/// THE ENTRY VALIDATES CONTRACT-VISIBLE FACTS; AN IMPLEMENTATION VALIDATES
/// LAID-WIDTH FACTS. A view shorter than the width a provider laid its arena
/// over would scatter off the end, and no entry can catch that -- so an
/// implementation compares the view against its own laid width at its hook.
/// PRESENT MEANS NON-EMPTY, all three ways: a named gradient arena with no
/// storage, with no table, or with a length of ZERO is refused. A gradient
/// arena, unlike a residual block, has no legitimate empty case -- an
/// aggregate with no primal variables is not a problem -- so a zero-length
/// view under a naming request is incoherent rather than degenerate.
inline void require_arena_view(const RhsArenaView &view, EvalRequest flag, const char *arena) {
    if (view.empty()) {
        const char *missing = view.values_ == nullptr      ? "storage"
                              : view.locations_ == nullptr ? "location table"
                                                           : "rows (its length is zero)";
        throw std::invalid_argument(
            fmt::format("assemble: the request names the {0} (flag 0x{1:x}), but that arena's "
                        "scatter view supplies no {2}. A request may leave empty only the arenas "
                        "it does NOT name; see the mapping table in model/candidate_point.h",
                        arena, static_cast<std::uint32_t>(flag), missing));
    }
}

/// A residual arena's check: the view's length MATCHES the declared row count.
///
/// A size match rather than a non-emptiness test, because a declared row count
/// of zero is an ordinary problem and not a missing destination: an
/// equality-only model declares no inequality rows and vice versa, and testing
/// such a view for "empty" would lock those models out of every values-bearing
/// request -- the residual flag names both arenas at once. A zero declared row
/// count therefore accepts any view, including a wholly default one.
///
/// This works ONLY because declaration() reports the row space AS LAID: a
/// provider whose treatment configuration appends internal rows (for fixed
/// variables, say) counts them in the declaration, so a residual view sized to
/// the solver's actual row space still matches the declared count exactly. A
/// provider allowed to hide rows from its own declaration is the change that
/// would break this, and it is the declaration's contract that would have to
/// be re-settled first.
inline void require_residual_arena(const RhsArenaView &view, Eigen::Index declared_rows,
                                   const char *arena) {
    if (declared_rows == 0) {
        return;
    }
    if (view.locations_ == nullptr || view.values_ == nullptr) {
        throw std::invalid_argument(fmt::format(
            "assemble: the request names the {0} arena, whose declared length is {1}, but the "
            "view supplies no {2}",
            arena, declared_rows, view.values_ == nullptr ? "storage" : "location table"));
    }
    if (view.size_ != declared_rows) {
        throw std::invalid_argument(
            fmt::format("assemble: the {0} arena's view is {1} rows, but the aggregate declares "
                        "{2} {0} rows",
                        arena, view.size_, declared_rows));
    }
}

} // namespace detail

/// Rejects an assemble call that names an output with nowhere to put it.
///
/// A FIXED AMOUNT OF WORK PER DESTINATION -- a null test, a size comparison.
/// This asks whether a destination EXISTS and never whether its contents agree
/// with the layout: a per-slot scan of a location table would re-derive at
/// every evaluation what the claim pass established once.
///
/// What it buys is the converse of "no output is written that the request did
/// not name": a request this entry ACCEPTS has a destination for everything it
/// names, so a consumer that forgot an arena is told which one at the entry
/// rather than discovering a silently unwritten block.
///
/// It does NOT leave an implementation with nothing to check: whether a
/// destination matches the width that provider actually laid its arena over is
/// the provider's own question, asked at its own hook -- see the two-boundary
/// split at require_arena_view. The empty-view permission is the complement:
/// an arena the request does not name may be empty, and shapes 5 and 7 of the
/// mapping table rely on that.
///
/// The residual arenas are checked against the DECLARED row counts rather than
/// for non-emptiness, so a model that declares no rows of one kind is not
/// locked out of evaluating the kind it does declare -- see
/// require_residual_arena for why that is the rule and require_arena_view for
/// why the gradient arenas are not checked the same way.
inline void validate_request_destinations(EvalRequest request, const AggregateDeclaration &declared,
                                          const KktScatterView &kkt, const RhsScatterView &rhs) {
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
        detail::require_residual_arena(rhs.equality_residuals_, declared.equality_rows_,
                                       "equality residual");
        detail::require_residual_arena(rhs.inequality_residuals_, declared.inequality_rows_,
                                       "inequality residual");
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

/// Rejects a KKT view that does not name the destination the provider's
/// location tables were bound to.
///
/// A PROVIDER MAY BIND ITS LOCATION TABLES TO A DESTINATION IDENTITY AT
/// ANALYSIS TIME; this entry validates that the view names that destination.
/// Such a table is meaningful for one particular value array and no other, so
/// the binding is a fact about the tables rather than an extra obligation on
/// the consumer. A provider that binds nothing returns nullptr from
/// NlpAggregate::bound_kkt_destination and is never checked here.
///
/// A CAPTURED VALUE, NOT A LIVE READING: the bound address must be the one
/// recorded AT ANALYSIS TIME. Re-deriving it per call defeats the check twice
/// over -- a resized container moves its value array and a live reading moves
/// with it, so both sides agree while every recorded offset has gone stale --
/// and a destroyed container cannot be read at all, while this accessor runs
/// on EVERY assemble.
///
/// IDENTITY ONLY, NEVER A LIFETIME PROMISE: two addresses are compared as
/// values; neither is dereferenced, and storage lifetime stays the consumer's.
/// What it catches is the STALE TABLE -- a consumer matrix that was reallocated
/// moved its value array, so the recorded offsets describe storage that is
/// gone -- which is why the throw names the remedy (re-run the analysis).
///
/// THE CANDIDATE surface stays UNBOUND:
/// evaluate_candidate_values/evaluate_candidate_first_order take caller-owned
/// storage afresh on every call and bind to nothing. Binding is a property of
/// the hot path's location tables alone.
inline void validate_bound_destination(const double *bound, const KktScatterView &kkt) {
    if (bound == nullptr || kkt.empty()) {
        return;
    }
    if (kkt.values_ != bound) {
        throw std::invalid_argument(fmt::format(
            "assemble: the KKT scatter view names value array {0}, but this provider's location "
            "tables were bound at analysis time to {1}. The destination bound at analysis no "
            "longer matches the view; re-run the analysis against the destination you intend to "
            "fill. (A consumer matrix that was resized or reallocated moves its value array, so "
            "this is a stale location table being caught rather than a rejected argument.)",
            fmt::ptr(kkt.values_), fmt::ptr(bound)));
    }
}

/// The Level 2 provider interface: a partitioned collection of pieces plus its
/// layout.
///
/// An abstract base class rather than a type-erased handle: its virtuals are
/// per structural call and per evaluation fan-out -- never per element. The
/// type-erasure seam stays one level down, at the pieces, where a per-element
/// dispatch budget is the thing being defended.
///
/// PARTITION INVARIANCE, a contract sentence every implementation owes:
/// global (variable, row) identities are partition-invariant. A variable's
/// global index and a constraint's global row are properties of the declaration
/// alone. No renegotiation of the partition count and no re-partitioning of the
/// piece lists may renumber either. Partitions decide which thread evaluates a
/// piece and in what order claims are handed out within the arenas; they never
/// decide what a claim NAMES. Claim ORDER being partition-dependent is exactly
/// why the adopted partition count is a conjunct of the structural key.
///
/// CLAIM-ORDER DETERMINISM: claim order is a pure function of the declaration
/// and the adopted partition count. First-come-first-served issuance from
/// worker threads is not a legal implementation. Two separate constructions
/// from equal declarations at one adopted count produce the same claim stream,
/// element for element, and every implementation pins that alongside the
/// standing thread-count invariance pin.
///
/// CLAIM EXCLUSIVITY, the second contract sentence every implementation owes:
/// a claim slot belongs to exactly one partition. An arena row may receive
/// several partitions' slots; the provider folds them into the row serially,
/// after the join, in claim order (partition-index order), so each row has one
/// writer. Physical KKT destination clashes are the consumer's, resolved
/// through the clash marks and lock vector of the location table it
/// published (model/claim_space.h); a provider scatters through the table
/// honoring the marks and never pre-reduces contested KKT coordinates.
///
/// A SLOT IS NOT A COORDINATE, and the exclusivity above is a statement about
/// slots. A claim stream need not be injective on physical KKT coordinates:
/// pieces that legitimately overlap -- an integrand accumulated across several
/// pieces into one row is the shape that produces it -- emit several slots
/// naming one destination, and that is a legal stream, not a defect in it.
/// What each such slot belongs to exactly one of is a PARTITION. Resolving
/// several slots onto one destination is the consumer's, by the clash marks
/// and lock vector it published, or by whatever else it chooses to publish; a
/// consumer that supports only injective streams says so in its own support
/// statement rather than reading a prohibition into this one.
///
/// CLAIM-PASS SHAPE: claims are issued serially by the provider, in
/// partition-index order, never from worker threads. Pre-reserving slot ranges
/// is a permitted implementation detail; the OBSERVABLE stream is the ordered
/// one, which is what lets a claim-space cursor stay a bare serial counter.
/// Whether each domain's claims additionally occupy one contiguous slot range
/// is a property of the claim-stream interface, not of this base, and is stated
/// there (model/claim_stream_source.h).
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
    /// caller's blocks against real dimensions, and a reference to anything
    /// temporary would dangle. An implementation MAY materialize the stored
    /// state on the first read after a structural mutation, since
    /// once-per-mutation is not per call.
    virtual const AggregateDeclaration &declaration() const = 0;

    /// Adopts a partition count and returns the count ACTUALLY adopted, which
    /// may be smaller than the request -- a provider with fewer pieces than
    /// partitions caps it. A consumer that assumed its request was honoured
    /// would mis-key the structural key, whose partition conjunct must be the
    /// ADOPTED count.
    ///
    /// A renegotiation re-lays the arenas, so it is a structural event and
    /// bumps the structure epoch -- even when it adopts the count already in
    /// force and leaves the claim structure identical. Claim order moved, and a
    /// consumer holding claim-slot-indexed state is stale.
    ///
    /// A NON-POSITIVE REQUEST IS REFUSED with std::invalid_argument naming the
    /// value, and is never silently corrected to one: capping is honest work
    /// reported through the return value; a request for zero or fewer
    /// partitions names no partitioning, so there is nothing to cap.
    ///
    /// Engine-side threading (a subproblem solver's own thread count) is not
    /// this interface's business and never routes through it.
    virtual int negotiate_partition_count(int requested) = 0;

    /// @brief The evaluation thread budget.
    ///
    /// Reports the count evaluation actually uses.
    ///
    /// NO NUMERIC RELATIONSHIP between this count and the adopted partition
    /// count is contracted. One thread may serve more than one partition, and
    /// the deterministic path is bit-identical at every reported count. A
    /// provider documents its own occupancy policy in its own docstring.
    ///
    /// @return the count evaluation will actually use.
    virtual int evaluation_threads() const = 0;

    /// @brief Requests an evaluation thread budget.
    ///
    /// An implementation refuses n < 1 with std::invalid_argument naming the
    /// value, rather than correcting it. What a provider then does with a
    /// legal request is its own business; what it reports is not, and
    /// evaluation_threads() above binds that.
    ///
    /// @param n the requested count.
    /// @throws std::invalid_argument if @p n is below 1.
    virtual void set_evaluation_threads(int n) = 0;

    /// @brief The structural key of the declared model as currently laid.
    ///
    /// A PURE FUNCTION OF THREE THINGS: the declaration, the ADOPTED partition
    /// count, and the configured fixed-variable treatment together with its
    /// relax state. The third conjunct is not an accident of where the digest
    /// is taken -- it is the key's meaning. A treatment that eliminates
    /// bound-fixed variables changes the solver's reduced coordinate space, so
    /// the claim structure the key is taken over is the reduced one; primal and
    /// multiplier payloads are not transferable across a treatment flip, and a
    /// key that stayed still across one would say they were.
    ///
    /// DISTINCT FROM CLAIM-ORDER DETERMINISM, which is a pure function of the
    /// declaration and the adopted count ALONE (see the CLAIM-ORDER
    /// DETERMINISM paragraph on this class). The two are statements about
    /// different objects -- what order the claims come out in, and what
    /// structure they describe -- and neither implies the other. Two
    /// constructions from equal declarations at one adopted count hand out the
    /// same claim stream whatever treatment is configured; whether they key
    /// the same depends on the treatment as well.
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
    /// guarantee, which closes the failure where a consumer's cached
    /// claim-slot state survives a rejected reconfiguration because the epoch
    /// went out and came back.
    ///
    /// Virtual so that a provider wrapping another aggregate can forward the
    /// inner one's epoch; the default is the counter this base owns. AN
    /// OVERRIDE TAKES OVER THE FULL SET OF EPOCH OBLIGATIONS -- monotonicity,
    /// the ordering guarantee, and the failure-restore rule -- and must justify
    /// itself in its own contract text; the escape hatch exists for a
    /// delegating aggregate that mirrors an inner aggregate's epoch, not for a
    /// provider computing an epoch its own way.
    virtual StructureEpoch structure_epoch() const { return structure_epoch_counter_.current(); }

    /// What this provider declares about how it does its work. Declaring
    /// nothing is the default and is always a legal answer.
    virtual AggregateCapability capabilities() const { return AggregateCapability::kNone; }

    /// The KKT value array this provider's location tables were bound to at
    /// analysis time, or nullptr when it binds none.
    ///
    /// The default binds none. A provider that computes its locations as
    /// offsets into one particular array returns that array here, and the
    /// assemble entry then refuses a view naming anything else -- see
    /// validate_bound_destination above for what that check is and what it is
    /// not. AN IMPLEMENTATION RETURNS THE ADDRESS IT CAPTURED AT ANALYSIS
    /// TIME, held as a value; re-reading it out of the container would make
    /// the check vacuous against a resize and unsafe against destruction.
    virtual const double *bound_kkt_destination() const { return nullptr; }

    /// The hot path: fan out over the partitions, each piece scattering its own
    /// claims into the consumer's storage through the tables the consumer
    /// published.
    ///
    /// `request` selects what is EVALUATED this call. The named shapes are a
    /// union of consumer-owned families -- each family replaces its own
    /// consumer's legacy call bills bijectively and with no over-evaluation;
    /// the mapping table in model/candidate_point.h is the contract text,
    /// including each provider's SUPPORT statement (a provider handed a
    /// legal shape outside the families it serves refuses it by name). A
    /// provider refuses a mapped shape it does not serve with
    /// std::invalid_argument, naming the shape. Views
    /// for arenas a request does not name may be empty, and the scalar
    /// objective value has an out slot of its own.
    ///
    /// Request masking never alters layout, structural key or structure epoch,
    /// and every legal request subset carries the full path's determinism
    /// guarantee.
    ///
    /// VALIDATED HERE, NOT BY THE IMPLEMENTATION, before anything is evaluated:
    ///   * validate_eval_request(request) -- exactly the named shapes are
    ///     legal, and a composed-but-unmapped combination is refused rather
    ///     than approximated. Provider SUPPORT within the legal set is the
    ///     implementation's own refusal, at its hook.
    ///   * validate_candidate_point(point, ...) -- the primal block is sized to
    ///     the declaration; the multiplier blocks are empty or exact.
    ///   * validate_full_multipliers(point, ...) when
    ///     request_consumes_multipliers(request) -- a request naming a
    ///     constraint adjoint gradient or adjoint Hessian contracts a
    ///     derivative against the multipliers, so an empty multiplier block is
    ///     a missing input rather than a zero vector.
    ///   * validate_request_destinations(request, declaration(), kkt, rhs) -- every
    ///     output the request names has somewhere to go: a fixed amount of
    ///     work per destination, and an arena the request does not name stays
    ///     legally empty. The residual arenas are matched against the declared
    ///     row counts, so a model with rows of only one kind is not locked out.
    ///   * validate_bound_destination(bound_kkt_destination(), kkt) -- when this
    ///     provider bound its location tables to a destination at analysis
    ///     time, the view names that destination. One pointer comparison, and
    ///     vacuous for a provider that binds nothing.
    ///
    /// THE ENTRY IS NON-VIRTUAL AND THE HOOK BELOW IT IS THE VIRTUAL ONE,
    /// which makes both halves of the guarantee properties of the TYPE: no
    /// output is written that the request did not name, and a request the
    /// entry accepts has a destination for everything it does. An
    /// implementation cannot skip the validation, forget it, or reorder it
    /// after its own work; any NlpAggregate&, whatever is behind it, has
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
    /// own to zero. Accumulation is exact only against a -0.0-seeded arena;
    /// the seed choice is the consumer's and is byte-identity-relevant (the
    /// mechanism is detail/drivers/aggregate_eval_seam.h's, not restated here).
    ///
    /// IF assemble THROWS, the destinations hold an unspecified prefix of the
    /// fill. This call is not scratch-then-commit: a caller that retries
    /// re-seeds its destinations first.
    ///
    /// WHAT THIS CALL DOES NOT DO: fill the consumer's own KKT coefficients --
    /// slack Jacobian, primal and slack diagonals, constraint-row pivots. Those
    /// are values the consumer set on the consumer's own storage; scattering
    /// them is a consumer-owned step outside provider evaluation. The mapping
    /// table records that transfer per row, because in the shapes this call
    /// replaces the two steps ran inside one entry.
    ///
    /// A consumer may run its own coefficient steps concurrently with
    /// assemble() when its writes touch no destination the scatter views name;
    /// destination disjointness is the consumer's obligation. The interior
    /// engine does not meet it and sequences instead.
    void assemble(const CandidatePoint &point, EvalRequest request, KktScatterView kkt,
                  RhsScatterView rhs) {
        const AggregateDeclaration &declared = this->declaration();
        validate_eval_request(request);
        validate_candidate_point(point, declared.primal_vars_, declared.equality_rows_,
                                 declared.inequality_rows_);
        if (request_consumes_multipliers(request)) {
            validate_full_multipliers(point, declared.equality_rows_, declared.inequality_rows_);
        }
        validate_request_destinations(request, declared, kkt, rhs);
        validate_bound_destination(this->bound_kkt_destination(), kkt);
        this->assemble_impl(point, request, kkt, rhs);
    }

    /// Off the hot path, under the same discipline: an aggregate evaluation at
    /// an arbitrary candidate point, into caller-owned split storage. No engine
    /// cooperation, and no access to whatever model sits behind the provider.
    ///
    /// A VALUES path, so the point's multiplier blocks are legally empty: it
    /// reads none of them. The storage blocks are checked against the
    /// declaration's dimensions here, before the hook runs. Deliberately NOT
    /// spelled `eval_values`: the model-level method of that name has its own
    /// wording this contract does not re-word -- the two spellings name two
    /// different levels.
    ///
    /// THE CANDIDATE ENTRIES ASSIGN. Every block this call and
    /// evaluate_candidate_first_order write is ASSIGNED, not accumulated into,
    /// and that is deliberately the opposite of assemble's discipline: assemble
    /// is a MULTI-PIECE fan-out whose partitions and successive requests must
    /// compose against arenas the consumer zeroed once; a candidate evaluation
    /// is the WHOLE aggregate written once into a caller's own buffer off that
    /// path, and requiring its scorer to pre-zero would buy nothing while a
    /// forgotten zero would silently return the sum of two points.
    ///
    /// THE IDENTITY-SPACE PRINCIPLE, which binds every vector on this surface:
    /// the point's blocks and the storage blocks are indexed by DECLARED global
    /// (variable, row) identities -- the declaration's own space, the same one
    /// partition invariance is stated in. No reduction, elimination or staging
    /// state a provider keeps is visible here: a variable eliminated from
    /// whatever system the provider solves still has its declared index on this
    /// surface, and a provider that works in a narrower space maps into and out
    /// of it internally. An independent scorer can then be written against the
    /// declaration alone and needs to know nothing about the provider's
    /// internals -- which is the whole point of the surface existing.
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
    ///
    /// Assigns, and is in declaration space, on both counts exactly as
    /// evaluate_candidate_values above -- the two gradient blocks included, so
    /// they carry one row per DECLARED variable whatever the provider's own
    /// working space is.
    ///
    /// WHAT A SCORER OWES, an obligation on the CONSUMER rather than a defect
    /// in any provider: a variable whose declared bounds coincide carries no
    /// degree of freedom, and its stationarity row is not a stationarity
    /// condition -- the quantity that balances there is the bound multiplier
    /// holding it at its value, which this surface does not carry. A correct
    /// scorer therefore EXCLUDES the declared-fixed coordinates from
    /// stationarity scoring. The exclusion set is computable FROM DECLARATION
    /// DATA ALONE: the variables whose materialized bound record has
    /// lower == upper (declaration().materialize_variable_bounds(), the same
    /// intersection the structural key's bound conjunct is taken over).
    /// Providers differ in what they leave in an excluded coordinate's row,
    /// and a scorer that skips those rows is insensitive to the difference.
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
    /// kValuesFastPath gets a genuinely cheap probe for free. Virtual rather
    /// than split like the three entries above, because a probe takes no
    /// caller-owned storage and no request -- an implementation that routes
    /// through evaluate_candidate_values has the primal block checked already,
    /// at the entry it goes through.
    virtual IdentityProbe probe_identity(ConstVecRef x) = 0;

  protected:
    // THE EVALUATION HOOKS. Each public entry above validates and then forwards
    // to exactly one of these, so an implementation never writes the
    // contract's own checks and cannot omit them: by the time a hook runs,
    // every destination its request names is known to be present, and every
    // one whose length the contract knows is known to be right. What a hook
    // still owns is the half the contract cannot see -- whether a destination
    // matches the width THAT provider laid its arena over. See the
    // two-boundary split at require_arena_view.
    //
    // What the entries check is deliberately bounded: flag tests and dimension
    // comparisons, a fixed amount of work per call. The hot-path budget belongs
    // to the fill; a check that needs element work belongs where the elements
    // are already being walked.

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
