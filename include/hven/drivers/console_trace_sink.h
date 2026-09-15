// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// console_trace_sink.h -- the two composition sinks M6 W5 T8.7 adds.
//
// `ConsoleTraceSink` is where BOTH engines' human-readable console tables now
// live. Until this task the interior-point engine printed its table from inside
// its own iteration loop with `fmt::print` calls guarded by
// `common.print_level`, and the SQP engine printed nothing at all
// (`format_iteration_table` was called only by tests). Both are now written by
// this sink, from the events the engines already emit -- so the console is one
// consumer of the trace among others rather than a second, private reporting
// path that a caller could not see, redirect or reproduce.
//
// SINCE M6 W5 T8.7b THAT IS LITERALLY TRUE OF THE INTERIOR-POINT SIDE: the
// engine's last direct prints -- the per-phase Beginning/Finished lines, the
// KKT-analysis block, the per-phase exit statistics and the nine messages --
// are events, `src/drivers/interior_point_solver_print.cpp` is gone, and there
// is no `fmt::print` left anywhere in that engine's solve path. A solve's whole
// transcript is written HERE, by this sink alone, on every solve including the
// process's first.
//
// `FanOutTraceSink` is what makes "the solver attaches a console" not mean "the
// solver replaces your sink": when printing is on, the effective sink for the
// solve is a fan-out over the caller's sink and the console, and the caller's
// stream is byte-identical to what it would have been with no console at all.
//
// WHAT THIS HEADER NEEDS AND WHAT IT DOES NOT. It includes `trace.h`,
// `<fmt/color.h>` (for the one colour function it declares) and `<cstdio>`, and
// nothing else. `IterateInfo` -- the interior-point iteration record the row
// renderer reads -- arrives through `trace.h`, which refers to it by reference;
// a consumer including only this header never needs the `detail/` header
// itself. The install smoke compiles this header alone to prove that.
//
// PRINT LEVELS ARE THE INTERIOR-POINT ENGINE'S OWN THREE TIERS, unchanged and
// applied PER LINE KIND (`CommonOptions::print_level`; 0 prints everything,
// 3+ is silent):
//
//   == 0   the iteration rows, and the interior-point Problem Statistics block
//   <  2   headers, the Beginning/Finished lines, the timing summary, the
//          SQP table's header rule and its `Start Level` / `Scaling` trailer,
//          and (M6 W5 T8.7b) the per-phase Beginning/Finished lines, the
//          KKT-analysis block, the per-phase exit BLOCK and the
//          `solver_initialized` notice
//   <  3   the SQP table's `Status` line, and (M6 W5 T8.7b) the per-phase exit
//          VERDICT line and the eight interior-point warnings
//
// COLOUR. The interior-point renderings use fmt's styled `FILE *` overloads
// with the same `fmt::text_style` objects the old printer used, and fmt emits
// its SGR escapes UNCONDITIONALLY (it does not test `isatty` on those
// overloads). That is what makes the byte-pin in
// `tests/drivers/test_console_sink.cpp` reproducible: the escapes are fmt's own
// and move only when `dep/fmt` moves. The SQP table is uncoloured, exactly as
// `format_iteration_table` renders it.

#include <cstdio>
#include <string_view>

#include <fmt/color.h>

#include <hven/drivers/trace.h>

namespace hven::solvers {

/// @brief The interior-point iteration table's five-band residual colouring.
///
/// THE ONE COPY. It was `IpmSolver::calculate_color`, a private
/// static, until M6 W5 T8.7 moved the table out of the solver and into
/// `ConsoleTraceSink`. Through T8.7 the engine's own `print_exit_stats` called
/// it here so the two renderings could not drift apart; T8.7b moved that block
/// to `on_ipm_phase_exit` and deleted the function, so this sink is now the
/// only caller. The arithmetic is unchanged.
///
/// The bands are log-spaced between the CONVERGENCE tolerance and 1000x the
/// ACCEPTABLE one: below target is lime green, then yellow, orange, red, and
/// dark red beyond. A non-positive argument is floored at 1e-300 rather than
/// taking a logarithm of zero.
///
/// @param value      The residual to colour.
/// @param target     That column's convergence tolerance.
/// @param acceptable That column's acceptable-level tolerance.
/// @return An fmt foreground style.
fmt::text_style ipm_residual_color(double value, double target, double acceptable);

/// @brief Renders both engines' console tables from the trace stream.
///
/// ONE SINK, TWO TABLES. The interior-point side reproduces the output of
/// `interior_point_solver_print.cpp` -- the file T8.7b deleted once the last of
/// it had moved here -- byte for byte; the SQP side
/// reproduces `format_iteration_table` byte for byte, through the very
/// functions that renderer is built from (`src/drivers/sqp_print.cpp`), so the
/// two cannot drift.
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
        /// `CommonOptions::print_level` -- the three tiers above.
        int print_level = 0;
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
    /// is on the stream as an ordinary `ipm.iter` line (M6 W5 T8.6 fix1 gave
    /// the restoration-locally-infeasible door its own), and this sink renders
    /// one row per `ipm.iter`. Rendering here too would print that row twice.
    void on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &event) override;

    // --- The interior-point engine's last direct prints (M6 W5 T8.7b) ---
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
    void ipm_print_row(const IterateInfo &row);

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
/// off, nesting depth included. TWENTY events since M6 W5 T8.7b.
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

} // namespace hven::solvers
