// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AssemblyDeclaration's special members, its builder and its boundary
// validation.

#include "hven/model/assembly_declaration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "hven/model/constraint_function.h"
#include "hven/model/objective_function.h"
#include "hven/model/structure_identity.h"

namespace hven::solvers {

AssemblyDeclaration::AssemblyDeclaration() = default;

AssemblyDeclaration::AssemblyDeclaration(Index variables) {
    if (variables < 0 || variables > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(fmt::format(
            "AssemblyDeclaration: {0} variables is not a variable count a declaration can state "
            "(it must lie in [0, {1}])",
            variables, std::numeric_limits<int>::max()));
    }
    primal_vars_ = static_cast<int>(variables);
}
AssemblyDeclaration::~AssemblyDeclaration() = default;
AssemblyDeclaration::AssemblyDeclaration(const AssemblyDeclaration &) = default;
AssemblyDeclaration &AssemblyDeclaration::operator=(const AssemblyDeclaration &) = default;
AssemblyDeclaration::AssemblyDeclaration(AssemblyDeclaration &&) noexcept = default;
AssemblyDeclaration &AssemblyDeclaration::operator=(AssemblyDeclaration &&) noexcept = default;

namespace {

/// Rows the pieces of one block claim, summed over the block.
///
/// @param pieces the block's pieces.
/// @param which  the block's name, for the refusal message.
/// @throws std::invalid_argument if the summed row count is past INT_MAX.
///
/// Accumulated at 64 bits and narrowed once, checked. Unreachable in any
/// problem this engine can lay, and checked anyway: an unchecked narrowing
/// would wrap into a total the row-count comparison below then accepts, turning
/// an impossible input into a wrong answer instead of a refusal.
int declared_rows(const std::vector<ConstraintFunction> &pieces, const char *which) {
    std::int64_t rows = 0;
    for (const auto &piece : pieces) {
        rows += piece.num_con_eles();
    }
    if (rows > static_cast<std::int64_t>(std::numeric_limits<int>::max())) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration: the {0} pieces claim {1} rows in total, past the "
                        "{2} a declaration can state",
                        which, rows, std::numeric_limits<int>::max()));
    }
    return static_cast<int>(rows);
}

/// The piece-sum conjunct for one constraint block, as an EQUALITY against the
/// declared row count plus that block's declared shared-row overcount.
///
/// @param claimed   rows the block's pieces claim in total.
/// @param declared  rows the declaration states for the block.
/// @param overcount rows the pieces claim in excess because they share rows.
/// @param which     the block's name, for the refusal message.
/// @throws std::invalid_argument if the three do not balance.
///
/// KEPT AN EQUALITY rather than weakened to `claimed >= declared`. The excess
/// is real and has to be expressible, but it is a number the provider knows
/// and can state; an inequality would let a piece list that has drifted by any
/// amount pass the one check that would have caught it. What the boundary
/// cannot do is verify the stated excess -- see the field's own trust note --
/// so the refusal names all three numbers and leaves the reader to see which
/// one is wrong.
void require_piece_sum(int claimed, int declared, int overcount, const char *which) {
    // At 64 bits: `declared` and `overcount` are each in [0, INT_MAX] by the
    // checks above, and their sum is not.
    const std::int64_t expected =
        static_cast<std::int64_t>(declared) + static_cast<std::int64_t>(overcount);
    if (static_cast<std::int64_t>(claimed) != expected) {
        throw std::invalid_argument(fmt::format(
            "AssemblyDeclaration: the {0} pieces claim {1} rows in total, but the declaration "
            "states {2} {0} rows with a shared-row overcount of {3}, which is {4}",
            which, claimed, declared, overcount, expected));
    }
}

void require_no_overcount_without_pieces(int overcount, const char *which) {
    if (overcount != 0) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration: the {0} shared-row overcount is {1}, but the "
                        "declaration carries no pieces at all; an overcount states how far a "
                        "piece sum exceeds a declared row count, and there is no piece sum here",
                        which, overcount));
    }
}

void require_non_negative(int value, const char *what) {
    if (value < 0) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration: {0} is {1}, which is not a count", what, value));
    }
}

