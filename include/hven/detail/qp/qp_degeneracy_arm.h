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
// *** SUPERSEDED IN ITS ORACLE BY AMENDMENT A1, AND THE SUPERSESSION IS A
// *** MEASUREMENT, NOT A PREFERENCE. What B.2 proposed -- and what was built
// *** and gated at `3899bad6` -- was a back-solve through a delta/mu
// *** REGULARIZED local KKT factor. The Codex review of M4b/M4c (item 4)
// *** objected that the `-mu I` block is exactly what makes a dependent row
// *** factorizable, so one solve against that factor need not test the rank
// *** of the UNREGULARIZED active-row matrix at all. The implementer then
// *** MEASURED it: with the integer screen off, on M4's period-2 QP, the
// *** representation residual was bimodal at 1.0000000272292198e-08 (99
// *** candidates) and order one (3 candidates) -- the dependent population
// *** sitting exactly ON `dual_mu = 1e-8`, which is also the pre-registered
// *** tolerance, so the test classified all 99 as independent by a relative
// *** hair and converted nothing. THE PARAGRAPHS BELOW ARE KEPT AS THE RECORD
// *** OF WHAT WAS PROPOSED AND WHY IT FAILED; the oracle actually built is
// *** the rank-revealing QR of `R^T` described at `QpB1Rank`, and the
// *** tolerance is the same pre-declared `1e-8`, now applied to a statistic
// *** that has no `mu` floor under it.
//
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
// *** AND THE REFERENCE FOR THAT RULE IS SWAPPED BY AMENDMENT A4. The review's
// *** item 5 is that "largest dependency coefficient" is not theorem-backed:
// *** Gill & Wong, "Methods for convex and general quadratic programming",
// *** *Math. Prog. Comp.* 7 (2015) §4 formulate the working set as linearly
// *** independent throughout and give a MULTIPLIER-AWARE exchange with the
// *** sign conditions that make the swap admissible, and Fletcher (1988, 1993)
// *** supports a dedicated degeneracy PROCEDURE rather than an unqualified
// *** largest-coefficient swap. The registered production design item is
// *** therefore Gill & Wong's exchange -- never removing a mandatory equality
// *** or fixed bound, recording the dependency vector, the multiplier and sign
// *** conditions, the removed member, feasibility after the exchange, and the
// *** proof that the new set is independent -- and NOT the paragraph above.
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
// THE AMENDED DESIGN (settler amendments A1-A12, 2026-09-22) -- WHAT IS
// ACTUALLY BUILT BELOW, AND WHICH MUTATION SITE IS COVERED HOW
// ===========================================================================
//
// The rulings R1-R7 above were amended after the Codex review of M4b/M4c.
// A1-A12 OVERRIDE R2, R5 and R6 where they conflict; R1, R3, R4 and R7 stand.
// What the code below implements, in one list:
//
//   A1  THE ORACLE IS A RANK-REVEALING QR OF THE UNREGULARIZED ACTIVE ROWS,
//       built once per pass, with the PRE-DECLARED relative-residual
//       tolerance `dependency_tol = 1e-8` and the dependency coefficients `v`
//       from the same decomposition. The regularized-factor oracle of B.2 is
//       gone. See `QpB1Rank`.
//   A2  THE INVARIANT IS RANK: `rank(R) == me + |W| <= n`, read off that same
//       decomposition at that same tolerance. The row count `me + |W| - n` is
//       necessary but NOT sufficient and is kept only as the free integer
//       witness it is. See `b1_invariant_holds`.
//   A3  ONE SHARED PREDICATE, `admit_if_independent`, at EVERY add site the
//       walk has, plus an ENTRY REPAIR that makes the seed set independent
//       before the first pass, plus a REFUSAL (disarm and proceed unarmed)
//       when the MANDATORY rows are themselves dependent.
//   A4  NO EXCHANGE IN M5. b2 is registered with Gill & Wong (2015)'s
//       multiplier-aware exchange as its reference.
//   A5  THE LEAK MEASUREMENT IS STRENGTHENED: normalized `a^T p`, the worst
//       retained-row leak, the ratio to `mu*||lambda_W||_inf`, the
//       coefficients, and the `|a^T p - v^T (R p)|` comparison -- plus a mu
//       SWEEP in the fixtures, which is what licenses any "O(mu)" claim.
//   A8  THE SCREEN NEVER REFUSES ALONE. Under A2's invariant the screen and
//       the QR cannot disagree, so the screen is a free pre-check and the
//       three mechanisms (entry removals / screen-and-QR pass-overs / QR-only
//       pass-overs) are counted apart.
//   A9  A RANK-DEFICIENT SET FOUND AWAY FROM ENTRY DISARMS THE SOLVE and the
//       solve continues unarmed; that cell's verdict is UNSUPPORTED. It is
//       not a global stop.
//  A10  A PASSED-OVER CONSTRAINT'S POST-STEP VIOLATION IS BOUNDED BY THE LEAK
//       or the pass-over was wrong; the bound is pre-registered in
//       `b1_leak_bound_holds` and every failure is counted.
//  A11  THE QR IS BUILT ONCE PER PASS and reused wherever a mutation goes
//       through the predicate; the build count is a declared counter.
//
// --- SITE COVERAGE: THE SIX MUTATION SITES, AND HOW EACH IS COVERED --------
//
// A3 requires the predicate at every add site. The six sites the walk has in
// `src/qp/qp_engine.cpp`, and what the arm does at each:
//
//   1. ORDINARY BLOCKER ADD (`QpEngine::run`). COVERED, and it is the arm's
//      primary rule: the candidates are taken in ratio order and the first
//      INDEPENDENT one blocks (R3/R4). A refusal here means "take the next
//      candidate", not "decline the addition".
//   2. RIDE ADD (`ride_negative_curvature`). COVERED. A dependent blocker
//      makes the ride DECLINE -- a path production already has, reached
//      BEFORE the ride mutates `x` or the working set, so the arm never
//      leaves a half-applied ride behind.
//   3. SEED INGESTION (`ingest_seed_working_set`). COVERED BY THE ENTRY
//      REPAIR, which is A3's own prescription for the seed and which decides
//      every seed member through this same predicate. Hooking the ingestion
//      statement itself would decide the same members in the same order with
//      no access to the mandatory-first rule the repair needs.
//   4. SHIFT ADDITIONS (`refresh_shifts`). COVERED: a dependent shift row is
//      not added, and the refusal is counted. The shift stays positive, so
//      the homotopy's own bookkeeping is untouched and the row is offered
//      again on the next refresh.
//   5. ZERO-PROBE RESTORATION (`probe_zero_multiplier_drops`). CHECK-ONLY,
//      DELIBERATELY. This site puts BACK a member the probe removed for a
//      trial; refusing it would not change an ADD RULE, it would change the
//      probe's contract -- the walk would silently lose a constraint it never
//      decided to drop. The predicate is asked and its verdict recorded
//      (`indep_probe_restore_dependent`), and A2's rank check at the next
//      predicate call is what catches a set the restoration made deficient.
//   6. REPAIR PINS (`pin_at_best_bound`, from `repair_temporary_vertex`).
//      COVERED: a pin whose unit row is dependent is refused, which the
//      caller already handles -- it is the same `false` the function returns
//      for a variable unbounded on both sides, and the repair moves on to the
//      next variable in its own order.
//
// AND A SEVENTH MUTATION IS RULED OUT OF SCOPE EXPLICITLY. `refine_on_face`
// transcribes a CALLER-SUPPLIED face into a working set for one exact solve.
// It does not run the walk, it takes no ratio test and it reaches no add
// rule; its working set is an argument, not a decision. An arm that rewrote
// it would be changing what the caller asked for rather than what the walk
// chose. It is named here so the omission is a ruling and not an oversight.
//
// --- WHAT THE ARM STILL CANNOT SEE ----------------------------------------
//
// Sites 5 and 6 are hard to reach deliberately from a unit fixture: both are
// inside repair paths that fire on inertia verdicts a constructed QP does not
// reliably produce. They are declared UNCOVERED-BY-FIXTURE here, as A3
// anticipates, and what stands behind them is A2's rank check -- asked at the
// NEXT predicate call on that solve, which will disarm under A9 if either
// site left the set deficient.
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

