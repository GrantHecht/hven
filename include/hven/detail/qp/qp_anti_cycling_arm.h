// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_anti_cycling_arm.h — M7 W2 M4's ANTI-CYCLING EXPERIMENT: two arms over the
// primal active-set walk's pivot rules, on the `HVEN_TESTING` test-seam
// convention docs/testing.md describes and CLAUDE.md §8 mandates ("use this
// convention for any new need rather than inventing another").
//
// IT IS AN EXPERIMENT, AND NOTHING HERE SHIPS. Both arms are OFF by default;
// with the switch OFF the walk executes its production code path VERBATIM, so
// a build that defines `HVEN_TESTING` and never arms an arm is the production
// walk. Every declaration in this file is inside `#ifdef HVEN_TESTING`, so a
// production TU sees no line, no byte and no symbol of it, exactly as for
// `qp_walk_trace.h`. `HVEN_TESTING` is defined ONLY target-wide on the
// standalone test executables that recompile the instrumented sources a second
// time and do NOT link `hven::hven`.
//
// WHY IT IS A SEPARATE FILE FROM `qp_walk_trace.h`, AND INCLUDED FROM INSIDE
// IT. `qp_walk_trace.h` carries a RECORD-ONLY contract ("nothing here mutates
// the walk"), proved as a test. An arm is the opposite thing: armed, it is
// meant to change a decision. Keeping the two in separate files keeps that
// contract readable and keeps the observer's proof about the observer. The
// include sits inside `qp_walk_trace.h`'s own `#ifdef HVEN_TESTING` block
// rather than in `qp_engine.h`, so no production translation unit gains a
// LINE: adding one to `qp_engine.h` would move the DWARF line programme of
// every inline function below it in the three production objects that include
// it, and there is no reason to pay that.
//
// ---------------------------------------------------------------------------
// WHAT THE TWO ARMS ARE
// ---------------------------------------------------------------------------
//
// M7 W2 M3c's default-path census found that 19 of 21 traced capped QP solves
// end in an exactly periodic, ZERO-MOTION drop/re-add tail of ONE bound
// (period 2, sometimes 4), and that 92.9 % of all minors in those solves are
// spent after the walk has begun repeating itself. Its review located the
// mechanism as classical degenerate active-set cycling instantiated by hven's
// own conventions: `drop_worst` picks by Dantzig violation, then angle, then
// first candidate; the post-drop direction points back through the released
// bound while `x` is still exactly on it, so the lower-bound ratio is a SIGNED
// ZERO; and the add path re-pins `x` exactly onto that same bound.
//
// ARM E — EXPAND. Gill, Murray, Saunders and Wright, "A practical anti-cycling
// procedure for linearly constrained optimization", Mathematical Programming
// 45 (1989) 437-474. Every rule this file transcribes carries the paper's own
// section or equation number at its implementation site in
// `src/qp/qp_engine.cpp` and again in `QpAntiCycleParams` below. The three
// pieces are: a WORKING feasibility tolerance that grows by a fixed increment
// at the start of every minor (§4.2, and (4.2) for the increment itself); the
// two-pass ratio test — a relaxed first pass (Harris, (3.2)) followed by a
// LARGEST-PIVOT second pass over the exact steps that do not exceed it
// ((3.3a)/(3.3b)) — with a positive floor `alpha_min = tau/|p_r|` on the step
// (§4.1's pseudo-code, and §8.1's `alpha_blk = max{alpha_min, alpha_2}` for
// the linearly-constrained/QP context this walk is); and a RESET that
// re-projects the iterate onto its working bounds and restarts the tolerance
// when the expanding sequence reaches its length K (§4.3).
//
// WHAT BREAKS THE CYCLE HERE, STATED HONESTLY. In the simplex setting EXPAND's
// anti-cycling guarantee comes from `alpha >= alpha_min > 0`, which makes the
// objective strictly decrease (§4.4). That argument does NOT transfer verbatim
// to this walk: hven re-solves the EQP over the working set every minor and
// SNAPS the iterate onto that face's minimizer whenever the computed direction
// is negligible, so the walk's state is a function of the working set alone
// and a repeated working set reproduces a repeated state no matter how far the
// iterate wandered in between. In this walk the operative part of ARM E is the
// pair (3.2)/(3.3a): at a degenerate vertex several constraints have an EXACT
// step of zero, the relaxed first pass admits all of them (and any within
// `delta/|p_j|` beyond), and the second pass picks the one with the LARGEST
// PIVOT rather than the lowest index -- a different constraint enters the
// working set, so the set does not return to where it was. `alpha_min` and the
// expansion are carried faithfully because they are the paper's procedure and
// because the measurement is entitled to see what they do; the claim made for
// them here is not the paper's finiteness theorem.
//
// ARM L — LEXICOGRAPHIC TIE-BREAK. Bland, "New finite pivoting rules for the
// simplex method", Mathematics of Operations Research 2 (1977) 103-107 (the
// paper; reference [4] of the EXPAND paper above). Bland's rule fixes ONE
// total order on the constraint index set and, at every pivot, takes the
// smallest index that is eligible -- for the entering choice AND for the
// leaving choice. The classical argument is that a cycle would have to
// re-enter its own largest-index constraint, which the least-index rule
// forbids; the order may be ANY fixed order, which is what licenses the single
// order over two different index spaces (inequality rows and variable bounds)
// used below.
//
// WHAT THIS ARM IMPLEMENTS, AND THE GAP TO BLAND'S THEOREM, DECLARED. The arm
// as specified for this experiment is a TIE-BREAK: lexicographic order
// (index, then side, then block) among `drop_worst`'s Dantzig tie window and
// among the ratio test's tied minimum. That is strictly weaker than Bland's
// rule, whose finiteness needs the least-index choice over the WHOLE eligible
// pricing set, not merely over the set that ties for the Dantzig maximum.
// `lex_full_pricing` (default FALSE, i.e. the tie-break form) extends the drop
// rule to the full eligible set and is the form the classical argument
// actually covers. Which form ran is recorded per leg; no result below is
// reported as a finiteness guarantee.
//
// NO DATA IS PERTURBED BY EITHER ARM. ARM L reorders decisions and touches no
// floating-point value of the problem. ARM E changes the ratio test's
// arithmetic and the step, which is the point of it, and perturbs no entry of
// H, A, g, b, l or u.
//
// ---------------------------------------------------------------------------
// AND IT IS PER-THREAD, for the same reason `QpWalkObserver` is: an engine is
// used sequentially by one thread (`qp_engine.h`'s own contract), two
// instrumented solves on two threads must not share a switch, a tolerance or a
// counter, and process-global mutable state with no synchronization is a data
// race rather than an experiment. Arming, `reset()` and reading the counters
// are all per-thread.

