// bench/bench_scale.cpp — PHASE-5 TASK 2: the bench harness and wall-time
// instrumentation, the measurement instrument the rest of Phase 5 (Tasks 3,
// 4, 5, 9) reads. This is NOT a new algorithm: every number it reports comes
// from a Ledger (ledger.h) attached to an ordinary SqpDriver, exactly as
// tests/test_warm_start_battery.cpp's corpus does -- see that file's own
// EVERY COUNT IN THIS FILE COMES OFF A LEDGER note, which this harness
// deliberately follows rather than re-deriving its own counters.
//
// ---------------------------------------------------------------------
// THE FOUR ARMS -- what varies is the START, nothing else (same framing as
// the battery's own ARMS note):
//
//   cold   a FRESH SqpDriver (hence a fresh QpEngine, no retained K0) per
//          grid point, solved from the model's own start_point() via the
//          2-arg solve(). `--sweep N` is the LITERAL number of grid points,
//          evenly spaced over the family's fixed [p0, p1] (linspace, N >= 1;
//          N == 1 solves only at p1).
//   hot    ONE SqpDriver, constructed with SqpOptions::start_level = kHot,
//          walking the SAME evenly-spaced grid as `cold` but feeding each
//          solve's own WarmStart (primal/dual point AND the retained
//          factorization handle) into the next -- "chained solves on one
//          driver feeding warm+hot back". The first grid point is always a
//          plain 2-arg (cold) solve, since there is nothing to warm from yet.
//   warm   run_continuation (continuation.h) over [p0, p1] with
//          SqpOptions::start_level = kWarm and ContinuationOptions::
//          use_predictor = false. Unlike `cold`/`hot`, the ACTUAL number of
//          steps is continuation.h's own adaptive dp schedule, not `--sweep`
//          literally -- `--sweep` only seeds ContinuationOptions::dp_init
//          (as (p1 - p0) / (sweep - 1), i.e. the spacing the SAME-sized
//          `cold`/`hot` grid would use), which growth/shrink may then move
//          away from. This is a deliberate, documented divergence from the
//          other two arms' literal reading of `--sweep`; see --self-check
//          below for the one scenario that needs bit-exact reproduction of a
//          published step count, which does NOT go through this seeding path
//          at all.
//   pred   identical to `warm` with ContinuationOptions::use_predictor = true
//          (the tangential predictor, Task 9).
//
// ---------------------------------------------------------------------
// THE TWO FAMILIES (tests/sqp/support/{parametric_families,scale_problems}.h):
//
//   F3   F3SpringChain(n, /*p_act=*/0.5, /*p0=*/0.25) swept over p: 0.25 ->
//        0.75 -- the SAME construction and range
//        tests/test_warm_start_battery.cpp's "F3n1000"/"F3n50" cells use, so
//        a bench run at --n 1000 is directly comparable to that corpus.
//   F7   F7CollocationChain(n, 3, 2, 0.68, 1.0) (n = node count) swept over
//        p: 0.3 -> 0.9 -- inside the family's design range (0, R = 1) and
//        crossing its activation threshold p_activation() = 0.5 (see that
//        header's banner), the F3-analogue crossing for the collocation
//        family.
//
// ---------------------------------------------------------------------
// WALL-TIME AND PEAK RSS -- BOTH INFORMATIONAL, NEVER A REGRESSION CONTRACT.
//
//   wall_seconds  ledger.h's SqpSolveRecord::wall_seconds (Phase-5 Task 2):
//                 std::chrono::steady_clock timed by SqpDriver::solve()
//                 around solve_impl ALONE, per solve. For `cold`/`hot` this
//                 is naturally one measurement per CSV row; for `warm`/`pred`
//                 it is exactly as precise, because run_continuation makes
//                 one driver.solve() call per step and each gets its own
//                 timed record -- the harness does not need to (and does
//                 not) time the whole continuation call to get a per-row
//                 number.
//   peak_rss_mib  tests/sqp/support/scale_problems.h's peak_rss_mib() (moved
//                 there from test_scale_problems.cpp by this same task so
//                 the /proc/self/status parser is not duplicated): the
//                 WHOLE-PROCESS high-water mark (-1 off Linux or if
//                 unreadable), sampled once per `cold`/`hot` solve (so
//                 successive rows there see a monotonically non-decreasing
//                 sequence) and ONCE PER `warm`/`pred` run_continuation CALL
//                 (so every row from one continuation call shares the same
//                 sample -- an honest reading of a high-water mark, not a
//                 per-step measurement, since nothing here instruments
//                 continuation.h itself to sample mid-sweep).
//
// SINGLE-RUN VALUES ARE LEADING-DIGITS-ONLY (this project's timing-honesty
// rule, restated in ledger.h's own note on wall_seconds): a caller wanting a
// stable number reruns the whole invocation and compares medians, which is a
// Task 3/4/5/9 concern, not this harness's.
//
// ---------------------------------------------------------------------
// THE CSV SCHEMA IS PHASE API -- these column names are chosen once, here,
// for Tasks 3, 4, 5 and 9 to read:
//
//   family,n,arm,step,p,label,status,start_level,major_iters,qp_minor_iters,
//   factorizations,factorizations_saved,full_step_majors,watchdog_restores,
//   soc_steps,soc_applied,border_refine_steps,eqp_refine_steps,wall_seconds,
//   peak_rss_mib
//
// One row per solve, EVERY COLUMN FROM major_iters ON READ OFF THE LEDGER'S
// OWN SqpSolveRecord (ledger.h) -- never re-derived from SqpSolution or
// ContinuationResult directly, per this file's own banner note above. `p` is
// the one column that is not ledger-derivable (SqpSolveRecord carries no
// parameter value) and comes from the grid/ContinuationStep the harness
// itself tracks in lock-step with the ledger -- this is O-7's GRID
// DISCIPLINE: a downstream ratio-taker can group rows by `p` and assert grid
// relationships (e.g. "warm and cold visited the same points") directly off
// this column. `step` is the 0-based index of this solve within THIS
// invocation's run (cold/hot: the grid index; warm/pred: the continuation
// step index), deterministic given the flags.
//
// EVERYTHING BUT wall_seconds AND peak_rss_mib IS DETERMINISTIC GIVEN THE
// FLAGS (same family/n/arm/sweep always visits the same p grid and, on a
// fixed backend, produces the same counters) -- see --self-check below for
// the mode that pins this against a published reference.
//
// ---------------------------------------------------------------------
// --self-check -- THE CORRECTNESS GATE. Runs ONE FIXED, HARD-CODED scenario
// -- F3SpringChain(1000, 0.5, 0.25) swept p: 0.25 -> 0.75, warm arm, full_step
// ON, ContinuationOptions{} DEFAULTS (dp_init = 0.1, NOT derived from any
// --sweep flag) -- and compares its ledger-summed counters against the
// pinned reference in tests/test_warm_start_battery.cpp's
// kPins["F3n1000"].arm[kArmWarm] row ({6, 30, 84, 25, 0, 22, 1, 5, 0, 0, 84,
// 0}; cell[0], i.e. warm_full_step ON): steps, majors, minors,
// factorizations, factorizations_saved, full_step_majors, the resolved-level
// histogram, predictor_calls, border_refine_steps and elastic_activations.
// A mismatch means this harness's plumbing -- driver construction, ledger
// attachment, run_continuation wiring -- is NOT measuring what the battery
// measured, which is the one thing Tasks 3-5/9 need to be able to trust
// before reading anything else this binary produces. Prints every column
// (got vs want) and exits 1 on any mismatch, 0 on a full match; does not
// touch --family/--n/--arm/--sweep/--csv, and does not write a CSV file.
//
// ---------------------------------------------------------------------
// PHASE-5 TASK 3 ADDITION -- THE DIAGNOSTIC FLAGS AND THE QP-LEVEL CSV.
//
// Task 3's first job is diagnosing the F7 N=200 wall-time cliff recorded in
// tests/test_scale_problems.cpp's CARRY 2. The columns above could not
// discriminate its candidate causes (minor-iteration blow-up vs factorization
// cost vs cycling) for two reasons, both of them OBSERVATION gaps on this
// side of the library rather than anything missing from the engine:
//
//   (1) every solve ran at ONE fixed configuration (bench_options()'s
//       SqpOptions with a default-constructed QpOptions), so the counters
//       could not be moved against the knobs -- max_iter, schur_cap,
//       ws_algebra -- that the competing hypotheses actually differ on. The
//       five flags --max-iter/--qp-max-iter/--schur-cap/--ws-algebra/--p
//       expose exactly those knobs and nothing else; every one of them
//       DEFAULTS to the value bench_options()/QpOptions already used, so an
//       invocation that passes none of them is byte-identical to the
//       pre-Task-3 harness (--self-check does not read them at all).
//       PHASE-5 TASK 9 added a SIXTH on the same terms, --p0, the sweep's
//       START point -- see kUsage's own note for why the head-to-head sweep
//       needs it (F7's own p0 = 0.3 sits below the activation threshold
//       R/2 = 0.5, so the family's default range spends its first half with
//       an empty active window).
//       PHASE-6 TASK 1 added TWO MORE, again on the same terms and for the
//       same reason one level up: --probe-budget and --no-growth-suspension
//       are the continuation CONTROLLER's own knobs (ContinuationOptions),
//       and they exist because the retry-economics defaults had to be swept
//       on the very sweep that motivated them rather than argued for --
//       docs/notes/2026-08-02-controller-retry-economics.md is that sweep,
//       and this binary is its vehicle. Both default to the library value,
//       so the headline before/after invocation passes NEITHER.
//
//   (2) the CSV aggregated every QP subproblem of a solve into one row, so
//       "3 majors, 1002 minors" could not be resolved into the per-major
//       sequence that tells a capped first QP apart from a stalling
//       sequence of them. --qp-csv writes the QP-LEVEL SolveRecord entries
//       (ledger.h) the engine has always emitted -- one row per QP
//       subproblem solve, in engine order -- alongside the ordinary
//       per-solve CSV. Nothing new is measured: this is the SAME Ledger,
//       printing its other vector.
//
//       PHASE-6 TASK 3 appended ELEVEN COLUMNS to that QP-level CSV (five in
//       the original round -- ws_adds, ws_drops, shift_adds, degenerate_steps,
//       degenerate_run_max -- and six more in its fix round: ws_adds_bound,
//       ws_drops_bound, distinct_ineq_added, distinct_bound_added, drop_ties
//       and ratio_ties) -- for the same kind of
//       reason, one level further in, stated in full by QpCounters (types.h):
//       every pre-existing QP-level column counts a LINEAR-ALGEBRA event,
//       while the wide-window minor-stall this project carried out of Phase 5
//       is a claim about the COMBINATORIAL walk, which nothing observed. They
//       are OBSERVATION ONLY (written, never read by any engine decision), so
//       every other column on every row is byte-identical to the pre-Task-3
//       harness. NOTE that unlike the per-solve CSV, the QP-level CSV's column
//       set is NOT Phase API -- it is a diagnostic vector.
//
// ---------------------------------------------------------------------
// PHASE-5 TASK 9 ADDITION -- --dump-solution, THE CROSS-SOLVER REFEREE HOOK.
//
// Task 9 puts this SQP engine head to head with the IPM engine (now
// hven::solvers::InteriorPointSolver) on F7, and F7's MANUFACTURED SOLUTION is the referee:
// both solvers must land on x*(p) = ( y*(t_k) ; u*(t_k) )_k with
// y*(t) = min(p(1 + sin pi t), R) e, or the comparison is between two
// different problems. Checking that needs the CONVERGED POINT, which the CSV
// (one row of COUNTERS per solve) has never carried and deliberately still
// does not -- the CSV schema above is untouched by this flag.
//
//   --dump-solution <path>   after the run, write the LAST solve's converged
//                            point to <path> in bench_cli.h's dump format (a
//                            `# key: value` header plus one x component per
//                            line at {:.17g}).
//
// THE LAST SOLVE, not every solve: at F7 n = 10^5 variables one dump is 2 MB,
// and the cross-validation only ever compares ONE point per invocation. To
// dump the solution AT a chosen p, run `--sweep 1 --p <value>` on the cold or
// hot arm, which solves at exactly that one point (see --sweep above). On the
// warm/pred arms the last solve is the last CONTINUATION STEP, whose p is the
// sweep's endpoint when the sweep reached it and the last attempted value
// otherwise -- the dumped header's `p` line always states which, so a reader
// never has to infer it.
//
// `f` in the header is SqpSolution::f for cold/hot and model.eval_f(x)
// recomputed at the step's own p for warm/pred (ContinuationStep carries x but
// no objective value). Both are the objective at the dumped x; the
// distinction is recorded because the two travel through different code.
//
// A DUMP IS WRITTEN WHATEVER THE STATUS, with the status in the header. A
// non-converged point is exactly what a reader investigating a disagreement
// needs, and silently withholding it would be the fallback-that-hides-a-
// failure this project forbids.
//
// T6: an unknown flag, an unrecognised --family/--arm/--ws-algebra value, a
// malformed numeric value, or a --dump-solution path that cannot be opened
// throws std::invalid_argument with the usage text folded into the message
// (never printed as a side effect the exception doesn't also carry); main()
// catches it, prints it to stderr and exits 1.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/core/ledger.h>
#include <hven/detail/warmstart/continuation.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