#include <Eigen/QR>

#include <hven/core/types.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/working_set.h>

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

/// THE WORKING-SET MUTATION SITES THE WALK HAS, as amendment A3 enumerates
/// them and as they stand in `src/qp/qp_engine.cpp` at this commit. The value
/// is what a disarm, a refusal or a trace column NAMES, so a read-out can say
/// WHERE the arm acted rather than only that it did.
///
/// SITES 1-4 and 6 REFUSE; SITE 5 IS CHECK-ONLY; `refine_on_face` IS OUT OF
/// SCOPE. The reasons are in the UNCOVERED SITES block further down.
enum class QpB1Site : int {
    kNone = -1,
    /// `QpEngine::run`'s ordinary blocker add -- the ratio test's own
    /// decision, and the only site where a refusal means "take the NEXT
    /// candidate" rather than "decline this addition".
    kOrdinaryAdd = 0,
    /// `ride_negative_curvature`'s add.
    kRideAdd = 1,
    /// `ingest_seed_working_set` -- covered by the ENTRY REPAIR, which is the
    /// order-aware form A3 asks for, and which decides every seed member
    /// through this same predicate.
    kSeedEntry = 2,
    /// `refresh_shifts`'s shift additions.
    kShiftAdd = 3,
    /// `probe_zero_multiplier_drops`'s restoration. CHECK-ONLY.
    kProbeRestore = 4,
    /// `pin_at_best_bound`, called from `repair_temporary_vertex`.
    kRepairPin = 5,
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
    /// Candidate rows put through the QR oracle (one projection each).
    static inline thread_local Index indep_dep_tests = 0;
    /// RANK-REVEALING QR DECOMPOSITIONS OF `Rᵀ` BUILT -- the cost, in the
    /// currency CLAUDE.md section 7 asserts, and amendment A11's declared
    /// counter. ONE per pass under the amended design (A1), plus one per
    /// ACCEPTED member on the rare solve whose entry repair runs, plus one
    /// wherever a site mutates the working set without going through
    /// `admit_if_independent` and the next predicate call finds the cached
    /// decomposition stale.
    static inline thread_local Index indep_qr_builds = 0;
    /// Decompositions or projections that THREW. Counted, never absorbed: a
    /// candidate whose test could not be taken is left UNREFUSED, so a
    /// backend or allocation fault can only move the arm back toward
    /// production.
    static inline thread_local Index indep_test_failures = 0;
    /// THE THREE MECHANISMS, COUNTED APART (amendment A8). The QR is the
    /// oracle at all three; the integer screen is a free pre-check that, under
    /// A2's invariant, is implied by it, and the arm NEVER refuses on the
    /// screen alone. A leg's table says which mechanism acted by reading
    /// these three against each other.
    ///   (1) seed members the ENTRY REPAIR removed;
    static inline thread_local Index indep_entry_members_removed = 0;
    ///   (2) dependent verdicts where the screen ALSO fired;
    static inline thread_local Index indep_screen_and_qr_dependent = 0;
    ///   (3) dependent verdicts the QR reached with the screen silent.
    static inline thread_local Index indep_qr_only_dependent = 0;
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
    /// THE HARD INVARIANT, AS AMENDMENT A2 REDEFINES IT: not the row count
    /// `me + |W| - n <= 0` (necessary, NOT sufficient -- the Codex review's
    /// item 2) but `rank(R) == me + |W| <= n`, read off the SAME
    /// decomposition at the SAME tolerance. `me + |W| - n` is still kept,
    /// because a positive value is an integer PROOF of a violation and is
    /// free; `Index` min means "the arm never read the set".
    static inline thread_local Index indep_rows_minus_n_max = std::numeric_limits<Index>::min();
    /// The largest `me + |W| - rank(R)` the arm ever read: zero on a run that
    /// held the invariant throughout.
    static inline thread_local Index indep_rank_deficit_max = 0;
    /// Working sets the arm found RANK-DEFICIENT away from solve entry --
    /// amendment A9's `arm_rank_violation`. Each one DISARMS the arm for the
    /// rest of that solve (the solve continues, unarmed, and its verdict is
    /// UNSUPPORTED); it is not a global stop.
    static inline thread_local Index indep_rank_violations = 0;
    /// Solves the arm REFUSED because the MANDATORY rows (equalities and
    /// fixed bounds) were themselves dependent at entry -- A3's
    /// `armed_refused_mandatory_dependent`. The solve proceeds UNARMED and
    /// that cell's row says so.
    static inline thread_local Index indep_refused_mandatory_dependent = 0;
    /// TRUE once this solve has disarmed, with the site that disarmed it. The
    /// flag gates `independent_add_on()`, so every hook below returns to
    /// production's statements for the rest of the solve.
    static inline thread_local bool indep_disarmed = false;
    static inline thread_local int indep_disarm_site = -1;
    /// REFUSALS AT THE NON-ORDINARY MUTATION SITES (A3), counted apart from
    /// the ratio test's pass-overs because they are different acts: the ratio
    /// test moves to the NEXT candidate, these decline an addition outright.
    static inline thread_local Index indep_ride_refusals = 0;
    static inline thread_local Index indep_shift_refusals = 0;
    static inline thread_local Index indep_repair_pin_refusals = 0;
    /// The zero-multiplier probe's RESTORATION is a CHECK-ONLY site (see the
    /// UNCOVERED SITES block below): a dependent verdict there is recorded and
    /// never acted on, because refusing to put back a member the probe removed
    /// for a trial would change the probe's contract rather than the add rule.
    static inline thread_local Index indep_probe_restore_dependent = 0;
    /// AMENDMENT A10's PRE-REGISTERED BOUND, failed: a passed-over
    /// constraint whose post-step violation exceeded
    /// `(mu*||lambda_W||_inf + eps*||a||*||p||) * alpha * 10`. A nonzero value
    /// is a FINDING -- the candidate was not merely leaking -- and makes that
    /// solve's b1 verdict UNSUPPORTED.
    static inline thread_local Index indep_leak_bound_exceeded = 0;
    /// The largest post-step violation over every constraint the arm passed
    /// over, measured at the iterate the walk actually moved to.
    static inline thread_local double indep_max_passed_violation = 0.0;
    /// THE SOLVE STAMP the cached decomposition carries, so a cache can never
    /// be reused across two solves that happen to present the same working-set
    /// SIGNATURE on two DIFFERENT problems. Bumped by `begin_solve()`.
    static inline thread_local Index indep_solve_generation = 0;

