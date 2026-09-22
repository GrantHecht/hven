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
// ARM B — INDEPENDENCE-PRESERVING ADD, for the first. DESIGNED in the RULE
// DRAFT below and BUILT, as the settler's ruling R3 scopes it, in ONE of its
// two forms:
//
//   * b1, THE INDEPENDENCE-PRESERVING RATIO TEST — built. Candidates in ratio
//     order; the first the dependency test calls INDEPENDENT blocks; every
//     dependent candidate ahead of it is passed over and recorded; if none is
//     independent, nothing is admitted and the walk takes the full step. See
//     the ARM B1 block below the state for the rule, its cost and its
//     self-check.
//   * b2, THE CLASSICAL EXCHANGE (B.3) — NOT built, and REGISTERED as a
//     production design item with its three reasons written out in the ARM B2
//     block at the end of this file. R3 made it conditional on being
//     implementable inside the arm pattern; it is not.
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
// --- B.3  THE DEFAULT ACTION AS FIRST DESIGNED: THE CLASSICAL EXCHANGE ------
//
// SUPERSEDED BY SETTLER RULING R3, WHICH MADE b1 THE PRIMARY ARM AND b2 THE
// CONDITIONAL ONE. What follows is the exchange as it was designed; the ARM B2
// block at the end of this file records why the condition does not hold and
// what was registered instead. It is kept rather than deleted because the
// registered design item IS this text.
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
// ANSWERED, FOR BOTH ARMS. (1) is answered for b1 by NOT using the retained
// factor at all: the dependency test runs on a LOCAL eliminated-form factor
// built for the pass, so no algebra path's retained state is read or written,
// at a declared cost of one assemble+factorize per pass instead of one
// triangular solve. (2) is answered for b2 in the negative — see the ARM B2
// block at the end of this file. The two questions as first posed:
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

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include <hven/core/types.h>
#include <hven/detail/kkt/kkt_assembly.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>
#include <hven/qp/qp_types.h>

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
    /// ARM B only — the independence-preserving add, in its b1 form (the
    /// ratio test). b2, the exchange, is registered and not built.
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

    /// THE RATIO TEST'S OWN ELIGIBILITY CONSTANTS, so that arm b1's candidate
    /// enumeration considers EXACTLY the constraints `QpEngine::ratio_test`
    /// considers and no others. They are the engine's `kEngineDenomTol`,
    /// `kEngineInfBound` and `kEngineStepTieTol`, and the literals are
    /// repeated here rather than referenced for the same reason
    /// `dependency_tol`'s is: `qp_engine.h` includes this file (through
    /// `qp_walk_trace.h`) ABOVE its own declarations of them. They are TIED to
    /// the engine's own constants by `static_assert`s in `src/qp/qp_engine.cpp`
    /// beside M4's, where both are visible, so an edit to either one fails the
    /// build rather than silently giving the arm a different candidate set.
    double negligible_pivot = 1e-12;
    double inf_bound = 1e20;
    double step_tie_tol = 1e-9;

    /// A CEILING on how many candidates ONE pass may put through the
    /// dependency test. `0` is UNBOUNDED, and unbounded is the pre-registered
    /// rule (settler ruling R3/R4: candidates in ratio order until an
    /// independent one is found). The ceiling exists so a leg that meets a
    /// vertex with thousands of tied blockers can bound its own budget
    /// DECLAREDLY -- a truncation is counted
    /// (`indep_budget_truncations`), recorded in the trace, and the
    /// first unexamined candidate is then taken UNREFUSED, so truncating can
    /// only move the arm back toward production, never further from it.
    Index max_dep_tests_per_pass = 0;
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

    // ---- ARM B1's counters, per solve -------------------------------------
    /// Passes on which b1 examined the ratio test's candidates at all.
    static inline thread_local Index indep_passes = 0;
    /// Candidates put through the dependency test, summed over passes.
    static inline thread_local Index indep_candidates_examined = 0;
    /// Candidates the test called DEPENDENT and the arm therefore PASSED OVER.
    static inline thread_local Index indep_candidates_passed_over = 0;
    /// Back-solves taken (one per candidate whose numeric test ran).
    static inline thread_local Index indep_dep_solves = 0;
    /// Local factorizations built for the dependency test -- the cost, in the
    /// currency CLAUDE.md section 7 asserts. At most ONE per pass, and none at
    /// all on a pass whose candidates never reach the numeric test.
    static inline thread_local Index indep_factorizations = 0;
    /// Assemblies, factorizations or back-solves that THREW. Counted, never
    /// absorbed: a candidate whose test could not be taken is left
    /// UNREFUSED, so a backend fault can only move the arm back toward
    /// production.
    static inline thread_local Index indep_test_failures = 0;
    /// Dependent verdicts by the INTEGER screen and by the NUMERIC test,
    /// counted apart so a leg can say which one is doing the work.
    static inline thread_local Index indep_screen_dependent = 0;
    static inline thread_local Index indep_numeric_dependent = 0;
    /// Passes on which the arm MOVED the blocker off production's choice.
    static inline thread_local Index indep_blocker_switched = 0;
    /// Passes on which EVERY candidate was dependent, so the arm admitted
    /// nothing and the walk took the full step.
    static inline thread_local Index indep_no_blocker_passes = 0;
    /// Passes truncated by `max_dep_tests_per_pass`.
    static inline thread_local Index indep_budget_truncations = 0;
    /// THE ARM'S OWN SELF-CHECK, and a number that MUST be zero for a run's
    /// b1 read-out to mean anything. The arm re-enumerates the ratio test's
    /// candidates in production's own scan order and its FIRST candidate must
    /// be the one production's `ratio_test` just picked. A mismatch means the
    /// two enumerations disagree, so the arm REFUSES to act on that pass
    /// (production's choice stands, bit for bit) and counts it here.
    static inline thread_local Index indep_enumeration_mismatches = 0;
    /// THE HARD INVARIANT (settler ruling R6): `me + |W| - n` after every add
    /// b1 made. A POSITIVE value stops the arm. The max is kept, and
    /// `Index` min means "no add was made under the arm".
    static inline thread_local Index indep_rows_minus_n_max = std::numeric_limits<Index>::min();
    /// Adds under the arm whose post-add `me + |W| - n` was positive.
    static inline thread_local Index indep_invariant_violations = 0;
    /// The largest post-step violation over every constraint the arm passed
    /// over, measured at the iterate the walk actually moved to.
    static inline thread_local double indep_max_passed_violation = 0.0;

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
        indep_passes = 0;
        indep_candidates_examined = 0;
        indep_candidates_passed_over = 0;
        indep_dep_solves = 0;
        indep_factorizations = 0;
        indep_test_failures = 0;
        indep_screen_dependent = 0;
        indep_numeric_dependent = 0;
        indep_blocker_switched = 0;
        indep_no_blocker_passes = 0;
        indep_budget_truncations = 0;
        indep_enumeration_mismatches = 0;
        indep_rows_minus_n_max = std::numeric_limits<Index>::min();
        indep_invariant_violations = 0;
        indep_max_passed_violation = 0.0;
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