#include "bench_cli.h"

#include "support/parametric_families.h"
#include "support/scale_problems.h"

namespace {

using hven::Vec;
using hven::solvers::ContinuationOptions;
using hven::solvers::ContinuationResult;
using hven::solvers::ContinuationStep;
using hven::solvers::Index;
using hven::solvers::Ledger;
using hven::solvers::ParametricNlpModel;
using hven::solvers::QpMode;
using hven::solvers::QpOptions;
using hven::solvers::QpStatus;
using hven::solvers::run_continuation;
using hven::solvers::SolveRecord;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::SqpSolveRecord;
using hven::solvers::SqpStatus;
using hven::solvers::StartLevel;
using hven::solvers::WarmStart;
using hven::solvers::WorkingSetLinearAlgebra;
using hven::solvers::test_support::F3SpringChain;
using hven::solvers::test_support::F7CollocationChain;
using hven::solvers::test_support::peak_rss_mib;

constexpr const char *kUsage =
    "usage: hven_sqp_bench --family F3|F7 --n <int> --arm cold|warm|pred|hot "
    "--sweep <int> --csv <path>\n"
    "       hven_sqp_bench --self-check\n"
    "       hven_sqp_bench --help\n"
    "\n"
    "  --family F3|F7   the parametric family (tests/sqp/support/{parametric_families,\n"
    "                   scale_problems}.h); F3: p sweeps 0.25 -> 0.75, --n is the\n"
    "                   chain length; F7: p sweeps 0.3 -> 0.9, --n is the node count.\n"
    "  --n <int>        family size (F3: chain length n >= 2; F7: node count >= 3).\n"
    "  --arm ARM        cold | warm | pred | hot -- see this file's own banner for\n"
    "                   exactly what each one does.\n"
    "  --sweep <int>    >= 1. cold/hot: the literal number of evenly-spaced grid\n"
    "                   points. warm/pred: seeds run_continuation's initial step\n"
    "                   size; the ACTUAL step count is adaptive (see the banner).\n"
    "  --csv <path>     output file; one row per solve, header row included.\n"
    "\n"
    "DIAGNOSTIC FLAGS (Phase-5 Task 3; every one defaults to the value the\n"
    "pre-Task-3 harness already used, so omitting all of them is byte-identical\n"
    "to it -- see the file banner's THE DIAGNOSTIC FLAGS note):\n"
    "  --p <double>     override the sweep's END point p1 (the family's p0 is\n"
    "                   unchanged unless --p0 is also given). With --sweep 1 this\n"
    "                   selects the single parameter value the cold/hot arm solves\n"
    "                   at.\n"
    "  --p0 <double>    override the sweep's START point p0 (Phase-5 Task 9;\n"
    "                   defaults to the family's own p0, so omitting it is\n"
    "                   byte-identical to the pre-Task-9 harness). F7's own p0 of\n"
    "                   0.3 is below its activation threshold R/2 = 0.5, i.e. the\n"
    "                   first part of that sweep has an EMPTY active window; a\n"
    "                   study that wants activity on every step (Task 9's headline\n"
    "                   p-sweep) needs p0 > R/2 and this is the only flag that can\n"
    "                   ask for it. Both families' design ranges still apply.\n"
    "  --max-iter <int> SqpOptions::max_iter, the MAJOR-iteration cap (default 100).\n"
    "  --qp-max-iter <int>  QpOptions::max_iter, the MINOR-iteration cap per QP\n"
    "                   subproblem. Default: the library default, which since\n"
    "                   Phase-6 Task 4 (M6) is SIZE-DERIVED rather than a fixed\n"
    "                   500 -- any value passed here overrides it outright.\n"
    "  --schur-cap <int>    QpOptions::schur_cap (default 128, the library default).\n"
    "  --qp-mode walk|ssn   SqpOptions::qp_mode (default walk, the library\n"
    "                   default and every published figure's configuration).\n"
    "                   ssn routes each major's MAIN subproblem through the\n"
    "                   semismooth-Newton kernel (Phase-7). Omitting it is\n"
    "                   byte-identical to the pre-Phase-7 harness.\n"
    "  --ws-algebra border|refactorize  QpOptions::ws_algebra (default border).\n"
    "  --probe-budget <int>  ContinuationOptions::probe_budget (default 2, the\n"
    "                   library default), the warm/pred arms' FAILED-PROPOSAL\n"
    "                   detector; 0 disables it. Ignored by the cold and hot arms,\n"
    "                   which do not run a continuation at all.\n"
    "  --no-growth-suspension  ContinuationOptions::suspend_growth_after_failure =\n"
    "                   false (default true, the library default): let dp grow\n"
    "                   again on the first accepted step after a failed proposal.\n"
    "                   Ignored by the cold and hot arms.\n"
    "                   NOTE: the pre-Phase-6 controller is BOTH of these together,\n"
    "                   `--probe-budget 0 --no-growth-suspension`. --probe-budget 0\n"
    "                   alone leaves the growth suspension on, which is a\n"
    "                   step-length rule and moves which parameter values a failing\n"
    "                   sweep visits (measured: 14 attempts/6732 minors BASE vs\n"
    "                   15/5409 with the suspension alone, F7 N=20000, p 0.51->0.95).\n"
    "  --qp-csv <path>  ALSO write the QP-LEVEL ledger records (one row per QP\n"
    "                   subproblem solve, engine order) to this file.\n"
    "  --self-check     ignore every other flag; run the fixed F3/n=1000/warm\n"
    "                   scenario and check it against the Phase-4 battery's pinned\n"
    "                   counters. Exit 0 on a match, 1 on a mismatch.\n"
    "  --help           print this text and exit 0.\n"
    "\n"
    "CROSS-SOLVER FLAG (Phase-5 Task 9 -- the IPM bridge; see the file banner's\n"
    "own --dump-solution note):\n"
    "  --dump-solution <path>  ALSO write the LAST solve's converged point (one x\n"
    "                   component per line at 17 significant digits, under a\n"
    "                   '# key: value' header carrying family/arm/n_flag/nx/p/\n"
    "                   status/f) to this file. Use --sweep 1 --p <value> on the\n"
    "                   cold arm to dump the solution at one chosen parameter.\n"
    "                   NOTE THE UNITS: n_flag is --n as given (for F7 the NODE\n"
    "                   count N), nx is the VARIABLE count (F7: nx = 5N here).\n"
    "\n"
    "CSV COLUMNS (one row per solve, in this order -- this is Phase-5 API, see the\n"
    "file banner's THE CSV SCHEMA IS PHASE API note):\n"
    "  family,n,arm,step,p,label,status,start_level,major_iters,qp_minor_iters,\n"
    "  factorizations,factorizations_saved,full_step_majors,watchdog_restores,\n"
    "  soc_steps,soc_applied,border_refine_steps,eqp_refine_steps,wall_seconds,\n"
    "  peak_rss_mib\n"
    "Every column from major_iters on is read off the attached Ledger's own\n"
    "SqpSolveRecord (ledger.h); wall_seconds and peak_rss_mib are informational\n"
    "only (machine-dependent, never asserted -- see ledger.h's own note).\n";

// Thin binders over bench_cli.h's shared helpers, so every call site below
// reads exactly as it did before the Task-3 fix-round dedupe -- this file's own
// kUsage is the only thing they add.
[[noreturn]] void throw_usage(const std::string &detail) {
    hven::solvers::bench_cli::throw_usage(kUsage, detail);
}

// ---------------------------------------------------------------------
// One CSV row: the fields that are NOT read off a SqpSolveRecord (family, n,
// arm, step, p, peak_rss) plus the record itself, which write_row() below
// reads every ledger-derived column from.
struct Row {
    std::string family;
    Index n = 0;
    std::string arm;
    std::size_t step = 0;
    double p = 0.0;
    SqpSolveRecord rec;
    double peak_rss = -1.0;
};

// PHASE-5 TASK 9. The LAST solve's converged point, carried out of whichever
// arm ran so main() can write it if --dump-solution asked for it (see the file
// banner). `valid` stays false only when an arm produced no solve at all,
// which today cannot happen -- every arm solves at least once -- but is read
// rather than assumed, so a future arm that CAN return empty fails loudly at
// the flag instead of dumping a default-constructed vector.
struct SolutionDump {
    bool valid = false;
    double p = 0.0;
    SqpStatus status = SqpStatus::kOptimal;
    double f = 0.0;
    Vec x;
};

void write_header(std::ostream &os) {
    os << "family,n,arm,step,p,label,status,start_level,major_iters,qp_minor_iters,"
          "factorizations,factorizations_saved,full_step_majors,watchdog_restores,"
          "soc_steps,soc_applied,border_refine_steps,eqp_refine_steps,wall_seconds,"
          "peak_rss_mib\n";
}

// types.h declares no to_string(QpStatus) (only the SQP-level statuses have
// one), so the QP-level CSV names them here rather than adding a formatter to
// a library header for a bench-only column.
const char *qp_status_name(QpStatus s) {
    switch (s) {
    case QpStatus::kOptimal:
        return "Optimal";
    case QpStatus::kMaxIter:
        return "MaxIter";
    case QpStatus::kInfeasible:
        return "Infeasible";
    case QpStatus::kNumericalError:
        return "NumericalError";
    }
    return "Unknown";
}

// The QP-LEVEL csv (--qp-csv): one row per QP subproblem solve, straight off
// ledger.records() (ledger.h's SolveRecord, which the engine has emitted
// since Phase 2). `seq` is the 0-based index within THIS invocation's ledger,
// i.e. engine order; `label` is the engine's own per-solve label, which
// already carries the driver's prefix and the engine's solve counter.
void write_qp_header(std::ostream &os) {
    os << "family,n,arm,seq,label,warm,status,minor_iters,factorizations,schur_updates,"
          "symbolic_analyses,border_refine_steps,eqp_refine_steps,suspect_escalations,"
          "k0_reused,ws_adds,ws_drops,shift_adds,degenerate_steps,degenerate_run_max,"
          "ws_adds_bound,ws_drops_bound,distinct_ineq_added,distinct_bound_added,"
          "drop_ties,ratio_ties\n";
}

void write_qp_row(std::ostream &os, const std::string &family, Index n, const std::string &arm,
                  std::size_t seq, const SolveRecord &r) {
    os << fmt::format(
        "{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{},{}\n", family,
        n, arm, seq, r.label, r.warm ? 1 : 0, qp_status_name(r.status), r.counters.minor_iters,
        r.counters.factorizations, r.counters.schur_updates, r.counters.symbolic_analyses,
        r.counters.border_refine_steps, r.counters.eqp_refine_steps, r.counters.suspect_escalations,
        r.counters.k0_reused ? 1 : 0, r.counters.ws_adds, r.counters.ws_drops,
        r.counters.shift_adds, r.counters.degenerate_steps, r.counters.degenerate_run_max,
        r.counters.ws_adds_bound, r.counters.ws_drops_bound, r.counters.distinct_ineq_added,
        r.counters.distinct_bound_added, r.counters.drop_ties, r.counters.ratio_ties);
}

void write_row(std::ostream &os, const Row &r) {
    os << fmt::format("{},{},{},{},{:.17g},{},{},{},{},{},{},{},{},{},{},{},{},{},{:.9f},{:.3f}\n",
                      r.family, r.n, r.arm, r.step, r.p, r.rec.label, to_string(r.rec.status),
                      to_string(r.rec.start_level_used), r.rec.counters.major_iters,
                      r.rec.counters.qp_minor_iters, r.rec.counters.factorizations,
                      r.rec.factorizations_saved, r.rec.full_step_majors, r.rec.watchdog_restores,
                      r.rec.soc_steps, r.rec.soc_applied, r.rec.border_refine_steps,
                      r.rec.eqp_refine_steps, r.rec.wall_seconds, r.peak_rss);
}

// `steps` evenly spaced points from p0 to p1 INCLUSIVE (steps >= 2); steps <=
// 1 returns just {p1}, matching this file's own --sweep documentation.
std::vector<double> linspace(double p0, double p1, std::size_t steps) {
    std::vector<double> g;
    if (steps <= 1) {
        g.push_back(p1);
        return g;
    }
    g.reserve(steps);
    for (std::size_t i = 0; i < steps; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(steps - 1);
        g.push_back(p0 + t * (p1 - p0));
    }
    return g;
}

// The one SqpOptions every arm shares apart from start_level -- mirrors
// tests/test_warm_start_battery.cpp's battery_options() exactly (same
// tolerances, same adaptive_mu = false so a kHot resolution stays reachable
// -- see that function's own note on why adaptive_mu is off everywhere).
//
// PHASE-5 TASK 3: the two values a diagnostic flag can move -- SqpOptions::
// max_iter and the whole nested QpOptions -- now arrive in a Tuning, whose
// members default to EXACTLY what this function used to hardcode (100, and a
// default-constructed QpOptions, which is what SqpOptions::qp already was).
// An invocation that passes no diagnostic flag is therefore byte-identical to
// the pre-Task-3 harness.
struct Tuning {
    Index max_iter = 100; // bench_options()'s original literal
    QpOptions qp{};       // library defaults: max_iter 500, schur_cap 128, border
    // PHASE-6 TASK 1: the continuation controller's two retry-economics
    // levers, read ONLY by the warm/pred arms. Both start at the library
    // default (ContinuationOptions' own), so an invocation that passes
    // neither --probe-budget nor --no-growth-suspension configures the
    // controller exactly as a caller who never heard of these flags does.
    ContinuationOptions controller{};
    // PHASE-7 TASK 6: which QP KERNEL the driver's subproblems go through
    // (sqp_types.h's QpMode). Defaults to the library default kWalk, so an
    // invocation that does not pass --qp-mode is byte-identical to every
    // measurement this harness has ever produced -- including --self-check,
    // which does not read this struct's flags at all. The flag exists because
    // the IPM envelope row this phase owes is defined as "the SAME vehicle
    // as Phase-6 section 5.1, under kSsn", and a patched-header recompile
    // would have made that row unquotable from a committed binary.
    QpMode qp_mode = QpMode::kWalk;
};

SqpOptions bench_options(StartLevel level, const Tuning &tuning) {
    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = tuning.max_iter;
    opts.adaptive_mu = false;
    opts.start_level = level;
    opts.warm_full_step = true;
    opts.qp = tuning.qp;
    opts.qp_mode = tuning.qp_mode;
    return opts;
}

// ---- ARM cold: a fresh driver per grid point, from model.start_point(). ---
void run_cold(ParametricNlpModel &model, const std::string &family, Index n, double p0, double p1,
              std::size_t sweep, const Tuning &tuning, Ledger &ledger, std::vector<Row> &rows,
              SolutionDump &dump) {
    const std::vector<double> grid = linspace(p0, p1, sweep);
    for (std::size_t i = 0; i < grid.size(); ++i) {
        SqpDriver driver(bench_options(StartLevel::kCold, tuning));
        driver.attach_ledger(&ledger, fmt::format("cold{}", i));
        model.set_parameters(Vec::Constant(1, grid[i]));
        const SqpSolution sol = driver.solve(model, model.start_point());
        rows.push_back(
            Row{family, n, "cold", i, grid[i], ledger.sqp_records().back(), peak_rss_mib()});
        dump = SolutionDump{true, grid[i], sol.status, sol.f, sol.x}; // last solve wins
    }
}

// ---- ARM hot: one driver, one engine, feeding warm+hot forward. -----------
void run_hot(ParametricNlpModel &model, const std::string &family, Index n, double p0, double p1,
             std::size_t sweep, const Tuning &tuning, Ledger &ledger, std::vector<Row> &rows,
             SolutionDump &dump) {
    const std::vector<double> grid = linspace(p0, p1, sweep);
    SqpDriver driver(bench_options(StartLevel::kHot, tuning));
    driver.attach_ledger(&ledger, "hot");
    WarmStart warm; // default-constructed: valid == false, i.e. cold
    for (std::size_t i = 0; i < grid.size(); ++i) {
        model.set_parameters(Vec::Constant(1, grid[i]));
        const SqpSolution sol =
            (i == 0) ? driver.solve(model, model.start_point()) : driver.solve(model, warm.x, warm);
        warm = sol.warm_start;
        rows.push_back(
            Row{family, n, "hot", i, grid[i], ledger.sqp_records().back(), peak_rss_mib()});
        dump = SolutionDump{true, grid[i], sol.status, sol.f, sol.x}; // last solve wins
    }
}

// ---- ARMs warm/pred: run_continuation, adaptive step count. --------------
void run_continuation_arm(ParametricNlpModel &model, const std::string &family, Index n, double p0,
                          double p1, std::size_t sweep, bool use_predictor, const std::string &arm,
                          const Tuning &tuning, Ledger &ledger, std::vector<Row> &rows,
                          SolutionDump &dump) {
    SqpDriver driver(bench_options(StartLevel::kWarm, tuning));
    driver.attach_ledger(&ledger, arm);

    ContinuationOptions copts = tuning.controller;
    copts.use_predictor = use_predictor;
    // Seed dp_init at the SAME spacing the equal-sized cold/hot grid would
    // use (this file's banner note); dp_max is raised to match so a single
    // large sweep step is not rejected by ContinuationOptions' own dp_init
    // <= dp_max precondition (continuation.h's validate()).
    const double spacing =
        (sweep >= 2) ? std::abs(p1 - p0) / static_cast<double>(sweep - 1) : std::abs(p1 - p0);
    copts.dp_init = std::max(spacing, copts.dp_min);
    copts.dp_max = std::max(copts.dp_max, copts.dp_init);

    // The ledger now OUTLIVES this function (Phase-5 Task 3 hoisted it into
    // main() so --qp-csv can print the QP-level vector after the arm returns),
    // so the records this call appends do NOT necessarily start at index 0.
    // Today exactly one arm runs per invocation and `base` is always 0, but
    // reading it rather than assuming it is what keeps that an OBSERVATION
    // instead of a precondition nothing states.
    const std::size_t base = ledger.sqp_records().size();
    const ContinuationResult res =
        run_continuation(model, Vec::Constant(1, p0), Vec::Constant(1, p1), driver, copts);
    // ONE high-water-mark sample for every row this call produced -- see the
    // banner's WALL-TIME AND PEAK RSS note for why this differs from
    // cold/hot's per-solve sampling.
    const double rss = peak_rss_mib();
    for (std::size_t i = 0; i < res.steps.size(); ++i) {
        rows.push_back(
            Row{family, n, arm, i, res.steps[i].p(0), ledger.sqp_records()[base + i], rss});
    }
    // PHASE-5 TASK 9. The last CONTINUATION STEP's point. ContinuationStep
    // carries x but no objective value, so f is recomputed with the model
    // re-posed at that step's own p -- which is where run_continuation left it
    // only when the last step was also the last thing it attempted, so the
    // set_parameters call below is not redundant.
    if (!res.steps.empty()) {
        const ContinuationStep &last = res.steps.back();
        model.set_parameters(last.p);
        dump = SolutionDump{true, last.p(0), last.status, model.eval_f(last.x), last.x};
    }
}

void run_arm(ParametricNlpModel &model, const std::string &family, Index n, double p0, double p1,
             const std::string &arm, std::size_t sweep, const Tuning &tuning, Ledger &ledger,
             std::vector<Row> &rows, SolutionDump &dump) {
    if (arm == "cold") {
        run_cold(model, family, n, p0, p1, sweep, tuning, ledger, rows, dump);
    } else if (arm == "hot") {
        run_hot(model, family, n, p0, p1, sweep, tuning, ledger, rows, dump);
    } else if (arm == "warm") {
        run_continuation_arm(model, family, n, p0, p1, sweep, /*use_predictor=*/false, "warm",
                             tuning, ledger, rows, dump);
    } else { // "pred", already validated by parse_args
        run_continuation_arm(model, family, n, p0, p1, sweep, /*use_predictor=*/true, "pred",
                             tuning, ledger, rows, dump);
    }
}

// ---------------------------------------------------------------------
// --self-check. See the file banner's own note for the scenario and the
// exact pinned reference this checks against.
struct ExpectedCounts {
    std::size_t steps;
    Index majors, minors, factorizations, fact_saved, full_step_majors;
    Index n_cold, n_seeded, n_warm, n_hot, predictor_calls, border_refine, elastic_activations;
};

// tests/test_warm_start_battery.cpp, kPins[] "F3n1000" row, arm[kArmWarm]
// (index 1), cell[0] (warm_full_step ON): {6, 30, 84, 25, 0, 22, 1, 5, 0, 0,
// 84, 0}.
constexpr ExpectedCounts kF3n1000WarmPin{/*steps=*/6,
                                         /*majors=*/30,
                                         /*minors=*/84,
                                         /*factorizations=*/25,
                                         /*fact_saved=*/0,
                                         /*full_step_majors=*/22,
                                         /*n_cold=*/1,
                                         // PHASE-6 TASK 5: the kSeeded level.
                                         // Zero here and expected to stay zero
                                         // -- every solve in this sweep after
                                         // the first is fed a hand-off from a
                                         // solve of the SAME model object, so
                                         // its structure_hash matches and it
                                         // resolves kWarm. A nonzero reading
                                         // would mean the hash gate stopped
                                         // matching on a same-model chain,
                                         // which is a regression, not a new
                                         // level doing its job.
                                         /*n_seeded=*/0,
                                         /*n_warm=*/5,
                                         /*n_hot=*/0,
                                         /*predictor_calls=*/0,
                                         /*border_refine=*/84,
                                         /*elastic_activations=*/0};

bool run_self_check() {
    F3SpringChain model(1000, 0.5, 0.25);
    const Vec p0 = Vec::Constant(1, 0.25);
    const Vec p1 = Vec::Constant(1, 0.75);

    SqpOptions opts;
    opts.kkt_tol = 1e-8;
    opts.feas_tol = 1e-8;
    opts.max_iter = 60;
    opts.adaptive_mu = false;
    opts.start_level = StartLevel::kWarm;
    opts.warm_full_step = true;

    SqpDriver driver(opts);
    Ledger ledger;
    driver.attach_ledger(&ledger, "selfcheck");

    ContinuationOptions copts; // defaults -- NOT --sweep-derived, see banner
    copts.use_predictor = false;

    const ContinuationResult res = run_continuation(model, p0, p1, driver, copts);

    Index majors = 0, minors = 0, factorizations = 0, fact_saved = 0, full_step_majors = 0;
    Index n_cold = 0, n_seeded = 0, n_warm = 0, n_hot = 0, border_refine = 0,
          elastic_activations = 0;
    for (const SqpSolveRecord &r : ledger.sqp_records()) {
        majors += r.counters.major_iters;
        minors += r.counters.qp_minor_iters;
        factorizations += r.counters.factorizations;
        fact_saved += r.factorizations_saved;
        full_step_majors += r.full_step_majors;
        border_refine += r.border_refine_steps;
        elastic_activations += r.counters.elastic_activations;
        switch (r.start_level_used) {
        case StartLevel::kCold:
            ++n_cold;
            break;
        case StartLevel::kSeeded:
            ++n_seeded;
            break;
        case StartLevel::kWarm:
            ++n_warm;
            break;
        case StartLevel::kHot:
            ++n_hot;
            break;
        }
    }

    const ExpectedCounts &want = kF3n1000WarmPin;
    const bool ok = res.reached_p1 && res.steps.size() == want.steps && majors == want.majors &&
                    minors == want.minors && factorizations == want.factorizations &&
                    fact_saved == want.fact_saved && full_step_majors == want.full_step_majors &&
                    n_cold == want.n_cold && n_seeded == want.n_seeded && n_warm == want.n_warm &&
                    n_hot == want.n_hot && res.predictor_calls == want.predictor_calls &&
                    border_refine == want.border_refine &&
                    elastic_activations == want.elastic_activations;

    fmt::print("self-check: F3 n=1000 arm=warm (full_step ON) vs "
               "tests/test_warm_start_battery.cpp kPins[\"F3n1000\"].arm[kArmWarm]\n");
    fmt::print("  reached_p1={} (want true)\n", res.reached_p1);
    fmt::print("  steps={} (want {})  majors={} (want {})  minors={} (want {})\n", res.steps.size(),
               want.steps, majors, want.majors, minors, want.minors);
    fmt::print("  factorizations={} (want {})  fact_saved={} (want {})  full_step_majors={} "
               "(want {})\n",
               factorizations, want.factorizations, fact_saved, want.fact_saved, full_step_majors,
               want.full_step_majors);
    fmt::print("  n_cold={} (want {})  n_seeded={} (want {})  n_warm={} (want {})  "
               "n_hot={} (want {})\n",
               n_cold, want.n_cold, n_seeded, want.n_seeded, n_warm, want.n_warm, n_hot,
               want.n_hot);
    fmt::print("  predictor_calls={} (want {})  border_refine={} (want {})  "
               "elastic_activations={} (want {})\n",
               res.predictor_calls, want.predictor_calls, border_refine, want.border_refine,
               elastic_activations, want.elastic_activations);
    fmt::print("{}\n", ok ? "SELF-CHECK PASSED" : "SELF-CHECK FAILED");
    return ok;
}

// ---------------------------------------------------------------------
// CLI parsing. T6: every rejection throws std::invalid_argument with the
// usage text folded into the message.
struct Args {
    bool help = false;
    bool self_check = false;
    std::optional<std::string> family;
    std::optional<Index> n;
    std::optional<std::string> arm;
    std::optional<std::size_t> sweep;
    std::optional<std::string> csv;
    // Task-3 diagnostic flags -- all optional, all defaulting to the
    // pre-Task-3 values when absent (see the file banner).
    std::optional<double> p;
    std::optional<double> p0; // Task 9; see kUsage's --p0 note
    std::optional<Index> max_iter;
    std::optional<Index> qp_max_iter;
    std::optional<Index> schur_cap;
    std::optional<std::string> ws_algebra;
    std::optional<std::string> qp_mode;
    // Phase-6 Task 1 controller flags; both default to the library defaults,
    // so omitting them is byte-identical to the shipped controller.
    std::optional<Index> probe_budget;
    bool no_growth_suspension = false;
    std::optional<std::string> qp_csv;
    // Task-9 cross-solver flag (see the file banner).
    std::optional<std::string> dump_solution;
};

double parse_double(const std::string &flag, const std::string &value) {
    return hven::solvers::bench_cli::parse_double(kUsage, flag, value);
}

long long parse_ll(const std::string &flag, const std::string &value) {
    return hven::solvers::bench_cli::parse_ll(kUsage, flag, value);
}

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
        } else if (arg == "--self-check") {
            a.self_check = true;
        } else if (arg == "--family") {
            const std::string v = next_value(arg);
            if (v != "F3" && v != "F7") {
                throw_usage(fmt::format("--family: '{}' is not one of F3|F7", v));
            }
            a.family = v;
        } else if (arg == "--n") {
            const std::string v = next_value(arg);
            const long long parsed = parse_ll(arg, v);
            if (parsed < 1) {
                throw_usage(fmt::format("--n: '{}' must be >= 1", v));
            }
            a.n = static_cast<Index>(parsed);
        } else if (arg == "--arm") {
            const std::string v = next_value(arg);
            if (v != "cold" && v != "warm" && v != "pred" && v != "hot") {
                throw_usage(fmt::format("--arm: '{}' is not one of cold|warm|pred|hot", v));
            }
            a.arm = v;
        } else if (arg == "--sweep") {
            const std::string v = next_value(arg);
            const long long parsed = parse_ll(arg, v);
            if (parsed < 1) {
                throw_usage(fmt::format("--sweep: '{}' must be >= 1", v));
            }
            a.sweep = static_cast<std::size_t>(parsed);
        } else if (arg == "--csv") {
            a.csv = next_value(arg);
        } else if (arg == "--p") {
            a.p = parse_double(arg, next_value(arg));
        } else if (arg == "--p0") {
            a.p0 = parse_double(arg, next_value(arg));
        } else if (arg == "--max-iter" || arg == "--qp-max-iter" || arg == "--schur-cap") {
            const std::string v = next_value(arg);
            const long long parsed = parse_ll(arg, v);
            if (parsed < 1) {
                throw_usage(fmt::format("{}: '{}' must be >= 1", arg, v));
            }
            const auto value = static_cast<Index>(parsed);
            if (arg == "--max-iter") {
                a.max_iter = value;
            } else if (arg == "--qp-max-iter") {
                a.qp_max_iter = value;
            } else {
                a.schur_cap = value;
            }
        } else if (arg == "--qp-mode") {
            const std::string v = next_value(arg);
            if (v != "walk" && v != "ssn") {
                throw_usage(fmt::format("--qp-mode: '{}' is not one of walk|ssn", v));
            }
            a.qp_mode = v;
        } else if (arg == "--ws-algebra") {
            const std::string v = next_value(arg);
            if (v != "border" && v != "refactorize") {
                throw_usage(fmt::format("--ws-algebra: '{}' is not one of border|refactorize", v));
            }
            a.ws_algebra = v;
        } else if (arg == "--probe-budget") {
            const std::string v = next_value(arg);
            const long long parsed = parse_ll(arg, v);
            if (parsed < 0) {
                throw_usage(
                    fmt::format("{}: '{}' must be >= 0 (0 disables the probe budget)", arg, v));
            }
            a.probe_budget = static_cast<Index>(parsed);
        } else if (arg == "--no-growth-suspension") {
            a.no_growth_suspension = true;
        } else if (arg == "--qp-csv") {
            a.qp_csv = next_value(arg);
        } else if (arg == "--dump-solution") {
            a.dump_solution = next_value(arg);
        } else {
            throw_usage(fmt::format("unknown flag: '{}'", arg));
        }
    }
    return a;
}

} // namespace