    static bool scaled_drop_on() {
        return arm == QpDegeneracyArm::kScaledDrop || arm == QpDegeneracyArm::kBoth;
    }
    /// ARM B IS ARMED ON THIS THREAD AND HAS NOT DISARMED ITSELF. Every hook
    /// site asks THIS, not the switch, so amendment A9's disarm-and-continue
    /// returns the whole rest of the solve to production's statements with one
    /// flag rather than with a branch per site.
    static bool independent_add_on() {
        return !indep_disarmed &&
               (arm == QpDegeneracyArm::kIndependentAdd || arm == QpDegeneracyArm::kBoth);
    }
    /// The SWITCH, ignoring the disarm -- what a read-out asks when it wants
    /// to know whether this solve was MEANT to be armed.
    static bool independent_add_armed() {
        return arm == QpDegeneracyArm::kIndependentAdd || arm == QpDegeneracyArm::kBoth;
    }
    static bool any_on() { return arm != QpDegeneracyArm::kOff; }

    /// AMENDMENT A9's DISARM. Records the site, stops the arm for the rest of
    /// THIS solve, and lets the solve run on unarmed. Idempotent: the FIRST
    /// site to disarm is the one recorded, because it is the one that
    /// explains the rest.
    static void disarm(int site) {
        if (!indep_disarmed) {
            indep_disarmed = true;
            indep_disarm_site = site;
        }
    }

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
        indep_dep_tests = 0;
        indep_qr_builds = 0;
        indep_test_failures = 0;
        indep_entry_members_removed = 0;
        indep_screen_and_qr_dependent = 0;
        indep_qr_only_dependent = 0;
        indep_blocker_switched = 0;
        indep_no_blocker_passes = 0;
        indep_budget_truncations = 0;
        indep_enumeration_mismatches = 0;
        indep_rows_minus_n_max = std::numeric_limits<Index>::min();
        indep_rank_deficit_max = 0;
        indep_rank_violations = 0;
        indep_refused_mandatory_dependent = 0;
        indep_disarmed = false;
        indep_disarm_site = -1;
        indep_ride_refusals = 0;
        indep_shift_refusals = 0;
        indep_repair_pin_refusals = 0;
        indep_probe_restore_dependent = 0;
        indep_leak_bound_exceeded = 0;
        indep_max_passed_violation = 0.0;
    }

    /// Called once per `QpEngine::run`, before the walk loop. Clears the
    /// published scale so a solve can never judge its first drop against the
    /// PREVIOUS solve's scale, and leaves the switches, the parameters and the
    /// counters alone — a caller measuring a whole SQP solve wants the totals
    /// over every subproblem, and a fixture that wants one solve's numbers
    /// calls `reset_counters()` itself.
    ///
    /// IT ALSO RE-ARMS. Amendment A9's disarm is per SOLVE: a rank violation
    /// in one subproblem says nothing about the next one, and a disarm that
    /// persisted would silently turn an armed leg into an unarmed one after
    /// its first bad set. The COUNTERS of the disarm survive -- they are what
    /// the read-out reports -- and only the flag is cleared.
    static void begin_solve() {
        stat_scale = std::numeric_limits<double>::quiet_NaN();
        indep_disarmed = false;
        indep_disarm_site = -1;
        ++indep_solve_generation;
    }

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
// no working-set mutation the loop does not already perform, and NO ENGINE
// STATE AT ALL -- and under amendment A1 that last claim is stronger than it
// was, because the oracle is no longer a KKT factor of any kind. It is a
// dense QR of the problem's own constraint rows, built in the arm's own
// stack frame from `qp` and `ws`. The engine's `kkt`, its face and the live
// Schur border stack are not read, not written and not even reachable from
// the oracle, on every algebra path, by construction rather than by care.
// When the arm passes nothing over, the caller leaves production's blocker
// and production's step untouched, so ARM-ON-BUT-NOTHING-DEPENDENT IS
// BIT-IDENTICAL TO PRODUCTION BY CONSTRUCTION rather than by arithmetic that
// happens to agree.
//
// THE COST, DECLARED (A1 and A11). ONE dense rank-revealing QR of `R^T` per
// pass -- `O(n * (me+|W|)^2)` with an `n x (me+|W|)` matrix materialized --
// plus one orthogonal projection per candidate examined and one least-squares
// solve per candidate the oracle calls dependent. That is a heavier cost
// class than the "one triangular solve against the RETAINED factor" B.2
// described, and heavier again than the local KKT factorization the
// superseded build paid; A1 accepts it because it is the only oracle that
// answers the question asked -- the rank of the UNREGULARIZED rows -- and
// because this is an EXPERIMENT whose cost is a declared counter, not a
// shipped path. A cell that cannot finish inside its leg's deadline under
// this cost is CENSORED and reported as TRUNCATED. The production cost
// target stays the retained-factor solve and is registered as a design item,
// not built here. Wall-clock is informational (CLAUDE.md section 7);
// `indep_qr_builds` is the asserted currency.

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
    /// `‖a − Q Qᵀ a‖₂ / ‖a‖₂` from the rank-revealing QR of `Rᵀ` (A1). NaN
    /// when the oracle did not run.
    double dep_residual = std::numeric_limits<double>::quiet_NaN();
    /// True when the INTEGER screen fired for this candidate's set. A
    /// PRE-CHECK: it never refuses on its own (A8).
    bool by_screen = false;
    // ---- AMENDMENT A5's STRENGTHENED LEAK MEASUREMENT --------------------
    /// `‖a‖₂`, and the NORMALIZED derivative `aᵀp / (‖a‖₂‖p‖₂)` -- the
    /// quantity that is comparable across rows of different scale, which a
    /// bare `aᵀp` is not (the Codex review's item 6).
    double a_norm = 0.0;
    double a_dot_p_normalized = std::numeric_limits<double>::quiet_NaN();
    /// `aᵀp / (μ‖λ_W‖∞)`: the leak in units of the bound B.0 derives for it.
    /// A dependent candidate whose blocking IS the regularization leak has
    /// this of order one. NaN when `μ‖λ_W‖∞` is zero.
    double leak_ratio = std::numeric_limits<double>::quiet_NaN();
    /// `|aᵀp − vᵀ(R p)|` with `a = Rᵀv` from the same decomposition: whether
    /// the candidate's derivative IS reconstructed by the retained rows' own
    /// leaks. Small means "this blocking is the retained rows' leak, seen
    /// through the dependency". NaN when the coefficients were not taken.
    double coeff_residual = std::numeric_limits<double>::quiet_NaN();
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
    // ---- THE AMENDED DESIGN'S PER-PASS READ-OUTS -------------------------
    /// `me + |W|` and `rank(R)` on the set this pass decided against (A2),
    /// and the decompositions this pass paid for (A11).
    Index rows = 0;
    Index rank = 0;
    Index qr_builds = 0;
    /// A8's three-mechanism split, for THIS pass: pass-overs where the screen
    /// also fired, and pass-overs the QR reached alone. (The third mechanism,
    /// the entry repair, is per solve and is a counter only.)
    Index screen_and_qr_passed = 0;
    Index qr_only_passed = 0;
    /// A5: `max |A_W p|` over the RETAINED rows, `μ‖λ_W‖∞`, and `‖p‖₂` --
    /// the three numbers the per-candidate leak columns are read against.
    double max_retained_leak = std::numeric_limits<double>::quiet_NaN();
    double leak_bound = std::numeric_limits<double>::quiet_NaN();
    double p_norm = std::numeric_limits<double>::quiet_NaN();
    /// A9: the arm disarmed on this pass, so production's choice stands and
    /// every pass after it in this solve is production's too.
    bool disarmed = false;
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

