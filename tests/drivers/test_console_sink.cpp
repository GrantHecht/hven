// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// The console sink's pins (M6 W5 T8.7).
//
// WHAT THIS FILE PROVES, and what it deliberately leaves to a leg:
//
//   * THE SCRIPTED BYTE-PIN. Twelve constructed `IterateInfo` records, in two
//     phases of six, fed to `ConsoleTraceSink` and compared BYTE FOR BYTE
//     against a literal captured from the OLD `print_last_iterate` at BASE
//     (c0e3c6c) on the SAME records. Wide and narrow. That is the identity
//     proof for the table this task moved out of the solver.
//   * THE PHASE RESET, both ways: a phase change WITHOUT a second begin (the
//     shape a `{kSolve, kOptimize}` solve actually produces, since the engine
//     emits one begin/end pair per CALL), and two begin/end pairs.
//   * THE SQP TABLE against `format_iteration_table` on a constructed
//     solution, the depth rule, and the print-level tiers.
//   * THE FAN-OUT: ordering, null halves, and that a user sink is not
//     displaced.
//
// WHAT IS A LEG, NOT A TEST: the begin and end BLOCKS' content (the banner,
// the problem statistics, the timing summary). Their inputs are a real
// factorization's size and fill and a real solve's wall clock, so a scripted
// literal for them would pin invented numbers. They are pinned instead by the
// LIVE transcripts -- HS071 at `print_level 0`, cold and `{kSolve, kOptimize}`,
// wide and narrow, captured at BASE and at HEAD and compared with the timing
// lines masked -- recorded in `.superpowers/w5-t8-7-psym-transcripts.md`.
//
// THE PIN'S PROVENANCE, in one place. fmt emits its SGR escapes
// UNCONDITIONALLY on the styled overloads (it does not test `isatty`), so the
// bytes below are fmt's own and are stable while `dep/fmt` is: it is pinned at
// 12.1.0, commit 407c905e45ad75fc29bf0f9bb7c5c2fd3475976f. A fmt bump is a
// DECLARED re-derivation of these two literals, not a silent one.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <hven/detail/interior/iterate_info.h>
#include <hven/drivers/console_trace_sink.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/trace.h>

