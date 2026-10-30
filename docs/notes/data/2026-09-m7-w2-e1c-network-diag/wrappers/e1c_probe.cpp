// M7 W2 M3c PASS 2 -- THE DEFAULT-OPTION PER-MINOR TRACE PROBE.
//
// ==========================================================================
// THIS IS A COPY OF M3b FIX ROUND 1's `fix1_probe.cpp`, WHICH IS NEVER EDITED.
// M3b's artifact is frozen; what this task needs from it is COPIED here
// (RULE.md section 2.1). Four things differ, and they are the whole of the
// adaptation:
//
//   1. `--capture-solve K` (default 0, `-1` = EVERY solve on the thread).
//      Pass 2's question is about solves the first-QP capture cannot reach --
//      the LAST capped main-engine QP, and the restoration sub-driver's own
//      solves, which the ledger never sees. `QpWalkMinorRecord::solve` already
//      carries each record's own solve index, so one `-1` run recovers every
//      solve's boundary, its minor count and its factorization count WITHOUT
//      ANY NEW OBSERVER FIELD. None was added; nothing under src/ or include/
//      is touched by this file or its CMake.
//
//   2. DEFAULT OPTIONS ARE STRUCTURAL. With `--tr-init` absent the probe does
//      not ASSIGN `options.tr_init` at all, so "default" is the field's own
//      default rather than a number this program re-types. `tr_init_touched`
//      is printed so a reader can check it. M3b's whole result is at
//      `tr_init = +inf`, a configuration hven does not ship; this task's whole
//      point is that it does not run there.
//
//   3. `--stop-after-first-qp` DEFAULTS TO OFF. Pass 2 runs the solve to its
//      own termination.
//
//   4. A PER-SOLVE SUMMARY block, printed to stdout and derived entirely from
//      the records already in hand: per solve, the minor count, the summed
//      `pass_factorizations` and `pass_schur_updates`, the incoming and final
//      working-set sizes, and the degenerate/ride/post-drop counts. This is
//      the observer-index <-> ledger-record MAPPING's own evidence (RULE.md
//      section 2.2): a main-engine solve is identified by matching its
//      (minors, factorizations) pair against a `Ledger` record's.
// ==========================================================================
//
// ==========================================================================
// THIS IS A COPY OF `m3b_probe.cpp` BESIDE IT, AND THAT FILE IS NEVER EDITED.
//
// M3b's probe produced the committed traces in `traces/` and the committed
// rows; a gate's evidence must stay reproducible from the program that made
// it, so the first probe stands as evidence and this is a second program.
// The whole of the difference is in `write_trace`: the same header keys plus
// `allocation_failed`, and the same per-minor columns plus the fields fix
// round 1 added to `include/hven/detail/qp/qp_walk_trace.h` -- the main
// inertia verdict and its triple against the expected one, the linear-algebra
// path and its refinement count, section 4c's curvature gate with `pᵀHp` and
// `pᵀp` taken apart, the GRANULAR ride outcome with the four numbers that
// chose it, the released constraint's own component of `p`, the drop rule's
// candidate set, and the effective regularization with the unregularized
// stationarity and active-row residuals.
//
// The PROBE_BEGIN and LEDGER_FIRST key blocks are UNCHANGED, key for key, so
// RULE-G.md section 2.1's identity gate compares this program's record 0
// against M3a's committed `rows/f/<cell>@F-trinf.txt` with no translation.
// Nothing under src/ or include/ is touched by this file or by its CMake.
// ==========================================================================
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

// ===========================================================================
// M7 W2 M4 -- THE ANTI-CYCLING ARM PROBE.
//
// This file is M7 W2 M3c's `wrappers/m3c_p2_probe.cpp`, COPIED (M3c's own copy
// is never edited) and extended by NEW ARGUMENTS AND NEW PRINTED KEYS ONLY.
// Not one line of its existing printing is removed, renamed or reordered, so a
// row of this probe is a SUPERSET of a row of M3c's -- which is what makes
// RULE.md section 5's OFF-control gate against M3c's committed pass-1 rows a
// comparison of the same fields rather than a comparison of two vocabularies.
//
// WHAT IS NEW, and nothing else is:
//   --arm off|E|L|L+   arms `QpAntiCycleArmState` on the solving thread, before
//                      solve(), and resets the whole state after it. `off` is
//                      the default and leaves the state default-constructed,
//                      which IS production behaviour (RULE.md section 2).
//   the eight EXPAND / lexicographic parameters, each defaulting to the
//   header's own default, so a leg that passes none of them runs RULE.md
//   section 2.1's table exactly.
//   the arm's own seven per-solve counters and every parameter's REALIZED
//   value, printed as new `arm_*` keys inside the PROBE block.
//
// NOTHING UNDER src/ OR include/ IS TOUCHED BY THIS TASK. The switch, the
// parameters and the counters are all reachable from outside those trees,
// because Task A put them in a header the seam already includes. If they had
// not been, RULE.md section 8 says the work STOPS and reports it; it does not
// reach in.
//
// THE WHOLE ARM SURFACE IS `#ifdef HVEN_TESTING`, so this file also compiles
// without the define -- in which case `--arm` accepts only `off` and REFUSES
// anything else, rather than silently accepting an arm it cannot arm.
// ===========================================================================

