// M7 W2 M3b -- THE FIRST-QP PER-MINOR TRACE PROBE.
//
// Adapted from `docs/notes/data/2026-09-m7-w2-m3a-request-probe/wrappers/
// m3a_probe.cpp` (M3a's request probe), which is NEVER edited: it produced
// committed rows and a gate's evidence must stay reproducible from the program
// that produced it. Three things are different here and they are the whole of
// the adaptation:
//
//   1. THE BUILD. M3a's probe LINKS AN INSTALLED hven. This one cannot: the
//      instrument it reads (`hven::solvers::detail::testing::QpWalkObserver`,
//      include/hven/detail/qp/qp_walk_trace.h) compiles to NOTHING without
//      `HVEN_TESTING`, and `HVEN_TESTING` is never defined on the production
//      library. So this program is built by `probe-CMakeLists.txt` as a
//      STANDALONE EXECUTABLE that RECOMPILES hven's own sources a SECOND time
//      with `HVEN_TESTING` defined target-wide and DOES NOT LINK `hven::hven`
//      -- exactly the convention CLAUDE.md section 8 mandates and
//      docs/testing.md describes, and exactly what the three seam test
//      executables in tests/CMakeLists.txt already do. This program is the
//      fourth such target and it lives outside the repository's own build.
//
//   2. NO DRIVER ACCESSOR IS NEEDED, AND NONE WAS ADDED. The observer is
//      STATIC -- `QpWalkObserver::active`, `::capture_solve`, `::records` are
//      static members, the same arrangement every other observer on this seam
//      uses, for the reason qp_walk_trace.h states: the engine is reached
//      through a deep call chain and the reader needs what it recorded after
//      the call returns. So arming it before `SqpSolver::solve` records the
//      driver's OWN main walk engine, through the public surface, with NO new
//      accessor in `src/drivers/sqp_solver.cpp` and NO change to any file under
//      src/ or include/. `capture_solve = 0` selects the FIRST `QpEngine::run`
//      of the process, which the identity gate then PROVES was the first QP of
//      major 0 by reproducing M3a's committed record 0.
//
//   3. `restoration_sub_status` IS A `RestorationSubStatus`. M3a's probe is
//      pinned to `11b066e8`'s surface and prints it as a `SolveStatus`; the
//      M3a fix round changed the field's type. This program prints it through
//      `to_string(RestorationSubStatus)`.
//
// WHAT IT PRINTS. The same PROBE_BEGIN key block and LEDGER_FIRST block M3a's
// probe prints, key for key and name for name, so the identity gate can compare
// this program's record 0 against M3a's COMMITTED
// `rows/f/<cell>@F-trinf.txt` without a translation table. Plus, when
// `--trace on`, the per-minor trace, written to the path `--trace-out` names
// (never to stdout: 4 680 rows do not belong in a row file).
//
// WHAT IT CHANGES IN THE SOLVER: NOTHING. It sets `tr_init`, which is a public
// option, and it may STOP the solve after the first QP through the public
// iteration callback. The observer is record-only and its neutrality is proved
// by tests/qp/test_qp_walk_seam.cpp in both configurations AND, here, by the
// identity gate against a row produced by a DIFFERENT BINARY that had no
// observer in it at all.
//
// CLAUDE.md section 7: wall-clock is informational; counters and the trace are
// the currency. HOLDOUT is never run. /home/ghecht/Projects/hven-bench is
// READ-ONLY and its CMake is never entered.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
#include <hven/detail/qp/qp_walk_trace.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp_problem_model.h>

#include "lanes/cutest/cutest_source.h"
#include "lanes/hven/problem_source.h"

#ifndef HVEN_TESTING
#error "m3b_probe must be compiled with HVEN_TESTING: without it the walk observer is not there"
#endif

