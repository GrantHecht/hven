// M7 W2 M3a ADDENDUM E -- THE QP-CAP EXPERIMENTS: the stage-1 probe below,
// verbatim but for TWO ARGUMENTS and FIVE PRINTED KEYS, all named here so a
// reader can diff against the binary stage 1 ran and find nothing else. Both
// arguments default to the SHIPPED value, so a leg that passes neither is
// stage 1's own solve -- which is what makes RULE-E.md section 3's identity leg
// an identity check rather than a fresh baseline.
//
//   * `--qp-cap-retry on|off` (default off) sets `SqpOptions::qp_cap_retry`,
//     RULE-E.md's E1 arm: re-solve a subproblem the WALK answered with
//     QpStatus::kMaxIter ONCE, at twice the cap that solve resolved, warm from
//     the capped solve's own WORKING SET. Default false, and off means no retry
//     is attempted and no second engine is constructed. Printed as the key
//     `qp_cap_retry`.
//
//   * `--qp-max-iter N` (default 0 = THE SENTINEL, the shipped default) sets
//     `SqpOptions::qp.max_iter`, RULE-E.md's E2 arm. A POSITIVE value is an
//     EXPLICIT, ABSOLUTE cap that wins outright at any size; the sentinel
//     derives the cap from the subproblem's own size. RULE-E.md section 1 states
//     the scope difference this creates against E1 -- an absolute cap applies to
//     EVERY QP solve in the solve, not only the first. Printed as the key
//     `qp_max_iter`.
//
//   * `qp_cap_retries` -- `SqpCounters::qp_cap_retries`, one per retry
//     ATTEMPTED whatever its outcome. Read it with `qp_maxiter_exits`: capped
//     exits are counted at the ROUTING SITE, so an attempt the retry RESCUED is
//     in `qp_cap_retries` alone, every other capped exit is in
//     `qp_maxiter_exits` alone, and a retry that hit the doubled cap too is in
//     BOTH (solver_counters.h:1435-1535).
//
//   * `first_qp_minor_iters` and `first_qp_factorizations` --
//     `SqpIterate::qp_minor_iters` and `SqpIterate::qp_factorizations` on
//     `history[0]`, both meaningful iff `qp_solved` and printed as
//     `n/a (qp_solved = 0)` otherwise, exactly as the first-QP group's other
//     keys are. These are what answer "how many minors did the first QP
//     actually need". On an `elastic_applied` row they describe the ELASTIC
//     solve, for the same reason `first_qp_status` does -- the row's whole qp_*
//     group is overwritten -- so they are read beside `first_qp_elastic_applied`
//     and never without it.
//
// `QpCounters::schur_updates` is NOT on `SqpCounters` and is therefore not
// reachable from `SqpResult`. RULE-E.md section 4 records it as ABSENT rather
// than substituting a neighbouring counter for it; the Schur/border quantities
// this probe can reach are `factorizations`, `symbolic_analyses` and
// `border_refine_steps`, all already printed below.
//
// M7 W2 M3a -- THE RESTORATION REQUEST PROBE: M2b's probe
// (docs/notes/data/2026-09-m7-w2-m2b-proximity/wrappers/m2b_probe.cpp), verbatim
// but for FOUR additions, all named here so a reader can diff the two files and
// find nothing else. Every addition is a NEW PRINTED KEY or a NEW ARGUMENT; not
// one line of the M2b measurement path changed, which is what makes RULE.md
// section 4's control leg an identity check rather than a fresh baseline.
//
//   * `--tr-init r` sets `SqpOptions::tr_init`, the l-infinity radius the FIRST
//     subproblem is given. The default is 1.0 -- the shipped value and M2b's --
//     so a leg without this argument is M2b's own solve. `+inf` is accepted and
//     is the "no window at all" arm: `SqpOptions::qp.tr_radius` is +inf by
//     default, so the window `min(st.delta, opts_.qp.tr_radius)` IS tr_init on
//     the shipped path. Printed as the key `tr_init`.
//
//   * `--deadline s` (default 300) is the SOLVER deadline of RULE.md section 6,
//     enforced HERE because hven has no wall-clock option: once an
//     IterationEvent reports elapsed_seconds past it, the callback returns
//     CallbackAction::kStop and the solve exits `interrupted` at the point it
//     was standing on. `deadline_seconds` and `deadline_fired` are printed on
//     every leg, so a TRUNCATED row can never be mistaken for a finished one.
//     THE DEADLINE FIRES ONLY BETWEEN ITERATIONS -- a solve that spends its
//     whole budget inside one subproblem emits no event and the deadline cannot
//     fire, which is what the chain's own outer `timeout` is for. With
//     `--callback off` there is no callback and therefore no deadline.
//
//   * `restoration_sub_status` -- M3a task A's field, the feasibility
//     sub-solve's own terminal status at the LAST entry. It has no "never
//     entered" sentinel: the record's absence rule is `restoration_entries == 0`,
//     so it is printed immediately after that field and is never to be read
//     without it.
//
//   * THE FIRST QP's OWN STATUS, and the ONE HOLE in it. `SqpResult::history` is
//     public and `SqpIterate::qp_status` is a public field, so `history[0]`'s is
//     reachable -- but on a major the ELASTIC tier supplied, the driver
//     overwrites that row's qp_* fields with the elastic solve's
//     (`SqpIterate::elastic_applied`'s own contract says so), and the plain
//     solve's kInfeasible is then NOT on the public surface. So three keys are
//     printed rather than one: `first_qp_status` (the row, verbatim),
//     `first_qp_elastic_applied` (the flag), and the derived
//     `first_qp_plain_status`, which resolves by RULE.md section 2.1 and carries
//     the word `(inferred)` INSIDE its value when it rests on the walk-mode
//     argument -- the elastic tier being the driver's only consumer of
//     QpStatus::kInfeasible -- rather than on a reading. The precondition for
//     that inference is CHECKED, not assumed: `elastic_from_ipqp_escape` is
//     printed and the inference is drawn only when it is 0. Where neither holds
//     the value is `unavailable`, and this probe does not invent one.
//
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

