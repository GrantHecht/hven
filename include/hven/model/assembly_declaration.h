// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// assembly_declaration.h — a problem stated as pieces, and what a declared piece
// must be able to do.
//
// The declaration is a VALUE: the layout is a pure function of the declaration
// and the adopted partition count. It is built with the builder members below
// (add_objective, add_equality, add_inequality, set_bounds, set_partitions),
// which derive and check the row counts, or written field by field on the raw
// route (docs/assembly-protocol.md). Problem(AssemblyDeclaration) solves it.
//
// Engine-independent by construction: this header includes the piece protocol
// (model/) and nothing of either engine.

#include <concepts>
#include <limits>
#include <mutex>
#include <string>
#include <type_traits>
#include <vector>

#include <Eigen/Core>
#include <Eigen/SparseCore>

#include "hven/core/types.h"
#include "hven/model/constraint_function.h"
#include "hven/model/local_blocks.h"
#include "hven/model/objective_function.h"
#include "hven/model/piece_interfaces.h"

namespace hven {

/// One declared variable bound: a variable's global index and the two sides.
///
/// Bounds are part of the declared problem rather than of a staging record, so
/// they travel with the declaration as data. Repeated declarations on one index
/// are intersected (tightest wins) when the bounds are materialized, so this
/// type records a declaration verbatim and merges nothing.
struct VariableBound {
    int index = 0;
    double lower = -std::numeric_limits<double>::infinity();
    double upper = std::numeric_limits<double>::infinity();

    friend bool operator==(const VariableBound &, const VariableBound &) = default;
};

/// @brief What a declared raw-protocol piece of either kind must be able to do,
///        at layout time: report its name, its input count and its threading
///        posture, say how many KKT slots it will claim, and claim them.
///
/// Stated over the indexing-data type rather than over a concrete one, so the
/// contract names no provider's internals. The type-erasure seam still decides
/// what gets stored; this concept says what a stored thing must be able to do.
template <class Piece, class IndexData>
concept AssemblyPiece =
    requires(const Piece &piece, Piece &mutable_piece, Eigen::Ref<Eigen::VectorXi> indices,
             int &free_slot, int offset, bool flag, IndexData &data) {
        { piece.name() } -> std::convertible_to<std::string>;
        { piece.input_rows() } -> std::convertible_to<int>;
        { piece.thread_safe() } -> std::convertible_to<bool>;
        mutable_piece.get_kkt_space(indices, indices, free_slot, offset, flag, flag, data);
        { piece.num_kkt_elements(flag, flag) } -> std::convertible_to<int>;
    };

/// @brief The three scalar-objective members only, split from the constraint
///        surface the same way the type-erasure seam splits them, so a type
///        missing exactly one group is diagnosed for exactly that.
template <class Piece, class IndexData>
concept ObjectiveAssemblySurface = requires(
    const Piece &piece, double scale, const Eigen::Ref<const Eigen::VectorXd> &x, double &value,
    Eigen::Ref<Eigen::VectorXd> gradient, Eigen::SparseMatrix<double, Eigen::RowMajor> &kkt,
    Eigen::Ref<Eigen::VectorXi> indices, std::vector<std::mutex> &locks, const IndexData &data) {
    piece.objective(scale, x, value, data);
    piece.objective_gradient(scale, x, value, gradient, data);
    piece.objective_gradient_hessian(scale, x, value, gradient, kkt, indices, indices, locks, data);
};

/// A piece of the constraint kind: everything a piece must do, its output
/// count, and the five constraint evaluation shapes.
template <class Piece, class IndexData>
concept ConstraintAssemblyPiece =
    AssemblyPiece<Piece, IndexData> &&
    requires(const Piece &piece) {
        { piece.output_rows() } -> std::convertible_to<int>;
    } &&
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

/// @brief A piece of the objective kind: everything a piece must do and the
///        three scalar-objective members. No output count and no constraint
///        surface: the handle that stores an objective forwards neither.
template <class Piece, class IndexData>
concept ObjectiveAssemblyPiece =
    AssemblyPiece<Piece, IndexData> && ObjectiveAssemblySurface<Piece, IndexData>;

/// The value a provider hands over: the three piece lists, the three
/// dimensions, the partition count it wants, and the declared variable bounds.
///
/// THE BUILDER is the default way to fill one:
///
///     AssemblyDeclaration d(n);                         // n variables
///     d.add_objective(Term{...}, vars);                 // vars: kInputs x applications
///     d.add_equality(Defect{...}, vars, rows);          // rows: kOutputs x applications
///     d.add_inequality(Path{...}, vars, rows);          // c(x) <= 0 rows
///     d.set_bounds(first, last, lower, upper);          // variables first..last, inclusive
///     d.set_partitions(4);
///     Problem problem(std::move(d));
///
/// A piece is a local-block piece (model/local_blocks.h) or a raw-protocol piece
/// (docs/assembly-protocol.md). Column V of an index map describes application V:
/// the variables it reads, and the rows of its block its outputs are. The builder
/// derives equality_rows, inequality_rows and the two shared-row overcounts
/// from the row maps: a block's row space is 0 .. (largest row named), every row
/// in it must be named by some application (checked by validate()), and rows
/// named by several applications are summed. Each call refuses, by name, an index
/// map of the wrong shape and an index outside the declaration.
///
/// THE RAW ROUTE writes the fields below directly; the builder members and the
/// raw fields fill the same value.
struct AssemblyDeclaration {
    /// @brief An empty declaration over no variables: the raw route's starting
    ///        point, whose fields are then written directly. The builder starts
    ///        from AssemblyDeclaration(n); a builder call on a declaration with
    ///        no variables refuses any variable and says so.
    AssemblyDeclaration();
    /// @brief An empty declaration over @p variables primal variables.
    /// @throws std::invalid_argument if @p variables is negative or past INT_MAX.
    explicit AssemblyDeclaration(Index variables);
    ~AssemblyDeclaration();
    AssemblyDeclaration(const AssemblyDeclaration &);
    AssemblyDeclaration &operator=(const AssemblyDeclaration &);
    AssemblyDeclaration(AssemblyDeclaration &&) noexcept;
    AssemblyDeclaration &operator=(AssemblyDeclaration &&) noexcept;

