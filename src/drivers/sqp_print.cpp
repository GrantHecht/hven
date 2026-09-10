// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// sqp_print.cpp -- the SQP engine's display-string helpers and its iteration
// -table renderer, carved out of the headers that declare them.
//
// CLAUDE.md section 5 names printing explicitly as code that
// belongs in a .cpp translation unit: none of it is a per-element hot path,
// none of it depends on inlining through a template parameter, and every one
// of these functions runs O(1) times per solve or once per report row. As a
// header `inline` each definition was parsed and code-generated in every TU
// that included the header, and the linker discarded all but one copy.
//
// THE WHOLE SET IS FP-ARITHMETIC-FREE: switch-to-`const char *` functions, and one
// renderer that hands already-computed doubles to `fmt::format`. `fmt` READS
// a double and formats its value; it does not compute with it, so no
// floating-point expression in this file can be re-associated, contracted or
// otherwise re-shaped by the codegen flags.
//
// WHAT LIVES WHERE. The display strings for `SolveStatus` and `StartLevel` are
// defined in `src/core/enum_names.cpp`, so no `core/` object resolves a symbol
// out of a `drivers/` one (an edge pointing up CLAUDE.md section 2's tier
// order). What stays here points DOWNWARD and is not an inversion:
// `StepVerdict` and `SqpSolution` are `drivers/` types, and
// `PredictorOutcome` is a `detail/warmstart/` one.

#include <string>

#include <fmt/format.h>

#include <hven/core/solver_status.h>
#include <hven/core/start_level.h>
#include <hven/detail/warmstart/predictor.h>
#include <hven/drivers/sqp_driver.h>

namespace hven::solvers {

const char *to_string(PredictorOutcome outcome) {
    switch (outcome) {
    case PredictorOutcome::kPredicted:
        return "Predicted";
    case PredictorOutcome::kZeroStep:
        return "ZeroStep";
    case PredictorOutcome::kDegraded:
        return "Degraded";
    }
    return "Unknown";
}

const char *to_string(StepVerdict v) {
    switch (v) {
    case StepVerdict::kAcceptF:
        return "AcceptF";
    case StepVerdict::kAcceptH:
        return "AcceptH";
    case StepVerdict::kReject:
        return "Reject";
    case StepVerdict::kRestore:
        return "Restore";
    }
    return "?";
}

// THE TABLE, IN FIVE PIECES (M6 W5 T8.7). `format_iteration_table` renders a
// FINISHED solve from its `history`; `ConsoleTraceSink` renders the SAME table
// LIVE, from the event stream, and reaches each piece as the event that carries
// it arrives -- the head at `sqp.solve.begin`, one row per depth-0 `sqp.major`,
// the three trailer lines at `sqp.solve.end`, each at its own print level.
//
// The two renderers therefore share these five functions rather than each
// holding its own copy of a format string. That is not tidiness: the console is
// PINNED byte-for-byte against `format_iteration_table` on a live solve, and a
// second copy of nine field widths is exactly the thing that drifts.

std::string sqp_iteration_table_head() {
    const std::string header =
        fmt::format("{:>5} {:>14} {:>14} {:>14} {:>12} {:>9} {:>7} {:>7} {:>3}", "Trial", "f",
                    "KKT Res", "h", "Delta", "Verdict", "QP It", "QP Fact", "WD");
    std::string result = header + "\n";
    result += std::string(header.size(), '-');
    result += "\n";
    return result;
}

std::string sqp_iteration_table_row(const SqpIterate &row) {
    const char *wd_marker = row.watchdog_restored ? "*" : "";
    if (row.qp_solved) {
        return fmt::format(
            "{:>5} {:>14.6e} {:>14.6e} {:>14.6e} {:>12.6e} {:>9} {:>7} {:>7} {:>3}\n", row.trial,
            row.f, row.kkt_residual, row.violation_l1, row.tr_radius, to_string(row.verdict),
            row.qp_minor_iters, row.qp_factorizations, wd_marker);
    }
    return fmt::format("{:>5} {:>14.6e} {:>14.6e} {:>14.6e} {:>12.6e} {:>9} {:>7} {:>7} {:>3}\n",
                       row.trial, row.f, row.kkt_residual, row.violation_l1, row.tr_radius, "-",
                       "-", "-", wd_marker);
}

std::string sqp_iteration_table_status_line(SolveStatus status) {
    return fmt::format("\nStatus: {}\n", to_string(status));
}

std::string sqp_iteration_table_start_level_line(StartLevel level) {
    return fmt::format("Start Level: {}\n", to_string(level));
}

// THE SCALING LINE (M6 W0.2). Every column of the table above is on the
// CALLER's scale whether the solve was scaled or not, so without this line
// a reader cannot tell the two apart at all -- and on a scaled solve the
// number the convergence test actually gated on (`scaled_kkt_residual`) is
// not in the table, by construction. One line, and "off" when it is off.
std::string sqp_iteration_table_scaling_line(bool active, double obj, double row_min,
                                             double row_max, double scaled_kkt_residual) {
    if (active) {
        return fmt::format("Scaling: obj={:.6e} rows=[{:.6e}, {:.6e}] scaled_kkt={:.6e}\n", obj,
                           row_min, row_max, scaled_kkt_residual);
    }
    return "Scaling: off\n";
}

std::string format_iteration_table(const SqpSolution &sol) {
    std::string result = sqp_iteration_table_head();
    for (const SqpIterate &row : sol.history) {
        result += sqp_iteration_table_row(row);
    }
    result += sqp_iteration_table_status_line(sol.status);
    result += sqp_iteration_table_start_level_line(sol.counters.start_level_used);
    result +=
        sqp_iteration_table_scaling_line(sol.scaling.active, sol.scaling.obj, sol.scaling.row_min,
                                         sol.scaling.row_max, sol.scaling.scaled_kkt_residual);
    return result;
}

} // namespace hven::solvers
