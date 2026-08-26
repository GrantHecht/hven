// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

// bench/bench_crossover.cpp — M5 W5: the crossover-leg runner. CLI glue only;
// the legs, the dual-bind adapter and the margin arithmetic all live in
// bench/crossover_legs.h (a header, deliberately, so
// tests/sqp/test_crossover_legs.cpp shares exactly this implementation rather
// than a bench-only copy that could drift).
//
// PROTOCOL (docs/notes/2026-08-m5-ledger.md, "W5 LEG PROTOCOL", 2026-08-25;
// CLAUDE.md §7). This binary is SERIAL by construction: one cell at a time, one
// solve at a time inside it, no co-run arm and no width flag. The protocol
// declares no co-run terms for this artifact, so none are implementable here --
// a runner that could co-run would be a runner that could produce rows the
// artifact's own README does not describe.
//
// COUNTERS ARE THE ASSERTED CURRENCY. Every wall column this writes is
// informational; the artifact's README says so, and the margins are taken over
// majors, QP minors and factorizations alone.
//
// THE PER-CELL DEADLINE. Each cell runs in a forked child and is SIGKILLed if
// it outlives --deadline-seconds; the parent then writes that cell's rows as
// `dnf_budget` with -1 in every counter column, the same ABSENT-not-zero
// convention bench_corpus.cpp uses. The parent itself never solves, so it holds
// no MKL state across the fork. This is the CLAUDE.md §8 obligation ("no
// unbounded cells") and it is also why the artifact is complete: a cell that ran
// out of wall is a row that says so, never a missing row.
//
// A DEADLINE OUTCOME IS WALL-DEPENDENT and is therefore not a
// scheduling-invariant column. That is the second reason this runner does not
// co-run: §7 requires wall-dependent statuses to run solo or with their full
// budget honoured.

#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <exception>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fmt/format.h>

#include "crossover_legs.h"

#ifndef HVEN_SQP_CROSSOVER_GIT_DESCRIBE
#define HVEN_SQP_CROSSOVER_GIT_DESCRIBE "unknown"
#endif

namespace {

using hven::Index;
using hven::solvers::SqpStatus;
using hven::solvers::corpus::CorpusCell;
using hven::solvers::crossover::CellLegs;
using hven::solvers::crossover::CounterMargin;
using hven::solvers::crossover::IpmLegRow;
using hven::solvers::crossover::LegOptions;
using hven::solvers::crossover::SqpLegRow;

// The five artifact files, in the order the child writes their rows.
constexpr const char *kLegFiles[] = {"leg_a_ipm_only.csv", "leg_b_sqp_cold.csv",
                                     "leg_c_sqp_warm_core.csv", "leg_d_sqp_warm_polish.csv",
                                     "margins.csv"};
constexpr int kFileCount = 5;

// The status string a deadline-killed or failed cell carries in every file.
constexpr const char *kDnfStatus = "dnf_budget";
constexpr const char *kErrorStatus = "engine_error";

// =============================================================================
// Rows
// =============================================================================

std::string cell_prefix(const CorpusCell &cell) {
    return fmt::format("{},{},{},{}", cell.id, cell.n_nodes,
                       hven::solvers::corpus::to_string(cell.ctag),
                       hven::solvers::corpus::to_string(cell.start));
}

const char *flag_string(hven::ConvergenceFlags flag) {
    switch (flag) {
    case hven::ConvergenceFlags::CONVERGED:
        return "CONVERGED";
    case hven::ConvergenceFlags::ACCEPTABLE:
        return "ACCEPTABLE";
    case hven::ConvergenceFlags::NOTCONVERGED:
        return "NOTCONVERGED";
    case hven::ConvergenceFlags::DIVERGING:
        return "DIVERGING";
    case hven::ConvergenceFlags::SINGULAR_KKT:
        return "SINGULAR_KKT";
    }
    return "UNKNOWN";
}

std::string leg_a_row(const CellLegs &legs) {
    const IpmLegRow &a = legs.a;
    return fmt::format("{},{},{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},{:.6f}\n",
                       cell_prefix(*legs.cell), legs.n, legs.me, legs.mi, flag_string(a.flag),
                       a.iters, a.analyses, a.factorizations, a.solves, a.f, a.kkt_inf, a.econ_inf,
                       a.icon_inf, a.barr_inf, a.export_has_polish ? 1 : 0, a.wall_s);
}

std::string sqp_row(const CellLegs &legs, const SqpLegRow &r) {
    return fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{:.6f}\n",
        cell_prefix(*legs.cell), legs.n, legs.me, legs.mi, hven::solvers::to_string(r.status),
        hven::solvers::to_string(r.start_level), r.major_iters, r.qp_minor_iters, r.factorizations,
        r.symbolic_analyses, r.ip_activity_inferred, r.seeded_clamped, r.f, r.kkt_residual,
        r.stationarity, r.feasibility, r.complementarity, r.wall_s);
}

