// Derived from ASSET (AlabamaASRL/asset_asrl), https://github.com/AlabamaASRL/asset_asrl
// Copyright 2020-present The University of Alabama-Astrodynamics and Space Research Lab.
// Original developer: James B. Pezent. Licensed under the Apache License, Version 2.0
// (notices/asset-apache2.txt).
//
// Modified in hven. Copyright 2026-present Grant R. Hecht. Apache License, Version 2.0
// (see LICENSE).
//
// THE ATTRIBUTION TRAVELS WITH THE CODE. The KKT-analysis block of the interior-point
// rendering (`on_ipm_kkt_analysis`) keeps functional fragments of ASSET-derived code that
// reached this file from the retired src/drivers/interior_point_solver_print.cpp, which carries
// exactly this header; the rest of the file was rewritten for trace schema v1 (M8 TR-A) and is
// hven's own. Rewriting a file around surviving derived fragments does not make it new code, and
// notices/asset-apache2.txt's own terms are that "per-file headers on ASSET-derived source
// files identify the original source and summarise the changes made".

// console_trace_sink.cpp -- the human trace (docs/trace-schema-v1.md, "Human trace"). Printing
// code, so CLAUDE.md section 5 puts it in a TU.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>

#include <fmt/color.h>
#include <fmt/format.h>

#include <hven/core/start_level.h>
#include <hven/core/version.h>
#include <hven/solver/iteration_record.h>
#include <hven/trace/console_trace_sink.h>