    /// @brief Adds an objective piece: its value, summed over the applications,
    ///        joins the objective.
    /// @param piece A local-block objective piece or a raw-protocol objective piece.
    /// @param variables kInputs x applications: column V lists the variables
    ///        application V reads, in the piece's input order.
    /// @throws std::invalid_argument on a map whose row count is not the piece's
    ///         input count, a map with no application, or a variable index outside
    ///         [0, primal_vars).
    template <class Piece>
    AssemblyDeclaration &add_objective(const Piece &piece, const Eigen::MatrixXi &variables) {
        add_objective_handle(ObjectiveInterface(piece), variables);
        return *this;
    }

    /// @brief Adds an equality piece: its outputs are equality rows c(x) = 0.
    /// @param piece A local-block constraint piece or a raw-protocol constraint piece.
    /// @param variables kInputs x applications, as for add_objective.
    /// @param rows kOutputs x applications: column V lists the equality rows
    ///        application V's outputs are, in the piece's output order.
    /// @throws std::invalid_argument on a map of the wrong shape (rows differing
    ///         from the piece's counts, or the two maps' application counts
    ///         differing), a map with no application, a variable index outside
    ///         [0, primal_vars), a negative row, or a row of INT_MAX (a block's
    ///         row count, the largest row plus one, must fit an int).
    template <class Piece>
    AssemblyDeclaration &add_equality(const Piece &piece, const Eigen::MatrixXi &variables,
                                      const Eigen::MatrixXi &rows) {
        add_constraint_handle(ConstraintInterface(piece), variables, rows, false);
        return *this;
    }

    /// @brief Adds an inequality piece: its outputs are inequality rows
    ///        c(x) <= 0. Arguments and refusals as add_equality.
    template <class Piece>
    AssemblyDeclaration &add_inequality(const Piece &piece, const Eigen::MatrixXi &variables,
                                        const Eigen::MatrixXi &rows) {
        add_constraint_handle(ConstraintInterface(piece), variables, rows, true);
        return *this;
    }