int main(int argc, char **argv) {
    try {
        const Args args = parse_args(argc, argv);
        if (args.help) {
            fmt::print("{}", kUsage);
            return 0;
        }
        if (args.self_check) {
            return run_self_check() ? 0 : 1;
        }
        if (!args.family || !args.n || !args.arm || !args.sweep || !args.csv) {
            throw_usage("--family, --n, --arm, --sweep and --csv are all required "
                        "(or pass --help / --self-check instead)");
        }

        std::vector<Row> rows;
        Ledger ledger;
        const Index n = *args.n;
        const std::size_t sweep = *args.sweep;

        Tuning tuning;
        if (args.max_iter) {
            tuning.max_iter = *args.max_iter;
        }
        if (args.qp_max_iter) {
            tuning.qp.max_iter = *args.qp_max_iter;
        }
        if (args.schur_cap) {
            tuning.qp.schur_cap = *args.schur_cap;
        }
        if (args.probe_budget) {
            tuning.controller.probe_budget = static_cast<int>(*args.probe_budget);
        }
        if (args.no_growth_suspension) {
            tuning.controller.suspend_growth_after_failure = false;
        }
        if (args.qp_mode) {
            tuning.qp_mode = (*args.qp_mode == "ssn") ? QpMode::kSsn : QpMode::kWalk;
        }
        if (args.ws_algebra) {
            tuning.qp.ws_algebra = (*args.ws_algebra == "refactorize")
                                       ? WorkingSetLinearAlgebra::kRefactorize
                                       : WorkingSetLinearAlgebra::kSchurBorder;
        }

        SolutionDump dump;
        if (*args.family == "F3") {
            F3SpringChain model(n, 0.5, 0.25);
            run_arm(model, "F3", n, args.p0.value_or(0.25), args.p.value_or(0.75), *args.arm, sweep,
                    tuning, ledger, rows, dump);
        } else {
            F7CollocationChain model(n, 3, 2, 0.68, 1.0);
            run_arm(model, "F7", n, args.p0.value_or(0.3), args.p.value_or(0.9), *args.arm, sweep,
                    tuning, ledger, rows, dump);
        }

        std::ofstream out(*args.csv);
        if (!out) {
            throw std::invalid_argument(
                fmt::format("--csv: could not open '{}' for writing", *args.csv));
        }
        write_header(out);
        for (const Row &r : rows) {
            write_row(out, r);
        }
        fmt::print("wrote {} row(s) to {}\n", rows.size(), *args.csv);

        if (args.qp_csv) {
            std::ofstream qp_out(*args.qp_csv);
            if (!qp_out) {
                throw std::invalid_argument(
                    fmt::format("--qp-csv: could not open '{}' for writing", *args.qp_csv));
            }
            write_qp_header(qp_out);
            const std::vector<SolveRecord> &qp_recs = ledger.records();
            for (std::size_t i = 0; i < qp_recs.size(); ++i) {
                write_qp_row(qp_out, *args.family, n, *args.arm, i, qp_recs[i]);
            }
            fmt::print("wrote {} QP-level row(s) to {}\n", qp_recs.size(), *args.qp_csv);
        }

        if (args.dump_solution) {
            if (!dump.valid) {
                throw_usage(fmt::format("--dump-solution: the '{}' arm produced no solve to dump",
                                        *args.arm));
            }
            std::ofstream dump_out = hven::solvers::bench_cli::open_output_or_throw(
                kUsage, "--dump-solution", *args.dump_solution);
            hven::solvers::bench_cli::write_solution_dump(
                dump_out, *args.family, static_cast<long long>(n), *args.arm, dump.p,
                to_string(dump.status), dump.f, dump.x.data(),
                static_cast<std::size_t>(dump.x.size()));
            fmt::print("wrote solution dump ({} component(s), p = {:.17g}, status {}) to {}\n",
                       dump.x.size(), dump.p, to_string(dump.status), *args.dump_solution);
        }
        return 0;
    } catch (const std::exception &e) {
        fmt::print(stderr, "hven_sqp_bench: error: {}\n", e.what());
        return 1;
    }
}
