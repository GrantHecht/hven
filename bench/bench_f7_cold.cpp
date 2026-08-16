// bench/bench_f7_cold.cpp — PHASE-5 TASK 3 (fix round 1): the single-solve F7
// cold-start probe. THE VEHICLE FOR docs/notes/2026-07-30-scale-study-cold.md
// SECTIONS 4.1, 4.2 AND 5 — every number in those three sections came out of
// this program, and it is committed so they are reproducible from the
// repository rather than from a scratch directory.
//
// ---------------------------------------------------------------------
// WHY THIS EXISTS ALONGSIDE hven_sqp_bench, WHICH IT DOES NOT REPLACE.
//
// hven_sqp_bench is Task 2's harness: four arms, a parameter sweep, a
// ledger-derived CSV whose column set is fixed Phase API, and a --self-check
// gate pinned to the Phase-4 warm-start battery. It answers "how do cold, warm,
// hot and predicted compare across a sweep". It deliberately fixes two things
// this probe needs to vary, and cannot report a third:
//
//   (a) TOLERANCES. bench_options() hardcodes kkt_tol = feas_tol = 1e-8 so its
//       rows stay comparable with tests/test_warm_start_battery.cpp's corpus.
//       The scale study's Arm B is quoted at 1e-9, which is what
//       tests/test_scale_problems.cpp's own tight_options() publishes its
//       figures at, so a scale number is comparable with the fixture's.
//   (b) adaptive_mu. bench_options() sets it FALSE (so a kHot resolution stays
//       reachable — see its own note); the library DEFAULT is true, and the
//       scale study measures the library default. This is a real difference,
//       not a cosmetic one, which is why re-deriving §4.2 through the bench
//       would not have reproduced these numbers.
//   (c) THE ANALYTIC ERROR. The CSV schema carries no |x - x*| column and
//       cannot: it is written from a Ledger, and a Ledger knows nothing about
//       F7's manufactured optimum. The correctness column in §4.1/§4.2 —
//       |x - x*|inf and relative |f - f*| against x_star(p)/f_star(p) — is the
//       whole reason F7 exists, and it is reported here.
//
// Both programs read the same F7CollocationChain from
// tests/sqp/support/scale_problems.h and the same SqpDriver, so where their
// configurations coincide their counters agree; the note records one such
// cross-check (N = 150, both tolerance settings, identical 1276 minors and
// 853/4 factorizations).
//
// ---------------------------------------------------------------------
// THE OPTION SET, STATED IN FULL because the note cites it per table. A
// default-constructed SqpOptions with exactly four fields moved:
//
//     kkt_tol  = feas_tol = 1e-9     (tight_options()'s values)
//     max_iter = 60                  (majors; every solve in the study used <= 5)
//     qp.max_iter, qp.schur_cap, qp.ws_algebra   from argv, defaults below
//
// EVERYTHING ELSE IS THE LIBRARY DEFAULT — including adaptive_mu = true,
// warm_full_step, tr_init, and the whole funnel/SOC/elastic configuration.
// The solve is always COLD: the 2-arg SqpDriver::solve() from
// model.start_point(), one solve per invocation, no warm start anywhere.
//
// USAGE
//     hven_sqp_f7_cold <nodes> <p> [qp_max_iter] [schur_cap] [ws_algebra] [qp_opt_tol]
//                       [crash_basis]
//
//     nodes        N >= 3. n = N*(ns + nc) = 5N with this file's fixed
//                  ns = 3, nc = 2 -- the mapping the whole study uses.
//     p            the family parameter, 0 < p < R = 1. p <= 0.5 is the
//                  EMPTY-WINDOW regime (no path row active), p > 0.5 the
//                  wide-window one. The study uses 0.45 and 0.85.
//     qp_max_iter  QpOptions::max_iter. Default: the library's own default, which
//                  since Phase-6 Task 4 (M6) is the SIZE-DERIVED cap, not a fixed
//                  500 -- pass 0 to name it explicitly, or any positive value for
//                  an absolute cap that overrides it.
//     schur_cap    QpOptions::schur_cap. Default: the library's 128.
//     ws_algebra   "border" (default) or "refactorize".
//     qp_opt_tol   QpOptions::opt_tol. Default: the library's 1e-9.
//     crash_basis  SqpOptions::crash_basis, 0 or 1. Default 0, the library
//                  default -- so an invocation that omits it is byte-identical
//                  to the pre-Task-4 program.
//
// PHASE-6 TASK 3 ADDED THE SIXTH ARGUMENT, on the same terms as Task 3-P5's
// own flags: it defaults to the library value, so an invocation that omits it
// is byte-identical to the pre-Task-3 program. It exists because opt_tol is
// the threshold the DROP RULE prices against (qp_engine.h's drop_worst: a
// working-set member is released once -lambda_j exceeds it), and the
// wide-window walk's cost turned out to be dominated by drop/re-add churn --
// so whether that churn is driven by genuine geometry or by
// regularization-scale multiplier noise is answerable by moving exactly this
// one number and nothing else. See
// docs/notes/2026-08-03-identification-stall-study.md Sec. 3.
//
// Prints one line of whitespace-separated key=value fields to stdout. Wall
// time and RSS are INFORMATIONAL ONLY, per the standing phase rule.
//
// PHASE-5 TASK 4 ADDED FOUR COLUMNS, and no other behaviour changed. The
// schur_cap policy study (docs/notes/2026-07-31-schur-cap-policy.md) needs the
// two counters that distinguish "border mode is rebuilding" from "border mode
// is bordering", and neither was printed before:
//
//     active=  the family's ANALYTIC active path-row count at this p and N
//              (model.active_set(p)), i.e. the churn profile this row belongs
//              to, re-derived from the family rather than inferred from the
//              solve. It is the study's independent variable and printing it
//              here is what makes a row self-describing.
//     schur=   summed QpCounters::schur_updates over every QP subproblem of
//              this solve. SqpCounters has NO schur_updates field (see
//              sqp_types.h), so this is read from a Ledger attached to the
//              driver -- the same route tests/test_scale_smoke.cpp uses -- by
//              summing the QP-level SolveRecords. Identically 0 under
//              ws_algebra = refactorize, where no border stack exists.
//     symb=    SqpCounters::symbolic_analyses, the Pardiso phase-11 count.
//     bref=    SqpCounters::border_refine_steps.
//
// The Ledger is attached unconditionally; it costs one SolveRecord per QP
// subproblem and does not touch the solve's trajectory.
//
// OUTPUT-FORMAT NOTE FOR ANYONE HOLDING OLD LOGS: this program prints
// `status=Optimal` (SqpStatus's own to_string). The uncommitted scratch
// version that produced the study's raw logs before this file was committed
// printed the ENUMERATOR VALUE instead -- `status=0` for kOptimal, `status=3`
// for kNumericalError. Every other field is byte-identical, and the counters
// reproduce exactly, but a mechanical diff of an old log against a fresh run
// will show that one column differing. Nothing else changed.
//
// PHASE-6 TASK 4 (M6) ADDED THREE COLUMNS and changed one argument's
// validation; no other behaviour changed, and every pre-existing column is
// byte-identical at a matched cap:
//
//     qpmax=   the LARGEST per-subproblem minor count of this solve, read
//              from the same Ledger the counters above are. This -- not
//              `minors` -- is the quantity a PER-SUBPROBLEM cap has to
//              clear, and it is what the M6 calibration table
//              (docs/notes/2026-08-03-crash-basis.md Sec. 4) is built on.
//     capbase= n + mi + #bounded for this model, i.e. the size the derived
//              cap is a multiple of (8N on this family: 5N variables, N path
//              rows, 2N box-constrained controls).
//     cap=     the cap this invocation actually ran at, resolved through
//              qp_engine.h's own detail::effective_qp_max_iter, so a row
//              records the policy it was measured under rather than leaving
//              it to be inferred from the argv line.
//
// And `qp_max_iter` now accepts 0, which is QpOptions::max_iter's own
// sentinel for the size-derived default (it used to be rejected as < 1).
//
// PHASE-6 TASK 4 ALSO ADDED A SEVENTH ARGUMENT AND THREE MORE COLUMNS, on the
// same terms as Task 3-P5's own sixth: `crash_basis` defaults to the library
// value (off), so an invocation that omits it is byte-identical to the
// pre-Task-4 program, and `crash=` / `crashrows=` / `crashbnd=` report the
// lever's setting and what it actually seeded, so an on/off row pair is
// self-describing.
//
// T6: every argument rejection throws std::invalid_argument with the usage
// text folded into the message; main() catches it, prints to stderr, exits 1.
// Nothing here prints a diagnostic it does not also fold into the throw.