/// Refuses an index map whose shape does not fit the piece, or that names no
/// application.
void require_map_shape(const Eigen::MatrixXi &map, int expected_rows, const char *call,
                       const std::string &piece, const char *map_name, const char *count_name) {
    if (map.rows() != expected_rows) {
        throw std::invalid_argument(fmt::format(
            "AssemblyDeclaration::{0}: piece '{1}' has {2} {3}, but the {4} map has {5} rows; "
            "row i of column V names {3} i of application V",
            call, piece, expected_rows, count_name, map_name, map.rows()));
    }
    if (map.cols() < 1) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::{0}: the {1} map of piece '{2}' has no column, so "
                        "it names no application",
                        call, map_name, piece));
    }
}

/// The declaration's variable space, as a refusal states it: "N variables
/// (indices 0..N-1)", or, for a declaration with none (default-constructed),
/// that it has none and how to give it some.
std::string variable_space(int primal_vars) {
    if (primal_vars <= 0) {
        return "no variables (construct it as AssemblyDeclaration(n), n the variable count)";
    }
    return fmt::format("{0} variables (indices 0..{1})", primal_vars, primal_vars - 1);
}

/// Refuses a variable index outside the declaration.
void require_variables_in_range(const Eigen::MatrixXi &variables, int primal_vars, const char *call,
                                const std::string &piece) {
    for (Index V = 0; V < variables.cols(); V++) {
        for (Index i = 0; i < variables.rows(); i++) {
            const int index = variables(i, V);
            if (index < 0 || index >= primal_vars) {
                throw std::invalid_argument(fmt::format(
                    "AssemblyDeclaration::{0}: application {1} of piece '{2}' names variable {3} "
                    "as input {4}, but the declaration has {5}",
                    call, V, piece, index, i, variable_space(primal_vars)));
            }
        }
    }
}

/// One past the largest row a block's applications name (0 for an empty block).
int row_space_end(const std::vector<ConstraintFunction> &pieces) {
    int end = 0;
    for (const ConstraintFunction &piece : pieces) {
        const Eigen::MatrixXi &rows = piece.index_data_.get_c_index();
        if (rows.size() > 0) {
            end = std::max(end, rows.maxCoeff() + 1);
        }
    }
    return end;
}

/// Refuses, on a block whose rows the builder derived, a row no application names
/// and a row past the block's count.
void require_rows_covered(const std::vector<ConstraintFunction> &pieces, int declared,
                          const char *which) {
    std::vector<char> named(static_cast<std::size_t>(std::max(declared, 0)), 0);
    for (std::size_t k = 0; k < pieces.size(); k++) {
        const Eigen::MatrixXi &rows = pieces[k].index_data_.get_c_index();
        for (Index V = 0; V < rows.cols(); V++) {
            for (Index j = 0; j < rows.rows(); j++) {
                const int row = rows(j, V);
                if (row < 0 || row >= declared) {
                    throw std::invalid_argument(fmt::format(
                        "AssemblyDeclaration: {0} piece {1} ('{2}') names {0} row {3} in "
                        "application {4}, outside the {5} {0} rows the declaration states",
                        which, k, pieces[k].function_.name(), row, V, declared));
                }
                named[static_cast<std::size_t>(row)] = 1;
            }
        }
    }
    for (int row = 0; row < declared; row++) {
        if (named[static_cast<std::size_t>(row)] == 0) {
            throw std::invalid_argument(fmt::format(
                "AssemblyDeclaration: {0} row {1} is named by no application of any {0} piece; "
                "every row of a block's row space 0..{2} must be some piece's output (a row "
                "with no piece is the constraint 0 = 0)",
                which, row, declared - 1));
        }
    }
}

/// Refuses a bound side that is NaN, or a lower side above the upper.
void require_bound_pair(Index index, double lower, double upper) {
    if (std::isnan(lower) || std::isnan(upper)) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::set_bounds: the bound on variable {0} is not a "
                        "number (lower={1}, upper={2})",
                        index, lower, upper));
    }
    if (lower > upper) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::set_bounds: the bound on variable {0} is inverted "
                        "(lower={1} is above upper={2})",
                        index, lower, upper));
    }
}

} // namespace