namespace hven::solvers {
namespace {

// --- the scripted records ---------------------------------------------------
//
// TWO PHASES OF SIX, and every column that the table shows is set. `iter_`
// restarts at 0 in the second phase exactly as the engine's does, which is also
// what makes the header re-print rule (`iter_ % 10 == 0`) fire twice.
//
// THE COLOUR THRESHOLDS ARE EXERCISED ON PURPOSE. Rows 0..4 of each phase
// improve every column monotonically; row 5 WORSENS `kkt_inf_`, `barr_inf_` and
// `barr_obj_` by a factor of ten while `econ_inf_` and `icon_inf_` keep
// improving -- so the six previous-row comparison colours are not all the same
// value on any row that has a predecessor. And phase 1's row 0 has a HIGHER
// `prim_obj_` than phase 0's row 5: if the phase reset were missing, that row
// would colour red instead of uncoloured, which is what makes the reset
// visible in these bytes rather than only in a separate assertion.
//
// THIS GENERATOR IS DUPLICATED VERBATIM in the BASE capture program
// (`.scratch/w5t87/basecap/capture_rows.cpp`, quoted in the transcripts): the
// two must construct the same records or the literal below means nothing.
std::vector<IterateInfo> scripted_rows() {
    std::vector<IterateInfo> v;
    for (int phase = 0; phase < 2; ++phase) {
        for (int k = 0; k < 6; ++k) {
            IterateInfo r;
            r.iter_ = k;
            const double bump = (k == 5) ? 10.0 : 1.0;
            r.mu_ = 1e-1 * std::pow(10.0, -k);
            r.prim_obj_ = 1.7e1 + (phase == 1 ? 1.0 : 0.0) - 0.5 * k;
            r.barr_obj_ = r.prim_obj_ + 0.25 * bump;
            r.kkt_inf_ = 1e-1 * std::pow(10.0, -k) * bump;
            r.barr_inf_ = 2e-1 * std::pow(10.0, -k) * bump;
            r.econ_inf_ = 3e-1 * std::pow(10.0, -k) / bump;
            r.icon_inf_ = 4e-1 * std::pow(10.0, -k) / bump;
            r.ls_iters_ = k;
            r.alpha_p_ = 1.0 - 0.05 * k;
            r.alpha_d_ = 1.0 - 0.02 * k;
            r.alpha_t_ = 1.0;
            r.h_facs_ = k % 3;
            r.h_pert_cum_ = 1e-8 * k;
            r.p_pivots_ = 2 * k;
            r.p_pivots_observed_ = true;
            r.max_e_mult_ = 1.5 + k;
            r.max_i_mult_ = 2.5 + k;
            r.merit_val_ = 1.9e1 - 0.3 * k;
            v.push_back(r);
        }
    }
    return v;
}

// THE SCRIPTED BEGIN EVENT carries the SHIPPED DEFAULT tolerances, which is
// what the BASE capture program's `IpmOptions` carried too -- so the five-band
// colouring is computed against the same eight numbers on both sides.
IpmSolveBeginTraceEvent scripted_begin(bool wide) {
    IpmSolveBeginTraceEvent e;
    e.n = 15;
    e.n_reduced = 10;
    e.me = 6;
    e.mi = 7;
    e.vars_free = 1;
    e.vars_lower_only = 2;
    e.vars_upper_only = 3;
    e.vars_ranged = 4;
    e.vars_fixed = 5;
    e.phases = 2;
    e.max_iters = 200;
    e.max_acc_iters = 25;
    e.kkt_tol = 1e-6;
    e.econ_tol = 1e-6;
    e.icon_tol = 1e-6;
    e.bar_tol = 1e-6;
    e.init_mu = 0.1;
    e.obj_scale = 1.0;
    e.acc_kkt_tol = 1e-2;
    e.acc_econ_tol = 1e-3;
    e.acc_icon_tol = 1e-3;
    e.acc_bar_tol = 1e-3;
    e.wide_console = wide;
    e.kkt_dim = 23;
    e.kkt_nnz = 101;
    e.internal_fixed_rows = 3;
    return e;
}

IpmSolveEndTraceEvent scripted_end() {
    IpmSolveEndTraceEvent e;
    e.status = SolveStatus::kOptimal;
    e.iters = 12;
    e.total_time_s = 1.5;
    e.pre_time_s = 0.25;
    e.func_time_s = 0.125;
    e.kkt_time_s = 0.0625;
    e.print_time_s = 0.03125;
    e.solver_init_time_s = 0.015625;
    e.misc_time_s = 1.015625;
    return e;
}

/// Reads a `FILE *` this test wrote to, from `from` to `to`.
std::string slice(std::FILE *f, long from, long to) {
    std::string out;
    if (to <= from) {
        return out;
    }
    std::fflush(f);
    out.resize(static_cast<std::size_t>(to - from));
    std::fseek(f, from, SEEK_SET);
    const std::size_t got = std::fread(out.data(), 1, out.size(), f);
    out.resize(got);
    std::fseek(f, 0, SEEK_END);
    return out;
}

std::string read_all(std::FILE *f) {
    std::fflush(f);
    const long end = std::ftell(f);
    return slice(f, 0, end);
}

/// A sink that counts every event it is handed and records the order.
struct CountingSink : TraceSink {
    std::vector<std::string> seen;
    // AN OPTIONAL SHARED, ORDERED OBSERVATION (M6 W5 T8.7 fix1, astra's
    // Minor). Two sinks with two private vectors cannot tell "a then b" from
    // "b then a": both vectors end up the same either way. When `shared` is
    // set, every event is also appended to ONE vector as "<tag>:<event>", and
    // the ORDER of delivery is then readable.
    std::vector<std::string> *shared = nullptr;
    std::string tag;

