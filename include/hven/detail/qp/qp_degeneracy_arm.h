// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// qp_degeneracy_arm.h — M7 W2 M5's DEGENERACY EXPERIMENT: two arms over the
// primal active-set walk's drop and add rules, plus a trace-only DIAGNOSTIC
// half, on the `HVEN_TESTING` test-seam convention docs/testing.md describes
// and CLAUDE.md §8 mandates ("use this convention for any new need rather than
// inventing another").
//
// IT IS AN EXPERIMENT, AND NOTHING HERE SHIPS. Both arms and the diagnostic
// are OFF by default; with everything off the walk executes its production
// code path VERBATIM, so a build that defines `HVEN_TESTING` and arms nothing
// is the production walk. Every declaration in this file is inside
// `#ifdef HVEN_TESTING`, so a production TU sees no line, no byte and no
// symbol of it, exactly as for `qp_walk_trace.h` and `qp_anti_cycling_arm.h`.
// `HVEN_TESTING` is defined ONLY target-wide on the standalone test
// executables that recompile the instrumented sources a second time and do NOT
// link `hven::hven`.
//
// WHY IT IS A SEPARATE FILE, AND INCLUDED FROM INSIDE `qp_walk_trace.h`. The
// same two reasons M4's `qp_anti_cycling_arm.h` gives: `qp_walk_trace.h`
// carries a RECORD-ONLY contract proved as a test, and an ARM is the opposite
// thing (armed, it is meant to change a decision), so the two live apart; and
// the include sits inside `qp_walk_trace.h`'s own `#ifdef HVEN_TESTING` block
// rather than in `qp_engine.h`, so no production translation unit gains a
// LINE. It is separate from M4's arm file because it is a separate experiment
// with a separate switch: M4's arms are pivot-RULE arms (which candidate, what
// step); these are DEGENERACY arms (whether a decision is admissible at all),
// and M5's legs arm them independently of M4's.
//
// ===========================================================================
// WHAT THE EXPERIMENT IS ABOUT
// ===========================================================================
//
// M7 W2 M4c's trace study classified 100 051 core drop→same-bound-re-add
// events in the ten shipped SQP default-path failures
// (`.superpowers/m7-w2-m4c-report.md`, §4.1):
//
//   * 68.5 % occur on a working set that is PROVABLY linearly dependent —
//     `me + |W| > n` after the drop, by up to +106 rows. On such a set the
//     multipliers are NOT UNIQUE, so `z_i < 0` on one of infinitely many
//     multiplier vectors carries no information about whether leaving bound
//     `i` improves anything. `qp_engine.h`'s own Degeneracy note says this in
//     advance (~:335) and so does its drop/direction note (~:1507).
//   * 2.5 % — all of YORKNET — are a FALSE DROP: `|λ| = 2⁻²⁵` against a
//     stationarity scale of 1.6e+11, i.e. 8.3e-04 ulps of the very gradient
//     terms `z` is a cancellation of. The drop clears the engine's bar only
//     because `drop_worst` tests `viol > opt_tol` ABSOLUTELY
//     (`src/qp/qp_engine.cpp`), while the same header's
//     `free_block_stationarity` / `free_block_is_stationary` test the same
//     class of residual RELATIVELY and say why in their own comment
//     (`qp_engine.h` ~:516: "THE THRESHOLD IS SCALED, unlike this file's other
//     (absolute) opt_tol tests: this one reads a GRADIENT RESIDUAL, which
//     scales with the objective").
//
// ARM A — SCALED DROP TOLERANCE, for the second of those. Under the arm
// `drop_worst`'s two absolute tests become `viol > opt_tol * scale`, where
// `scale` is EXACTLY the quantity `free_block_stationarity` already computed
// for this pass — shared through `QpDegeneracyArmState::stat_scale`, not
// re-derived. It is an INTERNAL-CONSISTENCY change, not an import: the form is
// the one `free_block_is_stationary` already uses, one function away in the
// same header. Papers: Gill, Murray & Wright, *Practical Optimization*
// (Academic Press, 1981) §8.3 on termination and dual tolerances at a
// degenerate vertex; Nocedal & Wright, *Numerical Optimization* 2nd ed. §16.5
// (degeneracy); Gill, Murray, Saunders & Wright, *Math. Prog.* 45 (1989) §2 on
// the role of tolerances at a degenerate vertex.
//
// ARM B — INDEPENDENCE-PRESERVING ADD, for the first. DESIGNED IN THIS FILE
// AND NOT YET IMPLEMENTED — see the RULE DRAFT below, which is the deliverable
// this dispatch stops at. Nothing in this file decides anything for arm B; its
// switch value exists so the diagnostic's columns and the arm's future hooks
// share one enum.
//
// NO DATA IS PERTURBED BY EITHER ARM. Arm A changes which candidates are
// ADMISSIBLE and touches no floating-point value of the problem. Arm B, as
// designed, changes which constraint enters the working set and touches no
// floating-point value of the problem either.
//
// ===========================================================================
// ARM B — THE RULE DRAFT (design only; no code below implements it)
// ===========================================================================
//
// --- B.0  HOW CAN A DEPENDENT CONSTRAINT BLOCK AT ALL? ----------------------
//
// The question has to be answered before the rule can be written, because in
// exact arithmetic it cannot happen. Let `A_W` be the active constraint matrix
// of the working set — the equality rows (always in the KKT), the working
// inequality rows, and one unit row `e_i` per pinned variable. The EQP
// candidate `eqp.x` satisfies the working set at equality and the iterate `x`
// sits on it, so `p = eqp.x − x` satisfies `A_W p = 0`; a candidate row `a`
// that is linearly DEPENDENT on `A_W` is `a = A_Wᵀ y` for some `y`, hence
// `aᵀp = yᵀ(A_W p) = 0` exactly, its ratio-test denominator is zero, and
// `kEngineDenomTol` screens it out. A dependent constraint CANNOT block.
//
// THREE THINGS BREAK THAT PREMISE IN THIS ENGINE, and the diagnostic measures
// all three:
//
//   (i) THE KKT IS REGULARIZED. `assemble_kkt` thickens the (1,1) block by
//       `primal_delta` and the constraint block by `−dual_mu`, so the system
//       actually solved has second block row `A_W p − μ λ = 0`, i.e.
//
//              A_W p = μ · λ_W        (NOT zero)
//
//       and therefore, for a dependent candidate `a = A_Wᵀ y`,
//
//              aᵀp = yᵀ (A_W p) = μ · yᵀ λ_W .
//
//       This is THE LEAK. With `dual_mu = 1e-8` and the multiplier magnitudes
//       M4c read off the failing cells (‖λ‖ up to ~4e+03 on ACOPP57), the leak
//       reaches ~1e-04 — four to eight orders of magnitude above
//       `kEngineDenomTol = 1e-12`, which is the only eligibility test the
//       ratio test applies. A dependent constraint is therefore fully eligible
//       to block, and the engine's Degeneracy note ("the delta/mu
//       regularization keeps the KKT matrix factorizable") is describing
//       exactly the mechanism that makes it so.
//
//  (ii) THE ITERATE IS NOT EXACTLY ON ITS WORKING SET. M4c measured
//       `active_res_inf / step_tol` medians of 1e+04 to 1e+09 on the failing
//       cells. `A_W x ≠ b_W`, so `A_W p = (b_W + μλ) − A_W x` carries that
//       residual on top of the leak.
//
// (iii) ROUNDING. Even with (i) and (ii) removed, `aᵀp` is a floating-point
//       dot product, so an exactly dependent `a` has `|aᵀp| ≈ ε‖a‖‖p‖`. On
//       BRITGAS solve 4, where M4c records `‖p‖∞ = 2.5e+10`, that floor alone
//       is ~1e-06 — again far above `kEngineDenomTol`.
//
// SO THE DIAGNOSTIC IS OBLIGED TO SAY WHICH ONE IT IS, and it does: per add it
// records the blocker's `aᵀp`, the leak bound `μ‖λ‖∞`, and the rounding floor
// `ε‖a‖₂‖p‖₂`. "The observed blocking is the leak" is then a comparison a
// reader makes from three recorded numbers, not a claim this file makes.
//
// --- B.1  THE INTEGER SCREEN ------------------------------------------------
//
// `me + |W| + 1 > n` ⇒ the set with the blocker added is linearly DEPENDENT by
// counting alone: `me + |W| + 1` rows in ℝⁿ. No factorization, no tolerance,
// no arithmetic beyond addition. SUFFICIENT, NOT NECESSARY — a set with
// `me + |W| + 1 ≤ n` can be dependent too — which is why B.2 exists.
//
// --- B.2  THE NUMERIC DEPENDENCY TEST, AND ITS TOLERANCE --------------------
//
// hven retains no QR or LQ factor of `A_W`; what it retains is the factor of
// the REGULARIZED KKT matrix
//
//        K = [ H + δI   A_Wᵀ ]
//            [ A_W     −μI  ]
//
// for the current working set — the very factor this pass's direction came
// out of. One BACK-SOLVE through it with the right-hand side `[a ; 0]` gives
// `[u ; v]` with
//
//        (H + δI) u + A_Wᵀ v = a ,        A_W u = μ v .
//
// Eliminating `u` gives `(μ(H + δI) + A_WᵀA_W) u = μ a`, so `‖u‖ = O(μ)` when
// `a ∈ range(A_Wᵀ)` and `O(1)` otherwise — and, in the dependent case,
// `A_Wᵀ v ≈ a`, i.e. **`v` IS the vector of dependency coefficients**. ONE
// solve therefore yields the verdict and the coefficients the exchange needs,
// which is why this is the form proposed rather than a fresh factorization of
// `A_W`.
//
// THE STATISTIC IS THE REPRESENTATION RESIDUAL, taken directly rather than
// inferred from `u`:
//
//        dep_residual(a) = ‖a − A_Wᵀ v‖∞ / ‖a‖∞
//
// — one sparse mat-vec on top of the solve, and a quantity with a meaning
// (`a` IS `A_Wᵀv` to this relative accuracy) rather than a proxy.
//
// THE TOLERANCE, DERIVED. The solve runs through a δ/μ-REGULARIZED factor, so
// even an exactly dependent `a` is reproduced only to within the perturbation
// that regularization introduces: `v` solves a perturbed system, and the
// residual it leaves is of order `μ‖v‖ / ‖a‖` plus `δ‖u‖ / ‖a‖` plus the
// factorization's own backward error. The engine already faces this exact
// problem one function away and already picked a number for it:
// `kRideNullspaceTol = 1e-8` ("the EQP is solved through a dual_mu-regularized
// KKT system, so even a consistent working row carries an irreducible
// dual_mu*|lambda| residual, while the cases this must REJECT miss by orders
// of magnitude more" — `qp_engine.h`). The proposal is the SAME number for the
// same reason, exposed as `dependency_tol` so a leg can move it, and the
// distribution of `dep_residual` is RECORDED by the diagnostic on every add so
// that the choice is checked against data instead of asserted: if the
// dependent and independent populations do not separate by orders of magnitude
// at `1e-8`, the arm is not entitled to its verdict and says so.
//
// --- B.3  THE DEFAULT ACTION: THE CLASSICAL EXCHANGE ------------------------
//
// On a DEPENDENT blocker the pre-registered action is the exchange of Gill,
// Murray & Wright, *Practical Optimization* (1981) ch. 5 (the working set is
// DEFINED there as a linearly independent subset of the active set, and a
// blocking constraint dependent on it is swapped for a member rather than
// added to it) and Fletcher, "Resolving degeneracy in quadratic programming",
// *Ann. OR* 46/47 (1993): identify the working-set member `j` with the largest
// `|v_j|` in the dependency `a ≈ Σ_j v_j a_j`, REMOVE `j`, ADD the blocker,
// and record the pair. Largest coefficient because that is the member the
// blocker most nearly reproduces, so the exchanged set is the best-conditioned
// of the admissible swaps.
//
// IF NO SUCH MEMBER EXISTS — the screen-only case, where `me + |W| + 1 > n`
// fired but the numeric test found no usable coefficient vector (for instance
// because the blocker is dependent on the EQUALITY rows, which the walk may
// not drop) — the action is to REFUSE the add and take the step, recording the
// blocker's post-step violation. That is the honest fallback: the walk keeps a
// linearly independent working set and the infeasibility it accepts is
// measured rather than hidden.
//
// --- B.4  THE IMPLEMENTABILITY QUESTION THE BRIEF ASKS ----------------------
//
// Two things must be true for the exchange to live inside the arm pattern
// without touching a production code path, and BOTH ARE OPEN AT THE TIME THIS
// FILE IS WRITTEN — which is why no code below implements the arm:
//
//   1. THE EXTRA SOLVE MUST BE STATE-NEUTRAL. On the ELIMINATED path the
//      retained factor is `kkt` and one `detail::solve_vec` through it is a
//      pure read. On the SCHUR-BORDER path — hven's DEFAULT `ws_algebra` — the
//      retained object is `K0` plus a live, INCREMENTALLY UPDATED Schur
//      complement, and a solve through it runs `solve_bordered_eqp`, which
//      touches refinement counters and may mark the stack for rebuild. The
//      arm may only proceed there if that solve can be shown to leave the
//      border stack bit-identical (counters saved and restored is necessary
//      and NOT sufficient).
//   2. THE EXCHANGE MUST NOT NEED A NEW ENGINE ENTRY POINT. Removing a member
//      and adding the blocker in the same pass is two `WorkingSet` mutations
//      the walk loop already performs separately, so the mutation itself is in
//      reach; what is not yet established is that the loop's own invariants
//      (`last_drop`, the probe's addition-sequence stamps, `refresh_shifts`'s
//      homotopy bookkeeping) survive a same-pass swap without a production
//      change.
//
// IF EITHER TURNS OUT TO BE FALSE, the brief's instruction is explicit: STOP
// and write why, and DO NOT build a different rule silently.
//
// ===========================================================================
// THE DIAGNOSTIC HALF (trace-only, both arms, OFF by default)
// ===========================================================================
//
// Three groups of NEW columns on `QpWalkMinorRecord`, and nothing removed or
// repurposed:
//
//   * PER DROP PASS — the scale the drop was judged against, the scaled
//     tolerance, how many candidates the scaled test refused and the largest
//     refused `|λ|/scale`; and the INTEGER dependency screen after the drop
//     (`me + |W|` against `n`), which costs an addition.
//   * PER DROP PASS, GATED — the POST-DROP inertia and rank evidence
//     `qp_walk_trace.h` deliberately omits ("would each need an extra
//     factorization, so they are deliberately NOT recorded"). That omission is
//     lifted here, under its own switch, at a DECLARED cost of ONE EXTRA
//     ASSEMBLE+FACTORIZE PER DROP PASS on a traced run. It is taken through a
//     LOCAL `KktFactor` and a LOCAL face with `ws_algebra` forced to
//     `kRefactorize`, so the engine's own `kkt`, face and border stack are not
//     touched at all, and the pass's counters are saved and restored around it
//     — which is what makes the diagnostic bit-neutral on EVERY algebra path
//     rather than only on the eliminated one.
//   * PER ADD — the blocker's `aᵀp`, the leak bound `μ‖λ‖∞`, the rounding
//     floor `ε‖a‖₂‖p‖₂`, and the integer screen before the add. B.0 is what
//     these are for. The NUMERIC dependency test's own columns are declared
//     and stay ABSENT until arm B lands; a column that reads absent is the
//     honest record of a test that did not run.
//
// ===========================================================================
// AND IT IS PER-THREAD, for the same reason `QpWalkObserver` and
// `QpAntiCycleArmState` are: an engine is used sequentially by one thread
// (`qp_engine.h`'s own contract), two instrumented solves on two threads must
// not share a switch, a scale or a counter, and process-global mutable state
// with no synchronization is a data race rather than an experiment. Arming,
// `reset()` and reading the counters are all per-thread.