#ifdef HVEN_TESTING

#include <hven/core/types.h>

namespace hven::solvers::detail::testing {

/// Which arm is armed on this thread. `kOff` is the default and is production
/// behaviour EXACTLY: every hook site in `src/qp/qp_engine.cpp` runs the
/// unmodified production statements under `kOff`, rather than a generalized
/// form specialized by a zero parameter.
enum class QpAntiCycleArm : int { kOff = 0, kExpand = 1, kLexicographic = 2 };

/// ARM E's parameters, every one of them the paper's own, with the paper's
/// own recommendation as the default where the paper gives one.
struct QpAntiCycleParams {
    /// §4.2, "the 'master' feasibility tolerance" delta_f. The paper derives
    /// delta_f = eps^(3/8) and reports 1e-6 for eps = 1e-16. hven's own
    /// `QpOptions::feas_tol` defaults to 1e-9, and a WORKING tolerance larger
    /// than the engine's structural-violation tolerance would let the walk
    /// certify points the engine's own verdict calls infeasible, so the
    /// DEFAULT HERE IS hven's feas_tol default and NOT the paper's 1e-6. The
    /// deviation is deliberate, is stated, and the value is a parameter: a
    /// measurement that wants the paper's number sets it.
    double delta_master = 1e-9;
    /// §4.2, delta_0 = 0.5 * delta_f: "the feasibility tolerance used to
    /// initiate an expanding sequence".
    double delta_0_factor = 0.5;
    /// §4.2, delta_K = 0.99 * delta_f: "the maximum feasibility tolerance
    /// during an expanding sequence".
    double delta_max_factor = 0.99;
    /// §4.2, K = eps^(-1/4): "the maximum number of simplex iterations allowed
    /// before resetting". The paper reports K = 10000 for eps = 1e-16.
    Index expand_k = 10000;
    /// §4.5's `t`: the tolerance below which a component of `p` is
    /// "negligible" and the constraint cannot block. hven's walk already has
    /// one -- `detail::kEngineDenomTol` -- and ARM E uses THAT rather than a
    /// second one, so the set of constraints the two arms can block on is the
    /// production set. This field records the choice; it is not read.
    double negligible_pivot = 1e-12;
    /// §4.3 step (1)'s "nontrivial" adjustment threshold, eps^(2/3) ~ 3.7e-11,
    /// used only to COUNT how many re-projections at a reset actually moved
    /// something. Never a decision.
    double reset_nontrivial = 3.7e-11;

