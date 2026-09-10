// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).
//
// THE ATTRIBUTION TRAVELS WITH THE CODE (M6 W5 T8.7 fix1, astra's Minor). The
// interior-point renderings below were MOVED here, verbatim, from
// src/drivers/interior_point_solver_print.cpp, which carries exactly this
// header; moving ASSET-derived code into a new file does not make it new code,
// and notices/asset-apache2.txt's own terms are that "per-file headers on
// ASSET-derived source files identify the original source and summarise the
// changes made". The SQP renderings in this file are not ASSET-derived: they
// call src/drivers/sqp_print.cpp's pieces, which are hven's own.

// console_trace_sink.cpp -- both engines' console tables, rendered from the
// trace stream (M6 W5 T8.7).
//
// WHERE THIS CODE CAME FROM. The interior-point renderings below are
// `src/drivers/interior_point_solver_print.cpp`'s `print_banner`,
// `print_stats`, `print_last_iterate`, `print_beginning`, `print_finished` and
// `print_timing_summary`, plus `interior_point_solver.h`'s `print_header`,
// MOVED HERE VERBATIM -- the same format strings, the same field widths, the
// same `fmt::text_style` objects, in the same order. Three mechanical changes
// and no others:
//
//   1. every `fmt::print(...)` becomes `fmt::print(out_, ...)`, so the sink can
//      be pointed at a `tmpfile()` for the byte-pin;
//   2. the values come off the event instead of off the solver's members;
//   3. the row colouring reads six saved scalars instead of indexing
//      `iters[size - 2]`.
//
// The SQP renderings are `src/drivers/sqp_print.cpp`'s five pieces, called --
// not copied.
//
// FP-ARITHMETIC. `ipm_residual_color`'s logarithms and the NNZ% percentage are
// the only floating-point EXPRESSIONS here; both moved verbatim from the file
// they came from, and the tree compiles under one uniform flag regime
// (CLAUDE.md section 7), so neither is re-shaped relative to the code it
// replaces. Everything else hands an already-computed double to fmt.
//
// CLAUDE.md section 5 puts printing in a .cpp TU without qualification: nothing
// here is a per-element hot path and nothing depends on inlining through a
// template parameter.

#include <hven/drivers/console_trace_sink.h>

#include <algorithm>
#include <cmath>

#include <fmt/format.h>

#include <hven/detail/interior/iterate_info.h>
#include <hven/drivers/sqp_driver.h>

