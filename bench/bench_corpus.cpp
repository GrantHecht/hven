// bench/bench_corpus.cpp — PHASE-7 TASK 1: the replay corpus runner. Thin CLI
// glue over bench/corpus_cells.h's engine interface -- every actual cell
// spec, tag, the gate evaluator and the walk engine's implementation live
// there (and are shared with tests/test_corpus_cells.cpp, the SNOPT-gate
// precedent bench/CMakeLists.txt already follows for exactly this reason: one
// implementation, not a bench copy and a test copy that could drift).
//
// FIX ROUND 1 moved the GATE POPULATIONS out of this file and into
// corpus_cells.h's `evaluate_gates(outcomes)`. What remains here is process
// orchestration and I/O: nothing that decides a verdict.
//
// NOT ctest-registered, same reason every other bench binary in this project
// is not (hven_sqp_bench, hven_sqp_f7_cold, hven_sqp_tau_bar_sweep_probe):
// it is a measurement instrument, and the baseline sweep this task commits
// (docs/notes/data/2026-08-06-corpus/walk_baseline.csv) touches N = 20000
// cells that run for minutes to hours. Its correctness gate is
// tests/test_corpus_cells.cpp, which drives this binary as a subprocess for
// everything a linked-in test cannot reach.
//
// T6: every rejection throws std::invalid_argument with the usage text
// folded into the message; main() catches it, prints to stderr, exits 1.
//
// =============================================================================
// THE PER-PHASE WALL-CLOCK DEADLINE (controller intervention, Phase-7 Task 1;
// split into two phases in fix round 1).
// =============================================================================
//
// corpus_cells.h's own minor-iteration budget (kMinorBudget) bounds ONE
// solve's own internal cost, but a kCorrupted/kFullWarm CELL pays TWO
// independent solves (a setup hop and the reported target hop), each
// separately eligible to spend up to that budget -- so it does not tightly
// bound a CELL's wall time, and three cells were measured still running 30+
// minutes into what would have been another multi-hour run with only that
// mechanism in place. A WALL deadline is enforced HERE, in the runner that
// owns process lifetime, because that is what "wall-clock" means and nothing
// inside a blocking SqpDriver::solve() call gives a safe mid-solve
// interruption point (this project's engines are not written to be
// preempted -- killing a thread mid-factorization is not a "DNF with
// counters", it is undefined state).
//
// THE MECHANISM: self-exec + fork/waitpid, not a new dependency. For
// `--engine walk` this binary re-invokes ITSELF (argv[0]) per cell in a
// hidden internal mode (`--internal-run-one <id> --internal-out <path>`,
// undocumented in --help on purpose -- it is not a stable CLI surface, only
// an implementation detail of the parent's own deadline enforcement) that
// runs exactly ONE cell and writes its row to a file.
//
// TWO PHASES, TWO DEADLINES (fix round 1, I1). The child creates a marker
// file (`<out>.setup`) at the instant its SETUP work -- model construction
// plus, for kCorrupted/kFullWarm, the cold solve at p0 -- is done and the
// cell's DESIGNATED solve is about to start; corpus_cells.h's `run_cell`
// takes the callback that writes it. The parent polls waitpid(WNOHANG) and
// that marker every 20 ms:
//
//   * marker absent, deadline passed -> SIGKILL, row is `dnf_setup`. The
//     budget was consumed BUILDING the hand-off, and the CSV says so.
//   * marker appears                 -> the clock RESTARTS for the target
//                                       solve, which is the only work the
//                                       row reports.
//   * marker present, deadline passed-> SIGKILL, row is `dnf_budget`.
//
// Before this split, a wide-window kCorrupted/kFullWarm cell at N >= 5000
// reported `dnf_budget` under its own taxonomy when what actually exhausted
// the budget was its COLD setup solve -- six of the first baseline's fourteen
// DNF rows were misattributed that way, and a Task-6 reader comparing "walk
// DNF vs SSN n factorizations" on them would have been comparing different
// work.
//
// A DNF row: `status` is the literal string `dnf_setup`/`dnf_budget` (a
// CSV-layer marker, not a new SqpStatus value -- no library header touched),
// every counter column `-1` -- ABSENT BY DESIGN, because nothing safe was
// measured past the kill -- and `wall_s` the enforced deadline. It is NOT an
// absence for scoring purposes: corpus_cells.h's pre-registration block P3
// charges it as the worst case in every gate.
//
// TASK 6: `--engine ssn` IS WRAPPED IN THIS MACHINERY TOO, and the reasoning
// that used to exclude it has inverted. Task 1 left it out because the name
// threw in-process and forking a clean throw would have produced an opaque
// nonzero child exit to re-decode; now that the SSN kernel really runs, the
// unbounded cell it could produce is exactly what the deadline exists to
// prevent, and an SSN arm scored against a BUDGETED walk arm while itself
// UNBUDGETED would not be a comparison. Same tiers, same two phases, same DNF
// semantics, one code path for both engines.
//
// TASK 6 ALSO WIDENED THE ROW: seventeen columns are appended to Task 1's
// fourteen -- the model-level KKT gate (instrument requirement 1, Task-5
// re-review NF-2) and the SSN kernel's own counters, including the two this
// task added for the refinement's cost and its dual signs (NF-1). Task 1's
// fourteen are untouched and in Task 1's order, so the committed walk baseline
// still reads through the same reader and a re-swept walk arm diffs against it
// column for column. See write_header's own note.

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <fmt/format.h>

#include "bench_cli.h"
#include "corpus_cells.h"

#ifndef HVEN_SQP_CORPUS_GIT_DESCRIBE
#define HVEN_SQP_CORPUS_GIT_DESCRIBE "unknown"
#endif