#ifdef HVEN_TESTING

#include <cmath>
#include <limits>

#include <hven/core/types.h>

namespace hven::solvers::detail::testing {

/// Which degeneracy arm is armed on this thread. `kOff` is the default and is
/// production behaviour EXACTLY: every hook site in `src/qp/qp_engine.cpp`
/// runs the unmodified production statements under `kOff`, rather than a
/// generalized form specialized by a neutral parameter.
///
/// `kBoth` exists because M5's legs run `a+b` together on the same cells; it
/// is not a third rule.
enum class QpDegeneracyArm : int {
    kOff = 0,
    /// ARM A only — the scaled drop tolerance.
    kScaledDrop = 1,
    /// ARM B only — the independence-preserving add. NOT IMPLEMENTED; the
    /// value exists so the diagnostic and the future hooks share one enum.
    kIndependentAdd = 2,
    /// Both arms.
    kBoth = 3,
};

/// THE ARMS' PARAMETERS. Two, and each one is a number this file derives
/// rather than types: `opt_tol` is the engine's own and is not restated here.
struct QpDegeneracyParams {
    /// ARM B's relative dependency tolerance (§B.2). Defaulted to hven's own
    /// `kRideNullspaceTol`, whose derivation — an irreducible `dual_mu·|λ|`
    /// residual on a consistent working row, against rejected cases that miss
    /// by orders of magnitude more — is the same argument this test needs.
    /// The literal is repeated rather than referenced because `qp_engine.h`
    /// includes this file (through `qp_walk_trace.h`) ABOVE its own
    /// declaration of that constant.
    double dependency_tol = 1e-8;

