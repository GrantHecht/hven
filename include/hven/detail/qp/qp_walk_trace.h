// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_walk_trace.h — the PRIMAL ACTIVE-SET WALK's test seam: a READ-ONLY
// per-minor observer, on the convention docs/testing.md describes and
// CLAUDE.md §8 mandates ("use this convention for any new need rather than
// inventing another"). It is the read-only VARIANT of that convention —
// observing internal state rather than injecting a fault — the same
// arrangement as `PardisoIparmObserver` (linear layer) and
// `IpqpInertiaReadObserver` / `IpqpFirstIterateObserver` (the IPQP tier).
//
// WHY IT EXISTS. `QpEngine`'s finest public instrument is the per-SOLVE
// `Ledger` record: one `QpCounters` per subproblem. M7 W2 M3a addendum F read
// those counters on STEENBRB and YORKNET at `tr_init = inf` and found the first
// QP subproblem spending its whole iteration cap — 4680 and 2560 minors — in
// what the counters describe as long degenerate active-set churn (2319 of 2344
// additions at a (near-)zero step on STEENBRB; 23 distinct bound variables
// admitted 2344 times). Its verdict was MIXED and cycling PROPER — a working
// set revisited — was NOT shown, because SIX different mechanisms produce those
// same counters and a per-solve aggregate cannot separate them: zero-multiplier
// probes (which `ws_drops` merges with Dantzig drops), an immediate drop and
// re-add of the bound just admitted, lower/upper ping-pong on one fixed or
// near-fixed variable, a long crawl through DIFFERENT sets built from the same
// few variables, negative-curvature ride blockers (folded into `ws_adds`), and a
// warm working-set hand-off replaying the same stall. Separating them needs the
// per-MINOR record this header declares, which no public surface carries and
// none should: it is a diagnostic instrument, not a product feature.
//
// WHAT IT RECORDS, per minor: the minor index; WHICH ACTION the loop took
// (every branch of the walk is enumerated in `QpWalkAction`, not summarized);
// the constraint's identity AND side; for a drop, the ROUTE (the Dantzig
// multiplier rule vs §4b's zero-multiplier probe); the step length, the
// direction's magnitude and the executed move; the ratio test's own uncapped
// minimum and the multiplicity of it; the multiplier that drove a drop with
// the tolerance it was compared against; whether the step was a
// negative-curvature RIDE; the working-set SIZE and a 64-bit ORDER-INDEPENDENT
// SIGNATURE of the working set (constraint id + side) both BEFORE and AFTER the
// pass; and two cheap norms of the iterate the pass began at, so that "the same
// working set at the same point" is decidable from the trace. Once per solve it
// records the INCOMING (warm) working set's signature and size.
//
// WHAT FIX-ROUND 1 ADDED, and why. The first trace answered WHAT the walk does
// — an exact zero-motion period-2 tail on both cells — and could not answer
// WHY, because four different causes produce that same tail: an indefinite
// reduced Hessian the ordinary add/drop path never consults the verdict about;
// a curvature gate that stays silent because the computed direction is not the
// negative-curvature one; §4c's ride vetoed (by the retained-set nullspace
// test, by a flat slope, or by descent and release-feasibility disagreeing);
// and a numerically wrong direction. Telling them apart needs, per pass: the
// MAIN inertia verdict with the backend's own triple and the gate's
// expectation; which linear-algebra path produced the direction and whether its
// factorization was fresh, updated or marked for rebuild; whether the curvature
// gate was reached and whether it fired, with pᵀHp and pᵀp separately; a
// GRANULAR ride outcome naming the branch taken; on a post-drop pass the
// released constraint's identity, `p` along it, the sign the ride chose and
// gᵀp; the drop rule's candidate set (identities and violations); the effective
// regularization; and the unregularized stationarity and active-row residuals.
// The candidates' PREDICTED POST-DROP INERTIA and rank evidence for the
// released constraint would each need an extra factorization, so they are
// deliberately NOT recorded and are marked absent in the rule instead.
//
// IT IS RECORD-ONLY. Nothing here mutates the walk: no decision, no counter, no
// floating-point operation of the solver's own arithmetic, and no factorization
// and no linear SOLVE. The norms, residuals and signatures it computes are
// computed from state the loop already holds — or by a mat-vec into storage the
// loop never reads. `tests/qp/test_qp_walk_seam.cpp` proves that as a TEST
// rather than as a claim: with the observer attached, every `QpCounters` field
// and the solution the engine returns are bit-identical to the unattached run
// on six cases, two of them degenerate.
//
// AND IT IS PER-THREAD. Every piece of observer state below has THREAD storage
// duration, so two instrumented `QpEngine::run` calls on two threads cannot
// share a capture, a solve counter, a record buffer or a current-record
// pointer. That is what makes "record-only" true under concurrency as well as
// in sequence: with process-global state the two would race, and a data race is
// undefined behaviour, which is not a property any amount of read-only
// discipline can recover. `QpWalkSeam.TwoEnginesOnTwoThreads*` runs the case.
// The consequence for a fixture: arming, `reset()` and reading `records` are
// all per-thread, and a thread that captures must copy its records out before
// it exits.
//
// ONE RECORD PER PASS, QUALIFIED. The per-minor scope's destructor emits on
// every exit the loop body has, so the count of records equals the count of
// passes entered — for a capture that was NOT truncated (`max_records` not
// reached: `QpWalkObserver::truncated`) and in which every record's storage was
// successfully allocated (`QpWalkObserver::allocation_failed`). Both are
// reported, and a measurement that asserts the per-pass correspondence must
// check both flags. An allocation failure is caught inside the destructor and
// recorded rather than propagated, so it cannot terminate the solve.
//
// IT COMPILES TO NOTHING without `HVEN_TESTING`, so including it from a normal
// build of any TU is provably inert. `HVEN_TESTING` is defined ONLY target-wide
// on the standalone executables in `tests/CMakeLists.txt` that recompile the
// instrumented sources a second time and do NOT link `hven::hven`
// (`hven_fault_injection_tests`, `hven_ipqp_seam_tests`, and this seam's
// `hven_qp_walk_seam_tests`), which is what keeps the production library and
// every ordinary test binary untouched by it.
//
// WHERE THE HOOKS GO: `src/qp/qp_engine.cpp`, an APACHE-2.0 FILE THIS
// REPOSITORY WROTE. CLAUDE.md §6's boundary preference is satisfied WITHOUT a
// deviation — no MPL-derived session file is touched and nothing about a
// backend call is observed — so this seam adds NO entry to `notices/` and no new
// sanctioned inside-the-session-file deviation. The facts it records live
// inside one iteration of one loop and leave no trace outside it, which is
// exactly the case §6 says the boundary cannot see.
//
// WHY THE INCLUDE IS IN `qp_engine.h` AND NOT IN `qp_engine.cpp`. Stated
// plainly, because it is unusual: `src/qp/qp_engine.cpp` carries an `assert()`
// ABOVE the walk loop, and `assert` materializes `__LINE__` as an immediate
// operand in a Debug object. An `#include` added at the top of that TU would
// move that immediate, so the production Debug object would differ in its
// INSTRUCTION STREAM — a difference P-SYM would correctly classify as
// `__LINE__`-class noise, but a text difference all the same. Including from
// the header this TU already includes adds no line to the TU at all, and every
// `#ifdef HVEN_TESTING` block below is under the walk loop, so the instruction
// stream cannot move. `qp_engine.h` itself contains no `assert`, no `throw` and
// no other `__LINE__`-bearing construct. The cost of the wider inclusion is
// zero by construction: without `HVEN_TESTING` this file's entire body is
// `#ifdef`-ed away.
//
// WHAT THAT PLACEMENT DOES NOT BUY, measured rather than assumed: it does NOT
// make the Debug object byte-identical. This header gains a line, so the DWARF
// line programme of every inline function defined below it moves, and the three
// production TUs that include it differ in bytes for that reason alone — with
// identical instructions, identical symbol tables, and identical relocation
// records outside `.rela.debug_line`. Release is byte-identical outright, all
// 42 library objects. The measurement is
// docs/notes/data/2026-09-m7-w2-m3b-minor-trace/gates/psym-derivation.txt.