#include <algorithm> // std::max, in the relative-error denominator
#include <chrono>
#include <cmath> // std::abs(double), same
#include <stdexcept>
#include <string>

#include <Eigen/Core> // Eigen::Infinity, for the lpNorm below
#include <fmt/format.h>

#include <hven/core/ledger.h>
#include <hven/drivers/sqp_driver.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

#include "bench_cli.h"

#include "support/scale_problems.h"

namespace {

using hven::Index;
using hven::solvers::SqpDriver;
using hven::solvers::SqpOptions;
using hven::solvers::SqpSolution;
using hven::solvers::WorkingSetLinearAlgebra;
using hven::solvers::test_support::F7CollocationChain;
using hven::solvers::test_support::peak_rss_mib;

constexpr const char *kUsage =
    "usage: hven_sqp_f7_cold <nodes> <p> [qp_max_iter] [schur_cap] [ws_algebra] "
    "[qp_opt_tol] [crash_basis]\n"
    "\n"
    "  nodes        N >= 3; n = 5N (ns = 3 states, nc = 2 controls per node).\n"
    "  p            0 < p < 1; p <= 0.5 empty window, p > 0.5 wide window.\n"
    "  qp_max_iter  QpOptions::max_iter   (default: the library default, which is\n"
    "               SIZE-DERIVED since M6; 0 names it, >0 is an absolute cap).\n"
    "  schur_cap    QpOptions::schur_cap  (default 128, the library default).\n"
    "  ws_algebra   border | refactorize  (default border).\n"
    "  qp_opt_tol   QpOptions::opt_tol  (default 1e-9, the library default).\n"
    "  crash_basis  SqpOptions::crash_basis, 0|1 (default 0, the library default).\n"
    "\n"
    "Solves ONE cold F7 solve at those settings and prints its counters, its\n"
    "error against the family's manufactured optimum, and wall/RSS (both\n"
    "informational only). See this file's banner for the full option set.\n";

// Thin binders over bench_cli.h's shared helpers (Task-3 fix round 2 factored
// these out of the two byte-identical copies the bench binaries had grown);
// this file's own kUsage is the only thing they add.
[[noreturn]] void throw_usage(const std::string &detail) {
    hven::solvers::bench_cli::throw_usage(kUsage, detail);
}

long long parse_ll(const std::string &what, const std::string &value) {
    return hven::solvers::bench_cli::parse_ll(kUsage, what, value);
}

double parse_double(const std::string &what, const std::string &value) {
    return hven::solvers::bench_cli::parse_double(kUsage, what, value);
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            fmt::print("{}", kUsage);
            return 0;
        }
        if (argc < 3 || argc > 8) {
            throw_usage("expected 2 to 7 arguments");
        }

