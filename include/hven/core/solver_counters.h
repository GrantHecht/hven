// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// The solver counter contract's types: QpCounters (one QP engine solve),
// SsnCounters (the semismooth-Newton kernel's own work), IpqpCounters (the
// IP-PMM interior-point tier's own work) and SqpCounters (one whole SQP
// driver solve, which aggregates all three).

#include <limits>

#include <hven/core/start_level.h>

namespace hven::solvers {

/// QP solver performance counters.
///
/// THE TWO REFINEMENT COUNTERS COUNT DIFFERENT THINGS ON PURPOSE, because the
/// two paths have different mandatory baselines and the interesting quantity
/// on each is "steps that would not have been taken before":
///
/// - border_refine_steps: TOTAL refinement steps KEPT by every
///   solve_bordered_eqp call in this solve, INCLUDING the mandatory first one
///   -- the same accounting detail::kMaxBorderRefineSteps uses ("total,
///   including the mandatory first"). A solve that goes through the border
///   path at all therefore reports at least one step per bordered EQP solve,
///   and the excess over that baseline is what the path's iterated refinement
///   loop buys.
/// - eqp_refine_steps: EXTRA refinement steps kept by every solve_eqp call in
///   this solve, BEYOND the single unconditional step solve_eqp takes. IT IS
///   IDENTICALLY ZERO, and is kept as an instrumented invariant rather than
///   as a live measurement: the flag-gated iterated loop it counted
///   (QpOptions::eqp_refine) was DELETED once both shipped backends measured
///   it inert -- 0 steps across 27 HS problems x 2 tolerance regimes x 2
///   algebra modes plus 13 adversarial probes, on MKL and on Accelerate
///   independently, with a structural identity (r_k = diag(reg)*c) explaining
///   why a path whose first solve is a genuine regularized solve cannot fire
///   the shared stopping rule. A nonzero reading here would mean solve_eqp
///   grew a second refinement step, which is a change, not a measurement --
///   which is exactly why the counter and its assertions stayed behind when
///   the loop went.
///
/// "KEPT" is the operative word in both: a candidate step rejected by the
/// strict-decrease acceptance rule is discarded and NOT counted, so these
/// count steps that moved the answer, not solves attempted.
struct QpCounters {
    Index factorizations = 0;
    Index schur_updates = 0;
    Index minor_iters = 0;
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    /// Rungs of the SUSPECT-STALL ESCALATION LADDER this solve spent (see
    /// qp_engine.h's section 4b). Each rung multiplies the effective
    /// primal_delta by detail::kSuspectDeltaFactor after a would-be-kOptimal
    /// exit off a kSuspect factorization failed the free-block stationarity
    /// check. Zero on every solve whose linear algebra was trustworthy, which
    /// is the overwhelming majority; a solve that reports kNumericalError with
    /// this at detail::kMaxSuspectEscalations exhausted the ladder, and that
    /// pairing IS the exhaustion diagnostic.
    Index suspect_escalations = 0;

    /// True iff qp_engine.h's border-mode reuse gate (`reuse_eligible` in
    /// QpEngine::run(), conditions (a)-(e)) judged the persisted K0/border
    /// cache trustworthy for THIS solve() call -- i.e. whether the engine
    /// actually skipped rebuilding K0 on the strength of its OWN or an
    /// ADOPTED hot-start handle's history, not merely whether
    /// `factorizations` happens to read 0. THE TWO ARE NOT THE SAME CLAIM:
    /// `factorizations == 0` can also arise from a reduced system with
    /// nothing to factorize at all (every variable pinned, no equalities, no
    /// working rows -- the elimination path's empty-system short-circuit,
    /// reachable from border mode too) or from a solve that never reached the
    /// loop (a crossed-bounds box, reported kInfeasible), NEITHER of which
    /// says anything about the reuse cache. This field is the direct signal a
    /// caller should read instead of inferring reuse from the factorization
    /// count. Always false under QpOptions::ws_algebra == kRefactorize, where
    /// the border-mode cache does not exist.
    bool k0_reused = false;

    /// Number of backend SYMBOLIC-ANALYSIS calls this solve() call actually
    /// paid for -- i.e. the number of times qp_engine.h's rebuild_k0() found
    /// the analysis decision `needed` before calling `factorize_checked()`
    /// (which skips the analysis whenever the sparsity pattern is unchanged
    /// from the last analyzed matrix -- kkt_calls.h; handing that decision in
    /// rather than letting it be retaken changes when the pattern is hashed
    /// and nothing else). Counted here, at the call site, rather than inside
    /// the factor, so this stays a QP-engine-level observable like every
    /// other QpCounters field and touches no MKL-adjacent code.
    ///
    /// THE REGRESSION THIS EXISTS TO CATCH: detaching onto a FRESH
    /// BorderState (a fresh KktFactor, with no cached pattern) on every
    /// value-changing major of an ordinary, NEVER-SHARED solve paid a full
    /// re-analysis per major -- measured at 48 on HS38's own 48-major solve,
    /// vs 1 for a solve whose sole-owner rebuilds reuse the SAME KktFactor's
    /// cached pattern. A never-shared solve should show this at 1 (or 0, if
    /// the pattern never needed rebuilding at all) regardless of how many
    /// `factorizations` it pays; a solve whose fixture legitimately forces
    /// detaches (the sharing/identity-mismatch tests) is expected to show
    /// more.
    Index symbolic_analyses = 0;

    // ---------------------------------------------------------------------
    // THE WORKING-SET WALK COUNTERS (eleven in total: the five immediately
    // below and the six after them).
    //
    // PURELY OBSERVATIONAL. None of the ELEVEN walk counters is read by any
    // decision in qp_engine.h; they are written and never consulted, so the
    // walk's trajectory is bit-for-bit what it was before they existed
    // (verified: `hven_sqp_bench --self-check` byte-exact and the full suite
    // green in both build configurations at the commit that added them).
    //
    // WHY THEY EXIST: every pre-existing field above counts LINEAR-ALGEBRA
    // events (factorizations, Schur updates, symbolic analyses, refinement
    // steps), while the candidate mechanisms behind a wide-window minor stall
    // -- a degenerate stall, an add/drop churn cycle, homotopy thrash, or a
    // genuinely long monotone identification walk -- are statements about the
    // COMBINATORIAL walk, which nothing observed. `minor_iters` alone cannot
    // tell a solve that spent 510165 minors cycling from one that spent them
    // making progress. These five close exactly that gap and nothing wider;
    // the six below close a second one -- these five cannot tell an
    // inequality ROW from a variable BOUND.
    //
    // THE ARITHMETIC A READER SHOULD DO WITH THEM. For a solve that ends at
    // a working set of W members having started from a seed of S:
    //
    //     ws_adds - ws_drops + shift_adds  ==  W - S   (net identity)
    //
    // so `ws_drops` is the CHURN: a monotone identification walk that never
    // backtracks has ws_drops == 0 and ws_adds ~ W - S, while a walk that
    // pays k re-discoveries of the same rows has ws_adds ~ (W - S) + k and
    // ws_drops ~ k. The ratio ws_drops / minor_iters is therefore the direct
    // churn fraction, and degenerate_steps / minor_iters the direct
    // degeneracy fraction; the two are independent and a stall can be either,
    // both, or neither (in which case the walk is simply long).
    //
    // **THE IDENTITY HOLDS ON THE CONVEX PATH, AND HAS EXACTLY ONE NAMED
    // EXCEPTION**: qp_engine.h's TEMPORARY-VERTEX START REPAIR (section 4b,
    // `repair_temporary_vertex` -> `pin_at_best_bound`) writes
    // `ws.bound_state()` DIRECTLY -- it pins variables onto bounds, and its
    // release pass unpins them -- WITHOUT touching QpCounters and without
    // marking the variable in the seen-set. So on any solve that ran the
    // repair:
    //   - the net identity above is off by the number of pins the repair left
    //     standing (S, the seed, is effectively enlarged by them);
    //   - `ws_adds_bound`/`ws_drops_bound` do not include the repair's pins or
    //     its releases;
    //   - `distinct_bound_added` does not count a variable whose ONLY
    //     admission was a repair pin, so the bound-repeat ratio below is
    //     computed over a set that excludes that route.
    // The repair runs only at `iter == 0` and only on a subproblem whose
    // inertia probe returned kWrong -- i.e. an INDEFINITE start vertex -- so
    // every convex corpus reports these fields exactly, which is why the
    // omission never shows up in a measurement. It is documented rather than
    // plumbed deliberately: the counters exist to describe the COMBINATORIAL
    // WALK, and the repair is a pre-walk vertex construction, not a step of
    // it -- adding it to `ws_adds` would make the churn fraction read a
    // start-point property as walk churn. A reading that needs the repair's
    // pins must say so and count them separately.

    /// BLOCKING-CONSTRAINT ADDITIONS: one per minor iteration whose ratio
    /// test named a blocker that then joined the working set, counting an
    /// inequality row and a variable bound alike (BlockKind::kIneq /
    /// kBound). The negative-curvature ride's own blocker (section 4c) is
    /// counted here too, since it is the same ratio test reporting the same
    /// kind of event; a minor that took a full unblocked step to the EQP
    /// point (kind == kNone) adds nothing and is counted nowhere.
    Index ws_adds = 0;

    /// WORKING-SET REMOVALS: one per constraint released by the Dantzig drop
    /// rule (drop_worst) plus one per constraint released by the
    /// zero-multiplier probe (probe_zero_multiplier_drops). A probe drop
    /// that is RESTORED because the probe declined it is not counted -- the
    /// working set is the same on both sides of it, so counting it would
    /// break the net identity above.
    Index ws_drops = 0;

    /// HOMOTOPY ADMISSIONS: rows that refresh_shifts() put into the working
    /// set because they are still violated (shift > 0), a structurally
    /// different event from a blocking-constraint add -- the drop rule
    /// deliberately SKIPS shifted rows, so a row admitted this way cannot
    /// leave until its shift reaches zero. INCLUDES the pre-loop
    /// refresh_shifts() call, i.e. the seed's own initial admission, which
    /// is why a healthy solve reports a large value here on its first QP and
    /// near-zero after.
    Index shift_adds = 0;

    /// DEGENERATE PIVOTS: minor iterations that added a blocking constraint
    /// while moving the iterate by no more than the loop's own step
    /// tolerance (alpha * ||p||inf <= feas_tol * max(1, ||x||inf) -- the
    /// SAME threshold the loop's KKT-point test uses one line earlier, so
    /// the two are consistent by construction rather than by a new
    /// tolerance). The textbook degeneracy signal: the working set grew but
    /// the objective could not have improved. qp_engine.h implements NO
    /// anti-cycling rule (no Bland, no Harris, no EXPAND, no perturbation)
    /// and says so; this field is the first observation of whether that
    /// omission ever costs anything on a real corpus.
    Index degenerate_steps = 0;

    /// The LONGEST CONSECUTIVE run of degenerate steps in this solve, where
    /// "consecutive" means back-to-back minor iterations. **A RUN IS BROKEN
    /// BY A MINOR ON WHICH THE ITERATE MOVED, AND BY NOTHING ELSE**: an
    /// ordinary non-degenerate step, a taken RIDE, and a START REPAIR each
    /// reset it (that is exactly where qp_engine.h resets it), while a DROP
    /// iteration and a ZERO-MULTIPLIER PROBE do NOT -- both snap x onto an
    /// EQP point already within step_tol of where it was, so the iterate did
    /// not move and a run that spans them is still one stall. **SO A LARGE
    /// VALUE HERE IS "the longest stretch on which the iterate did not
    /// move", NOT "an unbroken run of back-to-back degenerate ADDS"**, and
    /// any reading of a measured figure has to say which of the two it
    /// means (`degenerate_steps` above is unaffected: it counts the
    /// degenerate adds themselves and no run logic enters it.) This is the
    /// field that tells the two degeneracy pictures apart, and they call for
    /// different remedies: degenerate steps SPRINKLED through a healthy walk
    /// are the ordinary cost of a vertex with more active constraints than
    /// dimensions and want nothing done about them, while a long unbroken
    /// run is an iterate that has STOPPED MOVING while the working set keeps
    /// changing -- the cycling class, and the only class an anti-cycling
    /// rule (Bland/Harris/EXPAND) would address. Reported rather than acted
    /// on: qp_engine.h implements no anti-cycling rule and this field
    /// changes no decision.
    Index degenerate_run_max = 0;

    // THE SIX FIELDS THAT MAKE THE ABOVE TRACEABLE RATHER THAN INFERRED:
    // ws_adds and ws_drops MERGE inequality rows with variable bounds and
    // carry no constraint IDENTITY, so "the walk RE-DISCOVERS its rows"
    // cannot be concluded from ws_adds >> |W*| alone -- a walk touching many
    // DISTINCT bounds once each fits the same numbers. These six settle it
    // by direct measurement.

    /// Of `ws_adds`, how many were VARIABLE-BOUND pins (BlockKind::kBound).
    /// The inequality-row half is ws_adds - ws_adds_bound. `shift_adds`
    /// needs no such split: refresh_shifts() only ever adds inequality rows.
    Index ws_adds_bound = 0;

    /// Of `ws_drops`, how many released a variable bound rather than an
    /// inequality row, on both drop routes.
    Index ws_drops_bound = 0;