#ifdef HVEN_TESTING

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/types.h>
#include <hven/detail/qp/working_set.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers::detail::testing {

/// WHICH BRANCH OF THE WALK a minor took. Every `continue`, every `break` and
/// the fall-through step are enumerated, so a trace never has to infer an
/// action from the fields around it.
enum class QpWalkAction : int {
    /// The scope was destroyed without any branch claiming it. A record with
    /// this value is a BUG in the instrumentation, and the fixtures assert
    /// against it rather than tolerating it.
    kUnset = 0,
    /// §4b's temporary-vertex START REPAIR at `iter == 0`.
    kStartRepair,
    /// §4b's POST-PROBE RESTART, the same repair spent once later in the solve.
    kPostProbeRestart,
    /// A KKT point of the working set, and the Dantzig rule released something.
    kDropDantzig,
    /// A would-be `kOptimal` exit at which §4b's ZERO-MULTIPLIER PROBE found
    /// hidden negative curvature and made a drop real.
    kDropZeroProbe,
    /// §4b's SUSPECT-STALL gate escalated `primal_delta` and retried.
    kSuspectEscalation,
    /// A KKT point with nothing left to drop: the loop classified and broke.
    /// `QpWalkObserver::exit_status` carries what it decided.
    kTerminal,
    /// §4c's negative-curvature RIDE moved to a blocker.
    kRideStepped,
    /// §4c's ride found nothing blocking: the engine-internal unbounded exit.
    kRideUnbounded,
    /// The ordinary step, blocked by a constraint that joined the working set.
    kAdd,
    /// The ordinary step, nothing blocking: the full step to the EQP point.
    kMoveOnly,
};

/// Which block a constraint identity refers to.
enum class QpWalkBlock : int { kNone = 0, kIneq = 1, kBound = 2 };

/// HOW a constraint left the working set. The distinction `QpCounters::ws_drops`
/// cannot make — it counts both routes as one number.
enum class QpWalkDropRoute : int { kNone = 0, kDantzig = 1, kZeroProbe = 2 };