    /// ARM B's integer screen, on or off, so a leg can separate the screen's
    /// effect from the numeric test's. TRUE is the briefed rule.
    bool integer_screen = true;
};

/// THE SWITCH, THE SHARED SCALE AND THE ARMS' PER-SOLVE STATE. Static and
/// thread_local, on `QpWalkObserver`'s conventions and for its reasons.
struct QpDegeneracyArmState {
    // ---- the switches -----------------------------------------------------
    static inline thread_local QpDegeneracyArm arm = QpDegeneracyArm::kOff;
    static inline thread_local QpDegeneracyParams params{};

    /// THE DIAGNOSTIC's own switch, independent of the arms: a leg may trace
    /// the post-drop inertia with no arm armed, or arm A with the extra
    /// factorization off. OFF by default because it costs one extra
    /// assemble+factorize per drop pass.
    static inline thread_local bool diag_post_drop_inertia = false;

    // ---- the SHARED stationarity scale ------------------------------------
    /// The `scale` output of `detail::free_block_stationarity` for the pass in
    /// progress, published by the walk loop's own residual hook — the SAME
    /// call whose residual the trace records as `stat_scale`, not a second
    /// derivation. NaN until a pass publishes one.
    ///
    /// WHY IT IS PUBLISHED RATHER THAN RECOMPUTED. The brief's instruction is
    /// to share the computation and not to re-derive a second scale, and there
    /// is a substantive reason beyond cost: two derivations can drift, and an
    /// arm whose threshold is "the quantity `free_block_stationarity`
    /// computes" must be exactly that quantity or the internal-consistency
    /// argument it rests on is gone.
    ///
    /// THE ONE APPROXIMATION IN IT, DECLARED. The hook publishes the scale
    /// computed at the pass's STARTING iterate `x`, while `drop_worst` runs
    /// after the KKT-point branch has snapped `x = eqp.x`. That branch is
    /// entered only when `‖p‖∞ ≤ step_tol`, so the two iterates differ by at
    /// most `step_tol = feas_tol·max(1,‖x‖∞)` and the scale — a max of norms
    /// floored at 1 — by at most `‖H‖·step_tol`. On the object this arm is
    /// aimed at (YORKNET: `|λ|/scale = 8.3e-04` ulps) the gap to the threshold
    /// is twelve orders of magnitude, so the approximation cannot change a
    /// verdict there; a leg on a cell where it could is not entitled to the
    /// arm's verdict without saying so.
    static inline thread_local double stat_scale = std::numeric_limits<double>::quiet_NaN();