    /// How many DISTINCT inequality rows this solve ever put into the
    /// working set, by ANY route (blocking add, ride, or homotopy
    /// admission). The mean number of times a touched row was admitted is
    ///
    ///     (ws_adds - ws_adds_bound + shift_adds) / distinct_ineq_added
    ///
    /// **THAT RATIO IS ADMISSION MULTIPLICITY, WHICH COUNTS EACH
    /// CONSTRAINT'S FIRST ADMISSION** -- so the number of RE-admissions is
    /// the ratio MINUS ONE, and a reading must say which of the two it
    /// quotes. A value near 1 means "touched once, never repeated"; a value
    /// well above 1 means re-discovery, per constraint class separately.
    Index distinct_ineq_added = 0;

    /// How many DISTINCT variables this solve ever put into the working
    /// set, by ANY route; the bound-side analogue of distinct_ineq_added
    /// (admission multiplicity = ws_adds_bound / distinct_bound_added, and
    /// re-admissions are that minus one). Additionally EXCLUDES the start
    /// repair's pins -- see the named exception under the walk-counter
    /// banner above.
    Index distinct_bound_added = 0;

    /// Drop decisions (drop_worst) in which TWO OR MORE candidates fell
    /// inside the rule's relative tie window (detail::kEngineDropTieTol),
    /// i.e. decisions actually settled by the largest-angle/first-index
    /// tie-break rather than by the most-negative multiplier.
    Index drop_ties = 0;

    /// Ratio tests whose blocking constraint was chosen from TWO OR MORE
    /// candidates at EXACTLY the same minimum ratio, i.e. settled by the
    /// scan order (inequality rows before bounds, ascending index) rather
    /// than by the ratio. EXACT equality, deliberately: the tie that
    /// matters for degeneracy is several constraints at zero slack, and
    /// those compare bit-equal. NEAR-ties are not measured -- counting them
    /// needs a second pass over every candidate on every minor, a real cost
    /// in a loop that runs half a million times.
    Index ratio_ties = 0;
};

/// Work counters for the semismooth-Newton kernel. One instance lives inside
/// SsnResult (ssn_engine.h) describing ONE QP solve; a second lives inside
/// SqpCounters below, aggregated over every subproblem of a whole SQP solve,
/// exactly as qp_minor_iters/factorizations aggregate the walk's QpCounters.
///
/// PURELY OBSERVATIONAL, like the eleven walk counters above: nothing in
/// ssn_engine.h reads any of these back to make a decision, so writing them
/// cannot move a trajectory.
///
/// THREE OF THE SIX CORE FIELDS BELONG TO THE SAFEGUARDED ITERATION ALONE:
/// ssn_backtracks, ssn_prox_updates and ssn_uncertain_peak move only under
/// SsnSafeguards::kFull -- the production iteration with line search,
/// proximal ladder and uncertain set (ssn_engine.h's SCOPE banner) -- and are
/// structurally 0 under kBare, the bare local method (full undamped Newton
/// steps).
struct SsnCounters {
    /// Newton steps TAKEN. The convergence test runs BEFORE each step, so a
    /// start point already inside fb_tol reports 0 here.
    ///
    /// **IT DOES NOT EQUAL THE FACTORIZATION COUNT.** The invariant is
    ///
    ///     ssn_iters <= factorizations,
    ///
    /// for two reasons, both in ssn_engine.h: an ATTEMPT can pay its
    /// factorization and then take no step (a rejected line search, a wrong
    /// inertia), and under SsnSafeguards::kFull a CERTIFYING exit pays one
    /// more for the second-order verification read at the point it certifies
    /// (ssn_engine.h section 7b). Under kBare equality holds exactly,
    /// provided nothing intervened. Anything downstream that divides one by
    /// the other, or that reads either as the other, must say which it
    /// means.
    Index ssn_iters = 0;

    /// Newton steps whose IMPLIED ACTIVE SET differed from the preceding
    /// step's -- one per such step, not one per constraint that moved. The
    /// implied set is the partition the generalized Jacobian itself selects
    /// (row k active iff its FB pair has lambda_k > s_k, equivalently iff
    /// alpha_k > beta_k -- see ssn_engine.h), and on the FIRST step it is
    /// the caller's activity hint when one was supplied. This is the counter
    /// that distinguishes the two failure pictures the walk could not tell
    /// apart: a large value against a small iteration count is a set that
    /// keeps changing wholesale (the thrash class), while zero flips after
    /// the first step is the identification the kernel exists to buy.
    Index ssn_bulk_flips = 0;

    /// Line-search backtracks (kFull). Structurally 0 under kBare, which
    /// takes full steps unconditionally.
    Index ssn_backtracks = 0;

    /// Proximal-center/sigma updates (kFull's proximal ladder).
    /// Structurally 0 under kBare, which has no proximal term.
    Index ssn_prox_updates = 0;

    /// Solves that ended on an SsnEscape other than kNone (ssn_engine.h).
    /// At the SsnResult scale this is 0 or 1; at the SqpCounters scale it is
    /// the number of subproblems that escaped.
    Index ssn_escapes = 0;

    /// Peak size of the UNCERTAIN SET (rows the safeguarded method declines
    /// to assign to either branch), under kFull. Structurally 0 under
    /// kBare, which has no uncertain set at all.
    Index ssn_uncertain_peak = 0;

    // THE TIER-3 STABLE-FACE REFINEMENT, both polarities.
    //
    // Certifying SSN exits whose identified face was re-solved EXACTLY
    // (QpEngine::refine_on_face) and whose refined point the driver then used
    // as the step, and certifying exits where that solve was REFUSED (the face
    // was not of full rank, its KKT system failed the inertia gate, or the
    // refined point left the subproblem's own box / trust region / inactive
    // rows). Their SUM is the number of certifying SSN exits this solve made
    // -- WHEN SqpOptions::ssn_certify_from_face is off (the shipped default,
    // unconditionally true on every corpus cell run to date). Under that
    // opt-in lever the face solve is hoisted ahead of the usability gate, and
    // sqp_driver.h's charge_refused_face_refinement can increment
    // `ssn_refine_refused` on the one path where the exit is REFUSED and then
    // WITHDRAWN into an escape rather than a certificate -- so the sum is an
    // upper bound on certifying exits under that lever, not an identity.
    // Unreachable on every shipped corpus (0 withdrawals); see that
    // function's own comment for the dynamic.
    //
    // A REFUSAL IS NOT AN ERROR: the caller keeps the certificate the SSN tier
    // already gave it, exactly as it did before this tier existed. What a
    // refusal DOES mean is that this subproblem's complementarity is back to
    // the `fb_tol * ||lambda||inf` bound rather than the walk-exact identity --
    // see sqp_driver.h's WHAT IS MEASURED BUT NOT GATED note, which states the
    // per-mode guarantee. Each refinement costs ONE factorization, folded into
    // `SqpCounters::factorizations` like every other, so `ssn_refinements <=
    // factorizations` holds alongside `ssn_iters <= factorizations`.

    /// Exact face re-solves whose refined point was used as the step; see
    /// the tier-3 note above for the sum's meaning under both lever states.
    Index ssn_refinements = 0;

    /// Certifying exits where the face re-solve was REFUSED; see the tier-3
    /// note above. Driver-scale only (no SsnResult ever writes it).
    Index ssn_refine_refused = 0;

    // INSTRUMENT ONLY, both of them, and both driver-scale like the pair
    // above (no SsnResult ever writes either; the engine does not know the
    // refinement exists). They exist because neither measurement below is
    // answerable by arithmetic on the pair above -- doing so would be WRONG
    // rather than merely coarse.
    //
    // `ssn_refine_factorizations` -- the factorizations
    // QpEngine::refine_on_face ITSELF paid, summed over every attempt,
    // accepted or refused. It is NOT `ssn_refinements +
    // ssn_refine_refused`: refine_on_face short-circuits an EMPTY face and
    // fails its rank pre-screen BEFORE any factorization, so a refusal can
    // cost 0 (its own contract says both screens precede the solve). "The
    // refinement's share of total factorizations at corpus scale" is this
    // field over SqpCounters::factorizations -- a ratio of two measured
    // quantities rather than a count standing in for a cost.
    //
    // `ssn_refine_neg_duals` -- STRICTLY NEGATIVE inequality multipliers
    // ADOPTED from an ACCEPTED refinement, summed over rows and over
    // refinements (a refinement adopting three negative prices contributes
    // 3). Before the tier-3 refinement a certifying SSN exit guaranteed
    // lambda >= -O(fb_tol) by the FB row structure; after it the adopted
    // multipliers are `price(...)`'s output on the caller's face, UNBOUNDED
    // IN SIGN -- with no driver-side analogue of the walk's drop rule to
    // re-gate them. This is the sign half of measuring dual sign on the
    // scale corpus explicitly rather than inferring it from HS (the
    // magnitude half is the corpus row's own model-level KKT `dual_sign`).
    // NO TOLERANCE: the test is `< 0.0`, because a sign is a sign, and a
    // threshold here would be a new tuning constant on an observational
    // field.

    /// Factorizations refine_on_face itself paid, accepted or refused; see
    /// the instrument-only note above for why it is not the pair sum.
    Index ssn_refine_factorizations = 0;

    /// Strictly negative multipliers adopted from accepted refinements,
    /// summed over rows and refinements; NO TOLERANCE (`< 0.0`); see the
    /// instrument-only note above.
    Index ssn_refine_neg_duals = 0;

    // THE R6 SIGN SWEEP (M6 W0.3). REPAIR, not instrument -- the only pair
    // here that reports a value this driver CHANGED.
    //
    // WHAT IS WRONG WITHOUT IT. A certifying SSN exit's inequality prices are
    // not sign-constrained where they are produced: the FB kernel bounds them
    // only by `lambda >= -O(fb_tol)`, and the tier-3 refinement's `price(...)`
    // is unbounded in sign outright. Nothing downstream re-gates them, so a
    // negative price becomes `SqpSolution::lambda_i`, becomes
    // `WarmStart::lambda_i`, and through the currency's `iq_lmults_` becomes
    // an interior-point seed -- where the barrier's own
    // [kSeededIqMultFloor, kSeededMultInitMax] clamp absorbs it as though it
    // were near-zero noise. That is the leak R6 closes, and it is the one the
    // `"hven.ipm.polish.v1"` staging refusal already makes loud on the two
    // BOUND-dual blocks and cannot see on this one.
    //
    // WHERE IT RUNS: `SqpDriver::finish`, the single export boundary, on the
    // solution's and the warm start's multipliers together. NOT at the point
    // a face price is adopted into the iteration -- see
    // sqp_driver.h's sweep contract for the measurement that ruled that
    // placement out.
    //
    // NO TOLERANCE, exactly as `ssn_refine_neg_duals` above: the test is
    // `< 0.0`. A magnitude threshold here would be a tuning constant on a
    // sign.
    //
    // WHAT IT COSTS, AND WHY THE PAIR IS THE ONLY WAY TO SEE IT. Clamping a
    // price the stationarity condition was using perturbs that condition by at
    // most `ssn_sign_sweep_max * ||Ji||inf` over the swept rows -- and the
    // residuals in `SqpSolution::kkt` were computed BEFORE the sweep, at the
    // multipliers the solver actually reached. So on a solve with
    // `ssn_sign_swept > 0` the reported stationarity is OPTIMISTIC by at most
    // that bound, and an independent re-scoring of the returned point (the
    // corpus row's own model-level `kkt_stationarity`) will read the larger
    // value. This pair is what makes that gap computable rather than hidden;
    // a solve reporting `ssn_sign_swept == 0` carries no such gap at all.
    //
    // STRUCTURALLY ZERO UNDER kWalk, like the refinement pair above: an
    // active-set price is non-negative by the walk's own drop rule, so the
    // sweep is inert on that arm even though it is not conditioned on the
    // mode. That is also why the pair stays zero across a RESTORATION fold --
    // the sub-solve is walk-only -- rather than because no sweep ran there.
    //
    // SCOPE OF THE DRIVER-SCALE VALUES: they span the solve AND its
    // restoration sub-solve, which `accumulate_ssn_counters` folds in. The
    // count is that whole solve's total and the peak is its maximum.

    /// Strictly negative inequality prices clamped to 0 at export, summed
    /// over rows and over solves; NO TOLERANCE (`< 0.0`).
    Index ssn_sign_swept = 0;

    /// The largest magnitude clamped by that sweep (0.0 when nothing was
    /// swept). A PEAK, folded with `max` like `ssn_uncertain_peak` and unlike
    /// every summed field beside it -- it is the bound on how far the reported
    /// stationarity can be optimistic (see the R6 note above), which a sum
    /// would destroy.
    double ssn_sign_sweep_max = 0.0;