namespace {

using hven::Vec;
using hven::solvers::QpProblem;
using hven::solvers::corpus::all_cells;
using hven::solvers::corpus::budget_table_hash;
using hven::solvers::corpus::CorpusCell;
using hven::solvers::corpus::CorpusOutcome;
using hven::solvers::corpus::CorpusRow;
using hven::solvers::corpus::dnf_phase_status_string;
using hven::solvers::corpus::DnfPhase;
using hven::solvers::corpus::dual_sign_would_fail;
using hven::solvers::corpus::evaluate_gates;
using hven::solvers::corpus::find_cell;
using hven::solvers::corpus::kkt_gate_verdict;
using hven::solvers::corpus::KktVerdict;
using hven::solvers::corpus::run_cell;
using hven::solvers::corpus::to_string;
using hven::solvers::corpus::detail::first_qp_for_cell;
using hven::solvers::corpus::detail::wall_budget_for_cell;

// The measurement-arm levers, named once. See corpus_cells.h's EngineConfig.
using EngineLevers = hven::solvers::corpus::detail::EngineConfig;
// Task 6b Phase B's three iteration-shape rules (sqp_types.h), named here for
// the same reason every other type above is: this file spells one namespace.
using hven::solvers::SsnHintRule;
using hven::solvers::SsnInfeasibilityRule;
using hven::solvers::SsnSigmaRule;

constexpr const char *kUsage =
    "usage: hven_sqp_corpus --engine walk|ssn --cells all|<id1,id2,...> --csv <path> "
    "[--score-gates]\n"
    "       hven_sqp_corpus --from-csv <path1[,path2,...]> [--csv <merged>] [--score-gates]\n"
    "       hven_sqp_corpus --dump-qp <cell> --dump-qp-out <path>\n"
    "       hven_sqp_corpus --list\n"
    "       hven_sqp_corpus --help\n"
    "\n"
    "  --dump-qp <cell>  PHASE-7 TASK 2 (PIQP oracle). Build the cell's OWN\n"
    "                    designated (target) hop's FIRST QP subproblem --\n"
    "                    corpus_cells.h's first_qp_for_cell, the same\n"
    "                    build_subproblem call SqpDriver::solve's own first\n"
    "                    iteration would make -- and write it to --dump-qp-out\n"
    "                    in the triplet text format documented at the top of\n"
    "                    write_qp_dump below. SOLVES NOTHING of the designated\n"
    "                    hop itself; kCorrupted/kFullWarm still pay the real\n"
    "                    setup hop (a genuine cold solve at p0) because that is\n"
    "                    what the designated hop's (x, lambda_e, lambda_i) come\n"
    "                    from. NOT WRAPPED in this file's own wall-deadline\n"
    "                    machinery (see that section's banner) -- if a caller\n"
    "                    forgets a bound, this invocation is UNBOUNDED (fix\n"
    "                    round 1, review M5, stated plainly rather than only\n"
    "                    implied): wrap it in a shell-level `timeout`, e.g. at\n"
    "                    corpus_cells.h's own wall_budget_for_cell(cell)\n"
    "                    seconds (this file's --list prints that figure per\n"
    "                    cell). A `timeout` kill produces no row and no\n"
    "                    dnf_setup/dnf_budget attribution -- unlike an\n"
    "                    `--engine walk` DNF, it is simply silence.\n"
    "  --dump-qp-out <path>  required with --dump-qp; the output file.\n"
    "\n"
    "  --engine ARM      walk | ssn. walk replays through the ordinary SqpDriver\n"
    "                    (the only engine that exists today) under a PER-PHASE\n"
    "                    WALL DEADLINE (see this file's own banner; the deadline\n"
    "                    itself is corpus_cells.h's wall_budget_for_cell, whose\n"
    "                    band is re-derived from this repository's own committed\n"
    "                    uncontended runtimes at 3x margin). A cell that does not\n"
    "                    finish in time is reported as a DNF row -- status\n"
    "                    dnf_setup (the budget went on BUILDING the start) or\n"
    "                    dnf_budget (it went on the reported solve), counters -1\n"
    "                    -- never a hang. ssn replays the SAME cells through the\n"
    "                    semismooth-Newton kernel (SqpOptions::qp_mode = kSsn)\n"
    "                    under the SAME deadlines: the two arms differ in that\n"
    "                    one field and nothing else.\n"
    "  --ssn-prox-carry  MEASUREMENT ARM. Set SqpOptions::ssn_prox_carry (a real,\n"
    "                    shipped option that ships OFF -- see sqp_types.h for the\n"
    "                    sweep that ruled it off). Stamped into the CSV's own\n"
    "                    provenance header, so an arm can never be mistaken for a\n"
    "                    default-configuration run.\n"
    "  --ssn-certify-from-face   MEASUREMENT ARM (Task 6b Phase B, R5). Set\n"
    "                    SqpOptions::ssn_certify_from_face -- read the certifying\n"
    "                    exit's second-order evidence off the tier-3 face solve\n"
    "                    instead of paying a dedicated verification factorization.\n"
    "  --ssn-sigma-rule R        MEASUREMENT ARM (R1). ladder (default) |\n"
    "                    residual-armed | residual-always.\n"
    "  --ssn-hint-rule R         MEASUREMENT ARM (R2). exempt (default) | watchdog.\n"
    "  --ssn-infeasibility-rule R  MEASUREMENT ARM (R4). symptoms (default) | farkas.\n"
    "                    Every one of the four is a REAL option surface shipping at\n"
    "                    the shipped iteration's own value, and every non-default is\n"
    "                    stamped into the CSV's provenance header.\n"
    "  --cells SPEC      'all' (every corpus_cells.h census cell, 57 of them --\n"
    "                    see that file for the exact list) or a comma-separated\n"
    "                    list of cell ids (bench/corpus_cells.h's own ids, e.g.\n"
    "                    f7_n1000_bound_neutral).\n"
    "  --csv <path>      output file; provenance header, column header, one row\n"
    "                    per cell, written INCREMENTALLY as each cell finishes.\n"
    "  --from-csv SPEC   comma-separated list of ALREADY-WRITTEN corpus CSVs to\n"
    "                    read instead of running anything. Solves nothing and\n"
    "                    touches no engine. With --csv it MERGES them into one\n"
    "                    artifact in census order (the committed baseline is\n"
    "                    produced exactly this way); with --score-gates it\n"
    "                    re-scores a committed artifact offline.\n"
    "  --score-gates     ALSO compute and print the pre-registered gates (G1-G4,\n"
    "                    docs/superpowers/specs/2026-08-05-phase-7-design.md\n"
    "                    section 1) over these rows. DNF rows are SCORED, not\n"
    "                    skipped (corpus_cells.h's pre-registration block P3), and\n"
    "                    so are WRONG-ANSWER rows -- a row claiming kOptimal that\n"
    "                    fails the model-level KKT gate (W5). G1/G2 print BOTH\n"
    "                    kCorrupted readings, per corpus-design.md section 5.1's\n"
    "                    requirement on Task 6. Still PRINTED ONLY: the exit code\n"
    "                    never reflects pass/fail -- the asserted verdicts live in\n"
    "                    tests/test_scale_problems.cpp.\n"
    "  --list            print every census cell's id/tags and exit 0; touches\n"
    "                    neither --engine/--cells/--csv nor the solver.\n"
    "  --score-model-surface   TASK 4 (M4-Task5 plan) census hook. DEFAULT OFF.\n"
    "                    Requires --score-model-surface-out. Only meaningful with\n"
    "                    --engine/--cells/--csv (a real sweep): for each row this\n"
    "                    solve claimed kOptimal on, also scores the SAME returned\n"
    "                    point through bench/model_surface_kkt.h's engine-\n"
    "                    independent scorer and writes cell_id, the three recorded\n"
    "                    residuals, the three scorer residuals, and a\n"
    "                    verdict-equal boolean (both read through corpus_cells.h's\n"
    "                    own kkt_gate_verdict) to --score-model-surface-out. Never\n"
    "                    touches the main --csv artifact's own columns.\n"
    "  --score-model-surface-out <path>   required with --score-model-surface;\n"
    "                    the census-hook output path (e.g. wgate_scorer.csv).\n"
    "  --help            print this text and exit 0.\n"
    "\n"
    "CSV COLUMNS (one row per cell, in this order):\n"
    "  cell_id,family,n_nodes,window,taxonomy,degenerate,status,factorizations,\n"
    "  qp_minors,escapes,qp_subproblems,qp_fact_per_qp,kkt_residual,wall_s,\n"
    "  kkt_verdict,kkt_stationarity,kkt_primal,kkt_dual_sign,kkt_complementarity,\n"
    "  dual_scale,x_scale,neg_ineq_duals,ssn_iters,ssn_bulk_flips,ssn_backtracks,\n"
    "  ssn_prox_updates,ssn_uncertain_peak,ssn_refinements,ssn_refine_refused,\n"
    "  ssn_refine_facts,ssn_refine_neg_duals\n"
    "`qp_fact_per_qp` is the SEMICOLON-separated per-QP-subproblem numeric\n"
    "factorization count (SqpIterate::qp_factorizations), in history order --\n"
    "the quantity G1/G2 are pre-registered on. A row with status=dnf_setup or\n"
    "dnf_budget hit its wall deadline; every counter column on that row is -1\n"
    "(absent by design, not zero -- see this file's own banner), and wall_s is\n"
    "the DEADLINE that was enforced, not a measurement.\n";

[[noreturn]] void throw_usage(const std::string &detail) {
    hven::solvers::bench_cli::throw_usage(kUsage, detail);
}

// T6 with CONTEXT. std::stoi's own message is the word "stoi"; a malformed
// field in a multi-hour sweep's artifact deserves to name the file, the cell
// and the column it came from.
int parse_int_field(const std::string &what, const std::string &value) {
    try {
        std::size_t pos = 0;
        const int parsed = std::stoi(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw std::invalid_argument(
            fmt::format("hven_sqp_corpus: {}: '{}' is not an integer", what, value));
    }
}

double parse_double_field(const std::string &what, const std::string &value) {
    try {
        std::size_t pos = 0;
        const double parsed = std::stod(value, &pos);
        if (pos != value.size()) {
            throw std::invalid_argument("trailing characters");
        }
        return parsed;
    } catch (const std::exception &) {
        throw std::invalid_argument(
            fmt::format("hven_sqp_corpus: {}: '{}' is not a number", what, value));
    }
}

struct Args {
    bool help = false;
    bool list = false;
    bool score_gates = false;
    std::optional<std::string> engine;
    std::optional<std::string> cells;
    std::optional<std::string> csv;
    std::optional<std::string> from_csv;
    std::optional<std::string> dump_qp;
    std::optional<std::string> dump_qp_out;
    // Hidden internal mode -- see this file's own banner. Not part of the
    // documented CLI surface; a caller wanting the corpus replayed uses
    // --engine/--cells/--csv, never these three directly.
    std::optional<std::string> internal_run_one;
    std::optional<std::string> internal_out;
    // Hidden TEST-ONLY overrides of the two phase budgets in THIS invocation,
    // so tests/test_corpus_cells.cpp can force each DNF path deterministically
    // (kill during setup vs kill during the reported solve) without waiting
    // out a real 900 s+ deadline. Never documented in --help; a production
    // invocation never passes them -- and if one does, the runner says so
    // LOUDLY on stderr and stamps the value into the CSV's own provenance
    // header, so a forced budget can never silently masquerade as the
    // committed band.
    std::optional<double> internal_force_setup_budget_s;
    std::optional<double> internal_force_solve_budget_s;
    // TASK 6 MEASUREMENT ARM. Sets SqpOptions::ssn_prox_carry for every solve
    // in this invocation. A REAL, SHIPPED product option (sqp_types.h), not a
    // hidden test lever -- but it ships OFF (Task 5's corrected sweep costs
    // more on 13 of 23 rows), so a run that passes it is a measurement arm and
    // the CSV's provenance header says so.
    bool ssn_prox_carry = false;
    // TASK 6b PHASE B MEASUREMENT ARMS (R5/R1/R2/R4). Same discipline as
    // ssn_prox_carry above: real option surfaces, shipped defaults, and any
    // non-default stamped into the artifact's own provenance header.
    bool ssn_certify_from_face = false;
    SsnSigmaRule ssn_sigma_rule = SsnSigmaRule::kLadder;
    SsnHintRule ssn_hint_rule = SsnHintRule::kIterationZeroFree;
    SsnInfeasibilityRule ssn_infeasibility_rule = SsnInfeasibilityRule::kSymptoms;
    // Hidden TEST-ONLY lever. Makes the CHILD throw immediately after it
    // signals setup-complete, so tests/test_corpus_cells.cpp can drive the
    // PARENT's engine_error path deterministically. It exists because the
    // census's own throwing cells (the four kSsn rows of Task 6's battery) are
    // N >= 2000 cells that run for minutes -- far too heavy for ctest -- and a
    // scoring path with no test is exactly what fix round 1 of Task 1 was
    // written to stop. Warns on stderr and is stamped into the provenance
    // header, like every other hidden lever here.
    bool internal_force_child_throw = false;
    // Hidden TEST-ONLY lever, the SIGNAL counterpart of the one above. Makes
    // the child die by SIGABRT rather than by a caught exception, so
    // tests/test_corpus_cells.cpp can drive the parent's OTHER branch: "a
    // signal, or any other exit code, is a runner failure, never a
    // measurement". Without it that rule is unfixturable -- the deadline path
    // SIGKILLs the child but returns its DNF outcome before ever inspecting a
    // signal status, so nothing else in the suite reaches the branch.
    bool internal_force_child_abort = false;
    // TASK 4 (M4-Task5 plan): the model-surface census hook's opt-in flag and
    // its output path. DEFAULT OFF, and off means genuinely off -- see
    // corpus_cells.h's EngineConfig::score_model_surface and `timed_row` for
    // where the guard actually lives; this flag only decides whether that
    // guard is ever set to true. Paired with `--score-model-surface-out`
    // exactly as `--dump-qp`/`--dump-qp-out` are: required together, checked
    // in main() below.
    bool score_model_surface = false;
    std::optional<std::string> score_model_surface_out;
};

Args parse_args(int argc, char **argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next_value = [&](const std::string &flag) -> std::string {
            if (i + 1 >= argc) {
                throw_usage(fmt::format("{}: missing value", flag));
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            a.help = true;
        } else if (arg == "--list") {
            a.list = true;
        } else if (arg == "--score-gates") {
            a.score_gates = true;
        } else if (arg == "--score-model-surface") {
            a.score_model_surface = true;
        } else if (arg == "--score-model-surface-out") {
            a.score_model_surface_out = next_value(arg);
        } else if (arg == "--engine") {
            const std::string v = next_value(arg);
            if (v != "walk" && v != "ssn") {
                throw_usage(fmt::format("--engine: '{}' is not one of walk|ssn", v));
            }
            a.engine = v;
        } else if (arg == "--cells") {
            a.cells = next_value(arg);
        } else if (arg == "--csv") {
            a.csv = next_value(arg);
        } else if (arg == "--from-csv") {
            a.from_csv = next_value(arg);
        } else if (arg == "--dump-qp") {
            a.dump_qp = next_value(arg);
        } else if (arg == "--dump-qp-out") {
            a.dump_qp_out = next_value(arg);
        } else if (arg == "--internal-run-one") {
            a.internal_run_one = next_value(arg);
        } else if (arg == "--internal-out") {
            a.internal_out = next_value(arg);
        } else if (arg == "--internal-force-wall-budget-seconds") {
            // Legacy spelling, kept so an existing invocation keeps working:
            // forces BOTH phases.
            const double v = hven::solvers::bench_cli::parse_double(kUsage, arg, next_value(arg));
            a.internal_force_setup_budget_s = v;
            a.internal_force_solve_budget_s = v;
        } else if (arg == "--internal-force-setup-budget-seconds") {
            a.internal_force_setup_budget_s =
                hven::solvers::bench_cli::parse_double(kUsage, arg, next_value(arg));
        } else if (arg == "--internal-force-solve-budget-seconds") {
            a.internal_force_solve_budget_s =
                hven::solvers::bench_cli::parse_double(kUsage, arg, next_value(arg));
        } else if (arg == "--ssn-prox-carry") {
            a.ssn_prox_carry = true;
        } else if (arg == "--ssn-certify-from-face") {
            a.ssn_certify_from_face = true;
        } else if (arg == "--ssn-sigma-rule") {
            const std::string v = next_value(arg);
            if (v == "ladder") {
                a.ssn_sigma_rule = SsnSigmaRule::kLadder;
            } else if (v == "residual-armed") {
                a.ssn_sigma_rule = SsnSigmaRule::kResidualArmed;
            } else if (v == "residual-always") {
                a.ssn_sigma_rule = SsnSigmaRule::kResidualAlways;
            } else {
                throw_usage(fmt::format(
                    "--ssn-sigma-rule: '{}' is not one of ladder|residual-armed|residual-always",
                    v));
            }
        } else if (arg == "--ssn-hint-rule") {
            const std::string v = next_value(arg);
            if (v == "exempt") {
                a.ssn_hint_rule = SsnHintRule::kIterationZeroFree;
            } else if (v == "watchdog") {
                a.ssn_hint_rule = SsnHintRule::kWatchdog;
            } else {
                throw_usage(fmt::format("--ssn-hint-rule: '{}' is not one of exempt|watchdog", v));
            }
        } else if (arg == "--ssn-infeasibility-rule") {
            const std::string v = next_value(arg);
            if (v == "symptoms") {
                a.ssn_infeasibility_rule = SsnInfeasibilityRule::kSymptoms;
            } else if (v == "farkas") {
                a.ssn_infeasibility_rule = SsnInfeasibilityRule::kFarkasGated;
            } else {
                throw_usage(
                    fmt::format("--ssn-infeasibility-rule: '{}' is not one of symptoms|farkas", v));
            }
        } else if (arg == "--internal-force-child-throw") {
            a.internal_force_child_throw = true;
        } else if (arg == "--internal-force-child-abort") {
            a.internal_force_child_abort = true;
        } else {
            throw_usage(fmt::format("unknown flag: '{}'", arg));
        }
    }
    return a;
}

std::vector<std::string> split_on(const std::string &s, char sep) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, sep)) {
        if (!item.empty()) {
            out.push_back(item);
        }
    }
    return out;
}