/// THE ACTIVE-ROW MATRIX `R`, TRANSPOSED AND DENSE (amendment A1).
///
/// `R = [Ae ; A_W ; e_i for each pinned i]`, `me + |W|` rows in R^n, and
/// UNREGULARIZED -- that is the whole point of A1. The transpose is what is
/// built, because the decomposition below wants `R^T` (n x rows) and because
/// the question the oracle asks is about the RANGE of `R^T`, i.e. the row
/// space of `R`.
///
/// ROW ORDER IS CANONICAL AND DECLARED: equality rows in index order, then
/// working inequality rows in ascending index order (`WorkingSet` keeps
/// `active_ineq()` sorted), then pinned bounds in ascending variable order.
/// The set retains NO ingestion order, so this canonical order is the
/// surrogate the entry repair reverses; see `b1_entry_repair`.
inline Mat b1_active_rows_transposed(const QpProblem &qp, const WorkingSet &ws) {
    const Index rows = b1_dependency_rows(qp, ws);
    Mat Rt = Mat::Zero(qp.n(), rows);
    Index r = 0;
    for (Index e = 0; e < qp.me(); ++e, ++r) {
        for (SpMatRM::InnerIterator it(qp.Ae, e); it; ++it) {
            Rt(it.col(), r) = it.value();
        }
    }
    for (const Index j : ws.active_ineq()) {
        for (SpMatRM::InnerIterator it(qp.Ai, j); it; ++it) {
            Rt(it.col(), r) = it.value();
        }
        ++r;
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            Rt(i, r) = 1.0;
            ++r;
        }
    }
    return Rt;
}

/// `R p` -- every retained row's leak, in the canonical row order above.
/// Amendment A5 asks for `max |A_W p|` over the RETAINED rows; this vector is
/// that max's argument and also the `R p` the dependency coefficients are
/// checked against (`|a^T p - v^T (R p)|`).
inline Vec b1_active_rows_times(const QpProblem &qp, const WorkingSet &ws, const Vec &p) {
    const Index rows = b1_dependency_rows(qp, ws);
    Vec out = Vec::Zero(rows);
    Index r = 0;
    for (Index e = 0; e < qp.me(); ++e, ++r) {
        double s = 0.0;
        for (SpMatRM::InnerIterator it(qp.Ae, e); it; ++it) {
            s += it.value() * p(it.col());
        }
        out(r) = s;
    }
    for (const Index j : ws.active_ineq()) {
        double s = 0.0;
        for (SpMatRM::InnerIterator it(qp.Ai, j); it; ++it) {
            s += it.value() * p(it.col());
        }
        out(r) = s;
        ++r;
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (ws.bound_state()[static_cast<std::size_t>(i)] != BoundState::kFree) {
            out(r) = p(i);
            ++r;
        }
    }
    return out;
}