    // THE ESCAPE-REASON CENSUS. OBSERVATIONAL, like every field above.
    //
    // WHY: bare `ssn_escapes` counts hand-offs and says nothing about WHY,
    // which left the false-`kInfeasible` rate under combined extreme scaling
    // and the infeasible-trust-region-fixture mislabel untestable on anything
    // but hand-built fixtures -- both are statements about the DISTRIBUTION of
    // escape reasons.
    //
    // A COUNT PER REASON, not a "last reason": an SQP solve escapes on many
    // subproblems and a single label would lose every one but one.
    //
    // THE SIX PARTITION `ssn_escapes` EXACTLY, and that is an asserted
    // invariant rather than a convention -- see
    // SqpDriverSsnMode.TheEscapeReasonCensusPartitionsTheEscapeCount. Five map
    // one-to-one onto the non-`kNone` values of `SsnEscape` (ssn_engine.h);
    // the sixth has no `SsnEscape` value at all and is the reason this census
    // could not be a bare enum histogram:
    //
    //   `ssn_escape_gate_refused` -- the DRIVER's own refusal. A subproblem
    //   whose kernel reported `escape_reason == kNone` but whose exit
    //   sqp_driver.h's `ssn_exit_is_a_usable_step` declined (a certifying
    //   exit that left the trust region by more than the derived bound) went
    //   to the walk exactly like an escaped one did, and `ssn_escapes`
    //   already counts it at the driver scale. DRIVER-SCALE ONLY: no
    //   SsnResult ever carries a nonzero one, exactly like
    //   `ssn_refinements`/`ssn_refine_refused` above.
    //
    //   **AND IT IS STRUCTURALLY ZERO ON THE SHIPPED PATH, WHICH IS NOT THE
    //   SAME AS UNUSED.** It is written at the branch sqp_driver.h's
    //   `kSsnTrViolationFactor` note calls "PROVABLY INERT ON THE CERTIFYING
    //   PATH": the gate's bound is derived from the kernel's own
    //   `|phi| <= fb_tol`, so no certifying exit ssn_engine.h can produce
    //   reaches it, and no NLP fixture can drive it. A reader must NOT read
    //   a zero here as "measured, and the gate never refused" on a corpus
    //   where nothing could have refused. The bucket exists for the same
    //   reason the `ssn_escapes` increment beside it does: it is the one
    //   place a non-escape hand-off is counted, and if that derivation ever
    //   moves, the census must not silently stop partitioning.
    //
    // The other five are written by the ENGINE, beside its own `ssn_escapes`
    // increment, so a bare-kernel probe (bench/ssn_safeguard_probe.cpp) reads
    // the same census the driver aggregates.
    //
    // NO IN-MEMORY ABSENT SENTINEL: bench_corpus.cpp's `--from-csv` reader
    // stamps `-1` on these six fields for a pre-schema-37 artifact that
    // never measured them, but that ABSENT convention exists ONLY at the CSV
    // boundary -- these are plain `Index`, not an optional/sentinel type, so
    // a `-1`-stamped SsnCounters that reached accumulate_ssn_counters
    // (sqp_driver.h) would silently SUM to -6 rather than signal
    // "unmeasured". Not a live path today (the scorer never accumulates the
    // census fields, and a `--from-csv` merge re-emits `-1` verbatim rather
    // than summing it), but a future caller that folds a CSV-sourced
    // SsnCounters into a running total must re-check for the sentinel first.
    Index ssn_escape_budget = 0;
    Index ssn_escape_singular = 0;
    Index ssn_escape_no_contraction = 0;
    Index ssn_escape_infeasible_suspect = 0;
    Index ssn_escape_indefinite = 0;
    Index ssn_escape_gate_refused = 0;
};

/// Work counters for the IP-PMM interior-point tier (M6 W1,
/// `docs/notes/2026-08-m6-w1-ipqp-spec.md` section 7, as amended by the plan
/// section 7 notes b/e). One instance describes ONE tier subproblem solve,
/// exactly as QpCounters/SsnCounters do; a second lives inside SqpCounters as
/// `ipqp`, aggregated over every subproblem of a whole SQP solve by
/// `accumulate_ipqp_counters` (`sqp_driver.cpp`), mirroring
/// `accumulate_ssn_counters`.
///
/// LANDS INERT (M6 W1 task 2): nothing writes these fields yet. Populated
/// from task 4 onward; dispatched from task 6 onward.
///
/// DNF SENTINEL (spec section 7): a sweep row for a subproblem that did not
/// finish records its double-valued fields here as `1e6`, never left blank.
/// That is a convention for the sweep/CSV boundary a later task writes, not
/// logic this struct or `accumulate_ipqp_counters` implements -- a live
/// solve's counters never carry it.
///
/// FOLD RULE, stated once here rather than per field: every `Index` field
/// SUMS across subproblems, exactly like `accumulate_ssn_counters`, with
/// FOUR exceptions -- two PER-SUBPROBLEM STATUS fields, OVERWRITTEN (the
/// `SqpCounters::start_level_used` convention for a categorical reading
/// rather than an additive one): `ipqp_rho_demanded_last` and
/// `ipqp_final_inertia_read`. One `double` PEAK pair max-folded
/// (`ipqp_rho_demanded_max`, `ipqp_restart_shift_max`, model:
/// `ssn_sign_sweep_max` above) plus one min-folded pair
/// (`ipqp_alpha_p_min`, `ipqp_alpha_d_min`). And one `Index` MARKER
/// max-folded: `ipqp_tier_retired_after` (fix round 1: max, not sum, since
/// it is a once-per-solve major index, not a count, and summing two nonzero
/// readings would report an impossible major). Each exception is restated
/// at its own field below, along with what each field COUNTS and EXCLUDES.
struct IpqpCounters {
    /// IPQP iterations taken: one predictor+corrector pair each (spec
    /// section 3.1's Mehrotra predictor-corrector: one factorization, an
    /// affine solve, a corrector solve). Excludes a predictor attempt that
    /// fails before its corrector runs -- section 3.1 does not address the
    /// partial case, and the natural boundary is a completed pair only
    /// (T4 confirms).
    Index ipqp_iters = 0;

    /// Numeric factorizations paid. `>= ipqp_iters`, exceeding it by
    /// regularization-ladder rungs plus the section 2.2 required final
    /// unregularized inertia read. Excludes the tier-3 `refine_on_face`
    /// hand-off's own factorization, which lands in
    /// `SqpCounters::factorizations` like every other QP-engine refinement.
    Index ipqp_factorizations = 0;

    /// Symbolic analyses paid. `1` per SQP solve under the section 4.1
    /// cross-major hoisting rule (plan section 7 note a) while
    /// `AggregateEvalSeam::epoch()` stays unchanged. Excludes analyses paid
    /// by any other QP kernel (walk, SSN) in the same solve.
    Index ipqp_symbolic_analyses = 0;

    /// Backend triangular solves: EXACTLY 2 per completed iteration
    /// (predictor RHS, corrector RHS, both against the one numeric
    /// factorization that iteration paid for). Regularization-ladder rungs
    /// and the section 2.2 item 4 final inertia read add FACTORIZATIONS but
    /// NO solves -- they factorize to read an inertia and never
    /// back-substitute -- so `ipqp_solves == 2 * ipqp_iters` on a solve that
    /// completed every iteration it started, and the gap between this field
    /// and `2 * ipqp_factorizations` is exactly the ladder-plus-certification
    /// cost. (M6 W1 task 4 fix round 1: the earlier wording claimed those
    /// paths could add solves; they cannot.) Excludes triangular solves paid
    /// by any other QP kernel (walk, SSN) or by the tier-3 `refine_on_face`
    /// hand-off, which pays its own solve outside this struct.
    Index ipqp_solves = 0;

    /// Backend pattern-verify calls (mirrors
    /// `SymmetricFactor::Counters::pattern_verify_count`). Proves the plan
    /// section 7 note a discipline.
    ///
    /// THE CONTRACT IS "EXACTLY ONE OF {1 VERIFY, 1 ANALYZE} PER TIER ENTRY",
    /// not "exactly 1 verify" (M6 W1 task 4 fix round 1, I3 -- the code was
    /// right and this text was wrong). An entry that RE-ENTERS on an already
    /// laid pattern pays the one-time O(nnz) verify here and 0 analyses; the
    /// entry that LAYS the pattern pays 1 analysis and 0 verifies, because
    /// `KktFactorization::compute()` factorizes without verifying -- which is
    /// note (a)'s own premise, so the literal "+1 verify every entry" form is
    /// unsatisfiable. `IpqpOptions::ipqp_hoist_symbolic == false` forces every
    /// entry into the second state, which is how the OFF reading is pinned.
    /// Either way every LATER factorization in the entry, ladder rungs and the
    /// final inertia read included, runs `kAssumeAnalyzed` and moves neither
    /// count. Excludes pattern-verify calls paid by any other QP kernel in the
    /// same solve.
    Index ipqp_pattern_verifies = 0;

    /// The inertia-demanded modification `rho_dem`'s HIGH-WATER MARK across
    /// every ladder rung this subproblem paid. MAX-FOLDED across subproblems
    /// in `accumulate_ipqp_counters` (model: `ssn_sign_sweep_max`), so the
    /// `SqpCounters`-scale reading is the largest modification ANY subproblem
    /// in the solve was ever forced to. Excludes `delta`, which carries no
    /// separate high-water field, and excludes the section 3.2 schedule
    /// `rho_sched`, which is not a modification at all.
    ///
    /// A HIGH-WATER MARK IS NOT A LEVEL THE SOLVE ENDED AT (T4b). Before T4b
    /// the ladder was monotone per solve, so this equalled the working shift
    /// at every later iteration and `ipqp_rho_demanded_last` equalled it too.
    /// Algorithm IC's memory retries `rho_dem_last / 3` at every iteration, so
    /// under T4b a solve routinely settles well BELOW its own peak and the two
    /// fields differ. Still structurally `0.0` on a convex subproblem, where
    /// the ladder never arms -- which is the convex-inertness pin.
    double ipqp_rho_demanded_max = 0.0;

    /// The inertia-demanded modification at the LAST ladder rung this
    /// subproblem paid, which is Algorithm IC's own memory `rho_dem_last`: the
    /// shift the last SUCCESSFUL MODIFIED factorization ran at (or, on an
    /// exhausted ladder, the ceiling rung the tier was refused at). An
    /// iteration that succeeded on the UNMODIFIED system paid no rung and
    /// leaves this untouched, exactly as IC leaves its memory untouched
    /// there -- spec section 7's table row
    /// (`docs/notes/2026-08-m6-w1-ipqp-spec.md:713`) calls this reading
    /// "final" in so many words, which is what makes OVERWRITE (not summed
    /// or folded) the normative fold here, not merely this struct's own
    /// convention: the same `SqpCounters::start_level_used` convention for a
    /// per-solve categorical status rather than an additive count. Summing
    /// values drawn from many subproblems' own "last rho" would report a
    /// quantity with no meaning. Excludes `delta`'s own last value, which
    /// carries no separate field (only `rho`'s high-water and final
    /// readings are tracked, per the same spec row).
    double ipqp_rho_demanded_last = 0.0;

    /// Factorizations REJECTED on wrong OR EVIDENCE-INVALID inertia (the
    /// section 2.2 gate). M6 W1 task 4 fix round 1 (M2/I8) settled both
    /// halves of this against the code, which counts them alike because the
    /// gate refuses them alike: a WRONG reading (observed, disagreed with the
    /// required signature) and a PERTURBED one (observed, but describing a
    /// matrix the backend perturbed rather than the one assembled -- section
    /// 2.2's evidence-failure policy, so not evidence about this system at
    /// all) both cost a factorization the tier could not use and both are
    /// answered by a ladder rung. THE LADDER'S LAST, CEILING-TERMINATED
    /// factorization counts too: it was rejected on exactly the same grounds
    /// as every rung before it, and returning without counting it lost one
    /// rejection per exhausted ladder. Excludes the section 2.2 item 4
    /// REQUIRED final read -- that read's own outcome is
    /// `ipqp_final_inertia_read`, not this field, even when the final read
    /// itself comes back wrong.
    Index ipqp_inertia_retries = 0;

    /// Iterations whose step was TAKEN with a nonzero section 2.2
    /// INERTIA-DEMANDED MODIFICATION in force -- `rho_dem > 0`, i.e. the
    /// Newton matrix carried a shift the section 3.2 schedule did not ask for.
    /// Measured AFTER that iteration's ladder settles, so the iteration in
    /// which the ladder first demands a modification is counted (M6 W1 task 4
    /// fix round 1, I4: sampling before the ladder ran made this off by one,
    /// low, on every solve that ever armed the ladder). Excludes iterations
    /// whose step ran on the unmodified system, which is every iteration of
    /// every convex subproblem. (Before T4b the predicate was
    /// `max(rho_sched, rho_floor) > rho_sched`; with the two quantities
    /// separated and ADDITIVE it is simply `rho_dem > 0`, which is the same
    /// statement without the max.)
    Index ipqp_iters_at_elevated_rho = 0;