std::vector<const CorpusCell *> resolve_cells(const std::string &spec) {
    std::vector<const CorpusCell *> out;
    if (spec == "all") {
        for (const CorpusCell &c : all_cells()) {
            out.push_back(&c);
        }
        return out;
    }
    for (const std::string &id : split_on(spec, ',')) {
        const CorpusCell *c = find_cell(id);
        if (c == nullptr) {
            throw_usage(fmt::format("--cells: unknown cell id '{}' (try --list)", id));
        }
        out.push_back(c);
    }
    if (out.empty()) {
        throw_usage("--cells: resolved to zero cells");
    }
    return out;
}

// =============================================================================
// THE ARTIFACT: provenance header, column header, rows.
// =============================================================================

// PHASE-7 TASK 6b (docket D6): SIX MORE COLUMNS, APPENDED FOR THE SAME REASON
// TASK 6's SEVENTEEN WERE. The escape-reason census
// (SqpCounters::ssn::ssn_escape_*) turns `escapes` from a total into a
// distribution, which is the only form the two carried G4 watch items can be
// tested in on a scale corpus.
//
// THE COMMITTED ARTIFACTS ARE NOT REGENERATED. `walk_baseline.csv` (14
// columns, Task 1) and the 2026-08-08 battery CSVs (31 columns, Task 6) are
// PINNED evidence; they keep reading and re-scoring through this same reader,
// which treats each tail as optional and reports the absent census as `-1`
// (ABSENT, not zero -- the same convention every other absent column uses).
// The provenance header of a NEW artifact records `schema: 37` so a reader
// never has to count commas to know which generation it holds.
constexpr int kTask6bColumns = 37;

// The reviewer's biggest cannot-verify on the first baseline was "did all 57
// rows come from ONE sweep under the final binary and the final budget
// table?" -- the CSV carried nothing to answer it. It does now: the binary's
// own git description (baked at configure time), the budget table's
// fingerprint (a function of the tier boundaries and values ALONE, see
// corpus_cells.h::budget_table_hash), the exact invocation, the thread
// setting every quoted wall depends on, and -- if any -- the hidden test
// levers that were in force. Every line is a `#` comment, the same convention
// bench_cli.h's solution dump uses, so an ordinary CSV reader skips them.
void write_provenance(std::ostream &os, int argc, char **argv, const EngineLevers &levers,
                      bool forced_throw, const std::optional<double> &forced_setup,
                      const std::optional<double> &forced_solve) {
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
    os << "# hven_sqp_corpus provenance\n";
    os << fmt::format("# binary: {}\n", HVEN_SQP_CORPUS_GIT_DESCRIBE);
    // PHASE-7 TASK 6b: the CSV SCHEMA GENERATION, stated rather than counted.
    // 14 = Task 1's baseline, 31 = Task 6's KKT gate + SSN counters, 37 = this
    // task's escape-reason census. The reader accepts all three; the committed
    // 14- and 31-column artifacts are pinned evidence and are NOT regenerated,
    // so an artifact without this line is one of those two older generations.
    os << fmt::format("# schema: {}\n", kTask6bColumns);
    os << fmt::format("# budget_table_hash: {:#018x}\n", budget_table_hash());
    os << fmt::format("# invocation: {}\n", invocation);
    os << fmt::format("# MKL_NUM_THREADS: {}\n", mkl == nullptr ? "<unset>" : mkl);
    os << fmt::format("# host: {}\n", host[0] == '\0' ? "<unknown>" : host);
    os << fmt::format("# generated: {}\n", stamp[0] == '\0' ? "<unknown>" : stamp);
    if (levers.ssn_prox_carry) {
        os << "# lever: ssn_prox_carry=true (MEASUREMENT ARM -- the shipped default is false)\n";
    }
    if (levers.ssn_certify_from_face) {
        os << "# lever: ssn_certify_from_face=true (MEASUREMENT ARM R5 -- shipped default false)\n";
    }
    if (levers.ssn_sigma_rule != SsnSigmaRule::kLadder) {
        os << fmt::format(
            "# lever: ssn_sigma_rule={} (MEASUREMENT ARM R1 -- shipped default ladder)\n",
            levers.ssn_sigma_rule == SsnSigmaRule::kResidualArmed ? "residual-armed"
                                                                  : "residual-always");
    }
    if (levers.ssn_hint_rule != SsnHintRule::kIterationZeroFree) {
        os << "# lever: ssn_hint_rule=watchdog (MEASUREMENT ARM R2 -- shipped default exempt)\n";
    }
    if (levers.ssn_infeasibility_rule != SsnInfeasibilityRule::kSymptoms) {
        os << "# lever: ssn_infeasibility_rule=farkas (MEASUREMENT ARM R4 -- shipped default "
              "symptoms)\n";
    }
    if (forced_throw) {
        os << "# WARNING forced_child_throw: these rows are TEST FIXTURES, not measurements\n";
    }
    if (forced_setup || forced_solve) {
        os << fmt::format("# WARNING forced_test_budgets: setup={} solve={} -- these rows were "
                          "NOT produced under the committed budget table\n",
                          forced_setup ? fmt::format("{:.9f}", *forced_setup) : "<band>",
                          forced_solve ? fmt::format("{:.9f}", *forced_solve) : "<band>");
    }
}

// THE FIRST FOURTEEN COLUMNS ARE TASK 1's, IN TASK 1's ORDER, BYTE FOR BYTE.
// Task 6's seventeen are APPENDED, never interleaved, for two reasons: the
// committed walk baseline (14 columns, no KKT check) must keep reading and
// re-scoring through this same reader, and a diff of a re-swept walk arm
// against that baseline must be a diff of the columns Task 1 measured rather
// than a re-layout. `read_outcomes_csv` treats the tail as optional exactly
// on that contract.
constexpr int kTask1Columns = 14;
constexpr int kAllColumns = 31;

void write_header(std::ostream &os) {
    os << "cell_id,family,n_nodes,window,taxonomy,degenerate,status,factorizations,qp_minors,"
          "escapes,qp_subproblems,qp_fact_per_qp,kkt_residual,wall_s,"
          "kkt_verdict,kkt_stationarity,kkt_primal,kkt_dual_sign,kkt_complementarity,dual_scale,"
          "x_scale,neg_ineq_duals,ssn_iters,ssn_bulk_flips,ssn_backtracks,ssn_prox_updates,"
          "ssn_uncertain_peak,ssn_refinements,ssn_refine_refused,ssn_refine_facts,"
          "ssn_refine_neg_duals,"
          "esc_budget,esc_singular,esc_no_contraction,esc_infeasible_suspect,esc_indefinite,"
          "esc_gate_refused\n";
}

