// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// console_trace_sink.h -- the console sink and the fan-out that composes it.
//
// `ConsoleTraceSink` writes both engines' human-readable console tables from the
// events the engines emit, so the console is one consumer of the trace among
// others rather than a second, private reporting path a caller could not see,
// redirect or reproduce. Neither engine prints from inside its solve path: a
// solve's whole transcript is written here, by this sink alone, on every solve
// including the process's first.
//
// `FanOutTraceSink` is what makes "the solver attaches a console" not mean "the
// solver replaces your sink": when printing is on, the effective sink for the
// solve is a fan-out over the caller's sink and the console, and the caller's
// stream is byte-identical to what it would have been with no console at all.
//
// This header includes `trace.h` and `<cstdio>`, and nothing else. `IpmIterate`
// -- the interior-point iteration record the row renderer reads -- arrives
// through `trace.h`, which refers to it by reference. The install smoke compiles
// this header alone to prove it is self-contained.
//
// The Verbosity rungs, applied per line kind (`CommonOptions::verbosity`):
//
//   kIterations  the iteration rows, and the interior-point Problem Statistics block
//   kSummary+    headers, the Beginning/Finished lines, the timing summary, the SQP
//          table's header rule and its `Start Level` / `Scaling` trailer, the
//          per-phase Beginning/Finished lines, the KKT-analysis block, the
//          per-phase exit block and the `solver_initialized` notice
//   kWarnings+   the SQP table's `Status` line, the per-phase exit verdict line and
//          the eight interior-point warnings
//
// Colour: the interior-point renderings use fmt's styled `FILE *` overloads,
// and fmt emits its SGR escapes unconditionally (it does not test `isatty` on
// those overloads), which is what makes the byte-pin in
// `tests/drivers/test_console_sink.cpp` reproducible. The SQP table is
// uncoloured, exactly as `format_iteration_table` renders it.

#include <cstdio>
#include <string_view>

#include <hven/drivers/trace.h>

namespace hven {

/// @brief Renders both engines' console tables from the trace stream.
///
/// One sink, two tables. The SQP side reproduces `format_iteration_table` byte
/// for byte, through the very functions that renderer is built from
/// (`src/drivers/sqp_print.cpp`), so the two cannot drift.
///
/// STATE, AND WHEN IT RESETS. The interior-point row colouring compares six
/// columns against the PREVIOUS row, which the old printer read out of
/// `alg_impl`'s own per-phase `iters` vector. That vector starts empty in every
/// phase, so every phase's first row coloured as a first row. The engine emits
/// ONE `ipm.solve.begin`/`end` pair per CALL, not per phase, so this sink drops
/// its previous-row state on TWO events: `on_ipm_solve_begin`, and any
/// `ipm.iter` that opens a new phase (`phase` differs from the previous row's,
/// or `iter_ == 0`). Without the second, a `{kSolve, kOptimize}` solve would
/// colour phase 1's first row against phase 0's last.
///
/// NESTING. The SQP side tracks `depth` exactly as `JsonLinesTraceSink` does
/// (a nested solve's own pair pushes and pops) and renders ONLY depth-0 majors:
/// `format_iteration_table` renders `SqpResult::history`, which holds
/// top-level rows only, so rendering a nested row would make this sink differ
/// from the function it is pinned against. The interior-point pair moves no
/// depth, as `trace.h` says.
///
/// NOT OWNED, NOT CLOSED: the `FILE *` is borrowed and must outlive the sink.
/// Every rendered event is followed by an `fflush`, so output interleaves with
/// the engine's own remaining direct prints in the order it was produced.
///
/// THE EIGHT QP-TIER EVENTS RENDER NOTHING. `on_ipqp_*`, `on_qp_mode` and
/// `on_fallback_verdict` are no-op overrides: there has never been a console
/// convention for them and the SQP table never showed them.
class ConsoleTraceSink final : public TraceSink {
  public:
    /// @brief The sink's layout configuration, fixed for its lifetime.
    struct Format {
        /// The interior-point table's WIDE layout (159 columns against 119).
        /// A DEFAULT, not the last word: `on_ipm_solve_begin` adopts the begin
        /// event's own `wide_console`, so a solve's layout is the solve's.
        bool wide = false;
        /// The rung: `kIterations` (the default here) prints everything, as the
        /// tiers above describe; `kSilent` prints nothing.
        Verbosity verbosity = Verbosity::kIterations;
    };

