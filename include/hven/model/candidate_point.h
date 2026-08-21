// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// candidate_point.h — the point an aggregate is evaluated at, the caller-owned
// storage an off-path evaluation writes into, and the request flags that say
// what one on-path call evaluates.
//
// Split storage is the contract, in both directions: never a monolith the
// caller has to slice, and never storage the provider allocated.
//
// Engine-independent by construction: nothing here includes from the
// interior-point machinery.

#include <cstdint>
#include <stdexcept>

#include <Eigen/Core>

#include <fmt/format.h>

#include "hven/core/pattern_hash.h"
#include "hven/core/types.h"

namespace hven::solvers {

/// Where an aggregate is evaluated.
///
/// The blocks are non-owning views, so every one of them must outlive the call
/// that reads it -- exactly as a function argument would. The two multiplier
/// blocks may be empty, which means "no multipliers", not "zero multipliers of
/// the declared length": a values-only evaluation supplies neither.
///
/// The Eigen views are held BY VALUE rather than as reference members. They are
/// the same non-owning views either way, and a value member cannot be left
/// referring to a view object that died at the end of the expression that
/// formed it.
struct CandidatePoint {
    Eigen::Ref<const Vec> x_;
    Eigen::Ref<const Vec> equality_multipliers_;   ///< may be empty
    Eigen::Ref<const Vec> inequality_multipliers_; ///< may be empty
    double objective_scale_ = 1.0;
};

/// Caller-owned storage for an aggregate's VALUES at a candidate point.
///
/// ASSIGNED by the evaluation, never accumulated into, and indexed by DECLARED
/// global identities. Both rules -- and why the first is deliberately the
/// opposite of what assemble does to an arena -- are stated at
/// evaluate_candidate_values in model/nlp_aggregate.h.
struct CandidateValues {
    double &objective_;
    Eigen::Ref<Vec> equality_residuals_;
    Eigen::Ref<Vec> inequality_residuals_;
};

/// Caller-owned storage for an aggregate's values AND its first derivatives at
/// a candidate point.
///
/// This is the whole surface a model-level residual gate needs: objective
/// gradient, constraint adjoint gradient, both residual blocks, and -- with the
/// declaration's bounds beside it -- the bound multipliers' complementarity.
/// All of it at an arbitrary point, with no engine in the picture and no access
/// to whatever model sits behind the provider.
struct CandidateFirstOrder {
    CandidateValues values_;
    Eigen::Ref<Vec> objective_gradient_;
    Eigen::Ref<Vec> constraint_adjoint_gradient_;
};

/// Rejects a candidate point whose blocks do not match the dimensions they are
/// being evaluated against. The multiplier blocks are legally empty; any other
/// size is a caller error. Throws std::invalid_argument naming the block and
/// both numbers.
inline void validate_candidate_point(const CandidatePoint &point, Eigen::Index primal_vars,
                                     Eigen::Index equality_rows, Eigen::Index inequality_rows) {
    if (point.x_.size() != primal_vars) {
        throw std::invalid_argument(
            fmt::format("CandidatePoint: the primal block has {0} rows, but the aggregate "
                        "declares {1} primal variables",
                        point.x_.size(), primal_vars));
    }
    if (point.equality_multipliers_.size() != 0 &&
        point.equality_multipliers_.size() != equality_rows) {
        throw std::invalid_argument(
            fmt::format("CandidatePoint: the equality-multiplier block has {0} rows; it must be "
                        "either empty or {1} rows",
                        point.equality_multipliers_.size(), equality_rows));
    }
    if (point.inequality_multipliers_.size() != 0 &&
        point.inequality_multipliers_.size() != inequality_rows) {
        throw std::invalid_argument(
            fmt::format("CandidatePoint: the inequality-multiplier block has {0} rows; it must be "
                        "either empty or {1} rows",
                        point.inequality_multipliers_.size(), inequality_rows));
    }
}

/// Rejects candidate-values storage sized differently from the dimensions it is
/// being filled for. Throws std::invalid_argument naming the block and both
/// numbers.
inline void validate_candidate_values(const CandidateValues &out, Eigen::Index equality_rows,
                                      Eigen::Index inequality_rows) {
    if (out.equality_residuals_.size() != equality_rows) {
        throw std::invalid_argument(
            fmt::format("CandidateValues: the equality-residual block has {0} rows, but the "
                        "aggregate declares {1} equality rows",
                        out.equality_residuals_.size(), equality_rows));
    }
    if (out.inequality_residuals_.size() != inequality_rows) {
        throw std::invalid_argument(
            fmt::format("CandidateValues: the inequality-residual block has {0} rows, but the "
                        "aggregate declares {1} inequality rows",
                        out.inequality_residuals_.size(), inequality_rows));
    }
}

/// Rejects first-order storage sized differently from the dimensions it is
/// being filled for. Throws std::invalid_argument naming the block and both
/// numbers.
inline void validate_candidate_first_order(const CandidateFirstOrder &out, Eigen::Index primal_vars,
                                           Eigen::Index equality_rows,
                                           Eigen::Index inequality_rows) {
    validate_candidate_values(out.values_, equality_rows, inequality_rows);
    if (out.objective_gradient_.size() != primal_vars) {
        throw std::invalid_argument(
            fmt::format("CandidateFirstOrder: the objective-gradient block has {0} rows, but the "
                        "aggregate declares {1} primal variables",
                        out.objective_gradient_.size(), primal_vars));
    }
    if (out.constraint_adjoint_gradient_.size() != primal_vars) {
        throw std::invalid_argument(fmt::format(
            "CandidateFirstOrder: the constraint-adjoint-gradient block has {0} rows, but the "
            "aggregate declares {1} primal variables",
            out.constraint_adjoint_gradient_.size(), primal_vars));
    }
}

/// The digest half of an identity probe: a bitwise digest of the candidate
/// values at a point -- the objective, then each residual block, each preceded
/// by its length so the stream is self-delimiting and the same numbers split
/// differently between the blocks do not collide.
///
/// BITWISE over the value bytes, deliberately. It answers "is this the same
/// point?" for two probes taken in the same process by the same binary; it is
/// not a numeric comparison (+0.0 and -0.0 hash apart) and it is not a portable
/// pin.
inline std::uint64_t candidate_value_digest(const CandidateValues &values) {
    Fnv1a hash;
    hash.feed(&values.objective_, sizeof(double));
    hash.feed_index(values.equality_residuals_.size());
    for (Eigen::Index row = 0; row < values.equality_residuals_.size(); ++row) {
        const double value = values.equality_residuals_[row];
        hash.feed(&value, sizeof(double));
    }
    hash.feed_index(values.inequality_residuals_.size());
    for (Eigen::Index row = 0; row < values.inequality_residuals_.size(); ++row) {
        const double value = values.inequality_residuals_[row];
        hash.feed(&value, sizeof(double));
    }
    return hash.value();
}

// ---------------------------------------------------------------------------
// What one assemble call evaluates
// ---------------------------------------------------------------------------

/// What an assemble call EVALUATES. A separate vocabulary from ClaimDomain,
/// which names what a piece CLAIMS at layout time: an objective value is
/// evaluable but claims no structure, and conflating the two would blur exactly
/// the statement ClaimDomain exists to make checkable.
///
/// Four rules bind every request:
///
///   * No output is written that the request did not name. A view for an arena
///     a request does not touch may be empty, and the scalar objective value
///     has an out slot of its own rather than a repurposed view.
///   * Masking a request changes NOTHING structural. Layout, structural key and
///     structure epoch are functions of the declaration and the adopted
///     partition count alone, and every legal request subset carries the full
///     path's determinism guarantee.
///   * EXACTLY THE EIGHT NAMED SETS BELOW ARE LEGAL. The flags are a vocabulary
///     for reading a request, not a licence to compose one: a set outside the
///     eight names an evaluation shape no piece surface can produce in one pass
///     (the piece methods come in fixed combinations), so an implementation
///     would have to either over-evaluate or run two passes -- and both break
///     the call-for-call equivalence the eight-way mapping exists to give.
///     assemble() validates its request and throws on anything else;
///     validate_eval_request below is that check, so every provider spells the
///     refusal the same way.
///   * A request naming a constraint adjoint gradient or a constraint adjoint
///     Hessian CONSUMES the multipliers, so the point's multiplier blocks must
///     be present at full length rather than empty. See
///     request_consumes_multipliers and validate_full_multipliers.
enum class EvalRequest : std::uint32_t {
    kNone = 0,

