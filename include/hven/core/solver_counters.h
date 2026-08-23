// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// solver_counters.h -- the solver counter contract's types: QpCounters (one QP
// engine solve), SsnCounters (the semismooth-Newton kernel's own work) and
// SqpCounters (one whole SQP driver solve, which aggregates both).
//
// M3 PHASE-C S2 MOVED ALL THREE HERE, unchanged and with their member order
// untouched, QpCounters from `hven/qp/qp_types.h` and SsnCounters/SqpCounters
// from `hven/drivers/sqp_types.h`. Counters are the asserted currency of this
// project's correctness and performance claims (CLAUDE.md section 7) and
// `core/` is the counter contract's home; the move is also what lets
// `core/ledger.h` record a driver solve without including a `drivers/` header.
// Both donor headers include this one, so no call site changed.

#include <hven/core/start_level.h>

namespace hven::solvers {

// QP solver performance counters
//
// THE TWO REFINEMENT COUNTERS COUNT DIFFERENT THINGS ON PURPOSE, because the
// two paths have different mandatory baselines and the interesting quantity
// on each is "steps that would not have been taken before":
//
//   border_refine_steps  TOTAL refinement steps KEPT by every
//                        solve_bordered_eqp call in this solve, INCLUDING the
//                        mandatory first one -- the same accounting
//                        detail::kMaxBorderRefineSteps uses ("total,
//                        including the mandatory first"). So a solve that
//                        goes through the border path at all reports at least
//                        one step per bordered EQP solve, and the excess over
//                        that baseline is what Task 11b's iterated loop
//                        bought.
//   eqp_refine_steps     EXTRA refinement steps kept by every solve_eqp call
//                        in this solve, BEYOND the single unconditional step
//                        solve_eqp takes. IT IS NOW IDENTICALLY ZERO, and is
//                        kept as an instrumented invariant rather than as a
//                        live measurement: the flag-gated iterated loop it
//                        counted (QpOptions::eqp_refine) was DELETED once both
//                        shipped backends measured it inert -- 0 steps across
//                        27 HS problems x 2 tolerance regimes x 2 algebra
//                        modes plus 13 adversarial probes, on MKL and on
//                        Accelerate independently, with a structural identity
//                        (r_k = diag(reg)*c) explaining why a path whose first
//                        solve is a genuine regularized solve cannot fire the
//                        shared stopping rule. See
//                        docs/notes/2026-07-29-eqp-refinement-ab.md's
//                        DISPOSITION section and the Phase-C verdict in
//                        docs/notes/2026-07-29-accelerate-audit-results.md.
//                        A nonzero reading here would mean solve_eqp grew a
//                        second refinement step, which is a change, not a
//                        measurement -- which is exactly why the counter and
//                        its assertions stayed behind when the loop went.
//
// "KEPT" is the operative word in both: a candidate step rejected by the
// strict-decrease acceptance rule is discarded and NOT counted, so these
// count steps that moved the answer, not solves attempted.
struct QpCounters {
    Index factorizations = 0;
    Index schur_updates = 0;
    Index minor_iters = 0;
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;

    // Rungs of the SUSPECT-STALL ESCALATION LADDER this solve spent (see
    // qp_engine.h's section 4b). Each rung multiplies the effective
    // primal_delta by detail::kSuspectDeltaFactor after a would-be-kOptimal
    // exit off a kSuspect factorization failed the free-block stationarity
    // check. Zero on every solve whose linear algebra was trustworthy, which
    // is the overwhelming majority; a solve that reports kNumericalError with
    // this at detail::kMaxSuspectEscalations exhausted the ladder, and that
    // pairing IS the exhaustion diagnostic.
    Index suspect_escalations = 0;

    // PHASE-4 TASK 4 FIX ROUND 1. True iff qp_engine.h's border-mode reuse
    // gate (`reuse_eligible` in QpEngine::run(), conditions (a)-(e)) judged
    // the persisted K0/border cache trustworthy for THIS solve() call --
    // i.e. whether the engine actually skipped rebuilding K0 on the
    // strength of its OWN or an ADOPTED hot-start handle's history, not
    // merely whether `factorizations` happens to read 0. The two are NOT
    // the same claim: `factorizations == 0` can also arise from a reduced
    // system with nothing to factorize at all (every variable pinned, no
    // equalities, no working rows -- the elimination path's empty-system
    // short-circuit, reachable from border mode too) or from a solve that
    // never reached the loop (a crossed-bounds box, reported kInfeasible),
    // NEITHER of which says anything about the reuse cache. This field is
    // the direct signal a caller (sqp_driver.h's SqpCounters::
    // start_level_used correction) should read instead of inferring reuse
    // from the factorization count. Always false under
    // QpOptions::ws_algebra == kRefactorize, where the border-mode cache
    // does not exist.
    bool k0_reused = false;

    // PHASE-4 TASK 4 FIX ROUND 2. Number of backend SYMBOLIC-ANALYSIS
    // calls this solve() call actually paid for -- i.e. the number
    // of times qp_engine.h's rebuild_k0() found the analysis decision
    // `needed` before calling `factorize_checked()` (which skips the
    // analysis whenever the sparsity pattern is unchanged from the last
    // analyzed matrix -- kkt_calls.h; M3 phase C B2 hands that same decision
    // in rather than letting it be retaken, which changes when the pattern
    // is hashed and nothing else). Counted here, at the call site,
    // rather than inside the factor, so this stays a QP-engine-level
    // observable like every other QpCounters field and touches no
    // MKL-adjacent code. This is the direct regression net for the
    // re-review's finding 2c: detaching onto a FRESH BorderState (a fresh
    // KktFactor, with no cached pattern) on every value-changing major of an
    // ordinary, NEVER-SHARED solve paid a full re-analysis per major --
    // measured at 48 on HS38's own 48-major solve, vs 1 for a solve whose
    // sole-owner rebuilds reuse the SAME KktFactor's cached pattern. A
    // never-shared solve should show this at 1 (or 0, if the pattern never
    // needed rebuilding at all) regardless of how many `factorizations` it
    // pays; a solve whose fixture legitimately forces detaches (the
    // sharing/identity-mismatch tests) is expected to show more.
    Index symbolic_analyses = 0;