namespace {

using hven::Index;
using hven::Vec;
namespace seam = hven::solvers::detail::testing;

void kv(const char *k, double v) { std::printf("%s=%.17g\n", k, v); }
void kvi(const char *k, long long v) { std::printf("%s=%lld\n", k, v); }
void kvs(const char *k, const std::string &v) { std::printf("%s=%s\n", k, v.c_str()); }

// core/solver_status.h declares to_string(SolveStatus) but none for QpStatus,
// so this program spells the four itself -- M3a's probe's own words and its own
// four names, so the two programs' rows are comparable as STRINGS.
const char *qp_status_name(hven::solvers::QpStatus s) {
    switch (s) {
    case hven::solvers::QpStatus::kOptimal:
        return "optimal";
    case hven::solvers::QpStatus::kMaxIter:
        return "max_iter";
    case hven::solvers::QpStatus::kInfeasible:
        return "infeasible";
    case hven::solvers::QpStatus::kNumericalError:
        return "numerical_error";
    }
    return "UNRECOGNIZED";
}

// QpCounters' twenty fields in DECLARATION ORDER (core/solver_counters.h),
// copied from M3a's probe unchanged: the identity gate compares THESE twenty
// names against M3a's committed row, so the list must not be re-ordered or
// re-spelled here.
const char *const kQpCounterFields[] = {"factorizations",
                                        "schur_updates",
                                        "minor_iters",
                                        "eqp_refine_steps",
                                        "border_refine_steps",
                                        "verdict_refine_steps",
                                        "suspect_escalations",
                                        "k0_reused",
                                        "symbolic_analyses",
                                        "ws_adds",
                                        "ws_drops",
                                        "shift_adds",
                                        "degenerate_steps",
                                        "degenerate_run_max",
                                        "ws_adds_bound",
                                        "ws_drops_bound",
                                        "distinct_ineq_added",
                                        "distinct_bound_added",
                                        "drop_ties",
                                        "ratio_ties"};
constexpr int kQpCounterFieldCount = 20;
static_assert(sizeof(kQpCounterFields) / sizeof(kQpCounterFields[0]) == kQpCounterFieldCount,
              "the name table and the extractor below must agree");

constexpr int kIdxMinorIters = 2;
constexpr int kIdxWsAdds = 9;
constexpr int kIdxWsDrops = 10;
constexpr int kIdxShiftAdds = 11;
constexpr int kIdxDegenerateSteps = 12;

void qp_counter_values(const hven::solvers::QpCounters &q, long long (&out)[kQpCounterFieldCount]) {
    out[0] = static_cast<long long>(q.factorizations);
    out[1] = static_cast<long long>(q.schur_updates);
    out[2] = static_cast<long long>(q.minor_iters);
    out[3] = static_cast<long long>(q.eqp_refine_steps);
    out[4] = static_cast<long long>(q.border_refine_steps);
    out[5] = static_cast<long long>(q.verdict_refine_steps);
    out[6] = static_cast<long long>(q.suspect_escalations);
    out[7] = q.k0_reused ? 1 : 0;
    out[8] = static_cast<long long>(q.symbolic_analyses);
    out[9] = static_cast<long long>(q.ws_adds);
    out[10] = static_cast<long long>(q.ws_drops);
    out[11] = static_cast<long long>(q.shift_adds);
    out[12] = static_cast<long long>(q.degenerate_steps);
    out[13] = static_cast<long long>(q.degenerate_run_max);
    out[14] = static_cast<long long>(q.ws_adds_bound);
    out[15] = static_cast<long long>(q.ws_drops_bound);
    out[16] = static_cast<long long>(q.distinct_ineq_added);
    out[17] = static_cast<long long>(q.distinct_bound_added);
    out[18] = static_cast<long long>(q.drop_ties);
    out[19] = static_cast<long long>(q.ratio_ties);
}

std::string degenerate_fraction(const long long (&v)[kQpCounterFieldCount]) {
    if (v[kIdxMinorIters] == 0) {
        return "n/a (minor_iters = 0)";
    }
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.17g",
                  static_cast<double>(v[kIdxDegenerateSteps]) /
                      static_cast<double>(v[kIdxMinorIters]));
    return std::string(buf);
}