// ===========================================================================
// ARM B1 -- THE INDEPENDENCE-PRESERVING RATIO TEST (settler ruling R3)
// ===========================================================================
//
// The rule, in one sentence: take the ratio test's candidates IN RATIO ORDER
// and admit the FIRST one the dependency test calls INDEPENDENT, passing over
// every dependent candidate ahead of it; if no candidate is independent,
// admit nothing and take the full step, recording what the passed-over
// constraints are violating at the point the walk moved to.
//
// WHY THIS AND NOT THE EXCHANGE (B.3). See this file's B.4 and the arm-b2
// note at the end of this block: the exchange is registered as a production
// design item and is NOT built, because it cannot be done inside the arm
// pattern.
//
// WHAT THE ARM MAY AND MAY NOT TOUCH. It re-decides WHICH constraint blocks
// and, with it, the step. It touches no floating-point datum of the problem,
// no working-set mutation the loop does not already perform, and no engine
// state at all: the dependency test runs on a LOCAL `KktFactor` handed in by
// the caller (the one the post-drop inertia diagnostic owns), never on the
// walk's own `kkt` and never on the live Schur border stack. When the arm
// passes nothing over, the caller leaves production's blocker and production's
// step untouched, so ARM-ON-BUT-NOTHING-DEPENDENT IS BIT-IDENTICAL TO
// PRODUCTION BY CONSTRUCTION rather than by arithmetic that happens to agree.
//
// THE COST, DECLARED (settler ruling R2). ONE assemble+factorize per pass on
// which at least one candidate reaches the numeric test, plus ONE back-solve
// and one sparse gather per candidate examined. That is a heavier cost class
// than the "one triangular solve against the RETAINED factor" B.2 describes,
// and the reason is B.4's first open question: on the SCHUR-BORDER path --
// hven's default `ws_algebra` -- the retained object is a live, incrementally
// updated Schur complement, and a solve through it is not a pure read. A local
// factor is what makes the test path-independent and state-neutral on EVERY
// algebra path. The production cost target stays the retained-factor solve and
// is registered as a design item, not built here. Wall-clock is informational
// (CLAUDE.md section 7); the counters above are the asserted currency.

