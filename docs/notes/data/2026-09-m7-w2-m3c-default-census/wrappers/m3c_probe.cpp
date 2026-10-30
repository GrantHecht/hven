// M7 W2 M3c -- THE DEFAULT-PATH CENSUS PROBE: M3a's addendum-F probe
// (docs/notes/data/2026-09-m7-w2-m3a-request-probe/wrappers/m3a_probe.cpp),
// COPIED at HEAD b2bbaed8 and extended by TWO ARGUMENTS and THREE NEW BLOCKS,
// all named here so a reader can diff the two files and find nothing else.
// NOT ONE LINE OF THE SOLVE PATH DIFFERS, which is what makes RULE.md section
// 4's identity check against the harness's committed rows an identity check
// rather than a fresh baseline. The M3a original is NOT edited: it is evidence
// pinned to the rows it produced (M3a fix1's ruling on exactly this point).
//
//   * `--ladder on|off` (DEFAULT on, i.e. the M3a probe's behaviour). At `off`
//     the per-iteration callback does NOT copy the iterate, and the post-solve
//     model evaluations the LADDER block performs are not performed. THE SOLVE
//     IS UNCHANGED EITHER WAY -- the callback is still attached, still returns
//     kContinue, and still carries RULE.md section 3.1's deadline; what changes
//     is this program's own post-processing. Every census leg runs `off`: this
//     task reads none of the ladder's columns (they are M2's instrument), and
//     on a 300 s wall cell the per-event evaluations of f, cE and cI would cost
//     more than the solve. Printed as the key `ladder`, and the LADDER block
//     still prints its rows with the four model-derived columns as `nan` --
//     absent is SAID, never defaulted to a readable number.
//
//   * `--history-tail K` (DEFAULT 12) and the `HISTORY_BEGIN` ... `HISTORY_END`
//     BLOCK: the LAST K rows of `SqpResult::history`, with ALL TWENTY-NINE
//     public `SqpIterate` fields on each, in DECLARATION ORDER, beside the
//     row's ABSOLUTE index. Not a chosen subset -- a subset is what makes a
//     later question unanswerable. This is the evidence RULE.md sections 5 and
//     6 read: the terminating major's own `qp_status`, `elastic_applied`,
//     `verdict` and `tr_radius`.
//
//   * The `COUNTERS_BEGIN` ... `COUNTERS_END` BLOCK: ALL FORTY-NINE public
//     `SqpCounters` fields, in declaration order, prefixed `c_`. The
//     PROBE_BEGIN key block already prints twenty-odd of them under their bare
//     names and keeps doing so, because M3a's identity machinery names them
//     that way; this block is the TOTAL surface, and the prefix is what keeps
//     the two from colliding.
//
//   * The `OPTIONS_BEGIN` ... `OPTIONS_END` BLOCK: the option values RULE.md's
//     predicates read, prefixed `opt_`, taken off the `SqpOptions` object THIS
//     PROCESS CONSTRUCTED -- never off a header. A predicate with a threshold
//     reads the threshold from the leg's own row.
//
// THE THREE NEW BLOCKS SIT AFTER `LADDER_END` and before `PROBE_END`, beside
// the LEDGER block, for M3a's reason: the PROBE_BEGIN key block gains exactly
// TWO keys (`ladder` and `history_tail`) and nothing else.
//
// M7 W2 M3a ADDENDUM F -- THE DEGENERATE-STALL PROBE: the addendum-E probe
// below, verbatim but for ONE ARGUMENT, ONE PRINTED KEY and ONE NEW BLOCK, all
// named here so a reader can diff against the binary addendum E ran and find
// nothing else. The argument defaults to the E probe's behaviour EXACTLY, so a
// leg that passes neither it nor E's two arguments is stage 1's own solve --
// which is what makes RULE-F.md section 3.1's identity leg an identity check
// rather than a fresh baseline.
//
//   * `--ledger on|off` (DEFAULT off) constructs a `hven::solvers::Ledger` and
//     hands it to `SqpSolver::attach_ledger`. `off` is not "skip the reads": no
//     Ledger object is constructed and attach_ledger is never called, so
//     `SqpSolver::ledger_` stays nullptr and the driver takes the same branch it
//     takes for every caller that never asked for one. Printed as the key
//     `ledger`.
//
//   * the `LEDGER_BEGIN` ... `LEDGER_END` BLOCK, emitted AFTER `LADDER_END` and
//     before `PROBE_END`. It is a block of its own precisely so the
//     `PROBE_BEGIN` key block -- the thing RULE.md section 4's identity
//     machinery compares against the committed rows -- gains exactly ONE key
//     and nothing else.
//
// WHAT THE LEDGER IS, AND WHY IT IS REACHED THIS WAY. Addendum E's write-up
// recorded that `QpCounters::schur_updates` and the whole degeneracy set are
// "not reachable from SqpResult". THAT IS TRUE AND STAYS TRUE: `SqpCounters`
// does not carry those fields. What it missed is a SECOND public surface --
// `hven::solvers::Ledger` (core/ledger.h) is a public, read-only instrument;
// `SqpSolver::attach_ledger` is a public method that ALSO forwards the same
// Ledger to the driver's own QpEngine; and `Ledger::records()` hands back one
// `SolveRecord` PER QP-ENGINE SOLVE, each carrying a whole `QpCounters`. The
// harness's own M7 W2 C1 cost census already read the degeneracy set this way.
//
// ITS SCOPE, STATED HERE AND IN RULE-F.md SECTION 1.2 SO NO RECORD COUNT IS
// MISREAD: attach_ledger attaches to the driver's MAIN WALK ENGINE and to
// nothing else. The restoration phase's own sub-driver receives attach_trace
// but NO attach_ledger (core/ledger.h says so), and the four lazily-built
// engines (ipqp, elastic-ipqp, ssn, cap-retry) are never handed it either. So
// `records().size()` is the number of solves the main walk engine made and is a
// LOWER BOUND on the QP solves the whole solve performed.
//
// EVERY FIELD, NOT A CHOSEN SUBSET. `QpCounters` has exactly twenty fields and
// all twenty are printed on every record, in declaration order, beside the
// record's label, warm flag and terminal QpStatus. Two quantities RULE-F.md's
// ruling names do NOT exist on this struct and are recorded as ABSENT rather
// than substituted for: there is no negative-curvature field (the ride's own
// blocking constraint is folded into `ws_adds`, by that field's own doc) and no
// inertia field (`IpqpCounters::ipqp_final_inertia_read` is a different struct
// describing a tier that does not run here). `suspect_escalations` is the one
// escalation counter this struct carries and is printed like the rest.
//
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
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
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