    /// LADDER RECLIMBS: iterations on which a `rho_dem_last / kIpqpLadderDown`
    /// value was REJECTED on a wrong inertia and the ladder had to re-escalate
    /// past it. ONE PER ITERATION, never per rung -- an iteration that climbed
    /// three rungs off a rejected `/3` value is one reclimb, because the
    /// quantity being measured is "how often did the memory's guess come back
    /// too small", not how far the climb then went.
    ///
    /// **THE `/3` VALUE, WHEREVER IT IS TRIED** (M6 W1 T4b fix round 1, I2 /
    /// CX4). Algorithm IC reaches `rho_dem_last / 3` by two routes: as this
    /// iteration's FIRST trial, once `kIpqpLadderSkipAfter` consecutive
    /// iterations have needed a modification; and as the rung that ANSWERS a
    /// refused zero-trial before then, which is the common regime on a short
    /// solve. The first draft of this counter charged only the first route, so
    /// the three-iteration saddle family reported ZERO reclimbs while paying
    /// 3.33 factorizations per iteration -- the exact cost the field exists to
    /// make visible. Both routes are charged.
    ///
    /// THE COST SIGNAL, AND IT IS AN EXPECTED COST, NOT A FAULT (M6 W1 T4b).
    /// Algorithm IC deliberately re-tries a SMALLER shift than the one that
    /// last worked, so on a subproblem whose curvature sits near the ladder's
    /// threshold the accepted values cycle inside `(theta, 8 theta]` with a
    /// reclimb every second iteration or so -- roughly half an extra
    /// factorization per iteration. That is Ipopt's own behaviour, accepted by
    /// Ipopt, and the alternative (a monotone floor) is the defect T4b
    /// removed. Read it against `ipqp_reg_increases` and
    /// `ipqp_reg_decreases`: every scheduled decrease of `rho_sched` by `d` on
    /// a boundary-curvature row demands `rho_dem += d`, so reclimbs correlate
    /// with decreases on a nonconvex row by construction.
    ///
    /// THIS FIELD REPLACES `ipqp_rho_flaps`, WHICH IS GONE WITH THE RULE IT
    /// MEASURED. That field counted section 3.2 gated decreases refused by
    /// section 2.2 item 3's MONOTONE-PER-SOLVE FLOOR; the T4b plan of record
    /// (plan section 7 note (p)) deletes that floor -- Wachter-Biegler's
    /// Algorithm IC, which section 2.2 cites by name, has no such rule -- so
    /// the event has no referent any more. The rename is free because the
    /// field is M6-new and reaches no CSV schema before task 9.
    ///
    /// STRUCTURALLY 0 ON A CONVEX SUBPROBLEM, where the ladder never arms and
    /// there is no `/3` value to reject. Excludes the FIRST climb of a solve
    /// (there is no memory to have guessed with), an iteration whose `/3` value
    /// was accepted, and every inertia-demanded increase as such
    /// (`ipqp_reg_increases`).
    ///
    /// **EXCLUDES A PERTURBED-DRIVEN ESCALATION** (M6 W1 T4b fix round 1, I1).
    /// Since T4b, a perturbed-pivot report received while the primal ladder is
    /// ARMED escalates `rho_dem` rather than `delta` -- but that escalation is
    /// not a statement about curvature at all, so charging it here would let a
    /// backend's pivot perturbation inflate a signal T9 reads as the cost of
    /// the `/3` band. It is visible instead through
    /// `ipqp_pivot_reroute_primal`. The first draft of this field's doc said
    /// perturbed readings escalate `delta` and are therefore excluded; that
    /// stopped being true when the re-route landed, and the exclusion is now
    /// enforced at the charge itself (fix round 2, F2).
    Index ipqp_ladder_reclimbs = 0;

    /// Ladder rungs taken because a PERTURBED-PIVOT report arrived while the
    /// primal ladder was ARMED, i.e. answered by escalating `rho_dem` rather
    /// than `delta` (M6 W1 T4b fix round 1, settler ruling R2). Counts RUNGS,
    /// not iterations: the question it answers for T9 is how much
    /// factorization budget the re-route spends.
    ///
    /// WHY THE RE-ROUTE EXISTS. The section 2.2 modification is a UNIFORM shift
    /// of the Ruiz-scaled system, and Ruiz normalizes a dominant diagonal to
    /// almost exactly `-1` while `rho_dem = 1` is a rung of Algorithm IC's own
    /// first climb -- so a rung can ANNIHILATE a scaled pivot. That singularity
    /// is PRIMAL; measured on `H = diag(2, -1000)`, the pre-T4b rule ("a
    /// perturbed pivot means a larger DUAL shift") climbed `delta` from 8 to
    /// `1e6` for four wasted factorizations and escaped `kNumerical` with ZERO
    /// iterations taken.
    ///
    /// Structurally 0 on any solve whose ladder never arms, and 0 on any
    /// backend that never reports a perturbed pivot. Excludes a perturbed
    /// report received at `rho_dem == 0`, which takes the ordinary dual route
    /// and is not counted anywhere; excludes the rungs the FALLBACK then takes
    /// (`ipqp_pivot_reroute_dual_fallback`); and excludes every wrong-inertia
    /// rung, which is `ipqp_inertia_retries`' business.
    Index ipqp_pivot_reroute_primal = 0;

    /// Times the bounded primal re-route above GAVE UP and fell back to
    /// escalating `delta` -- `kIpqpPivotReroutePrimalMax` consecutive primal
    /// escalations failed to clear the perturbation report (M6 W1 T4b fix
    /// round 1, settler ruling R2). Counts FALLBACK EVENTS, one per exhausted
    /// run of primal attempts, not the dual rungs that follow.
    ///
    /// THE BOUND IS AN APPROXIMATION OF PIVOT PROVENANCE, AND THIS FIELD IS
    /// HOW ITS ACCURACY IS MEASURED. A perturbed pivot whose cause is DUAL
    /// (near-dependent equality or inequality rows) is not cleared by any
    /// primal rung, and an unbounded re-route would ride the ladder to
    /// `ipqp_reg_max` to reach an answer the dual escalation gives in one.
    /// `hven::linear::InertiaEvidence` carries pivot COUNTS and no pivot
    /// LOCATIONS, so two consecutive failed primal escalations stands in for
    /// asking which block was perturbed. Read against
    /// `ipqp_pivot_reroute_primal`: a high ratio of fallbacks to primal
    /// re-routes says the heuristic is guessing wrong often enough to be worth
    /// replacing with real provenance.
    ///
    /// Structurally 0 wherever `ipqp_pivot_reroute_primal` is 0.
    Index ipqp_pivot_reroute_dual_fallback = 0;

    /// Iterations that ARMED the ladder (`rho_dem > 0` at the settled reading,
    /// step taken) and on which the section 3.2 gate did NOT advance.
    ///
    /// THE DIRECT INSTRUMENT FOR THE NEGATIVE-CURVATURE WALK (T4b plan 2.1's
    /// declared gap). Once the two regularizations are separated, the inner
    /// iteration is a descent method on the schedule's barrier-proximal
    /// subproblem whose RESIDUAL IS NOT MONOTONE: along a direction where
    /// `H + rho_sched I + Sigma` still has negative curvature the residual
    /// GROWS, and it keeps growing until a bound approaches or an equality row
    /// removes the direction. The gate measures residual CONTRACTION, so it is
    /// silent for the whole walk -- `zeta` and `rho_sched` pinned -- and then
    /// advances in one step when the curvature turns and `rho_dem` falls to 0.
    /// That is not a freeze (`x` moves, `mu` falls, steps are full), but it is
    /// a walk whose length nothing else in the counter table would show. Task
    /// 9 reads this field.
    ///
    /// Structurally 0 on a convex subproblem, where the ladder never arms.
    /// Excludes an armed iteration on which the gate DID advance, an unarmed
    /// iteration whether or not the gate advanced, and any iteration whose
    /// step was not taken (the same "iterations taken" exclusion
    /// `ipqp_iters_at_elevated_rho` documents).
    Index ipqp_iters_ladder_armed_no_advance = 0;

    /// Outcome of the section 2.2 item 4 REQUIRED final unregularized
    /// inertia read on a certifying exit -- spec section 7's table row
    /// (`docs/notes/2026-08-m6-w1-ipqp-spec.md:717`) sits two lines below
    /// `ipqp_rho_demanded_last`'s own "final" row (`:713`), and the same
    /// per-solve categorical reading is normative here: OVERWRITTEN by
    /// `accumulate_ipqp_counters`, the `SqpCounters::start_level_used`
    /// convention, not an additive quantity.
    ///
    /// SETTLER RULING (fix round 2, Codex co-review I2; extended by M6 W1
    /// task 4 fix round 1, I5): the values map onto the escape census
    /// exactly, closing the indefinite/numerical boundary this field and its
    /// two escape counters used to leave open. `0` -- the reading was taken
    /// and AGREED with the required signature: the certificate stands, no
    /// escape. `1` -- the reading was taken and DISAGREED (at the item 4
    /// final certification factorization, or when the ladder reached
    /// `ipqp_reg_max` with the reading still wrong): a saddle-
    /// suspect certificate downgrade, `ipqp_escape_indefinite`. `2` --
    /// UNREADABLE: the read was ATTEMPTED and no usable evidence came back --
    /// no observed state at all, or a PERTURBED-pivot report, which section
    /// 2.2's evidence-failure policy says is not evidence about the assembled
    /// matrix and therefore is not a disagreement either --
    /// `ipqp_escape_numerical`. `3` -- NOT PERFORMED: no factorization was
    /// ever taken for this read, on either of the two ways that happens --
    /// the read was DECLINED because
    /// `IpqpOptions::ipqp_require_final_inertia` is false, or it was REFUSED
    /// by `ipqp_max_factorizations` before it could run (M6 W1 task 4 fix
    /// round 2, N2: a cap-refused read used to record `2`, which the
    /// partition above defines as ATTEMPTED-and-unusable and therefore maps
    /// to the numerical class -- but a factorization the budget refused was
    /// never attempted). `3` IS ALWAYS A DOWNGRADE AND NEVER ITS OWN ESCAPE:
    /// the certificate does not stand either way, and the escape (if any)
    /// comes from whatever else stopped the solve -- `kNone` on the option-off
    /// path (spec section 9's own row, "off = certificate always downgraded":
    /// no census entry and no section 6.1 K = 3 retirement charge, because a
    /// caller choosing to skip one factorization has not hit a failure), and
    /// `kBudget` on the cap-refused path, where the budget is genuinely why
    /// the solve stopped. `3` is kept distinct from `2` so the census cannot
    /// confuse a read that never ran with one that ran and failed. A
    /// solve-wide
    /// "was any subproblem's read ever unreliable" question reads the
    /// escape census (`ipqp_escape_indefinite` + `ipqp_escape_numerical`)
    /// rather than this per-subproblem field. Structurally `0` (its
    /// "agreed" value) on a subproblem that never reached a certifying
    /// exit, since the read is paid only there. Excludes every inertia
    /// read paid mid-ladder (`ipqp_inertia_retries`), which this field
    /// never reports.
    ///
    /// `0`'S ONE CAVEAT (M6 W1 task 5): `0` says THIS READ AGREED, which is
    /// not by itself the statement that the certificate stands. Section
    /// 2.2's evidence-failure policy downgrades a certificate FOR THE WHOLE
    /// SOLVE when any earlier factorization succeeded and could not report
    /// usable inertia evidence -- the tier then steps at a conservative
    /// `rho` floor and carries the downgrade to the end -- so `0` can pair
    /// with `IpqpResult::certificate_downgraded == true` and
    /// `escape_reason == kNone`. The certificate is read off
    /// `certificate_downgraded`, never off this field alone.
    Index ipqp_final_inertia_read = 0;

    /// `(rho, delta)` schedule GATED decreases actually applied: +1 per
    /// gated advance that moved EITHER `rho` or `delta` (M6 W1 task 4 fix
    /// round 1, I7 -- the field is the SCHEDULE'S, and an advance that moved
    /// `delta` alone is an applied decrease of the schedule; reading it as
    /// `rho`-only reported "no decrease applied" on a monotone-floor fixture
    /// while `delta` went 8 -> 0.8). Never more than 1 per advance, so it is
    /// bounded by `ipqp_prox_center_updates`.
    ///
    /// THE IDENTITY, RE-PINNED AT T4b: `ipqp_prox_center_updates ==
    /// ipqp_reg_decreases + (advances that moved nothing)`. The third class
    /// this field used to share the partition with -- a decrease REFUSED by
    /// section 2.2 item 3's monotone floor, counted as `ipqp_rho_flaps` -- no
    /// longer exists: T4b deletes that floor, so no gated advance can be
    /// refused by anything except the ABSOLUTE `ipqp_reg_floor`, which is the
    /// remaining "nothing moved" class. That class is a setting every schedule
    /// decays onto rather than evidence about this subproblem's curvature, so
    /// it earns no counter of its own; it is the only reason
    /// `ipqp_prox_center_updates` can exceed this field. The inertia ladder
    /// cannot contribute to either side of the identity now -- it moves
    /// `rho_dem`, which the schedule never sees.
    Index ipqp_reg_decreases = 0;

    /// `(rho, delta)` schedule inertia-demanded increases. Excludes the
    /// initial `rho_0`/`delta_0` assignment at subproblem start (section
    /// 3.2), which is not an increase.
    Index ipqp_reg_increases = 0;

    /// Proximal-estimate (`zeta`/`lambda_est`) advances. Excludes the
    /// initial `zeta_0 = x_0`, `lambda_est_0 = y_0` assignment (section
    /// 3.2), which is not an advance.
    Index ipqp_prox_center_updates = 0;

    /// Warm restarts (section 5.2) whose repair moved at least one
    /// component of the ingested seed (a strict-positivity clamp or the
    /// two-scalar shift). Excludes a warm restart whose seed needed no
    /// repair at all.
    Index ipqp_restart_repairs = 0;

    /// The LARGEST repair shift (section 5.2's `(delta_p, delta_d)`) applied
    /// to any component of any warm restart's seed; `0.0` when no restart
    /// was ever repaired. MAX-FOLDED across subproblems in
    /// `accumulate_ipqp_counters` (model: `ssn_sign_sweep_max`) -- the
    /// honest-magnitude field, not a sum, exactly like that field. Excludes
    /// a cold-started subproblem, which pays no repair and never touches
    /// this field.
    double ipqp_restart_shift_max = 0.0;

    /// `1` iff the payload `mu` raised `mu_0` off the measured floor
    /// (section 5.3's clamp: `mu_0 = clamp(max(mu_meas, kappa*mu_payload),
    /// min, init)`, and the payload term was the binding one), else `0`. A
    /// per-subproblem flag SUMMED like a count, exactly the convention
    /// `SqpCounters::n_seeded` uses for a per-solve flag. Excludes a
    /// cold-started subproblem, which has no payload `mu` to adopt and is
    /// structurally `0` here.
    ///
    /// A STATEMENT ABOUT THE CLAMP, NOT THE TRAJECTORY -- an adoption that
    /// binds can still leave the solve bit-identical.
    /// `.superpowers/w1-t7-report.md` FIX ROUND 2.
    Index ipqp_mu_adopted = 0;