        const long long nodes = parse_ll("nodes", argv[1]);
        if (nodes < 3) {
            throw_usage(fmt::format("nodes: '{}' must be >= 3", argv[1]));
        }
        const double p = parse_double("p", argv[2]);

        SqpOptions opts; // library defaults everywhere except the four below
        opts.kkt_tol = 1e-9;
        opts.feas_tol = 1e-9;
        opts.max_iter = 60;
        if (argc > 3) {
            // M6: 0 is now MEANINGFUL, not an error -- it is QpOptions::
            // max_iter's own sentinel for "derive the cap from the
            // subproblem's size", and therefore the way to invoke this probe
            // at the shipped default explicitly. Negative is still rejected.
            const long long v = parse_ll("qp_max_iter", argv[3]);
            if (v < 0) {
                throw_usage(fmt::format("qp_max_iter: '{}' must be >= 0 (0 = the library's "
                                        "size-derived default)",
                                        argv[3]));
            }
            opts.qp.max_iter = static_cast<Index>(v);
        }
        if (argc > 4) {
            const long long v = parse_ll("schur_cap", argv[4]);
            if (v < 1) {
                throw_usage(fmt::format("schur_cap: '{}' must be >= 1", argv[4]));
            }
            opts.qp.schur_cap = static_cast<Index>(v);
        }
        if (argc > 5) {
            const std::string ws = argv[5];
            if (ws != "border" && ws != "refactorize") {
                throw_usage(fmt::format("ws_algebra: '{}' is not one of border|refactorize", ws));
            }
            opts.qp.ws_algebra = (ws == "refactorize") ? WorkingSetLinearAlgebra::kRefactorize
                                                       : WorkingSetLinearAlgebra::kSchurBorder;
        }
        if (argc > 6) {
            const double v = parse_double("qp_opt_tol", argv[6]);
            if (!(v > 0.0)) {
                throw_usage(fmt::format("qp_opt_tol: '{}' must be > 0", argv[6]));
            }
            opts.qp.opt_tol = v;
        }
        if (argc > 7) {
            const long long v = parse_ll("crash_basis", argv[7]);
            if (v != 0 && v != 1) {
                throw_usage(fmt::format("crash_basis: '{}' must be 0 or 1", argv[7]));
            }
            opts.crash_basis = v != 0;
        }