std::string margin_cell(const CounterMargin &m) {
    if (!m.defined) {
        return "absent,absent,absent";
    }
    return fmt::format("{},{},{}", m.majors, m.qp_minors, m.factorizations);
}

std::string margins_row(const CellLegs &legs) {
    const CounterMargin c = margin_against_cold(legs.b, legs.c);
    const CounterMargin d = margin_against_cold(legs.b, legs.d);
    return fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{}\n", cell_prefix(*legs.cell), legs.n,
        hven::solvers::to_string(legs.b.status), legs.b.major_iters, legs.b.qp_minor_iters,
        legs.b.factorizations, hven::solvers::to_string(legs.c.status),
        hven::solvers::to_string(legs.d.status), margin_cell(c), margin_cell(d),
        flag_string(legs.a.flag), legs.a.export_has_polish ? 1 : 0, legs.legs_cd_identical ? 1 : 0);
}

// A cell that never produced an answer: -1 in every counter column, the phase
// in the status column. ABSENT, not zero -- the same convention every other
// absent column in this project's CSVs uses.
std::string absent_leg_a(const CorpusCell &cell, const char *status) {
    return fmt::format("{},-1,-1,-1,{},-1,-1,-1,-1,-1.0,-1.0,-1.0,-1.0,-1.0,-1,-1.0\n",
                       cell_prefix(cell), status);
}

std::string absent_sqp(const CorpusCell &cell, const char *status) {
    return fmt::format("{},-1,-1,-1,{},unknown,-1,-1,-1,-1,-1,-1,-1.0,-1.0,-1.0,-1.0,-1.0,-1.0\n",
                       cell_prefix(cell), status);
}

std::string absent_margins(const CorpusCell &cell, const char *status) {
    return fmt::format(
        "{},-1,{},-1,-1,-1,{},{},absent,absent,absent,absent,absent,absent,{},-1,-1\n",
        cell_prefix(cell), status, status, status, status);
}

void write_headers(std::ostream &os, int which) {
    static constexpr const char *kCommon = "cell_id,n_nodes,window,taxonomy,n,me,mi";
    switch (which) {
    case 0:
        os << kCommon
           << ",flag,ipm_iters,kkt_analyses,kkt_factorizations,kkt_solves,f,kkt_inf,econ_inf,"
              "icon_inf,barr_inf,export_has_polish,wall_s\n";
        return;
    case 4:
        os << "cell_id,n_nodes,window,taxonomy,n,cold_status,cold_majors,cold_qp_minors,"
              "cold_factorizations,warm_core_status,warm_polish_status,"
              "core_majors_saved,core_qp_minors_saved,core_factorizations_saved,"
              "polish_majors_saved,polish_qp_minors_saved,polish_factorizations_saved,"
              "ipm_flag,export_has_polish,legs_cd_identical\n";
        return;
    default:
        os << kCommon
           << ",status,start_level,majors,qp_minors,factorizations,symbolic_analyses,"
              "ip_activity_inferred,seeded_clamped,f,kkt_residual,stationarity,feasibility,"
              "complementarity,wall_s\n";
        return;
    }
}

// =============================================================================
// Provenance (CLAUDE.md §7: toolchain, hardware, date, commit)
// =============================================================================