    /// @brief Constructs a console sink.
    /// @param f   Layout configuration.
    /// @param out Borrowed output stream; must outlive this sink.
    explicit ConsoleTraceSink(Format f, std::FILE *out = stdout);
    ~ConsoleTraceSink() override;

    ConsoleTraceSink(const ConsoleTraceSink &) = delete;
    ConsoleTraceSink &operator=(const ConsoleTraceSink &) = delete;
    ConsoleTraceSink(ConsoleTraceSink &&) = delete;
    ConsoleTraceSink &operator=(ConsoleTraceSink &&) = delete;

    /// @brief The configuration this sink was built with (`wide` as supplied,
    ///        not as a begin event may have overridden it).
    const Format &format() const { return fmt_; }

    // --- The QP-tier events: nothing is rendered for any of them. ---
    void on_ipqp_iter(const IpqpTraceIterEvent &event) override;
    void on_ipqp_reg(const IpqpTraceRegEvent &event) override;
    void on_ipqp_restart(const IpqpTraceRestartEvent &event) override;
    void on_ipqp_route(const IpqpTraceRouteEvent &event) override;
    void on_ipqp_certify(const IpqpTraceCertifyEvent &event) override;
    void on_ipqp_escape(const IpqpTraceEscapeEvent &event) override;
    void on_qp_mode(const QpModeTraceEvent &event) override;
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override;

    // --- The SQP table ---
    void on_sqp_major(const SqpMajorTraceEvent &event) override;
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &event) override;
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &event) override;

    // --- The interior-point table ---
    void on_ipm_iter(const IpmIterTraceEvent &event) override;
    void on_ipm_solve_begin(const IpmSolveBeginTraceEvent &event) override;
    void on_ipm_solve_end(const IpmSolveEndTraceEvent &event) override;
    /// RENDERS NOTHING, and that is not an omission. The row this event marks
    /// is on the stream as an ordinary `ipm.iter` line, and this sink renders
    /// one row per `ipm.iter`. Rendering here too would print that row twice.
    void on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &event) override;

    // --- The interior-point phase, analysis and message events ---
    //
    // The tier each renders at is the `print_level` guard the SOLVER used to
    // apply at the site: the phase lines, the analysis block and the exit
    // BLOCK at `< 2`; the exit VERDICT line and every message at `< 3`.
    // `solver_initialized` keeps its second condition too -- the notice is
    // printed only when initialization took more than half a millisecond,
    // while the EVENT is emitted whenever initialization ran at all.
    void on_ipm_phase_begin(const IpmPhaseTraceEvent &event) override;
    void on_ipm_phase_end(const IpmPhaseTraceEvent &event) override;
    void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) override;
    void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) override;
    void on_ipm_message(const IpmMessageTraceEvent &event) override;

    /// @brief The SQP nesting depth this sink currently sits at -- 0 for a
    ///        top-level solve, as `JsonLinesTraceSink::depth()` reads it.
    Index sqp_depth() const { return sqp_depth_; }

  private:
    void ipm_print_header();
    void ipm_print_stats(const IpmSolveBeginTraceEvent &event);
    void ipm_print_beginning(std::string_view msg);
    void ipm_print_finished(std::string_view msg);
    void ipm_print_row(const IpmIterate &row);

    Format fmt_;
    std::FILE *out_;

    // --- Interior-point per-solve state, all of it set by the begin event ---
    bool wide_ = false;
    double kkt_tol_ = 0.0;
    double bar_tol_ = 0.0;
    double econ_tol_ = 0.0;
    double icon_tol_ = 0.0;
    double acc_kkt_tol_ = 0.0;
    double acc_bar_tol_ = 0.0;
    double acc_econ_tol_ = 0.0;
    double acc_icon_tol_ = 0.0;

    // --- Interior-point previous-row state (the six coloured columns) ---
    bool have_prev_row_ = false;
    Index prev_phase_ = 0;
    double prev_prim_obj_ = 0.0;
    double prev_barr_obj_ = 0.0;
    double prev_barr_inf_ = 0.0;
    double prev_econ_inf_ = 0.0;
    double prev_icon_inf_ = 0.0;
    double prev_kkt_inf_ = 0.0;

    // --- SQP nesting, on JsonLinesTraceSink's own two counters ---
    Index sqp_depth_ = 0;
    Index sqp_open_solves_ = 0;
};

