// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The sink's non-hot virtual destructor and its no-op event defaults live here,
// not inline in the header.

#include <cmath>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/drivers/trace.h>

namespace hven {

TraceSink::~TraceSink() = default;

// Every event's default is EMPTY: a sink overrides the events it wants, and an
// event added later is ignored by a sink that predates it.
void TraceSink::on_ipqp_iter(const IpqpIterTraceEvent &) {}
void TraceSink::on_ipqp_reg(const IpqpRegTraceEvent &) {}
void TraceSink::on_ipqp_restart(const IpqpRestartTraceEvent &) {}
void TraceSink::on_ipqp_route(const IpqpRouteTraceEvent &) {}
void TraceSink::on_ipqp_certify(const IpqpCertifyTraceEvent &) {}
void TraceSink::on_ipqp_escape(const IpqpEscapeTraceEvent &) {}
void TraceSink::on_qp_mode(const QpModeTraceEvent &) {}
void TraceSink::on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) {}
void TraceSink::on_solve_begin(const SolveBeginTraceEvent &) {}
void TraceSink::on_iter(const IterTraceEvent &) {}
void TraceSink::on_message(const MessageTraceEvent &) {}
void TraceSink::on_solve_end(const SolveEndTraceEvent &) {}
void TraceSink::on_solve_abort() noexcept {}
void TraceSink::on_ipm_phase_begin(const IpmPhaseTraceEvent &) {}
void TraceSink::on_ipm_phase_end(const IpmPhaseTraceEvent &) {}
void TraceSink::on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &) {}
void TraceSink::on_ipm_phase_exit(const IpmPhaseExitTraceEvent &) {}

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

} // namespace hven