/// ONE candidate of arm b1's ratio-ordered scan, carrying its own verdict.
struct QpB1Candidate {
    bool is_ineq = false;
    Index idx = -1;
    /// The side a BOUND candidate would be pinned at; `kFree` for a row.
    BoundState state = BoundState::kFree;
    /// PRODUCTION'S OWN RATIO for this candidate, clamps included -- the
    /// expression `QpEngine::ratio_test` computes, transcribed.
    double step = 0.0;
    /// `aᵀp`: `Ai.row(idx)·p` for a row, `p(idx)` for a bound (`a = e_i`).
    double a_dot_p = 0.0;
    /// `-1` not tested / `0` independent / `1` dependent.
    int verdict = -1;
    /// `‖a_F − C_Fᵀ v‖∞ / ‖a_F‖∞` from the back-solve (B.2). NaN when the
    /// numeric test did not run.
    double dep_residual = std::numeric_limits<double>::quiet_NaN();
    /// True when the INTEGER screen fired for this candidate's set.
    bool by_screen = false;
};

/// WHAT ONE PASS OF ARM B1 DECIDED.
struct QpB1Outcome {
    /// The arm ran and its self-check passed. `false` means production's
    /// choice stands untouched.
    bool ran = false;
    /// The arm MOVED the choice. When this is false the caller must change
    /// nothing at all -- that is what makes the neutral case bit-identical.
    bool switched = false;
    bool has_blocker = false;
    bool is_ineq = false;
    Index idx = -1;
    BoundState state = BoundState::kFree;
    /// The step the caller should take, already carrying `ratio_test`'s own
    /// cap-at-unit convention.
    double alpha = std::numeric_limits<double>::quiet_NaN();
    Index examined = 0;
    Index passed_over = 0;
    /// Candidates attaining the MINIMUM ratio, signed zeros included.
    Index tie_size = 0;
    double chosen_dep_residual = std::numeric_limits<double>::quiet_NaN();
    int chosen_verdict = -1;
    bool truncated = false;
    /// The self-check FAILED: the arm's enumeration and production's ratio
    /// test do not agree on the blocker, so the arm refused to act.
    bool mismatch = false;
    /// Production's own choice, kept so a switched pass records what it moved
    /// away from.
    bool prod_is_ineq = false;
    Index prod_idx = -1;
    /// Every candidate passed over, for the post-step violation read-out.
    std::vector<QpB1Candidate> passed;
};

/// THE LOCAL PER-PASS FACTOR, built at most once per pass and only if a
/// candidate actually reaches the numeric test.
struct QpB1Factor {
    bool tried = false;
    bool ok = false;
    KktAssembly assembly;
    /// full variable index -> free index, `-1` for a pinned variable.
    std::vector<Index> full_to_free;
    Index n_free = 0;
    Index me = 0;
    Index n_working = 0;
};

/// `me + |W|`: equality rows, working inequality rows, and one unit row per
/// pinned variable. It is `qp_walk_trace.h`'s `qp_walk_ws_size` plus `qp.me()`
/// and it is written out again HERE rather than called, because
/// `qp_walk_trace.h` includes THIS file and not the other way round. The two
/// must agree; a fixture asserts the arm's screen against the trace's.
inline Index b1_dependency_rows(const QpProblem &qp, const WorkingSet &ws) {
    Index rows = qp.me() + static_cast<Index>(ws.active_ineq().size());
    for (const BoundState st : ws.bound_state()) {
        if (st != BoundState::kFree) {
            ++rows;
        }
    }
    return rows;
}