    /// ARM L only. FALSE (the default) is the briefed TIE-BREAK form: the
    /// lexicographic order decides only among `drop_worst`'s Dantzig tie
    /// window. TRUE is Bland's rule proper: the least-index candidate over the
    /// whole eligible set, which is the form the classical finiteness argument
    /// covers. Recorded per leg.
    bool lex_full_pricing = false;

    /// §4.2's tau = (delta_K - delta_0)/K, derived rather than typed.
    double tau() const {
        const double d0 = delta_0_factor * delta_master;
        const double dk = delta_max_factor * delta_master;
        return (dk - d0) / static_cast<double>(expand_k > 0 ? expand_k : 1);
    }
    double delta_0() const { return delta_0_factor * delta_master; }
    double delta_max() const { return delta_max_factor * delta_master; }
};

/// THE SWITCH AND THE ARMS' PER-SOLVE STATE. Static and thread_local, on
/// `QpWalkObserver`'s conventions and for its reasons.
struct QpAntiCycleArmState {
    // ---- the switch -------------------------------------------------------
    static inline thread_local QpAntiCycleArm arm = QpAntiCycleArm::kOff;
    static inline thread_local QpAntiCycleParams params{};

    // ---- ARM E's per-solve state -----------------------------------------
    /// §4.2's delta_k, the working feasibility tolerance of the minor in
    /// progress. Zero when no arm is running.
    static inline thread_local double delta = 0.0;
    /// How many minors the expanding sequence in progress has run (§4.2's k,
    /// counted since the last reset).
    static inline thread_local Index expand_iter = 0;
    /// Whether the step the LAST ratio test returned was the `alpha_min`
    /// FLOOR rather than `alpha_2` -- §4.1's Cases 2 and 3, in which the
    /// blocking constraint deliberately ends the minor violating its bound by
    /// at most tau and must NOT be moved back onto it (§3.3: doing so is an
    /// extra, unmodelled step of size up to delta). Written by the ratio test,
    /// read by the walk loop's add path, and meaningless when no arm is on.
    static inline thread_local bool last_step_was_floor = false;