    // ---------------------------------------------------------------------
    // PHASE-6 TASK 3 -- THE WORKING-SET WALK COUNTERS (five here, six more
    // below in fix round 1 -- eleven in total).
    //
    // PURELY OBSERVATIONAL. None of the ELEVEN walk counters (the five here
    // and the six added in fix round 1, below) is read by any decision in
    // qp_engine.h; they are written and never consulted, so the walk's
    // trajectory is bit-for-bit what it was before they existed (verified:
    // `hven_sqp_bench --self-check` byte-exact and the full suite green in
    // both build configurations at the commit that added them).
    //
    // WHY THEY EXIST. Phase 5 closed with the F7 wide-window minor-stall as
    // an explicitly-owned open carry whose MECHANISM was unknown
    // (docs/notes/2026-08-01-phase-5-results.md Sec. 12). The counters that
    // existed then could not discriminate its candidate mechanisms, and the
    // reason is a level mismatch rather than a missing measurement: every
    // pre-existing field above counts LINEAR-ALGEBRA events (factorizations,
    // Schur updates, symbolic analyses, refinement steps), while every
    // candidate mechanism -- a degenerate stall, an add/drop churn cycle,
    // homotopy thrash, or a genuinely long monotone identification walk --
    // is a statement about the COMBINATORIAL walk, which nothing observed.
    // `minor_iters` alone cannot tell a solve that spent 510165 minors
    // cycling from one that spent them making progress, and
    // docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2 says so directly
    // ("the pathology is entirely in HOW MANY, not in how expensive each
    // one is"). These five close exactly that gap and nothing wider; the six
    // added in fix round 1 (below) close a second one the review found inside
    // it -- these five cannot tell an inequality ROW from a variable BOUND.
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
    // EXCEPTION** (final fix wave, W8; the exception was always there and no
    // note said so). qp_engine.h's TEMPORARY-VERTEX START REPAIR (section 4b,
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
    // every convex corpus in this project reports these fields exactly, and
    // that is why the omission has never shown up in a measurement. It is
    // documented rather than plumbed deliberately: the counters exist to
    // describe the COMBINATORIAL WALK, and the repair is a pre-walk vertex
    // construction, not a step of it -- adding it to `ws_adds` would make the
    // churn fraction read a start-point property as walk churn. A reading that
    // needs the repair's pins must say so and count them separately.
    //
    // ws_adds        BLOCKING-CONSTRAINT ADDITIONS: one per minor iteration
    //                whose ratio test named a blocker that then joined the
    //                working set, counting an inequality row and a variable
    //                bound alike (BlockKind::kIneq / kBound). The
    //                negative-curvature ride's own blocker (section 4c) is
    //                counted here too, since it is the same ratio test
    //                reporting the same kind of event; a minor that took a
    //                full unblocked step to the EQP point (kind == kNone)
    //                adds nothing and is counted nowhere.
    //
    // ws_drops       WORKING-SET REMOVALS: one per constraint released by
    //                the Dantzig drop rule (drop_worst) plus one per
    //                constraint released by the zero-multiplier probe
    //                (probe_zero_multiplier_drops). A probe drop that is
    //                RESTORED because the probe declined it is not counted --
    //                the working set is the same on both sides of it, so
    //                counting it would break the net identity above.
    //
    // shift_adds     HOMOTOPY ADMISSIONS: rows that refresh_shifts() put
    //                into the working set because they are still violated
    //                (shift > 0), which is a structurally different event
    //                from a blocking-constraint add -- the drop rule
    //                deliberately SKIPS shifted rows, so a row admitted this
    //                way cannot leave until its shift reaches zero. INCLUDES
    //                the pre-loop refresh_shifts() call, i.e. the seed's own
    //                initial admission, which is why a healthy solve reports
    //                a large value here on its first QP and near-zero after.
    //
    // degenerate_steps  DEGENERATE PIVOTS: minor iterations that added a
    //                blocking constraint while moving the iterate by no more
    //                than the loop's own step tolerance (alpha * ||p||inf <=
    //                feas_tol * max(1, ||x||inf) -- the SAME threshold the
    //                loop's KKT-point test uses one line earlier, so the two
    //                are consistent by construction rather than by a new
    //                tolerance). This is the textbook degeneracy signal: the
    //                working set grew but the objective could not have
    //                improved. qp_engine.h implements NO anti-cycling rule
    //                (no Bland, no Harris, no EXPAND, no perturbation) and
    //                says so; this field is the first observation of whether
    //                that omission ever costs anything on a real corpus.
    //
    // degenerate_run_max  The LONGEST CONSECUTIVE run of degenerate steps in
    //                this solve, where "consecutive" means back-to-back minor
    //                iterations. **A RUN IS BROKEN BY A MINOR ON WHICH THE
    //                ITERATE MOVED, NOT BY ANY MINOR THAT MERELY DIFFERS IN
    //                KIND** -- this line used to say "an ordinary step, a
    //                drop, a probe, a ride" all reset it, and the drop and the
    //                probe never have (final fix wave, W7; the doc was wrong,
    //                not the code). qp_engine.h resets the run in exactly
    //                three places and states the omissions deliberately: a
    //                non-degenerate ordinary step, a taken RIDE, and a START
    //                REPAIR. A DROP iteration and a ZERO-MULTIPLIER PROBE do
    //                NOT reset it, because both snap x onto an EQP point
    //                already within step_tol of where it was -- the iterate
    //                did not move, and a run that spans them is still one
    //                stall. **SO A LARGE VALUE HERE IS "the longest stretch on
    //                which the iterate did not move", NOT "an unbroken run of
    //                back-to-back degenerate ADDS"**, and any reading of a
    //                measured figure has to say which of the two it means.
    //                (`degenerate_steps` above is unaffected: it counts the
    //                degenerate adds themselves and no run logic enters it.)
    //                This is the field that tells the two
    //                degeneracy pictures apart, and they call for different
    //                remedies: degenerate steps SPRINKLED through a healthy
    //                walk are the ordinary cost of a vertex with more active
    //                constraints than dimensions and want nothing done about
    //                them, while a long unbroken run is an iterate that has
    //                STOPPED MOVING while the working set keeps changing --
    //                the cycling class, and the only class an anti-cycling
    //                rule (Bland/Harris/EXPAND) would address. It is reported
    //                rather than acted on: qp_engine.h still implements no
    //                anti-cycling rule and this field changes no decision.
    //
    // ---------------------------------------------------------------------
    // PHASE-6 TASK 3, FIX ROUND 1 -- THE SIX FIELDS THAT MAKE THE ABOVE
    // TRACEABLE RATHER THAN INFERRED. The review found the study's central
    // "the walk RE-DISCOVERS its rows" claim was an inference from
    // ws_adds >> |W*| plus a pigeonhole argument that does not actually
    // close: ws_adds and ws_drops MERGE inequality rows with variable bounds
    // and carry no constraint IDENTITY, so a walk touching many DISTINCT
    // bounds once each -- the brief's own "bound-flip" alternative -- fits
    // every number equally well. These six close it by direct measurement.
    //
    // ws_adds_bound   Of `ws_adds`, how many were VARIABLE-BOUND pins
    //                 (BlockKind::kBound). The inequality-row half is
    //                 ws_adds - ws_adds_bound. `shift_adds` needs no such
    //                 split: refresh_shifts() only ever adds inequality rows.
    // ws_drops_bound  Of `ws_drops`, how many released a variable bound
    //                 rather than an inequality row, on both drop routes.
    //
    // distinct_ineq_added / distinct_bound_added
    //                 How many DISTINCT inequality rows / DISTINCT variables
    //                 this solve ever put into the working set, by ANY route
    //                 (blocking add, ride, or homotopy admission). These are
    //                 the fields that settle re-discovery without an
    //                 argument: the mean number of times a touched
    //                 constraint was admitted is
    //
    //                   (ws_adds - ws_adds_bound + shift_adds) / distinct_ineq_added
    //
    //                 for rows and ws_adds_bound / distinct_bound_added for
    //                 bounds. **THAT RATIO IS ADMISSION MULTIPLICITY, WHICH
    //                 COUNTS EACH CONSTRAINT'S FIRST ADMISSION** -- so the
    //                 number of RE-admissions is the ratio MINUS ONE, and a
    //                 reading must say which of the two it quotes (final fix
    //                 wave, W3: published prose had called the multiplicity a
    //                 re-admission rate). A value near 1 means "touched once,
    //                 never repeated" (the bound-flip picture); a value well
    //                 above 1 means re-discovery, per constraint class
    //                 separately. `distinct_bound_added` additionally excludes
    //                 the start repair's pins -- see the named exception in
    //                 THE ARITHMETIC A READER SHOULD DO WITH THEM above.
    //
    // drop_ties       Drop decisions (drop_worst) in which TWO OR MORE
    //                 candidates fell inside the rule's relative tie window
    //                 (detail::kEngineDropTieTol), i.e. decisions actually
    //                 settled by the largest-angle/first-index tie-break
    //                 rather than by the most-negative multiplier.
    // ratio_ties      Ratio tests whose blocking constraint was chosen from
    //                 TWO OR MORE candidates at EXACTLY the same minimum
    //                 ratio, i.e. settled by the scan order (inequality rows
    //                 before bounds, ascending index) rather than by the
    //                 ratio. EXACT equality, deliberately: the tie that
    //                 matters for degeneracy is several constraints at zero
    //                 slack, and those compare bit-equal. NEAR-ties are not
    //                 measured -- counting them needs a second pass over
    //                 every candidate on every minor, which is a real cost in
    //                 a loop that runs half a million times.
    //
    // Together, drop_ties and ratio_ties are what let the tie-break question
    // the Phase-6 brief named as a deliverable ("ties?") be answered from a
    // measurement instead of from the degeneracy zero, which is a statement
    // about STEP LENGTH and says nothing about tie multiplicity.
    Index ws_adds = 0;
    Index ws_drops = 0;
    Index shift_adds = 0;
    Index degenerate_steps = 0;
    Index degenerate_run_max = 0;
    Index ws_adds_bound = 0;
    Index ws_drops_bound = 0;
    Index distinct_ineq_added = 0;
    Index distinct_bound_added = 0;
    Index drop_ties = 0;
    Index ratio_ties = 0;
};

// Work counters for the semismooth-Newton kernel. One instance lives inside
// SsnResult (ssn_engine.h) describing ONE QP solve; a second lives inside
// SqpCounters below, where Task 5 will aggregate over every subproblem of a
// whole SQP solve, exactly as qp_minor_iters/factorizations already aggregate
// the walk's QpCounters.
//
// PURELY OBSERVATIONAL, like the eleven walk counters above: nothing in
// ssn_engine.h reads any of these back to make a decision, so writing them
// cannot move a trajectory.
//
// **THREE OF THE SIX ARE STRUCTURALLY ZERO AS OF TASK 3**, and that is a
// scope statement rather than a measurement: Task 3 implements the LOCAL
// method only -- full Newton steps, no line search, no proximal
// regularization, no uncertain set (ssn_engine.h's SCOPE banner). The
// safeguards that would move ssn_backtracks, ssn_prox_updates and
// ssn_uncertain_peak off zero land in Task 4, and the fields land here now so
// that Task 4 adds behaviour rather than an interface.
struct SsnCounters {
    // Newton steps TAKEN. The convergence test runs BEFORE each step, so a
    // start point already inside fb_tol reports 0 here.
    //
    // **IT DOES NOT EQUAL THE FACTORIZATION COUNT, AND HAS NOT SINCE TASK 4**
    // (Task-4 deviation D6, corrected here in fix round 1 -- this text still
    // asserted Task 3's "a solve that reports k reports k factorizations",
    // which is the one place in the repository that stated the retired
    // invariant, and it is the file Task 5 aggregates through). The invariant
    // is now
    //
    //     ssn_iters <= factorizations,
    //
    // for two reasons, both in ssn_engine.h: an ATTEMPT can pay its
    // factorization and then take no step (a rejected line search, a wrong
    // inertia), and under SsnSafeguards::kFull a CERTIFYING exit pays one more
    // for the second-order verification read at the point it certifies
    // (ssn_engine.h section 7b). Under kBare -- Task 3's kernel -- equality is
    // exact and Task 3's contract stands verbatim. Anything downstream that
    // divides one by the other, or that reads either as the other, must say
    // which it means.
    Index ssn_iters = 0;
    // Newton steps whose IMPLIED ACTIVE SET differed from the preceding
    // step's -- one per such step, not one per constraint that moved. The
    // implied set is the partition the generalized Jacobian itself selects
    // (row k active iff its FB pair has lambda_k > s_k, equivalently iff
    // alpha_k > beta_k -- see ssn_engine.h), and on the FIRST step it is the
    // caller's activity hint when one was supplied. This is the counter that
    // distinguishes the two failure pictures the walk could not tell apart:
    // a large value against a small iteration count is a set that keeps
    // changing wholesale (the thrash class), while zero flips after the
    // first step is the identification the kernel exists to buy.
    Index ssn_bulk_flips = 0;
    // TASK 4. Line-search backtracks. Structurally 0 in Task 3 (the local
    // method takes full steps unconditionally).
    Index ssn_backtracks = 0;
    // TASK 4. Proximal-center/sigma updates. Structurally 0 in Task 3.
    Index ssn_prox_updates = 0;
    // Solves that ended on an SsnEscape other than kNone (ssn_engine.h). At
    // the SsnResult scale this is 0 or 1; at the SqpCounters scale it is the
    // number of subproblems that escaped.
    Index ssn_escapes = 0;
    // TASK 4. Peak size of the UNCERTAIN SET (rows the safeguarded method
    // declines to assign to either branch). Structurally 0 in Task 3, which
    // has no uncertain set at all.
    Index ssn_uncertain_peak = 0;
    // TASK 5 FIX ROUND 1 -- THE TIER-3 STABLE-FACE REFINEMENT, both polarities.
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
    // `sqp_driver.h`'s charge_refused_face_refinement can increment
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
    Index ssn_refinements = 0;
    Index ssn_refine_refused = 0;
    // TASK 6 -- INSTRUMENT ONLY, both of them, and both driver-scale like the
    // pair above (no SsnResult ever writes either; the engine does not know
    // the refinement exists). They exist because Task 6's brief names two
    // measurements the shipped counters cannot answer, and answering either by
    // arithmetic on the pair above would be WRONG rather than merely coarse.
    //
    // `ssn_refine_factorizations` -- the factorizations QpEngine::refine_on_face
    // ITSELF paid, summed over every attempt, accepted or refused. It is NOT
    // `ssn_refinements + ssn_refine_refused`: refine_on_face short-circuits an
    // EMPTY face and fails its rank pre-screen BEFORE any factorization, so a
    // refusal can cost 0 (its own contract says both screens precede the
    // solve). Task 6 reports "the refinement's share of total factorizations at
    // corpus scale", and that share is this field over
    // SqpCounters::factorizations -- a ratio of two measured quantities rather
    // than a count standing in for a cost.
    //
    // `ssn_refine_neg_duals` -- STRICTLY NEGATIVE inequality multipliers ADOPTED
    // from an ACCEPTED refinement, summed over rows and over refinements (so a
    // refinement adopting three negative prices contributes 3). This is Task-5
    // re-review finding NF-1 made observable: before the tier-3 refinement a
    // certifying SSN exit guaranteed lambda >= -O(fb_tol) by the FB row
    // structure, and after it the adopted multipliers are `price(...)`'s output
    // on the caller's face, UNBOUNDED IN SIGN -- with no driver-side analogue of
    // the walk's drop rule to re-gate them. The review's recommendation was
    // literally "measure dual sign on the scale corpus explicitly rather than
    // inferring it from HS", and this is that measurement's counter half (the
    // magnitude half is the corpus row's own model-level KKT `dual_sign`).
    // NO TOLERANCE: the test is `< 0.0`, because a sign is a sign, and a
    // threshold here would be a new tuning constant on an observational field.
    Index ssn_refine_factorizations = 0;
    Index ssn_refine_neg_duals = 0;
    // =========================================================================
    // PHASE-7 TASK 6b -- THE ESCAPE-REASON CENSUS (docket D6, Grant-adopted
    // option (c)). OBSERVATIONAL, like every field above it.
    // =========================================================================
    //
    // WHY. Before this, `ssn_escapes` counted hand-offs and said nothing about
    // WHY, so the two watch items the Task-5/6 reviews carried forward were
    // untestable on anything but hand-built fixtures: (i) the residual 0.55%
    // false-`kInfeasible` rate under combined extreme scaling
    // (docs/notes/2026-08-07-ssn-safeguards.md section 13.6), and (ii) the
    // MISLABEL -- 4 of 5 infeasible trust-region fixtures escape
    // `kNoContraction`/`kBudget` rather than `kInfeasibleSuspect` (section
    // 13.7). Both are statements about the DISTRIBUTION of escape reasons, and
    // a scale corpus that records only the total cannot carry either.
    //
    // A COUNT PER REASON, not a "last reason". An SQP solve escapes on many
    // subproblems and a single label would lose every one but one; the
    // distribution is the object both watch items are about.
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
    //   sqp_driver.h's `ssn_exit_is_a_usable_step` declined (a certifying exit
    //   that left the trust region by more than the derived bound) went to the
    //   walk exactly like an escaped one did, and `ssn_escapes` already counts
    //   it at the driver scale. It is therefore DRIVER-SCALE ONLY: no
    //   SsnResult ever carries a nonzero one, exactly like
    //   `ssn_refinements`/`ssn_refine_refused` above.
    //
    //   **AND IT IS STRUCTURALLY ZERO ON THE SHIPPED PATH, WHICH IS NOT THE
    //   SAME AS UNUSED.** It is written at the branch sqp_driver.h's
    //   `kSsnTrViolationFactor` note already calls "PROVABLY INERT ON THE
    //   CERTIFYING PATH": the gate's bound is derived from the kernel's own
    //   `|phi| <= fb_tol`, so no certifying exit ssn_engine.h can produce
    //   reaches it, and no NLP fixture can drive it. A reader must therefore
    //   NOT read a zero here as "measured, and the gate never refused" on a
    //   corpus where nothing could have refused. The bucket exists for the
    //   same reason the `ssn_escapes` increment beside it does: it is the one
    //   place a non-escape hand-off is counted, and if that derivation ever
    //   moves, the census must not silently stop partitioning.
    //
    // The other five are written by the ENGINE, beside its own `ssn_escapes`
    // increment, so a bare-kernel probe (bench/ssn_safeguard_probe.cpp) reads
    // the same census the driver aggregates.
    //
    // NO IN-MEMORY ABSENT SENTINEL (final branch review WAVE #14, T6b m5):
    // `bench_corpus.cpp`'s `--from-csv` reader stamps `-1` on these six fields
    // for a pre-schema-37 artifact that never measured them, but that
    // ABSENT convention exists ONLY at the CSV boundary -- these are plain
    // `Index`, not an optional/sentinel type, so a `-1`-stamped
    // `SsnCounters` that reached `accumulate_ssn_counters` (sqp_driver.h)
    // would silently SUM to -6 rather than signal "unmeasured". Not a live
    // path today (the scorer never accumulates the census fields, and a
    // `--from-csv` merge re-emits `-1` verbatim rather than summing it), but
    // a future caller that folds a CSV-sourced `SsnCounters` into a running
    // total must re-check for the sentinel first.
    Index ssn_escape_budget = 0;
    Index ssn_escape_singular = 0;
    Index ssn_escape_no_contraction = 0;
    Index ssn_escape_infeasible_suspect = 0;
    Index ssn_escape_indefinite = 0;
    Index ssn_escape_gate_refused = 0;
};

// Aggregate work counters for a whole solve.
//
// major_iters counts SUBPROBLEMS SOLVED -- not iterates evaluated, and not
// steps ACCEPTED: a subproblem that fails is counted, because the work was
// spent. A solve that converges immediately reports major_iters == 0 and a
// one-entry history. EXCEPT A TASK-7 SOC RE-SOLVE, WHICH IS NOT ONE OF
// THESE SUBPROBLEMS: it is tied to the TRIAL that triggered it (one per
// rejected trial, at most), not counted as its own major, and its cost is
// folded into qp_minor_iters/factorizations instead (see soc_steps below
// and sqp_driver.h's SECOND-ORDER CORRECTION note).
//
// THE HISTORY LENGTH THEREFORE DEPENDS ON WHERE THE SOLVE STOPPED, and a
// consumer indexing into it must branch on that (see SqpIterate):
//   - stopped AT an iterate (kOptimal, kMaxIter, and the non-finite-iterate
//     kNumericalError): history.size() == major_iters + 1, and the last row
//     has qp_solved == false;
//   - stopped ON a subproblem (kNumericalError propagated from the QP, or any
//     of the THREE kRestore routes of sqp_driver.h -- the funnel's signature,
//     the elastic tier's exhaustion and the radius floor):
//     history.size() == major_iters, every row has qp_solved == true, and the
//     last row carries the FAILING qp_status -- except on the kRestore
//     routes, where the last solve itself succeeded (the funnel's and the
//     floor's routes) or the ELASTIC re-solve did (Task 8's route) and it is
//     `verdict` that carries the reason.
// So `history[counters.major_iters]` is in bounds on the first family and
// OUT OF BOUNDS on the second. Task 6b consumes the history on exactly the
// second path; use history.back() and read qp_solved, never an index
// arithmetic on major_iters.
//
// ON A TERMINAL RESTORATION EXIT, SqpSolution::x IS NOT THE POINT THE LAST
// HISTORY ROW DESCRIBES (Task 9), and a consumer that plots one against the
// other must know it. The last row is the iterate that RAISED the request --
// its f, h and residuals are that point's -- while x/f/lambda_* are the
// RESTORED point the phase ended at, which has no row of its own because the
// phase produces none. Measured on the certified circle/line exit: the last
// row reports f = 2 at (1,1), and the solution reports f = 1.414 at
// (0.707, 0.707). (The two coincide only when the phase never moved the
// iterate -- e.g. the once-per-solve cap, which returns without running.)
//
// qp_minor_iters and factorizations are plain SUMS of the corresponding
// QpCounters fields over every subproblem solved, so they carry that header's
// semantics unchanged (qp_engine.h's COUNTER SEMANTICS note: minor_iters is
// one per QP major iteration; factorizations is not bounded by one per QP
// iteration in border mode). schur_updates is deliberately NOT aggregated --
// the brief's counter set does not name it, and the per-major values are in
// the history if a caller wants them.
//
// FROM TASK 7 ON THESE TWO SUMS CAN EXCEED sum(history[k].qp_* OVER k). A
// second-order correction (see sqp_driver.h's SECOND-ORDER CORRECTION note)
// re-solves the SAME iterate's subproblem with a shifted rhs, and that
// re-solve's minor_iters/factorizations are folded in HERE -- work was spent
// -- but deliberately NOT into the triggering row's qp_minor_iters/
// qp_factorizations, which stay exactly what the ORIGINAL (rejected) QP
// solve cost, preserving every existing reader's assumption that a row's
// qp_* fields describe ONE QpSolution. The SOC re-solve's own cost is
// therefore recoverable as the difference: counters.qp_minor_iters -
// sum(history[k].qp_minor_iters) (likewise factorizations), which is exactly
// how the SOC hot-start tests measure it.
//
// rejected_steps counts the majors spent SHRINKING THE RADIUS at an iterate
// that did not move. Two things produce one, and they are counted together
// because the loop treats them identically: a trial point the globalization
// strategy REJECTED (StepVerdict::kReject), and -- from Task 6b -- a
// SUBPROBLEM THAT FAILED but returned a usable iterate (sqp_driver.h's
// SUBPROBLEM FAILURE ROUTING), where there was no trial point to judge at
// all. Tell them apart by the row's qp_status: kOptimal on the first,
// kNumericalError/kMaxIter on the second. steps_accepted counts the
// complementary case: trials the strategy ACCEPTED (kAcceptF or kAcceptH),
// which is exactly the number of times the iterate moved on the CALLER'S OWN
// problem, and -- WITH THE ONE EXCEPTION BELOW -- the number of eval_hess
// calls the OPTIMALITY phase makes (the Hessian is evaluated once per iterate
// that builds a subproblem, never per trial -- see sqp_driver.h).
//
// THE EXCEPTION (Phase-5 Task 0): a solve that exits WITHOUT EVER BUILDING A
// SUBPROBLEM -- converged at its own start point, or a zero budget, both of
// which report steps_accepted == 0 AND major_iters == 0 -- pays ONE eval_hess
// anyway, in sqp_driver.h's make_warm_start, to hash the model's sparsity for
// the hand-off it emits (that function's THE ZERO-MAJOR PROBE note). So the
// exact optimality-phase count is steps_accepted + [major_iters == 0], and
// the extra call is bounded by one per solve, on precisely the solves that
// would otherwise have evaluated no Hessian at all. A solve whose start point
// the model could not evaluate pays nothing extra (it is not probed).
//
// FROM TASK 9 IT IS NOT THE NUMBER OF eval_hess CALLS THE SOLVE MAKES ON THE
// MODEL, and the difference is the restoration phase: RestorationModel's own
// eval_hess FORWARDS to the wrapped model's (with obj_scale = 0), so every
// restoration major costs one eval_hess on the caller's model too. The exact
// count is steps_accepted + (the accepted steps of the restoration sub-solve),
// and only the first term is reported -- the second is inside
// restoration_iters, which counts that sub-solve's majors rather than its
// acceptances. A consumer budgeting Hessian evaluations should treat
// steps_accepted + restoration_iters + 1 as the upper bound -- the trailing
// +1 is the zero-major probe above, which cannot fire on the same solve as
// any accepted step but is included so the bound holds unconditionally.
//
// BOTH ARE COUNTED EXPLICITLY BECAUSE THE OBVIOUS IDENTITY IS FALSE.
// steps_accepted == major_iters - rejected_steps holds only on a solve that
// stopped AT an iterate; a solve that stopped ON a subproblem spent a major
// that was neither accepted nor rejected -- the QP failed (kInfeasible, or a
// kNumericalError/kMaxIter that had already used its one retry), so there was
// no trial point to judge -- and EVERY kRestore row spends one that was judged
// but is neither. In general
//     major_iters = steps_accepted + rejected_steps + (kRestore rows),
// where the last term counts BOTH the terminal request (0 or 1) AND every
// request the phase RESUMED from, since a resumed restoration leaves its
// requesting row behind and the solve carries on. Task 9's own fix round
// measured a gap of 2 on the runaway-valley fixture (one resumed request plus
// one capped one), where the pre-Task-9 formula predicted at most 1. A
// consumer wanting "how many Hessians did this cost" must read steps_accepted
// (plus the paragraph above), not the subtraction.
//
// soc_steps counts SECOND-ORDER CORRECTION ATTEMPTS (Task 7): once per
// qualifying kReject trial (h_new > h_old, opts.enable_soc), regardless of
// whether the attempt succeeded -- a failed SOC re-solve, or one whose
// corrected point the funnel also rejected, still counts, because the work
// was spent. See sqp_driver.h's SECOND-ORDER CORRECTION note. It is 0 in
// every solve with enable_soc == false and was always 0 before Task 7.
//
// soc_applied/soc_qp_infeasible/soc_rejected (Phase-4, Task 1 of the
// warm-start plan) BREAK soc_steps' one number DOWN BY OUTCOME, closing a gap
// the HS battery's kappa_soc adjudication ran into: Task 7 measured ~70% of
// SOC attempts (on its own constructed fixtures) coming back kInfeasible from
// the re-solve itself, but that statistic was NOT re-derivable from an
// SqpSolution -- soc_steps counts attempts only, with no record of which of
// the three ways an attempt can end it hit (docs/notes/
// 2026-07-29-hs-battery-results.md, item 2's "what would be needed" section).
// These three fields are that record, and they are EXACTLY the three
// mutually-exclusive outcomes sqp_driver.h's SECOND-ORDER CORRECTION path can
// reach, so the identity
//     soc_steps == soc_applied + soc_qp_infeasible + soc_rejected
// holds on every solve:
//   - soc_applied: the re-solve returned kOptimal AND the strategy accepted
//     the corrected point (kAcceptF/kAcceptH) -- the attempt paid off. Same
//     event SqpIterate::soc_applied flags per-row; this is the solve-wide sum.
//   - soc_qp_infeasible: the re-solve itself did not return kOptimal (in
//     practice almost always kInfeasible -- the rhs shift scales with the
//     very violation that triggered SOC, so a large h_new is what "most
//     often" means in the SECOND-ORDER CORRECTION note's A FAILED SOC
//     RE-SOLVE table) -- there was no corrected point to judge at all.
//   - soc_rejected: the re-solve returned kOptimal but the strategy did NOT
//     accept the corrected point (a kReject, or a kRestore that was not
//     promoted) -- a corrected point existed and was still turned down.
// All three, like soc_steps itself, are folded in from a restoration
// sub-solve exactly as soc_steps is (the work was spent on that sub-solve's
// own SOC attempts too), so the identity holds on the aggregate counters of
// a solve that restored, not only on one that did not.
//
// elastic_activations counts SUBPROBLEMS REFORMULATED ELASTICALLY (Task 8):
// once per trial whose QP returned kInfeasible, whatever the reformulation
// then produced (a step, or the elastic tier's own exhaustion signal). It is
// therefore also an exact count of the kInfeasible subproblems this solve
// met, since from Task 8 on EVERY one of them is reformulated -- see
// sqp_driver.h's ELASTIC TIER note.
//
// elastic_escalations counts rho ESCALATIONS (x10 re-solves of the SAME
// elastic subproblem), summed over every activation -- NOT the number of
// elastic solves, which is elastic_activations + elastic_escalations. It is
// bounded by 6 per activation (kElasticRhoInit = 1e2 to kElasticRhoMax = 1e8)
// at SqpOptions::elastic_ladder_early_exit's default (false, i.e. the ladder
// always spends every rung). From Phase-5 Task 10 that bound is an UPPER
// bound only when the caller has opted into the early exit: an activation
// whose ladder stalls (a rung's solution repeats the previous one) then
// reads FEWER than 6 -- see that option's own note in this file for why it
// defaults off (a real trajectory-shaping effect was measured, not merely a
// cost cut). Their qp_minor_iters/factorizations are folded into the
// aggregate sums above, exactly as a SOC re-solve's are, and for the same
// reason.
//
// restoration_iters counts MAJOR ITERATIONS SPENT INSIDE THE RESTORATION
// PHASE (Task 9) -- i.e. subproblems solved on the FEASIBILITY problem, not
// on the NLP's own linearization. It is exactly the sub-solve's own
// major_iters, summed over every restoration entered (at most one per solve
// today; see sqp_driver.h's RESTORATION PHASE note for the cap and why it
// exists). Zero on every solve that never raised a restoration request, which
// is every clean solve.
//
// THESE MAJORS ARE NOT IN major_iters AND HAVE NO HISTORY ROWS. The two
// counters partition the QP solves a driver spends on models: major_iters
// counts the ones built from the NLP at an outer iterate, restoration_iters
// the ones built from the feasibility wrapper. They share the max_iter budget
// (see SqpOptions). The restoration sub-solve's own qp_minor_iters and
// factorizations ARE folded into this struct's aggregates, exactly as a SOC
// re-solve's and an elastic rung's are and for the same reason -- the work was
// spent -- so those two sums cover the whole solve including restoration,
// while soc_steps/elastic_* likewise absorb whatever the sub-solve did.
//
// A NONZERO VALUE DOES NOT IMPLY A FAILED SOLVE. Restoration is a recovery
// mechanism first: a solve that restores and then converges reports kOptimal
// with restoration_iters > 0, and that is the outcome the phase exists to
// produce. It is the pairing with SqpStatus::kInfeasible that says the
// restoration phase reached its OTHER conclusion.
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
    // EQP refinement work, summed over every QP solve this driver spent --
    // subproblems, SOC re-solves, elastic rungs and the restoration
    // sub-solve alike, exactly like qp_minor_iters/factorizations above. The
    // two fields carry QpCounters' (above) meanings unchanged:
    // border_refine_steps is TOTAL steps including each bordered solve's
    // mandatory first, eqp_refine_steps is EXTRA steps beyond solve_eqp's
    // mandatory first and so is identically 0 -- the flag-gated loop it once
    // counted was deleted as measured-inert on both shipped backends (see
    // QpCounters' own note above and the DISPOSITION section of
    // docs/notes/2026-07-29-eqp-refinement-ab.md). They exist so the refinement
    // A/B could price refinement per solve rather than per subproblem, and the
    // zero one stays as that A/B's standing invariant.
    Index eqp_refine_steps = 0;
    Index border_refine_steps = 0;
    // Rungs of the QP engine's SUSPECT-STALL ESCALATION LADDER
    // (qp_engine.h's section 4b), summed over every QP solve this driver
    // spent -- same aggregation set as the two refinement counters above, and
    // named identically to QpCounters::suspect_escalations (above) so the
    // per-solve and per-driver readings are the same quantity at two scales.
    //
    // ZERO IS THE EXPECTED READING and a nonzero one is a finding, not a
    // statistic: a rung is spent only when a would-be-kOptimal QP exit off a
    // SUSPECT factorization failed the free-block stationarity check, i.e.
    // when the subproblem's linear algebra produced a point that is not a KKT
    // point of anything. Nonzero-but-solved means the engine escalated its
    // way out and the answer stands; a QP that came back kNumericalError with
    // detail::kMaxSuspectEscalations rungs spent exhausted the ladder, and
    // THAT PAIRING IS THE EXHAUSTION DIAGNOSTIC -- it is the only place the
    // driver's caller can see why the subproblem was refused, which is why
    // this rolls up rather than dying at the QP boundary.
    Index suspect_escalations = 0;
    // PHASE-4 TASK 4 FIX ROUND 2. Pardiso phase-11 (symbolic analysis)
    // calls PAID THROUGH THE BORDER/REBUILD PATH (qp_engine.h's
    // rebuild_k0(), same scope as QpCounters::symbolic_analyses's own note),
    // summed over every QP solve this driver spent -- same aggregation set
    // as the two refinement counters and suspect_escalations above, and
    // named identically to QpCounters::symbolic_analyses (above; see that
    // field's own note for what it counts and why). THIS IS NOT A COUNT OF
    // EVERY PHASE-11 CALL THIS SOLVE PAID ACROSS EVERY CODE PATH: the
    // elimination path constructs its own per-solve KktFactor and pays its
    // own symbolic analyses through it, which this field does not see and so
    // under-reports on a solve that uses that path (measured: HS39 reads 1
    // here against 2 real analyze() calls -- Task 4 re-review r2's deferred
    // nit). A driver whose every major keeps a single, unshared
    // BorderState (the ordinary case) should see this stay small (1, or 0 on
    // an immediate-convergence solve) across the WHOLE solve regardless of
    // how many `factorizations` were paid, since qp_engine.h's rebuild_k0()
    // reuses one KktFactor's cached sparsity pattern across same-pattern
    // rebuilds; a large value on an otherwise-ordinary solve is the
    // regression signal Fix Round 2 exists to catch (detaching onto a fresh
    // KktFactor on every value-changing major, discarding that cache).
    Index symbolic_analyses = 0;
    // PHASE-4 TASK 3/4. The RESOLVED warm-start level this solve actually
    // used -- NOT the level a caller merely requested by populating `warm`
    // (sqp_driver.h's WARM-START INGEST note has the resolution rule).
    // Always kCold on the 2-arg solve(model, x0) overload (there is no
    // `warm` object to resolve there). On the 3-arg overload: kCold when
    // `warm.valid` is false, when `warm` is not dimensionally compatible with
    // this model, when any ingested vector is non-finite, when the seeded
    // `lambda_i >= 0` clamp DEGRADED the object (sqp_driver.h's THE SEEDED
    // DUAL CLAMP), or when SqpOptions::start_level caps it there.
    //
    // PHASE-6 TASK 5 REWROTE THE MIDDLE OF THIS LADDER. A hash MISMATCH -- the
    // hash == 0 "no model was seen" sentinel a mesh-transferred or crossover
    // object carries included -- no longer lands on kCold: it lands on
    // **kSeeded**, which takes the object's values (x, duals, activity hint)
    // and refuses its provenance-dependent state (see core/start_level.h's
    // StartLevel note for the exact list). kSeeded is also what a
    // `start_level` ceiling of kSeeded produces from an object that would
    // otherwise have earned kWarm/kHot. Then: kWarm when a structural
    // match was confirmed and the ceiling allows it, but either `warm.hot`
    // was null or the FIRST subproblem's own solve did not actually reuse
    // the cache; kHot (Task 4) exactly when a structural match was
    // confirmed, the ceiling allows it, `warm.hot` was non-null, AND that
    // first subproblem's own QpCounters::k0_reused reads true -- i.e. this
    // field records what WAS OBSERVED TO HAPPEN, not what was merely
    // offered: QpEngine's own reuse-eligibility conditions (a)-(e)
    // (qp_engine.h -- (e), added in Fix Round 1, is the shared object's own
    // generation counter, on top of the four fingerprint conditions (a)-(d)
    // that predate it) are the sole gate on whether an offered hot handle is
    // actually usable, and a mismatch there degrades this field to kWarm
    // silently rather than lying about which level ran. `k0_reused` --
    // rather than inferring reuse from `qp_factorizations == 0` -- is
    // deliberate: that count can read zero for reasons unrelated to reuse
    // (an empty reduced system, or a crossed-bounds exit before the loop
    // ever runs), which `k0_reused` does not confuse with a genuine cache
    // hit (see QpCounters::k0_reused's own note above).
    StartLevel start_level_used = StartLevel::kCold;