/// EVERY CONSTRAINT `QpEngine::ratio_test` WOULD CONSIDER, in production's own
/// scan order (inequality rows ascending, then bounds ascending), then STABLY
/// sorted by production's own ratio.
///
/// STABLE, AND THAT IS THE WHOLE OF THE TIE RULE. Production keeps the FIRST
/// candidate attaining the minimum (`ratio < best` is strict), scanning rows
/// before bounds, so a stable sort of production's scan order reproduces
/// production's choice exactly -- including the signed-zero case the census
/// named, since `-0.0 < 0.0` is false and `0.0 < -0.0` is false, so the two
/// compare EQUAL and the earlier one keeps its place.
inline std::vector<QpB1Candidate> b1_candidates(const QpProblem &qp, const WorkingSet &ws,
                                                const Vec &x, const Vec &Aix, const Vec &p) {
    const QpDegeneracyParams &prm = QpDegeneracyArmState::params;
    std::vector<QpB1Candidate> out;
    if (qp.mi() > 0) {
        const Vec Aip = qp.Ai * p;
        const std::vector<Index> &aw = ws.active_ineq();
        for (Index j = 0; j < qp.mi(); ++j) {
            if (std::binary_search(aw.begin(), aw.end(), j) || Aip(j) <= prm.negligible_pivot) {
                continue;
            }
            QpB1Candidate c;
            c.is_ineq = true;
            c.idx = j;
            c.a_dot_p = Aip(j);
            c.step = std::max(0.0, qp.bi(j) - Aix(j)) / Aip(j);
            out.push_back(c);
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            continue;
        }
        QpB1Candidate c;
        if (p(i) > prm.negligible_pivot && qp.upper(i) < prm.inf_bound) {
            c.state = BoundState::kAtUpper;
            c.step = std::max(0.0, qp.upper(i) - x(i)) / p(i);
        } else if (p(i) < -prm.negligible_pivot && qp.lower(i) > -prm.inf_bound) {
            c.state = BoundState::kAtLower;
            // The signed-zero site: with `x(i)` exactly on its lower bound
            // this is `min(0, 0)/negative == -0.0`, which the stable sort
            // above leaves where production's scan left it.
            c.step = std::min(0.0, qp.lower(i) - x(i)) / p(i);
        } else {
            continue;
        }
        c.is_ineq = false;
        c.idx = i;
        c.a_dot_p = p(i);
        out.push_back(c);
    }
    std::stable_sort(out.begin(), out.end(), [](const QpB1Candidate &a, const QpB1Candidate &b) {
        return a.step < b.step;
    });
    return out;
}

/// Builds the pass's LOCAL eliminated-form factor, once, lazily. Returns
/// false and counts a failure if assembly or factorization throws -- a
/// hypothetical set is exactly where either is entitled to fail, and a
/// diagnostic may never end a solve that would otherwise have finished.
inline bool b1_build_factor(const QpProblem &qp, const WorkingSet &ws, const QpOptions &opts,
                            KktFactor &kkt, QpB1Factor &fac) {
    if (fac.tried) {
        return fac.ok;
    }
    fac.tried = true;
    try {
        fac.assembly = assemble_kkt(qp, ws, opts);
        (void)factorize_checked(kkt, fac.assembly.K);
    } catch (...) {
        ++QpDegeneracyArmState::indep_test_failures;
        fac.ok = false;
        return false;
    }
    ++QpDegeneracyArmState::indep_factorizations;
    fac.n_free = static_cast<Index>(fac.assembly.free_of_full.size());
    fac.me = qp.me();
    fac.n_working = static_cast<Index>(ws.active_ineq().size());
    fac.full_to_free.assign(static_cast<std::size_t>(qp.n()), -1);
    for (Index k = 0; k < fac.n_free; ++k) {
        fac.full_to_free[static_cast<std::size_t>(
            fac.assembly.free_of_full[static_cast<std::size_t>(k)])] = k;
    }
    fac.ok = true;
    return true;
}