std::string join_qp_factorizations(const std::vector<int> &v) {
    std::string out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        out += (i == 0 ? "" : ";");
        out += std::to_string(v[i]);
    }
    return out;
}

std::vector<int> parse_qp_factorizations(const std::string &what, const std::string &field) {
    std::vector<int> out;
    for (const std::string &tok : split_on(field, ';')) {
        out.push_back(parse_int_field(what, tok));
    }
    return out;
}

void write_outcome(std::ostream &os, const CorpusOutcome &out) {
    const CorpusCell &cell = *out.cell;
    if (out.no_answer()) {
        // Every Task-6 column on a DNF row is `-1`/`unchecked` for exactly the
        // reason every Task-1 counter column already is: nothing was safely
        // measured past the kill. ABSENT, not zero, and not "ok".
        os << fmt::format("{},{},{},{},{},{},{},-1,-1,-1,-1,,-1.0,{:.9f},"
                          "unchecked,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,"
                          "-1,-1,-1,-1,-1,-1\n",
                          cell.id, to_string(cell.family), cell.n_nodes, to_string(cell.ctag),
                          to_string(cell.start), cell.degenerate ? 1 : 0,
                          out.engine_error ? hven::solvers::corpus::kEngineErrorStatusString
                                           : dnf_phase_status_string(out.dnf_phase),
                          out.dnf_wall_s);
        return;
    }
    const CorpusRow &row = out.row;
    os << fmt::format("{},{},{},{},{},{},{},{},{},{},{},{},{:.9e},{:.9f},"
                      "{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},"
                      "{},{},{},{},{},{},{},{},{},"
                      "{},{},{},{},{},{}\n",
                      row.cell_id, to_string(cell.family), cell.n_nodes, to_string(cell.ctag),
                      to_string(cell.start), cell.degenerate ? 1 : 0, to_string(row.status),
                      row.factorizations, row.qp_minors, row.escapes, row.qp_factorizations.size(),
                      join_qp_factorizations(row.qp_factorizations), row.kkt_residual, row.wall_s,
                      to_string(kkt_gate_verdict(row)), row.kkt_stationarity, row.kkt_primal,
                      row.kkt_dual_sign, row.kkt_complementarity, row.dual_scale, row.x_scale,
                      row.neg_ineq_duals, row.ssn.ssn_iters, row.ssn.ssn_bulk_flips,
                      row.ssn.ssn_backtracks, row.ssn.ssn_prox_updates, row.ssn.ssn_uncertain_peak,
                      row.ssn.ssn_refinements, row.ssn.ssn_refine_refused,
                      row.ssn.ssn_refine_factorizations, row.ssn.ssn_refine_neg_duals,
                      row.ssn.ssn_escape_budget, row.ssn.ssn_escape_singular,
                      row.ssn.ssn_escape_no_contraction, row.ssn.ssn_escape_infeasible_suspect,
                      row.ssn.ssn_escape_indefinite, row.ssn.ssn_escape_gate_refused);
}

// =============================================================================
// PHASE-7 TASK 2 (PIQP oracle): --dump-qp's triplet text format.
// =============================================================================
//
// DENSE-BLOCK-FREE: H/Ae/Ai (which can be n x n / me x n / mi x n at n up to
// 5 * 20000 = 100000) are written as sparse (row, col, value) triplets, never
// as a dense 2-D block. g/lower/upper (length n) and be/bi (length me/mi) are
// 1-D vectors, not blocks, and are written one value per line in index order
// -- cheap even at n = 100000 (a few MB of text at worst), and unambiguous to
// re-read without a caller needing to reconstruct sparsity for a vector that
// was never sparse to begin with. `H` stores ONLY its upper triangle
// (row <= col), exactly QpProblem::validate's own convention (qp_problem.h)
// -- a reader (piqp_f7_driver.cpp) symmetrizes explicitly rather than assume
// the writer already did.
//
// Every count line names the exact number of following lines/triplets, so a
// reader never has to guess EOF; `END` closes the file so a truncated dump
// (an interrupted write) is detectable rather than silently read as valid.
// Values are printed at `{:.17g}` -- enough decimal digits to round-trip an
// IEEE double exactly (17 significant digits is the standard bound).
void write_qp_dump(std::ostream &os, const CorpusCell &cell, const QpProblem &qp) {
    os << "PIQP_QP_DUMP 1\n";
    os << fmt::format("# cell: {}\n", cell.id);
    os << fmt::format("# family: {}  n_nodes: {}  window: {}  taxonomy: {}\n",
                      to_string(cell.family), cell.n_nodes, to_string(cell.ctag),
                      to_string(cell.start));
    os << fmt::format("# binary: {}\n", HVEN_SQP_CORPUS_GIT_DESCRIBE);
    os << fmt::format("cell {}\n", cell.id);
    os << fmt::format("taxonomy {}\n", to_string(cell.start));
    os << fmt::format("window {}\n", to_string(cell.ctag));
    os << fmt::format("n_nodes {}\n", cell.n_nodes);
    os << fmt::format("n {}\n", qp.n());
    os << fmt::format("me {}\n", qp.me());
    os << fmt::format("mi {}\n", qp.mi());

    std::size_t h_nnz = 0;
    for (int k = 0; k < qp.H.outerSize(); ++k) {
        for (hven::SpMatRM::InnerIterator it(qp.H, k); it; ++it) {
            ++h_nnz;
        }
    }
    os << fmt::format("H_NNZ {}\n", h_nnz);
    for (int k = 0; k < qp.H.outerSize(); ++k) {
        for (hven::SpMatRM::InnerIterator it(qp.H, k); it; ++it) {
            os << fmt::format("{} {} {:.17g}\n", it.row(), it.col(), it.value());
        }
    }

    os << fmt::format("G_VEC {}\n", qp.g.size());
    for (hven::Index i = 0; i < qp.g.size(); ++i) {
        os << fmt::format("{:.17g}\n", qp.g(i));
    }

    auto dump_sparse = [&](const char *tag, const Eigen::SparseMatrix<double, Eigen::RowMajor> &A) {
        std::size_t nnz = 0;
        for (int k = 0; k < A.outerSize(); ++k) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(A, k); it; ++it) {
                ++nnz;
            }
        }
        os << fmt::format("{}_NNZ {}\n", tag, nnz);
        for (int k = 0; k < A.outerSize(); ++k) {
            for (Eigen::SparseMatrix<double, Eigen::RowMajor>::InnerIterator it(A, k); it; ++it) {
                os << fmt::format("{} {} {:.17g}\n", it.row(), it.col(), it.value());
            }
        }
    };
    auto dump_vec = [&](const char *tag, const Vec &v) {
        os << fmt::format("{}_VEC {}\n", tag, v.size());
        for (hven::Index i = 0; i < v.size(); ++i) {
            os << fmt::format("{:.17g}\n", v(i));
        }
    };

    dump_sparse("AE", qp.Ae);
    dump_vec("BE", qp.be);
    dump_sparse("AI", qp.Ai);
    dump_vec("BI", qp.bi);
    dump_vec("LOWER", qp.lower);
    dump_vec("UPPER", qp.upper);
    os << "END\n";
}

void print_list() {
    for (const CorpusCell &c : all_cells()) {
        fmt::print("{:32s}  N={:<6}  p0={:<5.2f} p={:<5.2f}  {:5s}  {:9s}  degenerate={}  "
                   "budget={:.0f}s/phase\n",
                   c.id, c.n_nodes, c.p0, c.p, to_string(c.ctag), to_string(c.start),
                   c.degenerate ? "true" : "false", wall_budget_for_cell(c));
    }
}

hven::solvers::SqpStatus parse_status(const std::string &s) {
    if (s == "Optimal") {
        return hven::solvers::SqpStatus::kOptimal;
    }
    if (s == "MaxIter") {
        return hven::solvers::SqpStatus::kMaxIter;
    }
    if (s == "Infeasible") {
        return hven::solvers::SqpStatus::kInfeasible;
    }
    if (s == "NumericalError") {
        return hven::solvers::SqpStatus::kNumericalError;
    }
    if (s == "BudgetExhausted") {
        return hven::solvers::SqpStatus::kBudgetExhausted;
    }
    throw std::invalid_argument(
        fmt::format("hven_sqp_corpus: internal row parse: unrecognised status string '{}'", s));
}

// =============================================================================
// READING A COMMITTED ARTIFACT BACK (--from-csv): the offline re-score /
// merge path. The committed baseline is "the comparison column every later
// task cites"; before fix round 1 there was no way to turn it back into a
// GateVerdict short of re-running a multi-hour sweep.
// =============================================================================