namespace hven::solvers {

// --- the colour bands -------------------------------------------------------

fmt::text_style ipm_residual_color(double value, double target, double acceptable) {
    constexpr double kFloor = 1e-300;
    auto level1 = std::log(std::max(target, kFloor));
    auto level3 = std::log(std::max(acceptable, kFloor));
    auto level5 = std::log(std::max(acceptable * 1000.0, kFloor));
    auto level2 = (level1 + level3) / 2.0;
    auto level4 = (level3 + level5) / 2.0;

    auto logval = std::log(std::max(value, kFloor));
    fmt::color c;

    if (logval < level1)
        c = fmt::color::lime_green;
    else if (logval < level2)
        c = fmt::color::yellow;
    else if (logval < level3)
        c = fmt::color::orange;
    else if (logval < level4)
        c = fmt::color::red;
    else
        c = fmt::color::dark_red;
    return fmt::fg(c);
}

// --- ConsoleTraceSink -------------------------------------------------------

ConsoleTraceSink::ConsoleTraceSink(Format f, std::FILE *out) : fmt_(f), out_(out), wide_(f.wide) {}

ConsoleTraceSink::~ConsoleTraceSink() = default;

// The eight QP-tier events. Named parameters would be unused; the bodies are
// empty on purpose and the header says why.
void ConsoleTraceSink::on_ipqp_iter(const IpqpTraceIterEvent &) {}
void ConsoleTraceSink::on_ipqp_reg(const IpqpTraceRegEvent &) {}
void ConsoleTraceSink::on_ipqp_restart(const IpqpTraceRestartEvent &) {}
void ConsoleTraceSink::on_ipqp_route(const IpqpTraceRouteEvent &) {}
void ConsoleTraceSink::on_ipqp_certify(const IpqpTraceCertifyEvent &) {}
void ConsoleTraceSink::on_ipqp_escape(const IpqpTraceEscapeEvent &) {}
void ConsoleTraceSink::on_qp_mode(const QpModeTraceEvent &) {}
void ConsoleTraceSink::on_fallback_verdict(const SqpFallbackVerdictTraceEvent &) {}

// --- the interior-point table ----------------------------------------------

void ConsoleTraceSink::ipm_print_header() {
    fmt::print(out_, fmt::fg(fmt::color::white), "{0:=^{1}}\n", "", 65);
}

void ConsoleTraceSink::ipm_print_beginning(std::string_view msg) {
    fmt::print(out_, fmt::fg(fmt::color::dim_gray), "Beginning");
    fmt::print(out_, ": ");
    fmt::print(out_, fmt::fg(fmt::color::royal_blue), "{}", msg);
    fmt::print(out_, "\n");
}

void ConsoleTraceSink::ipm_print_finished(std::string_view msg) {
    fmt::print(out_, fmt::fg(fmt::color::dim_gray), "Finished ");
    fmt::print(out_, ": ");
    fmt::print(out_, fmt::fg(fmt::color::royal_blue), "{}", msg);
    fmt::print(out_, "\n");
}

void ConsoleTraceSink::ipm_print_stats(const IpmSolveBeginTraceEvent &event) {
    // print_banner()
    constexpr const char *kBannerStr = "    / /_ | | / / __ \\/ __ \\\n"
                                       "   / __ \\| |/ / /_/ / / / /\n"
                                       "  / / / /|   / _, _/ /_/ / \n"
                                       " /_/ /_/ |__/_/ |_|\\____/  \n";
    ipm_print_header();
    fmt::print(out_, fmt::fg(fmt::color::crimson), "{}", kBannerStr);
    fmt::print(out_, fmt::fg(fmt::color::crimson), " \n       hven Interior-Point Solver\n");
    ipm_print_header();

    auto cyan = fmt::fg(fmt::color::cyan);
    auto magenta = fmt::fg(fmt::color::magenta);

    fmt::print(out_, magenta, "Problem Statistics\n\n");

    fmt::print(out_, " Primal Variables         : ");
    fmt::print(out_, cyan, "{:<10}\n", event.n_reduced);
    fmt::print(out_, " Equality Constraints     : ");
    // The count of the SOLVED system, which under the make_constraint
    // fixed-variable treatment includes one internal row per fixed variable.
    // Those rows are the solver's own, so the breakdown is spelled out rather
    // than leaving a user who declared three rows wondering why five are
    // reported. No other treatment installs any, so the suffix is absent
    // everywhere else.
    if (event.internal_fixed_rows > 0) {
        fmt::print(out_, cyan, "{:<10}\n",
                   fmt::format("{0} ({1} declared + {2} fixing)", event.me,
                               event.me - event.internal_fixed_rows, event.internal_fixed_rows));
    } else {
        fmt::print(out_, cyan, "{:<10}\n", event.me);
    }
    fmt::print(out_, " Inequality Constraints   : ");
    fmt::print(out_, cyan, "{:<10}\n", event.mi);
    fmt::print(out_, "\n");
    fmt::print(out_, " KKT-Matrix DIM (P+S+E+I) : ");
    fmt::print(out_, cyan, "{:<10}\n", event.kkt_dim);
    fmt::print(out_, " KKT-Matrix NNZs          : ");
    fmt::print(out_, cyan, "{:<10}\n", event.kkt_nnz);
    fmt::print(out_, " KKT-Matrix NNZ%          : ");
    fmt::print(out_, cyan, "{:.6f}%\n",
               100.0 * double(event.kkt_nnz) / (double(event.kkt_dim) * double(event.kkt_dim)));
    fmt::print(out_, "\n");
}

void ConsoleTraceSink::ipm_print_row(const IterateInfo &last) {
    if (last.iter_ % 10 == 0) {
        if (wide_) {
            fmt::print(out_, "{0:=^{1}}\n", "", 159);
            fmt::print(
                out_,
                "|Iter| mu Val | Prim Obj |  Bar Obj |  KKT Inf |  Bar Inf | ECons Inf| ICons "
                "Inf|Max "
                "EMult|Max IMult| AlphaP | AlphaD | AlphaT | Merit Val|LSI|PPS|HFI| HPert |\n");
        } else {
            fmt::print(out_, "{0:=^{1}}\n", "", 119);
            fmt::print(out_,
                       "|Iter| mu Val | Prim Obj |  Bar Obj |  KKT Inf |  Bar Inf | ECons Inf| "
                       "ICons Inf| AlphaP | "
                       "AlphaD |LS| PPS |HF| HPert |\n");
        }
    }

    fmt::text_style PHashcol = fmt::text_style();
    fmt::text_style EHashcol = fmt::text_style();
    fmt::text_style IHashcol = fmt::text_style();
    fmt::text_style KHashcol = fmt::text_style();
    fmt::text_style BHashcol = fmt::text_style();
    fmt::text_style BOHashcol = fmt::text_style();

    fmt::text_style Kcol = ipm_residual_color(last.kkt_inf_, kkt_tol_, acc_kkt_tol_);
    fmt::text_style Bcol = ipm_residual_color(last.barr_inf_, bar_tol_, acc_bar_tol_);
    fmt::text_style Ecol = ipm_residual_color(last.econ_inf_, econ_tol_, acc_econ_tol_);
    fmt::text_style Icol = ipm_residual_color(last.icon_inf_, icon_tol_, acc_icon_tol_);

    // `iters.size() > 1` in the old printer: "this phase has already shown a
    // row". `have_prev_row_` is that predicate, cleared on a begin and on a
    // phase change.
    if (have_prev_row_) {

        auto GCol = fmt::fg(fmt::color::lime_green);
        auto BCol = fmt::fg(fmt::color::red);

        PHashcol = (last.prim_obj_ <= prev_prim_obj_) ? GCol : BCol;
        BOHashcol = (last.barr_obj_ <= prev_barr_obj_) ? GCol : BCol;
        BHashcol = (last.barr_inf_ <= prev_barr_inf_) ? GCol : BCol;
        EHashcol = (last.econ_inf_ <= prev_econ_inf_) ? GCol : BCol;
        IHashcol = (last.icon_inf_ <= prev_icon_inf_) ? GCol : BCol;
        KHashcol = (last.kkt_inf_ <= prev_kkt_inf_) ? GCol : BCol;
    }

    auto hash = [this]() { fmt::print(out_, "|"); };
    auto chash = [this](fmt::text_style c) { fmt::print(out_, c, "|"); };

    hash();
    fmt::print(out_, "{:<4}", last.iter_);
    hash();
    fmt::print(out_, "{:.2e}", last.mu_);
    hash();
    fmt::print(out_, "{:>10.3e}", last.prim_obj_);
    chash(PHashcol);
    fmt::print(out_, "{:>10.3e}", last.barr_obj_);
    chash(BOHashcol);
    fmt::print(out_, Kcol, "{:>10.4e}", last.kkt_inf_);
    chash(KHashcol);
    fmt::print(out_, Bcol, "{:>10.4e}", last.barr_inf_);
    chash(BHashcol);
    fmt::print(out_, Ecol, "{:>10.4e}", last.econ_inf_);
    chash(EHashcol);
    fmt::print(out_, Icol, "{:>10.4e}", last.icon_inf_);
    chash(IHashcol);

    // DISPLAY-ONLY CARVE-OUT: the HPert column shows the CUMULATIVE
    // perturbation total (h_pert_cum_), not the last delta (h_pert_). h_pert_
    // itself feeds the Hpert0 warm-start in alg_impl().
    if (wide_) {
        fmt::print(
            out_,
            "{:>9.3e}|{:>9.3e}|{:>8.2e}|{:>8.2e}|{:>8.2e}|{:>10.3e}|{:>3}|{:>3}|{:>3}|{:>6.1e}|\n",
            last.max_e_mult_, last.max_i_mult_, last.alpha_p_, last.alpha_d_, last.alpha_t_,
            last.merit_val_, last.ls_iters_, last.p_pivots_, last.h_facs_, last.h_pert_cum_);
    } else {
        fmt::print(out_, "{:>8.2e}|{:>8.2e}|{:>2}|{:>5}|{:>2}|{:>6.1e}|\n",
                   last.alpha_t_ * last.alpha_p_, last.alpha_t_ * last.alpha_d_, last.ls_iters_,
                   last.p_pivots_, last.h_facs_, last.h_pert_cum_);
    }
}

void ConsoleTraceSink::on_ipm_solve_begin(const IpmSolveBeginTraceEvent &event) {
    // THE SOLVE'S CONFIGURATION, adopted here and fixed for the solve: the
    // eight tolerances the colouring needs and the layout width. `Format::wide`
    // was only the default until this arrived.
    wide_ = event.wide_console;
    kkt_tol_ = event.kkt_tol;
    bar_tol_ = event.bar_tol;
    econ_tol_ = event.econ_tol;
    icon_tol_ = event.icon_tol;
    acc_kkt_tol_ = event.acc_kkt_tol;
    acc_bar_tol_ = event.acc_bar_tol;
    acc_econ_tol_ = event.acc_econ_tol;
    acc_icon_tol_ = event.acc_icon_tol;

    // RESET ONE OF TWO (the other is a phase change in on_ipm_iter): the first
    // row of a solve colours as a first row.
    have_prev_row_ = false;
    prev_phase_ = 0;

    if (fmt_.print_level >= 3) {
        return;
    }
    if (fmt_.print_level == 0) {
        ipm_print_stats(event);
    }
    if (fmt_.print_level < 2) {
        ipm_print_header();
        ipm_print_beginning("InteriorPointSolver ");
    }
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_iter(const IpmIterTraceEvent &event) {
    // RESET TWO OF TWO. The engine emits ONE begin/end pair per CALL, while the
    // old printer's colour state was `alg_impl`'s own per-phase `iters` vector,
    // so a new phase must colour its first row as a first row. `iter_ == 0` is
    // the same predicate the header rule keys on and covers the first row of a
    // phase whose index a caller cannot see.
    if (event.phase != prev_phase_ || event.iterate.iter_ == 0) {
        have_prev_row_ = false;
    }
    prev_phase_ = event.phase;

    if (fmt_.print_level == 0) {
        ipm_print_row(event.iterate);
        std::fflush(out_);
    }

    // THE PREVIOUS ROW IS REMEMBERED WHETHER OR NOT IT WAS PRINTED. At
    // print_level 0 the two coincide; at any other level nothing renders, and
    // keeping the state coherent costs six stores and means a level change
    // between solves cannot leave stale colours behind.
    prev_prim_obj_ = event.iterate.prim_obj_;
    prev_barr_obj_ = event.iterate.barr_obj_;
    prev_barr_inf_ = event.iterate.barr_inf_;
    prev_econ_inf_ = event.iterate.econ_inf_;
    prev_icon_inf_ = event.iterate.icon_inf_;
    prev_kkt_inf_ = event.iterate.kkt_inf_;
    have_prev_row_ = true;
}

void ConsoleTraceSink::on_ipm_solve_end(const IpmSolveEndTraceEvent &event) {
    if (fmt_.print_level >= 2) {
        return;
    }
    auto cyan = fmt::fg(fmt::color::cyan);
    // print_timing_summary()
    fmt::print(out_, " KKT Analysis/Init Time       : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.pre_time_s * 1000.0);
    fmt::print(out_, " NLP Function Evaluation Time : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.func_time_s * 1000.0);
    fmt::print(out_, " KKT Factor/Solve Time        : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.kkt_time_s * 1000.0);
    fmt::print(out_, " Console Print Time           : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.print_time_s * 1000.0);
    fmt::print(out_, " Misc Time                    : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.misc_time_s * 1000.0);
    // The solve's own total, which the engine reports in SECONDS and printed in
    // milliseconds.
    fmt::print(out_, " Total Solve Time             : ");
    fmt::print(out_, cyan, "{0:>10.3f} ms\n", event.total_time_s * 1000.0);
    ipm_print_finished("InteriorPointSolver ");
    ipm_print_header();
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_restoration_exit_row(const IpmRestorationExitRowTraceEvent &) {}

// --- the interior-point engine's last direct prints (M6 W5 T8.7b) ----------
//
// EVERY RENDERING BELOW IS THE OLD CODE, MOVED. The per-phase lines and the
// exit block are `interior_point_solver_print.cpp`'s `print_beginning`,
// `print_finished` and `print_exit_stats`; the analysis block is the two
// `print_level < 2` blocks that bracketed the factorization in
// `InteriorPointSolver::init_impl`; the nine messages are the nine `fmt::print`
// calls that stood at their sites. Same format strings, same widths, same
// `fmt::text_style` objects, same order. The three mechanical changes are this
// file's own three (`out_`, values off the event, no solver members).

void ConsoleTraceSink::on_ipm_phase_begin(const IpmPhaseTraceEvent &event) {
    // NO COLOUR RESET HERE, deliberately. The previous-row state is dropped on
    // `ipm.solve.begin` and on an `ipm.iter` that opens a new phase, and those
    // two rules are the whole of it (M6 W5 T8.7 (i)); a third reset keyed on
    // this event would be a second rule saying the same thing, which is one
    // more place for the two to disagree.
    if (fmt_.print_level >= 2) {
        return;
    }
    ipm_print_beginning(event.label);
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_phase_end(const IpmPhaseTraceEvent &event) {
    if (fmt_.print_level >= 2) {
        return;
    }
    ipm_print_finished(event.label);
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) {
    // BOTH HALVES FROM ONE EVENT. `init_impl` printed the `Beginning` line
    // before the factorization and the rest after it; nothing could interleave
    // between them, so rendering both here reproduces the transcript exactly.
    if (fmt_.print_level >= 2) {
        return;
    }
    ipm_print_beginning("KKT-Matrix Analysis ");
    auto cyan = fmt::fg(fmt::color::cyan);
    // THE TWO CONDITIONALS STAY IN THE CONSOLE, where they always were: the
    // size and FLOPs lines only on a fresh analysis (a refactorization's
    // figures are the previous analysis's), and the FLOPs line only when the
    // backend reported a positive count.
    if (event.docompute) {
        fmt::print(out_, " LDLT Factor Size      : ");
        fmt::print(out_, cyan, "{0:<10}\n", event.factor_mem);
        if (event.factor_flops > 0) {
            fmt::print(out_, " LDLT Factor FLOPs     : ");
            fmt::print(out_, cyan, "{0} MFLOPs\n", event.factor_flops);
        }
    }
    fmt::print(out_, " Analysis/Reorder Time : ");
    fmt::print(out_, cyan, "{0:.3f} ms\n", event.analysis_time_s * 1000.0);
    ipm_print_finished("KKT-Matrix Analysis ");
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) {
    if (fmt_.print_level >= 3) {
        return;
    }
    const IterateInfo &last = event.iterate;
    fmt::text_style Kcol = ipm_residual_color(last.kkt_inf_, kkt_tol_, acc_kkt_tol_);
    fmt::text_style Bcol = ipm_residual_color(last.barr_inf_, bar_tol_, acc_bar_tol_);
    fmt::text_style Ecol = ipm_residual_color(last.econ_inf_, econ_tol_, acc_econ_tol_);
    fmt::text_style Icol = ipm_residual_color(last.icon_inf_, icon_tol_, acc_icon_tol_);

    // THE VERDICT LINE, on the RESOLVED status (M6 W5 T8.7b). The old chain
    // keyed on `alg_impl`'s RAW exit code, which was printed before
    // `resolve_ipm_phase_status` ran; resolution only ever rewrites kMaxIter,
    // into kStalled or kInterrupted, and all three take the branch the raw
    // kMaxIter took. Same bytes, one key, and the three door fixtures pin it.
    const SolveStatus status = event.report.status;
    if (status == SolveStatus::kOptimal) {
        fmt::print(out_, fmt::fg(fmt::color::lime_green), "\nOptimal Solution Found\n");
    } else if (status == SolveStatus::kAcceptable) {
        fmt::print(out_, fmt::fg(fmt::color::yellow), "\nAcceptable Solution Found\n");
    } else if (status == SolveStatus::kDiverging) {
        fmt::print(out_, fmt::fg(fmt::color::dark_red), "\nSolution Diverging\n");
    } else if (status == SolveStatus::kMaxIter || status == SolveStatus::kStalled ||
               status == SolveStatus::kInterrupted) {
        fmt::print(out_, fmt::fg(fmt::color::red), "\nNo Solution Found\n");
    } else if (status == SolveStatus::kNumericalError) {
        fmt::print(out_, fmt::fg(fmt::color::dark_red), "\nKKT System Persistently Singular\n");
    }

    if (fmt_.print_level < 2) {
        // The divisor the old block used was `iters.size()`, which is exactly
        // what the report's own `iterations` counts for this phase.
        const Index iternum = event.report.iterations;
        auto TColor = fmt::fg(fmt::color::cyan);
        auto Printtime = [&](const char *msg, double t1) {
            fmt::print(out_, "{}", msg);
            fmt::print(out_, TColor, "{0:>10.3f} ms {1:>10.3f} ms/iter\n", t1,
                       double(t1 / double(iternum)));
        };

        fmt::print(out_, " Iterations : ");
        fmt::print(out_, "{:<5}\n", iternum);
        fmt::print(out_, " Prim Obj   : ");
        fmt::print(out_, "{:<15.8e}\n", last.prim_obj_);
        fmt::print(out_, " KKT Inf    : ");
        fmt::print(out_, Kcol, "{:<15.8e}\n", last.kkt_inf_);
        fmt::print(out_, " Bar Inf    : ");
        fmt::print(out_, Bcol, "{:<15.8e}\n", last.barr_inf_);
        fmt::print(out_, " ECons Inf  : ");
        fmt::print(out_, Ecol, "{:<15.8e}\n", last.econ_inf_);
        fmt::print(out_, " ICons Inf  : ");
        fmt::print(out_, Icol, "{:<15.8e}\n", last.icon_inf_);

        // The last non-Success factorization status observed across the CALL,
        // if any. Silent whenever every factorization reported Success. The
        // two printed spellings are the old ternary's, which named
        // NumericalIssue and called everything else InvalidInput.
        if (event.last_kkt_info != IpmKktFactorStatus::kSuccess) {
            fmt::print(out_, " KKT Factor Status : ");
            fmt::print(out_, fmt::fg(fmt::color::yellow), "{}\n",
                       event.last_kkt_info == IpmKktFactorStatus::kNumericalIssue ? "NumericalIssue"
                                                                                  : "InvalidInput");
        }

        fmt::print(out_, "\n");

        // The event carries SECONDS; the block prints milliseconds, exactly as
        // `on_ipm_solve_end`'s timing summary does.
        Printtime(" NLP Function Evaluation Time : ", event.func_s * 1000.0);
        Printtime(" KKT Matrix Factor/Solve Time : ", event.kkt_s * 1000.0);
        Printtime(" Console Print Time           : ", event.print_s * 1000.0);
        Printtime(" Total Time                   : ", event.total_s * 1000.0);

        fmt::print(out_, "\n");
    }
    std::fflush(out_);
}

void ConsoleTraceSink::on_ipm_message(const IpmMessageTraceEvent &event) {
    // THE NOTICE IS THE ONE `< 2` KIND, and it keeps its second condition: the
    // engine suppressed the line when initialization was trivially fast. The
    // EVENT fires whenever initialization ran, so a sink that is not this one
    // sees the fact regardless.
    if (event.kind == IpmMessageKind::kSolverInitialized) {
        constexpr double kSolverInitPrintThresholdMs = 0.5;
        if (event.a > kSolverInitPrintThresholdMs && fmt_.print_level < 2) {
            fmt::print(out_, " Solver Initialization : ");
            fmt::print(out_, fmt::fg(fmt::color::cyan), "{0:.3f} ms\n", event.a);
            std::fflush(out_);
        }
        return;
    }
    if (fmt_.print_level >= 3) {
        return;
    }
    auto yellow = fmt::fg(fmt::color::yellow);
    switch (event.kind) {
    case IpmMessageKind::kSolverInitialized:
        // Handled above, before the `< 3` tier: it is a notice, not a warning.
        break;
    case IpmMessageKind::kRankDeficiency:
        fmt::print(out_, yellow, "Warning: Potential Rank Deficiency Detected\n");
        break;
    case IpmMessageKind::kFactorizationHardError:
        fmt::print(out_, yellow, "Warning: KKT factorization reported a hard error (info={})\n",
                   event.k);
        break;
    case IpmMessageKind::kInertiaExhausted:
        fmt::print(out_, yellow,
                   "Warning: Inertia correction exhausted ({} perturbation attempts, "
                   "inertia p/n/z = {}/{}/{}, expected {}/{}/0)\n",
                   event.k, event.p, event.n, event.z, event.expected_p, event.expected_n);
        break;
    case IpmMessageKind::kRestorationLocallyInfeasible:
        fmt::print(out_, yellow,
                   "Feasibility restoration converged to a locally infeasible "
                   "point (infeasibility {:.3e} > {:.3e}); stopping "
                   "(not converged).\n",
                   event.a, event.b);
        break;
    case IpmMessageKind::kFeasibilityStall:
        fmt::print(out_, yellow,
                   "Feasibility phase stalled with its restoration budget "
                   "exhausted and no relative improvement over the violation "
                   "at its last restoration entry (infeasibility {:.3e}, "
                   "{:.3e} at that entry); ending the phase — the convergence "
                   "check still reports the final verdict, which may be "
                   "acceptable.\n",
                   event.a, event.b);
        break;
    case IpmMessageKind::kInterruptAtIteration:
        fmt::print(out_, yellow, "Solve interrupted by the iteration callback at iteration {}.\n",
                   event.iter);
        break;
    case IpmMessageKind::kPhaseDiverged:
        fmt::print(out_, yellow, "Phase diverged; skipping remaining phases.\n");
        break;
    case IpmMessageKind::kInterruptSkippingPhases:
        fmt::print(out_, yellow,
                   "Solve interrupted by the iteration callback; skipping remaining "
                   "phases.\n");
        break;
    }
    std::fflush(out_);
}

// --- the SQP table ----------------------------------------------------------

void ConsoleTraceSink::on_sqp_solve_begin(const SqpSolveBeginTraceEvent &) {
    // DEPTH FIRST, exactly as JsonLinesTraceSink does it, so a nested solve is
    // already at depth 1 when its own begin is considered.
    if (sqp_open_solves_ > 0) {
        ++sqp_depth_;
    }
    ++sqp_open_solves_;
    if (sqp_depth_ > 0 || fmt_.print_level >= 2) {
        return;
    }
    fmt::print(out_, "{}", sqp_iteration_table_head());
    std::fflush(out_);
}

void ConsoleTraceSink::on_sqp_major(const SqpMajorTraceEvent &event) {
    // DEPTH-0 ROWS ONLY: `format_iteration_table` renders `SqpSolution::history`,
    // which holds top-level rows, and this sink is pinned against it.
    if (sqp_depth_ > 0 || fmt_.print_level != 0) {
        return;
    }
    fmt::print(out_, "{}", sqp_iteration_table_row(event.row));
    std::fflush(out_);
}

void ConsoleTraceSink::on_sqp_solve_end(const SqpSolveEndTraceEvent &event) {
    // The trailer belongs to the solve this line CLOSES, so it is rendered
    // before the nesting unwinds -- the same order JsonLinesTraceSink writes in.
    if (sqp_depth_ == 0) {
        if (fmt_.print_level < 3) {
            fmt::print(out_, "{}", sqp_iteration_table_status_line(event.status));
        }
        if (fmt_.print_level < 2) {
            fmt::print(out_, "{}",
                       sqp_iteration_table_start_level_line(event.counters.start_level_used));
            fmt::print(out_, "{}",
                       sqp_iteration_table_scaling_line(event.scaling_active, event.obj_scale,
                                                        event.row_scale_min, event.row_scale_max,
                                                        event.scaled_kkt_residual));
        }
        if (fmt_.print_level < 3) {
            std::fflush(out_);
        }
    }
    if (sqp_open_solves_ > 0) {
        --sqp_open_solves_;
    }
    if (sqp_open_solves_ > 0 && sqp_depth_ > 0) {
        --sqp_depth_;
    }
}

// --- FanOutTraceSink --------------------------------------------------------

FanOutTraceSink::FanOutTraceSink(TraceSink *first, TraceSink *second)
    : first_(first), second_(second) {}

FanOutTraceSink::~FanOutTraceSink() = default;

// One macro-free body per event, first then second, each half null-checked.
#define HVEN_FANOUT_FORWARD(method, type)                                                          \
    void FanOutTraceSink::method(const type &event) {                                              \
        if (first_ != nullptr) {                                                                   \
            first_->method(event);                                                                 \
        }                                                                                          \
        if (second_ != nullptr) {                                                                  \
            second_->method(event);                                                                \
        }                                                                                          \
    }

// THE ONE MACRO IN THIS FILE, and it earns its place: TWENTY bodies that
// differ only in a method name and a parameter type, where a hand-written copy
// would be twenty chances to forward to the wrong half. `HVEN_`-prefixed per
// CLAUDE.md section 4 and #undef'd immediately below.
HVEN_FANOUT_FORWARD(on_ipqp_iter, IpqpTraceIterEvent)
HVEN_FANOUT_FORWARD(on_ipqp_reg, IpqpTraceRegEvent)
HVEN_FANOUT_FORWARD(on_ipqp_restart, IpqpTraceRestartEvent)
HVEN_FANOUT_FORWARD(on_ipqp_route, IpqpTraceRouteEvent)
HVEN_FANOUT_FORWARD(on_ipqp_certify, IpqpTraceCertifyEvent)
HVEN_FANOUT_FORWARD(on_ipqp_escape, IpqpTraceEscapeEvent)
HVEN_FANOUT_FORWARD(on_qp_mode, QpModeTraceEvent)
HVEN_FANOUT_FORWARD(on_fallback_verdict, SqpFallbackVerdictTraceEvent)
HVEN_FANOUT_FORWARD(on_sqp_major, SqpMajorTraceEvent)
HVEN_FANOUT_FORWARD(on_sqp_solve_begin, SqpSolveBeginTraceEvent)
HVEN_FANOUT_FORWARD(on_sqp_solve_end, SqpSolveEndTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_iter, IpmIterTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_solve_begin, IpmSolveBeginTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_solve_end, IpmSolveEndTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_restoration_exit_row, IpmRestorationExitRowTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_begin, IpmPhaseTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_end, IpmPhaseTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_kkt_analysis, IpmKktAnalysisTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_exit, IpmPhaseExitTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_message, IpmMessageTraceEvent)

#undef HVEN_FANOUT_FORWARD

} // namespace hven::solvers