#include "trace_names.h"

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace hven {

using detail::to_json;

namespace {

int rung(Verbosity v) { return static_cast<int>(v); }

bool is_terminal(std::FILE *f) {
#ifdef _WIN32
    return _isatty(_fileno(f)) != 0;
#else
    return isatty(fileno(f)) != 0;
#endif
}

std::string status_display(SolveStatus status) {
    std::string s = to_string(status);
    if (!s.empty()) {
        s[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(s[0])));
    }
    std::replace(s.begin(), s.end(), '_', ' ');
    return s;
}

// `-` right-aligned for an absent value, else the value in its format.
std::string opt_e(const std::optional<double> &v, int width, int precision) {
    if (!v) {
        return fmt::format("{:>{}}", "-", width);
    }
    return fmt::format("{:>{}.{}e}", *v, width, precision);
}

std::string opt_i(const std::optional<Index> &v, int width) {
    if (!v) {
        return fmt::format("{:>{}}", "-", width);
    }
    return fmt::format("{:>{}}", *v, width);
}

char tier_letter(IterationTier t) {
    switch (t) {
    case IterationTier::kBarrier:
        return 'B';
    case IterationTier::kIpqp:
        return 'I';
    case IterationTier::kSsn:
        return 'S';
    case IterationTier::kWalk:
        return 'W';
    case IterationTier::kElastic:
        return 'E';
    case IterationTier::kRestoration:
        return 'R';
    }
    return '?';
}

char verdict_letter(const std::optional<StepVerdict> &v) {
    if (!v) {
        return '-';
    }
    switch (*v) {
    case StepVerdict::kAcceptObjectiveStep:
        return 'F';
    case StepVerdict::kAcceptFeasibilityStep:
        return 'H';
    case StepVerdict::kReject:
        return 'J';
    case StepVerdict::kRestore:
        return 'Q';
    }
    return '?';
}

std::string ipm_codes(const IterationRecord &r, const IpmIterationRow &row) {
    std::string c;
    if (r.soc) {
        c += 's';
    }
    if (r.watchdog) {
        c += 'w';
    }
    if (r.rolled_back) {
        c += 'r';
    }
    if (row.first_rejected_trial) {
        c += 'k';
    }
    if (row.inertia_corrections && *row.inertia_corrections > 0) {
        c += 'i';
    }
    if (row.eval_exceptions > 0) {
        c += 'x';
    }
    if (r.accepted && !*r.accepted) {
        c += 'm';
    }
    return c;
}

std::string sqp_codes(const IterationRecord &r, const SqpIterationRow &row) {
    std::string c;
    if (r.soc) {
        c += 's';
    }
    if (r.watchdog) {
        c += 'w';
    }
    if (r.rolled_back) {
        c += 'r';
    }
    // `b` keeps its pre-I19-1 meaning (the step was region-limited): the radius OR a step-limit
    // pin.
    if (row.tr_binding || row.cap_binding) {
        c += 'b';
    }
    if (row.elastic_clamped) {
        c += 'c';
    }
    if (row.restoration_seed_used) {
        c += 'd';
    }
    if (row.qp_solved && row.qp_status && *row.qp_status != QpStatus::kOptimal) {
        c += 'n';
    }
    return c;
}

constexpr const char *kIpmColumns =
    " iter t      objective stat_scl feas_scl comp_scl     step  lgmu alpha_pr alpha_du ls lg(rg) "
    "codes";
constexpr const char *kSqpColumns =
    " iter t      objective stat_scl feas_scl comp_scl     step   radius    h(l1) qp_it qp_fa v "
    "codes";

// "variables 4 (boxed 4)": the parenthetical lists the non-empty categories.
std::string census_text(const char *noun, Index total,
                        std::initializer_list<std::pair<const char *, Index>> parts) {
    std::string inner;
    for (const auto &[name, count] : parts) {
        if (count > 0) {
            if (!inner.empty()) {
                inner += ", ";
            }
            inner += fmt::format("{} {}", name, count);
        }
    }
    return inner.empty() ? fmt::format("{} {}", noun, total)
                         : fmt::format("{} {} ({})", noun, total, inner);
}

std::string message_text(const MessageTraceEvent &e) {
    const char *viol = e.engine == TraceEngine::kIpm ? "theta" : "h";
    const auto num = [](const std::optional<double> &v) { return v ? *v : 0.0; };
    const auto idx = [](const std::optional<Index> &v) { return v ? *v : Index{0}; };
    switch (e.kind) {
    case MessageKind::kSolverInitialized:
        return fmt::format("solver initialized ({:.1f} ms)", num(e.a));
    case MessageKind::kRankDeficiency:
        return fmt::format("KKT matrix may be rank deficient (phase {}, iter {})", idx(e.phase),
                           idx(e.iter));
    case MessageKind::kFactorizationHardError:
        return fmt::format("KKT factorization failed, backend info {} (phase {}, iter {})",
                           idx(e.k), idx(e.phase), idx(e.iter));
    case MessageKind::kInertiaExhausted:
        return fmt::format(
            "inertia correction exhausted after {} attempts: inertia ({}, {}, {}), expected "
            "({}, {}, 0)",
            idx(e.k), idx(e.p), idx(e.n), idx(e.z), idx(e.expected_p), idx(e.expected_n));
    case MessageKind::kFeasibilityStall:
        return fmt::format("feasibility stalled: theta {:.1e}, theta at last restoration {:.1e}",
                           num(e.a), num(e.b));
    case MessageKind::kPhaseDiverged:
        return fmt::format("phase {} diverged; remaining phases skipped", idx(e.phase));
    case MessageKind::kInterruptSkippingPhases:
        return "callback stop honoured; remaining phases skipped";
    case MessageKind::kStopRequestedSkippingPhases:
        return "stop requested; remaining phases skipped";
    case MessageKind::kRestorationEntered:
        return fmt::format("restoration entered at iter {}: {} {:.1e}", idx(e.iter), viol,
                           num(e.a));
    case MessageKind::kRestorationExited: {
        const std::string_view why = e.s ? *e.s : std::string_view{};
        std::string t = fmt::format("restoration exited at iter {}: {}, {} {:.1e}", idx(e.iter),
                                    why, viol, num(e.a));
        if (why == "locally_infeasible" && e.b) {
            t += fmt::format(", threshold {:.1e}", *e.b);
        }
        return t;
    }
    case MessageKind::kRestorationRefused:
        return fmt::format("restoration refused ({})", e.s ? *e.s : std::string_view{});
    case MessageKind::kSubproblemFailure:
        return fmt::format("QP subproblem failed ({}); {} in a row",
                           e.s ? *e.s : std::string_view{}, idx(e.k));
    case MessageKind::kElasticExhausted: {
        std::string t = fmt::format("elastic ladder exhausted after {} rungs", idx(e.k));
        if (!e.ladder) {
            return t;
        }
        const ElasticLadderRow &l = *e.ladder;
        const auto entry_words = [](ElasticEntry v) {
            switch (v) {
            case ElasticEntry::kVerifiedInfeasible:
                return "verified infeasible in trust region";
            case ElasticEntry::kWorkingSetTermination:
                return "working-set numerical termination";
            case ElasticEntry::kRefusedCertificate:
                return "refused certificate";
            case ElasticEntry::kIpqpFallback:
                return "IPQP fallback";
            }
            return "unknown";
        };
        const auto status_words = [](QpStatus v) {
            switch (v) {
            case QpStatus::kOptimal:
                return "optimal";
            case QpStatus::kIterationLimit:
                return "iteration limit";
            case QpStatus::kInfeasible:
                return "infeasible";
            case QpStatus::kNumericalError:
                return "numerical failure";
            }
            return "unknown";
        };
        const auto yn = [](bool b) { return b ? "yes" : "no"; };
        return t +
               fmt::format(" (entered: {}; main QP: {}; last rung: {} at rho {:.0e}; closed {}, "
                           "reduced {}, promises_f {}; slack_l1 {:.1e})",
                           entry_words(l.entry), status_words(l.main_status),
                           status_words(l.rung_status), l.rho, yn(l.closed), yn(l.reduced),
                           yn(l.promises_f), l.slack_l1);
    }
    case MessageKind::kInconsistentEqualities:
        return "equality constraints are inconsistent";
    case MessageKind::kStartPointClamped:
        return fmt::format("start point clamped into the bounds ({} variables)", idx(e.k));
    case MessageKind::kScaledConvergenceRefused:
        return fmt::format("convergence on the scaled residual refused at iter {}", idx(e.iter));
    case MessageKind::kStopAtIteration:
        return fmt::format("stop at iter {} ({})", idx(e.iter), e.s ? *e.s : std::string_view{});
    }
    return "unknown message";
}

} // namespace

