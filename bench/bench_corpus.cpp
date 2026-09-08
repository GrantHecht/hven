// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

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

#include <algorithm>
#include <chrono>
#include <cmath>
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
#include "ipm_corpus_leg.h"
#include "support/hs_problems.h"

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

// The top-level interior-point leg (M6 W5 T8.1); see bench/ipm_corpus_leg.h.
using hven::solvers::FixedVariableTreatments;
using hven::solvers::corpus::interior_csv_header;
using hven::solvers::corpus::interior_csv_row;
using hven::solvers::corpus::interior_treatment_tag;
using hven::solvers::corpus::interior_treatments;
using hven::solvers::corpus::InteriorLevers;
using hven::solvers::corpus::InteriorRow;
using hven::solvers::corpus::kHs071FixedCellId;
using hven::solvers::corpus::run_interior_cell;
using hven::solvers::corpus::run_interior_hs071;

// The measurement-arm levers, named once. See corpus_cells.h's EngineConfig.
using EngineLevers = hven::solvers::corpus::detail::EngineConfig;
// Task 6b Phase B's three iteration-shape rules (sqp_types.h), named here for
// the same reason every other type above is: this file spells one namespace.
using hven::solvers::SsnHintRule;
using hven::solvers::SsnInfeasibilityRule;
using hven::solvers::SsnSigmaRule;

// M6 W5 T6.d LEG 2. The types the HS suite below needs, on the same
// one-namespace-spelling terms.
using hven::solvers::QpMode;
using hven::solvers::QpModeSite;
using hven::solvers::QpModeTraceEvent;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::TraceSink;