        // F7CollocationChain's ctor validates nodes/states/controls/radius, and
        // its analytic accessors validate p against the design range (0, R).
        // f_star(p) is evaluated HERE, before the solve rather than after it,
        // purely so an out-of-range p fails fast with the family's own message
        // instead of after a solve that could take an hour.
        F7CollocationChain model(static_cast<Index>(nodes), 3, 2, p, 1.0);
        const double f_star = model.f_star(p);

        SqpDriver driver(opts);
        hven::solvers::Ledger ledger;
        driver.attach_ledger(&ledger, "f7_cold");
        const auto t0 = std::chrono::steady_clock::now();
        const SqpSolution sol = driver.solve(model, model.start_point());
        const auto t1 = std::chrono::steady_clock::now();

        const double x_err = (sol.x - model.x_star(p)).lpNorm<Eigen::Infinity>();
        const double f_err = std::abs(sol.f - f_star) / std::max(1.0, std::abs(f_star));

        // PHASE-6 TASK 3 added ws_adds/ws_drops/shift_adds/degenerate_steps
        // to this sum for the same reason schur_updates was already summed
        // here: SqpCounters (sqp_types.h) carries no field for any of them,
        // so the only route to a whole-solve figure is the attached Ledger's
        // per-QP SolveRecords. Nothing else about this program changed, and
        // all eleven are OBSERVATION ONLY (see QpCounters' own note) -- every
        // pre-Task-3 column above reproduces byte-for-byte.
        // PHASE-6 TASK 4 (M6) added `qpmax` -- the LARGEST per-subproblem
        // minor count of this solve -- for one reason: the size-derived cap
        // (types.h's QpOptions::max_iter, qp_engine.h's
        // detail::effective_qp_max_iter) applies PER SUBPROBLEM, so the
        // quantity a cap has to clear is this maximum and NOT the `minors`
        // total beside it. On this family the two are nearly equal (one
        // subproblem carries essentially the whole identification -- see
        // docs/notes/2026-08-03-identification-stall-study.md Sec. 3.1), but
        // "nearly" is not a calibration, and the note's Sec. 4 table is built
        // on this column.
        Index qp_max_minors = 0;
        Index schur_updates = 0;
        Index ws_adds = 0;
        Index ws_drops = 0;
        Index shift_adds = 0;
        Index degenerate_steps = 0;
        Index degenerate_run_max = 0;
        Index ws_adds_bound = 0;
        Index ws_drops_bound = 0;
        Index distinct_ineq = 0;
        Index distinct_bound = 0;
        Index drop_ties = 0;
        Index ratio_ties = 0;
        for (const hven::solvers::SolveRecord &rec : ledger.records()) {
            qp_max_minors = std::max(qp_max_minors, rec.counters.minor_iters);
            schur_updates += rec.counters.schur_updates;
            ws_adds += rec.counters.ws_adds;
            ws_drops += rec.counters.ws_drops;
            shift_adds += rec.counters.shift_adds;
            degenerate_steps += rec.counters.degenerate_steps;
            degenerate_run_max = std::max(degenerate_run_max, rec.counters.degenerate_run_max);
            ws_adds_bound += rec.counters.ws_adds_bound;
            ws_drops_bound += rec.counters.ws_drops_bound;
            distinct_ineq += rec.counters.distinct_ineq_added;
            distinct_bound += rec.counters.distinct_bound_added;
            drop_ties += rec.counters.drop_ties;
            ratio_ties += rec.counters.ratio_ties;
        }
        const hven::solvers::test_support::AnalyticActiveSet analytic = model.active_set(p);
        const Index active_rows = static_cast<Index>(
            std::count(analytic.ineq_active.begin(), analytic.ineq_active.end(), 1));