void AssemblyDeclaration::add_objective_handle(ObjectiveInterface piece,
                                               const Eigen::MatrixXi &variables) {
    const std::string name = piece.name();
    require_map_shape(variables, piece.input_rows(), "add_objective", name, "variable", "inputs");
    require_variables_in_range(variables, primal_vars_, "add_objective", name);
    objectives_.emplace_back(piece, variables);
}

void AssemblyDeclaration::add_constraint_handle(ConstraintInterface piece,
                                                const Eigen::MatrixXi &variables,
                                                const Eigen::MatrixXi &rows, bool inequality) {
    const char *call = inequality ? "add_inequality" : "add_equality";
    const std::string name = piece.name();
    require_map_shape(variables, piece.input_rows(), call, name, "variable", "inputs");
    require_map_shape(rows, piece.output_rows(), call, name, "row", "outputs");
    if (rows.cols() != variables.cols()) {
        throw std::invalid_argument(fmt::format(
            "AssemblyDeclaration::{0}: piece '{1}' has {2} applications in its variable map and "
            "{3} in its row map; column V of both describes application V",
            call, name, variables.cols(), rows.cols()));
    }
    require_variables_in_range(variables, primal_vars_, call, name);
    for (Index V = 0; V < rows.cols(); V++) {
        for (Index j = 0; j < rows.rows(); j++) {
            if (rows(j, V) < 0) {
                throw std::invalid_argument(fmt::format(
                    "AssemblyDeclaration::{0}: application {1} of piece '{2}' names row {3} as "
                    "output {4}; a row is a non-negative index",
                    call, V, name, rows(j, V), j));
            }
            // The row space is 0 .. (largest row named), so its count is the
            // largest row plus one, and that has to be an int.
            if (rows(j, V) == std::numeric_limits<int>::max()) {
                throw std::invalid_argument(fmt::format(
                    "AssemblyDeclaration::{0}: application {1} of piece '{2}' names row {3} as "
                    "output {4}; a block's rows are 0 .. {5} (its row count must fit an int)",
                    call, V, name, rows(j, V), j, std::numeric_limits<int>::max() - 1));
            }
        }
    }

    std::vector<ConstraintFunction> &block =
        inequality ? inequality_constraints_ : equality_constraints_;
    block.emplace_back(piece, variables, rows);

    // The derived counts: the row space is 0 .. (largest row named), and the
    // overcount is how far the pieces' claimed rows exceed it. A block whose
    // pieces outgrow a count already on hand widens it; validate() then refuses
    // any row of the space no application names.
    int &declared = inequality ? inequality_rows_ : equality_rows_;
    int &overcount = inequality ? inequality_shared_row_overcount_ : equality_shared_row_overcount_;
    const char *which = inequality ? "inequality" : "equality";
    declared = std::max(declared, row_space_end(block));
    overcount = declared_rows(block, which) - declared;
    rows_derived_ = true;
}

AssemblyDeclaration &AssemblyDeclaration::set_bounds(Index index, double lower, double upper) {
    return set_bounds(index, index, lower, upper);
}

AssemblyDeclaration &AssemblyDeclaration::set_bounds(Index first, Index last, double lower,
                                                     double upper) {
    if (last < first) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::set_bounds: the range {0}..{1} is empty (last is "
                        "below first; the range is inclusive)",
                        first, last));
    }
    if (first < 0 || last >= primal_vars_) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::set_bounds: the range {0}..{1} is outside the "
                        "declaration, which has {2}",
                        first, last, variable_space(primal_vars_)));
    }
    require_bound_pair(first, lower, upper);
    for (Index index = first; index <= last; index++) {
        variable_bounds_.push_back(VariableBound{static_cast<int>(index), lower, upper});
    }
    return *this;
}

AssemblyDeclaration &AssemblyDeclaration::set_partitions(Index partitions) {
    if (partitions < 1 || partitions > std::numeric_limits<int>::max()) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration::set_partitions: {0} partitions is not a count the "
                        "layout can adopt (it must lie in [1, {1}])",
                        partitions, std::numeric_limits<int>::max()));
    }
    partition_count_ = static_cast<int>(partitions);
    return *this;
}

