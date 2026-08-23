// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AggregateDeclaration's special members and its boundary validation.
//
// Out of line because the declaration holds its piece lists BY VALUE while the
// header only declares the piece handle types: defining the special members
// here, where the definitions are in scope, is what lets a consumer name,
// construct, inspect and validate a declaration without pulling the piece
// machinery in behind it.

#include "hven/model/aggregate_declaration.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include "hven/detail/interior/constraint_function.h"
#include "hven/detail/interior/objective_function.h"

namespace hven::solvers {

AggregateDeclaration::AggregateDeclaration() = default;
AggregateDeclaration::~AggregateDeclaration() = default;
AggregateDeclaration::AggregateDeclaration(const AggregateDeclaration &) = default;
AggregateDeclaration &AggregateDeclaration::operator=(const AggregateDeclaration &) = default;
AggregateDeclaration::AggregateDeclaration(AggregateDeclaration &&) noexcept = default;
AggregateDeclaration &AggregateDeclaration::operator=(AggregateDeclaration &&) noexcept = default;

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
            fmt::format("AggregateDeclaration: the {0} pieces claim {1} rows in total, past the "
                        "{2} a declaration can state",
                        which, rows, std::numeric_limits<int>::max()));
    }
    return static_cast<int>(rows);
}

void require_non_negative(int value, const char *what) {
    if (value < 0) {
        throw std::invalid_argument(
            fmt::format("AggregateDeclaration: {0} is {1}, which is not a count", what, value));
    }
}

} // namespace

std::vector<VariableBound> AggregateDeclaration::materialize_variable_bounds() const {
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
                fmt::format("AggregateDeclaration: a bound names variable {0}, but the "
                            "declaration has {1} primal variables",
                            record.index_, primal_vars_));
        }
        if (std::isnan(record.lower_) || std::isnan(record.upper_)) {
            throw std::invalid_argument(
                fmt::format("AggregateDeclaration: the bound on variable {0} is not a number "
                            "(lower={1}, upper={2}); a bound that cannot be compared cannot "
                            "participate in the tightest-wins merge",
                            record.index_, record.lower_, record.upper_));
        }

        VariableBound &merged = materialized[static_cast<std::size_t>(record.index_)];
        merged.lower_ = std::max(merged.lower_, record.lower_);
        merged.upper_ = std::min(merged.upper_, record.upper_);

        if (merged.lower_ > merged.upper_) {
            throw std::invalid_argument(
                fmt::format("AggregateDeclaration: the declared bounds on variable {0} intersect "
                            "to an empty interval (lower={1} is above upper={2})",
                            record.index_, merged.lower_, merged.upper_));
        }
    }

    return materialized;
}

void AggregateDeclaration::validate() const {
    require_non_negative(primal_vars_, "the primal-variable count");
    require_non_negative(equality_rows_, "the equality-row count");
    require_non_negative(inequality_rows_, "the inequality-row count");

    // The fixing rows are a SUBSET of the declared equality rows: equality_rows_
    // is the row space as laid, and this says how many of those rows are the
    // internal ones a fixed-variable treatment appended. A count outside
    // [0, equality_rows_] describes no row space, and the subtraction the
    // adoption entry takes would leave a negative user count.
    if (fixing_rows_ < 0 || fixing_rows_ > equality_rows_) {
        throw std::invalid_argument(
            fmt::format("AggregateDeclaration: {0} of the {1} declared equality rows are internal "
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
                fmt::format("AggregateDeclaration: the equality list holds {0} pieces, past the "
                            "{1} a declaration can index",
                            piece_count, std::numeric_limits<int>::max()));
        }
        const int pieces = static_cast<int>(piece_count);
        if (pieces < fixing_rows_) {
            throw std::invalid_argument(fmt::format(
                "AggregateDeclaration: {0} of the declared equality rows are internal fixing "
                "rows, but the equality list holds only {1} pieces; a fixing row is one piece "
                "claiming one row, at the tail of that list",
                fixing_rows_, pieces));
        }
        for (int k = pieces - fixing_rows_; k < pieces; k++) {
            const int claimed = equality_constraints_[static_cast<std::size_t>(k)].num_con_eles();
            if (claimed != 1) {
                throw std::invalid_argument(fmt::format(
                    "AggregateDeclaration: equality piece {0} is one of the {1} declared internal "
                    "fixing rows but claims {2} rows; a fixing row is one piece claiming one row, "
                    "at the tail of that list",
                    k, fixing_rows_, claimed));
            }
        }
    }

    if (partition_count_ < 1) {
        throw std::invalid_argument(fmt::format(
            "AggregateDeclaration: the partition count is {0}; it must be at least 1 (what a "
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
        const int equality_claimed = declared_rows(equality_constraints_, "equality");
        if (equality_claimed != equality_rows_) {
            throw std::invalid_argument(fmt::format(
                "AggregateDeclaration: the equality pieces claim {0} rows in total, but the "
                "declaration states {1} equality rows",
                equality_claimed, equality_rows_));
        }

        const int inequality_claimed = declared_rows(inequality_constraints_, "inequality");
        if (inequality_claimed != inequality_rows_) {
            throw std::invalid_argument(fmt::format(
                "AggregateDeclaration: the inequality pieces claim {0} rows in total, but the "
                "declaration states {1} inequality rows",
                inequality_claimed, inequality_rows_));
        }
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
                fmt::format("AggregateDeclaration: the bound declared on variable {0} is inverted "
                            "(lower={1} is above upper={2})",
                            bound.index_, bound.lower_, bound.upper_));
        }
    }

    // Index range, NaN, and the emptiness of each variable's intersected
    // interval. Materializing is what makes the last of those detectable here,
    // before any layout runs, rather than only when a layout is attempted.
    static_cast<void>(this->materialize_variable_bounds());
}

} // namespace hven::solvers