/// THE DEPENDENCY TEST OF B.1 AND B.2, on ONE candidate.
///
/// THE REDUCTION THE ELIMINATED FORM MAKES, and why it is the right question.
/// `assemble_kkt` eliminates the pinned variables, so the factored system's
/// constraint block is `C_F` = the equality and working-inequality rows
/// restricted to the FREE columns; the pinned variables' unit rows are gone.
/// That is exactly right for this test: `a = A_Wᵀ y` with `A_W` including the
/// unit rows holds iff `a_F ∈ range(C_Fᵀ)`, because the unit rows contribute
/// nothing on free components and can match ANY values on the pinned ones.
///
/// ONE BACK-SOLVE with `[a_F ; 0 ; 0]` returns `[u ; v_e ; v_w]` satisfying
/// `(H_FF + δI) u + C_Fᵀ v = a_F`, so `C_Fᵀ v` is the best representation of
/// `a_F` the working set offers and the residual `‖a_F − C_Fᵀ v‖∞ / ‖a_F‖∞`
/// is B.2's statistic, formed by ONE sparse gather over `Ae` and the working
/// rows of `Ai` -- taken DIRECTLY rather than inferred from `‖u‖`.
///
/// THE SCREEN DOES NOT SHORT-CIRCUIT THE SOLVE, deliberately (settler ruling
/// R5). The screen decides the VERDICT when it fires, but the numeric residual
/// is still taken and recorded, because R5's pre-registered separation check
/// needs the residual distribution on exactly the cells where the screen fires
/// -- a tolerance that is never exercised cannot be checked against data. The
/// two are reported apart (`by_screen`, and the two counters).
inline void b1_dependency_test(const QpProblem &qp, const WorkingSet &ws, const QpOptions &opts,
                               KktFactor &kkt, QpB1Factor &fac, QpB1Candidate &c) {
    const QpDegeneracyParams &prm = QpDegeneracyArmState::params;
    const bool screen = prm.integer_screen && (b1_dependency_rows(qp, ws) + 1 > qp.n());
    c.by_screen = screen;
    if (!b1_build_factor(qp, ws, opts, kkt, fac)) {
        // The numeric test is unavailable. The screen still decides when it
        // fired; otherwise the candidate is left UNREFUSED (`-1`), which the
        // caller treats as "do not intervene".
        c.verdict = screen ? 1 : -1;
        if (screen) {
            ++QpDegeneracyArmState::indep_screen_dependent;
        }
        return;
    }
    const Index dim = fac.n_free + fac.me + fac.n_working;
    Vec a_f = Vec::Zero(dim);
    double a_inf = 0.0;
    if (c.is_ineq) {
        for (SpMatRM::InnerIterator it(qp.Ai, c.idx); it; ++it) {
            const Index k = fac.full_to_free[static_cast<std::size_t>(it.col())];
            if (k >= 0) {
                a_f(k) = it.value();
                a_inf = std::max(a_inf, std::abs(it.value()));
            }
        }
    } else {
        // A bound candidate is FREE by construction (the enumeration skips
        // every non-free variable), so its unit row has a free component.
        const Index k = fac.full_to_free[static_cast<std::size_t>(c.idx)];
        if (k >= 0) {
            a_f(k) = 1.0;
            a_inf = 1.0;
        }
    }
    if (!(a_inf > 0.0)) {
        // The row lives entirely on pinned variables, so it IS a combination
        // of their unit rows: dependent, with no solve and no tolerance.
        c.verdict = 1;
        c.dep_residual = 0.0;
        ++QpDegeneracyArmState::indep_screen_dependent;
        return;
    }
    Vec y;
    try {
        y = solve_vec(kkt, a_f);
    } catch (...) {
        ++QpDegeneracyArmState::indep_test_failures;
        c.verdict = screen ? 1 : -1;
        if (screen) {
            ++QpDegeneracyArmState::indep_screen_dependent;
        }
        return;
    }
    ++QpDegeneracyArmState::indep_dep_solves;
    Vec t = Vec::Zero(qp.n());
    for (Index r = 0; r < fac.me; ++r) {
        const double vr = y(fac.n_free + r);
        if (vr == 0.0) {
            continue;
        }
        for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
            t(it.col()) += vr * it.value();
        }
    }
    const std::vector<Index> &aw = ws.active_ineq();
    for (Index k = 0; k < fac.n_working; ++k) {
        const double vk = y(fac.n_free + fac.me + k);
        if (vk == 0.0) {
            continue;
        }
        for (SpMatRM::InnerIterator it(qp.Ai, aw[static_cast<std::size_t>(k)]); it; ++it) {
            t(it.col()) += vk * it.value();
        }
    }
    double res = 0.0;
    for (Index k = 0; k < fac.n_free; ++k) {
        const Index full = fac.assembly.free_of_full[static_cast<std::size_t>(k)];
        res = std::max(res, std::abs(a_f(k) - t(full)));
    }
    c.dep_residual = res / a_inf;
    const bool numeric_dependent = c.dep_residual <= prm.dependency_tol;
    c.verdict = (screen || numeric_dependent) ? 1 : 0;
    if (c.verdict == 1) {
        if (screen) {
            ++QpDegeneracyArmState::indep_screen_dependent;
        } else {
            ++QpDegeneracyArmState::indep_numeric_dependent;
        }
    }
}