    void note(const char *name) {
        seen.push_back(name);
        if (shared != nullptr) {
            shared->push_back(tag + ":" + name);
        }
    }
    void on_ipqp_iter(const IpqpTraceIterEvent &) override { note("ipqp.iter"); }
    void on_ipqp_reg(const IpqpTraceRegEvent &) override { note("ipqp.reg"); }
    void on_ipqp_restart(const IpqpTraceRestartEvent &) override { note("ipqp.restart"); }
    void on_ipqp_route(const IpqpTraceRouteEvent &) override { note("ipqp.route"); }
    void on_ipqp_certify(const IpqpTraceCertifyEvent &) override { note("ipqp.certify"); }
    void on_ipqp_escape(const IpqpTraceEscapeEvent &) override { note("ipqp.escape"); }
    void on_qp_mode(const QpModeTraceEvent &) override { note("qp.mode"); }
    void on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) override {
        note("fallback.verdict");
    }
    void on_sqp_major(const SqpMajorTraceEvent &) override { note("sqp.major"); }
    void on_sqp_solve_begin(const SqpSolveBeginTraceEvent &) override { note("sqp.solve.begin"); }
    void on_sqp_solve_end(const SqpSolveEndTraceEvent &) override { note("sqp.solve.end"); }
    void on_ipm_iter(const IpmIterTraceEvent &) override { note("ipm.iter"); }
    void on_ipm_solve_begin(const IpmSolveBeginTraceEvent &) override { note("ipm.solve.begin"); }
    void on_ipm_solve_end(const IpmSolveEndTraceEvent &) override { note("ipm.solve.end"); }
    void on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &) override {
        note("ipm.restoration_exit_row");
    }
};

// THE TWO LITERALS. Captured ONCE, at BASE (c0e3c6c), by running the OLD
// `print_last_iterate` on the records `scripted_rows()` builds -- six pushes
// into a fresh per-phase vector, printing after each, which is exactly what
// `alg_impl` did. The capture command and the program are in
// `.superpowers/w5-t8-7-psym-transcripts.md`.
#include "console_sink_base_rows.inc"

// --- the byte-pin -----------------------------------------------------------

void run_scripted(ConsoleTraceSink &sink, const std::vector<IterateInfo> &rows) {
    for (std::size_t i = 0; i < rows.size(); ++i) {
        sink.on_ipm_iter(IpmIterTraceEvent{rows[i], static_cast<Index>(i / 6)});
    }
}

TEST(ConsoleSink, IpmScriptedRowsAreByteIdenticalToTheOldPrinterNarrow) {
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{/*wide=*/false, /*print_level=*/0}, f);
    sink.on_ipm_solve_begin(scripted_begin(/*wide=*/false));
    std::fflush(f);
    const long after_begin = std::ftell(f);
    run_scripted(sink, scripted_rows());
    std::fflush(f);
    const long after_rows = std::ftell(f);
    sink.on_ipm_solve_end(scripted_end());

    EXPECT_EQ(slice(f, after_begin, after_rows), std::string(kBaseNarrowRows));
    // The blocks either side are non-empty and the whole capture is exactly the
    // three pieces: no stray output between them.
    const std::string all = read_all(f);
    EXPECT_GT(after_begin, 0);
    EXPECT_GT(static_cast<long>(all.size()), after_rows);
    std::fclose(f);
}

