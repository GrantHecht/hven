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
/// Two rules bind every request:
///
///   * No output is written that the request did not name. A view for an arena
///     a request does not touch may be empty, and the scalar objective value
///     has an out slot of its own rather than a repurposed view.
///   * Masking a request changes NOTHING structural. Layout, structural key and
///     structure epoch are functions of the declaration and the adopted
///     partition count alone, and every legal request subset carries the full
///     path's determinism guarantee.
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

/// True iff EVERY flag in `probe` is named by `request`. Probing for kNone is
/// vacuously true.
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
//
// 8. the full KKT system                     -> kRequestFullKkt
//    entry   eval_kkt
//    flags   every flag above
//    writes  objective, objective_gradient_, constraint_adjoint_gradient_,
//            equality_residuals_, inequality_residuals_, kkt (objective
//            Hessian + Jacobian + adjoint-Hessian blocks)
//    empty   -
//
// ---------------------------------------------------------------------------
// Three points the table is easy to misread on:
//
//   * Shapes 5 and 7 legally leave the objective-gradient arena empty even
//     though the shapes they replace passed a gradient buffer in. Those shapes
//     summed an identically ZERO contribution into it -- no objective piece
//     runs, and the arena is zeroed before the fan-out -- so declining to write
//     it is observationally identical, not a behaviour change.
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

} // namespace hven::solvers