    /// `1` iff the section 5.5 warm-kill fired on this subproblem (a warm
    /// restart overran its clamped budget and was restarted cold exactly
    /// once), else `0`. Excludes a cold-started subproblem (structurally
    /// `0`, no warm restart to abandon) and a warm restart that stayed
    /// inside its budget.
    Index ipqp_warm_restart_abandoned = 0;

    /// Subproblems the section 2.3/4b domain gate DECLINED pre-solve because
    /// the effective box (T4.b's `IpqpBounds`) contained a zero-width pair
    /// (plan section 7 note e). A DECLINE IS NOT AN ESCAPE: the tier never
    /// ran, so this never counts toward `ipqp_escapes` or the K=3
    /// retirement threshold, and the walk solves the declined subproblem
    /// exactly. Excludes every subproblem the tier actually entered,
    /// however it then concluded.
    Index ipqp_declined_pinned = 0;

    /// The major at which K = 3 consecutive escapes retired the tier for the
    /// remainder of this solve (spec section 6.1,
    /// `docs/notes/2026-08-m6-w1-ipqp-spec.md:615-617`); `0` if the tier was
    /// never retired.
    ///
    /// A MARKER, NOT A COUNT (fix round 1, Codex co-review I1): retirement
    /// fires AT MOST ONCE PER SOLVE -- section 6.1 is unambiguous ("after
    /// K = 3 consecutive escapes ... the tier is retired for the REMAINDER
    /// of that solve"), and "any success resets the count" resets only the
    /// consecutive-escape tally toward a future retirement, not an
    /// already-fired one; nothing in section 6.1 contradicts once-per-solve.
    /// FOLDED BY MAX in `accumulate_ipqp_counters`, the same discipline as
    /// the peak fields above (`ipqp_rho_demanded_max`,
    /// `ipqp_restart_shift_max`): order-independent, and `0` (never retired)
    /// is the fold identity. Summing two nonzero readings would report an
    /// impossible major -- e.g. majors 4 and 7 summing to the impossible
    /// "major 11" -- so max is the only fold that stays a real major index.
    ///
    /// DRIVER-SCALE ONLY, the `ssn_escape_gate_refused` convention: no
    /// per-subproblem read of this struct ever carries it nonzero (retiring
    /// the tier is bookkeeping ACROSS subproblems, which no single
    /// subproblem's own solve can observe) -- the driver writes the
    /// `SqpCounters::ipqp` field directly when retirement fires. Max-folded
    /// here anyway for the same reason `ssn_escape_gate_refused` is summed:
    /// harmless on an always-zero per-subproblem contribution, and correct
    /// on the one real call site (a restoration sub-solve's own totals
    /// folding onto an already-populated running total, mirroring
    /// `accumulate_ssn_counters`). Excludes every major before retirement
    /// fired, which never sets this field, and excludes a solve where the
    /// tier was declined-pinned throughout (`ipqp_declined_pinned`) without
    /// ever accumulating three consecutive genuine escapes.
    Index ipqp_tier_retired_after = 0;

    /// Rows/bounds the section 2.3 ratio rule left UNCERTAIN (neither
    /// classification test satisfied), rather than asserted either way.
    /// ACCUMULATED ON EVERY EXIT, including escapes: the classifier runs on
    /// the returned point whatever the outcome was, so this is the population
    /// the RULE left uncertain, and the population `refine_on_face` was
    /// actually handed is the subset that reached it (the routing's rows 2a-2c
    /// -- an escaped subproblem's face is never handed on). Excludes
    /// rows/bounds the ratio rule classified definitively active or inactive.
    Index ipqp_face_uncertain = 0;

    /// Tier-3 `refine_on_face` hand-offs ACCEPTED as the step. EVERY usable
    /// tier exit is handed on (section 2.3 item 3 -- tier 3 owns the last two
    /// decades, whether or not the ratio rule left anything uncertain), so
    /// this excludes only a subproblem that never produced a usable exit: a
    /// decline, a retired-tier major, or a genuine escape. A refusal is
    /// `ipqp_refine_refused` instead.
    Index ipqp_refine_accepted = 0;

    /// Tier-3 `refine_on_face` hand-offs REFUSED (empty/rank-deficient face,
    /// a failed inertia gate, or the refined point leaving the box/TR/
    /// inactive rows) -- the certificate the tier already had stands; see
    /// `SsnCounters::ssn_refine_refused` for the identical convention on the
    /// SSN tier. Excludes an acceptance, which is `ipqp_refine_accepted`
    /// instead, and a subproblem that never reached tier-3.
    Index ipqp_refine_refused = 0;

    /// Subproblems the routing chain handed to the tier-3 `refine_on_face`
    /// step (section 2.3 item 3) -- the FIRST destination of every usable tier
    /// exit, which is exactly `ipqp_refine_accepted + ipqp_refine_refused`.
    ///
    /// IT EXISTS TO CLOSE THE ROUTING PARTITION. Without it the group has no
    /// term for the refinement destination, so "every consulted subproblem
    /// went somewhere" cannot be stated as arithmetic -- and two rows falsify
    /// the two-term version: a converged `kBudget` exit whose section 2.2 item
    /// 4 read the factorization budget refused is counted in
    /// `ipqp_escape_budget` and routed HERE, not to the walk. Excludes a
    /// declined or retired-major subproblem (the tier produced no exit to
    /// refine) and a genuine escape (`ipqp_to_walk`, or `ipqp_to_ssn` for the
    /// saddle-suspect one).
    Index ipqp_to_refine = 0;

    /// Subproblems handed to the SSN warm-grade path -- section 2.3 item 4's
    /// TWO feeders, so this is exactly `ipqp_refine_refused +
    /// ipqp_escape_indefinite`: a `refine_on_face` refusal, and a
    /// saddle-suspect (`IpqpEscape::kIndefinite`) exit, which goes to SSN
    /// directly without a refinement attempt. Excludes a `refine_on_face`
    /// acceptance (`ipqp_refine_accepted`), which is the step and reaches no
    /// further routing step.
    Index ipqp_to_ssn = 0;

    /// Routing outcomes handed to the walk: a genuine tier escape, which goes
    /// COLD with the iterate discarded (section 2.3 item 5), or a
    /// declined-pinned subproblem (`ipqp_declined_pinned` above) re-routed
    /// pre-solve, which goes to the ORDINARY seeded walk because the tier
    /// never ran and there is no iterate to discard. Should be rare by design
    /// -- the routing chain's own note. Excludes a hand-off to SSN
    /// (`ipqp_to_ssn`) and to the refinement (`ipqp_to_refine`); excludes a
    /// SADDLE-SUSPECT escape, which is an escape that goes to SSN rather than
    /// here; and excludes the CONVERGED `kBudget` exit of `ipqp_to_refine`'s
    /// note, which is counted in the escape census and routed to the
    /// refinement. `ipqp_escapes - ipqp_to_walk` is therefore not a
    /// meaningful quantity on its own.
    ///
    /// THE CLOSED STATEMENT IS OVER FIRST DESTINATIONS, and it is NOT the raw
    /// three-term sum (corrected, fix round 3):
    ///
    ///     ipqp_to_refine + ipqp_escape_indefinite
    ///                    + (ipqp_to_walk - ipqp_declined_pinned)
    ///         == the subproblems the tier was CONSULTED on
    ///            (entered the engine: neither retired-past nor declined)
    ///
    /// `ipqp_to_ssn` cannot stand in that sum: a refinement REFUSAL reaches
    /// SSN as a SECOND destination and is already counted in
    /// `ipqp_to_refine`, so adding `ipqp_to_ssn` would count it twice.
    /// `ipqp_escape_indefinite` is the only route that reaches SSN FIRST.
    /// And `ipqp_declined_pinned` is subtracted because a decline is counted
    /// here -- the walk really is where it goes -- without the tier having
    /// run, so it is not one of the consulted subproblems the sum is over.
    /// `tests/sqp/support/ipqp_test_support.h`'s
    /// `assert_ipqp_routing_partition` asserts exactly this, together with
    /// `ipqp_to_refine == accepted + refused` and
    /// `ipqp_to_ssn == refused + escape_indefinite`.
    Index ipqp_to_walk = 0;

    /// Subproblems the tier ESCAPED (any of the five reasons below), summed
    /// across the whole solve. Excludes declined-pinned subproblems -- see
    /// `ipqp_declined_pinned` above, which the tier never entered.
    Index ipqp_escapes = 0;

    // THE FIVE-WAY ESCAPE CENSUS. The five MUST SUM TO `ipqp_escapes`
    // (`SsnCounters`:522-527's discipline, restated here for this tier): each
    // subproblem escape increments exactly one of the five below and
    // `ipqp_escapes` together. Each field's own doc comment states its
    // count and, per the block discipline above, what it excludes -- always
    // "the other four", stated once here rather than repeated five times.
    //
    // Plan section 7 note b (FINAL, r3): the spec v2 draft's three
    // stall-reason sub-counters under `ipqp_escape_stall` are DROPPED as
    // ill-posed -- all three conjuncts hold at every stall escape, so a
    // partition among them is degenerate. `ipqp_escape_stall` below is
    // escape-COUNT only; the three conjunct VALUES (mu ratio over the
    // window, relative residual improvement, min alpha) travel instead in
    // the stall escape's own evidence block (T5), never as counters here.

    /// Escapes via the section 6.1 hard iteration cap, `IpqpEscape::kBudget`
    /// -- of LAST RESORT (section 6.1: the stall test below should fire
    /// first on anything genuinely stuck). Excludes an escape whose stall
    /// test fired first, which is `ipqp_escape_stall` instead (section 6.1
    /// states budget is of last resort precisely so stall pre-empts it),
    /// and excludes the section 5.5 warm-kill budget -- a warm restart's
    /// own budget, not the tier's iteration cap, and tracked separately as
    /// `ipqp_warm_restart_abandoned`, which is not an escape at all.
    Index ipqp_escape_budget = 0;

    /// Escapes via the section 6.2 early-stall test (the `mu`/residual/
    /// min-alpha window, all three conjuncts required). Excludes the
    /// DROPPED `ipqp_stall_reason_mu/_residual/_alpha` sub-counters (note
    /// b) -- the three conjunct values travel in the stall escape's own
    /// evidence block instead, never as counters here.
    Index ipqp_escape_stall = 0;

    /// Escapes via `IpqpEscape::kIndefinite` (section 2.2 item 4): an
    /// inertia reading was READ (an evidence state was actually observed)
    /// and DISAGREED with the required signature -- at the item 4 final
    /// certification factorization on an otherwise-converged point, or
    /// when the monotone regularization ladder reached `ipqp_reg_max` with
    /// the reading still wrong. SETTLER RULING (fix round 2, Codex
    /// co-review I2): this is `ipqp_final_inertia_read == 1`, a
    /// saddle-suspect certificate downgrade that routes to SSN warm-grade
    /// per section 2.3 item 4. Excludes an inertia-gate failure mid-ladder
    /// before the final read (`ipqp_inertia_retries` instead, which
    /// retries rather than escaping), and excludes an UNREADABLE reading
    /// (no evidence state observed at all) -- that is
    /// `ipqp_escape_numerical` (`ipqp_final_inertia_read == 2`) instead,
    /// the field immediately below.
    Index ipqp_escape_indefinite = 0;

    /// Escapes that stop the tier for a non-convergence reason other than
    /// budget, stall, or infeasible-suspect (section 2.3 item 5's
    /// "numerical error" class): a factorization failure, an UNREADABLE
    /// inertia reading (`ipqp_final_inertia_read == 2` -- no evidence
    /// state was observed to compare against the required signature at
    /// all), or a non-finite iterate/residual/step. SETTLER RULING (fix
    /// round 2, Codex co-review I2): this is the complement of
    /// `ipqp_escape_indefinite` within "the reading was inertia-related" --
    /// indefinite means a reading WAS taken and disagreed, numerical means
    /// either no reading could be taken at all or the failure was not an
    /// inertia reading in the first place. Excludes an
    /// indefinite-certificate escape (`ipqp_escape_indefinite` above,
    /// `ipqp_final_inertia_read == 1`) -- a reading that WAS taken and
    /// disagreed is never double-counted here.
    Index ipqp_escape_numerical = 0;

    /// Escapes via `IpqpEscape::kInfeasibleSuspect` (section 6.3's
    /// two-conjunct test: primal residual flat on a positive floor AND
    /// `||(y, z)||` growth over the window), carried with its own evidence
    /// block (`IpqpResult::infeasibility_evidence`). Excludes a
    /// certificate: the tier never returns `QpStatus::kInfeasible` (section
    /// 6.3) -- only this escape signature.
    Index ipqp_escape_infeasible_suspect = 0;

    /// The smallest PRIMAL fraction-to-boundary step taken, across every
    /// iteration of every subproblem -- the acquisition-health signal.
    /// MIN-FOLDED across subproblems in `accumulate_ipqp_counters` (the
    /// minimum counterpart of `ssn_sign_sweep_max`'s max-fold). Defaults to
    /// `+infinity`, NOT `0.0`: valid fraction-to-boundary steps lie in
    /// `(0, 1]`, so a `0.0` default would be indistinguishable from, and
    /// would defeat, an observed step and would make `std::min` never move
    /// off it. `+infinity` reads as "no step observed yet" and folds
    /// correctly with `std::min`. Excludes a declined-pinned subproblem
    /// (`ipqp_declined_pinned`), which the tier never enters and so never
    /// takes a fraction-to-boundary step.
    double ipqp_alpha_p_min = std::numeric_limits<double>::infinity();