/// `‖λ_W‖∞` over the WORKING rows -- the equality multipliers and the
/// multipliers of the inequality rows actually in the working set, and
/// nothing else. It is the denominator of amendment A5's leak ratio and the
/// first term of A10's bound, taken from the multipliers the walk itself just
/// priced rather than re-derived.
inline double b1_working_multiplier_inf(const QpProblem &qp, const WorkingSet &ws,
                                        const Vec &lambda_e, const Vec &lambda_i) {
    double m = 0.0;
    for (Index r = 0; r < qp.me() && r < lambda_e.size(); ++r) {
        m = std::max(m, std::abs(lambda_e(r)));
    }
    for (const Index j : ws.active_ineq()) {
        if (j < lambda_i.size()) {
            m = std::max(m, std::abs(lambda_i(j)));
        }
    }
    return m;
}

/// ONE CANDIDATE ROW, DENSE. `a = A_i(idx,:)` for an inequality row and
/// `a = e_idx` for a bound.
inline Vec b1_candidate_row(const QpProblem &qp, bool is_ineq, Index idx) {
    Vec a = Vec::Zero(qp.n());
    if (is_ineq) {
        for (SpMatRM::InnerIterator it(qp.Ai, idx); it; ++it) {
            a(it.col()) = it.value();
        }
    } else {
        a(idx) = 1.0;
    }
    return a;
}

/// THE RANK ORACLE (amendment A1): a dense RANK-REVEALING QR of `R^T`, with
/// the working-set signature that says whether it is still current.
///
/// WHY A QR OF THE UNREGULARIZED ROWS AND NOT THE KKT FACTOR. The superseded
/// design tested a candidate by back-solving through a delta/mu-REGULARIZED
/// local KKT factor. The Codex review's item 4 and amendment A1 reject that,
/// and the predecessor's own measurement is why: with the screen off, on M4's
/// period-2 QP, the representation residual through that factor was BIMODAL
/// at 1.0000000272e-08 and order one -- i.e. the dependent population sat
/// exactly ON `dual_mu = 1e-8`, because the `-mu I` block is precisely what
/// makes a dependent row factorizable. A tolerance derived from the same `mu`
/// therefore cannot separate the two populations except by a relative hair.
/// The QR of the UNREGULARIZED rows has no such floor: a dependent row's
/// residual is at the rounding level of the rows themselves.
///
/// THE COST, DECLARED (A1). `O(n * rows^2)` per decomposition, dense, with an
/// `n x rows` matrix materialized. ONE decomposition per pass in the ordinary
/// case (A11), counted in `indep_qr_builds`. On a cell where this is too
/// expensive to finish inside the leg's deadline the cell is CENSORED and
/// reported as TRUNCATED -- never quietly skipped. Wall-clock is
/// informational; the build count is the asserted currency.
struct QpB1Rank {
    bool built = false;
    bool ok = false;
    /// `me + |W|` at the decomposition, and `rank(R)` read at
    /// `dependency_tol`.
    Index rows = 0;
    Index n = 0;
    Index rank = 0;
    Eigen::ColPivHouseholderQR<Mat> qr;
    /// THE SIGNATURE. A cached decomposition is reused only for the EXACT
    /// working set it was taken on -- this is A11's "reuse that pass's QR"
    /// made decidable rather than assumed, so a mutation at a site that does
    /// not go through the predicate can never be tested against a stale
    /// factor.
    std::vector<Index> sig_ineq;
    std::vector<BoundState> sig_bound;
    /// The solve this decomposition was taken in. Two DIFFERENT problems can
    /// present the same working-set signature, so the signature alone is not
    /// enough to license a reuse; the stamp is.
    Index generation = -1;

    bool matches(const QpProblem &qp, const WorkingSet &ws) const {
        return built && generation == QpDegeneracyArmState::indep_solve_generation && n == qp.n() &&
               sig_ineq == ws.active_ineq() && sig_bound == ws.bound_state();
    }
};

/// THE ONE CACHED DECOMPOSITION A SOLVE SHARES ACROSS ITS MUTATION SITES
/// (amendment A11). `admit_if_independent` rebuilds it whenever the signature
/// or the solve stamp says it is stale, which is exactly "reuse that pass's QR
/// when the mutation went through the predicate, rebuild where it did not" --
/// decided by comparison rather than by an assumption about call order.
///
/// It is `thread_local` for the same reason every other piece of this state
/// is: an engine is used sequentially by one thread, and two instrumented
/// solves on two threads must not share a factor.
inline QpB1Rank &b1_site_rank() {
    static thread_local QpB1Rank rk;
    return rk;
}

/// Builds the decomposition if the cached one is not the current set's.
/// Returns false, counted, when it throws -- a candidate whose test could not
/// be taken is left UNREFUSED.
inline bool b1_rank_refresh(const QpProblem &qp, const WorkingSet &ws, QpB1Rank &rk) {
    if (rk.matches(qp, ws)) {
        return rk.ok;
    }
    rk.built = true;
    rk.ok = false;
    rk.n = qp.n();
    rk.rows = b1_dependency_rows(qp, ws);
    rk.sig_ineq = ws.active_ineq();
    rk.sig_bound = ws.bound_state();
    rk.generation = QpDegeneracyArmState::indep_solve_generation;
    if (rk.rows == 0) {
        // THE EMPTY WORKING SET IS NOT A DECOMPOSITION. It spans nothing, so
        // every nonzero candidate is independent of it by definition, and
        // `ColPivHouseholderQR` is entitled to reject a matrix with no columns
        // (its column-norm sweep reduces an empty row vector, which Eigen's
        // Debug asserts catch). Answered without arithmetic, and NOT counted
        // as a build, because none was taken.
        rk.rank = 0;
        rk.ok = true;
        return true;
    }
    try {
        const Mat Rt = b1_active_rows_transposed(qp, ws);
        rk.qr.compute(Rt);
        rk.qr.setThreshold(QpDegeneracyArmState::params.dependency_tol);
        rk.rank = rk.qr.rank();
    } catch (...) {
        ++QpDegeneracyArmState::indep_test_failures;
        return false;
    }
    ++QpDegeneracyArmState::indep_qr_builds;
    rk.ok = true;
    return true;
}