ConsoleTraceSink::ConsoleTraceSink(Format f, std::FILE *out) : fmt_(f), out_(out) {
    color_ = f.color == ConsoleColor::kAlways ||
             (f.color == ConsoleColor::kAuto && out != nullptr && is_terminal(out) &&
              std::getenv("NO_COLOR") == nullptr);
}

ConsoleTraceSink::~ConsoleTraceSink() = default;

bool ConsoleTraceSink::at_least(Verbosity v) const { return rung(fmt_.verbosity) >= rung(v); }

void ConsoleTraceSink::print_line(std::string_view text) { fmt::print(out_, "{}\n", text); }

// --- the QP-tier events: kDetail lines ---------------------------------------

void ConsoleTraceSink::on_ipqp_iter(const IpqpIterTraceEvent &) {}
void ConsoleTraceSink::on_ipqp_reg(const IpqpRegTraceEvent &) {}
void ConsoleTraceSink::on_ipqp_certify(const IpqpCertifyTraceEvent &) {}
void ConsoleTraceSink::on_ipqp_route(const IpqpRouteTraceEvent &) {}

void ConsoleTraceSink::on_ipqp_restart(const IpqpRestartTraceEvent &event) {
    if (!at_least(Verbosity::kDetail)) {
        return;
    }
    fmt::print(out_, "{:>5} . ipqp.restart {}{}{}\n", "", to_json(event.grade),
               event.repaired ? " repaired" : "", event.abandoned ? " abandoned" : "");
}

void ConsoleTraceSink::on_ipqp_escape(const IpqpEscapeTraceEvent &event) {
    if (!at_least(Verbosity::kDetail)) {
        return;
    }
    fmt::print(out_, "{:>5} . ipqp.escape {}\n", "", to_json(event.reason));
}

void ConsoleTraceSink::on_qp_mode(const QpModeTraceEvent &event) {
    if (!at_least(Verbosity::kDetail)) {
        return;
    }
    fmt::print(out_, "{:>5} . qp.mode {} {} at {}, {} iterations\n", "", to_json(event.mode),
               to_json(event.outcome), to_json(event.site), event.iters);
}

void ConsoleTraceSink::on_fallback_verdict(const SqpFallbackVerdictTraceEvent &event) {
    if (!at_least(Verbosity::kDetail)) {
        return;
    }
    fmt::print(out_, "{:>5} . fallback.verdict {}\n", "", to_json(event.verdict));
}

// --- solve.begin: header ------------------------------------------------------