// ---- the trace's own vocabulary ------------------------------------------
// Every enumerator is spelled, and an unrecognised value is SAID rather than
// printed as an integer a reader would have to decode. `kUnset` is printed as
// itself: RULE.md section 2.2 makes it an ABORT, and an abort condition that
// the artifact cannot show would not be one.
const char *action_name(seam::QpWalkAction a) {
    switch (a) {
    case seam::QpWalkAction::kUnset:
        return "unset";
    case seam::QpWalkAction::kStartRepair:
        return "start_repair";
    case seam::QpWalkAction::kPostProbeRestart:
        return "post_probe_restart";
    case seam::QpWalkAction::kDropDantzig:
        return "drop_dantzig";
    case seam::QpWalkAction::kDropZeroProbe:
        return "drop_zero_probe";
    case seam::QpWalkAction::kSuspectEscalation:
        return "suspect_escalation";
    case seam::QpWalkAction::kTerminal:
        return "terminal";
    case seam::QpWalkAction::kRideStepped:
        return "ride_stepped";
    case seam::QpWalkAction::kRideUnbounded:
        return "ride_unbounded";
    case seam::QpWalkAction::kAdd:
        return "add";
    case seam::QpWalkAction::kMoveOnly:
        return "move_only";
    }
    return "UNRECOGNIZED";
}

const char *block_name(seam::QpWalkBlock b) {
    switch (b) {
    case seam::QpWalkBlock::kNone:
        return "none";
    case seam::QpWalkBlock::kIneq:
        return "ineq";
    case seam::QpWalkBlock::kBound:
        return "bound";
    }
    return "UNRECOGNIZED";
}

const char *route_name(seam::QpWalkDropRoute r) {
    switch (r) {
    case seam::QpWalkDropRoute::kNone:
        return "none";
    case seam::QpWalkDropRoute::kDantzig:
        return "dantzig";
    case seam::QpWalkDropRoute::kZeroProbe:
        return "zero_probe";
    }
    return "UNRECOGNIZED";
}

const char *side_name(hven::solvers::BoundState s) {
    switch (s) {
    case hven::solvers::BoundState::kFree:
        return "free";
    case hven::solvers::BoundState::kAtLower:
        return "lower";
    case hven::solvers::BoundState::kAtUpper:
        return "upper";
    case hven::solvers::BoundState::kFixed:
        return "fixed";
    }
    return "UNRECOGNIZED";
}

// A double for the trace. NaN is the observer's declared ABSENT value and is
// written as `nan` -- never as 0, which would read as "the loop computed zero".
void put_double(std::FILE *f, double v) {
    if (std::isnan(v)) {
        std::fprintf(f, "nan");
    } else {
        std::fprintf(f, "%.17g", v);
    }
}