/// THE DEPENDENCY STATISTIC (A1): the RELATIVE PROJECTION RESIDUAL of `a` on
/// the row space of `R`,
///
///        ||a - Q Q^T a||_2 / ||a||_2 ,
///
/// where `Q` is the first `rank(R)` columns of the decomposition's orthogonal
/// factor and therefore an orthonormal basis of `range(R^T)`. Because `Q` is
/// orthogonal the residual is just the tail of `Q^T a` below the rank, so no
/// basis is ever materialized.
///
/// `> dependency_tol` ==> INDEPENDENT. A zero row is dependent on anything
/// and returns 0 without a projection.
inline double b1_projection_residual(const QpB1Rank &rk, const Vec &a) {
    const double an = a.norm();
    if (!(an > 0.0)) {
        return 0.0;
    }
    if (rk.rows == 0 || rk.rank == 0) {
        return 1.0;
    }
    const Vec q = rk.qr.householderQ().transpose() * a;
    double tail = 0.0;
    for (Index k = rk.rank; k < rk.n; ++k) {
        tail += q(k) * q(k);
    }
    return std::sqrt(tail) / an;
}

/// THE DEPENDENCY COEFFICIENTS `v` with `a = R^T v` (amendment A5), taken
/// from the same decomposition. On an independent `a` this is the
/// least-squares fit and is reported for what it is; on a dependent `a` it is
/// the representation A5 wants to check the leak against.
inline Vec b1_dependency_coefficients(const QpB1Rank &rk, const Vec &a) {
    if (rk.rows == 0) {
        return Vec::Zero(0);
    }
    return rk.qr.solve(a);
}

/// WHAT THE SHARED PREDICATE DECIDED ABOUT ONE CANDIDATE ROW.
struct QpB1Admission {
    /// The site may proceed with the addition.
    bool admit = true;
    /// `-1` not tested / `0` independent / `1` dependent.
    int verdict = -1;
    double residual = std::numeric_limits<double>::quiet_NaN();
    /// The INTEGER screen fired for the set this add would create. It is a
    /// free PRE-CHECK and never a refusal on its own (A8).
    bool by_screen = false;
    /// The invariant held on the INCOMING set. False means the arm has just
    /// disarmed (A9) and the caller is back on production's statements.
    bool rank_ok = true;
};

/// THE INVARIANT CHECK (amendments A2 and A9), on the CURRENT working set.
///
/// `rank(R) == me + |W| <= n`. A violation away from solve entry is
/// `arm_rank_violation`: it is recorded, it DISARMS the arm for the rest of
/// this solve, and the solve continues unarmed. It is not a stop, and the
/// cell's verdict is UNSUPPORTED.
inline bool b1_invariant_holds(const QpProblem &qp, const QpB1Rank &rk) {
    using Adeg = QpDegeneracyArmState;
    const Index deficit = rk.rows - rk.rank;
    Adeg::indep_rows_minus_n_max = std::max(Adeg::indep_rows_minus_n_max, rk.rows - qp.n());
    Adeg::indep_rank_deficit_max = std::max(Adeg::indep_rank_deficit_max, deficit);
    return deficit <= 0 && rk.rows <= qp.n();
}

/// THE SHARED PREDICATE (amendment A3). ONE function, asked at EVERY add site
/// the walk has, so the rule cannot differ between them and so a new site
/// added later has one obvious thing to call.
///
/// IT ASKS THE INVARIANT FIRST. A3's entry repair exists so that the incoming
/// set is independent; if it is not -- which can only mean a site mutated the
/// set without the predicate -- then the QR's verdict about a CANDIDATE is
/// answering the wrong question (the dependency is among the OLD rows), which
/// is exactly what the predecessor's 0.667-to-1.0 residuals on
/// screen-positive candidates were. A9 rules that case: record, disarm,
/// continue.
///
/// THE SCREEN IS A PRE-CHECK, NOT A VERDICT (A8). Under the invariant the two
/// cannot disagree: `me + |W| = n` makes the rows span R^n, so every
/// candidate projects with residual 0 and the QR says DEPENDENT too; below
/// `n` the screen is silent and the QR alone decides. It is recorded because
/// the three-mechanism split A8 asks for needs it.
inline QpB1Admission admit_if_independent(const QpProblem &qp, const WorkingSet &ws, QpB1Rank &rk,
                                          bool is_ineq, Index idx, QpB1Site site) {
    using Adeg = QpDegeneracyArmState;
    const QpDegeneracyParams &prm = Adeg::params;
    QpB1Admission out;
    if (!b1_rank_refresh(qp, ws, rk)) {
        return out; // untestable: ADMIT, never refuse on a test that failed
    }
    if (!b1_invariant_holds(qp, rk)) {
        ++Adeg::indep_rank_violations;
        Adeg::disarm(static_cast<int>(site));
        out.rank_ok = false;
        return out;
    }
    out.by_screen = prm.integer_screen && (rk.rows + 1 > qp.n());
    const Vec a = b1_candidate_row(qp, is_ineq, idx);
    out.residual = b1_projection_residual(rk, a);
    ++Adeg::indep_dep_tests;
    const bool dependent = out.residual <= prm.dependency_tol;
    out.verdict = dependent ? 1 : 0;
    out.admit = !dependent;
    if (dependent) {
        if (out.by_screen) {
            ++Adeg::indep_screen_and_qr_dependent;
        } else {
            ++Adeg::indep_qr_only_dependent;
        }
    }
    return out;
}