/// WHICH LINEAR-ALGEBRA PATH produced this pass's EQP candidate. The three are
/// materially different systems, not three spellings of one: the bordered
/// Schur solve reads K0's backend inertia plus the border stack's Haynsworth
/// contribution, the eliminated path factorizes the bound-eliminated K outright,
/// and the empty system is factorized by nobody.
enum class QpWalkEqpPath : int {
    kNone = 0,
    /// Every variable pinned, no rows: a 0x0 system, no factorization at all.
    kEmptySystem = 1,
    /// `eliminated_candidate` — one fresh factorization of the reduced K.
    kEliminated = 2,
    /// `solve_bordered_eqp` through the live Schur complement.
    kBorderSchur = 3,
};

/// The backend's own `InertiaEvidence::State`, widened to carry ABSENCE. `-1`
/// (`kAbsent`) means no verdict site ran on this pass, which is not the same
/// fact as a backend that could not report (`kUnavailable`).
enum class QpWalkInertiaState : int {
    kAbsent = -1,
    kObserved = 0,
    kQueryFailed = 1,
    kUnavailable = 2,
};

/// WHY §4c's negative-curvature ride did or did not move the iterate. The
/// engine's own `RideOutcome` has three values and folds every refusal into
/// `kDeclined`; the review's FIX-ROUND finding is precisely that "the ride did
/// not step" and "the ride was vetoed for reason R" are different facts and the
/// first cannot stand in for the second. These are the branches
/// `ride_negative_curvature` and `ride_sign` actually have.
enum class QpWalkRideOutcome : int {
    /// The pass was not a post-drop pass, or the curvature gate did not fire,
    /// so `ride_negative_curvature` was never called.
    kNotAttempted = 0,
    /// `ride_stays_in_working_set` was false: `p` leaves the RETAINED working
    /// set's nullspace, so riding it would violate a constraint still held.
    kNullspaceVeto = 1,
    /// `ride_sign` found no strictly descending sign — `|gᵀp|` inside
    /// `slope_tol`. A flat direction has no descending sign and declines.
    kSlopeFlatVeto = 2,
    /// `ride_sign` found a descending sign, and FEASIBILITY vetoed it: that
    /// sign steps back into the constraint the drop rule just released.
    kSignDisagreementVeto = 3,
    /// A sign was chosen and the uncapped ratio test found NOTHING blocking:
    /// the engine's `kUnbounded`.
    kUnbounded = 4,
    /// A sign was chosen, a blocker was found, and the iterate moved.
    kStepped = 5,
};