int write_trace(const std::string &path, const std::string &cell, double tr_init) {
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        std::fprintf(stderr, "m3b_probe: cannot open --trace-out %s\n", path.c_str());
        return 1;
    }
    // THE HEADER IS PART OF THE EVIDENCE. Everything a reader needs to know
    // which solve this is, before a single row.
    std::fprintf(f, "# M7 W2 M3b first-QP per-minor trace\n");
    std::fprintf(f, "# cell=%s\n", cell.c_str());
    std::fprintf(f, "# tr_init=%.17g\n", tr_init);
    std::fprintf(f, "# solve_index=%lld\n", static_cast<long long>(seam::QpWalkObserver::solve_index));
    std::fprintf(f, "# solves_seen=%lld\n", static_cast<long long>(seam::QpWalkObserver::solves));
    std::fprintf(f, "# warm=%d\n", seam::QpWalkObserver::warm ? 1 : 0);
    std::fprintf(f, "# n=%lld\n", static_cast<long long>(seam::QpWalkObserver::n));
    std::fprintf(f, "# me=%lld\n", static_cast<long long>(seam::QpWalkObserver::me));
    std::fprintf(f, "# mi=%lld\n", static_cast<long long>(seam::QpWalkObserver::mi));
    std::fprintf(f, "# max_iter=%lld\n", static_cast<long long>(seam::QpWalkObserver::max_iter));
    std::fprintf(f, "# incoming_ws_signature=%llu\n",
                 static_cast<unsigned long long>(seam::QpWalkObserver::incoming_ws_signature));
    std::fprintf(f, "# incoming_ws_size=%lld\n",
                 static_cast<long long>(seam::QpWalkObserver::incoming_ws_size));
    std::fprintf(f, "# exit_status=%s\n",
                 qp_status_name(static_cast<hven::solvers::QpStatus>(
                     seam::QpWalkObserver::exit_status)));
    std::fprintf(f, "# exit_status_raw=%d\n", seam::QpWalkObserver::exit_status);
    std::fprintf(f, "# truncated=%d\n", seam::QpWalkObserver::truncated ? 1 : 0);
    std::fprintf(f, "# max_records=%llu\n",
                 static_cast<unsigned long long>(seam::QpWalkObserver::max_records));
    std::fprintf(f, "# records=%llu\n",
                 static_cast<unsigned long long>(seam::QpWalkObserver::records.size()));
    std::fprintf(f, "solve,minor,action,block,block_index,block_side,drop_route,bound_gap,"
                    "alpha,p_inf,dx_inf,step_tol,degenerate,ride,raw_ratio,ratio_tied,"
                    "ratio_capped,decision_multiplier,decision_tol,best_multiplier,"
                    "drop_candidates,drop_tied,shift_adds,ws_size_in,ws_sig_in,ws_size_out,"
                    "ws_sig_out,x_inf,x_l1\n");
    for (const seam::QpWalkMinorRecord &r : seam::QpWalkObserver::records) {
        std::fprintf(f, "%lld,%lld,%s,%s,%lld,%s,%s,", static_cast<long long>(r.solve),
                     static_cast<long long>(r.minor), action_name(r.action), block_name(r.block),
                     static_cast<long long>(r.block_index), side_name(r.block_side),
                     route_name(r.drop_route));
        put_double(f, r.bound_gap);
        std::fprintf(f, ",");
        put_double(f, r.alpha);
        std::fprintf(f, ",");
        put_double(f, r.p_inf);
        std::fprintf(f, ",");
        put_double(f, r.dx_inf);
        std::fprintf(f, ",");
        put_double(f, r.step_tol);
        std::fprintf(f, ",%d,%d,", r.degenerate ? 1 : 0, r.ride ? 1 : 0);
        put_double(f, r.raw_ratio);
        std::fprintf(f, ",%lld,%d,", static_cast<long long>(r.ratio_tied), r.ratio_capped ? 1 : 0);
        put_double(f, r.decision_multiplier);
        std::fprintf(f, ",");
        put_double(f, r.decision_tol);
        std::fprintf(f, ",");
        put_double(f, r.best_multiplier);
        std::fprintf(f, ",%lld,%lld,%lld,%lld,%llu,%lld,%llu,",
                     static_cast<long long>(r.drop_candidates),
                     static_cast<long long>(r.drop_tied), static_cast<long long>(r.shift_adds),
                     static_cast<long long>(r.ws_size_in),
                     static_cast<unsigned long long>(r.ws_sig_in),
                     static_cast<long long>(r.ws_size_out),
                     static_cast<unsigned long long>(r.ws_sig_out));
        put_double(f, r.x_inf);
        std::fprintf(f, ",");
        put_double(f, r.x_l1);
        std::fprintf(f, "\n");
    }
    const int rc = std::fclose(f);
    if (rc != 0) {
        std::fprintf(stderr, "m3b_probe: writing %s failed on close\n", path.c_str());
        return 1;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    std::string problem_name;
    std::string trace_out;
    // hven's own default; `inf` is what this task measures and is passed
    // explicitly on every leg.
    double tr_init = 1.0;
    double deadline_seconds = 300.0;
    // `off` is the CONTROL ARM: the identical solve with the observer never
    // armed. RULE.md section 2 runs it first, so "the rebuilt binary reproduces
    // M3a's row" is established BEFORE the observer is switched on, and
    // "the observer changed nothing" is then a second, independent comparison.
    bool trace_on = false;
    // Stop the driver as soon as the FIRST QP is finished. Independent of the
    // observer: it is armed off `QpWalkObserver`'s own completion when the
    // observer is on, and off the iteration index when it is off.
    bool stop_after_first_qp = true;
    // The ledger is how record 0 is read, exactly as in M3a's probe.
    bool attach_ledger_flag = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--problem" && i + 1 < argc) {
            problem_name = argv[++i];
        } else if (arg == "--tr-init" && i + 1 < argc) {
            tr_init = std::atof(argv[++i]);
            if (!(tr_init > 0.0) || std::isnan(tr_init)) {
                std::fprintf(stderr, "m3b_probe: --tr-init takes a value > 0 (+inf allowed)\n");
                return 2;
            }
        } else if (arg == "--deadline" && i + 1 < argc) {
            deadline_seconds = std::atof(argv[++i]);
            if (!(deadline_seconds > 0.0) || !std::isfinite(deadline_seconds)) {
                std::fprintf(stderr, "m3b_probe: --deadline takes a finite value > 0\n");
                return 2;
            }
        } else if (arg == "--trace" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                trace_on = true;
            } else if (value != "off") {
                std::fprintf(stderr, "m3b_probe: --trace takes on|off\n");
                return 2;
            }
        } else if (arg == "--trace-out" && i + 1 < argc) {
            trace_out = argv[++i];
        } else if (arg == "--stop-after-first-qp" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                stop_after_first_qp = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m3b_probe: --stop-after-first-qp takes on|off\n");
                return 2;
            }
        } else if (arg == "--ledger" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                attach_ledger_flag = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m3b_probe: --ledger takes on|off\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "m3b_probe: unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (problem_name.empty()) {
        std::fprintf(stderr, "usage: m3b_probe --problem NAME [--tr-init r] [--deadline s] "
                             "[--trace on|off] [--trace-out PATH] "
                             "[--stop-after-first-qp on|off] [--ledger on|off]\n");
        return 2;
    }
    if (trace_on && trace_out.empty()) {
        std::fprintf(stderr, "m3b_probe: --trace on requires --trace-out PATH\n");
        return 2;
    }

    // The SET'S OWN ROSTER is the guard, not this program's -- M3a's probe's
    // rule, kept.
    const std::vector<std::string> &names = hven_bench::cutest::cutest_problems();
    bool known = false;
    for (const std::string &candidate : names) {
        if (candidate == problem_name) {
            known = true;
            break;
        }
    }
    if (!known) {
        std::fprintf(stderr, "m3b_probe: %s is not in the cutest-nlp-35 roster\n",
                     problem_name.c_str());
        return 2;
    }
    const auto source = std::make_shared<hven_bench::cutest::CutestSource>(problem_name);

    try {
        const auto problem = source->make_model();
        auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);

        // The lane's `default` label, spelled out: hven's own defaults plus the
        // two the harness sets on every hven row. Identical to M3a's probe.
        hven::solvers::SqpOptions options{};
        options.common.threads = 1;
        options.common.print_level = 10;
        options.tr_init = tr_init;

        hven::solvers::SqpSolver solver(options);
        std::unique_ptr<hven::solvers::Ledger> ledger;
        if (attach_ledger_flag) {
            ledger = std::make_unique<hven::solvers::Ledger>();
            solver.attach_ledger(ledger.get(), "f");
        }

        // ARM THE OBSERVER, BEFORE solve(). Static by design (qp_walk_trace.h):
        // no accessor on the driver is needed or was added. `capture_solve = 0`
        // takes the FIRST QpEngine::run of this process and nothing else; that
        // it IS the first QP of major 0 is not assumed here, it is PROVED by
        // the identity gate against M3a's committed record 0.
        seam::QpWalkObserver::reset();
        if (trace_on) {
            seam::QpWalkObserver::active = true;
            seam::QpWalkObserver::capture_solve = 0;
        }

        bool deadline_fired = false;
        bool stopped_after_first_qp = false;
        long long callback_events = 0;
        solver.set_iteration_callback([&](const hven::solvers::IterationEvent &ev) {
            ++callback_events;
            // The 300 s SOLVER deadline (M3a's RULE.md section 6, kept), and the
            // only place other than the first-QP stop that this callback returns
            // anything but kContinue.
            if (ev.elapsed_seconds > deadline_seconds) {
                deadline_fired = true;
                return hven::solvers::CallbackAction::kStop;
            }
            if (!stop_after_first_qp) {
                return hven::solvers::CallbackAction::kContinue;
            }
            // FIRST-QP STOP. With the observer on, the condition is the
            // observer's OWN completion of solve 0 -- `capturing` has cleared
            // and an exit status has been written -- which is exactly "the
            // first QP has returned" and does not depend on what an
            // IterationEvent's index means. With it off, the ledger's record
            // count is the same fact read through the public surface; with
            // neither, one event is taken as one major.
            bool first_qp_done = false;
            if (trace_on) {
                first_qp_done = !seam::QpWalkObserver::capturing &&
                                seam::QpWalkObserver::solve_index == 0 &&
                                seam::QpWalkObserver::exit_status >= 0;
            } else if (ledger != nullptr) {
                first_qp_done = !ledger->records().empty();
            } else {
                first_qp_done = callback_events >= 1;
            }
            if (first_qp_done) {
                stopped_after_first_qp = true;
                return hven::solvers::CallbackAction::kStop;
            }
            return hven::solvers::CallbackAction::kContinue;
        });

        hven::solvers::SolveBudget budget{};
        budget.max_iterations = 0;

        const hven::solvers::SqpResult result = solver.solve(*model, source->x0(), budget);

        // Disarm before anything else touches a QpEngine.
        seam::QpWalkObserver::active = false;

        std::printf("PROBE_BEGIN\n");
        kvs("problem", problem_name);
        kv("tr_init", tr_init);
        kvs("trace", trace_on ? "on" : "off");
        kvs("stop_after_first_qp", stop_after_first_qp ? "on" : "off");
        kvi("stopped_after_first_qp", stopped_after_first_qp ? 1 : 0);
        kvi("callback_events", callback_events);
        kvs("ledger", attach_ledger_flag ? "on" : "off");
        kv("deadline_seconds", deadline_seconds);
        kvi("deadline_fired", deadline_fired ? 1 : 0);
        kvi("n", static_cast<long long>(model->n()));
        kvi("me", static_cast<long long>(model->me()));
        kvi("mi", static_cast<long long>(model->mi()));
        kvs("status", hven::solvers::to_string(result.status));
        kv("f", result.f);
        kv("stationarity", result.stationarity);
        kv("feasibility_e", result.feasibility_e);
        kv("feasibility_i", result.feasibility_i);
        kv("complementarity", result.complementarity);
        kvi("infeasibility_certified", result.infeasibility_certified ? 1 : 0);

        const auto &c = result.counters;
        kvi("major_iters", static_cast<long long>(c.major_iters));
        kvi("restoration_iters", static_cast<long long>(c.restoration_iters));
        kvi("qp_minor_iters", static_cast<long long>(c.qp_minor_iters));
        kvi("factorizations", static_cast<long long>(c.factorizations));
        kvi("symbolic_analyses", static_cast<long long>(c.symbolic_analyses));
        kvi("steps_accepted", static_cast<long long>(c.steps_accepted));
        kvi("rejected_steps", static_cast<long long>(c.rejected_steps));
        kvi("qp_maxiter_exits", static_cast<long long>(c.qp_maxiter_exits));
        kvi("qp_numerical_exits", static_cast<long long>(c.qp_numerical_exits));
        kvi("qp_failures_routed", static_cast<long long>(c.qp_failures_routed));
        kvi("border_refine_steps", static_cast<long long>(c.border_refine_steps));
        kvi("restoration_entries", static_cast<long long>(c.restoration_entries));
        // ADAPTATION 3: a RestorationSubStatus, not a SolveStatus.
        kvs("restoration_sub_status", hven::solvers::to_string(c.restoration_sub_status));

        std::printf("LEDGER_BEGIN\n");
        kvi("ledger_attached", ledger ? 1 : 0);
        if (ledger == nullptr) {
            kvs("ledger_note", "no Ledger was constructed and attach_ledger was never called");
            std::printf("LEDGER_END\n");
        } else {
            const std::vector<hven::solvers::SolveRecord> &qp_records = ledger->records();
            kvi("ledger_qp_records", static_cast<long long>(qp_records.size()));
            kvi("ledger_sqp_records", static_cast<long long>(ledger->sqp_records().size()));
            if (!ledger->sqp_records().empty()) {
                kvs("ledger_sqp_status",
                    hven::solvers::to_string(ledger->sqp_records().front().status));
            }
            // THE FIRST RECORD, in full -- the object RULE.md section 2.1's
            // identity gate compares. An absent record is SAID, never
            // defaulted: a row of zeroes would read as a measurement.
            std::printf("LEDGER_FIRST_BEGIN\n");
            if (qp_records.empty()) {
                kvs("first_record", "n/a (no QP-engine record)");
            } else {
                const hven::solvers::SolveRecord &r0 = qp_records.front();
                kvs("first_label", r0.label);
                kvi("first_warm", r0.warm ? 1 : 0);
                kvs("first_qp_record_status", qp_status_name(r0.status));
                long long v[kQpCounterFieldCount];
                qp_counter_values(r0.counters, v);
                for (int i = 0; i < kQpCounterFieldCount; ++i) {
                    std::printf("first_%s=%lld\n", kQpCounterFields[i], v[i]);
                }
                kvs("first_degenerate_fraction", degenerate_fraction(v));
                kvi("first_ws_adds_minus_drops", v[kIdxWsAdds] - v[kIdxWsDrops]);
                kvi("first_ws_net_change", v[kIdxWsAdds] - v[kIdxWsDrops] + v[kIdxShiftAdds]);
            }
            std::printf("LEDGER_FIRST_END\n");
            std::printf("LEDGER_END\n");
        }

        // THE OBSERVER'S OWN REPORT, printed whether or not it was armed, so a
        // control leg's row SAYS it recorded nothing rather than being silent
        // about it.
        std::printf("TRACE_BEGIN\n");
        kvs("trace_armed", trace_on ? "on" : "off");
        kvi("trace_solves_seen", static_cast<long long>(seam::QpWalkObserver::solves));
        kvi("trace_solve_index", static_cast<long long>(seam::QpWalkObserver::solve_index));
        kvi("trace_records", static_cast<long long>(seam::QpWalkObserver::records.size()));
        kvi("trace_truncated", seam::QpWalkObserver::truncated ? 1 : 0);
        kvi("trace_warm", seam::QpWalkObserver::warm ? 1 : 0);
        kvi("trace_n", static_cast<long long>(seam::QpWalkObserver::n));
        kvi("trace_me", static_cast<long long>(seam::QpWalkObserver::me));
        kvi("trace_mi", static_cast<long long>(seam::QpWalkObserver::mi));
        kvi("trace_max_iter", static_cast<long long>(seam::QpWalkObserver::max_iter));
        std::printf("trace_incoming_ws_signature=%llu\n",
                    static_cast<unsigned long long>(seam::QpWalkObserver::incoming_ws_signature));
        kvi("trace_incoming_ws_size", static_cast<long long>(seam::QpWalkObserver::incoming_ws_size));
        kvi("trace_exit_status_raw", seam::QpWalkObserver::exit_status);
        if (seam::QpWalkObserver::exit_status >= 0) {
            kvs("trace_exit_status", qp_status_name(static_cast<hven::solvers::QpStatus>(
                                         seam::QpWalkObserver::exit_status)));
        }
        if (trace_on) {
            const int wrc = write_trace(trace_out, problem_name, tr_init);
            if (wrc != 0) {
                std::printf("trace_written=0\n");
                std::printf("TRACE_END\n");
                return 1;
            }
            kvs("trace_out", trace_out);
            kvi("trace_written", 1);
        } else {
            kvi("trace_written", 0);
        }
        std::printf("TRACE_END\n");
        std::printf("PROBE_END\n");
        return 0;
    } catch (const std::exception &e) {
        std::fprintf(stderr, "m3b_probe: %s\n", e.what());
        return 1;
    }
}