std::vector<VariableBound> AssemblyDeclaration::materialize_variable_bounds() const {
    constexpr double kInf = std::numeric_limits<double>::infinity();

    std::vector<VariableBound> materialized;
    materialized.reserve(static_cast<std::size_t>(std::max(primal_vars_, 0)));
    for (int index = 0; index < primal_vars_; ++index) {
        materialized.push_back(VariableBound{index, -kInf, kInf});
    }

    // Tightest wins, applied in declaration order, with the emptiness test
    // after each record: the same rule in the same order the engine's own bound
    // materializer applies, so the two boundaries cannot disagree about which
    // histories describe a problem.
    for (const VariableBound &record : variable_bounds_) {
        if (record.index_ < 0 || record.index_ >= primal_vars_) {
            throw std::invalid_argument(
                fmt::format("AssemblyDeclaration: a bound names variable {0}, but the "
                            "declaration has {1} primal variables",
                            record.index_, primal_vars_));
        }
        if (std::isnan(record.lower_) || std::isnan(record.upper_)) {
            throw std::invalid_argument(
                fmt::format("AssemblyDeclaration: the bound on variable {0} is not a number "
                            "(lower={1}, upper={2}); a bound that cannot be compared cannot "
                            "participate in the tightest-wins merge",
                            record.index_, record.lower_, record.upper_));
        }

        VariableBound &merged = materialized[static_cast<std::size_t>(record.index_)];
        merged.lower_ = std::max(merged.lower_, record.lower_);
        merged.upper_ = std::min(merged.upper_, record.upper_);

        if (merged.lower_ > merged.upper_) {
            throw std::invalid_argument(
                fmt::format("AssemblyDeclaration: the declared bounds on variable {0} intersect "
                            "to an empty interval (lower={1} is above upper={2})",
                            record.index_, merged.lower_, merged.upper_));
        }
    }

    return materialized;
}

