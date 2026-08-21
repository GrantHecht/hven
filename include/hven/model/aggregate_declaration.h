// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

#pragma once

// aggregate_declaration.h — what a provider DECLARES, and what a declared piece
// must be able to do.
//
// The declaration is a VALUE, not a sequence of setter calls, and that is
// load-bearing rather than stylistic: the layout is then a pure function of the
// declaration and the adopted partition count, which is the property a
// layout-determinism check asserts against and the property the structural key
// keys on.
//
// Engine-independent by construction: this header includes nothing from the
// interior-point machinery. It does DECLARE the two piece handle types the
// pieces are stored as, so the declaration can hold them by value; a
// declaration is a value type over pieces, and a piece list of pointers to
// somewhere else would give up exactly the property above. A translation unit
// that constructs, copies or destroys an AggregateDeclaration therefore needs
// the piece definitions in scope; one that merely names the type does not.
// (The same forward-declaration shape the piece adapter already uses for the
// evaluation engine.)

#include <concepts>
#include <limits>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

namespace hven::solvers {

struct ObjectiveFunction;
struct ConstraintFunction;

/// One declared variable bound: a variable's global index and the two sides.
///
/// Bounds are part of the declared problem rather than of a staging record, so
/// they travel with the declaration as data. Repeated declarations on one index
/// are intersected (tightest wins) when the bounds are materialized, so this
/// type records a declaration verbatim and merges nothing.
struct VariableBound {
    int index_ = 0;
    double lower_ = -std::numeric_limits<double>::infinity();
    double upper_ = std::numeric_limits<double>::infinity();

    friend bool operator==(const VariableBound &, const VariableBound &) = default;
};

/// What a declared piece must be able to do, at layout time: report its size
/// and its threading posture, say how many KKT slots it will claim, and claim
/// them.
///
/// The piece surface is otherwise an unwritten duck-typing contract, enforced
/// only by whether the type-erasure seam compiles. This concept states it. It
/// does not REPLACE that seam: the seam still decides what gets stored; the
/// concept says what a stored thing must be able to do.
///
/// Stated over the indexing-data type rather than over a concrete one, so the
/// contract names no provider's internals. A provider instantiates it with
/// whatever per-piece indexing state it threads through its own claim pass.
///
/// The claim pass is spelled here as the pieces spell it today. Retiring the
/// `get_`-prefixed spelling in favour of a claim-space-typed one is a change to
/// the pieces, and belongs with the change that makes it -- not with the header
/// that first writes the contract down.
template <class Piece, class IndexData>
concept AggregatePiece =
    requires(const Piece &piece, Piece &mutable_piece, Eigen::Ref<Eigen::VectorXi> indices,
             int &free_slot, int offset, bool flag, IndexData &data) {
        { piece.name() } -> std::convertible_to<std::string>;
        { piece.input_rows() } -> std::convertible_to<int>;
        { piece.output_rows() } -> std::convertible_to<int>;
        { piece.thread_safe() } -> std::convertible_to<bool>;
        mutable_piece.get_kkt_space(indices, indices, free_slot, offset, flag, flag, data);
        { piece.num_kkt_elements(flag, flag) } -> std::convertible_to<int>;
    };

/// A piece of the objective kind: everything a piece must do, plus the scalar
/// objective evaluations.
template <class Piece, class IndexData>
concept ObjectiveAggregatePiece =
    AggregatePiece<Piece, IndexData> &&
    requires(const Piece &piece, double scale, const Eigen::Ref<const Eigen::VectorXd> &x,
             double &value, Eigen::Ref<Eigen::VectorXd> gradient,
             Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt, Eigen::Ref<Eigen::VectorXi> indices,
             std::vector<std::mutex> &locks, const IndexData &data) {
        piece.objective(scale, x, value, data);
        piece.objective_gradient(scale, x, value, gradient, data);
        piece.objective_gradient_hessian(scale, x, value, gradient, kkt, indices, indices, locks,
                                         data);
    };

/// A piece of the constraint kind: everything a piece must do, plus the five
/// constraint evaluation shapes.
template <class Piece, class IndexData>
concept ConstraintAggregatePiece =
    AggregatePiece<Piece, IndexData> &&
    requires(const Piece &piece, const Eigen::Ref<const Eigen::VectorXd> &x,
             Eigen::Ref<Eigen::VectorXd> values, Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
             Eigen::Ref<Eigen::VectorXi> indices, std::vector<std::mutex> &locks,
             const IndexData &data) {
        piece.constraints(x, values, data);
        piece.constraints_adjointgradient(x, x, values, values, data);
        piece.constraints_jacobian(x, values, kkt, indices, indices, locks, data);
        piece.constraints_jacobian_adjointgradient(x, x, values, values, kkt, indices, indices,
                                                   locks, data);
        piece.constraints_jacobian_adjointgradient_adjointhessian(x, x, values, values, kkt,
                                                                  indices, indices, locks, data);
    };

/// The value a provider hands over: the three piece lists, the three
/// dimensions, the partition count it wants, and the declared variable bounds.
///
/// Special members are declared here and defined out of line, so a consumer may
/// name, default-construct, inspect and validate a declaration without the
/// piece definitions in scope; filling the piece lists needs them.
struct AggregateDeclaration {
    AggregateDeclaration();
    ~AggregateDeclaration();
    AggregateDeclaration(const AggregateDeclaration &);
    AggregateDeclaration &operator=(const AggregateDeclaration &);
    AggregateDeclaration(AggregateDeclaration &&) noexcept;
    AggregateDeclaration &operator=(AggregateDeclaration &&) noexcept;

    std::vector<ObjectiveFunction> objectives_;
    std::vector<ConstraintFunction> equality_constraints_;
    std::vector<ConstraintFunction> inequality_constraints_;

    int primal_vars_ = 0;
    int equality_rows_ = 0;
    int inequality_rows_ = 0;

    /// The partition count REQUESTED. What is adopted is what
    /// NlpAggregate::negotiate_partition_count returns, and it is the adopted
    /// count that is a conjunct of the structural key.
    int partition_count_ = 1;

    std::vector<VariableBound> variable_bounds_;

    /// Rejects a declaration that cannot describe a problem: non-positive
    /// partition count, negative dimensions, piece row counts that do not sum
    /// to the declared row counts, and bounds naming a variable the declaration
    /// does not have. Throws std::invalid_argument naming what disagreed and
    /// both numbers.
    void validate() const;
};

} // namespace hven::solvers