/// ARM B1'S DECISION FOR ONE PASS.
///
/// `prod_is_ineq` / `prod_idx` are the blocker `QpEngine::ratio_test` just
/// returned. THE ARM'S FIRST ACT IS TO CHECK ITSELF AGAINST THEM: its own
/// enumeration, stably ordered, must begin with production's blocker. If it
/// does not, the two enumerations disagree about what the ratio test even
/// considers, the arm's read-out would be meaningless, and the arm REFUSES the
/// pass (`ran = false`, `mismatch = true`, nothing changed).
inline QpB1Outcome b1_select_blocker(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                     const Vec &Aix, const Vec &p, const QpOptions &opts,
                                     KktFactor &kkt, bool prod_is_ineq, Index prod_idx) {
    const QpDegeneracyParams &prm = QpDegeneracyArmState::params;
    QpB1Outcome o;
    o.prod_is_ineq = prod_is_ineq;
    o.prod_idx = prod_idx;
    std::vector<QpB1Candidate> cands = b1_candidates(qp, ws, x, Aix, p);
    if (cands.empty() || cands.front().is_ineq != prod_is_ineq || cands.front().idx != prod_idx) {
        ++QpDegeneracyArmState::indep_enumeration_mismatches;
        o.mismatch = true;
        return o;
    }
    o.ran = true;
    ++QpDegeneracyArmState::indep_passes;
    const double best = cands.front().step;
    for (const QpB1Candidate &c : cands) {
        if (c.step == best) {
            ++o.tie_size;
        }
    }

    QpB1Factor fac;
    const QpB1Candidate *pick = nullptr;
    std::size_t next = 0;
    for (std::size_t k = 0; k < cands.size(); ++k) {
        if (prm.max_dep_tests_per_pass > 0 && o.examined >= prm.max_dep_tests_per_pass) {
            o.truncated = true;
            ++QpDegeneracyArmState::indep_budget_truncations;
            break;
        }
        b1_dependency_test(qp, ws, opts, kkt, fac, cands[k]);
        ++o.examined;
        next = k + 1;
        if (cands[k].verdict != 1) {
            // INDEPENDENT (`0`) or UNTESTABLE (`-1`): admitted. An untestable
            // candidate is admitted deliberately -- a test that could not be
            // taken may not refuse a constraint.
            pick = &cands[k];
            break;
        }
        o.passed.push_back(cands[k]);
        ++o.passed_over;
    }
    if (pick == nullptr && o.truncated && next < cands.size()) {
        // The budget ran out. Take the first UNEXAMINED candidate unrefused,
        // so a truncation can only move the arm back toward production.
        pick = &cands[next];
    }
    QpDegeneracyArmState::indep_candidates_examined += o.examined;
    QpDegeneracyArmState::indep_candidates_passed_over += o.passed_over;
    if (pick == nullptr) {
        // EVERY candidate is dependent. Admit nothing and take the full step
        // -- `ratio_test`'s `cap_at_unit` convention with no blocker left.
        o.has_blocker = false;
        o.alpha = 1.0;
        ++QpDegeneracyArmState::indep_no_blocker_passes;
    } else {
        o.has_blocker = true;
        o.is_ineq = pick->is_ineq;
        o.idx = pick->idx;
        o.state = pick->state;
        o.chosen_dep_residual = pick->dep_residual;
        o.chosen_verdict = pick->verdict;
        // `QpEngine::ratio_test`'s own cap-at-unit convention, transcribed:
        // a blocker past the unit step means the full step is interior and
        // nothing joins.
        if (pick->step > 1.0 + prm.step_tie_tol) {
            o.has_blocker = false;
            o.alpha = 1.0;
        } else {
            o.alpha = std::min(1.0, pick->step);
        }
    }
    // NOTHING PASSED OVER MEANS NOTHING MOVED. The pick is then production's
    // own blocker and the caller must leave `kind`, `block_idx`,
    // `block_state` and `alpha` exactly as they were -- which is what makes
    // an armed run bit-identical to production wherever the arm finds nothing
    // dependent, by construction rather than by arithmetic.
    o.switched = o.passed_over > 0;
    if (o.switched) {
        ++QpDegeneracyArmState::indep_blocker_switched;
    }
    return o;
}