void write_provenance(std::ostream &os, int argc, char **argv, const char *leg_name,
                      double deadline_s) {
    std::string invocation;
    for (int i = 0; i < argc; ++i) {
        invocation += (i == 0 ? "" : " ");
        invocation += argv[i];
    }
    const char *mkl = std::getenv("MKL_NUM_THREADS");
    char host[256] = {0};
    if (::gethostname(host, sizeof(host) - 1) != 0) {
        host[0] = '\0';
    }
    const std::time_t now = std::time(nullptr);
    char stamp[64] = {0};
    std::tm utc{};
    if (::gmtime_r(&now, &utc) != nullptr) {
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc);
    }
    os << "# hven_sqp_crossover provenance (M5 W5)\n";
    os << fmt::format("# leg: {}\n", leg_name);
    os << fmt::format("# binary: {}\n", HVEN_SQP_CROSSOVER_GIT_DESCRIBE);
    os << fmt::format("# invocation: {}\n", invocation);
    os << fmt::format("# MKL_NUM_THREADS: {}\n", mkl == nullptr ? "<unset>" : mkl);
    os << fmt::format("# host: {}\n", host[0] == '\0' ? "<unknown>" : host);
    os << fmt::format("# generated: {}\n", stamp[0] == '\0' ? "<unknown>" : stamp);
    os << fmt::format("# per-cell wall deadline: {:.0f}s (SIGKILL; a killed cell is a "
                      "'{}' row, not a missing one)\n",
                      deadline_s, kDnfStatus);
    os << "# execution: SERIAL -- one cell at a time, one solve at a time, no co-run arm.\n";
    os << "# asserted currency: counters (majors, qp minors, factorizations). Every wall\n";
    os << "#   column here is INFORMATIONAL and is never quoted as a measurement.\n";
}

// =============================================================================
// The child: one cell, four legs, five rows
// =============================================================================

// Each row is written and FLUSHED the moment its leg finishes, tagged with the
// index of the file it belongs in. A child that is SIGKILLed part way through
// therefore leaves the legs that did finish behind, and the parent fills only
// the rest as absent -- which is why the legs run (a), (c), (d), (b), with the
// expensive cold leg last.
int run_child(const CorpusCell &cell, const LegOptions &opts, const std::string &sidecar) {
    try {
        std::ofstream out(sidecar);
        if (!out) {
            return 1;
        }
        const auto sink = [&out](const CellLegs &legs, hven::solvers::crossover::LegStage stage) {
            switch (stage) {
            case hven::solvers::crossover::LegStage::kIpm:
                out << "0\t" << leg_a_row(legs);
                break;
            case hven::solvers::crossover::LegStage::kWarmCore:
                out << "2\t" << sqp_row(legs, legs.c);
                break;
            case hven::solvers::crossover::LegStage::kWarmPolish:
                out << "3\t" << sqp_row(legs, legs.d);
                break;
            case hven::solvers::crossover::LegStage::kCold:
                out << "1\t" << sqp_row(legs, legs.b);
                break;
            case hven::solvers::crossover::LegStage::kMargins:
                out << "4\t" << margins_row(legs);
                break;
            }
            out.flush();
        };
        (void)hven::solvers::crossover::run_cell_legs(cell, opts, sink);
        return out ? 0 : 1;
    } catch (const std::exception &error) {
        std::ofstream err(sidecar + ".error");
        err << error.what() << "\n";
        return 1;
    }
}

struct CellOutcome {
    bool ok = false; // every leg finished
    bool timed_out = false;
    std::string error;
    // Per file index, the row the child wrote, or empty when that leg never
    // finished. A partially-filled outcome is still written: the legs that ran
    // are measurements, and only the ones that did not are absent.
    std::vector<std::string> rows = std::vector<std::string>(kFileCount);
};

// Reads whatever rows the child managed to flush. Each line is
// "<file index>\t<row>"; anything else is ignored rather than trusted.
void read_sidecar(const std::string &sidecar, CellOutcome &outcome) {
    std::ifstream in(sidecar);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos) {
            continue;
        }
        const int index = std::atoi(line.substr(0, tab).c_str());
        if (index < 0 || index >= kFileCount) {
            continue;
        }
        outcome.rows[static_cast<std::size_t>(index)] = line.substr(tab + 1) + "\n";
    }
    ::unlink(sidecar.c_str());
}