TEST(ConsoleSink, IpmScriptedRowsAreByteIdenticalToTheOldPrinterWide) {
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    // `Format::wide` is FALSE here on purpose: the begin event's own
    // `wide_console` is what a solve's layout is, and this proves the sink
    // adopts it rather than keeping its construction-time default.
    ConsoleTraceSink sink(ConsoleTraceSink::Format{/*wide=*/false, /*print_level=*/0}, f);
    sink.on_ipm_solve_begin(scripted_begin(/*wide=*/true));
    std::fflush(f);
    const long after_begin = std::ftell(f);
    run_scripted(sink, scripted_rows());
    std::fflush(f);
    const long after_rows = std::ftell(f);

    EXPECT_EQ(slice(f, after_begin, after_rows), std::string(kBaseWideRows));
    std::fclose(f);
}

TEST(ConsoleSink, ThePhaseChangeColoursTheNextPhasesFirstRowAsAFirstRow) {
    // A1's shape: ONE begin, twelve rows, a phase change in the middle. Phase
    // 1's first row has a HIGHER prim_obj than phase 0's last, so a missing
    // reset would paint its separator red; a correct reset leaves it plain.
    const std::vector<IterateInfo> rows = scripted_rows();
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_ipm_solve_begin(scripted_begin(false));
    std::fflush(f);
    const long after_begin = std::ftell(f);
    sink.on_ipm_iter(IpmIterTraceEvent{rows[0], 0});
    std::fflush(f);
    const long after_first = std::ftell(f);
    run_scripted(sink, rows);
    std::fflush(f);
    const long after_all = std::ftell(f);

    const std::string first_row_of_phase_0 = slice(f, after_begin, after_first);
    const std::string replay = slice(f, after_first, after_all);
    // The twelfth row of the replay is phase 1's row 0. Its bytes must equal
    // phase 0's row 0 except for the objective columns' VALUES -- and, in
    // particular, must carry no colour on the separators. The cheapest exact
    // statement of that: the replay contains phase 0's row 0 twice over (once
    // as its own first row, once as the start of phase 1 with a different
    // objective), so we compare the SEPARATOR bytes instead.
    EXPECT_NE(first_row_of_phase_0.find("|Iter| mu Val"), std::string::npos)
        << "the header re-prints at iter_ % 10 == 0";
    // Phase 1's row 0 is the seventh row in `replay`; find it by the header,
    // which re-prints there because iter_ is 0 again.
    //
    // THE SECOND ONE, AND THAT MATTERS (M6 W5 T8.7 fix1, astra's Minor):
    // `replay` begins with phase 0's rows -- re-emitted from row 0, so the
    // FIRST header in it is phase 0's own re-print, and selecting it examined
    // phase 0 twice while the name said phase 1. The phase boundary is the
    // SECOND header.
    const std::size_t first_header = replay.find("|Iter| mu Val");
    ASSERT_NE(first_header, std::string::npos) << "phase 0's replay re-prints the header";
    const std::size_t second_header = replay.find("|Iter| mu Val", first_header + 1);
    ASSERT_NE(second_header, std::string::npos) << "phase 1 re-prints the header";
    ASSERT_GT(second_header, first_header);
    // Exactly ONE row: from the end of that header line to the end of the row
    // that follows it.
    const std::size_t header_eol = replay.find('\n', second_header);
    ASSERT_NE(header_eol, std::string::npos);
    const std::size_t row_eol = replay.find('\n', header_eol + 1);
    ASSERT_NE(row_eol, std::string::npos);
    const std::string phase1_first = replay.substr(header_eol + 1, row_eol - header_eol);
    // A first row carries the DEFAULT style on the six comparison separators,
    // i.e. a bare "|" with no SGR sequence before it. A coloured separator
    // would be "\x1b[38;2;...m|\x1b[0m".
    EXPECT_EQ(phase1_first.find("m|\x1b[0m"), std::string::npos)
        << "phase 1's first row coloured a separator against phase 0's last row: " << phase1_first;
    // ... and the row that FOLLOWS it does colour, so the assertion above is
    // not passing for want of any colour at all.
    const std::size_t next_eol = replay.find('\n', row_eol + 1);
    ASSERT_NE(next_eol, std::string::npos);
    EXPECT_NE(replay.substr(row_eol + 1, next_eol - row_eol).find("m|\x1b[0m"), std::string::npos);
    std::fclose(f);
}