    /// The DUAL-side counterpart of `ipqp_alpha_p_min`: same fold and same
    /// `+infinity` default, for the same reason. Excludes a declined-pinned
    /// subproblem (`ipqp_declined_pinned`), which the tier never enters and
    /// so never takes a fraction-to-boundary step -- the same exclusion
    /// `ipqp_alpha_p_min` states for itself.
    double ipqp_alpha_d_min = std::numeric_limits<double>::infinity();

    /// T4c: KEPT bound sides in the item 4 read's disclosure band. Additive
    /// fold. U0 corpus 0/0 is EXPECTED (near-equality-constrained first
    /// QPs), meaningful only on activity-taxonomy/path-bound cells. See
    /// `.superpowers/w1-t4c-report.md`.
    Index ipqp_read_kept_tight_sides = 0;

    /// T4c: band-counted sides `ipqp_classify_barrier_noise` (ipqp_math.h)
    /// classifies `kSuspect`; `0` on the band-only fallback. Additive fold;
    /// same U0 corpus caveat as `ipqp_read_kept_tight_sides` above. See
    /// `.superpowers/w1-t4c-report.md`.
    Index ipqp_read_barrier_noise_sides = 0;
};

/// Aggregate work counters for a whole solve.
///
/// major_iters counts SUBPROBLEMS SOLVED -- not iterates evaluated, and not
/// steps ACCEPTED: a subproblem that fails is counted, because the work was
/// spent. A solve that converges immediately reports major_iters == 0 and a
/// one-entry history. A SOC RE-SOLVE IS NOT ONE OF THESE SUBPROBLEMS: it is
/// tied to the TRIAL that triggered it (one per rejected trial, at most), not
/// counted as its own major, and its cost is folded into
/// qp_minor_iters/factorizations instead (see soc_steps below and
/// sqp_driver.h's SECOND-ORDER CORRECTION note).
///
/// THE HISTORY LENGTH THEREFORE DEPENDS ON WHERE THE SOLVE STOPPED, and a
/// consumer indexing into it must branch on that (see SqpIterate):
/// - stopped AT an iterate (kOptimal, kMaxIter, and the non-finite-iterate
///   kNumericalError): history.size() == major_iters + 1, and the last row
///   has qp_solved == false;
/// - stopped ON a subproblem (kNumericalError propagated from the QP, or any
///   of the THREE kRestore routes of sqp_driver.h -- the funnel's signature,
///   the elastic tier's exhaustion and the radius floor):
///   history.size() == major_iters, every row has qp_solved == true, and the
///   last row carries the FAILING qp_status -- except on the kRestore
///   routes, where the last solve itself succeeded (the funnel's and the
///   floor's routes) or the ELASTIC re-solve did (the elastic-tier route)
///   and it is `verdict` that carries the reason.
/// So `history[counters.major_iters]` is in bounds on the first family and
/// OUT OF BOUNDS on the second. Use history.back() and read qp_solved, never
/// index arithmetic on major_iters.
///
/// ON A TERMINAL RESTORATION EXIT, SqpSolution::x IS NOT THE POINT THE LAST
/// HISTORY ROW DESCRIBES, and a consumer that plots one against the other
/// must know it. The last row is the iterate that RAISED the request -- its
/// f, h and residuals are that point's -- while x/f/lambda_* are the RESTORED
/// point the phase ended at, which has no row of its own because the phase
/// produces none. Measured on the certified circle/line exit: the last row
/// reports f = 2 at (1,1), and the solution reports f = 1.414 at
/// (0.707, 0.707). (The two coincide only when the phase never moved the
/// iterate -- e.g. the once-per-solve cap, which returns without running.)
///
/// qp_minor_iters and factorizations are plain SUMS of the corresponding
/// QpCounters fields over every subproblem solved, so they carry that
/// header's semantics unchanged (qp_engine.h's COUNTER SEMANTICS note:
/// minor_iters is one per QP major iteration; factorizations is not bounded
/// by one per QP iteration in border mode). schur_updates is deliberately
/// NOT aggregated -- the per-major values are in the history if a caller
/// wants them.
///
/// FROM SOC ON THESE TWO SUMS CAN EXCEED sum(history[k].qp_* OVER k). A
/// second-order correction (see sqp_driver.h's SECOND-ORDER CORRECTION note)
/// re-solves the SAME iterate's subproblem with a shifted rhs, and that
/// re-solve's minor_iters/factorizations are folded in HERE -- work was spent
/// -- but deliberately NOT into the triggering row's qp_minor_iters/
/// qp_factorizations, which stay exactly what the ORIGINAL (rejected) QP
/// solve cost, preserving every existing reader's assumption that a row's
/// qp_* fields describe ONE QpSolution. The SOC re-solve's own cost is
/// therefore recoverable as the difference: counters.qp_minor_iters -
/// sum(history[k].qp_minor_iters) (likewise factorizations), which is
/// exactly how the SOC hot-start tests measure it.
///
/// rejected_steps counts the majors spent SHRINKING THE RADIUS at an iterate
/// that did not move. Two things produce one, and they are counted together
/// because the loop treats them identically: a trial point the globalization
/// strategy REJECTED (StepVerdict::kReject), and a SUBPROBLEM THAT FAILED but
/// returned a usable iterate (sqp_driver.h's SUBPROBLEM FAILURE ROUTING),
/// where there was no trial point to judge at all. Tell them apart by the
/// row's qp_status: kOptimal on the first, kNumericalError/kMaxIter on the
/// second. steps_accepted counts the complementary case: trials the strategy
/// ACCEPTED (kAcceptF or kAcceptH), which is exactly the number of times the
/// iterate moved on the CALLER'S OWN problem, and -- WITH ONE EXCEPTION --
/// the number of eval_hess calls the OPTIMALITY phase makes (the Hessian is
/// evaluated once per iterate that builds a subproblem, never per trial --
/// see sqp_driver.h).
///
/// THE EXCEPTION: a solve that exits WITHOUT EVER BUILDING A SUBPROBLEM --
/// converged at its own start point, or a zero budget, both of which report
/// steps_accepted == 0 AND major_iters == 0 -- pays ONE eval_hess anyway, in
/// sqp_driver.h's make_warm_start, to hash the model's sparsity for the
/// hand-off it emits (that function's THE ZERO-MAJOR PROBE note). So the
/// exact optimality-phase count is steps_accepted + [major_iters == 0], and
/// the extra call is bounded by one per solve, on precisely the solves that
/// would otherwise have evaluated no Hessian at all. A solve whose start
/// point the model could not evaluate pays nothing extra (it is not probed).
///
/// IT IS ALSO NOT THE NUMBER OF eval_hess CALLS THE SOLVE MAKES ON THE
/// MODEL, and the difference is the restoration phase: RestorationModel's own
/// eval_hess FORWARDS to the wrapped model's (with obj_scale = 0), so every
/// restoration major costs one eval_hess on the caller's model too. The exact
/// count is steps_accepted + (the accepted steps of the restoration
/// sub-solve), and only the first term is reported -- the second is inside
/// restoration_iters, which counts that sub-solve's majors rather than its
/// acceptances. A consumer budgeting Hessian evaluations should treat
/// steps_accepted + restoration_iters + 1 as the upper bound -- the trailing
/// +1 is the zero-major probe above, which cannot fire on the same solve as
/// any accepted step but is included so the bound holds unconditionally.
///
/// BOTH ACCEPT/REJECT COUNTERS ARE COUNTED EXPLICITLY BECAUSE THE OBVIOUS
/// IDENTITY IS FALSE. steps_accepted == major_iters - rejected_steps holds
/// only on a solve that stopped AT an iterate; a solve that stopped ON a
/// subproblem spent a major that was neither accepted nor rejected -- the QP
/// failed (kInfeasible, or a kNumericalError/kMaxIter that had already used
/// its one retry), so there was no trial point to judge -- and EVERY kRestore
/// row spends one that was judged but is neither. In general
///     major_iters = steps_accepted + rejected_steps + (kRestore rows),
/// where the last term counts BOTH the terminal request (0 or 1) AND every
/// request the phase RESUMED from, since a resumed restoration leaves its
/// requesting row behind and the solve carries on (measured gap of 2 on the
/// runaway-valley fixture: one resumed request plus one capped one). A
/// consumer wanting "how many Hessians did this cost" must read
/// steps_accepted (plus the paragraphs above), not the subtraction.
///
/// soc_steps counts SECOND-ORDER CORRECTION ATTEMPTS: once per qualifying
/// kReject trial (h_new > h_old, opts.enable_soc), regardless of whether the
/// attempt succeeded -- a failed SOC re-solve, or one whose corrected point
/// the funnel also rejected, still counts, because the work was spent. See
/// sqp_driver.h's SECOND-ORDER CORRECTION note. It is 0 in every solve with
/// enable_soc == false.
///
/// soc_applied/soc_qp_infeasible/soc_rejected BREAK soc_steps' one number
/// DOWN BY OUTCOME -- soc_steps counts attempts only, with no record of which
/// way an attempt ended, and battery adjudication needs that record. They are
/// EXACTLY the three mutually-exclusive outcomes sqp_driver.h's
/// SECOND-ORDER CORRECTION path can reach, so the identity
///     soc_steps == soc_applied + soc_qp_infeasible + soc_rejected
/// holds on every solve:
/// - soc_applied: the re-solve returned kOptimal AND the strategy accepted
///   the corrected point (kAcceptF/kAcceptH) -- the attempt paid off. Same
///   event SqpIterate::soc_applied flags per-row; this is the solve-wide sum.
/// - soc_qp_infeasible: the re-solve itself did not return kOptimal (in
///   practice almost always kInfeasible -- the rhs shift scales with the very
///   violation that triggered SOC, so a large h_new is what "most often"
///   means in the SECOND-ORDER CORRECTION note's A FAILED SOC RE-SOLVE table)
///   -- there was no corrected point to judge at all.
/// - soc_rejected: the re-solve returned kOptimal but the strategy did NOT
///   accept the corrected point (a kReject, or a kRestore that was not
///   promoted) -- a corrected point existed and was still turned down.
/// All three, like soc_steps itself, are folded in from a restoration
/// sub-solve exactly as soc_steps is (the work was spent on that sub-solve's
/// own SOC attempts too), so the identity holds on the aggregate counters of
/// a solve that restored, not only on one that did not.
///
/// elastic_activations counts SUBPROBLEMS REFORMULATED ELASTICALLY: once per
/// trial whose QP returned kInfeasible, whatever the reformulation then
/// produced (a step, or the elastic tier's own exhaustion signal). It is
/// therefore also an exact count of the kInfeasible subproblems this solve
/// met, since EVERY one of them is reformulated -- see sqp_driver.h's ELASTIC
/// TIER note.
///
/// SINCE M6 W2 THAT IS THE WALK ROUTE ONLY: the certified fallback reformulates on the IPQP
/// tier's infeasibility EVIDENCE instead, with no kInfeasible QP in front of it, and charges
/// activations of its own -- `elastic_from_ipqp_escape` counts every one of them, so that
/// counter and the walk-route remainder split this total (the identity is stated on it).
/// `ipqp_suspicion_disproved`, `ipqp_fallback_rung_b` and `elastic_floor_retries` classify the
/// fallback's ENTRIES rather than its activations; `elastic_rho0_ceiling_hits` counts LADDERS.
///
/// elastic_escalations counts rho ESCALATIONS (x10 re-solves of the SAME
/// elastic subproblem), summed over every activation -- NOT the number of
/// elastic solves, which is elastic_activations + elastic_escalations. It is
/// bounded by 6 per activation -- kElasticRhoInit = 1e2 to kElasticRhoMax = 1e8, at most that
/// when infeasibility evidence places the first rung higher (capped by THE PLACEMENT BOUND,
/// sqp_driver.h: the escalation headroom 1e7 and the dual_mu safety margin, so an evidence
/// placement always leaves at least one rung above it) -- at
/// SqpOptions::elastic_ladder_early_exit's default (false, i.e. the
/// ladder always spends every rung). With the early exit opted in, that bound
/// is an UPPER bound only: an activation whose ladder stalls (a rung's
/// solution repeats the previous one) reads FEWER than 6 -- see that option's
/// own note in this file for why it defaults off (a real trajectory-shaping
/// effect was measured, not merely a cost cut). Their qp_minor_iters/
/// factorizations are folded into the aggregate sums above, exactly as a SOC
/// re-solve's are, and for the same reason.
///
/// restoration_iters counts MAJOR ITERATIONS SPENT INSIDE THE RESTORATION
/// PHASE -- i.e. subproblems solved on the FEASIBILITY problem, not on the
/// NLP's own linearization. It is exactly the sub-solve's own major_iters,
/// summed over every restoration entered (at most one per solve today; see
/// sqp_driver.h's RESTORATION PHASE note for the cap and why it exists).
/// Zero on every solve that never raised a restoration request, which is
/// every clean solve.
///
/// THESE MAJORS ARE NOT IN major_iters AND HAVE NO HISTORY ROWS. The two
/// counters partition the QP solves a driver spends on models: major_iters
/// counts the ones built from the NLP at an outer iterate, restoration_iters
/// the ones built from the feasibility wrapper. They share the max_iter
/// budget (see SqpOptions). The restoration sub-solve's own qp_minor_iters
/// and factorizations ARE folded into this struct's aggregates, exactly as a
/// SOC re-solve's and an elastic rung's are and for the same reason -- the
/// work was spent -- so those two sums cover the whole solve including
/// restoration, while soc_steps/elastic_* likewise absorb whatever the
/// sub-solve did.
///
/// A NONZERO VALUE DOES NOT IMPLY A FAILED SOLVE. Restoration is a recovery
/// mechanism first: a solve that restores and then converges reports kOptimal
/// with restoration_iters > 0, and that is the outcome the phase exists to
/// produce. It is the pairing with SqpStatus::kInfeasible that says the
/// restoration phase reached its OTHER conclusion.
struct SqpCounters {
    Index major_iters = 0;
    Index qp_minor_iters = 0;
    Index factorizations = 0;
    Index steps_accepted = 0;
    Index rejected_steps = 0;
    Index soc_steps = 0;
    Index soc_applied = 0;
    Index soc_qp_infeasible = 0;
    Index soc_rejected = 0;
    Index elastic_activations = 0;
    Index elastic_escalations = 0;
    Index restoration_iters = 0;

