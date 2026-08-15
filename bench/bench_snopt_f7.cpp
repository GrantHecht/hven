// bench/bench_snopt_f7.cpp — PHASE-6 TASK 0. THE SNOPT MEASUREMENT BINARY:
// the CLI over bench/snopt_f7_driver.h that produced every SNOPT column in
// docs/notes/2026-08-02-snopt-first-contact.md, and that Task 6 re-runs
// VERBATIM for the full three-solver calibration.
//
// READ bench/snopt_f7_driver.h FIRST. The source firewall, the F7 -> snOptA
// mapping in both directions, the 0-based indexing fact, where the counters
// come from, how the warm basis is carried, and why SNOPT's tolerances are
// relative while ours are absolute -- all of that is documented there, not
// here. This file is argument parsing, a sweep loop, and a CSV writer.
//
// ---------------------------------------------------------------------
// UNITS, WHICH BURNED A PHASE-5 REVIEW AND ARE THEREFORE STATED TWICE.
// --n IS THE NODE COUNT N, NOT THE VARIABLE COUNT. F7 at N nodes with this
// file's fixed ns = 3, nc = 2 has n = N*(ns+nc) = 5N variables. The CSV
// carries BOTH, in separate columns named N and n, and so does every line of
// stdout. N = 2000 means n = 10^4; N = 20000 means n = 10^5.
//
// ---------------------------------------------------------------------
// THE CSV SCHEMA IS FIXED BY THE PHASE SPEC and is not extended here:
//
//   solver,family,N,n,p,arm,status,iterations_major,iterations_minor,
//   wall_seconds,x_err_inf,f_err_rel
//
// Task 6 re-runs this binary and joins on those columns, so adding a
// thirteenth would be a schema change made by a measurement task, which is
// exactly the kind of drift the fixed schema exists to prevent. THE
// DIAGNOSTICS THAT DO NOT FIT -- absolute primal infeasibility, superbasic
// count, SNOPT's nInf/sInf, the snSTOP call count -- go to STDOUT as a
// key=value line in bench_f7_cold.cpp's style, and the first-contact note
// cites the captured stdout logs for those. Nothing in the note comes from a
// number that was not written to one of the two.
//
// `solver` is always `snopt` and `arm` is always the START TYPE THAT WAS
// ACTUALLY USED for that row -- `cold` for a Start = 0 solve, `warm` for a
// Start = 2 one. So `--arm warm --sweep k` emits ONE cold row (the seed,
// which has nothing to warm-start from) followed by k warm rows. Labelling by
// the requested arm instead would have put a cold solve in a row marked warm.
//
// PER-SOLVER COUNTER SEMANTICS ARE NEVER CONFLATED. `iterations_major` and
// `iterations_minor` here are SNOPT's own SQP major count and total QP minor
// count (guide section 8.6's "No. of major iterations" and "No. of
// iterations"). They are NOT SqpCounters::major_iters and ::qp_minor_iters:
// different QP solver, different Hessian, different active-set algebra. The
// note's tables prefix every counter with its solver and never put ours and
// SNOPT's in one column. The column NAMES are shared only because the schema
// is; the MEANINGS are not.
//
// ---------------------------------------------------------------------
// USAGE
//     hven_sqp_snopt_f7 --n <nodes> --p <val> --arm cold|warm --csv <path>
//                        [--p0 <val>] [--sweep <steps>]
//                        [--major-feas-tol <val>] [--major-opt-tol <val>]
//                        [--minor-feas-tol <val>]
//                        [--major-iter-limit <k>] [--minor-iter-limit <k>]
//                        [--iter-limit <k>] [--superbasics-limit <k>]
//                        [--reduced-hessian-dim <k>] [--time-limit <secs>]
//                        [--snopt-option "<verbatim option string>"]...
//                        [--print-file <path>] [--dump-solution <path>]
//
// A single solve is `--n N --p P --arm cold`. A sweep is `--p0 A --p B
// --sweep K`, which visits K+1 evenly spaced parameter values from A to B
// inclusive; under `--arm warm` every point after the first is a Start = 2
// re-solve carrying the previous point's basis, primal iterate and duals.
//
// --dump-solution writes bench_cli.h's solution-dump format (the same one
// hven_sqp_bench writes and prototypes/psiopt_bridge/run_comparison.py
// reads), so a SNOPT solution can be cross-validated against ours by the
// existing tooling rather than by a new comparison path. On a sweep it is
// the LAST point that is dumped.
//
// T6: every argument rejection throws std::invalid_argument with this file's
// usage text folded into the message (bench_cli.h's shared helpers); main()
// catches, prints to stderr, returns 1. Nothing prints a diagnostic it does
// not also fold into the throw.