// ADDENDUM F -- THE PER-RECORD PRINT CAP, RULE-F.md section 5. A PRINT cap on
// the artifact's size, NOT a measurement cap: the aggregates below are computed
// over EVERY record whatever this does, and record 0 -- the only one any verdict
// reads -- is always printed. Stage 1's majors and elastic activations put the
// expected record count in the tens, so it is not expected to bind.
constexpr int kMaxLedgerRecordsPrinted = 5000;

// ADDENDUM F -- QpCounters' twenty fields in DECLARATION ORDER
// (core/solver_counters.h:31). This list IS the struct's; nothing selects from
// it, which is why it sits beside an extractor rather than beside twenty print
// statements at each of the three sites that need it.
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

// The two indices that are NOT summed when a leg's records are aggregated
// (RULE-F.md section 4.2): `k0_reused` is a bool and folds as a COUNT of
// records reading true, `degenerate_run_max` is a PEAK and folds with max.
constexpr int kIdxK0Reused = 7;
constexpr int kIdxDegenerateRunMax = 13;
// The indices the two derived quantities and the verdict ratio are built from.
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

// `degenerate_steps / minor_iters` -- RULE-F.md section 4's ratio, formatted.
// An undefined ratio is SAID rather than printed as a number: RULE-F.md section
// 4's undefined case makes `minor_iters == 0` an UNDETERMINED cell, and a 0 here
// would read as "no degeneracy" instead.
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