    // ---- ARM A's counters, per solve --------------------------------------
    /// Drops the arm MADE: `drop_worst` calls that released a constraint with
    /// arm A armed.
    static inline thread_local Index scaled_drops_made = 0;
    /// CANDIDATES the scaled test refused that the absolute test would have
    /// admitted — counted per candidate, summed over passes.
    static inline thread_local Index scaled_candidates_refused = 0;
    /// Passes on which the scaled test emptied the candidate set that the
    /// absolute test would have left non-empty: the drop was not merely
    /// re-chosen, it was NOT MADE. This is the direct read-out of the arm's
    /// pre-registered prediction and is why it is counted separately from the
    /// candidate total.
    static inline thread_local Index scaled_drops_prevented = 0;
    /// The largest `|λ|/scale` among every refused candidate. Zero when
    /// nothing was refused — a refusal always has a positive ratio, so zero is
    /// unambiguous here and needs no NaN.
    static inline thread_local double scaled_max_refused_ratio = 0.0;
    /// Passes on which the arm was armed and NO scale had been published, so
    /// it fell back to `scale = 1` and reproduced production's absolute test.
    /// It should be zero on every run; a nonzero value invalidates that run's
    /// arm-A read-out and must be reported, not absorbed.
    static inline thread_local Index scale_unavailable_passes = 0;