        // M6's two derived columns, printed so a calibration row is
        // self-describing: `capbase` is n + mi + #bounded for THIS model
        // (F7's box is on the controls only -- 2 per node -- so it is 8N),
        // and `cap` is the cap this invocation actually ran at, resolved
        // through the same helper the engine uses.
        Index bounded = 0;
        for (Index i = 0; i < model.n(); ++i) {
            if (model.lower()(i) > -hven::solvers::detail::kEngineInfBound ||
                model.upper()(i) < hven::solvers::detail::kEngineInfBound) {
                ++bounded;
            }
        }
        const Index cap_base = model.n() + model.mi() + bounded;
        const Index cap_eff = opts.qp.max_iter > 0
                                  ? opts.qp.max_iter
                                  : hven::solvers::detail::derived_qp_max_iter(cap_base);

        fmt::print("N={} n={} p={:.6f} active={} status={} majors={} minors={} qpmax={} "
                   "capbase={} cap={} crash={} crashrows={} crashbnd={} fact={} "
                   "schur={} symb={} bref={} adds={} drops={} shifts={} degen={} degrun={} "
                   "addsb={} dropsb={} dineq={} dbnd={} dties={} rties={} otol={:.1e} "
                   "xerr={:.6e} ferr_rel={:.6e} wall={:.3f} rss={:.1f}\n",
                   nodes, model.n(), p, active_rows, to_string(sol.status),
                   sol.counters.major_iters, sol.counters.qp_minor_iters, qp_max_minors, cap_base,
                   cap_eff, opts.crash_basis ? 1 : 0, sol.counters.crash_seeded_rows,
                   sol.counters.crash_seeded_bounds, sol.counters.factorizations, schur_updates,
                   sol.counters.symbolic_analyses, sol.counters.border_refine_steps, ws_adds,
                   ws_drops, shift_adds, degenerate_steps, degenerate_run_max, ws_adds_bound,
                   ws_drops_bound, distinct_ineq, distinct_bound, drop_ties, ratio_ties,
                   opts.qp.opt_tol, x_err, f_err, std::chrono::duration<double>(t1 - t0).count(),
                   peak_rss_mib());
        return 0;
    } catch (const std::exception &e) {
        fmt::print(stderr, "hven_sqp_f7_cold: error: {}\n", e.what());
        return 1;
    }
}