TEST(ConsoleSink, ASecondSolveBeginAlsoResetsThePreviousRow) {
    const std::vector<IterateInfo> rows = scripted_rows();
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_ipm_solve_begin(scripted_begin(false));
    sink.on_ipm_iter(IpmIterTraceEvent{rows[0], 0});
    sink.on_ipm_iter(IpmIterTraceEvent{rows[1], 0});
    sink.on_ipm_solve_end(scripted_end());
    std::fflush(f);
    const long mark = std::ftell(f);
    sink.on_ipm_solve_begin(scripted_begin(false));
    std::fflush(f);
    const long after_begin2 = std::ftell(f);
    // Row index 1 again: it HAS a predecessor in the first solve, so without
    // the reset it would colour; as a solve's first row it must not.
    sink.on_ipm_iter(IpmIterTraceEvent{rows[1], 0});
    std::fflush(f);
    const long after = std::ftell(f);
    const std::string row = slice(f, after_begin2, after);
    EXPECT_GT(mark, 0);
    EXPECT_EQ(row.find("m|\x1b[0m"), std::string::npos)
        << "the first row after a second begin coloured against the first solve";
    std::fclose(f);
}

TEST(ConsoleSink, IpmRendersNothingAboveTheTiers) {
    const std::vector<IterateInfo> rows = scripted_rows();
    // print_level 1: no stats block, no rows; the header and the two
    // Beginning/Finished lines and the timing summary still render.
    std::FILE *f1 = std::tmpfile();
    ASSERT_NE(f1, nullptr);
    ConsoleTraceSink one(ConsoleTraceSink::Format{false, 1}, f1);
    one.on_ipm_solve_begin(scripted_begin(false));
    run_scripted(one, rows);
    one.on_ipm_solve_end(scripted_end());
    const std::string at1 = read_all(f1);
    EXPECT_EQ(at1.find("Problem Statistics"), std::string::npos);
    EXPECT_EQ(at1.find("|Iter| mu Val"), std::string::npos);
    EXPECT_NE(at1.find("Beginning"), std::string::npos);
    EXPECT_NE(at1.find("Total Solve Time"), std::string::npos);
    std::fclose(f1);

    // print_level 2: the exit block's own tier; nothing this sink writes.
    std::FILE *f2 = std::tmpfile();
    ASSERT_NE(f2, nullptr);
    ConsoleTraceSink two(ConsoleTraceSink::Format{false, 2}, f2);
    two.on_ipm_solve_begin(scripted_begin(false));
    run_scripted(two, rows);
    two.on_ipm_solve_end(scripted_end());
    EXPECT_EQ(read_all(f2), "");
    std::fclose(f2);

    // print_level 3: silent.
    std::FILE *f3 = std::tmpfile();
    ASSERT_NE(f3, nullptr);
    ConsoleTraceSink three(ConsoleTraceSink::Format{false, 3}, f3);
    three.on_ipm_solve_begin(scripted_begin(false));
    run_scripted(three, rows);
    three.on_ipm_solve_end(scripted_end());
    EXPECT_EQ(read_all(f3), "");
    std::fclose(f3);
}

TEST(ConsoleSink, TheRestorationExitRowMarkerRendersNothing) {
    // It marks a row that is ALREADY on the stream as an `ipm.iter` line, and
    // the sink renders one row per `ipm.iter`. Rendering here would double it.
    const std::vector<IterateInfo> rows = scripted_rows();
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_ipm_solve_begin(scripted_begin(false));
    sink.on_ipm_iter(IpmIterTraceEvent{rows[0], 0});
    std::fflush(f);
    const long before = std::ftell(f);
    sink.on_ipm_restoration_exit_row(IpmRestorationExitRowTraceEvent{rows[0], 0, 1.5e-3, 1.0e-6});
    std::fflush(f);
    EXPECT_EQ(std::ftell(f), before);
    std::fclose(f);
}