constexpr const char *kUsage =
    "usage: hven_sqp_corpus --engine walk|ssn|ipm --cells all|<id1,id2,...> --csv <path> "
    "[--score-gates]\n"
    "       hven_sqp_corpus --hs --engine walk|ssn|ipm --csv <path> [--repeat N]\n"
    "                       [--hs-cells all|<n1,n2,...>] [--hs-trace off|sink]\n"
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
    "  --engine ARM      walk | ssn | ipm | interior. walk replays through the ordinary SqpDriver\n"
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
    "                    one field and nothing else. interior is NOT one of the\n"
    "                    three SQP arms: it replays the TOP-LEVEL interior-point\n"
    "                    driver (bench/ipm_corpus_leg.h) over the dual-bindable\n"
    "                    cells, IN PROCESS, three rows per cell (one per\n"
    "                    fixed-variable treatment), on its own 19-column schema.\n"
    "                    Requires --cells and --csv; REFUSES --from-csv/\n"
    "                    --score-gates/--score-model-surface/--dump-qp, the SSN\n"
    "                    measurement levers and the hidden test levers. It ALWAYS\n"
    "                    runs the fixed-variable cell hs071_x1_fixed in addition\n"
    "                    to the cells it is given -- no F7 cell has a bound-fixed\n"
    "                    variable, so it is the one cell on which the three\n"
    "                    treatments take three different paths. A cell that does\n"
    "                    not dual-bind is REFUSED BY NAME, with the reason, into\n"
    "                    the artifact's provenance header and onto stdout.\n"
    "                    Partitions and backend threads are pinned to 1 and\n"
    "                    stamped; there is no wall deadline (these cells are\n"
    "                    seconds-scale through this driver).\n"
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
    "\n"
    "  --hs              THE HS SUITE (M6 W5 T6.d leg 2). Solve the 27\n"
    "                    Hock-Schittkowski problems of\n"
    "                    tests/sqp/support/hs_problems.h through SqpDriver in\n"
    "                    the --engine mode, IN PROCESS, and write one timed row\n"
    "                    per cell. Small, major-dense cells -- the corpus's own\n"
    "                    are F7 at n >= 800, where the QP dominates and a\n"
    "                    per-major helper is invisible. Requires --engine and\n"
    "                    --csv; REFUSES --cells/--from-csv/--score-gates/\n"
    "                    --score-model-surface/--dump-qp and the hidden test\n"
    "                    levers, rather than accepting and ignoring them. NOT\n"
    "                    gate-scored, and NOT wrapped in this file's wall\n"
    "                    deadline: these cells run in milliseconds, so a fork\n"
    "                    per cell would cost more than the measurement.\n"
    "  --repeat N        HS ONLY. Solve each cell N times and report the MEDIAN\n"
    "                    wall, with min/max and the full spread as a percentage\n"
    "                    of the median. Calibration: raise N until the\n"
    "                    A-arm-alone per-cell MEDIAN SE is inside +/-0.5 %.\n"
    "                    Read median_se_pct, NOT spread_pct: max-min is\n"
    "                    monotone in N by construction (0.000 % at N=1, 44 % at\n"
    "                    N=10 on this box) and cannot converge, while the\n"
    "                    standard error of the reported median falls as\n"
    "                    1/sqrt(N). The arm is ALSO run three times and its\n"
    "                    per-cell medians compared, which assumes no\n"
    "                    distribution at all.\n"
    "                    corpus figure is the SUM of per-cell medians, never a\n"
    "                    mean of ratios. Default 1. Every repeat of one cell is\n"
    "                    checked to produce the SAME counters; a cell where they\n"
    "                    move reports counters_stable=0 and its median is not a\n"
    "                    median over one computation.\n"
    "  --hs-warmup W     HS ONLY. W UNTIMED solves per cell before the timed\n"
    "                    repeats. Default 1. The first solve of a cell pays\n"
    "                    first-touch page faults, allocator growth and MKL's\n"
    "                    own first-call init; that cost lands on repeat 1 and\n"
    "                    no --repeat clears it, because the spread is max-min\n"
    "                    and the cold run stays the max. Measured: hs5 at\n"
    "                    --repeat 3 read a 468 % spread from that alone. Pass 0\n"
    "                    to reproduce the cold reading. Stamped in the\n"
    "                    provenance and carried in the `warmup` column.\n"
    "  --hs-cells SPEC   HS ONLY. 'all' (the default) or a comma-separated list\n"
    "                    of shipped HS problem numbers, e.g. 5,10,76.\n"
    "  --hs-trace MODE   HS ONLY. off (default) leaves no trace sink attached --\n"
    "                    today's shape, and the arm in which NEITHER driver-side\n"
    "                    trace_outcome_of call executes, since both sit inside\n"
    "                    `if (ipqp_trace_ != nullptr)`. sink attaches a real\n"
    "                    counting sink so both mapper sites run. The two are\n"
    "                    SEPARATE rows (the `trace` column) and must not be\n"
    "                    averaged: they are different populations. The\n"
    "                    ev_* columns count qp.mode events PER SITE and are how\n"
    "                    a reader proves each site fired rather than assuming\n"
    "                    it.\n"
    "\n"
    "  --list            print every census cell's id/tags and exit 0; touches\n"
    "                    neither --engine/--cells/--csv nor the solver.\n"
    "  --score-model-surface   the model-surface census hook. DEFAULT OFF.\n"
    "                    Requires --score-model-surface-out. Only meaningful with\n"
    "                    --engine/--cells/--csv (a real sweep): scores EVERY row\n"
    "                    the sweep produces, whatever it exited with, through\n"
    "                    bench/model_surface_kkt.h's engine-independent scorer\n"
    "                    against the SAME returned point, and writes cell_id, the\n"
    "                    three recorded residuals, the three scorer residuals, and\n"
    "                    a verdict-equal boolean (both read through\n"
    "                    corpus_cells.h's own kkt_gate_verdict) to\n"
    "                    --score-model-surface-out. A kOptimal filter, where one\n"
    "                    applies, is the gate's own, applied downstream over these\n"
    "                    columns -- never this hook's. Never touches the main\n"
    "                    --csv artifact's own columns.\n"
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
    "the DEADLINE that was enforced, not a measurement.\n"
    "\n"
    "`--engine interior` writes a DIFFERENT, 19-column schema (one row per cell\n"
    "per treatment), and `--from-csv` does not read it -- the reader accepts the\n"
    "corpus widths 14/31/37/76 and nothing else:\n"
    "  cell_id,family,n_nodes,window,taxonomy,status,iter_num,obj_val,kkt_inf,\n"
    "  barr_inf,econ_inf,icon_inf,factorizations,solves,analyses,soc_steps,\n"
    "  watchdog_activations,fixed_treatment,wall_s\n"
    "`cell_id` there is the cell joined to the treatment by a slash, so the\n"
    "column is unique per row; `fixed_treatment` carries the treatment alone.\n";

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
    // The model-surface census hook's opt-in flag and its output path
    // (docs/notes/2026-08-21-m4-task5-design.md). DEFAULT OFF, and off means
    // genuinely off -- see
    // corpus_cells.h's EngineConfig::score_model_surface and `timed_row` for
    // where the guard actually lives; this flag only decides whether that
    // guard is ever set to true. Paired with `--score-model-surface-out`
    // exactly as `--dump-qp`/`--dump-qp-out` are: required together, checked
    // in main() below.
    bool score_model_surface = false;
    std::optional<std::string> score_model_surface_out;

    // M6 W5 T6.d LEG 2 -- the HS suite. See the HS SUITE section below for why
    // it lives in this binary and what its terms are. `--hs` selects it;
    // `--engine` and `--csv` keep their meanings; every other corpus flag is
    // REFUSED with it rather than silently ignored, because a flag that reads
    // as accepted and does nothing is how a measurement arm gets mislabelled.
    bool hs = false;
    std::optional<std::string> hs_cells;
    // Repeats per HS cell, reduced to a MEDIAN. Calibrated at T6.d time by
    // raising N until the A-arm-alone per-cell `median_se_pct` is inside
    // +/-0.5 % -- NOT `spread_pct`, which is monotone in N and cannot converge.
    // HS-ONLY on purpose: the corpus arms are seconds-scale and already stable,
    // and a repeat loop there would perturb a pinned artifact's producer.
    int repeat = 1;
    // Attach a real TraceSink. OFF is today's shape (`ipqp_trace_ == nullptr`)
    // and is the arm that does NOT execute either driver-side
    // `trace_outcome_of` call; `sink` is the arm that does. The two are
    // reported as separate rows, never averaged.
    bool hs_trace_sink = false;
    // UNTIMED solves before the timed repeats, per cell. DECLARED, stamped in
    // the provenance and carried in a column, because a warm-up is a choice
    // about what is measured and not a detail.
    //
    // WHY IT EXISTS, measured rather than assumed: on the first solve of a cell
    // the process pays first-touch page faults, allocator growth and MKL's own
    // first-call initialisation, and that cost lands on repeat 1 alone. A
    // Debug smoke of hs5 at --repeat 3 read min 0.746 ms, max 4.448 ms, spread
    // 468 % -- and no N clears that, because the spread is max-min and the cold
    // run stays the max forever. §11.3's calibration target ("raise N until the
    // A-arm-alone per-cell spread is inside +/-0.5 %") is unreachable for a
    // reason that has nothing to do with the library under test. One untimed
    // solve removes it. 0 is accepted, and is how a reader reproduces the cold
    // reading if they want to see it.
    int hs_warmup = 1;
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
            if (v != "walk" && v != "ssn" && v != "ipm" && v != "interior") {
                throw_usage(fmt::format("--engine: '{}' is not one of walk|ssn|ipm|interior", v));
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
        } else if (arg == "--hs") {
            a.hs = true;
        } else if (arg == "--hs-cells") {
            a.hs_cells = next_value(arg);
        } else if (arg == "--hs-trace") {
            const std::string v = next_value(arg);
            if (v == "off") {
                a.hs_trace_sink = false;
            } else if (v == "sink") {
                a.hs_trace_sink = true;
            } else {
                throw_usage(fmt::format("--hs-trace: '{}' is not one of off|sink", v));
            }
        } else if (arg == "--hs-warmup") {
            const int v = parse_int_field("--hs-warmup", next_value(arg));
            if (v < 0) {
                throw_usage(fmt::format("--hs-warmup: {} is not >= 0", v));
            }
            a.hs_warmup = v;
        } else if (arg == "--repeat") {
            const int v = parse_int_field("--repeat", next_value(arg));
            if (v < 1) {
                throw_usage(fmt::format("--repeat: {} is not >= 1", v));
            }
            a.repeat = v;
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
// The provenance header of a NEW artifact records its schema width (76 since
// M6 W1 T9) so a reader never has to count commas to know its generation.
constexpr int kTask6bColumns = 37;

// M6 W1 T9: THIRTY-NINE MORE on the same optional-tail contract -- all of
// IpqpCounters, one column per field, so the kIpm arm's replay ASSERTS the
// tier's census. Declared move 37 -> 76; nothing existing moves.
constexpr int kIpqpColumns = 76;

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
    // THE SCHEMA GENERATION, stated rather than counted: 14, 31, 37 and (T9)
    // 76, which the reader accepts AT EXACTLY THOSE WIDTHS. The older
    // committed artifacts are pinned evidence and are NOT regenerated.
    os << fmt::format("# schema: {}\n", kIpqpColumns);
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
          "esc_gate_refused,"
          "ipqp_iters,ipqp_factorizations,ipqp_symbolic_analyses,ipqp_solves,ipqp_pattern_verifies,"
          "ipqp_rho_demanded_max,ipqp_rho_demanded_last,ipqp_inertia_retries,"
          "ipqp_iters_at_elevated_rho,ipqp_ladder_reclimbs,ipqp_pivot_reroute_primal,"
          "ipqp_pivot_reroute_dual_fallback,ipqp_iters_ladder_armed_no_advance,"
          "ipqp_final_inertia_read,"
          "ipqp_reg_decreases,ipqp_reg_increases,ipqp_prox_center_updates,ipqp_restart_repairs,"
          "ipqp_restart_shift_max,ipqp_mu_adopted,ipqp_warm_restart_abandoned,ipqp_declined_pinned,"
          "ipqp_tier_retired_after,ipqp_face_uncertain,ipqp_refine_accepted,ipqp_refine_refused,"
          "ipqp_to_refine,ipqp_to_ssn,ipqp_to_walk,ipqp_escapes,ipqp_escape_budget,"
          "ipqp_escape_stall,"
          "ipqp_escape_indefinite,ipqp_escape_numerical,ipqp_escape_infeasible_suspect,"
          "ipqp_alpha_p_min,ipqp_alpha_d_min,ipqp_read_kept_tight_sides,"
          "ipqp_read_barrier_noise_sides\n";
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
                          "-1,-1,-1,-1,-1,-1,"
                          "-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,"
                          "-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1\n",
                          cell.id, to_string(cell.family), cell.n_nodes, to_string(cell.ctag),
                          to_string(cell.start), cell.degenerate ? 1 : 0,
                          out.engine_error ? hven::solvers::corpus::kEngineErrorStatusString
                                           : dnf_phase_status_string(out.dnf_phase),
                          out.dnf_wall_s);
        return;
    }
    const CorpusRow &row = out.row;
    os << fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{:.9e},{:.9f},"
        "{},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{:.9e},{},"
        "{},{},{},{},{},{},{},{},{},"
        "{},{},{},{},{},{},"
        "{},{},{},{},{},{:.9e},{:.9e},{},{},{},{},{},"
        "{},{},{},{},{},{},{:.9e},{},{},{},{},{},"
        "{},{},{},{},{},{},{},{},{},{},{},{:.9e},"
        "{:.9e},{},{}\n",
        row.cell_id, to_string(cell.family), cell.n_nodes, to_string(cell.ctag),
        to_string(cell.start), cell.degenerate ? 1 : 0, to_string(row.status), row.factorizations,
        row.qp_minors, row.escapes, row.qp_factorizations.size(),
        join_qp_factorizations(row.qp_factorizations), row.kkt_residual, row.wall_s,
        to_string(kkt_gate_verdict(row)), row.kkt_stationarity, row.kkt_primal, row.kkt_dual_sign,
        row.kkt_complementarity, row.dual_scale, row.x_scale, row.neg_ineq_duals, row.ssn.ssn_iters,
        row.ssn.ssn_bulk_flips, row.ssn.ssn_backtracks, row.ssn.ssn_prox_updates,
        row.ssn.ssn_uncertain_peak, row.ssn.ssn_refinements, row.ssn.ssn_refine_refused,
        row.ssn.ssn_refine_factorizations, row.ssn.ssn_refine_neg_duals, row.ssn.ssn_escape_budget,
        row.ssn.ssn_escape_singular, row.ssn.ssn_escape_no_contraction,
        row.ssn.ssn_escape_infeasible_suspect, row.ssn.ssn_escape_indefinite,
        row.ssn.ssn_escape_gate_refused, row.ipqp.ipqp_iters, row.ipqp.ipqp_factorizations,
        row.ipqp.ipqp_symbolic_analyses, row.ipqp.ipqp_solves, row.ipqp.ipqp_pattern_verifies,
        row.ipqp.ipqp_rho_demanded_max, row.ipqp.ipqp_rho_demanded_last,
        row.ipqp.ipqp_inertia_retries, row.ipqp.ipqp_iters_at_elevated_rho,
        row.ipqp.ipqp_ladder_reclimbs, row.ipqp.ipqp_pivot_reroute_primal,
        row.ipqp.ipqp_pivot_reroute_dual_fallback, row.ipqp.ipqp_iters_ladder_armed_no_advance,
        row.ipqp.ipqp_final_inertia_read, row.ipqp.ipqp_reg_decreases, row.ipqp.ipqp_reg_increases,
        row.ipqp.ipqp_prox_center_updates, row.ipqp.ipqp_restart_repairs,
        row.ipqp.ipqp_restart_shift_max, row.ipqp.ipqp_mu_adopted,
        row.ipqp.ipqp_warm_restart_abandoned, row.ipqp.ipqp_declined_pinned,
        row.ipqp.ipqp_tier_retired_after, row.ipqp.ipqp_face_uncertain,
        row.ipqp.ipqp_refine_accepted, row.ipqp.ipqp_refine_refused, row.ipqp.ipqp_to_refine,
        row.ipqp.ipqp_to_ssn, row.ipqp.ipqp_to_walk, row.ipqp.ipqp_escapes,
        row.ipqp.ipqp_escape_budget, row.ipqp.ipqp_escape_stall, row.ipqp.ipqp_escape_indefinite,
        row.ipqp.ipqp_escape_numerical, row.ipqp.ipqp_escape_infeasible_suspect,
        row.ipqp.ipqp_alpha_p_min, row.ipqp.ipqp_alpha_d_min, row.ipqp.ipqp_read_kept_tight_sides,
        row.ipqp.ipqp_read_barrier_noise_sides);
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
        // A TASK-1-ERA artifact (14 columns, the committed walk baseline) reads
        // exactly as it always did and gets `unchecked` for the Task-6 tail;
        // nothing is padded any more -- the width test below is exact.
        // EXACT WIDTH PER GENERATION (fix round 1, Codex 2): a `>=` test read a
        // 38-to-75-column row as schema 37 and silently DISCARDED its partial
        // IPQP tail, re-emitting all 39 counters as absent `-1`.
        const std::size_t width = col.size();
        if (width != static_cast<std::size_t>(kTask1Columns) &&
            width != static_cast<std::size_t>(kAllColumns) &&
            width != static_cast<std::size_t>(kTask6bColumns) &&
            width != static_cast<std::size_t>(kIpqpColumns)) {
            throw std::invalid_argument(fmt::format(
                "{}: {} fields is not a known corpus CSV schema (14, 31, 37 or {}) -- a partial "
                "tail must never be scored",
                where, width, kIpqpColumns));
        }
        const bool has_task6_tail = width >= static_cast<std::size_t>(kAllColumns);
        const bool has_task6b_tail = width >= static_cast<std::size_t>(kTask6bColumns);
        const bool has_ipqp_tail = width >= static_cast<std::size_t>(kIpqpColumns);
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
        // The IPQP census on the SAME optional-tail contract: an older
        // artifact carries no IPQP columns and reads ABSENT (-1), never the
        // zero that would say "the tier ran and did nothing".
        if (!has_ipqp_tail) {
            o.row.ipqp.ipqp_iters = -1;
            o.row.ipqp.ipqp_factorizations = -1;
            o.row.ipqp.ipqp_symbolic_analyses = -1;
            o.row.ipqp.ipqp_solves = -1;
            o.row.ipqp.ipqp_pattern_verifies = -1;
            o.row.ipqp.ipqp_rho_demanded_max = -1.0;
            o.row.ipqp.ipqp_rho_demanded_last = -1.0;
            o.row.ipqp.ipqp_inertia_retries = -1;
            o.row.ipqp.ipqp_iters_at_elevated_rho = -1;
            o.row.ipqp.ipqp_ladder_reclimbs = -1;
            o.row.ipqp.ipqp_pivot_reroute_primal = -1;
            o.row.ipqp.ipqp_pivot_reroute_dual_fallback = -1;
            o.row.ipqp.ipqp_iters_ladder_armed_no_advance = -1;
            o.row.ipqp.ipqp_final_inertia_read = -1;
            o.row.ipqp.ipqp_reg_decreases = -1;
            o.row.ipqp.ipqp_reg_increases = -1;
            o.row.ipqp.ipqp_prox_center_updates = -1;
            o.row.ipqp.ipqp_restart_repairs = -1;
            o.row.ipqp.ipqp_restart_shift_max = -1.0;
            o.row.ipqp.ipqp_mu_adopted = -1;
            o.row.ipqp.ipqp_warm_restart_abandoned = -1;
            o.row.ipqp.ipqp_declined_pinned = -1;
            o.row.ipqp.ipqp_tier_retired_after = -1;
            o.row.ipqp.ipqp_face_uncertain = -1;
            o.row.ipqp.ipqp_refine_accepted = -1;
            o.row.ipqp.ipqp_refine_refused = -1;
            o.row.ipqp.ipqp_to_refine = -1;
            o.row.ipqp.ipqp_to_ssn = -1;
            o.row.ipqp.ipqp_to_walk = -1;
            o.row.ipqp.ipqp_escapes = -1;
            o.row.ipqp.ipqp_escape_budget = -1;
            o.row.ipqp.ipqp_escape_stall = -1;
            o.row.ipqp.ipqp_escape_indefinite = -1;
            o.row.ipqp.ipqp_escape_numerical = -1;
            o.row.ipqp.ipqp_escape_infeasible_suspect = -1;
            o.row.ipqp.ipqp_alpha_p_min = -1.0;
            o.row.ipqp.ipqp_alpha_d_min = -1.0;
            o.row.ipqp.ipqp_read_kept_tight_sides = -1;
            o.row.ipqp.ipqp_read_barrier_noise_sides = -1;
        } else {
            o.row.ipqp.ipqp_iters = parse_int_field(where + " column ipqp_iters", col[37]);
            o.row.ipqp.ipqp_factorizations =
                parse_int_field(where + " column ipqp_factorizations", col[38]);
            o.row.ipqp.ipqp_symbolic_analyses =
                parse_int_field(where + " column ipqp_symbolic_analyses", col[39]);
            o.row.ipqp.ipqp_solves = parse_int_field(where + " column ipqp_solves", col[40]);
            o.row.ipqp.ipqp_pattern_verifies =
                parse_int_field(where + " column ipqp_pattern_verifies", col[41]);
            o.row.ipqp.ipqp_rho_demanded_max =
                parse_double_field(where + " column ipqp_rho_demanded_max", col[42]);
            o.row.ipqp.ipqp_rho_demanded_last =
                parse_double_field(where + " column ipqp_rho_demanded_last", col[43]);
            o.row.ipqp.ipqp_inertia_retries =
                parse_int_field(where + " column ipqp_inertia_retries", col[44]);
            o.row.ipqp.ipqp_iters_at_elevated_rho =
                parse_int_field(where + " column ipqp_iters_at_elevated_rho", col[45]);
            o.row.ipqp.ipqp_ladder_reclimbs =
                parse_int_field(where + " column ipqp_ladder_reclimbs", col[46]);
            o.row.ipqp.ipqp_pivot_reroute_primal =
                parse_int_field(where + " column ipqp_pivot_reroute_primal", col[47]);
            o.row.ipqp.ipqp_pivot_reroute_dual_fallback =
                parse_int_field(where + " column ipqp_pivot_reroute_dual_fallback", col[48]);
            o.row.ipqp.ipqp_iters_ladder_armed_no_advance =
                parse_int_field(where + " column ipqp_iters_ladder_armed_no_advance", col[49]);
            o.row.ipqp.ipqp_final_inertia_read =
                parse_int_field(where + " column ipqp_final_inertia_read", col[50]);
            o.row.ipqp.ipqp_reg_decreases =
                parse_int_field(where + " column ipqp_reg_decreases", col[51]);
            o.row.ipqp.ipqp_reg_increases =
                parse_int_field(where + " column ipqp_reg_increases", col[52]);
            o.row.ipqp.ipqp_prox_center_updates =
                parse_int_field(where + " column ipqp_prox_center_updates", col[53]);
            o.row.ipqp.ipqp_restart_repairs =
                parse_int_field(where + " column ipqp_restart_repairs", col[54]);
            o.row.ipqp.ipqp_restart_shift_max =
                parse_double_field(where + " column ipqp_restart_shift_max", col[55]);
            o.row.ipqp.ipqp_mu_adopted =
                parse_int_field(where + " column ipqp_mu_adopted", col[56]);
            o.row.ipqp.ipqp_warm_restart_abandoned =
                parse_int_field(where + " column ipqp_warm_restart_abandoned", col[57]);
            o.row.ipqp.ipqp_declined_pinned =
                parse_int_field(where + " column ipqp_declined_pinned", col[58]);
            o.row.ipqp.ipqp_tier_retired_after =
                parse_int_field(where + " column ipqp_tier_retired_after", col[59]);
            o.row.ipqp.ipqp_face_uncertain =
                parse_int_field(where + " column ipqp_face_uncertain", col[60]);
            o.row.ipqp.ipqp_refine_accepted =
                parse_int_field(where + " column ipqp_refine_accepted", col[61]);
            o.row.ipqp.ipqp_refine_refused =
                parse_int_field(where + " column ipqp_refine_refused", col[62]);
            o.row.ipqp.ipqp_to_refine = parse_int_field(where + " column ipqp_to_refine", col[63]);
            o.row.ipqp.ipqp_to_ssn = parse_int_field(where + " column ipqp_to_ssn", col[64]);
            o.row.ipqp.ipqp_to_walk = parse_int_field(where + " column ipqp_to_walk", col[65]);
            o.row.ipqp.ipqp_escapes = parse_int_field(where + " column ipqp_escapes", col[66]);
            o.row.ipqp.ipqp_escape_budget =
                parse_int_field(where + " column ipqp_escape_budget", col[67]);
            o.row.ipqp.ipqp_escape_stall =
                parse_int_field(where + " column ipqp_escape_stall", col[68]);
            o.row.ipqp.ipqp_escape_indefinite =
                parse_int_field(where + " column ipqp_escape_indefinite", col[69]);
            o.row.ipqp.ipqp_escape_numerical =
                parse_int_field(where + " column ipqp_escape_numerical", col[70]);
            o.row.ipqp.ipqp_escape_infeasible_suspect =
                parse_int_field(where + " column ipqp_escape_infeasible_suspect", col[71]);
            o.row.ipqp.ipqp_alpha_p_min =
                parse_double_field(where + " column ipqp_alpha_p_min", col[72]);
            o.row.ipqp.ipqp_alpha_d_min =
                parse_double_field(where + " column ipqp_alpha_d_min", col[73]);
            o.row.ipqp.ipqp_read_kept_tight_sides =
                parse_int_field(where + " column ipqp_read_kept_tight_sides", col[74]);
            o.row.ipqp.ipqp_read_barrier_noise_sides =
                parse_int_field(where + " column ipqp_read_barrier_noise_sides", col[75]);
            // THE FIVE-WAY ESCAPE CENSUS MAY NOT DISAGREE WITH ITS OWN TOTAL,
            // for the reason the SSN census may not (spec section 7).
            const hven::Index ipqp_census =
                o.row.ipqp.ipqp_escape_budget + o.row.ipqp.ipqp_escape_stall +
                o.row.ipqp.ipqp_escape_indefinite + o.row.ipqp.ipqp_escape_numerical +
                o.row.ipqp.ipqp_escape_infeasible_suspect;
            if (ipqp_census != o.row.ipqp.ipqp_escapes) {
                throw std::invalid_argument(fmt::format(
                    "{}: the IPQP escape census sums to {} but `ipqp_escapes` says {} -- the "
                    "artifact is internally inconsistent and must not be scored",
                    where, ipqp_census, o.row.ipqp.ipqp_escapes));
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

// The model-surface census hook's OWN small sidecar format
// (docs/notes/2026-08-21-m4-task5-design.md), entirely separate from
// write_outcome/read_outcomes_csv's 31/37-column artifact contract -- see
// corpus_cells.h's CorpusRow::ms_* note for
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
    // The model-surface sidecar's own path, written by the child only when
    // `levers.score_model_surface` is set (see
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

// --score-model-surface's own artifact (docs/notes/2026-08-21-m4-task5-design.md),
// written ONLY when that flag is on -- main() below never calls this
// otherwise. One row
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

// =============================================================================
// THE HS SUITE — M6 W5 T6.d's LEG 2 (ownership doc §11.3, "an HS-scale leg,
// which (d) cannot do without").
// =============================================================================
//
// WHY IT IS HERE RATHER THAN IN A TEST BINARY. Every U0 corpus cell is F7 at
// n >= 800, where the QP dominates and a de-inlined per-major helper is
// invisible; the plan's "loop-used arithmetic is not cold" is a statement about
// MAJORS, not about n. So T6.d needs a leg whose cells are small and
// major-dense. A TEST target is a different link and a different flag surface,
// which is a poor instrument for a veto-grade neutrality claim -- and it is
// unnecessary, because bench/CMakeLists.txt already puts tests/sqp on this
// target's include path (bench/ipqp_e1_arm.cpp includes the same header from a
// bench target today). So: ONE binary, the bench flag regime, and the corpus
// path below untouched.
//
// THE TERMS THIS CODE EXISTS TO MEET, all from §11.3 and the (d) design review:
//
//   * IT LANDS ONCE, BEFORE ANY T6.d NUMBER IS TAKEN. A `--repeat` added
//     between arms VOIDS the leg -- the two arms must differ ONLY in the
//     library linked under them, and the harness SOURCE and CONFIGURATION are
//     what is held identical (each arm is separately LINKED, with its
//     executable hash retained; one executable cannot run two static library
//     implementations, which is why the "identical binary" term was amended).
//
//   * BOTH TRACE-ENABLED MAPPER SITES MUST EXECUTE. `trace_outcome_of` -- the
//     one symbol cut (d) gives external linkage, and the only one of the six
//     with no standalone symbol in today's object -- is called from the driver
//     at exactly two sites, and BOTH are inside `if (ipqp_trace_ != nullptr)`.
//     A null-sink leg therefore measures the split's clearest new call exposure
//     exactly ZERO TIMES. `--hs-trace sink` attaches a real sink; the per-site
//     event counts below are how a reader PROVES each site fired rather than
//     assuming it. Null-sink and sink rows carry a `trace` column so the two
//     populations stay separately identifiable and are never averaged together.
//
//   * THE FALLBACK/LADDER PATH MUST BE SHOWN TO HAVE FIRED. Four counters ride
//     on every row (`elastic_activations`, `elastic_escalations`,
//     `elastic_from_ipqp_escape`, `ipqp_fallback_rung_b`). A cell that does not
//     fire the path is not evidence about it.
//
//   * AGGREGATION IS MEDIAN-OF-N PER CELL, and the corpus figure is the SUM of
//     per-cell medians -- never a mean of ratios. `--repeat` is calibrated by
//     raising N until the A-arm-alone `median_se_pct` is inside +/-0.5 %; that
//     column, NOT `spread_pct`, is what the calibration reads (see run_hs_cell).
//
// NO DEADLINE MACHINERY. The HS cells run in milliseconds, so the fork/exec
// wall-deadline this file wraps the corpus arms in would cost more than the
// measurement and would put a process spawn inside the timed region. This path
// is deliberately IN-PROCESS. It is also not gate-scored: it produces timings
// and counters, and nothing here decides a verdict.

/// @brief A real sink that COUNTS, per site, and keeps nothing else.
///
/// It must do real work at the call -- a sink whose overrides were empty could
/// be optimised into nothing and would not prove the emit path ran -- but it
/// must also not dominate the timing it is there to enable, so it counts and
/// returns. The per-site counts ARE the evidence that each mapper site fired.
class CountingTraceSink final : public TraceSink {
  public:
    void on_ipqp_iter(const hven::solvers::IpqpTraceIterEvent &) override { ++ipqp_iter_; }
    void on_ipqp_reg(const hven::solvers::IpqpTraceRegEvent &) override { ++other_; }
    void on_ipqp_restart(const hven::solvers::IpqpTraceRestartEvent &) override { ++other_; }
    void on_ipqp_route(const hven::solvers::IpqpTraceRouteEvent &) override { ++other_; }
    void on_ipqp_certify(const hven::solvers::IpqpTraceCertifyEvent &) override { ++other_; }
    void on_ipqp_escape(const hven::solvers::IpqpTraceEscapeEvent &) override { ++other_; }
    void on_fallback_verdict(const hven::solvers::SqpFallbackVerdictTraceEvent &) override {
        ++fallback_verdict_;
    }
    void on_sqp_major(const hven::solvers::SqpMajorTraceEvent &) override { ++other_; }
    void on_sqp_solve_begin(const hven::solvers::SqpSolveBeginTraceEvent &) override { ++other_; }
    void on_sqp_solve_end(const hven::solvers::SqpSolveEndTraceEvent &) override { ++other_; }
    void on_ipm_iter(const hven::solvers::IpmIterTraceEvent &) override { ++other_; }
    void on_ipm_solve_begin(const hven::solvers::IpmSolveBeginTraceEvent &) override { ++other_; }
    void on_ipm_solve_end(const hven::solvers::IpmSolveEndTraceEvent &) override { ++other_; }

    /// THE ONE THAT MATTERS. `qp.mode` is the event both driver-side
    /// `trace_outcome_of` calls construct and both kernels-side
    /// `emit_qp_mode_line` calls construct, and `site` is what tells them
    /// apart -- so this counter, split by site, is the whole proof that the
    /// mapper's call sites executed.
    void on_qp_mode(const QpModeTraceEvent &event) override {
        switch (event.site) {
        case QpModeSite::kDispatch:
            ++dispatch;
            break;
        case QpModeSite::kSsnWarmGrade:
            ++ssn_warm_grade;
            break;
        case QpModeSite::kFallbackRungB:
            ++fallback_rung_b;
            break;
        case QpModeSite::kElasticRung:
            ++elastic_rung;
            break;
        case QpModeSite::kSocResolve:
            ++soc_resolve;
            break;
        }
    }

    /// Zeroed explicitly rather than by assigning a fresh instance: this type
    /// is polymorphic, and copy-assigning a polymorphic object to reset it is
    /// the kind of clever that stops being true when someone adds a member.
    void reset() {
        dispatch = 0;
        soc_resolve = 0;
        elastic_rung = 0;
        fallback_rung_b = 0;
        ssn_warm_grade = 0;
        ipqp_iter_ = 0;
        fallback_verdict_ = 0;
        other_ = 0;
    }

    long long dispatch = 0;
    long long soc_resolve = 0;
    long long elastic_rung = 0;
    long long fallback_rung_b = 0;
    long long ssn_warm_grade = 0;

  private:
    long long ipqp_iter_ = 0;
    long long fallback_verdict_ = 0;
    long long other_ = 0;
};

QpMode hs_qp_mode(const std::string &engine) {
    if (engine == "walk") {
        return QpMode::kWalk;
    }
    if (engine == "ssn") {
        return QpMode::kSsn;
    }
    if (engine == "ipm") {
        return QpMode::kIpm;
    }
    throw std::invalid_argument(
        fmt::format("hven_sqp_corpus: --hs --engine: '{}' is not one of walk|ssn|ipm", engine));
}

/// One HS cell's measured row. Every counter is the LAST repeat's, and every
/// repeat of one cell is asserted to produce the same ones (see run_hs_suite):
/// a cell whose counters move between repeats is not a stable timing cell and
/// says so rather than reporting a median over two different computations.
struct HsRow {
    int number = 0;
    double wall_median_s = 0.0;
    double wall_min_s = 0.0;
    double wall_max_s = 0.0;
    double spread_pct = 0.0;
    double median_se_pct = 0.0;
    bool counters_stable = true;
    hven::solvers::SqpStatus status = hven::solvers::SqpStatus::kOptimal;
    double f = 0.0;
    long long majors = 0;
    long long qp_minors = 0;
    long long factorizations = 0;
    long long soc_steps = 0;
    long long soc_applied = 0;
    long long elastic_activations = 0;
    long long elastic_escalations = 0;
    long long elastic_from_ipqp_escape = 0;
    long long ipqp_fallback_rung_b = 0;
    long long history_rows = 0;
    long long ev_dispatch = 0;
    long long ev_soc_resolve = 0;
    long long ev_elastic_rung = 0;
    long long ev_fallback_rung_b = 0;
    long long ev_ssn_warm_grade = 0;
};

double median_of(std::vector<double> v) {
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    if (n == 0) {
        return 0.0;
    }
    return n % 2 == 1 ? v[n / 2] : 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

std::vector<int> resolve_hs_cells(const std::string &spec) {
    const std::vector<int> &shipped = hven::solvers::test_support::hs_numbers();
    if (spec == "all") {
        return shipped;
    }
    std::vector<int> out;
    for (const std::string &tok : split_on(spec, ',')) {
        const int n = parse_int_field("--hs-cells", tok);
        if (std::find(shipped.begin(), shipped.end(), n) == shipped.end()) {
            throw_usage(fmt::format("--hs-cells: problem {} is not shipped by "
                                    "tests/sqp/support/hs_problems.h",
                                    n));
        }
        out.push_back(n);
    }
    if (out.empty()) {
        throw_usage("--hs-cells: resolved to zero cells");
    }
    return out;
}

/// Solve ONE HS problem `repeat` times and reduce to one row.
///
/// THE MODEL IS REBUILT PER REPEAT, deliberately: a solve is entitled to leave
/// state in the model it was handed, and reusing one across repeats would make
/// repeat k a different computation from repeat 1 -- which the counter-stability
/// check below would then report as an instability that is really the harness's.
/// Construction is outside the timed region.
HsRow run_hs_cell(int number, QpMode mode, int repeat, int warmup, CountingTraceSink *sink) {
    HsRow row;
    row.number = number;
    // THE WARM-UP SOLVES, discarded: same model, same options, same sink, same
    // code path -- only the clock is not read. See Args::hs_warmup for the
    // measurement that made this necessary.
    for (int w = 0; w < warmup; ++w) {
        hven::solvers::test_support::HsProblem hs = hven::solvers::test_support::make_hs(number);
        SqpOptions opts;
        opts.qp_mode = mode;
        SqpDriver driver(opts);
        if (sink != nullptr) {
            sink->reset();
            driver.attach_trace(sink);
        }
        const SqpSolution discarded = driver.solve(*hs.model);
        (void)discarded;
    }
    std::vector<double> walls;
    walls.reserve(static_cast<std::size_t>(repeat));
    hven::solvers::SqpCounters first_counters;
    for (int r = 0; r < repeat; ++r) {
        hven::solvers::test_support::HsProblem hs = hven::solvers::test_support::make_hs(number);
        SqpOptions opts;
        opts.qp_mode = mode;
        SqpDriver driver(opts);
        if (sink != nullptr) {
            sink->reset();
            driver.attach_trace(sink);
        }
        const auto t0 = std::chrono::steady_clock::now();
        const SqpSolution sol = driver.solve(*hs.model);
        const auto t1 = std::chrono::steady_clock::now();
        walls.push_back(std::chrono::duration<double>(t1 - t0).count());

        if (r == 0) {
            first_counters = sol.counters;
        } else if (sol.counters.major_iters != first_counters.major_iters ||
                   sol.counters.qp_minor_iters != first_counters.qp_minor_iters ||
                   sol.counters.factorizations != first_counters.factorizations ||
                   sol.counters.elastic_activations != first_counters.elastic_activations) {
            row.counters_stable = false;
        }
        row.status = sol.status;
        row.f = sol.f;
        row.majors = static_cast<long long>(sol.counters.major_iters);
        row.qp_minors = static_cast<long long>(sol.counters.qp_minor_iters);
        row.factorizations = static_cast<long long>(sol.counters.factorizations);
        row.soc_steps = static_cast<long long>(sol.counters.soc_steps);
        row.soc_applied = static_cast<long long>(sol.counters.soc_applied);
        row.elastic_activations = static_cast<long long>(sol.counters.elastic_activations);
        row.elastic_escalations = static_cast<long long>(sol.counters.elastic_escalations);
        row.elastic_from_ipqp_escape =
            static_cast<long long>(sol.counters.elastic_from_ipqp_escape);
        row.ipqp_fallback_rung_b = static_cast<long long>(sol.counters.ipqp_fallback_rung_b);
        row.history_rows = static_cast<long long>(sol.history.size());
        if (sink != nullptr) {
            row.ev_dispatch = sink->dispatch;
            row.ev_soc_resolve = sink->soc_resolve;
            row.ev_elastic_rung = sink->elastic_rung;
            row.ev_fallback_rung_b = sink->fallback_rung_b;
            row.ev_ssn_warm_grade = sink->ssn_warm_grade;
        }
    }
    row.wall_median_s = median_of(walls);
    row.wall_min_s = *std::min_element(walls.begin(), walls.end());
    row.wall_max_s = *std::max_element(walls.begin(), walls.end());
    // TWO DISPERSION READINGS, AND ONLY THE SECOND IS THE CALIBRATION ONE.
    //
    // `spread_pct` is the full max-min range over the median: the raw sample
    // cloud, kept because it is what an outlier shows up in.
    //
    // IT IS NOT A CALIBRATION STATISTIC, and that was MEASURED rather than
    // reasoned about. max-min is monotonically NON-DECREASING in N by
    // construction -- more samples are more chances to catch a straggler -- so
    // "raise N until the spread is inside +/-0.5 %" cannot converge. On this
    // box, ipm, warmup 1, the worst per-cell reading went 0.000 % at N=1
    // (trivially: max == min == the one sample), 7.8 % at N=3, 8.2 % at N=5,
    // 44.4 % at N=10, 21.2 % at N=20. Raising N makes that number WORSE.
    //
    // `median_se_pct` is the standard error OF THE REPORTED MEDIAN as a
    // percentage of it -- 1.2533 * sigma / sqrt(N), the large-sample standard
    // error of a sample median, over the median. It answers the question the
    // calibration is actually asking: how tightly is the number this row
    // REPORTS pinned down, and therefore how much of an A-vs-B difference is
    // real. It falls as 1/sqrt(N), so it converges, and it is the column N is
    // raised against.
    //
    // Neither replaces the empirical check: the arm is also run three times
    // and its per-cell MEDIANS compared, which is the reproducibility the
    // comparison actually rests on and assumes no distribution at all.
    row.spread_pct = row.wall_median_s > 0.0
                         ? 100.0 * (row.wall_max_s - row.wall_min_s) / row.wall_median_s
                         : 0.0;
    if (walls.size() > 1 && row.wall_median_s > 0.0) {
        double mean = 0.0;
        for (const double w : walls) {
            mean += w;
        }
        mean /= static_cast<double>(walls.size());
        double ss = 0.0;
        for (const double w : walls) {
            ss += (w - mean) * (w - mean);
        }
        const double sigma = std::sqrt(ss / static_cast<double>(walls.size() - 1));
        row.median_se_pct = 100.0 * 1.2533 * sigma /
                            (std::sqrt(static_cast<double>(walls.size())) * row.wall_median_s);
    }
    return row;
}

std::string hs_invocation(int argc, char **argv) {
    std::string s;
    for (int i = 0; i < argc; ++i) {
        s += (i == 0 ? "" : " ");
        s += argv[i];
    }
    return s;
}

std::string hs_host() {
    char host[256] = {0};
    if (::gethostname(host, sizeof(host) - 1) != 0 || host[0] == '\0') {
        return "<unknown>";
    }
    return host;
}

std::string hs_utc_stamp() {
    const std::time_t now = std::time(nullptr);
    char stamp[64] = {0};
    std::tm utc{};
    if (::gmtime_r(&now, &utc) == nullptr ||
        std::strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%SZ", &utc) == 0) {
        return "<unknown>";
    }
    return stamp;
}

/// The HS artifact's own provenance block. SEPARATE from `write_provenance`
/// above, and deliberately not a refactor of it: that function stamps the
/// corpus schema width and the budget-table hash, neither of which means
/// anything here, and it is the producer of PINNED committed artifacts. A
/// measurement commit does not edit the code that writes pinned evidence.
void write_hs_provenance(std::ostream &os, int argc, char **argv, const std::string &engine,
                         int repeat, int warmup, bool trace_sink) {
    const char *mkl = std::getenv("MKL_NUM_THREADS");
    const char *omp = std::getenv("OMP_NUM_THREADS");
    os << "# hven_sqp_corpus HS-suite provenance (M6 W5 T6.d leg 2)\n";
    os << fmt::format("# binary: {}\n", HVEN_SQP_CORPUS_GIT_DESCRIBE);
    os << fmt::format("# engine: {}\n", engine);
    os << fmt::format("# repeat: {}\n", repeat);
    os << fmt::format("# warmup: {} (untimed solves per cell, discarded)\n", warmup);
    os << fmt::format("# trace: {}\n", trace_sink ? "sink" : "off");
    os << fmt::format("# invocation: {}\n", hs_invocation(argc, argv));
    os << fmt::format("# MKL_NUM_THREADS: {}\n", mkl == nullptr ? "<unset>" : mkl);
    os << fmt::format("# OMP_NUM_THREADS: {}\n", omp == nullptr ? "<unset>" : omp);
    os << fmt::format("# host: {}\n", hs_host());
    os << fmt::format("# generated: {}\n", hs_utc_stamp());
    os << "# cells: tests/sqp/support/hs_problems.h hs_numbers()\n";
    os << "# aggregation: wall_median_s is the MEDIAN of `repeat` in-process solves; the corpus\n";
    os << "#              figure is the SUM of per-cell medians, never a mean of ratios.\n";
    os << "# NOT gate-scored, and no wall-deadline machinery: these cells run in milliseconds.\n";
    os << "# Wall-clock here is quotable ONLY under CLAUDE.md section 7's terms -- solo, pinned,\n";
    os << "# MKL_NUM_THREADS=1, OMP_NUM_THREADS=1, one process, alternating arms.\n";
}

void write_hs_header(std::ostream &os) {
    os << "hs,engine,trace,repeat,warmup,wall_median_s,wall_min_s,wall_max_s,spread_pct,"
          "median_se_pct,"
          "counters_stable,status,f,majors,qp_minors,factorizations,soc_steps,soc_applied,"
          "elastic_activations,elastic_escalations,elastic_from_ipqp_escape,ipqp_fallback_rung_b,"
          "history_rows,ev_dispatch,ev_soc_resolve,ev_elastic_rung,ev_fallback_rung_b,"
          "ev_ssn_warm_grade\n";
}

void write_hs_row(std::ostream &os, const HsRow &r, const std::string &engine, bool trace_sink,
                  int repeat, int warmup) {
    os << fmt::format("{},{},{},{},{},{:.9e},{:.9e},{:.9e},{:.6f},{:.6f},{},{},{:.17g},{},{},{},{},"
                      "{},{},{},{},{},{},{},{},{},{},{}\n",
                      r.number, engine, trace_sink ? "sink" : "off", repeat, warmup,
                      r.wall_median_s, r.wall_min_s, r.wall_max_s, r.spread_pct, r.median_se_pct,
                      r.counters_stable ? 1 : 0, to_string(r.status), r.f, r.majors, r.qp_minors,
                      r.factorizations, r.soc_steps, r.soc_applied, r.elastic_activations,
                      r.elastic_escalations, r.elastic_from_ipqp_escape, r.ipqp_fallback_rung_b,
                      r.history_rows, r.ev_dispatch, r.ev_soc_resolve, r.ev_elastic_rung,
                      r.ev_fallback_rung_b, r.ev_ssn_warm_grade);
}

// =============================================================================
// THE TOP-LEVEL INTERIOR-POINT LEG (--engine interior).
// =============================================================================
//
// A SIBLING provenance writer, not an extension of write_provenance: that one
// stamps the SSN measurement levers over the corpus's 76-column schema, and
// this arm shares neither. Every lever a row depends on is written here, so a
// reader never has to know which defaults were in force when it was captured.

void write_interior_provenance(std::ostream &os, int argc, char **argv,
                               const InteriorLevers &levers,
                               const std::vector<std::string> &refusals) {
    std::string invocation;
    for (int i = 0; i < argc; ++i) {
        invocation += (i == 0 ? "" : " ");
        invocation += argv[i];
    }
    const char *mkl = std::getenv("MKL_NUM_THREADS");
    const char *omp = std::getenv("OMP_NUM_THREADS");
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
    os << "# hven_sqp_corpus provenance -- ENGINE interior (top-level interior-point driver)\n";
    os << fmt::format("# binary: {}\n", HVEN_SQP_CORPUS_GIT_DESCRIBE);
    os << "# schema: 19\n";
    os << fmt::format("# invocation: {}\n", invocation);
    os << fmt::format("# MKL_NUM_THREADS: {}\n", mkl == nullptr ? "<unset>" : mkl);
    os << fmt::format("# OMP_NUM_THREADS: {}\n", omp == nullptr ? "<unset>" : omp);
    os << fmt::format("# host: {}\n", host[0] == '\0' ? "<unknown>" : host);
    os << fmt::format("# generated: {}\n", stamp[0] == '\0' ? "<unknown>" : stamp);
    os << fmt::format("# levers: max_iters={} print_level={} partitions={} qp_threads={}\n",
                      levers.max_iters, levers.print_level, levers.num_partitions,
                      levers.qp_threads);
    os << fmt::format("# levers: kkt_tol={:.9e} econ_tol={:.9e} icon_tol={:.9e} barr_tol={:.9e}\n",
                      levers.kkt_tol, levers.econ_tol, levers.icon_tol, levers.barr_tol);
    os << fmt::format("# levers: bound_relax_factor={:.9e}\n", levers.bound_relax_factor);
    os << "# treatments: MakeParameter,MakeConstraint,RelaxBounds -- one row each, per cell\n";
    os << "# key: column 0 is <cell_id>/<fixed_treatment>, unique per row\n";
    for (const std::string &refusal : refusals) {
        os << fmt::format("# refused: {}\n", refusal);
    }
}

// The cells this arm runs, and the refusal line for every requested cell it
// cannot: a cell an NLPProblem cannot state is named, never silently dropped.
struct InteriorPlan {
    std::vector<const CorpusCell *> cells;
    std::vector<std::string> refusals;
};

InteriorPlan plan_interior_cells(const std::string &spec) {
    InteriorPlan plan;
    std::vector<const CorpusCell *> requested;
    if (spec == "all") {
        for (const CorpusCell &c : all_cells()) {
            requested.push_back(&c);
        }
    } else {
        for (const std::string &id : split_on(spec, ',')) {
            // The fixed-variable cell runs unconditionally, so naming it is
            // accepted and adds nothing rather than reading as an unknown id.
            if (id == kHs071FixedCellId) {
                continue;
            }
            const CorpusCell *c = find_cell(id);
            if (c == nullptr) {
                throw_usage(fmt::format("--cells: unknown cell id '{}' (try --list; --engine "
                                        "interior also accepts '{}')",
                                        id, kHs071FixedCellId));
            }
            requested.push_back(c);
        }
    }
    for (const CorpusCell *c : requested) {
        const std::string why = hven::solvers::corpus::interior_cell_refusal(*c);
        if (why.empty()) {
            plan.cells.push_back(c);
        } else {
            plan.refusals.push_back(fmt::format("{} -- {}", c->id, why));
        }
    }
    return plan;
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

        // --------------------------------------------------------------
        // THE HS SUITE (M6 W5 T6.d leg 2). Its own path, ahead of every
        // corpus mode, and it shares nothing with them but --engine and
        // --csv. See the HS SUITE section for the terms.
        // --------------------------------------------------------------
        if (args.hs) {
            if (!args.engine || !args.csv) {
                throw_usage("--hs requires --engine and --csv");
            }
            // REFUSED, NOT IGNORED. A corpus flag that reads as accepted here
            // and does nothing is how an arm gets mislabelled in a report that
            // quotes its invocation line.
            if (args.cells || args.from_csv || args.dump_qp || args.dump_qp_out ||
                args.score_gates || args.score_model_surface || args.score_model_surface_out) {
                throw_usage("--hs runs the Hock-Schittkowski suite in process: it cannot be "
                            "combined with --cells/--from-csv/--dump-qp/--dump-qp-out/"
                            "--score-gates/--score-model-surface/--score-model-surface-out, none "
                            "of which has a meaning over these cells");
            }
            if (args.internal_run_one || args.internal_force_setup_budget_s ||
                args.internal_force_solve_budget_s || args.internal_force_child_throw ||
                args.internal_force_child_abort) {
                throw_usage("--hs takes none of the hidden internal/test levers: this path runs "
                            "in process and has no child to force, no deadline to override, and "
                            "produces timings that a forced fixture would silently corrupt");
            }
            const QpMode mode = hs_qp_mode(*args.engine);
            const std::vector<int> numbers = resolve_hs_cells(args.hs_cells.value_or("all"));
            CountingTraceSink sink;
            CountingTraceSink *sink_ptr = args.hs_trace_sink ? &sink : nullptr;

            std::ofstream out(*args.csv);
            if (!out) {
                throw std::invalid_argument(
                    fmt::format("--csv: could not open '{}' for writing", *args.csv));
            }
            write_hs_provenance(out, argc, argv, *args.engine, args.repeat, args.hs_warmup,
                                args.hs_trace_sink);
            write_hs_header(out);
            out.flush();

            double corpus_s = 0.0;
            double worst_spread = 0.0;
            double worst_se = 0.0;
            int unstable = 0;
            long long ev_dispatch = 0;
            long long ev_soc_resolve = 0;
            long long ev_elastic_rung = 0;
            long long ev_fallback_rung_b = 0;
            long long fired_elastic = 0;
            long long fired_rung_b = 0;
            for (const int number : numbers) {
                const HsRow row = run_hs_cell(number, mode, args.repeat, args.hs_warmup, sink_ptr);
                write_hs_row(out, row, *args.engine, args.hs_trace_sink, args.repeat,
                             args.hs_warmup);
                out.flush();
                corpus_s += row.wall_median_s;
                worst_spread = std::max(worst_spread, row.spread_pct);
                worst_se = std::max(worst_se, row.median_se_pct);
                unstable += row.counters_stable ? 0 : 1;
                ev_dispatch += row.ev_dispatch;
                ev_soc_resolve += row.ev_soc_resolve;
                ev_elastic_rung += row.ev_elastic_rung;
                ev_fallback_rung_b += row.ev_fallback_rung_b;
                fired_elastic += row.elastic_activations;
                fired_rung_b += row.ipqp_fallback_rung_b;
                fmt::print("hs{} {} trace={} median={:.6f}s spread={:.3f}% majors={} "
                           "elastic={} rung_b={}\n",
                           number, *args.engine, args.hs_trace_sink ? "sink" : "off",
                           row.wall_median_s, row.spread_pct, row.majors, row.elastic_activations,
                           row.ipqp_fallback_rung_b);
            }
            // THE CORPUS FIGURE IS THE SUM OF PER-CELL MEDIANS (§11.3), and
            // the worst per-cell spread is the calibration reading `--repeat`
            // is raised against.
            fmt::print("\nHS SUITE: {} cell(s), engine {}, trace {}, repeat {}, warmup {}\n",
                       numbers.size(), *args.engine, args.hs_trace_sink ? "sink" : "off",
                       args.repeat, args.hs_warmup);
            fmt::print("  corpus (sum of per-cell medians): {:.6f} s\n", corpus_s);
            fmt::print("  worst per-cell median SE: {:.3f} %   <-- THE CALIBRATION READING "
                       "(target <= 0.5, falls as 1/sqrt(N))\n",
                       worst_se);
            fmt::print("  worst per-cell max-min spread: {:.3f} %  (raw cloud; NOT a calibration\n"
                       "                                            statistic -- it is monotone "
                       "in N)\n",
                       worst_spread);
            fmt::print("  cells with unstable counters across repeats: {}\n", unstable);
            fmt::print("  elastic activations: {}   fallback rung B: {}\n", fired_elastic,
                       fired_rung_b);
            if (args.hs_trace_sink) {
                fmt::print("  qp.mode events by site: dispatch={} soc_resolve={} "
                           "elastic_rung={} fallback_rung_b={}\n",
                           ev_dispatch, ev_soc_resolve, ev_elastic_rung, ev_fallback_rung_b);
                // THE TWO DRIVER-SIDE MAPPER SITES, named. This is a REPORT,
                // never an exit code: a mode in which SOC legitimately never
                // fires is a fact about the cells, not a runner failure.
                if (ev_dispatch == 0) {
                    fmt::print("  NOTE: the kDispatch mapper site did NOT fire in this arm\n");
                }
                if (ev_soc_resolve == 0) {
                    fmt::print("  NOTE: the kSocResolve mapper site did NOT fire in this arm\n");
                }
            }
            fmt::print("wrote {} row(s) to {}\n", numbers.size(), *args.csv);
            return 0;
        }
        if (args.repeat != 1) {
            throw_usage("--repeat applies to the HS suite only (--hs); the corpus arms are "
                        "seconds-scale, already stable, and produce PINNED artifacts whose "
                        "producer this flag deliberately does not touch");
        }
        if (args.hs_cells || args.hs_trace_sink || args.hs_warmup != 1) {
            throw_usage("--hs-cells, --hs-trace and --hs-warmup apply to the HS suite only (--hs)");
        }

        // --------------------------------------------------------------
        // THE TOP-LEVEL INTERIOR-POINT LEG (M6 W5 T8.1). Its own path,
        // ahead of every SQP corpus mode: it shares --engine, --cells and
        // --csv with them and nothing else -- not the schema, not the
        // fork/exec wall deadline, and not the gate scorer.
        // --------------------------------------------------------------
        if (args.engine && *args.engine == "interior") {
            if (!args.cells || !args.csv) {
                throw_usage("--engine interior requires --cells and --csv");
            }
            // REFUSED, NOT IGNORED, on the --hs path's own reasoning: a flag
            // that reads as accepted and does nothing is how an arm gets
            // mislabelled in a report that quotes its invocation line.
            if (args.from_csv || args.score_gates || args.score_model_surface ||
                args.score_model_surface_out || args.dump_qp || args.dump_qp_out) {
                throw_usage("--engine interior writes its own 19-column schema, which none of "
                            "--from-csv/--score-gates/--score-model-surface/"
                            "--score-model-surface-out/--dump-qp can read or score: the offline "
                            "reader accepts the corpus widths 14/31/37/76 and the gates are "
                            "pre-registered on SQP columns this leg does not produce");
            }
            if (args.ssn_prox_carry || args.ssn_certify_from_face ||
                args.ssn_sigma_rule != SsnSigmaRule::kLadder ||
                args.ssn_hint_rule != SsnHintRule::kIterationZeroFree ||
                args.ssn_infeasibility_rule != SsnInfeasibilityRule::kSymptoms) {
                throw_usage("--engine interior takes none of the SSN measurement levers: they set "
                            "SqpOptions fields, and this arm runs no SqpDriver");
            }
            if (args.internal_force_setup_budget_s || args.internal_force_solve_budget_s ||
                args.internal_force_child_throw || args.internal_force_child_abort) {
                throw_usage("--engine interior takes none of the hidden internal/test levers: it "
                            "runs in process and has no child to force and no deadline to "
                            "override");
            }

            const InteriorLevers levers;
            const InteriorPlan plan = plan_interior_cells(*args.cells);

            std::ofstream out(*args.csv);
            if (!out) {
                throw std::invalid_argument(
                    fmt::format("--csv: could not open '{}' for writing", *args.csv));
            }
            write_interior_provenance(out, argc, argv, levers, plan.refusals);
            out << interior_csv_header();
            out.flush();

            for (const std::string &refusal : plan.refusals) {
                fmt::print("refused (not dual-bindable): {}\n", refusal);
            }

            std::size_t written = 0;
            for (const CorpusCell *cell : plan.cells) {
                for (const FixedVariableTreatments treatment : interior_treatments()) {
                    fmt::print("running {} (N={}, treatment {})...\n", cell->id, cell->n_nodes,
                               interior_treatment_tag(treatment));
                    const InteriorRow row = run_interior_cell(*cell, treatment, levers);
                    out << interior_csv_row(row);
                    out.flush();
                    ++written;
                    fmt::print("  -> {} in {} iterations (kkt_inf {:.3e})\n", row.status,
                               row.iter_num, row.kkt_inf);
                }
            }
            // The fixed-variable cell, always and last: no F7 cell has a
            // bound-fixed variable, so it is the only one on which the three
            // treatments take three different paths.
            for (const FixedVariableTreatments treatment : interior_treatments()) {
                fmt::print("running {} (treatment {})...\n", kHs071FixedCellId,
                           interior_treatment_tag(treatment));
                const InteriorRow row = run_interior_hs071(treatment, levers);
                out << interior_csv_row(row);
                out.flush();
                ++written;
                fmt::print("  -> {} in {} iterations (kkt_inf {:.3e})\n", row.status, row.iter_num,
                           row.kkt_inf);
            }
            fmt::print("wrote {} row(s) to {} ({} cell(s) refused)\n", written, *args.csv,
                       plan.refusals.size());
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
            if (args.engine || args.cells || args.score_model_surface ||
                args.score_model_surface_out) {
                throw_usage("--from-csv reads committed rows and runs nothing: it cannot be "
                            "combined with --engine/--cells/--score-model-surface/"
                            "--score-model-surface-out -- the census hook needs the solve's own "
                            "(x, lambda, z), which a committed CSV row does not carry, so scoring "
                            "it here would be silent nonsense rather than a genuine re-score");
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