CellOutcome run_cell_with_deadline(const CorpusCell &cell, const LegOptions &opts,
                                   double deadline_s) {
    CellOutcome outcome;
    const std::string sidecar = fmt::format("/tmp/hven_crossover_{}_{}.rows", ::getpid(), cell.id);
    ::unlink(sidecar.c_str());
    ::unlink((sidecar + ".error").c_str());

    // FORK WITHOUT EXEC. The parent never solves -- every solve in this binary
    // happens in a child -- so there is no MKL thread state for the fork to
    // duplicate, and the child needs nothing forwarded because it inherits the
    // whole configuration already. The child writes only its own sidecar; the
    // parent's open CSV streams are never touched from the child side.
    const pid_t pid = ::fork();
    if (pid < 0) {
        outcome.error = fmt::format("fork failed: {}", std::strerror(errno));
        return outcome;
    }
    if (pid == 0) {
        ::_exit(run_child(cell, opts, sidecar));
    }

    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::duration<double>(deadline_s);
    int status = 0;
    for (;;) {
        const pid_t done = ::waitpid(pid, &status, WNOHANG);
        if (done == pid) {
            break;
        }
        if (done < 0) {
            outcome.error = fmt::format("waitpid failed: {}", std::strerror(errno));
            return outcome;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            ::kill(pid, SIGKILL);
            (void)::waitpid(pid, &status, 0);
            outcome.timed_out = true;
            read_sidecar(sidecar, outcome); // salvage the legs that did finish
            return outcome;
        }
        ::usleep(20000); // 20 ms, bench_corpus.cpp's own poll interval
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::ifstream err(sidecar + ".error");
        std::string what((std::istreambuf_iterator<char>(err)), std::istreambuf_iterator<char>());
        outcome.error = what.empty() ? "child exited nonzero with no message" : what;
        ::unlink((sidecar + ".error").c_str());
        read_sidecar(sidecar, outcome); // salvage the legs that did finish
        return outcome;
    }

    read_sidecar(sidecar, outcome);
    for (const std::string &row : outcome.rows) {
        if (row.empty()) {
            outcome.error = "child exited cleanly but did not write every leg";
            return outcome;
        }
    }
    outcome.ok = true;
    return outcome;
}

// =============================================================================
// CLI
// =============================================================================

void print_help() {
    fmt::print("hven_sqp_crossover -- the M5 W5 IPM -> SQP crossover measurement legs.\n"
               "\n"
               "Runs four legs per dual-bindable replay-corpus cell: (a) IPM-only, which is\n"
               "also the exporter, (b) SQP cold, (c) SQP warm from (a)'s export with the tag\n"
               "stripped, (d) SQP warm with the polish extension. Counters are the asserted\n"
               "currency; every wall column is informational.\n"
               "\n"
               "  --cells all|<id1,id2,...>   which cells to run; 'all' is every dual-bindable\n"
               "                              cell, in corpus order. A named cell that does not\n"
               "                              dual-bind is refused with its reason.\n"
               "  --out-dir <dir>             where the five CSVs are written. Required.\n"
               "  --deadline-seconds <s>      per-cell wall deadline, default 1800. A cell that\n"
               "                              outlives it is SIGKILLed and written as a\n"
               "                              'dnf_budget' row with -1 counters.\n"
               "  --ipm-max-iters <n>         interior-point iteration cap, default 200.\n"
               "  --list                      print every corpus cell, with its dual-bind\n"
               "                              verdict and (when refused) the reason.\n"
               "  --help, -h                  this text.\n"
               "\n"
               "MEASUREMENT DISCIPLINE (CLAUDE.md section 7). Run alone on the machine with\n"
               "MKL_NUM_THREADS=1 exported. This binary is serial by construction and offers no\n"
               "co-run arm: the W5 protocol declares none.\n");
}

void print_list() {
    fmt::print("{:<36} {:>7}  {:<9} {}\n", "cell_id", "n_nodes", "dual-bind", "reason if refused");
    for (const CorpusCell &cell : hven::solvers::corpus::all_cells()) {
        const std::string refusal = hven::solvers::crossover::dual_bind_refusal(cell);
        fmt::print("{:<36} {:>7}  {:<9} {}\n", cell.id, cell.n_nodes,
                   refusal.empty() ? "yes" : "no", refusal);
    }
}

struct Args {
    std::optional<std::string> cells;
    std::optional<std::string> out_dir;
    double deadline_s = 1800.0;
    int ipm_max_iters = 200;
    bool list = false;
    bool help = false;
};

Args parse_args(int argc, char **argv) {
    Args args;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto value = [&](const char *what) -> std::string {
            if (i + 1 >= argc) {
                throw std::invalid_argument(fmt::format("{} needs a value", what));
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            args.help = true;
        } else if (arg == "--list") {
            args.list = true;
        } else if (arg == "--cells") {
            args.cells = value("--cells");
        } else if (arg == "--out-dir") {
            args.out_dir = value("--out-dir");
        } else if (arg == "--deadline-seconds") {
            args.deadline_s = std::stod(value("--deadline-seconds"));
        } else if (arg == "--ipm-max-iters") {
            args.ipm_max_iters = std::stoi(value("--ipm-max-iters"));
        } else {
            throw std::invalid_argument(fmt::format("unknown argument '{}' (try --help)", arg));
        }
    }
    if (args.deadline_s <= 0.0) {
        throw std::invalid_argument("--deadline-seconds must be > 0");
    }
    if (args.ipm_max_iters < 1) {
        throw std::invalid_argument("--ipm-max-iters must be >= 1");
    }
    return args;
}

