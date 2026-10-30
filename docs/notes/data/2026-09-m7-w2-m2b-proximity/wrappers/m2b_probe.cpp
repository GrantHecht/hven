// M7 W2 M2b -- THE PROXIMITY LADDER'S INSTRUMENT: M2's probe
// (docs/notes/data/2026-09-m7-w2-m2-restoration/wrappers/m2_probe.cpp), verbatim
// but for TWO additions, both named here so a reader can diff the two files and
// find nothing else:
//
//   * `--proximity zeta` sets `SqpOptions::restoration_proximity`, the field
//     M2b landed at a declared OFF. The default is 0.0, i.e. the M2 probe's own
//     solve, which is what makes the zeta = 0 legs an OFF-IDENTITY CHECK of the
//     new library against M2's committed ladder rather than a new measurement.
//   * one column, `prox_rel = ||D (x - x_k)||^2` with `D` built from `x_k` the
//     same way the engine builds it (d_i = min(1, 1/|x_{k,i}|)) -- the quantity
//     the pre-registered decision rule is stated in and the one the M2 ladder
//     does not carry. It is a SUM over coordinates, so it can exceed dx_inf^2
//     by up to n, and it is scaled by d_i <= 1, so it can fall below it without
//     bound: neither direction is bounded from dx_inf, which is why the column
//     exists (design page section 3.5).
//
// ONE MEASUREMENT-MEANING CAVEAT, carried here as well as in the README: at
// zeta > 0 the sub-solve minimizes h + P, so a depth-1 row's `f_event` -- the
// wrapper's objective -- is NO LONGER the wrapper's violation. The identity
// `f_event == h` at a phase's first depth-1 row is a ZETA = 0 FACT.
//
// M7 W2 M2 -- THE RESTORATION EXCURSION LADDER'S INSTRUMENT.
//
// One solve of one CUTEst cell at the SQP lane's `default` options, with a
// per-iteration callback attached, printing ONE ROW PER RESTORATION ITERATION:
// the restoration iterate's h and f ON THE CALLER'S OWN MODEL, its distance
// from the requesting iterate, and the sub-solve's trust radius -- plus the
// S5 excursion record and the whole counters block at exit.
//
// WHY NOT THE HARNESS'S CELL BINARY. `hven_bench_cell` prints a hand-enumerated
// subset of `SqpCounters` (it has no S5 fields) and attaches no callback, so
// three of the five columns this ladder needs cannot come out of it: the
// per-restoration-iteration trust radius, ||x - x_k||_inf, and the record
// itself. The budget ladder the A-SQP read used -- one run per `--budget-iters
// k` -- gives the main model's f and h at each k and NOTHING ELSE, and it is
// kept here as the CONTROL rather than as the instrument (README section 3).
//
// WHY THE CALLBACK CANNOT MOVE THE MEASUREMENT. `CallbackAction::kContinue` is
// the only value returned, and hven's contract for it is explicit: "a callback
// that only ever returns this cannot move a counter, a status or a residual"
// (`drivers/solve_result.h`). The claim is not taken on trust -- the README's
// identity check compares this probe's uncapped counters against the H3 run-A
// row for the same cell, column by column.
//
// NOTHING IN THE HARNESS TREE IS WRITTEN OR MODIFIED, AND ITS CMake IS NOT
// ENTERED AT ALL (settler ruling, after the harness's re-pin task added a guard
// that REFUSES `find_package(hven)` resolving outside the pinned prefix -- which
// is precisely this probe's case, since it must link an hven AHEAD of that pin
// to see the S5 fields at all). This project COMPILES the three bridge sources
// it needs, read-only, out of the harness tree at its own sha, and links the
// installed hven directly:
//
//   lanes/cutest/cutest_model.cpp        the CUTEst -> NlpTripletModel bridge
//   lanes/cutest/cutest_source.cpp       the set's roster and its ProblemSource
//   lanes/hven/nlp_model_triplet_view.cpp the NlpModel view the bridge needs
//
// CONSEQUENCE, STATED HERE SO NO ROW CAN BE MISREAD: these binaries carry NO
// harness pin stamp by construction (`HVEN_BENCH_PIN_SHA` is the harness
// CMake's, and that CMake never runs). Every row this program prints is M2's
// LADDER and is never cited as a harness row.
//
// THE MODEL IS EVALUATED AFTER THE SOLVE, never inside the callback: CUTEst is
// process-global and holds exactly one decoded problem, so the callback COPIES
// each iterate and the objective/constraint evaluations happen once the solver
// has returned.

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp_problem_model.h>

#include "lanes/cutest/cutest_source.h"
#include "lanes/hven/problem_source.h"