// core/solver_status.h declares to_string(SolveStatus) but none for QpStatus,
// so this program spells the four itself rather than printing an integer a
// reader would have to decode. The four values are QpStatus's whole set.
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
    // THE TWO OPTIONS THIS TASK ADDS. 1.0 is the shipped tr_init and M2b's, so a
    // leg without --tr-init is M2b's own solve; 300 s is RULE.md section 6's
    // solver deadline.
    double tr_init = 1.0;
    double deadline_seconds = 300.0;
    // THE TWO OPTIONS ADDENDUM E ADDS. `false` and `0` are the SHIPPED defaults
    // -- `qp_cap_retry` is off and `qp.max_iter` is the size-derived sentinel --
    // so a leg passing neither argument is stage 1's own solve, which is what
    // RULE-E.md section 3's identity leg checks.
    bool qp_cap_retry = false;
    long long qp_max_iter = 0;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--problem" && i + 1 < argc) {
            problem_name = argv[++i];
        } else if (arg == "--budget-iters" && i + 1 < argc) {
            budget_iters = std::atoll(argv[++i]);
        } else if (arg == "--tr-init" && i + 1 < argc) {
            // `inf` / `+inf` / `Infinity` all parse through strtod, which is
            // what atof calls; the "no window at all" arm is spelled in the
            // chain as `inf`.
            tr_init = std::atof(argv[++i]);
            if (!(tr_init > 0.0) || std::isnan(tr_init)) {
                std::fprintf(stderr, "m3a_probe: --tr-init takes a value > 0 (+inf allowed)\n");
                return 2;
            }
        } else if (arg == "--deadline" && i + 1 < argc) {
            deadline_seconds = std::atof(argv[++i]);
            if (!(deadline_seconds > 0.0) || !std::isfinite(deadline_seconds)) {
                std::fprintf(stderr, "m3a_probe: --deadline takes a finite value > 0\n");
                return 2;
            }
        } else if (arg == "--proximity" && i + 1 < argc) {
            proximity = std::atof(argv[++i]);
            if (!(proximity >= 0.0) || !std::isfinite(proximity)) {
                std::fprintf(stderr, "m3a_probe: --proximity takes a finite value >= 0\n");
                return 2;
            }
        } else if (arg == "--qp-cap-retry" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                qp_cap_retry = true;
            } else if (value != "off") {
                std::fprintf(stderr, "m3a_probe: --qp-cap-retry takes on|off\n");
                return 2;
            }
        } else if (arg == "--qp-max-iter" && i + 1 < argc) {
            // 0 is the SENTINEL and the shipped default (size-derived); a
            // POSITIVE value is an explicit absolute cap. A negative value is
            // refused here rather than passed through as a second spelling of
            // the sentinel -- RULE-E.md's E2 arm sets one literal integer and a
            // typo must not silently become the default.
            qp_max_iter = std::atoll(argv[++i]);
            if (qp_max_iter < 0) {
                std::fprintf(stderr, "m3a_probe: --qp-max-iter takes 0 (the shipped sentinel) or a "
                                     "positive absolute cap\n");
                return 2;
            }
        } else if (arg == "--callback" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                attach_callback = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m3a_probe: --callback takes on|off\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "m3a_probe: unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (problem_name.empty()) {
        std::fprintf(stderr,
                     "usage: m3a_probe --problem NAME [--budget-iters k] [--proximity zeta] "
                     "[--tr-init r] [--deadline s] [--callback on|off] "
                     "[--qp-cap-retry on|off] [--qp-max-iter N]\n");
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
        std::fprintf(stderr, "m3a_probe: %s is not in the cutest-nlp-35 roster\n",
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
        // THE ONE OPTION M3a VARIES. At the default 1.0 this assignment is a
        // no-op by value, which is what keeps the control leg an identity check.
        options.tr_init = tr_init;
        // THE TWO ADDENDUM E VARIES. At `false` and `0` both assignments are
        // no-ops by value, for the same reason: they ARE the shipped defaults.
        options.qp_cap_retry = qp_cap_retry;
        options.qp.max_iter = static_cast<Index>(qp_max_iter);

        hven::solvers::SqpSolver solver(options);
        std::vector<Row> rows;
        bool deadline_fired = false;
        if (attach_callback) {
            solver.set_iteration_callback([&rows, &deadline_fired, deadline_seconds](
                                              const hven::solvers::IterationEvent &ev) {
                Row row;
                row.iteration = ev.iteration;
                row.depth = ev.depth ? *ev.depth : 0;
                row.f_event = ev.f;
                row.radius = ev.radius ? *ev.radius : std::numeric_limits<double>::quiet_NaN();
                row.step_norm = ev.step_norm;
                row.elapsed = ev.elapsed_seconds;
                row.x = ev.x; // BORROWED for the call only -- copied here, as the contract says
                rows.push_back(std::move(row));
                // RULE.md section 6's SOLVER deadline, and the ONLY place this
                // callback can return anything but kContinue. The row is pushed
                // FIRST so the ladder carries the point the stop was taken at.
                // `elapsed_seconds` is informational everywhere else in this
                // program; here it is the stop condition, and a leg it stops is
                // TRUNCATED -- its counters are reported, its status is not
                // ranked, and it can neither qualify a radius nor condemn one.
                if (ev.elapsed_seconds > deadline_seconds) {
                    deadline_fired = true;
                    return hven::solvers::CallbackAction::kStop;
                }
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
        kv("tr_init", tr_init);
        // ADDENDUM E's two arguments, printed as keys so every row says which
        // arm produced it. `off` and `0` are the shipped defaults.
        kvs("qp_cap_retry", qp_cap_retry ? "on" : "off");
        kvi("qp_max_iter", qp_max_iter);
        kv("deadline_seconds", attach_callback ? deadline_seconds : 0.0);
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
        kvi("elastic_activations", static_cast<long long>(c.elastic_activations));
        kvi("elastic_escalations", static_cast<long long>(c.elastic_escalations));
        kvi("elastic_rho0_ceiling_hits", static_cast<long long>(c.elastic_rho0_ceiling_hits));
        kvi("elastic_floor_retries", static_cast<long long>(c.elastic_floor_retries));
        kvi("qp_maxiter_exits", static_cast<long long>(c.qp_maxiter_exits));
        kvi("qp_numerical_exits", static_cast<long long>(c.qp_numerical_exits));
        kvi("qp_failures_routed", static_cast<long long>(c.qp_failures_routed));
        kvi("qp_failures_in_a_row_peak", static_cast<long long>(c.qp_failures_in_a_row_peak));
        // ADDENDUM E. Counted at the ROUTING SITE alongside qp_maxiter_exits: a
        // capped attempt the retry RESCUED is HERE and not there; a retry that
        // hit the doubled cap too is in both (solver_counters.h:1435-1535).
        // Identically 0 at the shipped OFF state, which is the whole of what a
        // fired-counter is for.
        kvi("qp_cap_retries", static_cast<long long>(c.qp_cap_retries));
        kvi("border_refine_steps", static_cast<long long>(c.border_refine_steps));
        // THE PRECONDITION OF `first_qp_plain_status`'s INFERENCE, printed so it
        // is checked rather than assumed: at this lane's `default` options
        // (qp_mode == kWalk) no interior-point tier runs, and this counter is 0.
        kvi("elastic_from_ipqp_escape", static_cast<long long>(c.elastic_from_ipqp_escape));
        // THE S5 RECORD. `restoration_sub_status` (M3a task A) has NO "never
        // entered" sentinel -- the record's absence rule is
        // `restoration_entries == 0` -- so it is printed immediately after that
        // field and must never be read without it.
        kvi("restoration_entries", static_cast<long long>(c.restoration_entries));
        kvs("restoration_sub_status", hven::solvers::to_string(c.restoration_sub_status));
        kv("restoration_h_in", c.restoration_h_in);
        kv("restoration_h_out", c.restoration_h_out);
        kv("restoration_f_in", c.restoration_f_in);
        kv("restoration_f_out", c.restoration_f_out);

        // THE FIRST QP -- see this file's header note and RULE.md section 2.1.
        // `history.size()` tracks major_iters alone (restoration produces no
        // history rows), so an empty history means no major reported at all and
        // every key below says so rather than defaulting to something readable.
        kvi("history_rows", static_cast<long long>(result.history.size()));
        if (result.history.empty()) {
            kvs("first_qp_solved", "n/a (empty history)");
            kvs("first_qp_status", "n/a (empty history)");
            kvs("first_qp_elastic_applied", "n/a (empty history)");
            kvs("first_qp_plain_status", "unavailable (empty history)");
            kvs("first_qp_verdict", "n/a (empty history)");
            kvs("first_qp_tr_radius", "n/a (empty history)");
            kvs("first_qp_tr_binding", "n/a (empty history)");
            kvs("first_qp_restoration_seed_used", "n/a (empty history)");
            kvs("first_qp_minor_iters", "n/a (empty history)");
            kvs("first_qp_factorizations", "n/a (empty history)");
        } else {
            const hven::solvers::SqpIterate &h0 = result.history.front();
            kvi("first_qp_solved", h0.qp_solved ? 1 : 0);
            // `qp_status` is meaningful iff `qp_solved`; an unsolved row's value
            // is the enum's default and is NOT reported as a reading.
            kvs("first_qp_status",
                h0.qp_solved ? qp_status_name(h0.qp_status) : "n/a (qp_solved = 0)");
            kvi("first_qp_elastic_applied", h0.elastic_applied ? 1 : 0);
            std::string plain;
            if (!h0.qp_solved) {
                plain = "unavailable (qp_solved = 0)";
            } else if (!h0.elastic_applied) {
                plain = qp_status_name(h0.qp_status);
            } else if (c.elastic_from_ipqp_escape == 0) {
                plain = "infeasible (inferred)";
            } else {
                plain = "unavailable (elastic_applied with a tier escape)";
            }
            kvs("first_qp_plain_status", plain);
            kvs("first_qp_verdict", hven::solvers::to_string(h0.verdict));
            kv("first_qp_tr_radius", h0.tr_radius);
            kvi("first_qp_tr_binding", h0.tr_binding ? 1 : 0);
            kvi("first_qp_restoration_seed_used", h0.restoration_seed_used ? 1 : 0);
            // ADDENDUM E -- RULE-E.md section 4's "how many minors did the first
            // QP actually need". Both are meaningful iff `qp_solved`, on the
            // same contract as `qp_status` above, and on an `elastic_applied`
            // row they describe the ELASTIC solve because the row's whole qp_*
            // group is overwritten: read them beside
            // `first_qp_elastic_applied`, never without it.
            if (h0.qp_solved) {
                kvi("first_qp_minor_iters", static_cast<long long>(h0.qp_minor_iters));
                kvi("first_qp_factorizations", static_cast<long long>(h0.qp_factorizations));
            } else {
                kvs("first_qp_minor_iters", "n/a (qp_solved = 0)");
                kvs("first_qp_factorizations", "n/a (qp_solved = 0)");
            }
        }

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