// ===========================================================================
// M7 W2 M5 -- THE DEGENERACY ARMS PROBE.
//
// This file is M7 W2 M4's `wrappers/m4_arm_probe.cpp`, COPIED (M4's own copy is
// NEVER edited: it produced M4's and M4b's committed rows, and a gate's
// evidence must stay reproducible from the program that made it) and extended
// by NEW ARGUMENTS AND NEW PRINTED KEYS ONLY. Every key M3c's pass-1 rows
// carry is printed here, in M3c's order, unrenamed and unreordered, so
// RULE.md section 8's OFF-control smoke against M3c's COMMITTED ACOPP57 row is
// a comparison of the same fields rather than of two vocabularies.
//
// WHAT IS NEW, and nothing else is:
//   --arm off|a|b1|a+b1   arms `QpDegeneracyArmState` on the solving thread
//                         before solve() and resets the whole state after it.
//                         `off` is the default and leaves the state
//                         default-constructed, which IS production behaviour.
//                         `a` is the scaled drop tolerance, `b1` the
//                         independence-preserving ratio test, `a+b1` both.
//   --dependency-tol t    ARM B's relative dependency tolerance.
//   --integer-screen on|off   ARM B's free integer pre-check.
//   --max-dep-tests N     the per-pass ceiling on dependency tests (0 =
//                         unbounded, which is the pre-registered rule).
//   --diag-inertia on|off the post-drop inertia diagnostic, independent of
//                         both arms and OFF by default because it costs one
//                         extra assemble+factorize per drop pass.
//   the arms' parameters as REALIZED and every `scaled_*` / `scale_*` /
//   `diag_*` / `indep_*` counter the arm keeps, printed as new keys inside the
//   PROBE block and HARVESTED BEFORE the disarm.
//
// M4's ANTI-CYCLING ARM IS NOT CARRIED, and this is the one thing this copy
// REMOVES rather than adds. There can be only one `--arm`, and M5's legs never
// arm M4's switch: RULE.md's legs are `off`, `a`, `b1` and `a+b1` and no other.
// Carrying M4's eight EXPAND/lexicographic parameters and its nine `arm_*`
// counters would print a default-constructed description of an arm this
// program never arms -- a surface no leg exercises and no gate covers. The
// keys consequently absent, relative to M4's probe, are exactly:
// `arm_delta_master`, `arm_delta_0_factor`, `arm_delta_max_factor`,
// `arm_expand_k`, `arm_negligible_pivot` (M5 prints its own, from ITS params),
// `arm_reset_nontrivial`, `arm_lex_full_pricing`, `arm_tau`, `arm_delta_0`,
// `arm_delta_max`, `arm_min_steps`, `arm_pivot_switches`, `arm_unpinned_adds`,
// `arm_lex_drop_switches`, `arm_lex_ratio_switches`, `arm_resets`,
// `arm_reset_nontrivial_moves`, `arm_delta_at_exit`, `arm_expand_iter_at_exit`.
// None of them is in M3c's pass-1 rows, so the OFF smoke is untouched by their
// absence; the identity gate's pinned manifest contains none of them either.
//
// `a+b1` CARRIES A `+`. Every script that greps for an arm label in this
// program's output greps it as a FIXED STRING (`grep -F`), for the reason M4's
// chain records about `L+`: an ERE would read the `+` as a repetition operator
// and the arm whose whole purpose is to be distinguished from `b1` would match
// nothing.
//
// NOTHING UNDER src/ OR include/ IS TOUCHED BY THIS FILE OR BY ITS BUILD. The
// switch, the parameters and the counters -- INCLUDING amendment 1's
// `indep_max_cond_r` -- are all reachable from outside those trees because M5
// Task A put them in a header the seam already includes. If they had not been,
// RULE.md section 8 says the work STOPS and reports it; it does not reach in.
//
// THE WHOLE ARM SURFACE IS `#ifdef HVEN_TESTING`, so this file also compiles
// without the define -- in which case `--arm` accepts only `off` and REFUSES
// anything else, rather than silently accepting an arm it cannot arm.
//
// CLAUDE.md section 7: wall-clock is informational; counters are the currency.
// HOLDOUT is never run. /home/ghecht/Projects/hven-bench is READ-ONLY.
// ===========================================================================

// ===========================================================================
// M7 W2 E1c -- THE NETWORK-CELL DIAGNOSTIC PROBE (equality basis ON vs OFF).
//
// This file is M7 W2 M5's `wrappers/m5_arm_probe.cpp`, COPIED (M5's own copy
// is NEVER edited: it produced M5's committed rows) and extended by NEW
// ARGUMENTS AND NEW PRINTED KEYS ONLY. Not one existing key is removed,
// renamed or reordered. M5's `--arm` surface stays compiled in; this task runs
// `--arm off` ONLY (the retired arms are not used; brief
// `.superpowers/m7-w2-e1c-diag-brief.md`).
//
// WHAT IS NEW, and nothing else is:
//   --equality-basis on|off   `SqpOptions::equality_basis`. ABSENT, the field
//                             is NEVER ASSIGNED, so the leg runs hven's own
//                             default (ON at 43800aae) by construction;
//                             `equality_basis_touched` says which.
//   `opt_equality_basis`      the option AS REALIZED, read back off the
//                             options object handed to the solver.
//   `eqb_*`                   every field of `SqpResult::equality_basis`
//                             (`SqpEqualityBasisReport`), lists spelled
//                             `;`-separated, plus `eqb_dropped_count` and
//                             `eqb_me_reduced` (= me - dropped at exit).
//   `eqb_ce_*`                the RETURNED point's equality residual split by
//                             the exit's dropped/retained partition, read off
//                             `SolveResult::ce` (declared space, caller units;
//                             scaling is off by default) -- Q3's measurement.
//   `c_*`                     every SqpCounters field the harness's
//                             counters-hven-sqp.csv carries, under the names
//                             M3c's identity gate maps, so the smoke compares
//                             the WHOLE harness counter row.
//   E1C_HISTORY block         one CSV row per `SqpResult::history` entry.
//   E1C_EVENTS block          one CSV row per iteration-callback event,
//                             INCLUDING `depth` (0 caller, 1 restoration
//                             sub-solve) -- recorded by the callback that was
//                             already installed; its return value is
//                             unchanged, so the solve is unchanged.
//
// NOTHING UNDER src/ OR include/ IS TOUCHED. Every value read here is on the
// public result surface. HOLDOUT is never run; hven-bench is READ-ONLY.
// ===========================================================================

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
#include <hven/detail/qp/qp_degeneracy_arm.h>
#include <hven/detail/qp/qp_walk_trace.h>
#include <hven/drivers/solve_result.h>
#include <hven/drivers/sqp_solver.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/model/nlp_problem_model.h>

#include "lanes/cutest/cutest_source.h"
#include "lanes/hven/problem_source.h"

#ifndef HVEN_TESTING
#error "fix1_probe must be compiled with HVEN_TESTING: without it the walk observer is not there"
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

// ---- FIX ROUND 1's names, one per new enum -------------------------------
// Printed by NAME for the same reason `action_name` is: a rule that has to
// decode an integer is a rule a reader cannot check by eye. Every enumerator
// is listed, so a value the header adds later reads as `UNRECOGNIZED` rather
// than silently taking a neighbour's name.

const char *eqp_path_name(seam::QpWalkEqpPath p) {
    switch (p) {
    case seam::QpWalkEqpPath::kNone:
        return "none";
    case seam::QpWalkEqpPath::kEmptySystem:
        return "empty_system";
    case seam::QpWalkEqpPath::kEliminated:
        return "eliminated";
    case seam::QpWalkEqpPath::kBorderSchur:
        return "border_schur";
    }
    return "UNRECOGNIZED";
}

const char *inertia_state_name(seam::QpWalkInertiaState s) {
    switch (s) {
    case seam::QpWalkInertiaState::kAbsent:
        return "absent";
    case seam::QpWalkInertiaState::kObserved:
        return "observed";
    case seam::QpWalkInertiaState::kQueryFailed:
        return "query_failed";
    case seam::QpWalkInertiaState::kUnavailable:
        return "unavailable";
    }
    return "UNRECOGNIZED";
}