namespace {

using hven::Index;
using hven::Vec;

struct Row {
    Index iteration = 0;
    Index depth = 0;
    double f_event = std::numeric_limits<double>::quiet_NaN();
    double radius = std::numeric_limits<double>::quiet_NaN();
    double step_norm = std::numeric_limits<double>::quiet_NaN();
    double elapsed = 0.0;
    Vec x;
};

double violation_l1(const Vec &ce, const Vec &ci) {
    double h = 0.0;
    for (Index i = 0; i < ce.size(); ++i) {
        h += std::abs(ce(i));
    }
    for (Index j = 0; j < ci.size(); ++j) {
        const double v = ci(j);
        h += (v > 0.0 || std::isnan(v)) ? v : 0.0;
    }
    return h;
}

void kv(const char *k, double v) { std::printf("%s=%.17g\n", k, v); }
void kvi(const char *k, long long v) { std::printf("%s=%lld\n", k, v); }
void kvs(const char *k, const std::string &v) { std::printf("%s=%s\n", k, v.c_str()); }

} // namespace

int main(int argc, char **argv) {
    std::string problem_name;
    long long budget_iters = 0;
    // THE CALLBACK'S OWN CONTROL ARM. `--callback off` runs the identical solve
    // with nothing attached, so "the instrument did not move the measurement"
    // is an A/B on this binary rather than an appeal to the contract.
    bool attach_callback = true;
    // THE ONE OPTION THIS TASK ADDS. 0.0 is OFF and is the shipped default, so
    // a leg without this argument is the M2 probe's own solve.
    double proximity = 0.0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--problem" && i + 1 < argc) {
            problem_name = argv[++i];
        } else if (arg == "--budget-iters" && i + 1 < argc) {
            budget_iters = std::atoll(argv[++i]);
        } else if (arg == "--proximity" && i + 1 < argc) {
            proximity = std::atof(argv[++i]);
            if (!(proximity >= 0.0) || !std::isfinite(proximity)) {
                std::fprintf(stderr, "m2b_probe: --proximity takes a finite value >= 0\n");
                return 2;
            }
        } else if (arg == "--callback" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                attach_callback = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m2b_probe: --callback takes on|off\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "m2b_probe: unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (problem_name.empty()) {
        std::fprintf(stderr,
                     "usage: m2b_probe --problem NAME [--budget-iters k] [--proximity zeta] "
                     "[--callback on|off]\n");
        return 2;
    }

    // THE SET'S OWN ROSTER IS THE GUARD, not this program's: `cutest_problems()`
    // is the literal list the harness's binary carries, so a name that is not
    // in it is refused here rather than handed to the decoder.
    const std::vector<std::string> &names = hven_bench::cutest::cutest_problems();
    bool known = false;
    for (const std::string &candidate : names) {
        if (candidate == problem_name) {
            known = true;
            break;
        }
    }
    if (!known) {
        std::fprintf(stderr, "m2b_probe: %s is not in the cutest-nlp-35 roster\n",
                     problem_name.c_str());
        return 2;
    }
    const auto source = std::make_shared<hven_bench::cutest::CutestSource>(problem_name);

    try {
        const auto problem = source->make_model();
        auto model = std::make_shared<hven::solvers::NlpProblemModel>(problem);

        // THE LANE'S `default` LABEL, spelled out: hven's own defaults plus the
        // two the harness sets on every hven row
        // (`lanes/hven/hven_bench_cell.cpp`'s `sqp_options`).
        hven::solvers::SqpOptions options{};
        options.common.threads = 1;
        options.common.print_level = 10;
        options.restoration_proximity = proximity;

        hven::solvers::SqpSolver solver(options);
        std::vector<Row> rows;
        if (attach_callback) {
            solver.set_iteration_callback([&rows](const hven::solvers::IterationEvent &ev) {
                Row row;
                row.iteration = ev.iteration;
                row.depth = ev.depth ? *ev.depth : 0;
                row.f_event = ev.f;
                row.radius = ev.radius ? *ev.radius : std::numeric_limits<double>::quiet_NaN();
                row.step_norm = ev.step_norm;
                row.elapsed = ev.elapsed_seconds;
                row.x = ev.x; // BORROWED for the call only -- copied here, as the contract says
                rows.push_back(std::move(row));
                return hven::solvers::CallbackAction::kContinue;
            });
        }

        hven::solvers::SolveBudget budget{};
        budget.max_iterations = static_cast<Index>(budget_iters);

        const hven::solvers::SqpResult result = solver.solve(*model, source->x0(), budget);

        std::printf("PROBE_BEGIN\n");
        kvs("problem", problem_name);
        kvi("budget_iters_argument", budget_iters);
        kvs("callback", attach_callback ? "on" : "off");
        kv("proximity", proximity);
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
        kvi("elastic_activations", static_cast<long long>(c.elastic_activations));
        kvi("elastic_escalations", static_cast<long long>(c.elastic_escalations));
        kvi("elastic_rho0_ceiling_hits", static_cast<long long>(c.elastic_rho0_ceiling_hits));
        kvi("elastic_floor_retries", static_cast<long long>(c.elastic_floor_retries));
        kvi("qp_maxiter_exits", static_cast<long long>(c.qp_maxiter_exits));
        kvi("qp_numerical_exits", static_cast<long long>(c.qp_numerical_exits));
        kvi("qp_failures_routed", static_cast<long long>(c.qp_failures_routed));
        kvi("qp_failures_in_a_row_peak", static_cast<long long>(c.qp_failures_in_a_row_peak));
        kvi("border_refine_steps", static_cast<long long>(c.border_refine_steps));
        // THE S5 RECORD.
        kvi("restoration_entries", static_cast<long long>(c.restoration_entries));
        kv("restoration_h_in", c.restoration_h_in);
        kv("restoration_h_out", c.restoration_h_out);
        kv("restoration_f_in", c.restoration_f_in);
        kv("restoration_f_out", c.restoration_f_out);

        // THE LADDER. `x_k` is the REQUESTING ITERATE, and finding it takes two
        // rules rather than one.
        //
        // ORDINARILY it is the last depth-0 event before the first depth-1 one.
        // That event is the major BEFORE the requesting one -- a major's row is
        // emitted after the phase it requested has returned -- but it is the
        // same POINT, because an iterate cannot move without an accepted step
        // and a restoration request follows rejections at an iterate that has
        // not moved. The `dx_inf` of the phase's own first row is 0 on every
        // cell where this rule fires, which is that argument checked rather
        // than asserted.
        //
        // WHEN THE PHASE IS ENTERED IN MAJOR 0 there is no such event at all,
        // and the requesting iterate is the START POINT. Reported rather than
        // left NaN: "the phase went nowhere" and "the probe could not say" are
        // different readings and must not look alike.
        //
        // WHEN NO PHASE RAN there is no requesting iterate and the column is
        // NaN by construction -- a distance from a point that does not exist is
        // not 0.
        //
        // `x_k_source` says which rule produced the column, so a reader never
        // has to infer it.
        const Index n = model->n();
        Vec x_k;
        bool have_x_k = false;
        Vec last_depth0;
        bool seen_depth0 = false;
        bool seen_depth1 = false;
        const char *x_k_source = "none (no restoration phase ran)";
        for (const Row &row : rows) {
            if (row.depth == 0) {
                if (!seen_depth1) {
                    last_depth0 = row.x;
                    seen_depth0 = true;
                }
                continue;
            }
            if (!seen_depth1) {
                seen_depth1 = true;
                if (seen_depth0) {
                    x_k = last_depth0;
                    have_x_k = true;
                    x_k_source = "depth0_event (the major before the requesting one)";
                } else {
                    x_k = source->x0();
                    have_x_k = true;
                    x_k_source = "start_point (the phase was entered in major 0)";
                }
            }
        }
        kvs("x_k_source", x_k_source);
        if (have_x_k && x_k.size() == n) {
            const Vec ce_k = model->me() > 0 ? model->eval_ce(x_k) : Vec();
            const Vec ci_k = model->mi() > 0 ? model->eval_ci(x_k) : Vec();
            kv("requesting_h", violation_l1(ce_k, ci_k));
            kv("requesting_f", model->eval_f(x_k));
        }
        // THE PROXIMITY SCALING, built from `x_k` EXACTLY as the engine builds
        // it (detail/globalization/sqp/restoration.h): d_i = min(1, 1/|x_{k,i}|),
        // with |x_{k,i}| == 0 taking the first branch and no division.
        Vec d_k;
        if (have_x_k && x_k.size() == n) {
            d_k = Vec::Ones(n);
            for (Index i = 0; i < n; ++i) {
                const double a = std::abs(x_k(i));
                d_k(i) = a > 1.0 ? 1.0 / a : 1.0;
            }
        }
        std::printf("LADDER_BEGIN\n");
        std::printf(
            "depth,event_iteration,f_event,radius,step_norm,f_main,h_main,dx_inf,prox_rel\n");
        for (const Row &row : rows) {
            double f_main = std::numeric_limits<double>::quiet_NaN();
            double h_main = std::numeric_limits<double>::quiet_NaN();
            double dx = std::numeric_limits<double>::quiet_NaN();
            double prox_rel = std::numeric_limits<double>::quiet_NaN();
            if (row.x.size() >= n) {
                const Vec x = row.x.head(n);
                if (x.allFinite()) {
                    const Vec ce = model->me() > 0 ? model->eval_ce(x) : Vec();
                    const Vec ci = model->mi() > 0 ? model->eval_ci(x) : Vec();
                    f_main = model->eval_f(x);
                    h_main = violation_l1(ce, ci);
                    if (have_x_k && x_k.size() == n) {
                        dx = (x - x_k).lpNorm<Eigen::Infinity>();
                        double acc = 0.0;
                        for (Index i = 0; i < n; ++i) {
                            const double t = d_k(i) * (x(i) - x_k(i));
                            acc += t * t;
                        }
                        prox_rel = acc;
                    }
                }
            }
            std::printf("%lld,%lld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n",
                        static_cast<long long>(row.depth), static_cast<long long>(row.iteration),
                        row.f_event, row.radius, row.step_norm, f_main, h_main, dx, prox_rel);
        }
        std::printf("LADDER_END\n");
        std::printf("PROBE_END\n");
        return 0;
    } catch (const std::exception &e) {
        std::printf("PROBE_BEGIN\nthrew=1\nexception=%s\nPROBE_END\n", e.what());
        return 4;
    }
}