    // ---- the DIAGNOSTIC's own counters ------------------------------------
    /// Extra factorizations the post-drop diagnostic performed — the cost, in
    /// the currency CLAUDE.md §7 asserts, rather than a wall-clock claim.
    static inline thread_local Index diag_factorizations = 0;
    /// Post-drop diagnostic attempts that THREW (an assembly or factorization
    /// failure on the hypothetical set). Caught and counted so a diagnostic
    /// can never end a solve that would otherwise have finished.
    static inline thread_local Index diag_factorization_failures = 0;

    static bool scaled_drop_on() {
        return arm == QpDegeneracyArm::kScaledDrop || arm == QpDegeneracyArm::kBoth;
    }
    static bool independent_add_on() {
        return arm == QpDegeneracyArm::kIndependentAdd || arm == QpDegeneracyArm::kBoth;
    }
    static bool any_on() { return arm != QpDegeneracyArm::kOff; }

    /// Full reset, INCLUDING the switches and the parameters. A fixture calls
    /// this between cases.
    static void reset() {
        arm = QpDegeneracyArm::kOff;
        params = QpDegeneracyParams{};
        diag_post_drop_inertia = false;
        reset_counters();
    }

    static void reset_counters() {
        stat_scale = std::numeric_limits<double>::quiet_NaN();
        scaled_drops_made = 0;
        scaled_candidates_refused = 0;
        scaled_drops_prevented = 0;
        scaled_max_refused_ratio = 0.0;
        scale_unavailable_passes = 0;
        diag_factorizations = 0;
        diag_factorization_failures = 0;
    }