void ConsoleTraceSink::on_solve_begin(const SolveBeginTraceEvent &event) {
    if (open_solves_ > 0) {
        ++depth_;
    }
    ++open_solves_;
    if (depth_ > 0) {
        return;
    }
    engine_ = event.engine;
    ipm_phases_ = event.ipm ? event.ipm->phases : 0;
    header_pending_ = true;
    rows_since_header_ = 0;
    if (!at_least(Verbosity::kSummary)) {
        return;
    }
    const ProblemStatistics &st = event.statistics;
    fmt::print(out_, "hven {} -- {}\n", version(),
               event.engine == TraceEngine::kIpm ? "interior-point engine" : "SQP engine");
    fmt::print(out_, "problem    {} · {} · absent bounds {}\n",
               census_text("variables", st.variables,
                           {{"free", st.free_variables},
                            {"lower", st.lower_bounded_variables},
                            {"upper", st.upper_bounded_variables},
                            {"boxed", st.boxed_variables},
                            {"fixed", st.fixed_variables}}),
               census_text("constraints", st.constraints,
                           {{"equality", st.equality_rows},
                            {"lower", st.lower_bounded_rows},
                            {"upper", st.upper_bounded_rows},
                            {"range", st.range_rows},
                            {"free", st.free_rows}}),
               st.absent_bounds);
    if (event.ipm) {
        const IpmSolveBeginInfo &i = *event.ipm;
        fmt::print(
            out_, "engine     KKT dim {}, nnz {} · inertia {} · restoration {} · phases {}\n",
            i.kkt_dim, i.kkt_nnz, to_json(i.inertia_mode), to_json(i.restoration_mode), i.phases);
    } else if (event.sqp) {
        const SqpSolveBeginInfo &i = *event.sqp;
        fmt::print(out_,
                   "engine     QP mode {} ({}) · Hessian {} · row scaling {} · column scaling {}\n",
                   to_json(i.qp_mode), to_json(i.ws_algebra), to_json(i.hessian_mode),
                   i.row_scaling ? "on" : "off", to_json(i.column_scaling));
    }
    fmt::print(out_, "tolerances optimality {:g} · feasibility {:g} · max iterations {}\n",
               event.optimality_tol, event.feasibility_tol, event.max_iterations);
}

// --- iter: column header, row -------------------------------------------

void ConsoleTraceSink::on_iter(const IterTraceEvent &event) {
    if (!at_least(Verbosity::kIterations)) {
        return;
    }
    const IterationRecord &r = event.record;
    const bool ipm = r.ipm.has_value();
    if (header_pending_ || rows_since_header_ >= 20) {
        fmt::print(out_, "{}\n", ipm ? kIpmColumns : kSqpColumns);
        header_pending_ = false;
        rows_since_header_ = 0;
    }
    ++rows_since_header_;
    const std::optional<double> &obj = r.recovering ? r.f_recovery : r.f;
    std::string line =
        fmt::format("{:>5} {} {} {:>8.2e} {:>8.2e} {:>8.2e} {}", r.iteration, tier_letter(r.tier),
                    opt_e(obj, 14, 7), r.tested_stationarity, r.tested_feasibility,
                    r.tested_complementarity, opt_e(r.step_norm, 8, 2));
    std::string codes;
    if (r.ipm) {
        const IpmIterationRow &row = *r.ipm;
        std::string lg_rg = "-";
        if (row.regularization_primal_total && *row.regularization_primal_total > 0.0) {
            lg_rg = fmt::format("{:.1f}", std::log10(*row.regularization_primal_total));
        }
        line += fmt::format(" {:>5.1f} {} {} {} {:>6}", std::log10(row.mu),
                            opt_e(row.alpha_primal, 8, 2), opt_e(row.alpha_dual, 8, 2),
                            opt_i(row.line_search_trials, 2), lg_rg);
        codes = ipm_codes(r, row);
    } else if (r.sqp) {
        const SqpIterationRow &row = *r.sqp;
        line += fmt::format(" {:>8.2e} {:>8.2e} {} {} {}", row.trust_radius, row.violation_l1,
                            opt_i(row.qp_minor_iterations, 5), opt_i(row.qp_factorizations, 4),
                            verdict_letter(row.verdict));
        codes = sqp_codes(r, row);
    }
    if (!codes.empty()) {
        line += ' ';
        line += codes;
    }
    print_line(line);
    std::fflush(out_);
}

// --- message ------------------------------------------------------------------------

