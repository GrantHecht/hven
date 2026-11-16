// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// console_trace_sink.h -- the console sink and the fan-out that composes it.
//
// `ConsoleTraceSink` writes the human trace (docs/trace-schema-v1.md, "Human trace") from
// the same events the machine trace is written from, so the console is one consumer of
// the stream among others. `FanOutTraceSink` makes "the solver attaches a console" not
// mean "the solver replaces your sink".
//
// This header includes `trace.h` and `<cstdio>` only; the install smoke compiles it alone.
//
// The verbosity rungs (`CommonOptions::verbosity`), each printing the rungs below it:
//   kWarnings    warning messages; the footer's `exit` line
//   kSummary     header; IPM phase lines (multi-phase solves); the rest of the footer
//   kIterations  legend; column headers; every iteration row, restoration rows included;
//                notice messages
//   kDetail      one condensed line per QP-tier event; the IPM KKT-analysis and
//                phase-exit lines
// Rows and blocks carry no colour; `Format::color` colours the warning mark and the exit line.

#include <cstdio>
#include <string_view>

#include <hven/drivers/trace.h>

namespace hven {

/// @brief When the console colours the warning mark and the `exit` line.
enum class ConsoleColor {
    kAuto,   ///< Only when the output is a terminal and NO_COLOR is unset.
    kAlways, ///< Always (ANSI escapes).
    kNever,  ///< Never.
};

/// @brief Renders the human trace from the trace stream.
class ConsoleTraceSink final : public TraceSink {
  public:
    struct Format {
        Verbosity verbosity = Verbosity::kIterations;
        ConsoleColor color = ConsoleColor::kAuto;
        bool legend = true; ///< Print the legend once per solve (kIterations and up).
        bool timing = true; ///< Print the footer's wall-clock line (informational).
    };

    explicit ConsoleTraceSink(Format f, std::FILE *out = stdout);
    ~ConsoleTraceSink() override;

    ConsoleTraceSink(const ConsoleTraceSink &) = delete;
    ConsoleTraceSink &operator=(const ConsoleTraceSink &) = delete;
    ConsoleTraceSink(ConsoleTraceSink &&) = delete;
    ConsoleTraceSink &operator=(ConsoleTraceSink &&) = delete;

    const Format &format() const { return fmt_; }

    void on_ipqp_iter(const IpqpIterTraceEvent &event) override;
    void on_ipqp_reg(const IpqpRegTraceEvent &event) override;
    void on_ipqp_restart(const IpqpRestartTraceEvent &event) override;
    void on_ipqp_route(const IpqpRouteTraceEvent &event) override;
    void on_ipqp_certify(const IpqpCertifyTraceEvent &event) override;
    void on_ipqp_escape(const IpqpEscapeTraceEvent &event) override;
    void on_qp_mode(const QpModeTraceEvent &event) override;
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override;

    void on_solve_begin(const SolveBeginTraceEvent &event) override;
    void on_iter(const IterTraceEvent &event) override;
    void on_message(const MessageTraceEvent &event) override;
    void on_solve_end(const SolveEndTraceEvent &event) override;

    void on_ipm_phase_begin(const IpmPhaseTraceEvent &event) override;
    void on_ipm_phase_end(const IpmPhaseTraceEvent &event) override;
    void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) override;
    void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) override;

    /// Restoration sub-solve nesting, from the begin/end pairs seen.
    Index sqp_depth() const { return depth_; }

  private:
    bool at_least(Verbosity v) const;
    void print_line(std::string_view text);
    void print_header_line(TraceEngine engine);

    Format fmt_;
    std::FILE *out_;
    bool color_ = false;

    Index depth_ = 0;
    Index open_solves_ = 0;
    TraceEngine engine_ = TraceEngine::kIpm;
    Index ipm_phases_ = 0;
    bool legend_done_ = false;
    bool header_pending_ = true; ///< The column header precedes the next row.
    Index rows_since_header_ = 0;
};

/// @brief Forwards every event to two sinks, first then second.
class FanOutTraceSink final : public TraceSink {
  public:
    FanOutTraceSink(TraceSink *first, TraceSink *second);
    ~FanOutTraceSink() override;

    FanOutTraceSink(const FanOutTraceSink &) = delete;
    FanOutTraceSink &operator=(const FanOutTraceSink &) = delete;
    FanOutTraceSink(FanOutTraceSink &&) = delete;
    FanOutTraceSink &operator=(FanOutTraceSink &&) = delete;

    TraceSink *first() const { return first_; }
    TraceSink *second() const { return second_; }

    void on_ipqp_iter(const IpqpIterTraceEvent &event) override;
    void on_ipqp_reg(const IpqpRegTraceEvent &event) override;
    void on_ipqp_restart(const IpqpRestartTraceEvent &event) override;
    void on_ipqp_route(const IpqpRouteTraceEvent &event) override;
    void on_ipqp_certify(const IpqpCertifyTraceEvent &event) override;
    void on_ipqp_escape(const IpqpEscapeTraceEvent &event) override;
    void on_qp_mode(const QpModeTraceEvent &event) override;
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override;
    void on_solve_begin(const SolveBeginTraceEvent &event) override;
    void on_iter(const IterTraceEvent &event) override;
    void on_message(const MessageTraceEvent &event) override;
    void on_solve_end(const SolveEndTraceEvent &event) override;
    void on_ipm_phase_begin(const IpmPhaseTraceEvent &event) override;
    void on_ipm_phase_end(const IpmPhaseTraceEvent &event) override;
    void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) override;
    void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) override;

  private:
    TraceSink *first_ = nullptr;
    TraceSink *second_ = nullptr;
};

} // namespace hven