std::vector<CorpusOutcome> read_outcomes_csv(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::invalid_argument(
            fmt::format("hven_sqp_corpus: --from-csv: could not read '{}'", path));
    }
    std::vector<CorpusOutcome> out;
    std::string line;
    std::size_t line_no = 0;
    while (std::getline(in, line)) {
        ++line_no;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.rfind("cell_id,", 0) == 0) {
            continue; // the column header
        }
        // 14 fields, and the qp_fact_per_qp field may legitimately be empty,
        // so split WITHOUT dropping empties.
        std::vector<std::string> col;
        {
            std::stringstream ss(line);
            std::string item;
            while (std::getline(ss, item, ',')) {
                col.push_back(item);
            }
        }
        const std::string where = fmt::format("--from-csv '{}' line {}", path, line_no);
        // A trailing empty field is dropped by getline when the line ends with
        // the separator; pad so the fixed indices below are safe. A TASK-1-ERA
        // artifact (14 columns, the committed walk baseline) reads exactly as
        // it always did and gets `unchecked` for the Task-6 tail -- see
        // write_header's own note on why the tail is appended, not interleaved.
        const bool has_task6_tail = col.size() >= static_cast<std::size_t>(kAllColumns);
        const bool has_task6b_tail = col.size() >= static_cast<std::size_t>(kTask6bColumns);
        if (col.size() < static_cast<std::size_t>(kTask1Columns)) {
            col.resize(static_cast<std::size_t>(kTask1Columns));
        }
        const CorpusCell *cell = find_cell(col[0]);
        if (cell == nullptr) {
            throw std::invalid_argument(fmt::format(
                "{}: unknown cell id '{}' (not in this binary's census)", where, col[0]));
        }
        CorpusOutcome o;
        o.cell = cell;
        const std::string &status = col[6];
        if (status == "dnf_setup" || status == "dnf_budget" ||
            status == hven::solvers::corpus::kEngineErrorStatusString) {
            if (status == hven::solvers::corpus::kEngineErrorStatusString) {
                o.engine_error = true;
            } else {
                o.dnf_phase = status == "dnf_setup" ? DnfPhase::kSetup : DnfPhase::kSolve;
            }
            o.dnf_wall_s = parse_double_field(where + " column wall_s", col[13]);
            out.push_back(std::move(o));
            continue;
        }
        o.row.cell_id = cell->id;
        o.row.factorizations = parse_int_field(where + " column factorizations", col[7]);
        o.row.qp_minors = parse_int_field(where + " column qp_minors", col[8]);
        o.row.escapes = parse_int_field(where + " column escapes", col[9]);
        o.row.status = parse_status(status);
        o.row.qp_factorizations =
            parse_qp_factorizations(where + " column qp_fact_per_qp", col[11]);
        o.row.kkt_residual = parse_double_field(where + " column kkt_residual", col[12]);
        o.row.wall_s = parse_double_field(where + " column wall_s", col[13]);
        const int declared = parse_int_field(where + " column qp_subproblems", col[10]);
        if (declared != static_cast<int>(o.row.qp_factorizations.size())) {
            throw std::invalid_argument(fmt::format(
                "{}: qp_subproblems says {} but qp_fact_per_qp carries {} entr(ies) -- the "
                "artifact is internally inconsistent and must not be scored",
                where, declared, o.row.qp_factorizations.size()));
        }
        if (has_task6_tail) {
            o.row.kkt_stationarity =
                parse_double_field(where + " column kkt_stationarity", col[15]);
            o.row.kkt_primal = parse_double_field(where + " column kkt_primal", col[16]);
            o.row.kkt_dual_sign = parse_double_field(where + " column kkt_dual_sign", col[17]);
            o.row.kkt_complementarity =
                parse_double_field(where + " column kkt_complementarity", col[18]);
            o.row.dual_scale = parse_double_field(where + " column dual_scale", col[19]);
            o.row.x_scale = parse_double_field(where + " column x_scale", col[20]);
            o.row.neg_ineq_duals = parse_int_field(where + " column neg_ineq_duals", col[21]);
            o.row.ssn.ssn_iters = parse_int_field(where + " column ssn_iters", col[22]);
            o.row.ssn.ssn_bulk_flips = parse_int_field(where + " column ssn_bulk_flips", col[23]);
            o.row.ssn.ssn_backtracks = parse_int_field(where + " column ssn_backtracks", col[24]);
            o.row.ssn.ssn_prox_updates =
                parse_int_field(where + " column ssn_prox_updates", col[25]);
            o.row.ssn.ssn_uncertain_peak =
                parse_int_field(where + " column ssn_uncertain_peak", col[26]);
            o.row.ssn.ssn_refinements = parse_int_field(where + " column ssn_refinements", col[27]);
            o.row.ssn.ssn_refine_refused =
                parse_int_field(where + " column ssn_refine_refused", col[28]);
            o.row.ssn.ssn_refine_factorizations =
                parse_int_field(where + " column ssn_refine_facts", col[29]);
            o.row.ssn.ssn_refine_neg_duals =
                parse_int_field(where + " column ssn_refine_neg_duals", col[30]);
            // THE ARTIFACT MAY NOT ASSERT ITS OWN INNOCENCE. The stored verdict
            // is re-derived from the stored residuals and must agree; a row
            // that says `ok` while its own numbers say `wrong` is rejected
            // rather than scored, exactly as an internally inconsistent
            // qp_subproblems column is. This is what makes the wrong-answer
            // category re-checkable offline instead of trusted.
            const hven::solvers::corpus::KktVerdict stored =
                hven::solvers::corpus::kkt_verdict_from_string(col[14]);
            const hven::solvers::corpus::KktVerdict derived =
                hven::solvers::corpus::kkt_gate_verdict(o.row);
            if (stored != derived) {
                throw std::invalid_argument(fmt::format(
                    "{}: kkt_verdict says '{}' but this row's own residuals re-derive to '{}' -- "
                    "the artifact is internally inconsistent and must not be scored",
                    where, col[14], hven::solvers::corpus::to_string(derived)));
            }
        }
        // PHASE-7 TASK 6b's escape-reason census, read on the SAME optional-tail
        // contract. A 14- or 31-column artifact (the committed walk baseline,
        // the committed 2026-08-08 battery CSVs) carries no census, and this
        // reader says so with -1 rather than inventing a zero -- a merge of an
        // older artifact through --from-csv therefore emits `-1` in these six
        // columns, which is ABSENT and is distinguishable from "measured, and
        // nothing escaped".
        if (!has_task6b_tail) {
            // ABSENT, and stamped so EXPLICITLY: SsnCounters default-constructs
            // to 0, which here would read as "measured, and nothing escaped".
            o.row.ssn.ssn_escape_budget = -1;
            o.row.ssn.ssn_escape_singular = -1;
            o.row.ssn.ssn_escape_no_contraction = -1;
            o.row.ssn.ssn_escape_infeasible_suspect = -1;
            o.row.ssn.ssn_escape_indefinite = -1;
            o.row.ssn.ssn_escape_gate_refused = -1;
        } else {
            o.row.ssn.ssn_escape_budget = parse_int_field(where + " column esc_budget", col[31]);
            o.row.ssn.ssn_escape_singular =
                parse_int_field(where + " column esc_singular", col[32]);
            o.row.ssn.ssn_escape_no_contraction =
                parse_int_field(where + " column esc_no_contraction", col[33]);
            o.row.ssn.ssn_escape_infeasible_suspect =
                parse_int_field(where + " column esc_infeasible_suspect", col[34]);
            o.row.ssn.ssn_escape_indefinite =
                parse_int_field(where + " column esc_indefinite", col[35]);
            o.row.ssn.ssn_escape_gate_refused =
                parse_int_field(where + " column esc_gate_refused", col[36]);
            // THE CENSUS MAY NOT DISAGREE WITH THE TOTAL IT PARTITIONS, for
            // exactly the reason the KKT verdict may not disagree with its own
            // residuals: an artifact whose six buckets do not sum to `escapes`
            // is internally inconsistent and must not be scored. (`escapes`
            // is CorpusRow::escapes, read from column 9 above, which is
            // SqpCounters::ssn::ssn_escapes verbatim.)
            const hven::Index census =
                o.row.ssn.ssn_escape_budget + o.row.ssn.ssn_escape_singular +
                o.row.ssn.ssn_escape_no_contraction + o.row.ssn.ssn_escape_infeasible_suspect +
                o.row.ssn.ssn_escape_indefinite + o.row.ssn.ssn_escape_gate_refused;
            if (census != static_cast<hven::Index>(o.row.escapes)) {
                throw std::invalid_argument(fmt::format(
                    "{}: the escape-reason census sums to {} but `escapes` says {} -- the "
                    "artifact is internally inconsistent and must not be scored",
                    where, census, o.row.escapes));
            }
        }
        out.push_back(std::move(o));
    }
    return out;
}

// Census order, so a merged artifact is byte-comparable across sweeps
// regardless of the order the fan-out happened to finish in.
std::vector<CorpusOutcome> in_census_order(std::vector<CorpusOutcome> outcomes) {
    std::vector<CorpusOutcome> ordered;
    ordered.reserve(outcomes.size());
    for (const CorpusCell &c : all_cells()) {
        for (CorpusOutcome &o : outcomes) {
            if (o.cell == &c) {
                ordered.push_back(o);
            }
        }
    }
    if (ordered.size() != outcomes.size()) {
        throw std::invalid_argument(
            fmt::format("hven_sqp_corpus: merge: {} of {} rows did not match a census cell",
                        outcomes.size() - ordered.size(), outcomes.size()));
    }
    return ordered;
}

// =============================================================================
// THE CHILD (internal single-cell mode) AND THE PARENT'S TWO-PHASE DEADLINE.
// =============================================================================

// Writes exactly one line -- the CorpusRow fields, comma-separated, with the
// per-QP factorization list last -- to `out_path`, and exits 0. It also
// touches `<out_path>.setup` at setup-complete (see the file banner). Any
// exception is left to propagate to main()'s own catch block, exiting 1 with
// the usual T6 message; the PARENT reads a nonzero exit before the deadline
// as a hard error, never as a DNF (a DNF is specifically "did not finish in
// time").
// TASK 6: THE CHILD NOW WRITES THE ARTIFACT'S OWN ROW FORMAT, not a private
// seven-field one. Task 1 had two row encodings (this one and the CSV) and
// therefore two places to extend and two places to get wrong; with the Task-6
// tail that would have been thirty-one fields duplicated. The child emits
// exactly one `write_outcome` line and the parent reads it back through
// `read_outcomes_csv`, so there is ONE writer and ONE reader for a corpus row,
// and the reader's own consistency checks (qp_subproblems, and now the KKT
// verdict) apply to the child's output too.

// TASK 4 (M4-Task5 plan): the model-surface census hook's OWN small sidecar
// format, entirely separate from write_outcome/read_outcomes_csv's 31/37-
// column artifact contract -- see corpus_cells.h's CorpusRow::ms_* note for
// why touching that contract is the wrong move here. One line, five
// comma-separated values, in CorpusRow::ms_* declaration order. Written by
// the CHILD (run_internal_one, only when EngineLevers::score_model_surface is
// set) and read by the PARENT (run_cell_with_deadline) to carry the five
// fields across the fork/exec boundary that write_outcome's own format does
// not touch.
void write_model_surface_sidecar(const std::string &path, const CorpusRow &row) {
    std::ofstream out(path);
    if (!out) {
        throw std::invalid_argument(
            fmt::format("--internal-run-one: could not open '{}' for writing", path));
    }
    out << fmt::format("{:.17g},{:.17g},{:.17g},{:.17g},{:.17g}\n", row.ms_stationarity,
                       row.ms_complementarity, row.ms_primal, row.ms_dual_scale, row.ms_x_scale);
}