    /// @brief Bounds one variable: lower <= x[index] <= upper. Either side may be
    ///        infinite (hven::kInfinity). Repeated bounds on one variable
    ///        intersect, tightest wins.
    /// @throws std::invalid_argument on an index outside [0, primal_vars), a NaN
    ///         side, or lower above upper.
    AssemblyDeclaration &set_bounds(Index index, double lower, double upper);

    /// @brief Bounds variables first .. last, INCLUSIVE, each to [lower, upper].
    /// @throws std::invalid_argument on last below first, an index outside
    ///         [0, primal_vars), a NaN side, or lower above upper.
    AssemblyDeclaration &set_bounds(Index first, Index last, double lower, double upper);

    /// @brief The number of evaluation partitions the interior-point layout is
    ///        asked for; the one place the count is given.
    ///
    /// The layout may clamp a request DOWN, never up: it adopts at most one
    /// partition per 1000 claimed KKT elements, and at least one, so a small
    /// problem asking for several runs on fewer. Problem::num_partitions() reports
    /// the count adopted. Partitions are evaluated concurrently, so a piece is
    /// evaluated from several threads when more than one is adopted.
    /// @throws std::invalid_argument if @p partitions is below 1 or past INT_MAX.
    AssemblyDeclaration &set_partitions(Index partitions);

    std::vector<ObjectiveFunction> objectives;
    std::vector<ConstraintFunction> equality_constraints;
    std::vector<ConstraintFunction> inequality_constraints;

    int primal_vars = 0;
    int equality_rows = 0;
    int inequality_rows = 0;

    /// The internal fixing rows included in equality_rows (the AS-LAID
    /// count); subtracting this yields the user count the sizing entry takes.
    /// A fixing row is ONE SINGLE-ROW PIECE AT THE TAIL of
    /// equality_constraints, the shape a fixed-variable treatment appends and
    /// the shape validate() holds this count to, so a consumer may split the
    /// tail off by piece count alone.
    ///
    /// TRUSTED: not derivable from the tail -- the piece type is erased, and a
    /// user constraint over a single application has that same shape. A
    /// declaration that labels user pieces as fixing rows is accepted here and
    /// loses them at the next treatment, which discards what this count
    /// claims.
    ///
    /// THIS IS THE DECLARATION'S TRUST BOUNDARY, and two other fields stand on
    /// it: equality_shared_row_overcount and inequality_shared_row_overcount
    /// below. What the three have in common is the reason they are trusted
    /// rather than checked -- each states something about the pieces that the
    /// piece surface does not expose, so nothing at this boundary can tell a
    /// correct declaration from a wrong one. A wrong fixing-row count silently
    /// discards user rows at the next treatment; a wrong overcount silently
    /// buys a drifting piece list past the one conjunct that would have caught
    /// it. Both are refused for SIGN and for LEGALITY (an overcount is legal
    /// only where pieces exist, a fixing-row count only inside the equality row
    /// space), and neither is verified for VALUE, because there is nothing here
    /// to verify it against. A provider that states one states it correctly.
    int fixing_rows = 0;