void AssemblyDeclaration::validate() const {
    // A builder-derived row space has a piece behind every row. Checked first: a
    // row no piece names is what a negative derived overcount below would mean,
    // and this says which row.
    if (rows_derived_) {
        require_rows_covered(equality_constraints_, equality_rows_, "equality");
        require_rows_covered(inequality_constraints_, inequality_rows_, "inequality");
    }

    require_non_negative(primal_vars_, "the primal-variable count");
    require_non_negative(equality_rows_, "the equality-row count");
    require_non_negative(inequality_rows_, "the inequality-row count");
    require_non_negative(equality_shared_row_overcount_, "the equality shared-row overcount");
    require_non_negative(inequality_shared_row_overcount_, "the inequality shared-row overcount");

    // The fixing rows are a SUBSET of the declared equality rows: equality_rows_
    // is the row space as laid, and this says how many of those rows are the
    // internal ones a fixed-variable treatment appended. A count outside
    // [0, equality_rows_] describes no row space, and the subtraction the
    // adoption entry takes would leave a negative user count.
    if (fixing_rows_ < 0 || fixing_rows_ > equality_rows_) {
        throw std::invalid_argument(
            fmt::format("AssemblyDeclaration: {0} of the {1} declared equality rows are internal "
                        "fixing rows; that count must be at least 0 and no greater than the "
                        "equality-row count",
                        fixing_rows_, equality_rows_));
    }

    // The tail SHAPE, checked here rather than by whoever adopts this
    // declaration: a fixing row is one piece claiming one row, at the tail of
    // the equality list, which is what a fixed-variable treatment appends. A
    // consumer that lifts those pieces off before it lays can then split by
    // piece count alone, and a declaration that does not describe that shape is
    // refused at this boundary -- before anything has been moved anywhere.
    //
    // Per PIECE rather than over the tail sum: a tail of a zero-row piece
    // beside a two-row piece sums correctly and is still not the shape.
    if (fixing_rows_ > 0) {
        const std::size_t piece_count = equality_constraints_.size();
        if (piece_count > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            throw std::invalid_argument(
                fmt::format("AssemblyDeclaration: the equality list holds {0} pieces, past the "
                            "{1} a declaration can index",
                            piece_count, std::numeric_limits<int>::max()));
        }
        const int pieces = static_cast<int>(piece_count);
        if (pieces < fixing_rows_) {
            throw std::invalid_argument(fmt::format(
                "AssemblyDeclaration: {0} of the declared equality rows are internal fixing "
                "rows, but the equality list holds only {1} pieces; a fixing row is one piece "
                "claiming one row, at the tail of that list",
                fixing_rows_, pieces));
        }
        for (int k = pieces - fixing_rows_; k < pieces; k++) {
            const int claimed = equality_constraints_[static_cast<std::size_t>(k)].num_con_eles();
            if (claimed != 1) {
                throw std::invalid_argument(fmt::format(
                    "AssemblyDeclaration: equality piece {0} is one of the {1} declared internal "
                    "fixing rows but claims {2} rows; a fixing row is one piece claiming one row, "
                    "at the tail of that list",
                    k, fixing_rows_, claimed));
            }
        }
    }

    if (partition_count_ < 1) {
        throw std::invalid_argument(fmt::format(
            "AssemblyDeclaration: the partition count is {0}; it must be at least 1 (what a "
            "provider actually adopts is what negotiate_partition_count returns)",
            partition_count_));
    }

    // The piece-sum conjunct is the one conditional check here, and it is
    // conditional on the declaration having pieces at all. A provider that is
    // not a piece collection -- a bridge over a single model, say -- declares
    // none, and there is then no sum for the row counts to disagree with. Every
    // other check in this routine always runs. Coverage of a provider that
    // should have declared pieces and did not is owned downstream, by its own
    // claim pass.
    const bool has_pieces =
        !objectives_.empty() || !equality_constraints_.empty() || !inequality_constraints_.empty();
    if (has_pieces) {
        require_piece_sum(declared_rows(equality_constraints_, "equality"), equality_rows_,
                          equality_shared_row_overcount_, "equality");
        require_piece_sum(declared_rows(inequality_constraints_, "inequality"), inequality_rows_,
                          inequality_shared_row_overcount_, "inequality");
    } else {
        // An overcount is an excess over a piece sum, and there is no piece sum
        // here. Refused rather than ignored: a provider that is not a piece
        // collection has nothing to say about shared rows, so a non-zero value
        // is a declaration built for a different provider's shape, and
        // accepting it would leave the one conjunct that reads it switched off
        // with no diagnostic anywhere.
        require_no_overcount_without_pieces(equality_shared_row_overcount_, "equality");
        require_no_overcount_without_pieces(inequality_shared_row_overcount_, "inequality");
    }

    // Per RECORD: a single declaration whose two finite sides are inverted is
    // nonsense on its own terms, and saying so names the record the caller
    // wrote rather than an intersection several records away from it. An
    // inversion involving an infinity is caught by the emptiness test below,
    // where max/min put it.
    //
    // ONE CASE PASSES BOTH CHECKS DELIBERATELY: equal non-finite bounds
    // (lower == upper == +inf, or both -inf) are neither inverted nor empty, so
    // they reach the bound digest and hash as fixed with no finite side. The
    // engine's own bound materializer accepts them identically -- the two
    // boundaries agree, which is the property this validation exists to keep --
    // and the engine refuses them one step later, when a treatment is
    // configured over the materialized bounds ("equal but non-finite bounds; a
    // fixed variable needs a finite value", non_linear_program.cpp's
    // configure_variable_treatment).
    constexpr double kInf = std::numeric_limits<double>::infinity();
    for (const VariableBound &bound : variable_bounds_) {
        const bool lower_finite = bound.lower_ > -kInf && bound.lower_ < kInf;
        const bool upper_finite = bound.upper_ > -kInf && bound.upper_ < kInf;
        if (lower_finite && upper_finite && bound.lower_ > bound.upper_) {
            throw std::invalid_argument(
                fmt::format("AssemblyDeclaration: the bound declared on variable {0} is inverted "
                            "(lower={1} is above upper={2})",
                            bound.index_, bound.lower_, bound.upper_));
        }
    }

    // Index range, NaN, and the emptiness of each variable's intersected
    // interval. Materializing is what makes the last of those detectable here,
    // before any layout runs, rather than only when a layout is attempted.
    static_cast<void>(this->materialize_variable_bounds());
}