// ---------------------------------------------------------------------------
// M7 W2 M3c -- THE THREE NEW BLOCKS' FIELD TABLES.
//
// Each list IS the struct's own declaration order, transcribed once, here,
// rather than spread over print statements at the call site. A field this
// program does not print is a field RULE.md's predicates could not read, so
// both lists are TOTAL and their totality is printed as a key (`c_field_count`,
// `h_field_count`) for the identity checker to pin.
// ---------------------------------------------------------------------------

// `SqpCounters`' forty-nine fields (core/solver_counters.h), in declaration
// order: forty-four Index, four double, one RestorationSubStatus.
constexpr int kSqpCounterFieldCount = 49;

// `SqpIterate`'s twenty-nine fields (drivers/sqp_solver_types.h), in
// declaration order. The header line of the HISTORY block is built from this
// table, so a name here and a value below can never drift apart unnoticed.
const char *const kIterateFields[] = {"trial",
                                      "f",
                                      "stationarity",
                                      "feasibility",
                                      "complementarity",
                                      "kkt_residual",
                                      "violation_l1",
                                      "tr_radius",
                                      "mu",
                                      "step_norm",
                                      "qp_solved",
                                      "ipqp_least_infeasible_primal",
                                      "ipqp_farkas_corroborated",
                                      "qp_status",
                                      "qp_minor_iters",
                                      "qp_factorizations",
                                      "tr_binding",
                                      "verdict",
                                      "soc_applied",
                                      "elastic_applied",
                                      "elastic_rho0_ceiling_hit",
                                      "restoration_seed_used",
                                      "watchdog_restored",
                                      "active_set_delta",
                                      "weak_active_rows",
                                      "near_active_rows",
                                      "active_rows",
                                      "active_lower_sides",
                                      "active_upper_sides"};
constexpr int kIterateFieldCount = 29;
static_assert(sizeof(kIterateFields) / sizeof(kIterateFields[0]) == kIterateFieldCount,
              "the HISTORY header table and the row writer below must agree");

