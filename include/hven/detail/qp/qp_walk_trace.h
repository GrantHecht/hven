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
// IT IS RECORD-ONLY. Nothing here mutates the walk: no decision, no counter, no
// floating-point operation of the solver's own arithmetic. The norms and the
// signature it computes are computed from state the loop already holds, into
// storage the loop never reads. `tests/qp/test_qp_walk_seam.cpp` proves that as
// a TEST rather than as a claim: with the observer attached, every `QpCounters`
// field and the solution the engine returns are bit-identical to the unattached
// run on three fixtures, one of them degenerate.
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
// move that immediate and cost the production Debug object its BYTE identity —
// a difference P-SYM would correctly classify as `__LINE__`-class noise, but
// one that would make "the production object is byte-identical" untrue in one
// configuration and true in the other. Including from the header this TU
// already includes adds no line to the TU at all, so the byte-identity claim is
// exact in BOTH configurations. `qp_engine.h` itself contains no `assert`, no
// `throw` and no other `__LINE__`-bearing construct, so nothing shifts there
// either. The cost of the wider inclusion is zero by construction: without
// `HVEN_TESTING` this file's entire body is `#ifdef`-ed away.

#ifdef HVEN_TESTING

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/types.h>
#include <hven/detail/qp/working_set.h>
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
};

/// THE OBSERVER. Static, like every other observer on this seam, because the
/// engine is reached through a deep call chain and a fixture needs to read what
/// it recorded after the solve returns.
struct QpWalkObserver {
    static inline bool active = false;

    /// How many `QpEngine::run` calls have begun since `reset()`.
    static inline Index solves = 0;
    /// Which solve to record; `-1` records every one. M7 W2 M3b's measurement
    /// wants the FIRST subproblem of a driver solve and nothing else.
    static inline Index capture_solve = -1;
    /// A hard ceiling on `records`, so an observer left armed over a long run
    /// cannot grow without bound. `truncated` says whether it bit.
    static inline std::size_t max_records = 1000000;
    static inline bool truncated = false;

    static inline std::vector<QpWalkMinorRecord> records;

    // ---- per-solve, for the solve being recorded ---------------------------
    /// True while a recorded solve is in progress.
    static inline bool capturing = false;
    /// The 0-based index of the solve the fields below describe.
    static inline Index solve_index = -1;
    /// `QpEngine::run`'s own `warm` argument.
    static inline bool warm = false;
    static inline Index n = 0;
    static inline Index me = 0;
    static inline Index mi = 0;
    /// `detail::effective_qp_max_iter`'s resolved cap for this solve.
    static inline Index max_iter = 0;
    /// The INCOMING working set — after the seed ingest and the seeding
    /// `refresh_shifts`, i.e. exactly the set the walk's first pass starts from.
    static inline std::uint64_t incoming_ws_signature = 0;
    static inline Index incoming_ws_size = 0;
    /// The status the recorded solve's walk loop left (`kTerminal`'s verdict,
    /// or `kMaxIter` when the cap was reached). `-1` until the loop ends.
    static inline int exit_status = -1;

    /// The record the current pass is filling, or `nullptr`. The hooks in
    /// `drop_worst`, `ratio_test`, `probe_zero_multiplier_drops` and
    /// `ride_negative_curvature` write through this rather than being handed a
    /// record, because those functions' signatures are production surface and
    /// this seam does not touch them.
    static inline QpWalkMinorRecord *current = nullptr;

    static void reset() {
        active = false;
        solves = 0;
        capture_solve = -1;
        max_records = 1000000;
        truncated = false;
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
        : ws_(ws), counters_(counters), shift_adds_base_(counters.shift_adds) {
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
        rec_.ws_size_out = qp_walk_ws_size(ws_);
        rec_.ws_sig_out = qp_walk_ws_signature(ws_);
        if (QpWalkObserver::records.size() >= QpWalkObserver::max_records) {
            QpWalkObserver::truncated = true;
            return;
        }
        QpWalkObserver::records.push_back(rec_);
    }

    QpWalkMinorRecord &record() { return rec_; }

  private:
    const WorkingSet &ws_;
    const QpCounters &counters_;
    Index shift_adds_base_ = 0;
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

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