    // PHASE-4 TASK 5. Majors solved while globalization.h's FULL-STEP MODE was
    // armed (SqpOptions::warm_full_step, above). It counts the same events
    // major_iters does -- SUBPROBLEMS SOLVED, so a routed QP failure taken
    // under the mode counts exactly as an accepted unit step does -- and is
    // therefore always <= major_iters, with 0 on every cold solve, every solve
    // with the lever off, and every solve whose `warm` did not resolve.
    //
    // IT IS NOT A COUNT OF UNIT STEPS TAKEN. A trial the mode declined to
    // accept (only one thing does that: a NON-FINITE trial point, which
    // FunnelStrategy::judge still rejects under the mode) is counted here too,
    // for the same reason major_iters counts it -- the QP was solved.
    Index full_step_majors = 0;
    // PHASE-4 TASK 5. WATCHDOG RESTORES: how many times the driver ended the
    // full-step mode by restoring the best iterate it had seen under it (by
    // ||KKT||inf) and handing the solve back to funnel globalization there.
    // Bounded by 1 today -- the mode is entered ONCE PER SOLVE, at the first
    // measurable iterate, and nothing re-enters it -- so the useful reading is
    // BINARY: 0 means the mode either never engaged or ran all the way to the
    // convergence test (the outcome it exists to produce), 1 means it was cut
    // short by divergence or a stall.
    //
    // A RESTORE IS COUNTED EVEN WHEN IT MOVES NOTHING. On the stall exit the
    // best iterate can BE the current one (e.g. a subproblem that kept failing
    // at an iterate that never moved), and the restore is then a no-op on x --
    // but the EVENT, the mode giving up, is what this counts, and that
    // happened either way.
    Index watchdog_restores = 0;