/// SplitMix64's finalizer, used only to spread constraint tokens before they
/// are summed into an order-independent signature. Nothing depends on its
/// cryptographic quality; it needs to avoid the near-linear collisions a plain
/// sum of small integers would have.
inline std::uint64_t qp_walk_mix(std::uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

/// One working-set member's token: the identity in the high bits, the SIDE in
/// the low three. Rows carry tag 4, bounds carry their own `BoundState` (1, 2
/// or 3), so a row `k` and a variable `k` never collide and a variable pinned
/// at its lower bound is a different member from the same variable pinned at
/// its upper one.
inline std::uint64_t qp_walk_token(bool is_ineq, Index idx, BoundState side) {
    const auto id = static_cast<std::uint64_t>(idx);
    const std::uint64_t tag = is_ineq ? 4ULL : static_cast<std::uint64_t>(side);
    return (id << 3) | tag;
}

/// A 64-bit ORDER-INDEPENDENT signature of a working set: the sum of the mixed
/// tokens of its members. Order-independent because addition is commutative,
/// and members are unique, so no member can cancel another.
inline std::uint64_t qp_walk_ws_signature(const WorkingSet &ws) {
    std::uint64_t sig = 0;
    for (const Index row : ws.active_ineq()) {
        sig += qp_walk_mix(qp_walk_token(true, row, BoundState::kFree));
    }
    const std::vector<BoundState> &bs = ws.bound_state();
    for (std::size_t i = 0; i < bs.size(); ++i) {
        if (bs[i] != BoundState::kFree) {
            sig += qp_walk_mix(qp_walk_token(false, static_cast<Index>(i), bs[i]));
        }
    }
    return sig;
}

/// How many constraints the working set holds (rows plus pinned variables).
inline Index qp_walk_ws_size(const WorkingSet &ws) {
    Index size = static_cast<Index>(ws.active_ineq().size());
    for (const BoundState st : ws.bound_state()) {
        if (st != BoundState::kFree) {
            ++size;
        }
    }
    return size;
}

/// ONE MEMBER of the Dantzig drop rule's candidate set: a constraint whose
/// multiplier violates its sign condition by more than `opt_tol`, i.e. one the
/// rule COULD have released on this pass. Recorded because the rule picks by
/// LARGEST VIOLATION alone, with no regard for whether the post-drop subspace
/// admits a usable direction — the review's still-plausible cause (1) — and the
/// identities and violations are the part of that candidate set observable
/// WITHOUT the extra factorization a predicted post-drop inertia would need.
struct QpWalkDropCandidate {
    QpWalkBlock block = QpWalkBlock::kNone;
    Index index = -1;
    /// The side a BOUND candidate is currently pinned at; `kFree` for a row.
    BoundState side = BoundState::kFree;
    double violation = std::numeric_limits<double>::quiet_NaN();
};

/// ONE MINOR ITERATION of the walk. An aggregate, so its members are bare
/// `snake_case` (CLAUDE.md §4). Every quantity that a branch does not produce
/// stays at its declared ABSENT value — NaN for a float, `-1` for an index,
/// `kNone` for a classification — and is never zero-filled, so a reader of the
/// trace can tell "the loop did not compute this" from "the loop computed
/// zero".
struct QpWalkMinorRecord {
    static constexpr double kAbsent = std::numeric_limits<double>::quiet_NaN();

    /// Which solve of this engine this record belongs to (0-based, counted by
    /// `QpWalkObserver::solves` over the observer's lifetime).
    Index solve = 0;
    /// `counters.minor_iters - 1` at the top of this pass: 0-based, and equal
    /// to the loop's own `iter`.
    Index minor = 0;
    QpWalkAction action = QpWalkAction::kUnset;

    // ---- the constraint this minor acted on, and which side of it ----------
    QpWalkBlock block = QpWalkBlock::kNone;
    Index block_index = -1;
    /// The side a BOUND was pinned at or released from; `kFree` for a row and
    /// for a minor that touched no bound.
    BoundState block_side = BoundState::kFree;
    QpWalkDropRoute drop_route = QpWalkDropRoute::kNone;
    /// `upper(i) - lower(i)` for a bound constraint: what makes a variable
    /// FIXED or NEAR-FIXED, which is one of the six mechanisms. Absent for rows.
    double bound_gap = kAbsent;

    // ---- the step -----------------------------------------------------------
    /// The step actually taken along `p` (the ratio test's return, capped at 1
    /// on the ordinary path; the raw ratio on a ride).
    double alpha = kAbsent;
    /// `||p||inf`, the EQP direction's magnitude before the step.
    double p_inf = kAbsent;
    /// `alpha * ||p||inf`: how far the iterate actually moved.
    double dx_inf = kAbsent;
    /// The loop's own negligible-step threshold at this pass
    /// (`feas_tol * max(1, ||x||inf)`) — the tolerance BOTH the KKT-point test
    /// and the degenerate-step classification are made against.
    double step_tol = kAbsent;
    /// True iff the loop counted this pass into `QpCounters::degenerate_steps`.
    bool degenerate = false;
    /// True iff §4c's ride was taken (a `kRideStepped` action).
    bool ride = false;

    // ---- the ratio test's own reading ---------------------------------------
    /// The UNCAPPED minimum ratio the ratio test found (infinite iff nothing
    /// blocks). Recorded before the cap-at-unit clamp, so a blocking ratio
    /// beyond the EQP point is visible as such.
    double raw_ratio = kAbsent;
    /// How many candidates shared that minimum EXACTLY (the quantity behind
    /// `QpCounters::ratio_ties`, per minor).
    Index ratio_tied = 0;
    /// False when the reading came from a RIDE's uncapped call.
    bool ratio_capped = true;

    // ---- the multiplier that drove a drop, and its tolerance ---------------
    /// The Dantzig violation of the constraint that was dropped, or the
    /// magnitude of the (numerically zero) multiplier the probe dropped on.
    double decision_multiplier = kAbsent;
    /// The tolerance it was compared against (`QpOptions::opt_tol`).
    double decision_tol = kAbsent;
    /// The largest violation in the drop rule's candidate set.
    double best_multiplier = kAbsent;
    /// How many candidates the drop rule considered, and how many of them sat
    /// inside its tie window (the quantity behind `QpCounters::drop_ties`).
    Index drop_candidates = 0;
    Index drop_tied = 0;

    // ---- the working set, before and after this pass ------------------------
    /// How many rows the HOMOTOPY put into the working set during this pass
    /// (`QpCounters::shift_adds`, differenced). Recorded because it is the one
    /// other way the set grows: without it the size arithmetic of a pass is
    /// not decidable from the trace, and a row entering because it is being
    /// driven to feasibility would read as an active-set addition.
    Index shift_adds = 0;
    Index ws_size_in = 0;
    std::uint64_t ws_sig_in = 0;
    Index ws_size_out = 0;
    std::uint64_t ws_sig_out = 0;

    // ---- the iterate this pass STARTED from --------------------------------
    /// Paired with `ws_sig_in`: together they are "the state this minor began
    /// in", which is what a recurrence has to match for CYCLING PROPER.
    double x_inf = kAbsent;
    double x_l1 = kAbsent;

    // =====================================================================
    // FIX-ROUND 1 (M7 W2 M3b): THE DISCRIMINATING FIELDS.
    //
    // Everything above answers WHAT the walk did. Everything below exists to
    // separate WHY, and only that: the first trace established an exact
    // zero-motion period-2 tail and then could not say whether the reduced
    // Hessian was indefinite and ignored, whether the curvature gate stayed
    // silent on the computed direction, whether §4c's ride was vetoed and by
    // which of its three refusals, or whether the direction was simply
    // numerically wrong. Each field below is read from a value the loop had
    // already computed, or from a mat-vec taken into observer storage the loop
    // never reads. NOTHING here performs a factorization or a linear SOLVE,
    // touches engine state, or touches a counter.
    // =====================================================================

    // ---- the MAIN pass's inertia verdict, and the evidence behind it -------
    //
    // "Main" is load-bearing: `probe_zero_multiplier_drops` calls the same
    // verdict site again for HYPOTHETICAL working sets, so these are snapshotted
    // out of `QpWalkObserver::eqp` immediately after the walk loop's own
    // `eqp_candidate` returns, before any probe can overwrite the scratch.
    /// `detail::InertiaVerdict` as an int (0 kOk / 1 kSuspect / 2 kWrong);
    /// `-1` when no verdict site ran.
    int inertia_verdict = -1;
    QpWalkInertiaState inertia_state = QpWalkInertiaState::kAbsent;
    /// The backend's own triple. `-1` is ABSENT, and is what the backend
    /// reports for a state other than `kObserved` — never a substituted zero.
    Index inertia_pos = -1;
    Index inertia_neg = -1;
    Index inertia_zero = -1;
    /// True when `inertia_zero` was DERIVED as dim − pos − neg (MKL) rather
    /// than reported natively (Accelerate). A derived zero of 0 is not an
    /// observation that there are no zero eigenvalues.
    bool inertia_zero_derived = false;
    /// Pardiso's perturbed-pivot count, `-1` when the backend has no such
    /// counter (Accelerate) — the OPTIONAL's absence, never zero-filled.
    Index inertia_perturbed = -1;
    /// What the gate EXPECTED, which is the other half of a verdict: `kWrong`
    /// alone does not say in which direction, or by how much.
    Index inertia_expected_pos = -1;
    Index inertia_expected_neg = -1;

    // ---- how the direction was produced -------------------------------------
    QpWalkEqpPath eqp_path = QpWalkEqpPath::kNone;
    /// True iff `border_candidate` was entered at all (so a `kEliminated` path
    /// with this set is a border-mode FALL-BACK, not the elimination mode).
    bool border_attempted = false;
    /// True iff `rebuild_k0` ran during this pass's main candidate call: the
    /// factorization is FRESH rather than an update over a retained K0.
    bool k0_rebuilt = false;
    /// True iff bordering was LATCHED off for this working-set shape.
    bool border_latched = false;
    /// `SchurComplement::needs_refactorization()` as `border_solve_or_fall_back`
    /// found it — "marked for rebuild" on entry to the solve.
    bool border_needs_refactorization = false;
    /// The live border stack's dimension at the solve; `-1` when none was used.
    Index border_dim = -1;
    /// THE MAIN CALL's own `EqpResult::refine_steps` (border: total, including
    /// the mandatory first; eliminated: extra beyond it, identically 0).
    Index eqp_call_refine_steps = -1;

    // ---- what the WHOLE pass cost, by counter difference --------------------
    //
    // Differenced across the pass exactly as `shift_adds` is, so these include
    // the probe's and the verdict site's calls as well as the main one. Read
    // from a `const QpCounters &`: the scope cannot write a counter.
    Index pass_factorizations = 0;
    Index pass_eqp_refine_steps = 0;
    Index pass_border_refine_steps = 0;
    Index pass_verdict_refine_steps = 0;
    Index pass_schur_updates = 0;

    // ---- §4c's curvature gate, and the direction it judged -----------------
    /// True iff the previous pass's drop armed the gate (`dropped.active`), so
    /// the curvature test was REACHED. False means §4c never looked.
    bool post_drop = false;
    /// True iff the gate FIRED (`curvature <= curvature_tol`), i.e. the ride
    /// was actually offered the direction.
    bool curvature_fired = false;
    /// The loop's own quotient `pᵀHp / pᵀp`, and the threshold it was compared
    /// against (`kCurvatureTolFactor * hessian_scale(qp)`).
    double curvature = kAbsent;
    double curvature_tol = kAbsent;
    /// The quotient's two halves, recorded separately so "the direction is not
    /// a negative-curvature one" is decidable without reconstructing them.
    /// Computed into observer storage; the loop's own expression is untouched.
    double p_hp = kAbsent;
    double p_sq = kAbsent;

    // ---- §4c's ride: which branch, and the numbers that chose it -----------
    QpWalkRideOutcome ride_outcome = QpWalkRideOutcome::kNotAttempted;
    /// `ride_sign`'s return: +1, −1, or 0 for "no admissible sign".
    int ride_sign_chosen = 0;
    /// `gᵀp` at the pass's starting iterate with `g = Hx + g`, i.e. the SLOPE
    /// `ride_sign` reads, and the tolerance it is judged flat against.
    double ride_slope = kAbsent;
    double ride_slope_tol = kAbsent;
    /// `-dropped_directional(qp, dropped, p)`: how far `+p` moves AWAY from the
    /// constraint just released, and its own tolerance. Together with the slope
    /// these are the whole of `ride_sign`'s decision.
    double ride_leaves = kAbsent;
    double ride_leaves_tol = kAbsent;

    // ---- the constraint the PREVIOUS pass released -------------------------
    //
    // Present on every post-drop pass, whether or not the ride fired: it is
    // what makes "the direction points back through the bound just released"
    // checkable per pass rather than inferred from the add that follows.
    QpWalkBlock released_block = QpWalkBlock::kNone;
    Index released_index = -1;
    /// The side the released BOUND was held at before the drop.
    BoundState released_side = BoundState::kFree;
    /// `p[released_index]` for a bound; `Ai.row(released_index) · p` for a row.
    double p_released = kAbsent;
    /// `dropped_directional(qp, dropped, p)` — the signed component of `p`
    /// along the released constraint's own INWARD direction.
    double released_directional = kAbsent;

    // ---- the drop rule's candidate set (identities and violations only) -----
    //
    // The post-drop inertia of the candidates NOT picked would need one extra
    // factorization each and is deliberately NOT computed: the rule marks it
    // ABSENT rather than approximating it.
    static constexpr std::size_t kMaxRecordedCandidates = 8;
    /// Up to `kMaxRecordedCandidates` candidates, largest violation first.
    QpWalkDropCandidate candidates[kMaxRecordedCandidates]{};
    /// How many of the `drop_candidates` are actually stored above.
    Index candidates_recorded = 0;

    // ---- regularization and residuals (read-only, no extra solve) ----------
    /// The EFFECTIVE regularization this pass solved at — `eff_opts`, not
    /// `opts_`: §4b's suspect ladder escalates `primal_delta` per solve.
    double primal_delta = kAbsent;
    double dual_mu = kAbsent;
    /// `detail::free_block_stationarity` at the pass's STARTING iterate `x`
    /// with the multipliers `price` just produced: the UNREGULARIZED first-order
    /// residual `Hx + g + Aeᵀλe + Aiᵀλi` over the free block, and the largest
    /// term that went into it (its scale).
    double stat_free_inf = kAbsent;
    double stat_scale = kAbsent;
    /// `max` over the EQUALITY rows of `|Ae·x − be|`, over the WORKING
    /// inequality rows of `|Ai·x − bi|` (read off the maintained `Aix`), and
    /// over the PINNED variables of `|x_i − bound_i|`: how far the iterate sits
    /// off the constraints the EQP holds at equality.
    double active_res_inf = kAbsent;
};

/// WHAT ONE CALL OF THE EQP CANDIDATE SITE REPORTED ABOUT ITSELF. A scratch
/// slot rather than a field on the record, because `probe_zero_multiplier_drops`
/// re-enters the same site for HYPOTHETICAL working sets: the walk loop
/// snapshots this into its own record the instant its own call returns, and
/// whatever a probe writes afterwards lands here and is discarded.
struct QpWalkEqpEvidence {
    QpWalkEqpPath path = QpWalkEqpPath::kNone;
    QpWalkInertiaState state = QpWalkInertiaState::kAbsent;
    Index n_pos = -1;
    Index n_neg = -1;
    Index n_zero = -1;
    bool zero_is_derived = false;
    Index perturbed_pivots = -1;
    Index expected_pos = -1;
    Index expected_neg = -1;
    Index refine_steps = -1;
    bool border_attempted = false;
    bool k0_rebuilt = false;
    bool latched = false;
    bool needs_refactorization = false;
    Index border_dim = -1;
};

/// THE OBSERVER. Static, like every other observer on this seam, because the
/// engine is reached through a deep call chain and a fixture needs to read what
/// it recorded after the solve returns — and, since FIX-ROUND 1, THREAD_LOCAL.
///
/// WHY THREAD_LOCAL (the review's Critical finding). Process-global mutable
/// state with no synchronization is a DATA RACE the moment two instrumented
/// `QpEngine::run` calls overlap: two solves would share one `solves` counter,
/// one `capturing` flag, one `records` vector and one `current` pointer, so one
/// solve could push into the other's buffer, disarm the other's capture, or
/// leave `current` dangling at the other's stack record. That is undefined
/// behaviour, and undefined behaviour is not "record-only" — which made the
/// answer to "can any hook alter a CONCURRENT solve's behaviour?" yes.
///
/// With thread storage duration every one of those is per-thread: a capture on
/// one thread can neither see nor disturb a solve on another, armed or not, and
/// `QpEngine`'s own header contract (sequential use of ONE engine) is what
/// makes per-thread the right granularity — an engine is never shared across
/// threads, so a thread's observer describes exactly the solves that thread ran.
///
/// THE CONTRACT A FIXTURE MUST HONOUR: arming, `reset()`, and READING
/// `records` are all per-thread. A thread that arms the observer must also read
/// (or copy out) its records BEFORE it exits; another thread cannot read them,
/// and after the thread ends the storage is gone.
struct QpWalkObserver {
    static inline thread_local bool active = false;

    /// How many `QpEngine::run` calls have begun ON THIS THREAD since
    /// `reset()`.
    static inline thread_local Index solves = 0;
    /// Which solve to record; `-1` records every one. M7 W2 M3b's measurement
    /// wants the FIRST subproblem of a driver solve and nothing else.
    static inline thread_local Index capture_solve = -1;
    /// A hard ceiling on `records`, so an observer left armed over a long run
    /// cannot grow without bound. `truncated` says whether it bit.
    static inline thread_local std::size_t max_records = 1000000;
    static inline thread_local bool truncated = false;
    /// Set when a record could not be stored because its allocation THREW.
    /// See `QpWalkMinorScope::~QpWalkMinorScope`: the throw is swallowed and
    /// reported here rather than propagating out of a destructor, so an
    /// out-of-memory condition cannot terminate a solve that would otherwise
    /// have completed. A capture with this flag set is INCOMPLETE.
    static inline thread_local bool allocation_failed = false;

    static inline thread_local std::vector<QpWalkMinorRecord> records;

    // ---- per-solve, for the solve being recorded ---------------------------
    /// True while a recorded solve is in progress.
    static inline thread_local bool capturing = false;
    /// The 0-based index of the solve the fields below describe.
    static inline thread_local Index solve_index = -1;
    /// `QpEngine::run`'s own `warm` argument.
    static inline thread_local bool warm = false;
    static inline thread_local Index n = 0;
    static inline thread_local Index me = 0;
    static inline thread_local Index mi = 0;
    /// `detail::effective_qp_max_iter`'s resolved cap for this solve.
    static inline thread_local Index max_iter = 0;
    /// The INCOMING working set — after the seed ingest and the seeding
    /// `refresh_shifts`, i.e. exactly the set the walk's first pass starts from.
    static inline thread_local std::uint64_t incoming_ws_signature = 0;
    static inline thread_local Index incoming_ws_size = 0;
    /// The status the recorded solve's walk loop left (`kTerminal`'s verdict,
    /// or `kMaxIter` when the cap was reached). `-1` until the loop ends.
    static inline thread_local int exit_status = -1;

    /// The record the current pass is filling, or `nullptr`. The hooks in
    /// `drop_worst`, `ratio_test`, `probe_zero_multiplier_drops` and
    /// `ride_negative_curvature` write through this rather than being handed a
    /// record, because those functions' signatures are production surface and
    /// this seam does not touch them.
    static inline thread_local QpWalkMinorRecord *current = nullptr;

    /// The scratch the EQP candidate site reports into. Cleared by the walk
    /// loop before its own call and snapshotted immediately after it.
    static inline thread_local QpWalkEqpEvidence eqp{};

    static void reset() {
        active = false;
        solves = 0;
        capture_solve = -1;
        max_records = 1000000;
        truncated = false;
        allocation_failed = false;
        records.clear();
        capturing = false;
        solve_index = -1;
        warm = false;
        n = 0;
        me = 0;
        mi = 0;
        max_iter = 0;
        incoming_ws_signature = 0;
        incoming_ws_size = 0;
        exit_status = -1;
        current = nullptr;
        eqp = QpWalkEqpEvidence{};
    }

    /// Called once per `QpEngine::run`, before the walk loop. Returns nothing:
    /// whether this solve is recorded is `capturing`.
    static void begin_solve(bool warm_in, Index n_in, Index me_in, Index mi_in, Index max_iter_in,
                            const WorkingSet &ws) {
        const Index index = solves;
        ++solves;
        capturing = active && (capture_solve < 0 || capture_solve == index);
        if (!capturing) {
            return;
        }
        solve_index = index;
        warm = warm_in;
        n = n_in;
        me = me_in;
        mi = mi_in;
        max_iter = max_iter_in;
        incoming_ws_signature = qp_walk_ws_signature(ws);
        incoming_ws_size = qp_walk_ws_size(ws);
        exit_status = -1;
        eqp = QpWalkEqpEvidence{};
    }

    /// Called once per `QpEngine::run`, after the walk loop.
    static void end_solve(int status) {
        if (capturing) {
            exit_status = status;
        }
        capturing = false;
        current = nullptr;
    }
};

/// THE PER-MINOR SCOPE. Declared at the top of the walk loop's body; its
/// destructor emits the record. A destructor is what makes ONE declaration
/// cover every exit the loop has — the six `continue`s, the `break`s and the
/// fall-through — without an emit call at each of them, and without any risk
/// that a path added later silently stops recording.
class QpWalkMinorScope {
  public:
    /// `counters` is held BY CONST REFERENCE and only ever READ (its
    /// `shift_adds` is differenced across the pass). The reference is const
    /// precisely so that "this scope cannot touch a counter" is a property of
    /// the type rather than a promise in a comment.
    QpWalkMinorScope(const WorkingSet &ws, const Vec &x, const QpCounters &counters, Index minor)
        : ws_(ws), counters_(counters), shift_adds_base_(counters.shift_adds),
          factorizations_base_(counters.factorizations),
          eqp_refine_base_(counters.eqp_refine_steps),
          border_refine_base_(counters.border_refine_steps),
          verdict_refine_base_(counters.verdict_refine_steps),
          schur_updates_base_(counters.schur_updates) {
        armed_ = QpWalkObserver::capturing;
        if (!armed_) {
            return;
        }
        rec_.solve = QpWalkObserver::solve_index;
        rec_.minor = minor;
        rec_.ws_size_in = qp_walk_ws_size(ws);
        rec_.ws_sig_in = qp_walk_ws_signature(ws);
        rec_.x_inf = x.size() > 0 ? x.lpNorm<Eigen::Infinity>() : 0.0;
        rec_.x_l1 = x.size() > 0 ? x.lpNorm<1>() : 0.0;
        QpWalkObserver::current = &rec_;
    }

    QpWalkMinorScope(const QpWalkMinorScope &) = delete;
    QpWalkMinorScope &operator=(const QpWalkMinorScope &) = delete;

    ~QpWalkMinorScope() {
        if (!armed_) {
            return;
        }
        QpWalkObserver::current = nullptr;
        rec_.shift_adds = counters_.shift_adds - shift_adds_base_;
        rec_.pass_factorizations = counters_.factorizations - factorizations_base_;
        rec_.pass_eqp_refine_steps = counters_.eqp_refine_steps - eqp_refine_base_;
        rec_.pass_border_refine_steps = counters_.border_refine_steps - border_refine_base_;
        rec_.pass_verdict_refine_steps = counters_.verdict_refine_steps - verdict_refine_base_;
        rec_.pass_schur_updates = counters_.schur_updates - schur_updates_base_;
        rec_.ws_size_out = qp_walk_ws_size(ws_);
        rec_.ws_sig_out = qp_walk_ws_signature(ws_);
        if (QpWalkObserver::records.size() >= QpWalkObserver::max_records) {
            QpWalkObserver::truncated = true;
            return;
        }
        // THE ONLY THROWING OPERATION IN THIS DESTRUCTOR, and it is caught
        // HERE rather than allowed to escape: a `push_back` that throws during
        // stack unwinding would call `std::terminate`, which would make the
        // observer capable of ending a solve that would otherwise have
        // finished. Swallowed and reported through `allocation_failed`, so the
        // seam's neutrality claim holds even when the allocator fails.
        try {
            QpWalkObserver::records.push_back(rec_);
        } catch (...) {
            QpWalkObserver::allocation_failed = true;
        }
    }

    QpWalkMinorRecord &record() { return rec_; }

    /// Copies the EQP candidate site's scratch into this pass's record. Called
    /// by the walk loop the instant ITS OWN `eqp_candidate` returns, so a later
    /// probe call on the same pass cannot overwrite what the main call said.
    void snapshot_eqp() {
        if (!armed_) {
            return;
        }
        const QpWalkEqpEvidence &e = QpWalkObserver::eqp;
        rec_.eqp_path = e.path;
        rec_.inertia_state = e.state;
        rec_.inertia_pos = e.n_pos;
        rec_.inertia_neg = e.n_neg;
        rec_.inertia_zero = e.n_zero;
        rec_.inertia_zero_derived = e.zero_is_derived;
        rec_.inertia_perturbed = e.perturbed_pivots;
        rec_.inertia_expected_pos = e.expected_pos;
        rec_.inertia_expected_neg = e.expected_neg;
        rec_.eqp_call_refine_steps = e.refine_steps;
        rec_.border_attempted = e.border_attempted;
        rec_.k0_rebuilt = e.k0_rebuilt;
        rec_.border_latched = e.latched;
        rec_.border_needs_refactorization = e.needs_refactorization;
        rec_.border_dim = e.border_dim;
    }

  private:
    const WorkingSet &ws_;
    const QpCounters &counters_;
    Index shift_adds_base_ = 0;
    Index factorizations_base_ = 0;
    Index eqp_refine_base_ = 0;
    Index border_refine_base_ = 0;
    Index verdict_refine_base_ = 0;
    Index schur_updates_base_ = 0;
    QpWalkMinorRecord rec_{};
    bool armed_ = false;
};

/// Writes `fn` into the record the current pass is filling, if there is one.
/// Every hook outside the loop body goes through this, so a hook can never
/// dereference a null `current`.
template <typename Fn> inline void qp_walk_note(Fn &&fn) {
    if (QpWalkObserver::current != nullptr) {
        fn(*QpWalkObserver::current);
    }
}

/// Writes `fn` into the EQP candidate site's SCRATCH. Gated on `capturing`
/// rather than on `current`, because the verdict site's own hooks run inside
/// calls that a pass has not yet claimed a record for.
template <typename Fn> inline void qp_walk_note_eqp(Fn &&fn) {
    if (QpWalkObserver::capturing) {
        fn(QpWalkObserver::eqp);
    }
}

/// Clears the scratch. The walk loop calls this immediately BEFORE its own
/// `eqp_candidate`, so a pass that reaches no verdict site records ABSENCE
/// rather than the previous pass's evidence.
inline void qp_walk_reset_eqp() {
    if (QpWalkObserver::capturing) {
        QpWalkObserver::eqp = QpWalkEqpEvidence{};
    }
}

/// Copies one factorization's own inertia evidence and the gate's expectation
/// into the scratch. Written as an explicit switch rather than a cast so the
/// trace's encoding does not depend on the declaration order of a backend enum
/// this file does not own. ABSENT stays `-1`: a state other than `kObserved`
/// carries no counts, and an optional perturbed-pivot count that is not present
/// (Accelerate) is never filled with a zero.
inline void qp_walk_note_inertia(const hven::linear::InertiaEvidence &e, Index expected_pos,
                                 Index expected_neg) {
    qp_walk_note_eqp([&](QpWalkEqpEvidence &s) {
        switch (e.state) {
        case hven::linear::InertiaEvidence::State::kObserved:
            s.state = QpWalkInertiaState::kObserved;
            s.n_pos = e.n_pos;
            s.n_neg = e.n_neg;
            s.n_zero = e.n_zero;
            break;
        case hven::linear::InertiaEvidence::State::kQueryFailed:
            s.state = QpWalkInertiaState::kQueryFailed;
            break;
        case hven::linear::InertiaEvidence::State::kUnavailable:
            s.state = QpWalkInertiaState::kUnavailable;
            break;
        }
        s.zero_is_derived = e.zero_is_derived;
        s.perturbed_pivots = e.perturbed_pivots.has_value() ? *e.perturbed_pivots : Index{-1};
        s.expected_pos = expected_pos;
        s.expected_neg = expected_neg;
    });
}

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