const char *ride_outcome_name(seam::QpWalkRideOutcome o) {
    switch (o) {
    case seam::QpWalkRideOutcome::kNotAttempted:
        return "not_attempted";
    case seam::QpWalkRideOutcome::kNullspaceVeto:
        return "nullspace_veto";
    case seam::QpWalkRideOutcome::kSlopeFlatVeto:
        return "slope_flat_veto";
    case seam::QpWalkRideOutcome::kSignDisagreementVeto:
        return "sign_disagreement_veto";
    case seam::QpWalkRideOutcome::kUnbounded:
        return "unbounded";
    case seam::QpWalkRideOutcome::kStepped:
        return "stepped";
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

// M3c PASS 2 -- THE PER-SOLVE SUMMARY, and the mapping's own evidence.
//
// Derived ENTIRELY from `QpWalkObserver::records`, every field of which already
// exists: `solve` tags each record with the QpEngine::run that produced it, and
// `pass_factorizations` / `pass_schur_updates` are per-pass counts. No observer
// field was added for this and none is needed.
//
// WHAT IT IS FOR. The ledger records the MAIN WALK ENGINE only; the observer
// counts every run on the thread. A main-engine solve is identified here by its
// (minors, factorizations) pair, which the ledger publishes for each of its own
// records -- so the mapping is established by MATCHING TWO NUMBERS, not by
// assuming an index correspondence.
void write_solve_summary(long long capture_solve_arg) {
    std::printf("SOLVE_SUMMARY_BEGIN\n");
    std::printf("summary_capture_solve=%lld\n", capture_solve_arg);
    std::printf("summary_solves_seen=%lld\n",
                static_cast<long long>(seam::QpWalkObserver::solves));
    std::printf("summary_records=%llu\n",
                static_cast<unsigned long long>(seam::QpWalkObserver::records.size()));
    std::printf("summary_truncated=%d\n", seam::QpWalkObserver::truncated ? 1 : 0);
    std::printf("summary_allocation_failed=%d\n",
                seam::QpWalkObserver::allocation_failed ? 1 : 0);
    std::printf("solve,minors,minor_first,minor_last,minor_contiguous,factorizations,"
                "schur_updates,eqp_refine,border_refine,verdict_refine,ws_size_in,"
                "ws_size_out,degenerate,rides,post_drop,drops,adds\n");
    const auto &recs = seam::QpWalkObserver::records;
    std::size_t i = 0;
    while (i < recs.size()) {
        const hven::Index sid = recs[i].solve;
        std::size_t j = i;
        long long minors = 0, facts = 0, schur = 0, eqp = 0, bord = 0, verd = 0;
        long long degen = 0, rides = 0, post = 0, drops = 0, adds = 0;
        long long first_minor = static_cast<long long>(recs[i].minor);
        long long last_minor = first_minor;
        bool contiguous = true;
        long long expect = first_minor;
        while (j < recs.size() && recs[j].solve == sid) {
            const auto &r = recs[j];
            ++minors;
            if (static_cast<long long>(r.minor) != expect) {
                contiguous = false;
            }
            ++expect;
            last_minor = static_cast<long long>(r.minor);
            facts += static_cast<long long>(r.pass_factorizations);
            schur += static_cast<long long>(r.pass_schur_updates);
            eqp += static_cast<long long>(r.pass_eqp_refine_steps);
            bord += static_cast<long long>(r.pass_border_refine_steps);
            verd += static_cast<long long>(r.pass_verdict_refine_steps);
            degen += r.degenerate ? 1 : 0;
            rides += r.ride ? 1 : 0;
            post += r.post_drop ? 1 : 0;
            // The two DROP actions the walk has -- the Dantzig release and
            // section 4b's zero-multiplier probe -- counted together, and the
            // ADD. The other seven actions are neither and are not folded in.
            if (r.action == seam::QpWalkAction::kDropDantzig ||
                r.action == seam::QpWalkAction::kDropZeroProbe) {
                ++drops;
            }
            if (r.action == seam::QpWalkAction::kAdd) {
                ++adds;
            }
            ++j;
        }
        std::printf("%lld,%lld,%lld,%lld,%d,%lld,%lld,%lld,%lld,%lld,%lld,%lld,"
                    "%lld,%lld,%lld,%lld,%lld\n",
                    static_cast<long long>(sid), minors, first_minor, last_minor,
                    contiguous ? 1 : 0, facts, schur, eqp, bord, verd,
                    static_cast<long long>(recs[i].ws_size_in),
                    static_cast<long long>(recs[j - 1].ws_size_out),
                    degen, rides, post, drops, adds);
        i = j;
    }
    std::printf("SOLVE_SUMMARY_END\n");
}

int write_trace(const std::string &path, const std::string &cell, double tr_init) {
    std::FILE *f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        std::fprintf(stderr, "m5_arm_probe: cannot open --trace-out %s\n", path.c_str());
        return 1;
    }
    // THE HEADER IS PART OF THE EVIDENCE. Everything a reader needs to know
    // which solve this is, before a single row.
    std::fprintf(f, "# M7 W2 M3b FIX ROUND 1 first-QP per-minor trace (discriminating fields)\n");
    std::fprintf(f, "# cell=%s\n", cell.c_str());
    std::fprintf(f, "# tr_init=%.17g\n", tr_init);
    std::fprintf(f, "# solve_index=%lld\n", static_cast<long long>(seam::QpWalkObserver::solve_index));
    std::fprintf(f, "# solves_seen=%lld\n", static_cast<long long>(seam::QpWalkObserver::solves));
    std::fprintf(f, "# m3c_pass2=1\n");
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
    // FIX ROUND 1. A capture with this set is INCOMPLETE -- a record could not
    // be stored because its allocation threw, and the destructor swallowed the
    // throw rather than terminating the solve. RULE-G.md section 2.2 aborts on it,
    // so it must be in the header a rule reads and not only in the process that
    // wrote the file.
    std::fprintf(f, "# allocation_failed=%d\n",
                 seam::QpWalkObserver::allocation_failed ? 1 : 0);
    std::fprintf(f, "# max_records=%llu\n",
                 static_cast<unsigned long long>(seam::QpWalkObserver::max_records));
    std::fprintf(f, "# records=%llu\n",
                 static_cast<unsigned long long>(seam::QpWalkObserver::records.size()));
    std::fprintf(f, "solve,minor,action,block,block_index,block_side,drop_route,bound_gap,"
                    "alpha,p_inf,dx_inf,step_tol,degenerate,ride,raw_ratio,ratio_tied,"
                    "ratio_capped,decision_multiplier,decision_tol,best_multiplier,"
                    "drop_candidates,drop_tied,shift_adds,ws_size_in,ws_sig_in,ws_size_out,"
                    "ws_sig_out,x_inf,x_l1,"
                    // ---- FIX ROUND 1's discriminating columns ----------------
                    // Appended, never interleaved: every column above keeps its
                    // position, so RULE-G.md section 3's tail definition over the
                    // M3b field set reads the same bytes it always did.
                    "inertia_verdict,inertia_state,inertia_pos,inertia_neg,inertia_zero,"
                    "inertia_zero_derived,inertia_perturbed,inertia_expected_pos,"
                    "inertia_expected_neg,"
                    "eqp_path,border_attempted,k0_rebuilt,border_latched,"
                    "border_needs_refactorization,border_dim,eqp_call_refine_steps,"
                    "pass_factorizations,pass_eqp_refine_steps,pass_border_refine_steps,"
                    "pass_verdict_refine_steps,pass_schur_updates,"
                    "post_drop,curvature_fired,curvature,curvature_tol,p_hp,p_sq,"
                    "ride_outcome,ride_sign_chosen,ride_slope,ride_slope_tol,ride_leaves,"
                    "ride_leaves_tol,"
                    "released_block,released_index,released_side,p_released,"
                    "released_directional,"
                    "candidates_recorded,candidates,"
                    "primal_delta,dual_mu,stat_free_inf,stat_scale,active_res_inf\n");
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

        // ---- FIX ROUND 1's discriminating fields, in header order ----------
        // Every `-1` below is the observer's declared ABSENT value for an index
        // and is written as `-1`, never as 0: `inertia_zero = -1` means the
        // backend reported nothing, `inertia_zero = 0` means it reported none.
        std::fprintf(f, ",%d,%s,%lld,%lld,%lld,%d,%lld,%lld,%lld", r.inertia_verdict,
                     inertia_state_name(r.inertia_state), static_cast<long long>(r.inertia_pos),
                     static_cast<long long>(r.inertia_neg),
                     static_cast<long long>(r.inertia_zero), r.inertia_zero_derived ? 1 : 0,
                     static_cast<long long>(r.inertia_perturbed),
                     static_cast<long long>(r.inertia_expected_pos),
                     static_cast<long long>(r.inertia_expected_neg));

        std::fprintf(f, ",%s,%d,%d,%d,%d,%lld,%lld", eqp_path_name(r.eqp_path),
                     r.border_attempted ? 1 : 0, r.k0_rebuilt ? 1 : 0, r.border_latched ? 1 : 0,
                     r.border_needs_refactorization ? 1 : 0,
                     static_cast<long long>(r.border_dim),
                     static_cast<long long>(r.eqp_call_refine_steps));

        std::fprintf(f, ",%lld,%lld,%lld,%lld,%lld",
                     static_cast<long long>(r.pass_factorizations),
                     static_cast<long long>(r.pass_eqp_refine_steps),
                     static_cast<long long>(r.pass_border_refine_steps),
                     static_cast<long long>(r.pass_verdict_refine_steps),
                     static_cast<long long>(r.pass_schur_updates));

        std::fprintf(f, ",%d,%d,", r.post_drop ? 1 : 0, r.curvature_fired ? 1 : 0);
        put_double(f, r.curvature);
        std::fprintf(f, ",");
        put_double(f, r.curvature_tol);
        std::fprintf(f, ",");
        put_double(f, r.p_hp);
        std::fprintf(f, ",");
        put_double(f, r.p_sq);

        std::fprintf(f, ",%s,%d,", ride_outcome_name(r.ride_outcome), r.ride_sign_chosen);
        put_double(f, r.ride_slope);
        std::fprintf(f, ",");
        put_double(f, r.ride_slope_tol);
        std::fprintf(f, ",");
        put_double(f, r.ride_leaves);
        std::fprintf(f, ",");
        put_double(f, r.ride_leaves_tol);

        std::fprintf(f, ",%s,%lld,%s,", block_name(r.released_block),
                     static_cast<long long>(r.released_index), side_name(r.released_side));
        put_double(f, r.p_released);
        std::fprintf(f, ",");
        put_double(f, r.released_directional);

        // THE CANDIDATE SET, one column. Entries are `;`-separated and their
        // four parts `:`-separated -- no comma anywhere, so the column stays a
        // plain CSV field and needs no quoting. `candidates_recorded` beside it
        // is how many of `drop_candidates` are actually here, so a reader can
        // never mistake the recorded set for the whole one.
        std::fprintf(f, ",%lld,", static_cast<long long>(r.candidates_recorded));
        const std::size_t n_cand =
            r.candidates_recorded < 0
                ? std::size_t{0}
                : std::min(static_cast<std::size_t>(r.candidates_recorded),
                           seam::QpWalkMinorRecord::kMaxRecordedCandidates);
        for (std::size_t c = 0; c < n_cand; ++c) {
            const seam::QpWalkDropCandidate &cand = r.candidates[c];
            if (c > 0) {
                std::fprintf(f, ";");
            }
            std::fprintf(f, "%s:%lld:%s:", block_name(cand.block),
                         static_cast<long long>(cand.index), side_name(cand.side));
            put_double(f, cand.violation);
        }

        std::fprintf(f, ",");
        put_double(f, r.primal_delta);
        std::fprintf(f, ",");
        put_double(f, r.dual_mu);
        std::fprintf(f, ",");
        put_double(f, r.stat_free_inf);
        std::fprintf(f, ",");
        put_double(f, r.stat_scale);
        std::fprintf(f, ",");
        put_double(f, r.active_res_inf);
        std::fprintf(f, "\n");
    }
    const int rc = std::fclose(f);
    if (rc != 0) {
        std::fprintf(stderr, "m5_arm_probe: writing %s failed on close\n", path.c_str());
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
    std::size_t max_records_arg = 1000000;
    // `off` is the CONTROL ARM: the identical solve with the observer never
    // armed. RULE.md section 2 runs it first, so "the rebuilt binary reproduces
    // M3a's row" is established BEFORE the observer is switched on, and
    // "the observer changed nothing" is then a second, independent comparison.
    bool trace_on = false;
    // Stop the driver as soon as the FIRST QP is finished. Independent of the
    // observer: it is armed off `QpWalkObserver`'s own completion when the
    // observer is on, and off the iteration index when it is off.
    bool stop_after_first_qp = false;   // M3c PASS 2: run the solve to its own end
    // The ledger is how record 0 is read, exactly as in M3a's probe.
    bool attach_ledger_flag = true;
    // M3c PASS 2. Which QpEngine::run on this thread to record. 0 is M3b's own
    // value; -1 records EVERY one, which is how the per-solve mapping is built.
    long long capture_solve_arg = 0;
    // M3c PASS 2. True iff --tr-init was given. At false the option field is
    // NEVER ASSIGNED, so the leg runs at hven's own default by construction.
    bool tr_init_touched = false;

    // ---- M7 W2 M5: the arms and their parameters --------------------------
    // THE DEFAULTS ARE THE HEADER'S OWN, read out of a DEFAULT-CONSTRUCTED
    // `QpDegeneracyParams` rather than re-typed here: a second copy of a
    // default is a second thing to go stale, and RULE.md's parameter table is
    // checked against what this probe PRINTS, not against what it was told.
    // The `#else` struct exists only so the file compiles WITHOUT
    // `HVEN_TESTING`; its literals are never used to run an arm, because a
    // binary built that way REFUSES every `--arm` but `off`.
    std::string arm_arg = "off";
    bool diag_inertia = false;
    // ---- M7 W2 E1c: the equality basis switch (structural default) -------
    bool equality_basis_touched = false;
    bool equality_basis_on = true;
#ifdef HVEN_TESTING
    seam::QpDegeneracyParams arm_params{};
#else
    struct { double dependency_tol = 1e-8; bool integer_screen = true;
             double negligible_pivot = 1e-12; double inf_bound = 1e20;
             double step_tie_tol = 1e-9;
             long long max_dep_tests_per_pass = 0; } arm_params;
#endif

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--problem" && i + 1 < argc) {
            problem_name = argv[++i];
        } else if (arg == "--capture-solve" && i + 1 < argc) {
            capture_solve_arg = std::atoll(argv[++i]);
            if (capture_solve_arg < -1) {
                std::fprintf(stderr, "m5_arm_probe: --capture-solve takes -1 or a "
                                     "non-negative index\n");
                return 2;
            }
        } else if (arg == "--max-records" && i + 1 < argc) {
            const long long v = std::atoll(argv[++i]);
            if (v <= 0) {
                std::fprintf(stderr, "m5_arm_probe: --max-records takes a value > 0\n");
                return 2;
            }
            max_records_arg = static_cast<std::size_t>(v);
        } else if (arg == "--tr-init" && i + 1 < argc) {
            tr_init_touched = true;
            tr_init = std::atof(argv[++i]);
            if (!(tr_init > 0.0) || std::isnan(tr_init)) {
                std::fprintf(stderr, "m5_arm_probe: --tr-init takes a value > 0 (+inf allowed)\n");
                return 2;
            }
        } else if (arg == "--deadline" && i + 1 < argc) {
            deadline_seconds = std::atof(argv[++i]);
            if (!(deadline_seconds > 0.0) || !std::isfinite(deadline_seconds)) {
                std::fprintf(stderr, "m5_arm_probe: --deadline takes a finite value > 0\n");
                return 2;
            }
        } else if (arg == "--trace" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                trace_on = true;
            } else if (value != "off") {
                std::fprintf(stderr, "m5_arm_probe: --trace takes on|off\n");
                return 2;
            }
        } else if (arg == "--trace-out" && i + 1 < argc) {
            trace_out = argv[++i];
        } else if (arg == "--stop-after-first-qp" && i + 1 < argc) {
            const std::string value = argv[++i];
            // BOTH ARMS ASSIGN. M3b's copy only ever assigned `false`, because
            // its own default was `true` and `on` was therefore a no-op that
            // happened to be correct. This copy's default is `false`, so an
            // `on` that assigned nothing would have SILENTLY IGNORED the flag --
            // and it did, until the prepare script's refusal fold caught it.
            if (value == "off") {
                stop_after_first_qp = false;
            } else if (value == "on") {
                stop_after_first_qp = true;
            } else if (true) {
                std::fprintf(stderr, "m5_arm_probe: --stop-after-first-qp takes on|off\n");
                return 2;
            }
        } else if (arg == "--arm" && i + 1 < argc) {
            arm_arg = argv[++i];
            if (arm_arg != "off" && arm_arg != "a" && arm_arg != "b1" &&
                arm_arg != "a+b1") {
                std::fprintf(stderr, "m5_arm_probe: --arm takes off|a|b1|a+b1\n");
                return 2;
            }
#ifndef HVEN_TESTING
            if (arm_arg != "off") {
                std::fprintf(stderr, "m5_arm_probe: this binary was built WITHOUT "
                                     "HVEN_TESTING; --arm %s cannot be armed and is "
                                     "REFUSED rather than ignored\n", arm_arg.c_str());
                return 2;
            }
#endif
        } else if (arg == "--dependency-tol" && i + 1 < argc) {
            arm_params.dependency_tol = std::atof(argv[++i]);
            if (!(arm_params.dependency_tol > 0.0) ||
                !std::isfinite(arm_params.dependency_tol)) {
                std::fprintf(stderr, "m5_arm_probe: --dependency-tol takes a finite value > 0\n");
                return 2;
            }
        } else if (arg == "--integer-screen" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                arm_params.integer_screen = true;
            } else if (value == "off") {
                arm_params.integer_screen = false;
            } else {
                std::fprintf(stderr, "m5_arm_probe: --integer-screen takes on|off\n");
                return 2;
            }
        } else if (arg == "--max-dep-tests" && i + 1 < argc) {
            const long long v = std::atoll(argv[++i]);
            if (v < 0) {
                std::fprintf(stderr, "m5_arm_probe: --max-dep-tests takes a value >= 0 "
                                     "(0 is UNBOUNDED, the pre-registered rule)\n");
                return 2;
            }
            arm_params.max_dep_tests_per_pass =
                static_cast<decltype(arm_params.max_dep_tests_per_pass)>(v);
        } else if (arg == "--diag-inertia" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "on") {
                diag_inertia = true;
            } else if (value == "off") {
                diag_inertia = false;
            } else {
                std::fprintf(stderr, "m5_arm_probe: --diag-inertia takes on|off\n");
                return 2;
            }
        } else if (arg == "--equality-basis" && i + 1 < argc) {
            // M7 W2 E1c. BOTH VALUES ASSIGN, and an unknown value REFUSES (the
            // M3c lesson: an `on` that assigned nothing ignored the flag).
            const std::string value = argv[++i];
            if (value == "on") {
                equality_basis_touched = true;
                equality_basis_on = true;
            } else if (value == "off") {
                equality_basis_touched = true;
                equality_basis_on = false;
            } else {
                std::fprintf(stderr, "e1c_probe: --equality-basis takes on|off\n");
                return 2;
            }
        } else if (arg == "--ledger" && i + 1 < argc) {
            const std::string value = argv[++i];
            if (value == "off") {
                attach_ledger_flag = false;
            } else if (value != "on") {
                std::fprintf(stderr, "m5_arm_probe: --ledger takes on|off\n");
                return 2;
            }
        } else {
            std::fprintf(stderr, "m5_arm_probe: unknown argument %s\n", arg.c_str());
            return 2;
        }
    }
    if (problem_name.empty()) {
        std::fprintf(stderr, "usage: m5_arm_probe --problem NAME [--tr-init r] [--deadline s] "
                             "[--capture-solve K|-1] [--max-records N] "
                             "[--trace on|off] [--trace-out PATH] "
                             "[--stop-after-first-qp on|off] [--ledger on|off] "
                             "[--arm off|a|b1|a+b1] [--dependency-tol t] "
                             "[--integer-screen on|off] [--max-dep-tests N] "
                             "[--diag-inertia on|off] [--equality-basis on|off]\n");
        return 2;
    }
    if (stop_after_first_qp && trace_on && capture_solve_arg != 0) {
        std::fprintf(stderr, "m5_arm_probe: --stop-after-first-qp on needs "
                             "--capture-solve 0 (its stop condition is solve 0's own "
                             "completion)\n");
        return 2;
    }
    if (trace_on && trace_out.empty()) {
        std::fprintf(stderr, "m5_arm_probe: --trace on requires --trace-out PATH\n");
        return 2;
    }
    // M5's FOUR ARM LABELS ARE INDEPENDENT OF ITS PARAMETERS, so there is no
    // label/switch contradiction to refuse here as M4 refused `--arm L+` with
    // full pricing off: `a`, `b1` and `a+b1` name WHICH ARMS are on, and the
    // four parameters mean the same thing under each. A parameter passed with
    // `--arm off` is still REALIZED into the printed keys, so a reader can see
    // it was given and had no arm to act on.
    //
    // ONE REFUSAL IS ADDED, and it is the diagnostic's: the post-drop inertia
    // costs one extra assemble+factorize per drop pass (RULE.md section 6), so
    // a leg that asks for it with NO arm armed is asking this probe to change
    // what the unarmed run does. That is exactly what the OFF controls exist to
    // rule out, and it is refused rather than allowed and footnoted.
    if (diag_inertia && arm_arg == "off") {
        std::fprintf(stderr, "m5_arm_probe: --diag-inertia on with --arm off would add a "
                             "factorization per drop pass to an UNARMED run, which is "
                             "what the OFF controls exist to rule out\n");
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
        std::fprintf(stderr, "m5_arm_probe: %s is not in the cutest-nlp-35 roster\n",
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
        // STRUCTURAL DEFAULT: assigned only when the caller named a radius.
        if (tr_init_touched) {
            options.tr_init = tr_init;
        }
        // M7 W2 E1c: STRUCTURAL DEFAULT, exactly as tr_init above.
        if (equality_basis_touched) {
            options.equality_basis = equality_basis_on;
        }
        const bool realized_equality_basis = options.equality_basis;

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
        seam::QpWalkObserver::max_records = max_records_arg;
        if (trace_on) {
            seam::QpWalkObserver::active = true;
            seam::QpWalkObserver::capture_solve =
                static_cast<hven::Index>(capture_solve_arg);
        }

        // ARM THE DEGENERACY SWITCH, in the same place and by the same
        // conventions as the observer's arming above: a thread_local state set
        // before solve() and cleared after it, with no accessor on the driver
        // and no line under src/ or include/ added by this task.
        //
        // `reset()` FIRST, THEN the parameters, THEN the arm. `reset()` puts
        // the switches AND the parameters back to their defaults, so assigning
        // the parameters before it would throw them away; assigning the arm
        // before the parameters would leave a pass that ran in between judging
        // against the wrong tolerance. The order is the one M4's probe used,
        // for the same reason.
#ifdef HVEN_TESTING
        seam::QpDegeneracyArmState::reset();
        seam::QpDegeneracyArmState::params = arm_params;
        seam::QpDegeneracyArmState::diag_post_drop_inertia = diag_inertia;
        if (arm_arg == "a") {
            seam::QpDegeneracyArmState::arm = seam::QpDegeneracyArm::kScaledDrop;
        } else if (arm_arg == "b1") {
            seam::QpDegeneracyArmState::arm = seam::QpDegeneracyArm::kIndependentAdd;
        } else if (arm_arg == "a+b1") {
            seam::QpDegeneracyArmState::arm = seam::QpDegeneracyArm::kBoth;
        }
#endif

        bool deadline_fired = false;
        bool stopped_after_first_qp = false;
        long long callback_events = 0;
        // M7 W2 E1c: every event's scalars, copied (the views are borrowed).
        struct E1cEvent {
            long long iteration; long long depth; double f, stationarity,
                feasibility_e, feasibility_i, complementarity, step_norm, radius;
        };
        std::vector<E1cEvent> e1c_events;
        solver.set_iteration_callback([&](const hven::solvers::IterationEvent &ev) {
            ++callback_events;
            e1c_events.push_back(E1cEvent{
                static_cast<long long>(ev.iteration),
                ev.depth ? static_cast<long long>(*ev.depth) : -1LL, ev.f,
                ev.stationarity, ev.feasibility_e, ev.feasibility_i,
                ev.complementarity, ev.step_norm,
                ev.radius ? *ev.radius : std::numeric_limits<double>::quiet_NaN()});
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

        // HARVEST THE ARMS' COUNTERS BEFORE DISARMING. `reset()` zeroes every
        // one of them, so a harvest taken after the disarm would print a wall
        // of zeroes on every leg and the whole arm read-out would be a silent
        // null. EVERY `scaled_*`, `scale_*`, `diag_*` and `indep_*` member the
        // state keeps is taken here, including amendment 1's
        // `indep_max_cond_r` -- a counter the read-out needs but the probe did
        // not harvest would be indistinguishable from a counter the arm never
        // moved.
        long long a_drops_made = 0, a_cand_refused = 0, a_drops_prevented = 0;
        long long a_scale_unavail = 0;
        double a_max_refused_ratio = 0.0;
        long long d_factorizations = 0, d_factorization_failures = 0;
        long long b_passes = 0, b_cand_examined = 0, b_cand_passed_over = 0;
        long long b_dep_tests = 0, b_qr_builds = 0, b_test_failures = 0;
        long long b_entry_removed = 0, b_screen_and_qr = 0, b_qr_only = 0;
        long long b_blocker_switched = 0, b_no_blocker_passes = 0;
        long long b_budget_truncations = 0, b_enum_mismatches = 0;
        long long b_rows_minus_n_max = 0, b_rank_deficit_max = 0;
        long long b_rank_violations = 0, b_refused_mandatory = 0;
        long long b_disarmed = 0, b_disarm_site = -1;
        long long b_ride_refusals = 0, b_shift_refusals = 0;
        long long b_repair_pin_refusals = 0, b_probe_restore_dependent = 0;
        long long b_leak_bound_exceeded = 0, b_solve_generation = 0;
        double b_max_cond_r = std::numeric_limits<double>::quiet_NaN();
        double b_max_passed_violation = 0.0;
        bool b_rows_minus_n_seen = false;
#ifdef HVEN_TESTING
        {
            using S = seam::QpDegeneracyArmState;
            a_drops_made = static_cast<long long>(S::scaled_drops_made);
            a_cand_refused = static_cast<long long>(S::scaled_candidates_refused);
            a_drops_prevented = static_cast<long long>(S::scaled_drops_prevented);
            a_max_refused_ratio = S::scaled_max_refused_ratio;
            a_scale_unavail = static_cast<long long>(S::scale_unavailable_passes);
            d_factorizations = static_cast<long long>(S::diag_factorizations);
            d_factorization_failures =
                static_cast<long long>(S::diag_factorization_failures);
            b_passes = static_cast<long long>(S::indep_passes);
            b_cand_examined = static_cast<long long>(S::indep_candidates_examined);
            b_cand_passed_over = static_cast<long long>(S::indep_candidates_passed_over);
            b_dep_tests = static_cast<long long>(S::indep_dep_tests);
            b_qr_builds = static_cast<long long>(S::indep_qr_builds);
            b_test_failures = static_cast<long long>(S::indep_test_failures);
            b_entry_removed = static_cast<long long>(S::indep_entry_members_removed);
            b_screen_and_qr = static_cast<long long>(S::indep_screen_and_qr_dependent);
            b_qr_only = static_cast<long long>(S::indep_qr_only_dependent);
            b_blocker_switched = static_cast<long long>(S::indep_blocker_switched);
            b_no_blocker_passes = static_cast<long long>(S::indep_no_blocker_passes);
            b_budget_truncations = static_cast<long long>(S::indep_budget_truncations);
            b_enum_mismatches = static_cast<long long>(S::indep_enumeration_mismatches);
            // `indep_rows_minus_n_max` starts at Index::min() as a SENTINEL for
            // "no pass published one". It is printed as ABSENT rather than as
            // that integer, which a reader would otherwise have to decode.
            b_rows_minus_n_seen =
                S::indep_rows_minus_n_max != std::numeric_limits<hven::Index>::min();
            b_rows_minus_n_max = static_cast<long long>(S::indep_rows_minus_n_max);
            b_rank_deficit_max = static_cast<long long>(S::indep_rank_deficit_max);
            b_max_cond_r = S::indep_max_cond_r;
            b_rank_violations = static_cast<long long>(S::indep_rank_violations);
            b_refused_mandatory =
                static_cast<long long>(S::indep_refused_mandatory_dependent);
            b_disarmed = S::indep_disarmed ? 1 : 0;
            b_disarm_site = static_cast<long long>(S::indep_disarm_site);
            b_ride_refusals = static_cast<long long>(S::indep_ride_refusals);
            b_shift_refusals = static_cast<long long>(S::indep_shift_refusals);
            b_repair_pin_refusals =
                static_cast<long long>(S::indep_repair_pin_refusals);
            b_probe_restore_dependent =
                static_cast<long long>(S::indep_probe_restore_dependent);
            b_leak_bound_exceeded =
                static_cast<long long>(S::indep_leak_bound_exceeded);
            b_max_passed_violation = S::indep_max_passed_violation;
            b_solve_generation = static_cast<long long>(S::indep_solve_generation);
        }
#endif

        // Disarm before anything else touches a QpEngine.
        seam::QpWalkObserver::active = false;
#ifdef HVEN_TESTING
        seam::QpDegeneracyArmState::reset();
#endif

        std::printf("PROBE_BEGIN\n");
        kvs("problem", problem_name);
        kv("tr_init", tr_init);
        kvs("trace", trace_on ? "on" : "off");
        kvs("stop_after_first_qp", stop_after_first_qp ? "on" : "off");
        kvi("stopped_after_first_qp", stopped_after_first_qp ? 1 : 0);
        kvi("callback_events", callback_events);
        kvs("ledger", attach_ledger_flag ? "on" : "off");
        kvi("tr_init_touched", tr_init_touched ? 1 : 0);
        kvi("capture_solve_arg", capture_solve_arg);
        kvi("observer_max_records", static_cast<long long>(max_records_arg));
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

        // ---- M7 W2 M5: the ARMS, their parameters AS REALIZED, and every
        // counter they keep. APPENDED, never interleaved: every key above this
        // point is M3c's, in M3c's order, so the OFF control's identity gate
        // (RULE.md section 8) compares the field set it always did and these
        // keys land in its "printed by this probe only" list rather than
        // shifting anything.
        //
        // THE PARAMETERS ARE PRINTED AS REALIZED, not as requested, and they
        // come from a DEFAULT-CONSTRUCTED `QpDegeneracyParams` unless a flag
        // moved one. That is what makes RULE.md's parameter table CHECKABLE
        // rather than trusted.
        //
        // THE COUNTERS ARE THE HARVEST TAKEN ABOVE, BEFORE THE DISARM.
        kvs("arm", arm_arg);
#ifdef HVEN_TESTING
        kvi("arm_compiled_in", 1);
        kv("arm_dependency_tol", arm_params.dependency_tol);
        kvi("arm_integer_screen", arm_params.integer_screen ? 1 : 0);
        kv("arm_negligible_pivot", arm_params.negligible_pivot);
        kv("arm_inf_bound", arm_params.inf_bound);
        kv("arm_step_tie_tol", arm_params.step_tie_tol);
        kvi("arm_max_dep_tests_per_pass",
            static_cast<long long>(arm_params.max_dep_tests_per_pass));
#else
        kvi("arm_compiled_in", 0);
#endif
        kvi("arm_diag_post_drop_inertia", diag_inertia ? 1 : 0);

        // ARM A -- the scaled drop tolerance (RULE.md section 4).
        kvi("scaled_drops_made", a_drops_made);
        kvi("scaled_candidates_refused", a_cand_refused);
        kvi("scaled_drops_prevented", a_drops_prevented);
        kv("scaled_max_refused_ratio", a_max_refused_ratio);
        kvi("scale_unavailable_passes", a_scale_unavail);

        // THE DIAGNOSTIC (RULE.md section 6's last bullet), off by default.
        kvi("diag_factorizations", d_factorizations);
        kvi("diag_factorization_failures", d_factorization_failures);

        // ARM B1 -- the independence-preserving add (RULE.md section 5).
        kvi("indep_passes", b_passes);
        kvi("indep_candidates_examined", b_cand_examined);
        kvi("indep_candidates_passed_over", b_cand_passed_over);
        kvi("indep_dep_tests", b_dep_tests);
        kvi("indep_qr_builds", b_qr_builds);
        kvi("indep_test_failures", b_test_failures);
        // A8's THREE-MECHANISM SPLIT, counted apart (RULE.md section 3.1).
        kvi("indep_entry_members_removed", b_entry_removed);
        kvi("indep_screen_and_qr_dependent", b_screen_and_qr);
        kvi("indep_qr_only_dependent", b_qr_only);
        kvi("indep_blocker_switched", b_blocker_switched);
        kvi("indep_no_blocker_passes", b_no_blocker_passes);
        kvi("indep_budget_truncations", b_budget_truncations);
        kvi("indep_enumeration_mismatches", b_enum_mismatches);
        // The SENTINEL is SAID, never printed as the integer it is: a run in
        // which no pass published a value prints `absent`, which is not the
        // same fact as a published 0.
        if (b_rows_minus_n_seen) {
            kvi("indep_rows_minus_n_max", b_rows_minus_n_max);
        } else {
            kvs("indep_rows_minus_n_max", "absent");
        }
        kvi("indep_rank_deficit_max", b_rank_deficit_max);
        // RULE.md AMENDMENT 1 (C4's statistic): the per-solve high-water mark
        // of `cond(R)`, harvested before the disarm. NaN means NO
        // DECOMPOSITION WAS EVER TAKEN, which is NOT a well-conditioned set
        // reporting 1, and it is printed as `nan` rather than floored. It
        // EXPLAINS an exceedance of A10's bound and NEVER EXCUSES one: a solve
        // with `indep_leak_bound_exceeded > 0` is UNSUPPORTED whatever this
        // number says.
        kv("indep_max_cond_r", b_max_cond_r);
        kvi("indep_rank_violations", b_rank_violations);
        kvi("indep_refused_mandatory_dependent", b_refused_mandatory);
        kvi("indep_disarmed", b_disarmed);
        kvi("indep_disarm_site", b_disarm_site);
        kvi("indep_ride_refusals", b_ride_refusals);
        kvi("indep_shift_refusals", b_shift_refusals);
        kvi("indep_repair_pin_refusals", b_repair_pin_refusals);
        kvi("indep_probe_restore_dependent", b_probe_restore_dependent);
        kvi("indep_leak_bound_exceeded", b_leak_bound_exceeded);
        kv("indep_max_passed_violation", b_max_passed_violation);
        kvi("indep_solve_generation", b_solve_generation);

        // ---- M7 W2 E1c: NEW KEYS, appended; nothing above moves ----------
        kvi("equality_basis_touched", equality_basis_touched ? 1 : 0);
        kvi("opt_equality_basis", realized_equality_basis ? 1 : 0);
        {
            const auto &eb = result.equality_basis;
            auto join = [](const std::vector<Index> &v) {
                std::string out;
                for (std::size_t k = 0; k < v.size(); ++k) {
                    out += (k ? ";" : "") + std::to_string(static_cast<long long>(v[k]));
                }
                return out.empty() ? std::string("none") : out;
            };
            kvi("eqb_computed", eb.computed ? 1 : 0);
            kvi("eqb_undecidable", eb.undecidable ? 1 : 0);
            kvi("eqb_dropped_count", static_cast<long long>(eb.dropped_rows.size()));
            kvs("eqb_dropped_rows", join(eb.dropped_rows));
            kvi("eqb_me_reduced",
                static_cast<long long>(model->me()) - static_cast<long long>(eb.dropped_rows.size()));
            kvi("eqb_dropped_peak", static_cast<long long>(eb.dropped_peak));
            kvi("eqb_recomputes", static_cast<long long>(eb.recomputes));
            kvi("eqb_qr_factorizations", static_cast<long long>(eb.qr_factorizations));
            kvi("eqb_switched_off", eb.switched_off ? 1 : 0);
            kvi("eqb_inconsistent_row", static_cast<long long>(eb.inconsistent_row));
            kv("eqb_inconsistent_gap", eb.inconsistent_gap);
            kvs("eqb_inconsistent_support", join(eb.inconsistent_support));
            kvi("eqb_evidence_major_first", static_cast<long long>(eb.evidence_major_first));
            kvi("eqb_evidence_major_second", static_cast<long long>(eb.evidence_major_second));
            kv("eqb_max_gap", eb.max_gap);

            // Q3: the RETURNED point's equality residual, split by the exit's
            // partition. An absent/mis-sized ce is SAID, never zero-filled.
            const Vec &ce = result.ce;
            kvi("eqb_ce_size", static_cast<long long>(ce.size()));
            if (ce.size() == model->me()) {
                std::vector<char> is_dropped(static_cast<std::size_t>(ce.size()), 0);
                std::string per_row;
                double dmax = 0.0, rmax = 0.0;
                long long darg = -1, rarg = -1;
                for (Index r : eb.dropped_rows) {
                    if (r >= 0 && r < ce.size()) {
                        is_dropped[static_cast<std::size_t>(r)] = 1;
                    }
                }
                char buf[64];
                for (Index r = 0; r < ce.size(); ++r) {
                    const double a = std::abs(ce(r));
                    if (is_dropped[static_cast<std::size_t>(r)]) {
                        std::snprintf(buf, sizeof(buf), "%s%lld:%.17g", per_row.empty() ? "" : ";",
                                      static_cast<long long>(r), ce(r));
                        per_row += buf;
                        if (!(a <= dmax)) { dmax = a; darg = static_cast<long long>(r); }
                    } else {
                        if (!(a <= rmax)) { rmax = a; rarg = static_cast<long long>(r); }
                    }
                }
                kv("eqb_ce_dropped_max_abs", darg >= 0 ? dmax : std::numeric_limits<double>::quiet_NaN());
                kvi("eqb_ce_dropped_argmax", darg);
                kv("eqb_ce_retained_max_abs", rarg >= 0 ? rmax : std::numeric_limits<double>::quiet_NaN());
                kvi("eqb_ce_retained_argmax", rarg);
                kvs("eqb_ce_dropped_rows", per_row.empty() ? std::string("none") : per_row);
            } else {
                kvs("eqb_ce_note", "ce is not sized me; the split residual is NOT MEASURED");
            }
        }
        kv("sqp_stationarity", result.sqp_stationarity);
        kv("sqp_feasibility", result.sqp_feasibility);
        kv("sqp_complementarity", result.sqp_complementarity);
        kv("sqp_kkt_residual", result.kkt_residual);
        std::printf("E1C_COUNTERS_BEGIN\n");
        kvi("c_major_iters", static_cast<long long>(c.major_iters));
        kvi("c_qp_minor_iters", static_cast<long long>(c.qp_minor_iters));
        kvi("c_factorizations", static_cast<long long>(c.factorizations));
        kvi("c_steps_accepted", static_cast<long long>(c.steps_accepted));
        kvi("c_rejected_steps", static_cast<long long>(c.rejected_steps));
        kvi("c_soc_steps", static_cast<long long>(c.soc_steps));
        kvi("c_soc_applied", static_cast<long long>(c.soc_applied));
        kvi("c_elastic_activations", static_cast<long long>(c.elastic_activations));
        kvi("c_restoration_iters", static_cast<long long>(c.restoration_iters));
        kvi("c_eqp_refine_steps", static_cast<long long>(c.eqp_refine_steps));
        kvi("c_border_refine_steps", static_cast<long long>(c.border_refine_steps));
        kvi("c_verdict_refine_steps", static_cast<long long>(c.verdict_refine_steps));
        kvi("c_suspect_escalations", static_cast<long long>(c.suspect_escalations));
        kvi("c_symbolic_analyses", static_cast<long long>(c.symbolic_analyses));
        kvi("c_full_step_majors", static_cast<long long>(c.full_step_majors));
        kvi("c_watchdog_restores", static_cast<long long>(c.watchdog_restores));
        kvi("c_evals_full", static_cast<long long>(c.evals_full));
        kvi("c_evals_values", static_cast<long long>(c.evals_values));
        kvi("c_elastic_escalations", static_cast<long long>(c.elastic_escalations));
        kvi("c_soc_qp_infeasible", static_cast<long long>(c.soc_qp_infeasible));
        kvi("c_soc_rejected", static_cast<long long>(c.soc_rejected));
        std::printf("E1C_COUNTERS_END\n");

        std::printf("E1C_HISTORY_BEGIN\n");
        kvi("e1c_history_rows", static_cast<long long>(result.history.size()));
        std::printf("row,trial,f,stationarity,feasibility,complementarity,tr_radius,"
                    "step_norm,qp_solved,qp_status,qp_minor_iters,qp_factorizations,"
                    "tr_binding,verdict,soc_applied,elastic_applied,restoration_seed_used,"
                    "watchdog_restored,active_rows\n");
        for (std::size_t k = 0; k < result.history.size(); ++k) {
            const auto &h = result.history[k];
            std::printf("%zu,%lld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%d,%s,%lld,%lld,%d,%s,"
                        "%d,%d,%d,%d,%lld\n",
                        k, static_cast<long long>(h.trial), h.f, h.stationarity,
                        h.feasibility, h.complementarity, h.tr_radius, h.step_norm,
                        h.qp_solved ? 1 : 0, qp_status_name(h.qp_status),
                        static_cast<long long>(h.qp_minor_iters),
                        static_cast<long long>(h.qp_factorizations), h.tr_binding ? 1 : 0,
                        hven::solvers::to_string(h.verdict), h.soc_applied ? 1 : 0,
                        h.elastic_applied ? 1 : 0, h.restoration_seed_used ? 1 : 0,
                        h.watchdog_restored ? 1 : 0, static_cast<long long>(h.active_rows));
        }
        std::printf("E1C_HISTORY_END\n");

        std::printf("E1C_EVENTS_BEGIN\n");
        kvi("e1c_event_rows", static_cast<long long>(e1c_events.size()));
        std::printf("event,iteration,depth,f,stationarity,feasibility_e,feasibility_i,"
                    "complementarity,step_norm,radius\n");
        for (std::size_t k = 0; k < e1c_events.size(); ++k) {
            const auto &e = e1c_events[k];
            std::printf("%zu,%lld,%lld,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g,%.17g\n", k,
                        e.iteration, e.depth, e.f, e.stationarity, e.feasibility_e,
                        e.feasibility_i, e.complementarity, e.step_norm, e.radius);
        }
        std::printf("E1C_EVENTS_END\n");

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
            write_solve_summary(capture_solve_arg);
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
        std::fprintf(stderr, "m5_arm_probe: %s\n", e.what());
        return 1;
    }
}