    // PHASE-5 TASK 8, the eval-economics carry (docs/notes/2026-07-29-
    // phase-3-close-carries.md, extended by docs/notes/2026-07-30-phase-4-
    // close-carries.md). TL;DR: evals_full is a query that fetched
    // derivatives (grad/Je/Ji, whether or not it also read values first);
    // evals_values is a query that fetched f/cE/cI ONLY and never got
    // upgraded. A NlpModel query costs one of two things: a FULL eval_nlp
    // (f, grad, cE, Je, cI, Ji -- sqp_driver.h's NlpEval) or a VALUES-only
    // eval_nlp_values (f, cE, cI, via NlpModel::eval_values, no
    // derivatives). These two counters partition every model query this
    // solve made between the two -- evals_full + evals_values is the total
    // number of times x (or a probe point) was evaluated against the model
    // at all, ACROSS THE WHOLE SOLVE, folded in from a restoration sub-solve
    // exactly as qp_minor_iters/factorizations are (see solve_impl's
    // RESTORATION PHASE fold) since that work is genuinely spent evaluating
    // the model, not a property of the wrapper's own variables.
    //
    // evals_full COUNTS: the first iterate's evaluation, every ACCEPTED
    // trial (direct or via a promoted SOC correction -- exactly one full
    // eval per acceptance, whichever point it lands on), and a restoration
    // exit's re-evaluation of the main model at the point restoration
    // reached (when that point is finite). evals_values COUNTS: every
    // REJECTED trial's evaluation (including one that went through SOC and
    // was still not promoted), the warm-resolution probe's f/cE/cI fetch
    // (sqp_driver.h's WARM-START INGEST note), and nothing else today.
    //
    // BOTH ARE ZERO ON A SOLVE THAT NEVER MEASURED THE MODEL AT ALL -- there
    // is no such solve today (every solve_impl call evaluates at least the
    // entry point) -- and evals_values is IDENTICALLY ZERO on a solve that
    // never rejected a trial and never ran the warm-resolution probe (e.g.
    // every 2-arg solve() call, and a 3-arg call whose `warm` failed the
    // ceiling short-circuit or the dimension check before ever touching the
    // model). A caller comparing this task's before/after on a rejection-
    // heavy fixture reads evals_full here as the AFTER count and
    // (evals_full + evals_values) as the BEFORE count that same fixture
    // would have paid pre-Task-8, when every one of these queries was a full
    // eval_nlp -- see tests/test_sqp_driver.cpp's fixture built on this
    // reading.
    //
    // WHAT THESE COUNT, AND WHAT THEY DO NOT (Fix Round 1 addition, on
    // review): both fields are incremented from the driver's own
    // control-flow DECISION at each call site -- was this trial's fate an
    // upgrade to full, or not -- never from observing which NlpModel method
    // actually executed. Today the two always agree, because every
    // increment site sits immediately next to the real call it describes
    // (eval_nlp_values/upgrade_to_full/eval_nlp -- sqp_driver.h), so reading
    // the decision is equivalent to observing the dispatch. But a future
    // edit that changes which function a call site invokes WITHOUT also
    // updating that site's counter increment would silently desynchronize
    // the two: `evals_full`/`evals_values` would keep reporting the OLD
    // (correct-at-the-time) classification while the actual model-call
    // pattern had already regressed -- these fields are not a self-verifying
    // trace and must not be read as one. A hand-run mutation confirmed this
    // exactly: reverting sqp_driver.h's rejected-trial call site back to a
    // full eval_nlp, while leaving its surrounding accept/reject counter
    // logic untouched, left `evals_full`/`evals_values` UNCHANGED on every
    // fixture -- the regression was caught only by an independent
    // model-call-count cross-check (a CountingModel decorator counting
    // eval_grad/eval_jac_e/eval_jac_i directly;
    // tests/test_sqp_driver.cpp's SqpDriverEvalEconomics battery pairs
    // exactly this kind of cross-check with every assertion made against
    // these two fields, rather than trusting them alone). A results note or
    // benchmark claim built on `evals_full`/`evals_values` should keep doing
    // the same -- corroborate against an independent call count on at least
    // one fixture -- rather than treating these counters as sufficient
    // regression coverage by themselves.
    Index evals_full = 0;
    Index evals_values = 0;