void ConsoleTraceSink::on_message(const MessageTraceEvent &event) {
    const bool warning = event.severity == MessageSeverity::kWarning;
    if (event.kind == MessageKind::kSolverInitialized && !(event.a && *event.a > 0.5)) {
        return;
    }
    if (!at_least(warning ? Verbosity::kWarnings : Verbosity::kIterations)) {
        return;
    }
    const std::string text = message_text(event);
    const char mark = warning ? '!' : '-';
    if (warning && color_) {
        fmt::print(out_, "{:>5} ", "");
        fmt::print(out_, fmt::fg(fmt::color::orange_red), "{}", mark);
        fmt::print(out_, " {}\n", text);
    } else {
        fmt::print(out_, "{:>5} {} {}\n", "", mark, text);
    }
    header_pending_ = true;
    std::fflush(out_);
}

// --- IPM phase events -----------------------------------------------------------------

void ConsoleTraceSink::on_ipm_phase_begin(const IpmPhaseTraceEvent &) {}
void ConsoleTraceSink::on_ipm_phase_end(const IpmPhaseTraceEvent &) {}

void ConsoleTraceSink::on_ipm_kkt_analysis(const IpmKktAnalysisTraceEvent &event) {
    if (!at_least(Verbosity::kDetail)) {
        return;
    }
    std::string t = fmt::format("KKT analysis: dim {}, nnz {}", event.kkt_dim, event.nnz);
    if (event.docompute) {
        t += fmt::format(", factor size {}", event.factor_mem);
        if (event.factor_flops > 0) {
            t += fmt::format(", {} MFLOPs", event.factor_flops);
        }
    }
    if (fmt_.timing) {
        t += fmt::format(", {:.3f} ms", event.analysis_time_s * 1000.0);
    }
    fmt::print(out_, "{:>5} . {}\n", "", t);
}

void ConsoleTraceSink::on_ipm_phase_exit(const IpmPhaseExitTraceEvent &event) {
    const IpmPhaseReport &rep = event.report;
    if (at_least(Verbosity::kSummary) && ipm_phases_ > 1 && rep.ran) {
        fmt::print(out_, "phase {}: {} -- {}, {} iterations\n", event.phase, to_json(rep.phase),
                   to_string(rep.status), rep.iterations);
    }
    if (at_least(Verbosity::kDetail)) {
        fmt::print(out_,
                   "{:>5} . phase {} exit: stop {}, stationarity {:.1e}, complementarity {:.1e}\n",
                   "", event.phase, to_json(rep.stop_reason), event.iterate.kkt_inf,
                   event.iterate.barr_inf);
    }
}

// --- solve.end: footer ------------------------------------------------------------------

void ConsoleTraceSink::on_solve_end(const SolveEndTraceEvent &event) {
    if (depth_ == 0) {
        const bool declared = event.stop_reason.has_value();
        if (at_least(Verbosity::kWarnings) && declared) {
            const std::string reason = to_string(*event.stop_reason);
            const std::string msg = event.message ? std::string(*event.message) : std::string{};
            const std::string text =
                fmt::format("{} ({}) -- {}", status_display(event.status), reason, msg);
            fmt::print(out_, "{:<11} ", "exit");
            if (color_) {
                const fmt::color c = event.status == SolveStatus::kOptimal      ? fmt::color::green
                                     : event.status == SolveStatus::kAcceptable ? fmt::color::yellow
                                                                                : fmt::color::red;
                fmt::print(out_, fmt::fg(c), "{}", text);
                fmt::print(out_, "\n");
            } else {
                fmt::print(out_, "{}\n", text);
            }
        }
        if (at_least(Verbosity::kSummary) && declared) {
            const Index restoration_entries = event.ipm && event.ipm->counters != nullptr
                                                  ? event.ipm->counters->restoration_entries
                                              : event.sqp && event.sqp->counters != nullptr
                                                  ? event.sqp->counters->restoration_entries
                                                  : 0;
            fmt::print(out_, "{:<11} {} · restoration entries {} · factorizations {}\n",
                       "iterations", event.iterations, restoration_entries,
                       event.work ? event.work->factorizations : 0);
            fmt::print(out_, "{:<11} {:.8e}\n", "objective", event.f);
            if (event.diagnostics) {
                fmt::print(out_,
                           "{:<11} stationarity {:.1e} · feasibility {:.1e} · complementarity "
                           "{:.1e}   (declared)\n",
                           "diagnostics", event.diagnostics->stationarity,
                           event.diagnostics->feasibility, event.diagnostics->complementarity);
            }
            if (event.ipm && event.ipm->counters != nullptr) {
                const IpmCounters &c = *event.ipm->counters;
                fmt::print(out_, "{:<11} soc {} · watchdog {} · inertia corrections {}\n",
                           "counters", c.soc_steps, c.watchdog_activations, c.inertia_corrections);
            } else if (event.sqp && event.sqp->counters != nullptr) {
                const SqpCounters &c = *event.sqp->counters;
                fmt::print(out_,
                           "{:<11} soc {} · elastic {} · qp minors {} · start level {} · scaling "
                           "{}\n",
                           "counters", c.soc_applied, c.elastic_activations, c.qp_minor_iters,
                           to_json(event.sqp->start_level_used),
                           event.sqp->scaling_active ? "on" : "off");
            }
            if (fmt_.timing) {
                fmt::print(out_, "{:<11} {:.3f} s (informational)\n", "time", event.wall_s);
            }
        }
        if (at_least(Verbosity::kWarnings)) {
            std::fflush(out_);
        }
    }
    if (open_solves_ > 0) {
        --open_solves_;
    }
    if (open_solves_ > 0 && depth_ > 0) {
        --depth_;
    }
}