void read_model_surface_sidecar(const std::string &path, CorpusRow &row) {
    std::ifstream in(path);
    if (!in) {
        throw std::invalid_argument(fmt::format(
            "hven_sqp_corpus: --score-model-surface: could not read sidecar '{}' -- the child "
            "was asked to score the model surface but did not write it",
            path));
    }
    std::string line;
    std::getline(in, line);
    const std::vector<std::string> col = split_on(line, ',');
    if (col.size() != 5) {
        throw std::invalid_argument(fmt::format(
            "hven_sqp_corpus: --score-model-surface: sidecar '{}' carries {} field(s), expected 5",
            path, col.size()));
    }
    row.ms_stationarity = parse_double_field(path + " column ms_stationarity", col[0]);
    row.ms_complementarity = parse_double_field(path + " column ms_complementarity", col[1]);
    row.ms_primal = parse_double_field(path + " column ms_primal", col[2]);
    row.ms_dual_scale = parse_double_field(path + " column ms_dual_scale", col[3]);
    row.ms_x_scale = parse_double_field(path + " column ms_x_scale", col[4]);
}

void run_internal_one(const std::string &cell_id, const std::string &engine,
                      const std::string &out_path, const EngineLevers &levers, bool force_throw,
                      bool force_abort) {
    const CorpusCell *cell = find_cell(cell_id);
    if (cell == nullptr) {
        throw std::invalid_argument(
            fmt::format("--internal-run-one: unknown cell id '{}'", cell_id));
    }
    const std::string setup_marker = out_path + ".setup";
    CorpusOutcome outcome;
    outcome.cell = cell;
    try {
        outcome.row = run_cell(
            *cell, engine,
            [&] {
                std::ofstream marker(setup_marker);
                marker << "setup complete\n";
                marker.flush();
                if (force_abort) {
                    // Deliberately NOT an exception: this arm exists to make
                    // the child die by SIGNAL.
                    std::abort();
                }
                if (force_throw) {
                    throw std::invalid_argument("hven_sqp_corpus: FORCED TEST THROW from the "
                                                "child (--internal-force-child-throw)");
                }
            },
            levers);
    } catch (const std::exception &e) {
        // TASK 6. RECORD THE MESSAGE WHERE THE PARENT CAN FIND IT, then let it
        // propagate: the child still exits 1 through main()'s T6 handler and
        // still prints to stderr, so nothing about the existing contract
        // changes -- the parent simply gains the text it needs to put an
        // `engine_error` row's REASON in front of a reader instead of "the
        // child exited abnormally".
        std::ofstream err(out_path + ".error");
        err << e.what() << "\n";
        err.flush();
        throw;
    }
    std::ofstream out(out_path);
    if (!out) {
        throw std::invalid_argument(
            fmt::format("--internal-run-one: could not open '{}' for writing", out_path));
    }
    write_outcome(out, outcome);
    if (levers.score_model_surface) {
        write_model_surface_sidecar(out_path + ".ms", outcome.row);
    }
}

CorpusRow read_internal_row(const std::string &path, const CorpusCell &cell) {
    const std::vector<CorpusOutcome> rows = read_outcomes_csv(path);
    if (rows.size() != 1 || rows.front().cell != &cell) {
        throw std::invalid_argument(
            fmt::format("hven_sqp_corpus: internal row '{}' for cell '{}' carries {} row(s) for "
                        "the wrong cell or none at all",
                        path, cell.id, rows.size()));
    }
    return rows.front().row;
}

bool file_exists(const std::string &path) {
    struct ::stat st{};
    return ::stat(path.c_str(), &st) == 0;
}