    // PHASE-6 TASK 1 (controller retry economics). Did THIS solve stop
    // because the caller's own MINOR-ITERATION BUDGET ran out? 0 or 1 --
    // never more, because the budget test is an EXIT: the first time it
    // fires the solve returns. It is therefore a flag with a counter's type,
    // kept as an Index so it SUMS over a ledger exactly like every other
    // field here (a sweep's total is then "how many of my solves I cut
    // short", which is the quantity continuation.h aggregates into
    // ContinuationResult::proposals_abandoned).
    //
    // WHAT IT COUNTS: the 4-argument solve(model, x0, warm, minor_budget)
    // overload was given a POSITIVE budget, this solve reached the top of a
    // major having already spent >= that many qp_minor_iters, and it was NOT
    // converged there -- so it returned SqpStatus::kMaxIter at that iterate
    // instead of building another subproblem. See sqp_driver.h's PROBE
    // BUDGET note for the full contract (it is checked BETWEEN majors, so
    // the minors actually spent are >= the budget and bounded by
    // budget + whatever the crossing major cost, not by the budget itself).
    //
    // WHAT IT DOES NOT COUNT, and each of these is a real, reachable case
    // that a reader must not confuse with an abandonment:
    //   - an ordinary max_iter exhaustion (also kMaxIter, this field 0);
    //   - a kBudgetExhausted exit under SqpOptions::budget_mode, which is a
    //     DIFFERENT budget with a different contract (best-iterate hand-off,
    //     "continue at the same p") -- a probe-budget stop never reports
    //     that status even when budget_mode is on;
    //   - a solve that spent more than the budget and CONVERGED anyway. The
    //     convergence test wins at every pass, so an over-budget solve that
    //     is a KKT point is reported kOptimal with this field 0 and its
    //     answer is kept. Abandonment can only ever cost work that was about
    //     to be spent, never an answer that was already found.
    // A solve given no budget (the default, and every 2-/3-argument
    // solve() call) leaves this identically 0, which is what makes the whole
    // mechanism inert -- byte-identically so -- wherever it is not armed.
    Index probe_budget_stops = 0;