std::vector<const CorpusCell *> resolve_cells(const std::string &spec) {
    if (spec == "all") {
        return hven::solvers::crossover::dual_bindable_cells();
    }
    std::vector<const CorpusCell *> out;
    std::size_t start = 0;
    while (start <= spec.size()) {
        const std::size_t comma = spec.find(',', start);
        const std::string id =
            spec.substr(start, comma == std::string::npos ? std::string::npos : comma - start);
        if (!id.empty()) {
            const CorpusCell *cell = hven::solvers::corpus::find_cell(id);
            if (cell == nullptr) {
                throw std::invalid_argument(
                    fmt::format("'{}' is not a corpus cell id (try --list)", id));
            }
            const std::string refusal = hven::solvers::crossover::dual_bind_refusal(*cell);
            if (!refusal.empty()) {
                throw std::invalid_argument(
                    fmt::format("cell '{}' does not dual-bind: {}", id, refusal));
            }
            out.push_back(cell);
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1;
    }
    if (out.empty()) {
        throw std::invalid_argument("--cells resolved to no cells");
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Args args = parse_args(argc, argv);
        if (args.help) {
            print_help();
            return 0;
        }
        if (args.list) {
            print_list();
            return 0;
        }
        if (!args.cells || !args.out_dir) {
            throw std::invalid_argument("--cells and --out-dir are both required (try --help)");
        }

        const std::vector<const CorpusCell *> cells = resolve_cells(*args.cells);
        LegOptions opts;
        opts.ipm_max_iters = args.ipm_max_iters;

        std::vector<std::ofstream> files;
        files.reserve(kFileCount);
        for (int i = 0; i < kFileCount; ++i) {
            const std::string path = fmt::format("{}/{}", *args.out_dir, kLegFiles[i]);
            files.emplace_back(path);
            if (!files.back()) {
                throw std::runtime_error(fmt::format("cannot open '{}' for writing", path));
            }
            write_provenance(files.back(), argc, argv, kLegFiles[i], args.deadline_s);
            write_headers(files.back(), i);
            files.back().flush();
        }

        int completed = 0;
        int dnf = 0;
        int errored = 0;
        for (const CorpusCell *cell : cells) {
            fmt::print("running {} (N={}, {}, {}, deadline {:.0f}s)...\n", cell->id, cell->n_nodes,
                       hven::solvers::corpus::to_string(cell->ctag),
                       hven::solvers::corpus::to_string(cell->start), args.deadline_s);
            std::fflush(stdout);
            const CellOutcome outcome = run_cell_with_deadline(*cell, opts, args.deadline_s);
            const char *status = outcome.timed_out ? kDnfStatus : kErrorStatus;
            if (outcome.ok) {
                ++completed;
            } else if (outcome.timed_out) {
                ++dnf;
                fmt::print("  DNF: outlived the {:.0f}s deadline\n", args.deadline_s);
            } else {
                ++errored;
                fmt::print("  ERROR: {}\n", outcome.error);
            }
            // A LEG THAT FINISHED IS A MEASUREMENT even when a later one did
            // not, so each file takes the child's own row where there is one
            // and an absent row only where there is not.
            for (int i = 0; i < kFileCount; ++i) {
                const std::string &row = outcome.rows[static_cast<std::size_t>(i)];
                if (!row.empty()) {
                    files[i] << row;
                } else if (i == 0) {
                    files[i] << absent_leg_a(*cell, status);
                } else if (i == 4) {
                    files[i] << absent_margins(*cell, status);
                } else {
                    files[i] << absent_sqp(*cell, status);
                }
            }
            // INCREMENTAL: one cell's rows, flushed, so a run that is
            // interrupted leaves a complete artifact of everything finished.
            for (std::ofstream &file : files) {
                file.flush();
            }
        }

        fmt::print("wrote {} cell(s) to {}: {} completed, {} dnf, {} errored\n", cells.size(),
                   *args.out_dir, completed, dnf, errored);
        return 0;
    } catch (const std::exception &error) {
        fmt::print(stderr, "hven_sqp_crossover: error: {}\n", error.what());
        return 1;
    }
}