// --- the SQP table ----------------------------------------------------------

SqpIterate make_row(Index trial, double f, bool solved, bool wd) {
    SqpIterate r;
    r.trial = trial;
    r.f = f;
    r.kkt_residual = 1e-2 / double(trial + 1);
    r.violation_l1 = 3e-3 / double(trial + 1);
    r.tr_radius = 1.0 / double(trial + 1);
    r.verdict = solved ? StepVerdict::kAcceptF : StepVerdict::kReject;
    r.qp_minor_iters = 3 * (trial + 1);
    r.qp_factorizations = trial + 1;
    r.watchdog_restored = wd;
    r.qp_solved = solved;
    return r;
}

SqpSolution make_solution() {
    SqpSolution sol;
    sol.status = SolveStatus::kOptimal;
    sol.counters.start_level_used = StartLevel::kWarm;
    sol.scaling.active = true;
    sol.scaling.obj = 0.25;
    sol.scaling.row_min = 0.5;
    sol.scaling.row_max = 4.0;
    sol.scaling.scaled_kkt_residual = 7.5e-9;
    sol.history.push_back(make_row(0, 1.7e1, true, false));
    sol.history.push_back(make_row(1, 1.6e1, false, false));
    sol.history.push_back(make_row(2, 1.5e1, true, true));
    return sol;
}

TEST(ConsoleSink, TheSqpTableIsByteIdenticalToFormatIterationTable) {
    const SqpSolution sol = make_solution();
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    for (std::size_t i = 0; i < sol.history.size(); ++i) {
        sink.on_sqp_major(
            SqpMajorTraceEvent{sol.history[i], static_cast<Index>(i), IpqpTraceQpMode::kWalk});
    }
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{
        sol.status, sol.counters.major_iters, sol.counters, sol.scaling.active, sol.scaling.obj,
        sol.scaling.row_min, sol.scaling.row_max, sol.scaling.scaled_kkt_residual});
    EXPECT_EQ(read_all(f), format_iteration_table(sol));
    std::fclose(f);
}

TEST(ConsoleSink, TheSqpTableRendersOnlyDepthZeroMajors) {
    const SqpSolution sol = make_solution();
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    sink.on_sqp_major(SqpMajorTraceEvent{sol.history[0], 0, IpqpTraceQpMode::kWalk});
    // A nested solve: its begin, its rows, its end. None of it renders.
    sink.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    EXPECT_EQ(sink.sqp_depth(), 1);
    sink.on_sqp_major(SqpMajorTraceEvent{sol.history[1], 0, IpqpTraceQpMode::kWalk});
    sink.on_sqp_major(SqpMajorTraceEvent{sol.history[2], 1, IpqpTraceQpMode::kWalk});
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{sol.status, 2, sol.counters});
    EXPECT_EQ(sink.sqp_depth(), 0);
    sink.on_sqp_major(SqpMajorTraceEvent{sol.history[1], 1, IpqpTraceQpMode::kWalk});
    sink.on_sqp_major(SqpMajorTraceEvent{sol.history[2], 2, IpqpTraceQpMode::kWalk});
    sink.on_sqp_solve_end(SqpSolveEndTraceEvent{
        sol.status, sol.counters.major_iters, sol.counters, sol.scaling.active, sol.scaling.obj,
        sol.scaling.row_min, sol.scaling.row_max, sol.scaling.scaled_kkt_residual});
    EXPECT_EQ(read_all(f), format_iteration_table(sol));
    std::fclose(f);
}