    // THE CERTIFIED FALLBACK'S PARTITION (M6 W2 T5, plan amendment G). The counters below are
    // written by `certified_feasibility_fallback` -- the ONE judge, and the only
    // site that writes any of them. Their block discipline is the five-way escape census's
    // (:1062-1072), restated for this partition over ENTRIES:
    //
    //     ipqp_suspicion_disproved + <relaxed> + <exhausted> + ipqp_fallback_rung_b
    //         == the fallback entries whose evidence block FIRED
    //         == elastic_from_ipqp_escape - elastic_floor_retries
    //
    // where <relaxed> and <exhausted> are the rung-A-owned outcomes that carry no counter of
    // their own -- they are read off the returned `qp_status` (kOptimal vs the synthesized
    // kInfeasible), which is why plan section 5 states the partition "with qp_status alongside".
    //
    // AN ENTRY WHOSE BLOCK NEVER FIRED CHARGES NONE OF THEM: it is W1's single cold walk, with
    // no rung A entered and no activation charged, so it is outside the partition by
    // construction rather than by arithmetic.

    /// Elastic ACTIVATIONS THIS SOLVE OWES TO THE CERTIFIED FALLBACK rather than to a walk
    /// `kInfeasible`: every ladder the fallback ran -- an entry's first attempt AND its floor
    /// retry -- whatever the engine then did with it. Both routes into the elastic tier
    /// increment `elastic_activations`, so without this counter W2's whole effect on the
    /// currency is invisible, and with it the total splits in TWO terms:
    ///
    ///     elastic_activations == <walk-route activations> + elastic_from_ipqp_escape
    ///
    /// so the walk-route count is that difference -- the reading this counter exists to enable.
    ///
    /// EXCLUDES every activation the driver's own elastic branch raised on a walk `kInfeasible`,
    /// and an entry whose block never FIRED (no rung A was entered for it at all). It counts
    /// ACTIVATIONS, not entries: the ENTRY partition above is this counter MINUS
    /// `elastic_floor_retries`, and a rung-B entry is charged here as well as there.
    Index elastic_from_ipqp_escape = 0;

    /// Fallback entries whose rung A came back with CLOSED slacks -- the suspicion was FALSE and
    /// the elastic answer IS the unrelaxed subproblem's (the l1 exact-penalty property). The one
    /// number that says whether spec section 6.3's two-conjunct detector is calibrated:
    /// `ipqp.ipqp_escape_infeasible_suspect` counts the SUSPICION, this counts its FATE.
    ///
    /// EXCLUDES the other three arms of the partition above -- in particular a rung B that
    /// disproves the suspicion on its own (its walk solves the original QP and returns
    /// kOptimal), which this counter cannot see and `ipqp_fallback_rung_b` counts instead.
    Index ipqp_suspicion_disproved = 0;

    /// Fallback entries that fell through to RUNG B, the cold walk: rung A was DECLINED by the
    /// engine (a non-kOptimal ladder exit), and -- when the placement was above the floor -- so
    /// was its one retry there. The refusal path's own frequency, and the fourth arm of the
    /// partition above.
    ///
    /// EXCLUDES an entry whose evidence never FIRED (rung B runs, but rung A was never entered,
    /// so the entry is outside the partition) and the other three arms.
    Index ipqp_fallback_rung_b = 0;

    /// Ladders entered at a CLAMPED first rung -- `ElasticLadderReport::rho0_ceiling_hit` summed
    /// over every activation, on BOTH routes into `run_elastic_ladder`. A clamp means the
    /// evidence priced the violation above what the placement rule allows: the escalation
    /// headroom cap `kElasticRhoMax / kElasticRhoFactor`, or the dual-regularization safety cap
    /// `kElasticRhoDualMuSafety / dual_mu` (sqp_driver.h's THE PLACEMENT BOUND).
    ///
    /// EXCLUDES the ordinary placements. MEASURED ON TWO DIFFERENT SETS, which is why the two
    /// figures differ: 0/5 over W2 T3's five `w2_*` UNIT-fixture placements (the largest is
    /// 5.556e5, under the 1e6 cap), and 2/6 over the DRIVER fixtures HS10/HS11/HS15 at kIpm,
    /// whose fired escapes price 1e-2, 1e-2, 2.558e6 (HS10), 1.784e7, 5.754e4 (HS11) and 2.776e5
    /// (HS15) -- the two above 1e6 clamp. The driver figures were measured at W2 T5's fix round 1
    /// by a temporary instrumented run; only HS11's clamp is pinned in the tree (HS11's driver
    /// test). A nonzero reading is the frequency signal the
    /// telemetry was added for, not a statistic. Identically 0 on the no-evidence route, which
    /// is placed at the floor.
    Index elastic_rho0_ceiling_hits = 0;

    /// Rung-A RETRIES AT THE FLOOR: entries where a DECLINED rung A placed above
    /// `kElasticRhoInit` was re-run once at the floor before rung B was considered. Each retry
    /// is a second, real activation and is counted in `elastic_activations` and in
    /// `elastic_from_ipqp_escape` too -- the extra cost the rule is worth reporting -- while the
    /// ENTRY partition above counts the RETRY's own outcome, never the declined attempt as well.
    ///
    /// EXCLUDES a decline already AT the floor (there is nothing to retry) and every rung A the
    /// engine did not decline. Bounded by 1 per fallback entry.
    Index elastic_floor_retries = 0;

    /// EQP refinement work, summed over every QP solve this driver spent --
    /// subproblems, SOC re-solves, elastic rungs and the restoration
    /// sub-solve alike, exactly like qp_minor_iters/factorizations above.
    /// The two fields carry QpCounters' meanings unchanged:
    /// border_refine_steps is TOTAL steps including each bordered solve's
    /// mandatory first, eqp_refine_steps is EXTRA steps beyond solve_eqp's
    /// mandatory first and so is identically 0 -- the flag-gated loop it
    /// once counted was deleted as measured-inert on both shipped backends
    /// (see QpCounters' note above). They exist so the refinement A/B could
    /// price refinement per solve rather than per subproblem, and the zero
    /// one stays as that A/B's standing invariant.
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    /// Rungs of the QP engine's SUSPECT-STALL ESCALATION LADDER
    /// (qp_engine.h's section 4b), summed over every QP solve this driver
    /// spent -- same aggregation set as the two refinement counters above,
    /// and named identically to QpCounters::suspect_escalations so the
    /// per-solve and per-driver readings are the same quantity at two
    /// scales.
    ///
    /// ZERO IS THE EXPECTED READING and a nonzero one is a finding, not a
    /// statistic: a rung is spent only when a would-be-kOptimal QP exit off
    /// a SUSPECT factorization failed the free-block stationarity check,
    /// i.e. when the subproblem's linear algebra produced a point that is
    /// not a KKT point of anything. Nonzero-but-solved means the engine
    /// escalated its way out and the answer stands; a QP that came back
    /// kNumericalError with detail::kMaxSuspectEscalations rungs spent
    /// exhausted the ladder, and THAT PAIRING IS THE EXHAUSTION DIAGNOSTIC
    /// -- it is the only place the driver's caller can see why the
    /// subproblem was refused, which is why this rolls up rather than dying
    /// at the QP boundary.
    Index suspect_escalations = 0;

    /// Pardiso phase-11 (symbolic analysis) calls PAID THROUGH THE
    /// BORDER/REBUILD PATH (qp_engine.h's rebuild_k0(), same scope as
    /// QpCounters::symbolic_analyses), summed over every QP solve this
    /// driver spent -- same aggregation set as the two refinement counters
    /// and suspect_escalations above, named identically to
    /// QpCounters::symbolic_analyses (see that field's note for what it
    /// counts and why). THIS IS NOT A COUNT OF EVERY PHASE-11 CALL THIS
    /// SOLVE PAID ACROSS EVERY CODE PATH: the elimination path constructs
    /// its own per-solve KktFactor and pays its own symbolic analyses
    /// through it, which this field does not see and so under-reports on a
    /// solve that uses that path (measured: HS39 reads 1 here against 2
    /// real analyze() calls). A driver whose every major keeps a single,
    /// unshared BorderState (the ordinary case) should see this stay small
    /// (1, or 0 on an immediate-convergence solve) across the WHOLE solve
    /// regardless of how many `factorizations` were paid, since
    /// rebuild_k0() reuses one KktFactor's cached sparsity pattern across
    /// same-pattern rebuilds; a large value on an otherwise-ordinary solve
    /// is the regression signal this counter exists to catch (detaching
    /// onto a fresh KktFactor on every value-changing major, discarding
    /// that cache).
    Index symbolic_analyses = 0;

    /// The RESOLVED warm-start level this solve actually used -- NOT the
    /// level a caller merely requested by populating `warm`
    /// (sqp_driver.h's WARM-START INGEST note has the resolution rule).
    /// Always kCold on the 2-arg solve(model, x0) overload (there is no
    /// `warm` object to resolve there). On the 3-arg overload:
    /// kCold when `warm.valid` is false, when `warm` is not dimensionally
    /// compatible with this model, when any ingested vector is non-finite,
    /// when the seeded `lambda_i >= 0` clamp DEGRADED the object
    /// (sqp_driver.h's THE SEEDED DUAL CLAMP), or when
    /// SqpOptions::start_level caps it there.
    ///
    /// A HASH MISMATCH -- the hash == 0 "no model was seen" sentinel a
    /// mesh-transferred or crossover object carries included -- lands on
    /// **kSeeded**, which takes the object's values (x, duals, activity
    /// hint) and refuses its provenance-dependent state (see
    /// core/start_level.h's StartLevel note for the exact list). kSeeded is
    /// also what a `start_level` ceiling of kSeeded produces from an object
    /// that would otherwise have earned kWarm/kHot. Then: kWarm when a
    /// structural match was confirmed and the ceiling allows it, but either
    /// `warm.hot` was null or the FIRST subproblem's own solve did not
    /// actually reuse the cache; kHot exactly when a structural match was
    /// confirmed, the ceiling allows it, `warm.hot` was non-null, AND that
    /// first subproblem's own QpCounters::k0_reused reads true -- i.e. this
    /// field records what WAS OBSERVED TO HAPPEN, not what was merely
    /// offered: QpEngine's own reuse-eligibility conditions (a)-(e)
    /// (qp_engine.h -- (e) is the shared object's own generation counter,
    /// on top of the four fingerprint conditions (a)-(d)) are the sole gate
    /// on whether an offered hot handle is actually usable, and a mismatch
    /// there degrades this field to kWarm silently rather than lying about
    /// which level ran. Reading `k0_reused` -- rather than inferring reuse
    /// from `qp_factorizations == 0` -- is deliberate: that count can read
    /// zero for reasons unrelated to reuse (an empty reduced system, or a
    /// crossed-bounds exit before the loop ever runs), which `k0_reused`
    /// does not confuse with a genuine cache hit (see
    /// QpCounters::k0_reused's note above).
    StartLevel start_level_used = StartLevel::kCold;

    /// Majors solved while globalization.h's FULL-STEP MODE was armed
    /// (SqpOptions::warm_full_step). It counts the same events major_iters
    /// does -- SUBPROBLEMS SOLVED, so a routed QP failure taken under the
    /// mode counts exactly as an accepted unit step does -- and is
    /// therefore always <= major_iters, with 0 on every cold solve, every
    /// solve with the lever off, and every solve whose `warm` did not
    /// resolve.
    ///
    /// IT IS NOT A COUNT OF UNIT STEPS TAKEN. A trial the mode declined to
    /// accept (only one thing does that: a NON-FINITE trial point, which
    /// FunnelStrategy::judge still rejects under the mode) is counted here
    /// too, for the same reason major_iters counts it -- the QP was solved.
    Index full_step_majors = 0;

    /// WATCHDOG RESTORES: how many times the driver ended the full-step
    /// mode by restoring the best iterate it had seen under it (by
    /// ||KKT||inf) and handing the solve back to funnel globalization
    /// there. Bounded by 1 today -- the mode is entered ONCE PER SOLVE, at
    /// the first measurable iterate, and nothing re-enters it -- so the
    /// useful reading is BINARY: 0 means the mode either never engaged or
    /// ran all the way to the convergence test (the outcome it exists to
    /// produce), 1 means it was cut short by divergence or a stall.
    ///
    /// A RESTORE IS COUNTED EVEN WHEN IT MOVES NOTHING. On the stall exit
    /// the best iterate can BE the current one (e.g. a subproblem that kept
    /// failing at an iterate that never moved), and the restore is then a
    /// no-op on x -- but the EVENT, the mode giving up, is what this
    /// counts, and that happened either way.
    Index watchdog_restores = 0;

