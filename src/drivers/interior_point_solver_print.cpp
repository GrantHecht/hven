// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).

// WHAT IS LEFT HERE (M6 W5 T8.7). This file held the interior-point console
// table: the banner, the problem statistics, the iteration rows, the
// Beginning/Finished lines, the timing summary, the exit stats and the
// five-band residual colouring. Everything except the Beginning/Finished pair
// and `print_exit_stats` moved
// into `ConsoleTraceSink` (src/drivers/console_trace_sink.cpp), which renders
// it from the events the solver emits instead of from inside the solver;
// `calculate_color` went with it, as the free function `ipm_residual_color`,
// and is CALLED from here so the two renderings cannot drift. `print_settings`
// was deleted outright -- declared, defined, and called by nothing.
//
// The three that stay do so because the events that would carry them do not
// exist yet, and each is PER PHASE where the `ipm.solve` pair is per CALL:
// `print_beginning`/`print_finished` name a phase (and the KKT analysis), and
// `print_exit_stats` reports a phase's verdict, iterate and four times. T8.7b
// adds `on_ipm_phase_begin/end`, `on_ipm_kkt_analysis` and `on_ipm_phase_exit`,
// moves these three with them, and deletes this file.

#include "hven/drivers/interior_point_solver.h"

#include "hven/drivers/console_trace_sink.h"

void hven::solvers::InteriorPointSolver::print_beginning(std::string_view msg) const {
    fmt::print(fmt::fg(fmt::color::dim_gray), "Beginning");
    fmt::print(": ");
    fmt::print(fmt::fg(fmt::color::royal_blue), "{}", msg);
    fmt::print("\n");
}

void hven::solvers::InteriorPointSolver::print_finished(std::string_view msg) const {

    fmt::print(fmt::fg(fmt::color::dim_gray), "Finished ");
    fmt::print(": ");
    fmt::print(fmt::fg(fmt::color::royal_blue), "{}", msg);
    fmt::print("\n");
}

void hven::solvers::InteriorPointSolver::print_exit_stats(SolveStatus ExitCode,
                                                          const IterateInfo &last, int iternum,
                                                          double tottime, double nlptime,
                                                          double qptime, double printtime) {
    fmt::text_style Kcol = ipm_residual_color(last.kkt_inf_, opts_.kkt_tol, opts_.acc_kkt_tol);
    fmt::text_style Bcol = ipm_residual_color(last.barr_inf_, opts_.bar_tol, opts_.acc_bar_tol);
    fmt::text_style Ecol = ipm_residual_color(last.econ_inf_, opts_.econ_tol, opts_.acc_econ_tol);
    fmt::text_style Icol = ipm_residual_color(last.icon_inf_, opts_.icon_tol, opts_.acc_icon_tol);

    auto TColor = fmt::fg(fmt::color::cyan);
    auto Printtime = [&](const char *msg, double t1) {
        fmt::print("{}", msg);
        fmt::print(TColor, "{0:>10.3f} ms {1:>10.3f} ms/iter\n", t1, double(t1 / iternum));
    };

    if (opts_.common.print_level < 3) {
        if (ExitCode == SolveStatus::kOptimal) {
            fmt::print(fmt::fg(fmt::color::lime_green), "\nOptimal Solution Found\n");
        } else if (ExitCode == SolveStatus::kAcceptable) {
            fmt::print(fmt::fg(fmt::color::yellow), "\nAcceptable Solution Found\n");
        } else if (ExitCode == SolveStatus::kDiverging) {
            fmt::print(fmt::fg(fmt::color::dark_red), "\nSolution Diverging\n");
        } else if (ExitCode == SolveStatus::kMaxIter) {
            fmt::print(fmt::fg(fmt::color::red), "\nNo Solution Found\n");
        } else if (ExitCode == SolveStatus::kNumericalError) {
            fmt::print(fmt::fg(fmt::color::dark_red), "\nKKT System Persistently Singular\n");
        }
    }

    if (opts_.common.print_level < 2) {

        fmt::print(" Iterations : ");
        fmt::print("{:<5}\n", iternum);
        fmt::print(" Prim Obj   : ");
        fmt::print("{:<15.8e}\n", last.prim_obj_);
        fmt::print(" KKT Inf    : ");
        fmt::print(Kcol, "{:<15.8e}\n", last.kkt_inf_);
        fmt::print(" Bar Inf    : ");
        fmt::print(Bcol, "{:<15.8e}\n", last.barr_inf_);
        fmt::print(" ECons Inf  : ");
        fmt::print(Ecol, "{:<15.8e}\n", last.econ_inf_);
        fmt::print(" ICons Inf  : ");
        fmt::print(Icol, "{:<15.8e}\n", last.icon_inf_);

        // The last non-Success kkt_sol_.info() status observed across this
        // run_phase_sequence() call, if any. Silent whenever every
        // factorization reported Success.
        if (this->result_.last_kkt_info != Eigen::Success) {
            fmt::print(" KKT Factor Status : ");
            fmt::print(fmt::fg(fmt::color::yellow), "{}\n",
                       this->result_.last_kkt_info == Eigen::NumericalIssue ? "NumericalIssue"
                                                                            : "InvalidInput");
        }

        fmt::print("\n");

        Printtime(" NLP Function Evaluation Time : ", nlptime);
        Printtime(" KKT Matrix Factor/Solve Time : ", qptime);
        Printtime(" Console Print Time           : ", printtime);
        Printtime(" Total Time                   : ", tottime);

        fmt::print("\n");
    }
}