TEST(ConsoleSink, TheSqpTableHonoursTheThreeTiers) {
    const SqpSolution sol = make_solution();
    // 1: no rows; the head, Start Level and Scaling still render, and so does
    // Status.
    std::FILE *f1 = std::tmpfile();
    ASSERT_NE(f1, nullptr);
    ConsoleTraceSink one(ConsoleTraceSink::Format{false, 1}, f1);
    one.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    one.on_sqp_major(SqpMajorTraceEvent{sol.history[0], 0, IpqpTraceQpMode::kWalk});
    one.on_sqp_solve_end(
        SqpSolveEndTraceEvent{sol.status, 3, sol.counters, true, 0.25, 0.5, 4.0, 7.5e-9});
    const std::string at1 = read_all(f1);
    EXPECT_NE(at1.find("Trial"), std::string::npos);
    EXPECT_EQ(at1.find("1.700000e+01"), std::string::npos);
    EXPECT_NE(at1.find("Start Level"), std::string::npos);
    EXPECT_NE(at1.find("Scaling: obj="), std::string::npos);
    std::fclose(f1);

    // 2: the Status line only.
    std::FILE *f2 = std::tmpfile();
    ASSERT_NE(f2, nullptr);
    ConsoleTraceSink two(ConsoleTraceSink::Format{false, 2}, f2);
    two.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    two.on_sqp_major(SqpMajorTraceEvent{sol.history[0], 0, IpqpTraceQpMode::kWalk});
    two.on_sqp_solve_end(SqpSolveEndTraceEvent{sol.status, 3, sol.counters});
    EXPECT_EQ(read_all(f2), sqp_iteration_table_status_line(sol.status));
    std::fclose(f2);

    // 3: the SQP's own default. NOTHING, which is why this task's console is a
    // gain rather than a change for a caller who never set a print level.
    std::FILE *f3 = std::tmpfile();
    ASSERT_NE(f3, nullptr);
    ConsoleTraceSink three(ConsoleTraceSink::Format{false, 3}, f3);
    three.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    three.on_sqp_major(SqpMajorTraceEvent{sol.history[0], 0, IpqpTraceQpMode::kWalk});
    three.on_sqp_solve_end(SqpSolveEndTraceEvent{sol.status, 3, sol.counters});
    EXPECT_EQ(read_all(f3), "");
    std::fclose(f3);
}

TEST(ConsoleSink, TheEightQpTierEventsRenderNothing) {
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink sink(ConsoleTraceSink::Format{false, 0}, f);
    sink.on_ipqp_iter(IpqpTraceIterEvent{});
    sink.on_ipqp_reg(IpqpTraceRegEvent{});
    sink.on_ipqp_restart(IpqpTraceRestartEvent{});
    sink.on_ipqp_route(IpqpTraceRouteEvent{});
    sink.on_ipqp_certify(IpqpTraceCertifyEvent{});
    sink.on_ipqp_escape(IpqpTraceEscapeEvent{});
    sink.on_qp_mode(QpModeTraceEvent{});
    sink.on_fallback_verdict(SqpFallbackVerdictTraceEvent{});
    EXPECT_EQ(read_all(f), "");
    std::fclose(f);
}

// --- the fan-out ------------------------------------------------------------