    // EVAL ECONOMICS: evals_full is a query that fetched derivatives
    // (grad/Je/Ji, whether or not it also read values first); evals_values
    // is a query that fetched f/cE/cI ONLY and never got upgraded. A
    // NlpModel query costs one of two things: a FULL eval_nlp (f, grad, cE,
    // Je, cI, Ji -- sqp_driver.h's NlpEval) or a VALUES-only eval_nlp_values
    // (f, cE, cI, via NlpModel::eval_values, no derivatives). These two
    // counters partition every model query this solve made between the two
    // -- evals_full + evals_values is the total number of times x (or a
    // probe point) was evaluated against the model at all, ACROSS THE WHOLE
    // SOLVE, folded in from a restoration sub-solve exactly as
    // qp_minor_iters/factorizations are (sqp_driver.h's RESTORATION PHASE
    // fold), since that work is genuinely spent evaluating the model, not a
    // property of the wrapper's own variables.
    //
    // evals_full COUNTS: the first iterate's evaluation, every ACCEPTED
    // trial (direct or via a promoted SOC correction -- exactly one full
    // eval per acceptance, whichever point it lands on), a restoration
    // exit's re-evaluation of the main model at the point restoration
    // reached (when that point is finite), and the restoration seed's own
    // guard query at the exhausted-ladder site, charged whether the guard
    // passes or fails (W2 T4). evals_values COUNTS: every
    // REJECTED trial's evaluation (including one that went through SOC and
    // was still not promoted), the warm-resolution probe's f/cE/cI fetch
    // (sqp_driver.h's WARM-START INGEST note), and nothing else today.
    // ONE EXCEPTION, W2 T4: a rejected trial that restoration UPGRADED AT
    // THE REQUEST SITE (the seeding guard passed there) moves to
    // evals_full and out of evals_values -- the partition stays exact.
    //
    // BOTH ARE ZERO ON A SOLVE THAT NEVER MEASURED THE MODEL AT ALL -- there
    // is no such solve today (every solve_impl call evaluates at least the
    // entry point) -- and evals_values is IDENTICALLY ZERO on a solve that
    // never rejected a trial and never ran the warm-resolution probe (e.g.
    // every 2-arg solve() call, and a 3-arg call whose `warm` failed the
    // ceiling short-circuit or the dimension check before ever touching the
    // model). A caller comparing before/after on a rejection-heavy fixture
    // reads evals_full here as the AFTER count and (evals_full +
    // evals_values) as the BEFORE count that same fixture would have paid
    // before values-only queries existed, when every one of these queries
    // was a full eval_nlp.
    //
    // WHAT THESE COUNT, AND WHAT THEY DO NOT: both fields are incremented
    // from the driver's own control-flow DECISION at each call site -- was
    // this trial's fate an upgrade to full, or not -- never from observing
    // which NlpModel method actually executed. Today the two always agree,
    // because every increment site sits immediately next to the real call it
    // describes (eval_nlp_values/upgrade_to_full/eval_nlp -- sqp_driver.h),
    // so reading the decision is equivalent to observing the dispatch. But a
    // future edit that changes which function a call site invokes WITHOUT
    // also updating that site's counter increment would silently
    // desynchronize the two: these fields are not a self-verifying trace and
    // must not be read as one (a hand-run mutation confirmed it: reverting
    // sqp_driver.h's rejected-trial call site to a full eval_nlp while
    // leaving the counter logic untouched left both fields UNCHANGED on
    // every fixture; the regression was caught only by an independent
    // model-call-count cross-check -- a CountingModel decorator counting
    // eval_grad/eval_jac_e/eval_jac_i directly; tests/sqp/test_sqp_driver.cpp's
    // SqpDriverEvalEconomics battery pairs exactly this kind of cross-check
    // with every assertion made against these two fields). A results note or
    // benchmark claim built on them should keep doing the same --
    // corroborate against an independent call count on at least one fixture
    // -- rather than treating these counters as sufficient regression
    // coverage by themselves.
    Index evals_full = 0;
    Index evals_values = 0;

    /// Did THIS solve stop because the caller's own MINOR-ITERATION BUDGET
    /// ran out? 0 or 1 -- never more, because the budget test is an EXIT:
    /// the first time it fires the solve returns. A flag with a counter's
    /// type, kept as an Index so it SUMS over a ledger exactly like every
    /// other field here (a sweep's total is then "how many of my solves I
    /// cut short", the quantity continuation.h aggregates into
    /// ContinuationResult::proposals_abandoned).
    ///
    /// WHAT IT COUNTS: the 4-argument solve(model, x0, warm, minor_budget)
    /// overload was given a POSITIVE budget, this solve reached the top of a
    /// major having already spent >= that many qp_minor_iters, and it was
    /// NOT converged there -- so it returned SqpStatus::kMaxIter at that
    /// iterate instead of building another subproblem. See sqp_driver.h's
    /// PROBE BUDGET note for the full contract (checked BETWEEN majors, so
    /// the minors actually spent are >= the budget and bounded by budget +
    /// whatever the crossing major cost, not by the budget itself).
    ///
    /// WHAT IT DOES NOT COUNT, each a real, reachable case a reader must not
    /// confuse with an abandonment:
    /// - an ordinary max_iter exhaustion (also kMaxIter, this field 0);
    /// - a kBudgetExhausted exit under SqpOptions::budget_mode, which is a
    ///   DIFFERENT budget with a different contract (best-iterate hand-off,
    ///   "continue at the same p") -- a probe-budget stop never reports that
    ///   status even when budget_mode is on;
    /// - a solve that spent more than the budget and CONVERGED anyway. The
    ///   convergence test wins at every pass, so an over-budget solve that
    ///   is a KKT point is reported kOptimal with this field 0 and its
    ///   answer is kept. Abandonment can only ever cost work that was about
    ///   to be spent, never an answer that was already found.
    /// A solve given no budget (the default, and every 2-/3-argument solve()
    /// call) leaves this identically 0, which makes the whole mechanism
    /// inert -- byte-identically so -- wherever it is not armed.
    Index probe_budget_stops = 0;

    /// How many inequality ROWS and variable BOUNDS
    /// SqpOptions::crash_basis seeded into the FIRST QP subproblem's working
    /// set on this solve. Identically 0 with the lever off, on every
    /// warm/hot ingest (the seed is cold-only by construction), and on a
    /// solve that never builds a subproblem at all.
    ///
    /// COUNTED SEPARATELY BECAUSE THE TWO HALVES HAVE DIFFERENT CEILINGS,
    /// and folding them would hide the one that matters: a wide-window
    /// walk's variable-bound events measure ~29% of all working-set events
    /// at a 1.00x-1.08x per-bound REPEAT rate (pinned once, released once,
    /// never revisited), against 2.7x-4.1x per inequality row -- so a row
    /// seeded well can remove churn while a bound seeded well can only ever
    /// remove a one-shot transit. A reader pricing this lever must be able
    /// to tell which half moved.
    ///
    /// SEEDS OFFERED, NOT SEEDS HONOURED. qp_engine.h's
    /// ingest_seed_working_set applies its own WINDOW-CONSISTENCY RULE and
    /// silently drops any seeded bound that falls outside the first
    /// subproblem's trust-region window; a dropped hint is still counted
    /// here. The two agree whenever the radius does not cut the seeded bound
    /// off, but the field's claim is about what the DRIVER proposed.
    ///
    /// FOLDED FROM A RESTORATION SUB-SOLVE, exactly like evals_full/
    /// evals_values and the work counters above. That sub-solve inherits
    /// this lever and is cold by construction, so a solve that entered
    /// restoration with the lever on can report contributions from TWO
    /// first-subproblems, not one -- and the feasibility wrapper's slack
    /// columns start ON their own lower bound of 0, so the second
    /// contribution is typically large. Read a nonzero value as "seeds this
    /// solve proposed", never as "seeds the caller's own x0 supplied".
    Index crash_seeded_rows = 0;
    Index crash_seeded_bounds = 0;

    /// 1 iff THIS solve's warm-start resolution landed on
    /// StartLevel::kSeeded, else 0 -- a flag with a counter's type, kept as
    /// an Index so it SUMS over a ledger (a sweep's total is then "how many
    /// of my solves ingested values without provenance", the quantity a
    /// crossover or mesh-refinement loop reports on). Not redundant with
    /// `start_level_used`, which answers the same question for ONE solve and
    /// cannot be summed, nor with ledger.h's `level_histogram()`, which
    /// answers it for a whole ledger but requires one to be attached; this
    /// field rides on the SqpSolution every caller already has. Identically
    /// 0 on the 2-arg solve() overload, on every kCold/kWarm/kHot
    /// resolution, and on a seeded object that DEGRADED (see below).
    Index n_seeded = 0;

    /// How many `lambda_i` entries the seeded `lambda_i >= 0` enforcement
    /// ZEROED on this solve -- sqp_driver.h's THE SEEDED DUAL CLAMP.
    /// Bounded by model.mi(). Identically 0 at every other level, because
    /// the clamp is scoped to kSeeded (that note says why), and identically
    /// 0 on a well-formed seed, which is the ordinary case: a nonzero
    /// reading means the producer handed over at least one negative price
    /// on a row the destination model reports GEOMETRICALLY ACTIVE. Rows
    /// that are strictly SLACK never reach the clamp -- THE INGESTED
    /// MULTIPLIERS ARE MADE COMPLEMENTARY clear has already zeroed them,
    /// and it runs FIRST by contract -- so this counts only the sign
    /// violations that survived a geometric explanation.
    ///
    /// WHAT NEITHER FIELD COUNTS, stated because it is the one event a
    /// reader will look for here: a seeded object DEGRADED TO kCold by a
    /// beyond-the-band negative price is not counted by either. It need not
    /// be: a 3-argument solve() whose `warm` is valid, dimensionally
    /// compatible and finite can report `start_level_used == kCold` only
    /// through that degradation or through SqpOptions::start_level's own
    /// ceiling -- both readable directly. A degraded solve reports n_seeded
    /// == 0 and seeded_clamped == 0 because it ingested nothing at all,
    /// which is the truthful reading of a refusal.
    Index seeded_clamped = 0;

    /// How many inequality ROWS the INGESTED activity hint proposed active
    /// on this solve -- the size of the working set a kSeeded object handed
    /// the first subproblem, and on the crossover route exactly what
    /// `from_interior_point` inferred.
    ///
    /// WRITE-ONLY. Nothing in the driver, the engine or the globalization
    /// reads it or branches on it; it exists so a corpus row can TAG a
    /// crossover cell with the quality of the hint it was given, the one
    /// thing separating an "activity-only" start from a cold one and
    /// otherwise invisible in the counters (a hint that was correct and a
    /// hint that was empty produce the same `n_seeded == 1`).
    ///
    /// SCOPED TO kSeeded, and identically 0 at kCold/kWarm/kHot. Not a
    /// statement that a kWarm object carries no hint -- it always does --
    /// but that at kWarm the hint's provenance is confirmed, so its size
    /// answers no question a reader has; kSeeded is the only level whose
    /// hint arrived without provenance. A seeded object DEGRADED to kCold
    /// reports 0, for the same reason `n_seeded` does: it ingested nothing
    /// at all.
    ///
    /// NAMED FOR ITS MOTIVATING PRODUCER, NOT SCOPED TO IT: a mesh transfer
    /// (mesh_transfer.h) or a hand-built object resolving kSeeded is
    /// counted identically; `from_interior_point` is simply the producer
    /// the counter was added for and the one whose inference rule the count
    /// grades.
    ///
    /// ROWS ONLY -- the bound half of the hint is deliberately not folded
    /// in, on crash_seeded_rows/crash_seeded_bounds' argument above (the
    /// halves have different ceilings and folding hides the one that
    /// matters). No consumer needs the bound count yet; adding it later is
    /// additive.
    ///
    /// SEEDS OFFERED, NOT SEEDS HONOURED, exactly like the crash-basis
    /// pair: qp_engine.h's WINDOW-CONSISTENCY RULE may drop any part of the
    /// hint that does not fit the first trust-region window, and a dropped
    /// row is still counted here.
    Index ip_activity_inferred = 0;

    /// The semismooth-Newton kernel's work, summed over every subproblem of
    /// this solve -- the same aggregation set qp_minor_iters and
    /// factorizations above use, and empty for the same reason they would
    /// be if no subproblem were solved.
    ///
    /// **ZERO ON EVERY SOLVE RUN AT THE SHIPPED DEFAULT**
    /// (`SqpOptions::qp_mode == QpMode::kWalk`): no SSN subproblem is
    /// solved there, so nothing writes here. Populated when the solve runs
    /// `QpMode::kSsn`.
    SsnCounters ssn;

    /// The IP-PMM interior-point tier's work, folded over every subproblem
    /// of this solve by `accumulate_ipqp_counters` -- the `ssn` field's own
    /// aggregation, for the third QP kernel (M6 W1). See `IpqpCounters`'s
    /// own doc comment for the fold rule.
    ///
    /// **ZERO ON EVERY SOLVE RUN AT THE SHIPPED DEFAULT**
    /// (`SqpOptions::qp_mode == QpMode::kWalk`): no IPQP subproblem is
    /// solved there, so nothing writes here -- structurally, not by
    /// arithmetic (the dispatch's kWalk arm constructs no `IpqpEngine`), and
    /// pinned as such by `IpqpDispatch.EveryIpqpCounterIsStructurallyZeroAt
    /// KWalkAndKSsn`. **LIVE SINCE M6 W1 TASK 6**, which wired the section
    /// 2.3 routing chain: at `qp_mode == QpMode::kIpm` every field below is
    /// written from a real solve. Two of the routing fields are DRIVER-SCALE
    /// and have no per-subproblem contribution at all
    /// (`ipqp_tier_retired_after`, `ipqp_declined_pinned`); see each field's
    /// own note.
    IpqpCounters ipqp;
};

} // namespace hven::solvers