// ONE ROW OF THE HISTORY BLOCK, in the table's order. `qp_status` is printed by
// NAME and is meaningful iff `qp_solved` -- an unsolved row's value is the
// enum's default and is NOT reported as a reading, exactly as M3a's
// `first_qp_status` treats the same field.
void print_history_row(long long index, const hven::solvers::SqpIterate &h) {
    std::printf("%lld,%lld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%.17g,%d,%s,"
                "%lld,%lld,%d,%s,%d,%d,%d,%d,%d,%lld,%lld,%lld,%lld,%lld,%lld\n",
                index, static_cast<long long>(h.trial), h.f, h.stationarity, h.feasibility,
                h.complementarity, h.kkt_residual, h.violation_l1, h.tr_radius, h.mu, h.step_norm,
                h.qp_solved ? 1 : 0, h.ipqp_least_infeasible_primal,
                h.ipqp_farkas_corroborated ? 1 : 0,
                h.qp_solved ? qp_status_name(h.qp_status) : "n/a(qp_solved=0)",
                static_cast<long long>(h.qp_minor_iters),
                static_cast<long long>(h.qp_factorizations), h.tr_binding ? 1 : 0,
                hven::solvers::to_string(h.verdict), h.soc_applied ? 1 : 0,
                h.elastic_applied ? 1 : 0, h.elastic_rho0_ceiling_hit ? 1 : 0,
                h.restoration_seed_used ? 1 : 0, h.watchdog_restored ? 1 : 0,
                static_cast<long long>(h.active_set_delta),
                static_cast<long long>(h.weak_active_rows),
                static_cast<long long>(h.near_active_rows), static_cast<long long>(h.active_rows),
                static_cast<long long>(h.active_lower_sides),
                static_cast<long long>(h.active_upper_sides));
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
    // THE ONE OPTION ADDENDUM F ADDS, and it is not a solver option at all: it
    // attaches a PUBLIC READ-ONLY INSTRUMENT. `false` is the E probe's behaviour
    // EXACTLY -- no Ledger is constructed and attach_ledger is never called --
    // which is what RULE-F.md section 3.1's identity leg checks.
    bool attach_ledger_flag = false;
    // THE TWO OPTIONS M3c ADDS, and NEITHER IS A SOLVER OPTION. `on` is the M3a
    // probe's behaviour exactly (the callback copies the iterate and the ladder
    // evaluates the model at it); `off` changes this program's post-processing
    // and nothing the solver does. 12 history rows is a PRINT depth, not a
    // measurement depth: `history_rows` is printed whole beside it.
    bool ladder_flag = true;
    long long history_tail = 12;
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
                std::fprintf(stderr, "m3c_probe: --tr-init takes a value > 0 (+inf allowed)\n");
                return 2;
            }
        } else if (arg == "--deadline" && i + 1 < argc) {
            deadline_seconds = std::atof(argv[++i]);
            if (!(deadline_seconds > 0.0) || !std::isfinite(deadline_seconds)) {
                std::fprintf(stderr, "m3c_probe: --deadline takes a finite value > 0\n");
                return 2;
            }
        } else if (arg == "--proximity" && i + 1 < argc) {
            proximity = std::atof(argv[++i]);
            if (!(proximity >= 0.0) || !std::isfinite(proximity)) {
                std::fprintf(stderr, "m3c_probe: --proximity takes a finite value >= 0\n");
                return 2;
            }
        } else if (arg == "--qp-cap-retry" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                qp_cap_retry = true;
            } else if (value != "off") {
                std::fprintf(stderr, "m3c_probe: --qp-cap-retry takes on|off\n");
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
                std::fprintf(stderr, "m3c_probe: --qp-max-iter takes 0 (the shipped sentinel) or a "
                                     "positive absolute cap\n");
                return 2;
            }
        } else if (arg == "--ledger" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                attach_ledger_flag = true;
            } else if (value != "off") {
                std::fprintf(stderr, "m3c_probe: --ledger takes on|off\n");
                return 2;
            }
        } else if (arg == "--ladder" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                ladder_flag = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m3c_probe: --ladder takes on|off\n");
                return 2;
            }
        } else if (arg == "--history-tail" && i + 1 < argc) {
            history_tail = std::atoll(argv[++i]);
            if (history_tail < 1) {
                std::fprintf(stderr, "m3c_probe: --history-tail takes a positive count\n");
                return 2;
            }
        } else if (arg == "--callback" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                attach_callback = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m3c_probe: --callback takes on|off\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "m3c_probe: unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (problem_name.empty()) {
        std::fprintf(stderr,
                     "usage: m3c_probe --problem NAME [--budget-iters k] [--proximity zeta] "
                     "[--tr-init r] [--deadline s] [--callback on|off] "
                     "[--qp-cap-retry on|off] [--qp-max-iter N] [--ledger on|off] "
                     "[--ladder on|off] [--history-tail K]\n");
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
        std::fprintf(stderr, "m3c_probe: %s is not in the cutest-nlp-35 roster\n",
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
        // ADDENDUM F. `--ledger off` does NOT merely skip the reads below: no
        // Ledger object is constructed and attach_ledger is never called, so the
        // driver's `ledger_` stays nullptr and it takes the same branch it takes
        // for every caller that never asked for one. That is what makes
        // RULE-F.md section 3.2's neutrality check a real A/B rather than an
        // appeal to the contract.
        std::unique_ptr<hven::solvers::Ledger> ledger;
        if (attach_ledger_flag) {
            ledger = std::make_unique<hven::solvers::Ledger>();
            solver.attach_ledger(ledger.get(), "f");
        }
        std::vector<Row> rows;
        bool deadline_fired = false;
        if (attach_callback) {
            solver.set_iteration_callback([&rows, &deadline_fired, deadline_seconds, ladder_flag](
                                              const hven::solvers::IterationEvent &ev) {
                Row row;
                row.iteration = ev.iteration;
                row.depth = ev.depth ? *ev.depth : 0;
                row.f_event = ev.f;
                row.radius = ev.radius ? *ev.radius : std::numeric_limits<double>::quiet_NaN();
                row.step_norm = ev.step_norm;
                row.elapsed = ev.elapsed_seconds;
                // BORROWED for the call only -- copied here, as the contract
                // says. M3c: at `--ladder off` the copy is not taken at all and
                // `row.x` stays empty; the ladder's four model-derived columns
                // then print `nan` (see LADDER below). The callback still fires,
                // still records the scalar event, and still carries the deadline
                // -- so the SOLVE sees the identical callback either way.
                if (ladder_flag) {
                    row.x = ev.x;
                }
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
        // ADDENDUM F's one new key in this block. Everything else it prints
        // lives in the LEDGER block after LADDER_END, which is not part of the
        // key block any identity check compares.
        kvs("ledger", attach_ledger_flag ? "on" : "off");
        // M3c's TWO new keys in this block, and its only two.
        kvs("ladder", ladder_flag ? "on" : "off");
        kvi("history_tail", history_tail);
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
        // M3c: at `--ladder off` no iterate was copied, so there is no x_k to
        // find and the source SAYS so rather than naming a rule it did not run.
        if (!ladder_flag) {
            x_k_source = "n/a (--ladder off: the callback copied no iterate)";
        }
        for (const Row &row : rows) {
            if (!ladder_flag) {
                break; // no iterate was copied; there is nothing to search
            }
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

        // -------------------------------------------------------------------
        // M7 W2 M3c -- THE OPTIONS BLOCK (RULE.md sections 2.1, 5.1).
        //
        // Read off the SqpOptions object THIS PROCESS CONSTRUCTED, never off a
        // header: a predicate whose bar is an option value must read that value
        // from the leg's own row, or the bar is this file's opinion about a
        // default rather than the default the solve ran under.
        // -------------------------------------------------------------------
        std::printf("OPTIONS_BEGIN\n");
        kv("opt_tr_init", options.tr_init);
        kv("opt_tr_min", options.tr_min);
        kv("opt_tr_max", options.tr_max);
        kv("opt_feas_tol", options.feas_tol);
        kv("opt_kkt_tol", options.kkt_tol);
        kvi("opt_max_iter", static_cast<long long>(options.max_iter));
        kvi("opt_qp_failure_budget", static_cast<long long>(options.qp_failure_budget));
        kvi("opt_qp_cap_retry", options.qp_cap_retry ? 1 : 0);
        kvi("opt_qp_max_iter", static_cast<long long>(options.qp.max_iter));
        kv("opt_restoration_proximity", options.restoration_proximity);
        kvi("opt_qp_mode_is_walk", options.qp_mode == hven::solvers::QpMode::kWalk ? 1 : 0);
        kvi("opt_project_start_point", options.project_start_point ? 1 : 0);
        kvi("opt_enable_soc", options.enable_soc ? 1 : 0);
        kvi("opt_adaptive_mu", options.adaptive_mu ? 1 : 0);
        kvi("opt_enable_scaling", options.enable_scaling ? 1 : 0);
        kvi("opt_threads", static_cast<long long>(options.common.threads));
        std::printf("OPTIONS_END\n");

        // -------------------------------------------------------------------
        // M7 W2 M3c -- THE COUNTERS BLOCK: ALL FORTY-NINE `SqpCounters` fields
        // in declaration order (core/solver_counters.h), prefixed `c_`.
        //
        // TOTAL, not a chosen subset. The PROBE_BEGIN block above prints a
        // subset of these under their bare names because M3a's identity
        // machinery names them that way; this block is the whole struct, and
        // `c_field_count` is printed so a checker can refuse a short block
        // rather than compare what happens to be there.
        // -------------------------------------------------------------------
        std::printf("COUNTERS_BEGIN\n");
        kvi("c_field_count", kSqpCounterFieldCount);
        kvi("c_major_iters", static_cast<long long>(c.major_iters));
        kvi("c_qp_minor_iters", static_cast<long long>(c.qp_minor_iters));
        kvi("c_factorizations", static_cast<long long>(c.factorizations));
        kvi("c_steps_accepted", static_cast<long long>(c.steps_accepted));
        kvi("c_rejected_steps", static_cast<long long>(c.rejected_steps));
        kvi("c_soc_steps", static_cast<long long>(c.soc_steps));
        kvi("c_soc_applied", static_cast<long long>(c.soc_applied));
        kvi("c_soc_qp_infeasible", static_cast<long long>(c.soc_qp_infeasible));
        kvi("c_soc_rejected", static_cast<long long>(c.soc_rejected));
        kvi("c_elastic_activations", static_cast<long long>(c.elastic_activations));
        kvi("c_elastic_escalations", static_cast<long long>(c.elastic_escalations));
        kvi("c_restoration_iters", static_cast<long long>(c.restoration_iters));
        kvi("c_elastic_from_ipqp_escape", static_cast<long long>(c.elastic_from_ipqp_escape));
        kvi("c_ipqp_suspicion_disproved", static_cast<long long>(c.ipqp_suspicion_disproved));
        kvi("c_ipqp_fallback_rung_b", static_cast<long long>(c.ipqp_fallback_rung_b));
        kvi("c_elastic_rho0_ceiling_hits", static_cast<long long>(c.elastic_rho0_ceiling_hits));
        kvi("c_elastic_floor_retries", static_cast<long long>(c.elastic_floor_retries));
        kvi("c_eqp_refine_steps", static_cast<long long>(c.eqp_refine_steps));
        kvi("c_border_refine_steps", static_cast<long long>(c.border_refine_steps));
        kvi("c_verdict_refine_steps", static_cast<long long>(c.verdict_refine_steps));
        kvi("c_suspect_escalations", static_cast<long long>(c.suspect_escalations));
        kvi("c_symbolic_analyses", static_cast<long long>(c.symbolic_analyses));
        kvi("c_full_step_majors", static_cast<long long>(c.full_step_majors));
        kvi("c_watchdog_restores", static_cast<long long>(c.watchdog_restores));
        kvi("c_evals_full", static_cast<long long>(c.evals_full));
        kvi("c_evals_values", static_cast<long long>(c.evals_values));
        kvi("c_probe_budget_stops", static_cast<long long>(c.probe_budget_stops));
        kvi("c_crash_seeded_rows", static_cast<long long>(c.crash_seeded_rows));
        kvi("c_crash_seeded_bounds", static_cast<long long>(c.crash_seeded_bounds));
        kvi("c_n_seeded", static_cast<long long>(c.n_seeded));
        kvi("c_seeded_clamped", static_cast<long long>(c.seeded_clamped));
        kvi("c_ip_activity_inferred", static_cast<long long>(c.ip_activity_inferred));
        kvi("c_active_set_delta_total", static_cast<long long>(c.active_set_delta_total));
        kvi("c_active_set_delta_peak", static_cast<long long>(c.active_set_delta_peak));
        kvi("c_weak_active_peak", static_cast<long long>(c.weak_active_peak));
        kvi("c_near_active_peak", static_cast<long long>(c.near_active_peak));
        kvi("c_polish_ignored", static_cast<long long>(c.polish_ignored));
        kvi("c_start_point_clamps", static_cast<long long>(c.start_point_clamps));
        kvi("c_qp_maxiter_exits", static_cast<long long>(c.qp_maxiter_exits));
        kvi("c_qp_numerical_exits", static_cast<long long>(c.qp_numerical_exits));
        kvi("c_qp_failures_routed", static_cast<long long>(c.qp_failures_routed));
        kvi("c_qp_failures_in_a_row_peak",
            static_cast<long long>(c.qp_failures_in_a_row_peak));
        kvi("c_qp_cap_retries", static_cast<long long>(c.qp_cap_retries));
        kvi("c_restoration_entries", static_cast<long long>(c.restoration_entries));
        kv("c_restoration_h_in", c.restoration_h_in);
        kv("c_restoration_h_out", c.restoration_h_out);
        kv("c_restoration_f_in", c.restoration_f_in);
        kv("c_restoration_f_out", c.restoration_f_out);
        kvs("c_restoration_sub_status", hven::solvers::to_string(c.restoration_sub_status));
        // THE ONE DERIVED COUNTER RULE.md section 5.1 defines, printed here
        // beside its two inputs so no reader has to subtract: `rejected_steps`
        // has exactly two increment sites in src/drivers/sqp_solver.cpp -- the
        // ROUTED QP FAILURE, which also increments `qp_failures_routed`, and the
        // JUDGED REJECTION, which does not -- so the difference is EXACT.
        kvi("c_derived_judged_rejections",
            static_cast<long long>(c.rejected_steps) -
                static_cast<long long>(c.qp_failures_routed));
        std::printf("COUNTERS_END\n");

        // -------------------------------------------------------------------
        // M7 W2 M3c -- THE HISTORY BLOCK: the LAST `history_tail` rows of
        // `SqpResult::history`, every field, in declaration order.
        //
        // `history_rows` (printed in the key block above) is the WHOLE count;
        // this block's own `history_printed_from` says which absolute index the
        // table starts at, so a tail is never mistaken for the whole history.
        // -------------------------------------------------------------------
        std::printf("HISTORY_BEGIN\n");
        kvi("h_field_count", kIterateFieldCount);
        kvi("h_rows_total", static_cast<long long>(result.history.size()));
        {
            const long long total = static_cast<long long>(result.history.size());
            const long long take = history_tail < total ? history_tail : total;
            const long long from = total - take;
            kvi("h_rows_printed", take);
            kvi("h_printed_from_index", total == 0 ? -1 : from);
            std::printf("index");
            for (int i = 0; i < kIterateFieldCount; ++i) {
                std::printf(",%s", kIterateFields[i]);
            }
            std::printf("\n");
            for (long long k = from; k < total; ++k) {
                print_history_row(k, result.history[static_cast<std::size_t>(k)]);
            }
        }
        std::printf("HISTORY_END\n");

        // ADDENDUM F -- THE QP LEDGER BLOCK (RULE-F.md sections 1.3, 4.2).
        //
        // It sits AFTER LADDER_END and before PROBE_END, in a block of its own,
        // so the PROBE_BEGIN key block gains exactly one key (`ledger`) and the
        // identity and neutrality checks of RULE-F.md section 3 compare the same
        // key set stage 1 and addendum E compared.
        //
        // THREE VIEWS OF THE SAME RECORDS, none of them a selection: the FIRST
        // record in full (the only one any verdict reads), the AGGREGATE over
        // every record, and the per-record table. All twenty QpCounters fields
        // appear in each.
        std::printf("LEDGER_BEGIN\n");
        kvi("ledger_attached", ledger ? 1 : 0);
        if (ledger == nullptr) {
            kvs("ledger_note", "no Ledger was constructed and attach_ledger was never called");
            std::printf("LEDGER_END\n");
        } else {
            const std::vector<hven::solvers::SolveRecord> &qp_records = ledger->records();
            const long long record_count = static_cast<long long>(qp_records.size());
            kvi("ledger_qp_records", record_count);
            // The DRIVER-level record, for completeness: one per public solve()
            // call. It carries SqpCounters, which the PROBE_BEGIN block already
            // prints from SqpResult, so only its count and status are taken here.
            kvi("ledger_sqp_records", static_cast<long long>(ledger->sqp_records().size()));
            if (!ledger->sqp_records().empty()) {
                kvs("ledger_sqp_status",
                    hven::solvers::to_string(ledger->sqp_records().front().status));
            }
            kvi("ledger_records_print_cap", kMaxLedgerRecordsPrinted);
            const long long printed =
                record_count < static_cast<long long>(kMaxLedgerRecordsPrinted)
                    ? record_count
                    : static_cast<long long>(kMaxLedgerRecordsPrinted);
            kvi("ledger_records_printed", printed);
            kvi("ledger_records_print_truncated", printed < record_count ? 1 : 0);

            // --- THE FIRST RECORD, in full. RULE-F.md section 4 takes its
            // verdict from this record of the tr_init = inf legs and from
            // nothing else. An absent record is SAID, not defaulted: a row of
            // zeroes would read as a measurement.
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
                // The three DERIVED quantities, marked derived by their names
                // and by RULE-F.md sections 1.4 and 4.1. `ws_net_change` is
                // `W - S` by the walk counters' documented net identity -- the
                // sizes themselves are not fields -- and carries that identity's
                // one documented start-repair exception.
                kvs("first_degenerate_fraction", degenerate_fraction(v));
                kvi("first_ws_adds_minus_drops", v[kIdxWsAdds] - v[kIdxWsDrops]);
                kvi("first_ws_net_change", v[kIdxWsAdds] - v[kIdxWsDrops] + v[kIdxShiftAdds]);
            }
            std::printf("LEDGER_FIRST_END\n");

            // --- THE AGGREGATE, over EVERY record, print cap or not. Sums,
            // with the two exceptions RULE-F.md section 4.2 fixes: k0_reused is
            // a bool and folds as a COUNT of records reading true,
            // degenerate_run_max is a PEAK and folds with max.
            std::printf("LEDGER_AGG_BEGIN\n");
            long long agg[kQpCounterFieldCount] = {0};
            for (const hven::solvers::SolveRecord &r : qp_records) {
                long long v[kQpCounterFieldCount];
                qp_counter_values(r.counters, v);
                for (int i = 0; i < kQpCounterFieldCount; ++i) {
                    if (i == kIdxDegenerateRunMax) {
                        agg[i] = v[i] > agg[i] ? v[i] : agg[i];
                    } else {
                        agg[i] += v[i];
                    }
                }
            }
            for (int i = 0; i < kQpCounterFieldCount; ++i) {
                std::printf("agg_%s=%lld\n", kQpCounterFields[i], agg[i]);
            }
            kvs("agg_k0_reused_meaning", "a COUNT of records reading true, not a sum of bools");
            kvs("agg_degenerate_run_max_meaning", "a PEAK over records, folded with max");
            kvs("agg_degenerate_fraction", degenerate_fraction(agg));
            std::printf("LEDGER_AGG_END\n");

            // --- THE PER-RECORD TABLE. One line per record up to the print
            // cap, in the driver's own emission order; the label carries the
            // driver's sequence number so the ordering is read rather than
            // inferred.
            std::printf("LEDGER_RECORDS_BEGIN\n");
            std::printf("index,label,warm,status");
            for (int i = 0; i < kQpCounterFieldCount; ++i) {
                std::printf(",%s", kQpCounterFields[i]);
            }
            std::printf(",ws_adds_minus_drops,ws_net_change,degenerate_fraction\n");
            for (long long k = 0; k < printed; ++k) {
                const hven::solvers::SolveRecord &r = qp_records[static_cast<std::size_t>(k)];
                long long v[kQpCounterFieldCount];
                qp_counter_values(r.counters, v);
                std::printf("%lld,%s,%d,%s", k, r.label.c_str(), r.warm ? 1 : 0,
                            qp_status_name(r.status));
                for (int i = 0; i < kQpCounterFieldCount; ++i) {
                    std::printf(",%lld", v[i]);
                }
                std::printf(",%lld,%lld,%s\n", v[kIdxWsAdds] - v[kIdxWsDrops],
                            v[kIdxWsAdds] - v[kIdxWsDrops] + v[kIdxShiftAdds],
                            degenerate_fraction(v).c_str());
            }
            std::printf("LEDGER_RECORDS_END\n");
            std::printf("LEDGER_END\n");
        }

        std::printf("PROBE_END\n");
        return 0;
    } catch (const std::exception &e) {
        std::printf("PROBE_BEGIN\nthrew=1\nexception=%s\nPROBE_END\n", e.what());
        return 4;
    }
}