    // PHASE-6 TASK 4 (the crash basis). How many inequality ROWS and how many
    // variable BOUNDS SqpOptions::crash_basis seeded into the FIRST QP
    // subproblem's working set on this solve. Identically 0 with the lever
    // off, on every warm/hot ingest (the seed is cold-only by construction),
    // and on a solve that never builds a subproblem at all.
    //
    // THEY ARE COUNTED SEPARATELY BECAUSE THE TWO HALVES HAVE DIFFERENT
    // CEILINGS, and folding them would hide the one that matters.
    // docs/notes/2026-08-03-identification-stall-study.md Sec. 7.8(a')
    // measures a wide-window walk's variable-bound events at ~29 % of all
    // working-set events and at a 1.00x-1.08x per-bound REPEAT rate (pinned
    // once, released once, never revisited), against 2.7x-4.1x per
    // inequality row -- so a row seeded well can remove churn while a bound
    // seeded well can only ever remove a one-shot transit. A reader pricing
    // this lever must be able to tell which half moved.
    //
    // THEY COUNT SEEDS OFFERED, NOT SEEDS HONOURED. qp_engine.h's
    // ingest_seed_working_set applies its own WINDOW-CONSISTENCY RULE and
    // silently drops any seeded bound that falls outside the first
    // subproblem's trust-region window; a dropped hint is still counted
    // here. The two agree whenever the radius does not cut the seeded bound
    // off, which is every case measured, but the field's claim is about what
    // the DRIVER proposed.
    //
    // FOLDED FROM A RESTORATION SUB-SOLVE, exactly like evals_full/
    // evals_values and the work counters above (sqp_driver.h's fold block
    // says why). That sub-solve inherits this lever and is cold by
    // construction, so a solve that entered restoration with the lever on can
    // report contributions from TWO first-subproblems, not one -- and the
    // feasibility wrapper's slack columns start ON their own lower bound of
    // 0, so the second contribution is typically large. Read a nonzero value
    // as "seeds this solve proposed", never as "seeds the caller's own x0
    // supplied".
    Index crash_seeded_rows = 0;
    Index crash_seeded_bounds = 0;