/// THE ENTRY REPAIR (amendment A3), run once per solve on the seed working
/// set, before the walk's first pass.
///
/// WHAT IT DOES. If the seed set is already independent -- the ordinary case,
/// and ONE decomposition to establish it -- nothing happens at all. If it is
/// not, the arm rebuilds it: the MANDATORY rows first (equality rows and
/// FIXED bounds, which it may never remove), then every non-mandatory member
/// in canonical order, each admitted only if the QR calls it independent of
/// what has been accepted so far. A member that is not is REMOVED from the
/// working set and counted.
///
/// "REVERSE INGESTION ORDER", DECLARED HONESTLY. A3 asks for removal in
/// reverse ingestion order. `WorkingSet` retains NO ingestion order --
/// `active_ineq()` is kept sorted and `bound_state()` is indexed by variable
/// -- so the arm uses the CANONICAL order of `b1_active_rows_transposed` as
/// the surrogate and keeps the EARLIER member of a dependent pair, which is
/// removal in reverse canonical order. The deviation is declared here rather
/// than papered over, and it is the same order the oracle's own row layout
/// uses, so nothing downstream has to reconcile two orders.
///
/// THE MANDATORY-DEPENDENT PATH. If the mandatory rows are themselves
/// dependent, no repair can reach the invariant: the arm REFUSES that solve
/// (`indep_refused_mandatory_dependent`), disarms, and the solve proceeds
/// UNARMED. That cell's row says so, and its verdict is UNSUPPORTED.
///
/// THE COST. One decomposition when the seed is already independent; on the
/// repairing path one more per ACCEPTED member, all counted in
/// `indep_qr_builds`.
///
/// x IS NOT MOVED. Removing a pin leaves the variable exactly where it sits
/// and merely stops the walk from treating it as fixed, so the repair cannot
/// introduce an infeasibility of its own.
inline void b1_entry_repair(const QpProblem &qp, WorkingSet &ws) {
    using Adeg = QpDegeneracyArmState;
    QpB1Rank rk;
    if (!b1_rank_refresh(qp, ws, rk)) {
        return; // untestable: leave the seed alone
    }
    if (b1_invariant_holds(qp, rk)) {
        return; // already independent -- the ordinary case, one decomposition
    }

    // The MANDATORY skeleton: equality rows (implicit in `qp.me()`) and FIXED
    // bounds. Everything else starts out.
    WorkingSet acc(ws.n(), ws.mi());
    for (Index i = 0; i < qp.n(); ++i) {
        const auto si = static_cast<std::size_t>(i);
        if (ws.bound_state()[si] == BoundState::kFixed) {
            acc.bound_state()[si] = BoundState::kFixed;
        }
    }
    QpB1Rank ak;
    if (!b1_rank_refresh(qp, acc, ak)) {
        return;
    }
    if (ak.rows - ak.rank > 0 || ak.rows > qp.n()) {
        ++Adeg::indep_refused_mandatory_dependent;
        Adeg::disarm(static_cast<int>(QpB1Site::kSeedEntry));
        return;
    }

    const std::vector<Index> seed_rows = ws.active_ineq();
    for (const Index j : seed_rows) {
        const Vec a = b1_candidate_row(qp, /*is_ineq=*/true, j);
        const bool dependent = b1_projection_residual(ak, a) <= Adeg::params.dependency_tol;
        if (dependent) {
            ws.drop_ineq(j);
            ++Adeg::indep_entry_members_removed;
            continue;
        }
        acc.add_ineq(j);
        if (!b1_rank_refresh(qp, acc, ak)) {
            return;
        }
    }
    for (Index i = 0; i < qp.n(); ++i) {
        const auto si = static_cast<std::size_t>(i);
        const BoundState st = ws.bound_state()[si];
        if (st == BoundState::kFree || st == BoundState::kFixed) {
            continue;
        }
        const Vec a = b1_candidate_row(qp, /*is_ineq=*/false, i);
        const bool dependent = b1_projection_residual(ak, a) <= Adeg::params.dependency_tol;
        if (dependent) {
            ws.bound_state()[si] = BoundState::kFree;
            ++Adeg::indep_entry_members_removed;
            continue;
        }
        acc.bound_state()[si] = st;
        if (!b1_rank_refresh(qp, acc, ak)) {
            return;
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
///
/// THE ORACLE IS BUILT HERE, ONCE (amendment A1): one rank-revealing QR of
/// `Rᵀ` for the pass, reused across every candidate the scan examines. The
/// pass has already taken its drop (the drop branch `continue`s above this
/// site), so the set it decomposes is the set the ratio test just ran on.
///
/// `dual_mu` and `lambda_w_inf` are the pass's own regularization parameter
/// and the infinity norm of its working multipliers, passed in rather than
/// re-derived, because amendment A5's leak columns are expressed in units of
/// `μ‖λ_W‖∞` and a second derivation could drift from the one the walk used.
inline QpB1Outcome b1_select_blocker(const QpProblem &qp, const WorkingSet &ws, const Vec &x,
                                     const Vec &Aix, const Vec &p, double dual_mu,
                                     double lambda_w_inf, bool prod_is_ineq, Index prod_idx) {
    using Adeg = QpDegeneracyArmState;
    const QpDegeneracyParams &prm = Adeg::params;
    QpB1Outcome o;
    o.prod_is_ineq = prod_is_ineq;
    o.prod_idx = prod_idx;
    std::vector<QpB1Candidate> cands = b1_candidates(qp, ws, x, Aix, p);
    if (cands.empty() || cands.front().is_ineq != prod_is_ineq || cands.front().idx != prod_idx) {
        ++Adeg::indep_enumeration_mismatches;
        o.mismatch = true;
        return o;
    }
    o.ran = true;
    ++Adeg::indep_passes;
    const double best = cands.front().step;
    for (const QpB1Candidate &c : cands) {
        if (c.step == best) {
            ++o.tie_size;
        }
    }

    // AMENDMENT A5's per-pass denominators, taken ONCE: the retained rows'
    // own leaks `R p`, their worst, the leak bound `μ‖λ_W‖∞`, and `‖p‖₂`.
    const Vec Rp = b1_active_rows_times(qp, ws, p);
    o.max_retained_leak = Rp.size() > 0 ? Rp.cwiseAbs().maxCoeff() : 0.0;
    o.leak_bound = dual_mu * lambda_w_inf;
    o.p_norm = p.norm();

    const Index qr_before = Adeg::indep_qr_builds;
    QpB1Rank &rk = b1_site_rank();
    const QpB1Candidate *pick = nullptr;
    std::size_t next = 0;
    for (std::size_t k = 0; k < cands.size(); ++k) {
        if (prm.max_dep_tests_per_pass > 0 && o.examined >= prm.max_dep_tests_per_pass) {
            o.truncated = true;
            ++Adeg::indep_budget_truncations;
            break;
        }
        QpB1Candidate &c = cands[k];
        const QpB1Admission adm =
            admit_if_independent(qp, ws, rk, c.is_ineq, c.idx, QpB1Site::kOrdinaryAdd);
        if (!adm.rank_ok) {
            // AMENDMENT A9. The INCOMING set is rank-deficient, so no verdict
            // about a CANDIDATE means anything: the dependency is among the
            // rows already there. The arm has disarmed itself; production's
            // choice stands, untouched, for this pass and every pass after it
            // in this solve.
            o.disarmed = true;
            o.rows = rk.rows;
            o.rank = rk.rank;
            o.qr_builds = Adeg::indep_qr_builds - qr_before;
            o.switched = false;
            return o;
        }
        c.verdict = adm.verdict;
        c.dep_residual = adm.residual;
        c.by_screen = adm.by_screen;
        // A5's columns, on every candidate the oracle looked at.
        const Vec a = b1_candidate_row(qp, c.is_ineq, c.idx);
        c.a_norm = a.norm();
        const double den = c.a_norm * o.p_norm;
        c.a_dot_p_normalized =
            den > 0.0 ? c.a_dot_p / den : std::numeric_limits<double>::quiet_NaN();
        c.leak_ratio = o.leak_bound > 0.0 ? c.a_dot_p / o.leak_bound
                                          : std::numeric_limits<double>::quiet_NaN();
        if (adm.verdict == 1 && rk.ok) {
            // "IS THE CANDIDATE'S DERIVATIVE RECONSTRUCTED BY THE RETAINED
            // ROWS' LEAKS?" -- A5's question, answered by the coefficients
            // from the same decomposition: `a = Rᵀv` gives `aᵀp = vᵀ(R p)`
            // exactly in exact arithmetic, so the gap IS the answer.
            const Vec v = b1_dependency_coefficients(rk, a);
            if (v.size() == Rp.size()) {
                c.coeff_residual = std::abs(c.a_dot_p - v.dot(Rp));
            }
        }
        ++o.examined;
        next = k + 1;
        if (adm.admit) {
            // INDEPENDENT (`0`) or UNTESTABLE (`-1`): admitted. An untestable
            // candidate is admitted deliberately -- a test that could not be
            // taken may not refuse a constraint.
            pick = &cands[k];
            break;
        }
        if (c.by_screen) {
            ++o.screen_and_qr_passed;
        } else {
            ++o.qr_only_passed;
        }
        o.passed.push_back(c);
        ++o.passed_over;
    }
    o.rows = rk.rows;
    o.rank = rk.rank;
    o.qr_builds = Adeg::indep_qr_builds - qr_before;
    if (pick == nullptr && o.truncated && next < cands.size()) {
        // The budget ran out. Take the first UNEXAMINED candidate unrefused,
        // so a truncation can only move the arm back toward production.
        pick = &cands[next];
    }
    Adeg::indep_candidates_examined += o.examined;
    Adeg::indep_candidates_passed_over += o.passed_over;
    if (pick == nullptr) {
        // EVERY candidate is dependent. Admit nothing and take the full step
        // -- `ratio_test`'s `cap_at_unit` convention with no blocker left.
        o.has_blocker = false;
        o.alpha = 1.0;
        ++Adeg::indep_no_blocker_passes;
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
        ++Adeg::indep_blocker_switched;
    }
    return o;
}

/// AMENDMENT A10's PRE-REGISTERED BOUND, on ONE passed-over constraint.
///
/// A pass-over is justified by the claim that the candidate blocked only
/// because the direction LEAKED through it. If that is what happened, the
/// violation the step then commits is bounded by the leak itself:
///
///     violation  <=  (μ‖λ_W‖∞ + ε‖a‖₂‖p‖₂) · α · 10
///
/// with the factor of 10 declared in advance. A LARGER violation means the
/// candidate was not merely leaking -- it was a real blocking constraint the
/// arm refused -- which is a FINDING, and that solve's b1 verdict is
/// UNSUPPORTED rather than CONVERTED.
inline bool b1_leak_bound_holds(double violation, double leak_bound, double a_norm, double p_norm,
                                double alpha) {
    const double eps = std::numeric_limits<double>::epsilon();
    const double bound = (leak_bound + eps * a_norm * p_norm) * std::abs(alpha) * 10.0;
    return violation <= bound;
}

/// The largest amount by which the constraints the arm passed over are
/// violated at the iterate the walk actually moved to. Zero when nothing was
/// passed over, and zero when every passed-over constraint is still satisfied.
///
/// IT ALSO APPLIES AMENDMENT A10's PRE-REGISTERED BOUND, per constraint, and
/// counts every failure: a violation larger than the leak can account for is
/// a finding about the RULE, not a number to report and move past. `exceeded`
/// receives this pass's count.
inline double b1_passed_over_violation(const QpProblem &qp, const Vec &x_new, const QpB1Outcome &o,
                                       Index *exceeded) {
    double worst = 0.0;
    Index bad = 0;
    for (const QpB1Candidate &c : o.passed) {
        const double v = c.is_ineq
                             ? qp.Ai.row(c.idx).dot(x_new) - qp.bi(c.idx)
                             : (c.state == BoundState::kAtUpper ? x_new(c.idx) - qp.upper(c.idx)
                                                                : qp.lower(c.idx) - x_new(c.idx));
        worst = std::max(worst, v);
        if (v > 0.0 && !b1_leak_bound_holds(v, o.leak_bound, c.a_norm, o.p_norm, o.alpha)) {
            ++bad;
            ++QpDegeneracyArmState::indep_leak_bound_exceeded;
        }
    }
    if (exceeded != nullptr) {
        *exceeded = bad;
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
// AMENDMENT A4 DROPS b2 OUTRIGHT FOR M5 AND FIXES ITS REFERENCE: the
// registered item is Gill & Wong (2015) §4's MULTIPLIER-AWARE exchange, with
// its sign conditions and its prohibition on removing mandatory rows -- not
// B.3's "largest dependency coefficient", which the review's item 5 showed is
// not theorem-backed. The three implementability reasons above stand
// unchanged and are why the item is registered rather than built.
//
// SO IT IS REGISTERED: the exchange is a PRODUCTION design item, alongside
// B.2's "one triangular solve against the retained factor" cost target. b1 is
// the minimal intervention that tests the causal claim -- that dependent adds
// drive the drop/re-add recurrence -- and it needs none of the above.

} // namespace hven::solvers::detail::testing

#endif // HVEN_TESTING