TEST(FanOut, ForwardsEveryEventToBothHalvesFirstThenSecond) {
    // ONE SHARED, ORDERED OBSERVATION (M6 W5 T8.7 fix1): the two sinks write
    // into the same vector, tagged, so "first then second" is a property of the
    // sequence rather than of two sequences that happen to agree.
    std::vector<std::string> order;
    CountingSink a;
    CountingSink b;
    a.shared = &order;
    a.tag = "a";
    b.shared = &order;
    b.tag = "b";
    FanOutTraceSink fan(&a, &b);
    const SqpCounters counters;
    const IterateInfo row;
    const SqpIterate sqp_row;
    fan.on_ipqp_iter(IpqpTraceIterEvent{});
    fan.on_ipqp_reg(IpqpTraceRegEvent{});
    fan.on_ipqp_restart(IpqpTraceRestartEvent{});
    fan.on_ipqp_route(IpqpTraceRouteEvent{});
    fan.on_ipqp_certify(IpqpTraceCertifyEvent{});
    fan.on_ipqp_escape(IpqpTraceEscapeEvent{});
    fan.on_qp_mode(QpModeTraceEvent{});
    fan.on_fallback_verdict(SqpFallbackVerdictTraceEvent{});
    fan.on_sqp_major(SqpMajorTraceEvent{sqp_row, 0, IpqpTraceQpMode::kWalk});
    fan.on_sqp_solve_begin(SqpSolveBeginTraceEvent{});
    fan.on_sqp_solve_end(SqpSolveEndTraceEvent{SolveStatus::kOptimal, 0, counters});
    fan.on_ipm_iter(IpmIterTraceEvent{row, 0});
    fan.on_ipm_solve_begin(IpmSolveBeginTraceEvent{});
    fan.on_ipm_solve_end(IpmSolveEndTraceEvent{});
    fan.on_ipm_restoration_exit_row(IpmRestorationExitRowTraceEvent{row, 0, 0.0, 0.0});

    // EVERY virtual is covered: fifteen names, none repeated.
    EXPECT_EQ(a.seen.size(), 15u);
    EXPECT_EQ(a.seen, b.seen);
    EXPECT_EQ(fan.first(), &a);
    EXPECT_EQ(fan.second(), &b);

    // AND EVERY ONE OF THEM REACHED `a` BEFORE `b`: the shared log is exactly
    // thirty entries, alternating, each pair naming one event. Reversing the
    // fan-out's delivery order fails here and nowhere else.
    ASSERT_EQ(order.size(), 30u);
    for (std::size_t i = 0; i < a.seen.size(); ++i) {
        EXPECT_EQ(order[2 * i], "a:" + a.seen[i]) << "at event " << i;
        EXPECT_EQ(order[2 * i + 1], "b:" + a.seen[i]) << "at event " << i;
    }
}

TEST(FanOut, ANullHalfIsSkippedRatherThanDereferenced) {
    CountingSink a;
    FanOutTraceSink first_only(&a, nullptr);
    FanOutTraceSink second_only(nullptr, &a);
    FanOutTraceSink neither(nullptr, nullptr);
    const IterateInfo row;
    first_only.on_ipm_iter(IpmIterTraceEvent{row, 0});
    second_only.on_ipm_iter(IpmIterTraceEvent{row, 0});
    neither.on_ipm_iter(IpmIterTraceEvent{row, 0});
    EXPECT_EQ(a.seen.size(), 2u);
}

TEST(FanOut, AUserSinkIsNotDisplacedByTheConsole) {
    // The composition the drivers build: the caller's sink FIRST, the console
    // second. What the caller receives must be exactly what it receives with no
    // console at all.
    CountingSink user;
    std::FILE *f = std::tmpfile();
    ASSERT_NE(f, nullptr);
    ConsoleTraceSink console(ConsoleTraceSink::Format{false, 0}, f);
    FanOutTraceSink fan(&user, &console);
    const std::vector<IterateInfo> rows = scripted_rows();
    fan.on_ipm_solve_begin(scripted_begin(false));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        fan.on_ipm_iter(IpmIterTraceEvent{rows[i], static_cast<Index>(i / 6)});
    }
    fan.on_ipm_solve_end(scripted_end());

    CountingSink alone;
    alone.on_ipm_solve_begin(scripted_begin(false));
    for (std::size_t i = 0; i < rows.size(); ++i) {
        alone.on_ipm_iter(IpmIterTraceEvent{rows[i], static_cast<Index>(i / 6)});
    }
    alone.on_ipm_solve_end(scripted_end());

    EXPECT_EQ(user.seen, alone.seen);
    EXPECT_GT(read_all(f).size(), 0u);
    std::fclose(f);
}

} // namespace
} // namespace hven::solvers