    // PHASE-6 TASK 5 (the kSeeded ingest level, core/start_level.h's StartLevel).
    //
    // n_seeded: 1 iff THIS solve's warm-start resolution landed on
    // StartLevel::kSeeded, else 0 -- a flag with a counter's type, kept as an
    // Index for exactly the reason `probe_budget_stops` above is ("so it SUMS
    // over a ledger"): a sweep's total is then "how many of my solves ingested
    // values without provenance", which is the quantity a crossover or
    // mesh-refinement loop reports on. It is NOT redundant with
    // `start_level_used`, which answers the same question for ONE solve and
    // cannot be summed, nor with ledger.h's `level_histogram()`, which answers
    // it for a whole ledger but requires one to be attached; this field rides
    // on the SqpSolution every caller already has. It is identically 0 on the
    // 2-arg solve() overload, on every kCold/kWarm/kHot resolution, and on a
    // seeded object that DEGRADED (see below).
    //
    // seeded_clamped: how many `lambda_i` entries the seeded `lambda_i >= 0`
    // enforcement ZEROED on this solve -- sqp_driver.h's THE SEEDED DUAL CLAMP,
    // the closure of O-B1-4. Bounded by model.mi(). Identically 0 at every
    // other level, because the clamp is scoped to kSeeded (that note says why),
    // and identically 0 on a well-formed seed, which is the ordinary case: a
    // nonzero reading means the producer handed over at least one negative
    // price on a row the destination model reports GEOMETRICALLY ACTIVE. Rows
    // that are strictly SLACK never reach the clamp -- the B-1 clear
    // (`THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY`) has already zeroed
    // them, and it runs FIRST by contract -- so this counts only the sign
    // violations that survived a geometric explanation.
    //
    // WHAT NEITHER FIELD COUNTS, stated because it is the one event a reader
    // will look for here: a seeded object DEGRADED TO kCold by a
    // beyond-the-band negative price is not counted by either. It does not need
    // to be, and inventing a third counter for it would duplicate a signal that
    // already exists exactly: with this level in place, a 3-argument solve()
    // whose `warm` is valid, dimensionally compatible and finite can ONLY
    // report `start_level_used == kCold` through that degradation or through
    // SqpOptions::start_level's own ceiling -- both of which the caller can
    // read directly. A degraded solve reports n_seeded == 0 and
    // seeded_clamped == 0 because it ingested nothing at all, which is the
    // truthful reading of a refusal.
    Index n_seeded = 0;
    Index seeded_clamped = 0;