    /// Called once per `QpEngine::run`, before the walk loop. Clears the
    /// published scale so a solve can never judge its first drop against the
    /// PREVIOUS solve's scale, and leaves the switches, the parameters and the
    /// counters alone — a caller measuring a whole SQP solve wants the totals
    /// over every subproblem, and a fixture that wants one solve's numbers
    /// calls `reset_counters()` itself.
    static void begin_solve() { stat_scale = std::numeric_limits<double>::quiet_NaN(); }

    /// Publishes the pass's stationarity scale. Called from the walk loop's
    /// own residual hook with the value `free_block_stationarity` just
    /// returned through its `scale` out-parameter.
    static void publish_scale(double s) { stat_scale = s; }

    /// THE SCALE ARM A MULTIPLIES `opt_tol` BY, with its fallback made
    /// explicit and COUNTED. `free_block_stationarity` floors its scale at 1,
    /// so a published value is always `>= 1`; anything else (NaN, or a value
    /// below 1) means no pass published one and the arm degrades to
    /// production's absolute test rather than judging against a number it
    /// cannot account for.
    static double drop_scale() {
        const double s = stat_scale;
        if (!(s >= 1.0)) {
            ++scale_unavailable_passes;
            return 1.0;
        }
        return s;
    }

    /// ARM A's ADMISSIBILITY TEST AND ITS COUNTING, IN ONE FUNCTION, so the
    /// two call sites in `drop_worst` — the inequality-row sweep and the bound
    /// sweep — cannot diverge, and so the count and the decision cannot
    /// disagree. `scaled_tol` is `opt_tol * scale`, formed by the caller from
    /// the engine's own `opt_tol` (which this file deliberately does not
    /// restate) and `drop_scale()`.
    ///
    /// It is asked ONLY on candidates the ABSOLUTE test already admitted, so
    /// every FALSE it returns is a candidate production would have kept, and
    /// the refusal is counted here rather than at either call site.
    static bool admits(double violation, double scaled_tol, double scale) {
        if (violation > scaled_tol) {
            return true;
        }
        ++scaled_candidates_refused;
        const double ratio = scale > 0.0 ? violation / scale : violation;
        if (ratio > scaled_max_refused_ratio) {
            scaled_max_refused_ratio = ratio;
        }
        return false;
    }
};

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