/// @brief Forwards every event to two sinks, first then second.
///
/// THE COMPOSITION BEHIND "attaching the console never displaces a user sink".
/// A driver that is printing builds one of these over the caller's sink and its
/// own console and hands THAT to every emit site, its nested sub-solve
/// included -- so the caller's stream is byte-identical with the console on and
/// off, nesting depth included. Twenty events.
///
/// EITHER HALF MAY BE NULL and is then skipped; a fan-out over two nulls is a
/// legal, silent sink. Neither is owned.
///
/// A THROWING FIRST SINK means the second never sees that event: the exception
/// propagates out of the emit site and out of the solve, which is the solve's
/// own throw contract. The console is second in the driver's composition, so a
/// caller's throwing sink loses console output for that one event -- and the
/// solve is ending anyway.
class FanOutTraceSink final : public TraceSink {
  public:
    /// @brief Constructs a fan-out. Neither sink is owned; both must outlive it.
    FanOutTraceSink(TraceSink *first, TraceSink *second);
    ~FanOutTraceSink() override;

    FanOutTraceSink(const FanOutTraceSink &) = delete;
    FanOutTraceSink &operator=(const FanOutTraceSink &) = delete;
    FanOutTraceSink(FanOutTraceSink &&) = delete;
    FanOutTraceSink &operator=(FanOutTraceSink &&) = delete;

    /// @brief The sink this fan-out forwards to FIRST.
    TraceSink *first() const { return first_; }
    /// @brief The sink this fan-out forwards to SECOND.
    TraceSink *second() const { return second_; }

    void on_ipqp_iter(const IpqpTraceIterEvent &event) override;
    void on_ipqp_reg(const IpqpTraceRegEvent &event) override;
    void on_ipqp_restart(const IpqpTraceRestartEvent &event) override;
    void on_ipqp_route(const IpqpTraceRouteEvent &event) override;
    void on_ipqp_certify(const IpqpTraceCertifyEvent &event) override;
    void on_ipqp_escape(const IpqpTraceEscapeEvent &event) override;
    void on_qp_mode(const QpModeTraceEvent &event) override;
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) override;
    void on_sqp_major(const SqpMajorTraceEvent &event) override;
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &event) override;
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &event) override;
    void on_ipm_iter(const IpmIterTraceEvent &event) override;
    void on_ipm_solve_begin(const IpmSolveBeginTraceEvent &event) override;
    void on_ipm_solve_end(const IpmSolveEndTraceEvent &event) override;
    void on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &event) override;
    void on_ipm_phase_begin(const IpmPhaseTraceEvent &event) override;
    void on_ipm_phase_end(const IpmPhaseTraceEvent &event) override;
    void on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) override;
    void on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) override;
    void on_ipm_message(const IpmMessageTraceEvent &event) override;

  private:
    TraceSink *first_ = nullptr;
    TraceSink *second_ = nullptr;
};

} // namespace hven