    /// @brief How many rows the equality pieces claim IN EXCESS of
    ///        equality_rows, because pieces share rows with each other.
    ///
    /// A piece reports how many rows it claims; it does not report WHICH rows,
    /// and placement is assigned at lay. So a provider whose pieces
    /// deliberately accumulate into shared rows -- an integrand summed across
    /// several pieces into one constraint row is the shape that produces it --
    /// hands out more claimed rows than the row space it declares, and the
    /// difference is not recoverable from anything the declaration carries.
    /// This states it: validate()'s piece-sum conjunct reads
    /// `sum(piece rows) == equality_rows + this`, an EQUALITY rather than an
    /// inequality, so a provider with row-overlapping pieces is expressible
    /// and a piece list that has drifted by an unstated amount is still
    /// refused.
    ///
    /// Zero for a provider whose pieces claim disjoint rows, which makes a
    /// zero-overcount declaration exactly what it was before this field
    /// existed.
    ///
    /// TRUSTED, on fixing_rows's footing and for the same reason: nothing at
    /// this boundary can tell a shared row from a piece list that has drifted,
    /// so a wrong count buys silence from the one conjunct that would have
    /// caught the drift. Held non-negative, and legal only where the
    /// declaration carries pieces at all -- with no pieces there is no sum for
    /// an excess to be an excess OF.
    ///
    /// NOT A STRUCTURAL-KEY CONJUNCT, mirroring fixing_rows: the key is taken
    /// over the claim stream, the adopted partition count and the materialized
    /// bound structure, and two declarations differing only in this field
    /// describe the same laid system.
    ///
    /// Trusted on the way IN and derived on the way OUT: a provider emitting a
    /// declaration for a layout it has already laid holds both the pieces and
    /// the row space, so it computes this rather than replaying a number it
    /// was handed -- which is what makes an emitted declaration one that
    /// validates and can be adopted again, at a nonzero overcount as at zero.
    int equality_shared_row_overcount = 0;

    /// @brief The inequality block's shared-row overcount, on exactly the
    ///        terms equality_shared_row_overcount states.
    ///
    /// Per block rather than one number for both: the two conjuncts are
    /// checked separately and a refusal names the block it is about, so one
    /// block's overlap can never excuse the other block's drift.
    int inequality_shared_row_overcount = 0;

    /// Requested partition count (set_partitions); the adopted count is returned
    /// by negotiate_partition_count() and governs layout and the structural key.
    int partition_count = 1;

    /// Whether the builder derived the row counts. When set, validate() also
    /// refuses a row of either block that no application names, and a row past
    /// the block's count: a row with no piece behind it is a constraint 0 = 0
    /// with an empty Jacobian row.
    bool rows_derived = false;

    /// The declared bound records, verbatim and unmerged, in declaration order.
    /// Repeated records on one index are intersected tightest-wins when the
    /// bounds are materialized -- see materialize_variable_bounds.
    std::vector<VariableBound> variable_bounds;

    /// The declared records intersected into one record per primal variable, in
    /// variable order: length primal_vars, starting from (-inf, +inf) and
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
    ///         dimension; a fixing-row count outside [0, equality_rows]; a
    ///         fixing-row count the tail of the equality list does not have
    ///         one single-row piece each for; a bound naming a variable the
    ///         declaration does not have; a NaN bound; a single record whose
    ///         two finite sides are inverted; a piece row count that does not
    ///         sum to the declared row count plus that block's declared
    ///         shared-row overcount; a negative shared-row overcount; a
    ///         non-zero shared-row overcount on a declaration with no pieces;
    ///         a bound history whose intersection is empty; a piece row sum
    ///         past INT_MAX; an equality piece count past INT_MAX; and, on a
    ///         declaration whose rows the builder derived, a row no application
    ///         names or a row past its block's count.
    ///
    /// The piece-sum checks run only when at least one list carries pieces --
    /// "no pieces" is a property of all three lists together, and empty lists
    /// satisfy the sums vacuously. The two shared-row overcounts are checked
    /// for sign always and for legality against that same property: an excess
    /// over a sum that is not being checked is a number with nothing to mean.
    ///
    /// Carve-out, in the safe direction: the engine's own bound materializer
    /// silently no-op-drops a fully unbounded record -- (-inf, +inf) narrows
    /// nothing -- before it is staged and therefore never range-checks its
    /// index, so a record naming a variable the declaration does not have is
    /// refused here and accepted there. This boundary is the stricter of the
    /// two, and a record that changes no bound cannot change a layout.
    void validate() const;

  private:
    void add_objective_handle(ObjectiveInterface piece, const Eigen::MatrixXi &variables);
    void add_constraint_handle(ConstraintInterface piece, const Eigen::MatrixXi &variables,
                               const Eigen::MatrixXi &rows, bool inequality);
};

} // namespace hven