#include <chrono>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fmt/format.h>

#include <hven/qp/qp_types.h>

#include "bench_cli.h"
#include "snopt_f7_driver.h"

#include "support/scale_problems.h"

namespace {

using hven::solvers::Index;
using hven::solvers::Vec;
using hven::solvers::snopt_bridge::SnoptF7Driver;
using hven::solvers::snopt_bridge::SnoptOptions;
using hven::solvers::snopt_bridge::SnoptResult;
using hven::solvers::test_support::F7CollocationChain;
using hven::solvers::test_support::peak_rss_mib;

constexpr const char *kUsage =
    "usage: hven_sqp_snopt_f7 --n <nodes> --p <val> --arm cold|warm --csv <path>\n"
    "                          [--p0 <val>] [--sweep <steps>]\n"
    "                          [--major-feas-tol <val>] [--major-opt-tol <val>]\n"
    "                          [--minor-feas-tol <val>]\n"
    "                          [--major-iter-limit <k>] [--minor-iter-limit <k>]\n"
    "                          [--iter-limit <k>] [--superbasics-limit <k>]\n"
    "                          [--reduced-hessian-dim <k>] [--time-limit <secs>]\n"
    "                          [--snopt-option \"<verbatim string>\"]...\n"
    "                          [--print-file <path>] [--dump-solution <path>]\n"
    "\n"
    "  --n       NODE COUNT N >= 3. The variable count is n = 5N (ns = 3, nc = 2).\n"
    "  --p       family parameter, 0 < p < R = 1. p <= 0.5 empty window.\n"
    "            On a sweep this is the LAST point.\n"
    "  --arm     cold  -> every point solved from F7's own start point (Start = 0).\n"
    "            warm  -> first point cold, every later point Start = 2 carrying\n"
    "                     the previous point's basis, iterate and duals.\n"
    "  --csv     output path for the fixed phase schema (see this file's banner).\n"
    "  --p0      first point of a sweep. Requires --sweep.\n"
    "  --sweep   number of STEPS; the sweep visits --sweep + 1 points from --p0\n"
    "            to --p inclusive. Requires --p0.\n"
    "\n"
    "  --snopt-option  a VERBATIM SNOPT option string (guide section 7.5), repeatable,\n"
    "                  applied after every typed flag. The escape hatch for options\n"
    "                  this CLI has no name for -- e.g. \"QPSolver CG\".\n"
    "\n"
    "  Tolerance and limit flags are passed straight through to SNOPT and are\n"
    "  UNSET by default, so an unflagged run measures SNOPT at its own resolved\n"
    "  defaults. See bench/snopt_f7_driver.h on why SNOPT's tolerances are\n"
    "  relative where ours are absolute.\n"
    "\n"
    "Solves F7 through SNOPT and writes one CSV row per point. Wall time and\n"
    "RSS are INFORMATIONAL ONLY, per the standing phase rule.\n";

[[noreturn]] void throw_usage(const std::string &detail) {
    hven::solvers::bench_cli::throw_usage(kUsage, detail);
}

long long parse_ll(const std::string &what, const std::string &value) {
    return hven::solvers::bench_cli::parse_ll(kUsage, what, value);
}

double parse_double(const std::string &what, const std::string &value) {
    return hven::solvers::bench_cli::parse_double(kUsage, what, value);
}

// Fetch the value that follows `flag`, or throw with the usage text (T6).
const char *value_for(int argc, char **argv, int &i, const std::string &flag) {
    if (i + 1 >= argc) {
        throw_usage(fmt::format("{}: missing value", flag));
    }
    return argv[++i];
}

struct Config {
    long long nodes = 0;
    double p = 0.0;
    double p0 = 0.0;
    long long sweep = 0;
    bool have_p = false;
    bool have_p0 = false;
    bool have_sweep = false;
    std::string arm;
    std::string csv;
    std::string print_file;
    std::string dump_solution;
    SnoptOptions snopt;
};

Config parse(int argc, char **argv) {
    Config c;
    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];
        if (flag == "--n") {
            c.nodes = parse_ll("--n", value_for(argc, argv, i, flag));
        } else if (flag == "--p") {
            c.p = parse_double("--p", value_for(argc, argv, i, flag));
            c.have_p = true;
        } else if (flag == "--p0") {
            c.p0 = parse_double("--p0", value_for(argc, argv, i, flag));
            c.have_p0 = true;
        } else if (flag == "--sweep") {
            c.sweep = parse_ll("--sweep", value_for(argc, argv, i, flag));
            c.have_sweep = true;
        } else if (flag == "--arm") {
            c.arm = value_for(argc, argv, i, flag);
        } else if (flag == "--csv") {
            c.csv = value_for(argc, argv, i, flag);
        } else if (flag == "--print-file") {
            c.print_file = value_for(argc, argv, i, flag);
        } else if (flag == "--dump-solution") {
            c.dump_solution = value_for(argc, argv, i, flag);
        } else if (flag == "--major-feas-tol") {
            c.snopt.major_feas_tol = parse_double(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--major-opt-tol") {
            c.snopt.major_opt_tol = parse_double(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--minor-feas-tol") {
            c.snopt.minor_feas_tol = parse_double(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--major-iter-limit") {
            c.snopt.major_iter_limit = parse_ll(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--minor-iter-limit") {
            c.snopt.minor_iter_limit = parse_ll(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--iter-limit") {
            c.snopt.iter_limit = parse_ll(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--superbasics-limit") {
            c.snopt.superbasics_limit = parse_ll(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--reduced-hessian-dim") {
            c.snopt.reduced_hessian_dim = parse_ll(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--time-limit") {
            c.snopt.time_limit_seconds = parse_double(flag, value_for(argc, argv, i, flag));
        } else if (flag == "--snopt-option") {
            c.snopt.raw_options.emplace_back(value_for(argc, argv, i, flag));
        } else {
            throw_usage(fmt::format("unrecognised argument '{}'", flag));
        }
    }

    if (c.nodes < 3) {
        throw_usage(fmt::format("--n: node count must be >= 3, got {}", c.nodes));
    }
    if (!c.have_p) {
        throw_usage("--p is required");
    }
    if (c.arm != "cold" && c.arm != "warm") {
        throw_usage(fmt::format("--arm: '{}' is not one of cold|warm", c.arm));
    }
    if (c.csv.empty()) {
        throw_usage("--csv is required");
    }
    // --p0 and --sweep define a grid together; either alone is a silent
    // half-request, and silently ignoring one would produce a single-point
    // measurement in a file the caller believed held a sweep.
    if (c.have_p0 != c.have_sweep) {
        throw_usage("--p0 and --sweep must be given together");
    }
    if (c.have_sweep && c.sweep < 1) {
        throw_usage(fmt::format("--sweep: step count must be >= 1, got {}", c.sweep));
    }
    c.snopt.print_file = c.print_file;
    return c;
}

} // namespace

int main(int argc, char **argv) {
    try {
        if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            fmt::print("{}", kUsage);
            return 0;
        }
        const Config cfg = parse(argc, argv);

        // The parameter grid. A single solve is the degenerate one-point case,
        // so there is exactly one code path below and no "single" special case
        // that could drift away from the sweep's.
        std::vector<double> points;
        if (cfg.have_sweep) {
            for (long long i = 0; i <= cfg.sweep; ++i) {
                const double t = static_cast<double>(i) / static_cast<double>(cfg.sweep);
                points.push_back(cfg.p0 + t * (cfg.p - cfg.p0));
            }
        } else {
            points.push_back(cfg.p);
        }

        // F7CollocationChain validates (nodes, ns, nc, p, radius) in its ctor,
        // and its analytic accessors validate p against the design range
        // (0, R). Constructing at the FIRST point makes an out-of-range grid
        // fail before any solve runs rather than after the expensive ones.
        F7CollocationChain model(static_cast<Index>(cfg.nodes), 3, 2, points.front(), 1.0);

        std::ofstream csv =
            hven::solvers::bench_cli::open_output_or_throw(kUsage, "--csv", cfg.csv);
        csv << "solver,family,N,n,p,arm,status,iterations_major,iterations_minor,"
               "wall_seconds,x_err_inf,f_err_rel\n";

        // ONE driver for the whole sweep. That is the warm arm's entire
        // mechanism: the driver owns xstate/Fstate/x/Fmul/nS for its lifetime,
        // so not resetting them between points IS the SNOPT warm start (guide
        // section 3.4). The cold arm re-seeds the start point at every point,
        // which discards exactly that state.
        SnoptF7Driver driver(model, cfg.snopt);

        for (std::size_t i = 0; i < points.size(); ++i) {
            const double p = points[i];
            driver.set_parameter(p);

            // The FIRST point of a warm sweep has nothing to warm-start from,
            // so it is a cold solve and is labelled cold. Everything after it
            // in a warm sweep is Start = 2.
            const bool warm_here = (cfg.arm == "warm") && (i > 0);
            if (!warm_here) {
                driver.set_start_point(model.start_point());
            }
            const SnoptResult r = driver.solve(warm_here ? 2 : 0);
            const char *arm_label = warm_here ? "warm" : "cold";

            csv << fmt::format("snopt,F7,{},{},{:.6f},{},{},{},{},{:.6f},{:.6e},{:.6e}\n",
                               cfg.nodes, model.n(), p, arm_label, r.status, r.majors, r.minors,
                               r.wall_seconds, r.x_err_inf, r.f_err_rel);
            csv.flush();

            // Everything the fixed schema has no room for. See the banner: the
            // note cites these captured lines for the diagnostics, and the CSV
            // for the schema columns.
            fmt::print("solver=snopt N={} n={} p={:.6f} arm={} info={} status={} majors={} "
                       "minors={} stop_calls={} nS={} nInf={} sInf={:.6e} xerr={:.6e} "
                       "ferr_rel={:.6e} priminf_abs={:.6e} wall={:.3f} rss={:.1f}\n",
                       cfg.nodes, model.n(), p, arm_label, r.info, r.status, r.majors, r.minors,
                       r.stop_calls, r.superbasics, r.n_inf, r.s_inf, r.x_err_inf, r.f_err_rel,
                       r.prim_inf_abs, r.wall_seconds, peak_rss_mib());
            std::fflush(stdout);

            if (!cfg.dump_solution.empty() && i + 1 == points.size()) {
                const Vec x = driver.solution();
                std::ofstream dump = hven::solvers::bench_cli::open_output_or_throw(
                    kUsage, "--dump-solution", cfg.dump_solution);
                hven::solvers::bench_cli::write_solution_dump(dump, "F7", cfg.nodes, arm_label, p,
                                                              r.status, r.f, x.data(),
                                                              static_cast<std::size_t>(x.size()));
            }
        }
        return 0;
    } catch (const std::exception &e) {
        fmt::print(stderr, "hven_sqp_snopt_f7: error: {}\n", e.what());
        return 1;
    }
}
