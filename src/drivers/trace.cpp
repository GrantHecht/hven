// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// R6 (Codex 5, CLAUDE.md section 5): the sink's non-hot virtual destructor
// lives here, not inline in the header.

#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/drivers/trace.h>

namespace hven::solvers {

TraceSink::~TraceSink() = default;

// The W4 events' defaults are EMPTY, not pure (plan section 6 Q-S3): a sink
// written for the eight W1/W2 events keeps compiling and simply ignores them.
void TraceSink::on_sqp_major(const SqpMajorTraceEvent &) {}
void TraceSink::on_sqp_solve_begin(const SqpSolveBeginTraceEvent &) {}
void TraceSink::on_sqp_solve_end(const SqpSolveEndTraceEvent &) {}
void TraceSink::on_ipm_iter(const IpmIterTraceEvent &) {}
void TraceSink::on_ipm_solve_begin(const IpmSolveBeginTraceEvent &) {}
void TraceSink::on_ipm_solve_end(const IpmSolveEndTraceEvent &) {}
// M6 W5 T8.7's own event, on the same footing.
void TraceSink::on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &) {}
// M6 W5 T8.7b's five, on the same footing again: the interior-point engine's
// last direct prints are events now, and a sink written before them is not
// touched by their arrival.
void TraceSink::on_ipm_phase_begin(const IpmPhaseTraceEvent &) {}
void TraceSink::on_ipm_phase_end(const IpmPhaseTraceEvent &) {}
void TraceSink::on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &) {}
void TraceSink::on_ipm_phase_exit(const IpmPhaseExitTraceEvent &) {}
void TraceSink::on_ipm_message(const IpmMessageTraceEvent &) {}

// THE ONE COPY (M6 W4 T4). It lived in `sqp_driver.cpp`'s anonymous namespace
// through T2/T3 and moved here unchanged the moment a second caller appeared;
// the arithmetic, and therefore `sqp.solve.begin`'s golden line, is untouched.
VariableBoundCensus census_variable_bounds(const Vec &lower, const Vec &upper, Index n) {
    if (n < 0) {
        throw std::invalid_argument(
            fmt::format("census_variable_bounds: n is {}, which is negative", n));
    }
    if (lower.size() != n && lower.size() != 0) {
        throw std::invalid_argument(fmt::format(
            "census_variable_bounds: lower has size {}, expected {} or 0", lower.size(), n));
    }
    if (upper.size() != n && upper.size() != 0) {
        throw std::invalid_argument(fmt::format(
            "census_variable_bounds: upper has size {}, expected {} or 0", upper.size(), n));
    }
    VariableBoundCensus census;
    for (Index j = 0; j < n; ++j) {
        // An empty vector is "unbounded on this side for every variable" -- the
        // interior-point NLP materializes its box only once a bound is declared.
        const bool has_lo = lower.size() != 0 && std::isfinite(lower(j));
        const bool has_hi = upper.size() != 0 && std::isfinite(upper(j));
        if (!has_lo && !has_hi) {
            ++census.vars_free;
        } else if (has_lo && !has_hi) {
            ++census.vars_lower_only;
        } else if (!has_lo && has_hi) {
            ++census.vars_upper_only;
        } else if (lower(j) == upper(j)) {
            ++census.vars_fixed;
        } else {
            ++census.vars_ranged;
        }
    }
    return census;
}

} // namespace hven::solvers