/// The largest amount by which the constraints the arm passed over are
/// violated at the iterate the walk actually moved to. Zero when nothing was
/// passed over, and zero when every passed-over constraint is still satisfied.
inline double b1_passed_over_violation(const QpProblem &qp, const Vec &x_new,
                                       const std::vector<QpB1Candidate> &passed) {
    double worst = 0.0;
    for (const QpB1Candidate &c : passed) {
        const double v = c.is_ineq
                             ? qp.Ai.row(c.idx).dot(x_new) - qp.bi(c.idx)
                             : (c.state == BoundState::kAtUpper ? x_new(c.idx) - qp.upper(c.idx)
                                                                : qp.lower(c.idx) - x_new(c.idx));
        worst = std::max(worst, v);
    }
    return worst;
}

// ===========================================================================
// ARM B2 -- THE CLASSICAL EXCHANGE: REGISTERED, NOT BUILT (settler ruling R3)
// ===========================================================================
//
// R3 makes b2 conditional: "build it only if it is implementable within the
// arm pattern without touching non-HVEN_TESTING code paths and without
// breaking the one-drop-one-add invariants; otherwise register it (production
// design item) and say so." THE CONDITION DOES NOT HOLD. Three reasons, in
// increasing order of how hard they are to work around:
//
//   1. `refresh_shifts` RUNS IMMEDIATELY AFTER THE ADD AND CAN UNDO THE SWAP.
//      Its documented job is to put every inequality row carrying a positive
//      shift INTO the working set (`ratio_test`'s own note says so). A member
//      the exchange releases can therefore be re-admitted one statement later,
//      on the same pass, with no record that the exchange ever happened. An
//      arm whose effect the next production statement may erase is not an arm.
//   2. THE RELEASE IS NOT A DROP, AND THE LOOP'S DROP BOOKKEEPING IS WHAT
//      MAKES A RELEASE SAFE. `DropRecord dropped` arms section 4c's
//      negative-curvature ride on the following pass, the zero-multiplier
//      probe's addition-sequence stamps and exemptions are keyed on how a
//      constraint entered and left, and both were written for "at most one
//      drop per pass, decided by `drop_worst`". Writing that bookkeeping from
//      inside the arm means reimplementing production's release semantics in
//      the seam, which is the "one-drop-one-add invariant" R3 forbids
//      breaking; NOT writing it means releasing a constraint the loop does not
//      know was released.
//   3. RELEASING A PINNED BOUND CHANGES `num_free`, AND THE STEP ABOUT TO BE
//      TAKEN CAME OUT OF THE OLD FACE. `p` and `alpha` were computed for the
//      pre-swap working set; the swap's own justification (GMW ch. 5) is about
//      which set the NEXT EQP is solved over, so the pass must either re-solve
//      or carry a direction whose face no longer exists. Either choice is a
//      change to the walk's pass structure, i.e. a production change.
//
// SO IT IS REGISTERED: the exchange is a PRODUCTION design item, alongside
// B.2's "one triangular solve against the retained factor" cost target. b1 is
// the minimal intervention that tests the causal claim -- that dependent adds
// drive the drop/re-add recurrence -- and it needs none of the above.

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