    /// The scalar objective value, into RhsScatterView::objective_.
    kObjectiveValue = 1u << 0,
    /// The objective's gradient, into the objective-gradient arena.
    kObjectiveGradient = 1u << 1,
    /// The objective's Hessian contribution, into the KKT arena.
    kObjectiveHessian = 1u << 2,

    /// Both constraint residual blocks, into the equality- and
    /// inequality-residual arenas. One flag, not two, because no evaluation
    /// shape has ever produced one block without the other.
    kConstraintValues = 1u << 3,
    /// The constraints' adjoint gradient, into the adjoint-gradient arena.
    kConstraintAdjointGradient = 1u << 4,
    /// The constraint Jacobian blocks, into the KKT arena.
    kConstraintJacobian = 1u << 5,
    /// The constraints' adjoint Hessian contribution, into the KKT arena.
    kConstraintAdjointHessian = 1u << 6,
};

constexpr EvalRequest operator|(EvalRequest left, EvalRequest right) noexcept {
    return static_cast<EvalRequest>(static_cast<std::uint32_t>(left) |
                                    static_cast<std::uint32_t>(right));
}

constexpr EvalRequest operator&(EvalRequest left, EvalRequest right) noexcept {
    return static_cast<EvalRequest>(static_cast<std::uint32_t>(left) &
                                    static_cast<std::uint32_t>(right));
}

constexpr EvalRequest &operator|=(EvalRequest &left, EvalRequest right) noexcept {
    left = left | right;
    return left;
}

/// In-place masking, so narrowing a request down to a subset compiles the same
/// way widening one does.
constexpr EvalRequest &operator&=(EvalRequest &left, EvalRequest right) noexcept {
    left = left & right;
    return left;
}

/// True iff EVERY flag in `probe` is named by `request` -- ALL-BITS semantics,
/// not any-bit: `has_request(set, a | b)` asks whether the set names both.
/// Probing for kNone is vacuously true. To ask whether a request names ANY of a
/// group, compare the intersection: `(request & group) != EvalRequest::kNone`.
constexpr bool has_request(EvalRequest request, EvalRequest probe) noexcept {
    return (request & probe) == probe;
}

// ---------------------------------------------------------------------------
// THE EVALUATION-SHAPE MAPPING TABLE
// ---------------------------------------------------------------------------
//
// The partitioned evaluation engine has eight evaluation entry points whose
// output shapes genuinely differ. One assemble entry carrying a request replaces
// all eight, and the replacement is BIJECTIVE: each of the eight is exactly one
// request set, the eight sets are distinct, and no set names an output its
// shape did not produce. Nothing over-evaluates -- a later proof of call-for-
// call counter identity depends on that, since an entry that quietly computed
// more would move counters even where the numerics agreed.
//
// Reading the table: "entry" is the evaluation entry point of
// model/non_linear_program.h the row replaces; "writes" lists the destinations
// the shape fills; "empty" lists the destinations the request does not name,
// which may therefore be left empty by the caller. `objective` is
// RhsScatterView::objective_ (the scalar out slot); the four arena names are
// RhsScatterView's four members; `kkt` is the KktScatterView.
//
// ---------------------------------------------------------------------------
// 1. objective value only                    -> kRequestObjectiveOnly
//    entry   eval_obj
//    flags   kObjectiveValue
//    writes  objective
//    empty   every arena, kkt
//
// 2. objective value + both residual blocks  -> kRequestObjectiveAndConstraints
//    entry   eval_occ
//    flags   kObjectiveValue | kConstraintValues
//    writes  objective, equality_residuals_, inequality_residuals_
//    empty   objective_gradient_, constraint_adjoint_gradient_, kkt
//
// 3. objective value and gradient
//    + both residual blocks                  -> kRequestObjectiveGradientAndConstraints
//    entry   eval_ogc
//    flags   kObjectiveValue | kObjectiveGradient | kConstraintValues
//    writes  objective, objective_gradient_, equality_residuals_,
//            inequality_residuals_
//    empty   constraint_adjoint_gradient_, kkt
//
// 4. the full right-hand side: objective value
//    and gradient, both residual blocks,
//    constraint adjoint gradient             -> kRequestFirstOrderRhs
//    entry   eval_rhs
//    flags   kObjectiveValue | kObjectiveGradient | kConstraintValues
//            | kConstraintAdjointGradient
//    writes  objective, objective_gradient_, constraint_adjoint_gradient_,
//            equality_residuals_, inequality_residuals_
//    empty   kkt
//
// 5. constraint residuals + Jacobian, no
//    objective and no adjoint gradient       -> kRequestConstraintJacobianOnly
//    entry   eval_soe
//    flags   kConstraintValues | kConstraintJacobian
//    writes  equality_residuals_, inequality_residuals_, kkt (Jacobian blocks)
//    empty   objective, objective_gradient_, constraint_adjoint_gradient_
//    caller  also scatters its own solver KKT coefficients, outside assemble
//
// 6. first-order KKT: objective value and
//    gradient, residuals, adjoint gradient,
//    constraint Jacobian, no Hessian         -> kRequestFirstOrderKkt
//    entry   eval_aug
//    flags   kObjectiveValue | kObjectiveGradient | kConstraintValues
//            | kConstraintAdjointGradient | kConstraintJacobian
//    writes  objective, objective_gradient_, constraint_adjoint_gradient_,
//            equality_residuals_, inequality_residuals_, kkt (Jacobian blocks)
//    empty   -
//    caller  also scatters its own solver KKT coefficients, outside assemble
//
// 7. constraint-only KKT: residuals, adjoint
//    gradient, Jacobian and adjoint Hessian,
//    with the objective omitted entirely     -> kRequestConstraintKkt
//    entry   eval_kkt_no
//    flags   kConstraintValues | kConstraintAdjointGradient
//            | kConstraintJacobian | kConstraintAdjointHessian
//    writes  constraint_adjoint_gradient_, equality_residuals_,
//            inequality_residuals_, kkt (Jacobian + adjoint-Hessian blocks)
//    empty   objective, objective_gradient_
//    caller  also scatters its own solver KKT coefficients, outside assemble
//
// 8. the full KKT system                     -> kRequestFullKkt
//    entry   eval_kkt
//    flags   every flag above
//    writes  objective, objective_gradient_, constraint_adjoint_gradient_,
//            equality_residuals_, inequality_residuals_, kkt (objective
//            Hessian + Jacobian + adjoint-Hessian blocks)
//    empty   -
//    caller  also scatters its own solver KKT coefficients, outside assemble
//
// ---------------------------------------------------------------------------
// THE SOLVER-COEFFICIENT SCATTER: A RESPONSIBILITY THAT TRANSFERS.
//
// Every KKT-bearing shape above -- 5, 6, 7 and 8 -- does one more thing in its
// legacy form than this table's "writes" column lists. Each of them, at the end
// of the entry, also scatters the SOLVER's own KKT coefficients into the value
// array: the slack Jacobian, the primal and slack diagonals, and the equality
// and inequality pivot coefficients (non_linear_program.cpp, the
// `fill_solver_coeffs` arm of the parallel_task at the end of eval_soe,
// eval_aug, eval_kkt_no and eval_kkt).
//
// Those coefficients are not the provider's. They are values the consumer set
// on the consumer's own storage -- regularization diagonals, pivots, the slack
// block -- and under this contract filling them is a CONSUMER-owned step that
// happens OUTSIDE provider evaluation, before or after assemble() as the
// consumer's own assembly order requires. A provider neither knows them nor
// has anywhere to read them from.
//
// Written down here, in the table, and not left to the retarget to notice:
// these four rows are the only place where "what the entry did" is strictly
// larger than "what the request names", and the difference is a step that has
// to be re-attached on the consumer's side. A retarget that reads the table as
// the whole of the entry's behaviour drops the slack Jacobian and the pivots
// silently -- an assembled matrix missing its solver block factorizes, and
// factorizes wrong.
// ---------------------------------------------------------------------------
// ACCUMULATE-VS-ZERO: the discipline every shape shares.
//
// A provider ACCUMULATES into every destination it writes -- the KKT values
// through the location table, the four arenas through theirs, and the scalar
// objective out slot -- and never assigns to one, never zeroes one. The
// consumer owns its storage AND its initial state: it zeroes what it wants
// clean before the call. That is what the legacy entries do (the caller zeroes
// the KKT value array before dispatching, and its RHS blocks before the call;
// every fill is `+=`), and it is what makes several requests against one
// destination compose the way a single fan-out over partitions already does.
//
// Provider-internal coefficient scratch is a separate matter and stays the
// provider's own: the legacy entries zero their internal arrays at entry
// (all four for the shapes that fill gradients, the two constraint arrays for
// shape 2) precisely because their pieces accumulate into them. None of that is
// visible at this contract, and none of it is the consumer's to do.
// ---------------------------------------------------------------------------
// Three further points the table is easy to misread on:
//
//   * Shapes 5 and 7 legally leave the objective-gradient arena empty even
//     though the shapes they replace passed a gradient buffer in. Those shapes
//     summed an identically ZERO contribution into it -- no objective piece
//     runs, and the arena is zeroed before the fan-out -- so declining to write
//     it is observationally identical, not a behaviour change.
//     SHAPE 5 LEAVES THE CONSTRAINT-ADJOINT-GRADIENT ARENA EMPTY ON EXACTLY THE
//     SAME GROUND, and it is named here because the empty column is easy to
//     read as an oversight where the objective half is not. eval_soe calls the
//     constraints' JACOBIAN entry, which produces no adjoint gradient at all;
//     the entry nevertheless zeroes that arena and passes it to its fill, so
//     what the legacy shape summed in was identically zero, exactly as with the
//     objective gradient. Shape 7 is not in this half of the sentence: it calls
//     the adjoint entry and genuinely produces the constraint adjoint gradient,
//     so its empty column names the objective-gradient arena alone.
//   * kObjectiveValue is a flag of its own even though every shape that
//     produces the objective gradient produces the value with it, in one call.
//     Shape 1 produces the value alone, and the value's destination is a scalar
//     out slot rather than an arena, so it is separately nameable.
//   * kConstraintValues covers BOTH residual blocks. No shape splits them, and
//     splitting the flag would introduce request sets no evaluation shape has,
//     which is exactly what bijectivity rules out.
// ---------------------------------------------------------------------------

/// Shape 1.
inline constexpr EvalRequest kRequestObjectiveOnly = EvalRequest::kObjectiveValue;

/// Shape 2.
inline constexpr EvalRequest kRequestObjectiveAndConstraints =
    EvalRequest::kObjectiveValue | EvalRequest::kConstraintValues;

/// Shape 3.
inline constexpr EvalRequest kRequestObjectiveGradientAndConstraints =
    EvalRequest::kObjectiveValue | EvalRequest::kObjectiveGradient | EvalRequest::kConstraintValues;

/// Shape 4.
inline constexpr EvalRequest kRequestFirstOrderRhs =
    EvalRequest::kObjectiveValue | EvalRequest::kObjectiveGradient |
    EvalRequest::kConstraintValues | EvalRequest::kConstraintAdjointGradient;

/// Shape 5.
inline constexpr EvalRequest kRequestConstraintJacobianOnly =
    EvalRequest::kConstraintValues | EvalRequest::kConstraintJacobian;

/// Shape 6.
inline constexpr EvalRequest kRequestFirstOrderKkt =
    EvalRequest::kObjectiveValue | EvalRequest::kObjectiveGradient |
    EvalRequest::kConstraintValues | EvalRequest::kConstraintAdjointGradient |
    EvalRequest::kConstraintJacobian;

/// Shape 7.
inline constexpr EvalRequest kRequestConstraintKkt =
    EvalRequest::kConstraintValues | EvalRequest::kConstraintAdjointGradient |
    EvalRequest::kConstraintJacobian | EvalRequest::kConstraintAdjointHessian;

/// Shape 8.
inline constexpr EvalRequest kRequestFullKkt =
    EvalRequest::kObjectiveValue | EvalRequest::kObjectiveGradient |
    EvalRequest::kObjectiveHessian | EvalRequest::kConstraintValues |
    EvalRequest::kConstraintAdjointGradient | EvalRequest::kConstraintJacobian |
    EvalRequest::kConstraintAdjointHessian;

/// True iff `request` is one of the eight shapes the mapping table names.
constexpr bool is_legal_request(EvalRequest request) noexcept {
    return request == kRequestObjectiveOnly || request == kRequestObjectiveAndConstraints ||
           request == kRequestObjectiveGradientAndConstraints || request == kRequestFirstOrderRhs ||
           request == kRequestConstraintJacobianOnly || request == kRequestFirstOrderKkt ||
           request == kRequestConstraintKkt || request == kRequestFullKkt;
}

/// Rejects any request outside the eight named shapes. Every implementation of
/// assemble() calls this at entry, so a composed-but-unmapped request is
/// refused the same way everywhere rather than being served by whichever
/// provider happened to tolerate it.
inline void validate_eval_request(EvalRequest request) {
    if (!is_legal_request(request)) {
        throw std::invalid_argument(fmt::format(
            "EvalRequest: the flag combination 0x{0:x} is not one of the eight evaluation shapes "
            "this contract maps. The legal sets are kRequestObjectiveOnly (0x{1:x}), "
            "kRequestObjectiveAndConstraints (0x{2:x}), "
            "kRequestObjectiveGradientAndConstraints (0x{3:x}), kRequestFirstOrderRhs (0x{4:x}), "
            "kRequestConstraintJacobianOnly (0x{5:x}), kRequestFirstOrderKkt (0x{6:x}), "
            "kRequestConstraintKkt (0x{7:x}) and kRequestFullKkt (0x{8:x}); see the mapping table "
            "in model/candidate_point.h",
            static_cast<std::uint32_t>(request), static_cast<std::uint32_t>(kRequestObjectiveOnly),
            static_cast<std::uint32_t>(kRequestObjectiveAndConstraints),
            static_cast<std::uint32_t>(kRequestObjectiveGradientAndConstraints),
            static_cast<std::uint32_t>(kRequestFirstOrderRhs),
            static_cast<std::uint32_t>(kRequestConstraintJacobianOnly),
            static_cast<std::uint32_t>(kRequestFirstOrderKkt),
            static_cast<std::uint32_t>(kRequestConstraintKkt),
            static_cast<std::uint32_t>(kRequestFullKkt)));
    }
}

/// True iff `request` names an output the multipliers are an INPUT to: the
/// constraint adjoint gradient and the constraint adjoint Hessian are both
/// contractions of a constraint derivative against the multipliers, so a
/// request naming either cannot be served from an empty multiplier block.
constexpr bool request_consumes_multipliers(EvalRequest request) noexcept {
    constexpr EvalRequest kMultiplierConsumers =
        EvalRequest::kConstraintAdjointGradient | EvalRequest::kConstraintAdjointHessian;
    return (request & kMultiplierConsumers) != EvalRequest::kNone;
}

/// Rejects a point whose multiplier blocks are not present at full length.
///
/// The may-be-empty rule on CandidatePoint is about the paths that do not READ
/// the multipliers -- a values-only evaluation supplies neither, and empty
/// there means "no multipliers" rather than "zeros". Where they are read, empty
/// is not a legal spelling of a zero vector: it is a caller that forgot a block,
/// and silently contracting against nothing would return a wrong adjoint with
/// no diagnostic. Throws std::invalid_argument naming the block and both sizes.
inline void validate_full_multipliers(const CandidatePoint &point, Eigen::Index equality_rows,
                                      Eigen::Index inequality_rows) {
    if (point.equality_multipliers_.size() != equality_rows) {
        throw std::invalid_argument(fmt::format(
            "CandidatePoint: this evaluation reads the multipliers, so the equality-multiplier "
            "block must carry all {1} rows, but it has {0}",
            point.equality_multipliers_.size(), equality_rows));
    }
    if (point.inequality_multipliers_.size() != inequality_rows) {
        throw std::invalid_argument(fmt::format(
            "CandidatePoint: this evaluation reads the multipliers, so the inequality-multiplier "
            "block must carry all {1} rows, but it has {0}",
            point.inequality_multipliers_.size(), inequality_rows));
    }
}

} // namespace hven::solvers