// fork() + execl(self) + poll waitpid(WNOHANG) against a steady_clock
// deadline that RESTARTS when the child signals setup-complete -- see this
// file's own banner.
//
// TASK 6: BOTH ENGINES GO THROUGH HERE. Task 1 wrapped only `--engine walk`,
// because `ssn` threw in-process and forking a clean throw would have turned it
// into an opaque nonzero child exit. Now that `ssn` really runs, the opposite
// reasoning applies with force: an unbounded SSN cell is exactly the hang the
// deadline exists to prevent, and scoring an SSN arm against a walk arm that
// was budgeted while the SSN arm was not would not be a comparison at all.
// Same tiers, same two phases, same DNF semantics, same everything.
CorpusOutcome run_cell_with_deadline(const char *self_path, const CorpusCell &cell,
                                     const std::string &engine, const EngineLevers &levers,
                                     bool force_child_throw, bool force_child_abort,
                                     std::optional<double> forced_setup_s,
                                     std::optional<double> forced_solve_s) {
    const double band_s = wall_budget_for_cell(cell);
    const double setup_budget_s = forced_setup_s.value_or(band_s);
    const double solve_budget_s = forced_solve_s.value_or(band_s);
    const std::string out_path =
        fmt::format("/tmp/hven_sqp_corpus_internal_{}_{}.row", ::getpid(), cell.id);
    const std::string setup_marker = out_path + ".setup";
    const std::string error_path = out_path + ".error";
    // TASK 4 (M4-Task5 plan): the model-surface sidecar's own path, written by
    // the child only when `levers.score_model_surface` is set (see
    // write_model_surface_sidecar's own note) -- always removed at cleanup so
    // a stale file from an earlier PID reuse can never be misread as this
    // run's.
    const std::string ms_path = out_path + ".ms";
    std::remove(setup_marker.c_str());
    std::remove(error_path.c_str());
    std::remove(ms_path.c_str());

    const pid_t pid = fork();
    if (pid < 0) {
        throw std::runtime_error(
            fmt::format("hven_sqp_corpus: fork() failed for cell '{}'", cell.id));
    }
    if (pid == 0) {
        // Child: re-exec self in internal single-cell mode. execv only returns
        // on failure. The lever flags are forwarded EXPLICITLY rather than
        // inherited, because the child is a fresh process and a lever the
        // parent was asked for but did not pass on would silently produce a
        // default-configuration row under a non-default provenance header.
        std::vector<std::string> argv_own{self_path, "--internal-run-one", cell.id, "--engine",
                                          engine,    "--internal-out",     out_path};
        if (levers.ssn_prox_carry) {
            argv_own.emplace_back("--ssn-prox-carry");
        }
        if (levers.ssn_certify_from_face) {
            argv_own.emplace_back("--ssn-certify-from-face");
        }
        if (levers.ssn_sigma_rule != SsnSigmaRule::kLadder) {
            argv_own.emplace_back("--ssn-sigma-rule");
            argv_own.emplace_back(levers.ssn_sigma_rule == SsnSigmaRule::kResidualArmed
                                      ? "residual-armed"
                                      : "residual-always");
        }
        if (levers.ssn_hint_rule != SsnHintRule::kIterationZeroFree) {
            argv_own.emplace_back("--ssn-hint-rule");
            argv_own.emplace_back("watchdog");
        }
        if (levers.ssn_infeasibility_rule != SsnInfeasibilityRule::kSymptoms) {
            argv_own.emplace_back("--ssn-infeasibility-rule");
            argv_own.emplace_back("farkas");
        }
        if (levers.score_model_surface) {
            argv_own.emplace_back("--score-model-surface");
        }
        if (force_child_throw) {
            argv_own.emplace_back("--internal-force-child-throw");
        }
        if (force_child_abort) {
            argv_own.emplace_back("--internal-force-child-abort");
        }
        std::vector<char *> argv_c;
        argv_c.reserve(argv_own.size() + 1);
        for (std::string &s : argv_own) {
            argv_c.push_back(s.data());
        }
        argv_c.push_back(nullptr);
        execv(self_path, argv_c.data());
        _exit(127);
    }

    // Parent.
    auto cleanup = [&] {
        std::remove(out_path.c_str());
        std::remove(setup_marker.c_str());
        std::remove(error_path.c_str());
        std::remove(ms_path.c_str());
    };
    bool in_setup_phase = true;
    auto deadline =
        std::chrono::steady_clock::now() + std::chrono::duration<double>(setup_budget_s);
    for (;;) {
        int status = 0;
        const pid_t reaped = waitpid(pid, &status, WNOHANG);
        if (reaped == pid) {
            if (WIFEXITED(status) && WEXITSTATUS(status) == 1) {
                // THE ENGINE THREW (main()'s documented T6 exit). This is an
                // OUTCOME, not a runner failure: see corpus_cells.h's
                // kEngineErrorStatusString for why it is a row rather than an
                // aborted sweep, and why it is charged the worst case.
                CorpusOutcome out;
                out.cell = &cell;
                out.engine_error = true;
                out.dnf_wall_s = in_setup_phase ? setup_budget_s : solve_budget_s;
                std::ifstream why(error_path);
                std::getline(why, out.engine_error_what);
                cleanup();
                return out;
            }
            if (!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
                cleanup();
                throw std::runtime_error(fmt::format(
                    "hven_sqp_corpus: cell '{}' child exited abnormally "
                    "(WIFEXITED={} WEXITSTATUS={}) -- a signal or an unexpected exit code is a "
                    "runner failure, never a measurement",
                    cell.id, WIFEXITED(status) != 0, WIFEXITED(status) ? WEXITSTATUS(status) : -1));
            }
            CorpusOutcome out;
            out.cell = &cell;
            out.row = read_internal_row(out_path, cell);
            if (levers.score_model_surface) {
                // The child wrote this beside out_path -- see
                // write_model_surface_sidecar's own note for why this is a
                // separate small file rather than a fifth read_outcomes_csv
                // tail.
                read_model_surface_sidecar(ms_path, out.row);
            }
            cleanup();
            return out;
        }
        // THE PHASE CHECK COMES FIRST, deliberately: a child that finished
        // setup before this poll must be judged against the SOLVE budget, not
        // killed for a setup deadline it already cleared.
        if (in_setup_phase && file_exists(setup_marker)) {
            in_setup_phase = false;
            deadline =
                std::chrono::steady_clock::now() + std::chrono::duration<double>(solve_budget_s);
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            ::kill(pid, SIGKILL);
            int reap_status = 0;
            ::waitpid(pid, &reap_status, 0); // reap the zombie; ignore its (killed) status
            cleanup();
            CorpusOutcome out;
            out.cell = &cell;
            out.dnf_phase = in_setup_phase ? DnfPhase::kSetup : DnfPhase::kSolve;
            out.dnf_wall_s = in_setup_phase ? setup_budget_s : solve_budget_s;
            return out;
        }
        // 20 ms: cheap enough at production budgets (900-3600 s, tens of
        // thousands of polls, each one waitpid() syscall plus one stat()) and
        // fine enough that a test can force a sub-millisecond budget on a
        // cell taking low hundreds of milliseconds and reliably observe a DNF
        // within one or two polls.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void print_gate_verdict(const std::vector<CorpusOutcome> &outcomes) {
    std::size_t population = 0;
    std::size_t population_wc = 0;
    std::size_t dnfs = 0;
    std::size_t engine_errors = 0;
    std::size_t wrong = 0;
    std::size_t checked = 0;
    std::size_t dual_sign_fails = 0;
    std::size_t neg_dual_rows = 0;
    long long neg_duals = 0;
    long long refine_facts = 0;
    long long refine_neg = 0;
    long long total_facts = 0;
    long long refinements = 0;
    long long refusals = 0;
    std::size_t escapes = 0;
    std::size_t subproblems = 0;
    for (const CorpusOutcome &o : outcomes) {
        if (o.cell != nullptr && hven::solvers::corpus::in_g1_g2_population(*o.cell)) {
            ++population;
        }
        if (o.cell != nullptr &&
            hven::solvers::corpus::in_g1_g2_population(*o.cell, /*include_corrupted=*/true)) {
            ++population_wc;
        }
        if (o.dnf()) {
            ++dnfs;
            continue;
        }
        if (o.engine_error) {
            ++engine_errors;
            continue;
        }
        if (kkt_gate_verdict(o.row) != KktVerdict::kUnchecked) {
            ++checked;
        }
        if (o.wrong_answer()) {
            ++wrong;
        }
        if (dual_sign_would_fail(o.row)) {
            ++dual_sign_fails;
        }
        if (o.row.neg_ineq_duals > 0) {
            ++neg_dual_rows;
            neg_duals += o.row.neg_ineq_duals;
        }
        refine_facts += static_cast<long long>(o.row.ssn.ssn_refine_factorizations);
        refine_neg += static_cast<long long>(o.row.ssn.ssn_refine_neg_duals);
        refinements += static_cast<long long>(o.row.ssn.ssn_refinements);
        refusals += static_cast<long long>(o.row.ssn.ssn_refine_refused);
        total_facts += static_cast<long long>(std::max(0, o.row.factorizations));
        // G4's own universe: NON-DEGENERATE cells only (the gate's text), so
        // this measured figure is comparable with the charged one printed
        // below rather than being taken over a different set of rows.
        if (!o.cell->degenerate) {
            escapes += static_cast<std::size_t>(std::max(0, o.row.escapes));
            subproblems += o.row.qp_factorizations.size();
        }
    }
    const auto verdict = evaluate_gates(outcomes);
    const auto verdict_wc = evaluate_gates(outcomes, /*include_corrupted=*/true);
    fmt::print("\n--score-gates.\n");
    fmt::print("  rows = {}, DNF rows = {} (SCORED as worst case, see corpus_cells.h's P3),\n"
               "  ENGINE-ERROR rows = {} (the engine threw; charged as worst case),\n"
               "  KKT-gated rows = {}, WRONG-ANSWER rows = {} (charged as worst case, W5),\n"
               "  G1/G2 population (path-interface, warm|activity, non-degenerate) = {} cells\n",
               outcomes.size(), dnfs, engine_errors, checked, wrong, population);
    // PARTIAL-POOL GUARD (final branch review, WAVE #4; T1 NEW-4). A `--cells`
    // subset can score G1/G3 over an empty pool, and `median`/`percentile`
    // report 0.0 on empty -- a vacuous PASS a reader could mistake for a real
    // one. Warn loudly rather than relying on a reader noticing `rows = N`
    // above; this is informational only and never flips `pass[]`.
    if (verdict.g1g2_pool_size == 0) {
        fmt::print("  WARNING: G1/G2 pool is EMPTY (0 QP values) -- the PASS below is VACUOUS, "
                   "not a measurement (partial --cells run?).\n");
    }
    if (verdict.g3_pair_count == 0) {
        fmt::print("  WARNING: G3 has 0 matched 5000<->20000 pairs -- the PASS below is VACUOUS, "
                   "not a measurement (partial --cells run?).\n");
    }
    fmt::print("  G1 median factorizations per QP = {:.3f}  (<=12)  {}\n", verdict.g1_median,
               verdict.pass[0] ? "PASS" : "fail");
    fmt::print("  G2 p95 factorizations per QP    = {:.3f}  (<=25)  {}\n", verdict.g2_p95,
               verdict.pass[1] ? "PASS" : "fail");
    fmt::print("  G3 median growth 5000->20000    = {:.3f}  (<=0)   {}\n", verdict.g3_growth,
               verdict.pass[2] ? "PASS" : "fail");
    fmt::print("  G4 escape rate per QP           = {:.4f}  (<0.02) {}\n", verdict.g4_escape_rate,
               verdict.pass[3] ? "PASS" : "fail");
    // THE BOTH-WAYS READING (corpus-design.md section 5.1's requirement on this
    // task). Same rows, same evaluator, one predicate widened.
    fmt::print("  --- G1/G2 WITH kCorrupted admitted (population = {} cells) ---\n", population_wc);
    fmt::print("  G1 median factorizations per QP = {:.3f}  (<=12)  {}\n", verdict_wc.g1_median,
               verdict_wc.pass[0] ? "PASS" : "fail");
    fmt::print("  G2 p95 factorizations per QP    = {:.3f}  (<=25)  {}\n", verdict_wc.g2_p95,
               verdict_wc.pass[1] ? "PASS" : "fail");
    // G4's two figures, quoted separately as the corpus-design note's section
    // 6.5 asked Task 6 to do once the SSN arm had real escapes to report: the
    // MEASURED rate over finishing rows, and the rate the DNF/wrong-answer
    // charge produces. They are different claims and must not be conflated.
    // "correct" was the wrong word: this figure's population is every row
    // that produced an answer (dnf() and engine_error rows excluded above,
    // via `continue`) over non-degenerate cells, which ALSO admits a
    // finishing kNumericalError row -- that inclusion is the only way the
    // measured 7/59 arises (final branch review, WAVE #6). The number is
    // right under the actual rule; only the label claimed a narrower one.
    fmt::print("  G4 measured (finishing rows only) = {} escapes / {} QP subproblems "
               "= {:.4f}\n",
               escapes, subproblems,
               subproblems == 0 ? 0.0
                                : static_cast<double>(escapes) / static_cast<double>(subproblems));
    // TELEMETRY, NEVER A GATE (re-review NF-1, and W2's dual-sign split).
    fmt::print("  --- telemetry (reported, not gated) ---\n");
    fmt::print("  dual sign: {} row(s) carry a strictly negative inequality multiplier at the "
               "returned point ({} multipliers in total);\n"
               "             {} checked row(s) would ALSO fail a gate that included dual "
               "feasibility at the HS battery's 1e-9, relative\n",
               neg_dual_rows, neg_duals, dual_sign_fails);
    fmt::print("  tier-3 refinement: {} accepted / {} refused, {} factorization(s) of {} total "
               "({:.2f}%), {} negative multiplier(s) adopted\n",
               refinements, refusals, refine_facts, total_facts,
               total_facts == 0
                   ? 0.0
                   : 100.0 * static_cast<double>(refine_facts) / static_cast<double>(total_facts),
               refine_neg);
    fmt::print("  certifying SSN exits (refined + refused) = {}, each paying ONE inertia-evidence "
               "factorization = {:.2f}% of total\n",
               refinements + refusals,
               total_facts == 0 ? 0.0
                                : 100.0 * static_cast<double>(refinements + refusals) /
                                      static_cast<double>(total_facts));
}

// TASK 4 (M4-Task5 plan): --score-model-surface's own artifact, written ONLY
// when that flag is on -- main() below never calls this otherwise. One row
// per outcome that actually produced an answer (o.no_answer() rows -- DNF and
// engine_error -- are skipped: the census hook never ran for them, exactly as
// record_kkt_check never did). `verdict_equal` re-derives BOTH verdicts
// through corpus_cells.h's own `kkt_gate_verdict`, applied first to the row as
// recorded and then to a copy whose three W2 residuals and two W3 scales are
// swapped for the scorer's own reading -- so this is the SAME gate rule the
// main CSV's `kkt_verdict` column already uses, read twice over two
// independent measurements of one point.
void write_model_surface_census(const std::string &path,
                                const std::vector<CorpusOutcome> &outcomes) {
    std::ofstream out(path);
    if (!out) {
        throw std::invalid_argument(
            fmt::format("--score-model-surface-out: could not open '{}' for writing", path));
    }
    out << "cell_id,kkt_stationarity,kkt_complementarity,kkt_primal,"
           "ms_stationarity,ms_complementarity,ms_primal,verdict_equal\n";
    for (const CorpusOutcome &o : outcomes) {
        if (o.no_answer()) {
            continue;
        }
        CorpusRow scorer_view = o.row;
        scorer_view.kkt_stationarity = o.row.ms_stationarity;
        scorer_view.kkt_complementarity = o.row.ms_complementarity;
        scorer_view.kkt_primal = o.row.ms_primal;
        scorer_view.dual_scale = o.row.ms_dual_scale;
        scorer_view.x_scale = o.row.ms_x_scale;
        const bool verdict_equal = kkt_gate_verdict(o.row) == kkt_gate_verdict(scorer_view);
        out << fmt::format("{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{}\n", o.row.cell_id,
                           o.row.kkt_stationarity, o.row.kkt_complementarity, o.row.kkt_primal,
                           o.row.ms_stationarity, o.row.ms_complementarity, o.row.ms_primal,
                           verdict_equal ? 1 : 0);
    }
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Args args = parse_args(argc, argv);
        if (args.help) {
            fmt::print("{}", kUsage);
            return 0;
        }
        if (args.list) {
            print_list();
            return 0;
        }
        if (args.internal_run_one) {
            if (!args.engine || !args.internal_out) {
                throw_usage("--internal-run-one requires --engine and --internal-out");
            }
            EngineLevers levers;
            levers.ssn_prox_carry = args.ssn_prox_carry;
            levers.ssn_certify_from_face = args.ssn_certify_from_face;
            levers.ssn_sigma_rule = args.ssn_sigma_rule;
            levers.ssn_hint_rule = args.ssn_hint_rule;
            levers.ssn_infeasibility_rule = args.ssn_infeasibility_rule;
            levers.score_model_surface = args.score_model_surface;
            run_internal_one(*args.internal_run_one, *args.engine, *args.internal_out, levers,
                             args.internal_force_child_throw, args.internal_force_child_abort);
            return 0;
        }

        if (args.dump_qp) {
            if (!args.dump_qp_out) {
                throw_usage("--dump-qp requires --dump-qp-out");
            }
            if (args.engine || args.cells || args.csv || args.from_csv) {
                throw_usage("--dump-qp builds and dumps one cell's first QP; it cannot be "
                            "combined with --engine/--cells/--csv/--from-csv");
            }
            const CorpusCell *cell = find_cell(*args.dump_qp);
            if (cell == nullptr) {
                throw_usage(
                    fmt::format("--dump-qp: unknown cell id '{}' (try --list)", *args.dump_qp));
            }
            // fix round 1, review M6 (dead seam, documented rather than
            // removed): `first_qp_for_cell`'s `on_setup_complete` callback
            // exists so a caller CAN restart a wall-clock deadline at the
            // instant the setup hop finishes -- exactly the seam
            // `run_walk_cell_with_deadline` uses below for `--engine walk`.
            // This call site passes none: `--dump-qp` is not wrapped in
            // this file's own fork/exec deadline machinery today (see
            // kUsage's own `--dump-qp` text -- a caller wanting a wall
            // bound wraps the WHOLE invocation in a shell-level `timeout`
            // instead), so there is nothing here to restart a clock on. The
            // parameter is left in `first_qp_for_cell`'s signature (rather
            // than dropped) because it is the natural extension point if
            // `--dump-qp` ever grows its own in-process deadline -- a
            // `timeout` kill today produces no row and no
            // dnf_setup/dnf_budget attribution, unlike an `--engine walk`
            // DNF, which is the gap that extension would close.
            const QpProblem qp = first_qp_for_cell(*cell);
            std::ofstream out(*args.dump_qp_out);
            if (!out) {
                throw std::invalid_argument(fmt::format("--dump-qp-out: could not open '{}' for "
                                                        "writing",
                                                        *args.dump_qp_out));
            }
            write_qp_dump(out, *cell, qp);
            fmt::print("dumped {} (n={}, me={}, mi={}) to {}\n", cell->id, qp.n(), qp.me(), qp.mi(),
                       *args.dump_qp_out);
            return 0;
        }

        if (args.internal_force_child_throw) {
            fmt::print(stderr, "hven_sqp_corpus: WARNING: --internal-force-child-throw is in "
                               "force. Every cell in this invocation will report engine_error "
                               "regardless of what the engine would have done; these rows are "
                               "TEST FIXTURES and must never be cited as a measurement.\n");
        }
        if (args.internal_force_setup_budget_s || args.internal_force_solve_budget_s) {
            fmt::print(stderr,
                       "hven_sqp_corpus: WARNING: a hidden TEST-ONLY wall-budget override is in "
                       "force (setup={}, solve={}). These rows are NOT produced under the "
                       "committed budget table and must never be cited as a baseline; the CSV's "
                       "own provenance header records this.\n",
                       args.internal_force_setup_budget_s
                           ? fmt::format("{:.9f}s", *args.internal_force_setup_budget_s)
                           : "<band>",
                       args.internal_force_solve_budget_s
                           ? fmt::format("{:.9f}s", *args.internal_force_solve_budget_s)
                           : "<band>");
        }

        // --------------------------------------------------------------
        // OFFLINE: read committed artifacts, optionally merge, optionally
        // score. Solves nothing.
        // --------------------------------------------------------------
        if (args.from_csv) {
            if (args.engine || args.cells) {
                throw_usage("--from-csv reads committed rows and runs nothing: it cannot be "
                            "combined with --engine/--cells");
            }
            std::vector<CorpusOutcome> outcomes;
            for (const std::string &path : split_on(*args.from_csv, ',')) {
                std::vector<CorpusOutcome> part = read_outcomes_csv(path);
                outcomes.insert(outcomes.end(), part.begin(), part.end());
            }
            if (outcomes.empty()) {
                throw_usage(fmt::format("--from-csv: '{}' yielded zero rows", *args.from_csv));
            }
            outcomes = in_census_order(std::move(outcomes));
            if (args.csv) {
                std::ofstream out(*args.csv);
                if (!out) {
                    throw std::invalid_argument(
                        fmt::format("--csv: could not open '{}' for writing", *args.csv));
                }
                EngineLevers merge_levers;
                merge_levers.ssn_prox_carry = args.ssn_prox_carry;
                merge_levers.ssn_certify_from_face = args.ssn_certify_from_face;
                merge_levers.ssn_sigma_rule = args.ssn_sigma_rule;
                merge_levers.ssn_hint_rule = args.ssn_hint_rule;
                merge_levers.ssn_infeasibility_rule = args.ssn_infeasibility_rule;
                write_provenance(out, argc, argv, merge_levers, args.internal_force_child_throw,
                                 args.internal_force_setup_budget_s,
                                 args.internal_force_solve_budget_s);
                write_header(out);
                for (const CorpusOutcome &o : outcomes) {
                    write_outcome(out, o);
                }
                fmt::print("merged {} row(s) into {}\n", outcomes.size(), *args.csv);
            }
            if (args.score_gates) {
                print_gate_verdict(outcomes);
            }
            if (!args.csv && !args.score_gates) {
                throw_usage("--from-csv needs --csv (merge) and/or --score-gates (score); on its "
                            "own it would do nothing");
            }
            return 0;
        }

        if (!args.engine || !args.cells || !args.csv) {
            throw_usage("--engine, --cells and --csv are all required (or pass --help / --list / "
                        "--from-csv instead)");
        }
        if (args.score_model_surface != args.score_model_surface_out.has_value()) {
            throw_usage("--score-model-surface and --score-model-surface-out must be passed "
                        "together (the first without the second has nowhere to write; the second "
                        "without the first would never be written to)");
        }

        const std::vector<const CorpusCell *> cells = resolve_cells(*args.cells);

        EngineLevers levers;
        levers.ssn_prox_carry = args.ssn_prox_carry;
        levers.ssn_certify_from_face = args.ssn_certify_from_face;
        levers.ssn_sigma_rule = args.ssn_sigma_rule;
        levers.ssn_hint_rule = args.ssn_hint_rule;
        levers.ssn_infeasibility_rule = args.ssn_infeasibility_rule;
        levers.score_model_surface = args.score_model_surface;

        // INCREMENTAL. One abnormally-exiting child used to abort a
        // multi-hour sweep with an empty file; every row that completed is
        // now on disk before the next cell starts.
        std::ofstream out(*args.csv);
        if (!out) {
            throw std::invalid_argument(
                fmt::format("--csv: could not open '{}' for writing", *args.csv));
        }
        write_provenance(out, argc, argv, levers, args.internal_force_child_throw,
                         args.internal_force_setup_budget_s, args.internal_force_solve_budget_s);
        write_header(out);
        out.flush();

        std::vector<CorpusOutcome> outcomes;
        outcomes.reserve(cells.size());
        for (const CorpusCell *cell : cells) {
            fmt::print("running {} (N={}, {}, {}, engine {}, wall budget {:.0f}s per phase)...\n",
                       cell->id, cell->n_nodes, to_string(cell->ctag), to_string(cell->start),
                       *args.engine,
                       args.internal_force_solve_budget_s.value_or(wall_budget_for_cell(*cell)));
            CorpusOutcome outcome = run_cell_with_deadline(
                argv[0], *cell, *args.engine, levers, args.internal_force_child_throw,
                args.internal_force_child_abort, args.internal_force_setup_budget_s,
                args.internal_force_solve_budget_s);
            if (outcome.engine_error) {
                fmt::print("  -> ENGINE ERROR (scored as a non-answer, worst case): {}\n",
                           outcome.engine_error_what);
            } else if (outcome.dnf()) {
                fmt::print("  -> {} at the {:.0f}s deadline\n",
                           dnf_phase_status_string(outcome.dnf_phase), outcome.dnf_wall_s);
            } else if (outcome.wrong_answer()) {
                fmt::print("  -> WRONG ANSWER: claimed {} but failed the model-level KKT gate "
                           "(stat {:.3e}, primal {:.3e}, sign {:.3e}, comp {:.3e}, dual scale "
                           "{:.3e})\n",
                           to_string(outcome.row.status), outcome.row.kkt_stationarity,
                           outcome.row.kkt_primal, outcome.row.kkt_dual_sign,
                           outcome.row.kkt_complementarity, outcome.row.dual_scale);
            }
            write_outcome(out, outcome);
            out.flush();
            outcomes.push_back(std::move(outcome));
        }
        fmt::print("wrote {} row(s) to {}\n", outcomes.size(), *args.csv);

        if (args.score_gates) {
            print_gate_verdict(outcomes);
        }
        if (args.score_model_surface) {
            write_model_surface_census(*args.score_model_surface_out, outcomes);
            fmt::print("wrote the model-surface census to {}\n", *args.score_model_surface_out);
        }
        return 0;
    } catch (const std::exception &e) {
        fmt::print(stderr, "hven_sqp_corpus: error: {}\n", e.what());
        return 1;
    }
}
