// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// AggregateDeclaration's special members and its boundary validation.
//
// Out of line for one reason: the declaration holds its piece lists BY VALUE,
// and the header only declares the piece handle types. Defining the special
// members here -- where the definitions are in scope -- is what lets a
// consumer name, construct, inspect and validate a declaration without pulling
// the piece machinery in behind it.

#include "hven/model/aggregate_declaration.h"

#include <algorithm>
#include <cmath>
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
int declared_rows(const std::vector<ConstraintFunction> &pieces) {
    int rows = 0;
    for (const auto &piece : pieces) {
        rows += piece.num_con_eles();
    }
    return rows;
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
    // other check in this routine always runs, which is what makes this boundary
    // universal rather than one engine's description of itself. Coverage of a
    // provider that should have declared pieces and did not is owned downstream,
    // by its own claim pass.
    const bool has_pieces = !objectives_.empty() || !equality_constraints_.empty() ||
                            !inequality_constraints_.empty();
    if (has_pieces) {
        const int equality_claimed = declared_rows(equality_constraints_);
        if (equality_claimed != equality_rows_) {
            throw std::invalid_argument(fmt::format(
                "AggregateDeclaration: the equality pieces claim {0} rows in total, but the "
                "declaration states {1} equality rows",
                equality_claimed, equality_rows_));
        }

        const int inequality_claimed = declared_rows(inequality_constraints_);
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
    // ONE CASE PASSES BOTH CHECKS, and it is recorded here rather than closed:
    // equal non-finite bounds (lower == upper == +inf, or both -inf) are not
    // inverted and do not intersect to nothing, so they reach the bound digest
    // and hash as fixed with no finite side. The engine's own bound
    // materializer accepts them identically -- the two boundaries agree, which
    // is the property this validation exists to keep -- and the engine refuses
    // them one step later, when a treatment is configured over the materialized
    // bounds ("equal but non-finite bounds; a fixed variable needs a finite
    // value", non_linear_program.cpp's configure_variable_treatment). Refusing
    // them here instead would put this boundary ahead of the materializer it
    // mirrors without closing the digest case anyway, since the digest is taken
    // over the materialization rather than over this check.
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
