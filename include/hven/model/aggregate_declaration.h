// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// aggregate_declaration.h — what a provider DECLARES, and what a declared piece
// must be able to do.
//
// The declaration is a VALUE, not a sequence of setter calls: the layout is
// then a pure function of the declaration and the adopted partition count.
//
// Engine-independent by construction: this header includes nothing from the
// interior-point machinery. It does DECLARE the two piece handle types the
// pieces are stored as, so the declaration can hold them by value -- a
// translation unit that constructs, copies or destroys an AggregateDeclaration
// therefore needs the piece definitions in scope; one that merely names the
// type does not.

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

/// @brief What a declared piece must be able to do, at layout time: report its
///        size and its threading posture, say how many KKT slots it will
///        claim, and claim them.
///
/// Stated over the indexing-data type rather than over a concrete one, so the
/// contract names no provider's internals. The type-erasure seam still decides
/// what gets stored; this concept says what a stored thing must be able to do.
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

/// @brief The three scalar-objective members only, split from the constraint
///        surface the same way the type-erasure seam splits them, so a type
///        missing exactly one group is diagnosed for exactly that.
template <class Piece, class IndexData>
concept ObjectiveAggregateSurface = requires(
    const Piece &piece, double scale, const Eigen::Ref<const Eigen::VectorXd> &x, double &value,
    Eigen::Ref<Eigen::VectorXd> gradient, Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
    Eigen::Ref<Eigen::VectorXi> indices, std::vector<std::mutex> &locks, const IndexData &data) {
    piece.objective(scale, x, value, data);
    piece.objective_gradient(scale, x, value, gradient, data);
    piece.objective_gradient_hessian(scale, x, value, gradient, kkt, indices, indices, locks, data);
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

/// @brief A piece of the objective kind: the constraint surface as well as the
///        objective one.
///
/// The type-erasure seam that stores an objective forwards both surfaces, so a
/// type carrying only the three scalar methods is not storable as an objective
/// however objective-shaped it looks -- a weaker concept would accept a piece
/// the seam then rejects. The pieces that exist today all satisfy it: an
/// objective piece answers the constraint surface by refusing it at run time,
/// which is a different question from whether the members are there to call.
template <class Piece, class IndexData>
concept ObjectiveAggregatePiece =
    ConstraintAggregatePiece<Piece, IndexData> && ObjectiveAggregateSurface<Piece, IndexData>;

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

    /// The internal fixing rows included in equality_rows_ (the AS-LAID
    /// count); subtracting this yields the user count the sizing entry takes.
    /// A fixing row is ONE SINGLE-ROW PIECE AT THE TAIL of
    /// equality_constraints_, the shape a fixed-variable treatment appends and
    /// the shape validate() holds this count to, so a consumer may split the
    /// tail off by piece count alone.
    ///
    /// TRUSTED: not derivable from the tail -- the piece type is erased, and a
    /// user constraint over a single application has that same shape. A
    /// declaration that labels user pieces as fixing rows is accepted here and
    /// loses them at the next treatment, which discards what this count
    /// claims.
    int fixing_rows_ = 0;

    /// Requested partition count; the adopted count is returned by
    /// negotiate_partition_count() and governs layout and the structural key.
    int partition_count_ = 1;

    /// The declared bound records, verbatim and unmerged, in declaration order.
    /// Repeated records on one index are intersected tightest-wins when the
    /// bounds are materialized -- see materialize_variable_bounds.
    std::vector<VariableBound> variable_bounds_;

    /// The declared records intersected into one record per primal variable, in
    /// variable order: length primal_vars_, starting from (-inf, +inf) and
    /// narrowed by each record in declaration order (lower = max, upper = min).
    ///
    /// This is what decides the layout, and therefore what the structural key's
    /// bound conjunct is taken over: two different declaration histories that
    /// intersect to the same per-variable structure lay the same system.
    ///
    /// Throws std::invalid_argument for a record naming a variable the
    /// declaration does not have, a NaN bound, or an intersection that is EMPTY
    /// (lower above upper) -- the last applied at the same point, in the same
    /// order, as the engine's own bound materializer applies it.
    std::vector<VariableBound> materialize_variable_bounds() const;

    /// @brief Rejects a declaration that cannot describe a problem. A provider
    ///        validates its declaration with validate() before its first
    ///        layout.
    ///
    /// @throws std::invalid_argument naming what disagreed and both numbers,
    ///         for any of: a non-positive partition count; a negative
    ///         dimension; a fixing-row count outside [0, equality_rows_]; a
    ///         fixing-row count the tail of the equality list does not have
    ///         one single-row piece each for; a bound naming a variable the
    ///         declaration does not have; a NaN bound; a single record whose
    ///         two finite sides are inverted; a piece row count that does not
    ///         sum to the declared row count; and a bound history whose
    ///         intersection is empty.
    ///
    /// The piece-sum checks run only when at least one list carries pieces --
    /// "no pieces" is a property of all three lists together, and empty lists
    /// satisfy the sums vacuously.
    ///
    /// Carve-out, in the safe direction: the engine's own bound materializer
    /// silently no-op-drops a fully unbounded record -- (-inf, +inf) narrows
    /// nothing -- before it is staged and therefore never range-checks its
    /// index, so a record naming a variable the declaration does not have is
    /// refused here and accepted there. This boundary is the stricter of the
    /// two, and a record that changes no bound cannot change a layout.
    void validate() const;
};

} // namespace hven::solvers