// A solve left by an exception: unwind exactly what on_solve_end would, print nothing.
// Saturating, so an abort the sink never saw the begin of (a sink earlier in a fan-out
// threw first) cannot drive the counts negative.
void ConsoleTraceSink::on_solve_abort() noexcept {
    if (open_solves_ > 0) {
        --open_solves_;
    }
    if (open_solves_ > 0 && depth_ > 0) {
        --depth_;
    }
    if (open_solves_ == 0) {
        depth_ = 0;
    }
}

// --- FanOutTraceSink ----------------------------------------------------------------------

FanOutTraceSink::FanOutTraceSink(TraceSink *first, TraceSink *second)
    : first_(first), second_(second) {}

FanOutTraceSink::~FanOutTraceSink() = default;

// One body per event, first then second, each half null-checked. The only macro in this
// file: sixteen bodies that differ in a method name and a parameter type.
#define HVEN_FANOUT_FORWARD(method, type)                                                          \
    void FanOutTraceSink::method(const type &event) {                                              \
        if (first_ != nullptr) {                                                                   \
            first_->method(event);                                                                 \
        }                                                                                          \
        if (second_ != nullptr) {                                                                  \
            second_->method(event);                                                                \
        }                                                                                          \
    }

HVEN_FANOUT_FORWARD(on_ipqp_iter, IpqpIterTraceEvent)
HVEN_FANOUT_FORWARD(on_ipqp_reg, IpqpRegTraceEvent)
HVEN_FANOUT_FORWARD(on_ipqp_restart, IpqpRestartTraceEvent)
HVEN_FANOUT_FORWARD(on_ipqp_route, IpqpRouteTraceEvent)
HVEN_FANOUT_FORWARD(on_ipqp_certify, IpqpCertifyTraceEvent)
HVEN_FANOUT_FORWARD(on_ipqp_escape, IpqpEscapeTraceEvent)
HVEN_FANOUT_FORWARD(on_qp_mode, QpModeTraceEvent)
HVEN_FANOUT_FORWARD(on_fallback_verdict, SqpFallbackVerdictTraceEvent)
HVEN_FANOUT_FORWARD(on_solve_begin, SolveBeginTraceEvent)
HVEN_FANOUT_FORWARD(on_iter, IterTraceEvent)
HVEN_FANOUT_FORWARD(on_message, MessageTraceEvent)
HVEN_FANOUT_FORWARD(on_solve_end, SolveEndTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_begin, IpmPhaseTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_end, IpmPhaseTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_kkt_analysis, IpmKktAnalysisTraceEvent)
HVEN_FANOUT_FORWARD(on_ipm_phase_exit, IpmPhaseExitTraceEvent)

#undef HVEN_FANOUT_FORWARD

void FanOutTraceSink::on_solve_abort() noexcept {
    if (first_ != nullptr) {
        first_->on_solve_abort();
    }
    if (second_ != nullptr) {
        second_->on_solve_abort();
    }
}

} // namespace hven