    // ---- observation only: what each arm DID, per solve -------------------
    /// §4.3 resets performed.
    static inline thread_local Index resets = 0;
    /// Variables a reset moved back onto a bound by more than
    /// `reset_nontrivial` (§4.3 step (1)'s "nontrivial" count).
    static inline thread_local Index reset_nontrivial_moves = 0;
    /// Minors whose step was the `alpha_min` FLOOR rather than `alpha_2`
    /// (§4.1's Cases 2 and 3).
    static inline thread_local Index min_steps = 0;
    /// Minors on which the largest-pivot second pass ((3.3a)) selected a
    /// DIFFERENT constraint from the one production's first-candidate minimum
    /// ratio would have selected. This is the arm's own count of how often it
    /// actually changed the add.
    static inline thread_local Index pivot_switches = 0;
    /// Minors on which the blocking constraint was left violating its bound
    /// (§4.1 Cases 2/3: the exact re-pin is skipped when the step was the
    /// floor).
    static inline thread_local Index unpinned_adds = 0;
    /// ARM L: drops and adds the lexicographic order moved off production's
    /// choice.
    static inline thread_local Index lex_drop_switches = 0;
    static inline thread_local Index lex_ratio_switches = 0;

    static bool expand_on() { return arm == QpAntiCycleArm::kExpand; }
    static bool lex_on() { return arm == QpAntiCycleArm::kLexicographic; }
    static bool any_on() { return arm != QpAntiCycleArm::kOff; }

    /// Full reset, INCLUDING the switch and the parameters. A fixture calls
    /// this between cases.
    static void reset() {
        arm = QpAntiCycleArm::kOff;
        params = QpAntiCycleParams{};
        reset_counters();
    }

    static void reset_counters() {
        delta = 0.0;
        expand_iter = 0;
        last_step_was_floor = false;
        resets = 0;
        reset_nontrivial_moves = 0;
        min_steps = 0;
        pivot_switches = 0;
        unpinned_adds = 0;
        lex_drop_switches = 0;
        lex_ratio_switches = 0;
    }

    /// Called once per `QpEngine::run`, before the walk loop. Starts a fresh
    /// expanding sequence (§4.2: delta_1 = delta_0 + tau at the first minor)
    /// and leaves the switch and the parameters alone. Counters are NOT
    /// cleared here: a caller measuring a whole SQP solve wants the totals
    /// over every subproblem, and a fixture that wants one solve's numbers
    /// calls `reset_counters()` itself.
    static void begin_solve() {
        delta = any_on() ? params.delta_0() : 0.0;
        expand_iter = 0;
    }

    /// §4.2's per-iteration expansion, called at the top of every minor:
    /// `delta_k <- delta_{k-1} + tau` ((4.2)). Returns TRUE when the
    /// expanding sequence has reached its length K and the caller must run
    /// §4.3's reset.
    static bool begin_minor() {
        if (!expand_on()) {
            return false;
        }
        ++expand_iter;
        delta += params.tau();
        if (expand_iter >= params.expand_k || delta >= params.delta_max()) {
            return true;
        }
        return false;
    }

    /// §4.3 step (3): "the feasibility tolerance is reinitialized to delta_0,
    /// and a new expanding sequence begins."
    static void note_reset() {
        delta = params.delta_0();
        expand_iter = 0;
        ++resets;
    }

    /// A FIXED TOTAL ORDER on the constraint index set, for ARM L. Bland's
    /// argument needs only that the order be fixed; it does not care which
    /// one. This one is INDEX-MAJOR -- the brief's "(index, then side)" --
    /// which is deliberately NOT production's order (production scans every
    /// inequality row before every bound, so it is BLOCK-major). A rule that
    /// reproduced production's order would be a rule that changes nothing.
    ///
    /// `is_ineq` distinguishes the two index spaces at equal index; `side` is
    /// 0 for a lower bound and 1 for an upper bound (and 0 for a row, which
    /// has no side). Smaller is preferred.
    struct LexKey {
        Index index = 0;
        int side = 0;
        int block = 0; // 0 = inequality row, 1 = bound

        bool operator<(const LexKey &o) const {
            if (index != o.index) {
                return index < o.index;
            }
            if (side != o.side) {
                return side < o.side;
            }
            return block < o.block;
        }
    };
};

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