    // PHASE-7 TASK 0 (the crossover activity repair, warm_start.h's ACTIVITY
    // INFERENCE note). How many inequality ROWS the INGESTED activity hint
    // proposed active on this solve -- the size of the working set a kSeeded
    // object handed the first subproblem, and on the crossover route exactly
    // what `from_interior_point` inferred.
    //
    // WRITE-ONLY. Nothing in the driver, the engine or the globalization reads
    // it or branches on it; it exists so a corpus row can TAG a crossover cell
    // with the quality of the hint it was given, which is the one thing
    // separating an "activity-only" start from a cold one and is otherwise
    // invisible in the counters (a hint that was correct and a hint that was
    // empty produce the same `n_seeded == 1`).
    //
    // SCOPED TO kSeeded, and identically 0 at kCold/kWarm/kHot. That is not a
    // statement that a kWarm object carries no hint -- it always does -- but
    // that at kWarm the hint's provenance is confirmed, so its size answers no
    // question a reader has. kSeeded is the only level whose hint arrived
    // without provenance, which is the only level where "how much did it
    // claim?" is a question. A seeded object DEGRADED to kCold reports 0, for
    // the same reason `n_seeded` does: it ingested nothing at all.
    //
    // NAMED FOR ITS MOTIVATING PRODUCER, NOT SCOPED TO IT. A mesh transfer
    // (mesh_transfer.h) or a hand-built object resolving kSeeded is counted
    // identically; `from_interior_point` is simply the producer the counter
    // was added for and the one whose inference rule the count grades.
    //
    // ROWS ONLY -- the bound half of the hint is deliberately not folded in,
    // on `crash_seeded_rows`/`crash_seeded_bounds`'s own argument above (the
    // two halves have different ceilings and folding hides the one that
    // matters). No consumer needs the bound count yet; adding it later is
    // additive.
    //
    // SEEDS OFFERED, NOT SEEDS HONOURED, exactly like the crash-basis pair:
    // qp_engine.h's WINDOW-CONSISTENCY RULE may drop any part of the hint that
    // does not fit the first trust-region window, and a dropped row is still
    // counted here.
    Index ip_activity_inferred = 0;

    // PHASE-7 TASK 3. The semismooth-Newton kernel's work, summed over every
    // subproblem of this solve -- the same aggregation set qp_minor_iters and
    // factorizations above already use, and empty for the same reason they
    // would be if no subproblem were solved.
    //
    // **IDENTICALLY ZERO AS OF TASK 3, ON EVERY SOLVE**: sqp_driver.h does not
    // construct an SsnEngine and does not read SqpOptions::qp_mode (Task 5
    // wires both), so nothing writes here yet. It lands write-only, with the
    // selector it belongs to, so that Task 5 adds a summation rather than a
    // struct.
    SsnCounters ssn;
};

} // namespace hven::solvers