// --- THE DECLARATION-IDENTITY DIGEST (model/structure_identity.h) ---
//
// Defined here rather than in the otherwise header-only structure_identity.h:
// the digest is declaration-level logic, and this TU is where declaration-level
// logic lives with the piece definitions in scope.
//
// WHAT IS DELIBERATELY NOT FED, and why each exclusion is forced rather than
// chosen. The test is the same one every time: this digest must be the SAME
// VALUE for one declared problem on both of this project's engines and under
// every fixed-variable treatment, so anything a provider states about ITSELF
// rather than about the problem cannot be in it.
//
//   * `partition_count_`, and every piece's thread mode. Layout and threading
//     POLICY: they decide how the declared problem is laid out and evaluated,
//     never what problem it is, and they differ between the two engines on one
//     identical declaration. ModelStructureKey is where they belong.
//   * THE CLAIM STREAM. The same reason, measured: the two engines hand out
//     the same (row, column) claim SET in a different ORDER, and
//     claim_stream_digest is order-sensitive by design.
//   * THE PER-PIECE ROW STRUCTURE, and the two shared-row overcounts derived
//     from it -- not because a piece split says nothing, but because it is not
//     a cross-engine property of a declaration. The nlp_model_assembly.h
//     bridge leaves all three piece lists EMPTY (it is one serial piece of its
//     own), while the interior-point program's declaration carries one
//     type-erased handle per piece, so feeding the split would key the two
//     engines' readings of ONE declared problem differently on every problem
//     with a constraint in it. The overcounts go with it: they are
//     `sum(piece rows) - declared rows`, identically 0 on a declaration with
//     no pieces.
//   * BOUND VALUES. Only which sides are finite and whether they coincide
//     reach the bound conjunct (detail::feed_variable_bound).
//
// The cost: two declarations with the same dimensions and the same box key the
// same even if their constraint FUNCTIONS differ. The declaration surface
// carries no engine-neutral identity for a piece's mathematics, only
// type-erased handles, so this is the strongest engine-independent statement it
// supports today. A warm start applied to a differently-posed problem of
// identical shape costs a bad starting point rather than a wrong answer --
// every block is re-measured by the receiving solve's own first convergence
// test. If the bridge ever declares its pieces, the split can join this digest
// as a format-version event.

std::uint64_t declaration_identity_digest(const AssemblyDeclaration &declaration) {
    // THE FIXING TAIL HAS TO BE A LEGAL SPLIT before the user row count is
    // taken: this function is reachable on a declaration nobody validated. A
    // count that does not name a legal split is refused rather than clamped --
    // clamping would key two different problems the same, which is the one
    // thing a stamp may never do.
    const int fixing = declaration.fixing_rows_;
    if (fixing < 0 || fixing > declaration.equality_rows_) {
        throw std::invalid_argument(fmt::format(
            "declaration_identity_digest: the declaration states {0} internal fixing rows inside "
            "an equality row space of {1} -- a fixing-row count names rows a treatment appended "
            "to that space, so it must lie in [0, equality_rows_]",
            fixing, declaration.equality_rows_));
    }

    Fnv1a hash;
    // THE DECLARED DIMENSIONS, with the USER equality count. Fed through the
    // same self-delimiting preamble claim_stream_digest opens with, so the two
    // digests agree about what a dimension triple hashes to even though they
    // agree about nothing else.
    detail::feed_dimensions(hash, declaration.primal_vars_, declaration.equality_rows_ - fixing,
                            declaration.inequality_rows_);
    return hash.value();
}

DeclarationKey declaration_key(const AssemblyDeclaration &declaration) {
    // Each conjunct through its own public builder, and the bound one through
    // the SAME builder ModelStructureKey's bound conjunct uses: its input is
    // the declaration's own materialized bound records, which are declaration
    // data on both engines and under every treatment (a relaxing treatment
    // widens the BoundSet it hands the barrier, never the records the
    // declaration carries).
    return DeclarationKey{declaration_identity_digest(declaration),
                          materialized_bound_digest(declaration)};
}

} // namespace hven::solvers
