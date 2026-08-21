// =============================================================================
// New file in hven (Copyright 2026-present Grant R. Hecht, Apache 2.0 — see
//   LICENSE.txt)
// =============================================================================

// AggregateDeclaration's special members and its boundary validation.
//
// Out of line for one reason: the declaration holds its piece lists BY VALUE,
// and the header only declares the piece handle types. Defining the special
// members here -- where the definitions are in scope -- is what lets a
// consumer name, construct, inspect and validate a declaration without pulling
// the piece machinery in behind it.

#include "hven/model/aggregate_declaration.h"

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

    const int equality_claimed = declared_rows(equality_constraints_);
    if (equality_claimed != equality_rows_) {
        throw std::invalid_argument(
            fmt::format("AggregateDeclaration: the equality pieces claim {0} rows in total, but "
                        "the declaration states {1} equality rows",
                        equality_claimed, equality_rows_));
    }

    const int inequality_claimed = declared_rows(inequality_constraints_);
    if (inequality_claimed != inequality_rows_) {
        throw std::invalid_argument(
            fmt::format("AggregateDeclaration: the inequality pieces claim {0} rows in total, but "
                        "the declaration states {1} inequality rows",
                        inequality_claimed, inequality_rows_));
    }

    for (const VariableBound &bound : variable_bounds_) {
        if (bound.index_ < 0 || bound.index_ >= primal_vars_) {
            throw std::invalid_argument(
                fmt::format("AggregateDeclaration: a bound names variable {0}, but the "
                            "declaration has {1} primal variables",
                            bound.index_, primal_vars_));
        }
        if (std::isnan(bound.lower_) || std::isnan(bound.upper_)) {
            throw std::invalid_argument(
                fmt::format("AggregateDeclaration: the bound on variable {0} is not a number "
                            "(lower={1}, upper={2}); a bound that cannot be compared cannot "
                            "participate in the tightest-wins merge",
                            bound.index_, bound.lower_, bound.upper_));
        }
    }
}

} // namespace hven::solvers
