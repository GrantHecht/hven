#pragma once

// sqp_driver.h — the SQP major loop: the first NLP-solving code in the
// project.
//
//     min   f(x)   s.t.  cE(x) = 0,  cI(x) <= 0,  l <= x <= u      (nlp_model.h)
//
// TASK 6 SCOPE: THE TRUST-REGION MAJOR LOOP. Task 4 built this file as
// FULL-STEP SQP -- every subproblem step accepted unconditionally at a radius
// held fixed for the whole solve -- and recorded, as executable measurements,
// exactly what that fails to do (a Rosenbrock objective that rises by 90x
// mid-solve; a permanent 2-cycle on HS5 at a radius merely twice the default).
// Task 5 built the acceptance test (globalization.h's KLV funnel) in
// isolation. THIS task is the wiring: solve at the current radius, evaluate
// the trial point, let the strategy judge it, move or shrink accordingly.
// That is what makes the method globally convergent, and the two measurements
// above are now the two tests that pin the fix.
//
// TASK 6b added SUBPROBLEM FAILURE ROUTING (see that note): a QP that fails
// but hands back a usable iterate is a rejected step, not an abort.
//
// TASK 7 added the SECOND-ORDER CORRECTION (see that note below): a kReject
// whose constraint violation INCREASED gets one hot-started rescue attempt
// before the radius shrinks.
//
// TASK 8 added the ELASTIC TIER (see that note): a subproblem that returns
// kInfeasible is REFORMULATED with penalized slacks and re-solved, instead of
// ending the solve. This is where KLV Algorithm 5's authoritative "if
// subproblem infeasible" restoration trigger lives.
//
// TASK 9 added the RESTORATION PHASE (see that note) and the RADIUS FLOOR
// (see RADIUS MANAGEMENT). A restoration REQUEST -- from any of the three
// sources that can now raise one -- is no longer an exit: the driver switches
// to minimizing the infeasibility measure h, using this same class on a
// wrapper model, and comes back either with a feasible-enough point to resume
// from or with a CERTIFICATE that the problem is infeasible where it stands.
// That closes the last interim exit in this file.
//
// --- ONE TRIAL ----------------------------------------------------------
//
// At the iterate (x, lambda_e, lambda_i), with radius Delta and a count of
// consecutive rejections already burned HERE:
//
//   1. CONVERGENCE TEST FIRST, on the iterate's own NlpEval (which is the
//      trial evaluation of the step that produced it -- see MODEL EVALUATION
//      below). evaluate_kkt measures the point BEFORE any subproblem is
//      built, so a solve started at a solution returns kOptimal having solved
//      zero QPs (counters.major_iters == 0). This ordering is KLV Algorithm
//      2's own "if ||d|| = 0 then acceptable <- true // KKT point found"
//      short-circuit, which runs AHEAD of the acceptance test; globalization.h
//      says so explicitly and declines to test it, so it must live here. A
//      NON-FINITE iterate is detected here too -- see evaluate_kkt's note on
//      why that check must be explicit rather than left to the residual
//      arithmetic.
//   2. MAX-ITER TEST. opts.max_iter bounds SUBPROBLEMS SOLVED (a rejected
//      trial costs one), i.e. exactly counters.major_iters.
//   3. BUILD the subproblem (build_subproblem below) by linearizing at x --
//      but ONLY if the iterate moved since the last trial. A rejection
//      re-solves the SAME QpProblem object; see RADIUS MANAGEMENT.
//   4. SOLVE it on the driver's single QpEngine at tr_radius = Delta
//      (SolveOverrides), WARM-SEEDED from the previous QpSolution.
//   5. EVALUATE THE TRIAL POINT x + p and judge it: the strategy sees
//      (f, h) old and new, the QP MODEL's predicted decrease, whether the
//      radius was active, and how many rejections this iterate has already
//      cost.
//   6. ACCEPT (kAcceptF/kAcceptH) -> x <- x + p, multipliers <- the
//      subproblem's, rejection count <- 0, Delta possibly GROWS.
//      REJECT -> if opts.enable_soc and the trial's violation INCREASED
//      (h_new > h_old), try ONE second-order correction first (see SECOND-
//      ORDER CORRECTION); that rescue may turn this into an ACCEPT of the
//      CORRECTED point. Otherwise: nothing moves, rejection count++, Delta
//      SHRINKS, go to 4.
//      RESTORE -> the RESTORATION PHASE; see that note.
//      A subproblem that FAILED but returned a usable iterate is routed the
//      same way a kReject is -- see there. SOC is NEVER attempted on that
//      route: there is no certified step to correct (see SECOND-ORDER
//      CORRECTION).
//
// --- RADIUS MANAGEMENT ---------------------------------------------------
//
// PORTED FROM STANDARD TRUST-REGION PRACTICE, not from [KLV]: the paper's
// Algorithm 3 says only "if the trust region is active at d then increase the
// radius" and leaves the constants unstated, so the three below are the
// classical ones -- Conn, Gould & Toint, "Trust-Region Methods" (MPS-SIAM
// Series on Optimization, 2000), Algorithm BTR (Sec. 6.1) and its Table 6.1.1
// of "reasonable" values: a very-successful threshold eta_2 = 0.75, an
// expansion factor gamma_2 = 2 and a contraction factor gamma_1 = 0.5. They
// are named constants below so a future re-derivation has one place to edit.
//
//   GROW: Delta <- min(2*Delta, tr_max), but ONLY when BOTH
//         (i) the radius was ACTIVE at the accepted step (QpSolution::
//             tr_active anywhere -- growing a radius the step did not even
//             reach buys nothing and only weakens the next model), and
//         (ii) the step was STRONG: actual/predicted >= kTrGrowThreshold,
//             with the predicted decrease positive. The ratio is the
//             classical rho, here on f itself and its QP model. On an h-TYPE
//             accepted step (KLV Eq. (12)) f may legitimately RISE, so rho is
//             negative and the radius does not grow: that is deliberate --
//             the objective model demonstrably did not describe that step, so
//             there is no evidence for trusting it further.
//   SHRINK: Delta <- Delta/2 on every kReject, and the SAME subproblem is
//         re-solved. Nothing else about the iterate changes.
//
// THE SHRINK FACTOR IS COUPLED TO globalization.h's kRestoreMinRejections,
// and TASK 9 DISCHARGED THAT COUPLING: that constant is now DERIVED from this
// one (4 = ceil(log2(10)) at a factor of 1/2, so the gate opens only after the
// radius has fallen by more than an order of magnitude), with a second,
// independent argument about the routed/judged evidence mix landing on the
// same number. The derivation lives at the constant, which is where a future
// re-derivation must start; what remains true here is that CHANGING EITHER
// CONSTANT ALONE CHANGES WHAT THE RESTORATION SIGNATURE MEANS.
//
// THE RADIUS FLOOR (Task 9), SqpOptions::tr_min. A shrink that would take
// Delta below the floor does NOT clamp and re-solve -- it RAISES A
// RESTORATION REQUEST. This is KLV Algorithm 4's "alpha < alpha_min" trigger
// in its trust-region form, and globalization.h has recorded all along that it
// is one of the paper's two AUTHORITATIVE restoration entries (the other,
// Algorithm 5's infeasible subproblem, is the elastic tier's exhaustion) and
// that the funnel's own kRestore signature cannot stand in for either. The
// floor is not a safety net for a runaway loop -- max_iter already bounds
// that, and did before this task -- it is a DIAGNOSIS: a radius that small
// means every direction the model can still see has been tried and rejected,
// which is a statement about the point, not about the budget. Reaching it
// used to produce kMaxIter at whatever point the loop happened to be stuck on.
//
// WHAT A SHRINK FROM +inf DOES (Task 9). At tr_init = +inf the first
// rejection lands the radius on tr_max, and every subsequent one halves
// normally. Before this task inf/2 was inf, so the shrink half of the rule was
// a no-op and a solve started at +inf could only reject forever: measured at
// kMaxIter with 59 of 60 trials rejected. tr_max is the landing value rather
// than an arbitrary large constant because it is ALREADY the caller's stated
// ceiling on the radius (the growth rule expands toward it), so a radius that
// has to become finite has exactly one value the option set already says is
// admissible. Note the growth rule is still skipped while Delta is +inf --
// min(inf*2, tr_max) would REDUCE it -- so this is the only way a +inf solve
// acquires a finite radius, and it acquires it only when the method has
// evidence it needs one.
//
// THE eta_1 SHRINK-ON-WEAK-ACCEPT BRANCH IS DELIBERATELY NOT PORTED, and
// Task 9 (which owns the radius rules) confirms the Task-6 decision rather
// than revisiting it. CGT's Algorithm BTR shrinks the radius when an ACCEPTED
// step's ratio rho falls below eta_1 = 0.01; KLV Algorithm 3 has no such
// branch, and adding it here would be worse than redundant:
//   (i) It would contradict the strategy. In BTR the ratio test IS the
//       acceptance test, so eta_1 and the accept threshold are the same
//       instrument read at two levels. Here ACCEPTANCE IS THE FUNNEL'S, and an
//       h-type acceptance (KLV Eq. (12)) legitimately has rho < 0 -- f may
//       RISE on a step that buys feasibility. eta_1 would shrink the radius on
//       exactly the steps KLV's proof relies on, after the strategy vouched
//       for them.
//   (ii) The growth rule's own conjunct already withholds reward from a weak
//       step (kTrGrowThreshold), so the radius does not run away upward on
//       the strength of steps the model described badly; eta_1 would only add
//       an active punishment.
//   (iii) It would be a fourth constant with no source in [KLV], in a file
//       whose radius constants are already ported from a different method.
// The cost of leaving it out is that a sequence of weak-but-accepted steps
// keeps a too-large radius; the funnel's next rejection is what corrects that,
// one halving at a time. No fixture in this file's suite distinguishes the two
// policies, which is stated as scoping rather than as evidence for the choice.
//
// --- MODEL EVALUATION, AND WHAT A REJECTED TRIAL COSTS --------------------
//
// Globalization has to look before it leaps: the funnel judges f and h AT THE
// TRIAL POINT, so every trial costs one model evaluation there. On an
// ACCEPTANCE that evaluation is not thrown away -- it becomes the next
// iterate's NlpEval, and the convergence test at the top of the next trial
// reuses it -- so an accepted step costs exactly what Task 4 cost: one full
// eval_nlp per iterate plus one eval_hess per subproblem built.
//
// PHASE-5 TASK 8 CLOSES THE SPLIT THIS PARAGRAPH USED TO DEFER. A REJECTED
// trial no longer costs a wasted eval_nlp: the funnel's own judge() reads
// only f/h (StepContext, globalization.h), never a gradient or a Jacobian, so
// the trial is evaluated through eval_nlp_values (nlp_model.h's eval_values,
// this header's own values-only companion to eval_nlp) instead, and STAYS
// values-only for the rest of that trial's life if the verdict is a reject
// (or a restore, or an accept that arrived via a promoted SOC correction at
// a DIFFERENT point -- see THE REJECTED-TRIAL FUNNEL EVALUATION below). The
// one case that still needs the derivatives -- a DIRECT acceptance, where
// this same NlpEval becomes the next iterate's `ev` -- upgrades it in place
// (upgrade_to_full) the moment that is known, rather than recomputing f/cE/cI
// a second time. So the two-phase NlpEval this paragraph once said would
// require "every caller of build_subproblem" to thread a new interface
// through does not: build_subproblem is unchanged, only the three call sites
// that judge or hash a point without ever building a subproblem from it
// switched. SqpCounters::evals_full/evals_values is the ledger of which
// happened; see that struct's own note and
// tests/test_sqp_driver.cpp's SqpDriverEvalEconomics battery for the
// measured reduction on a rejection-heavy fixture.
//
// PHASE-5 TASK 0 ADDS THE ONE eval_hess THIS ACCOUNTING DOES NOT COVER, and
// it is deliberately outside build_subproblem's "once per accepted iterate"
// rule: a solve that exits WITHOUT EVER BUILDING A SUBPROBLEM (converged at
// its start point, or spent a zero budget) pays ONE eval_hess in
// make_warm_start, to hash the model's sparsity for the hand-off it emits --
// see that function's THE ZERO-MAJOR PROBE note for why it is paid and what
// it buys. It is the ONLY eval_hess a zero-major solve makes, and a solve
// that built even one subproblem never makes it, so the two cases are
// disjoint and the total is still bounded by max(1, subproblems built).
// The third qp_built == false exit (an unevaluable start point) pays nothing:
// it is not probed at all (THE UNEVALUABLE EXIT).
//
// A rejected trial costs NO eval_hess: the subproblem is not rebuilt. This
// still holds when Task 7's second-order correction is attempted: SOC never
// calls eval_hess, and its rhs-shift construction (build_soc_subproblem)
// reads ONLY quantities already in hand -- see the SECOND-ORDER CORRECTION
// note's own cost accounting for what SOC DOES add.
//
// TASK 7 ADDS ONE MORE MODEL-EVALUATION COST: whenever an SOC re-solve
// itself reaches kOptimal (qs_soc.status -- NOT on every attempt; the
// majority of attempts return kInfeasible and pay nothing further, see the
// SECOND-ORDER CORRECTION note's measured cost table), the driver pays one
// evaluation at the corrected point (to judge it and, on acceptance, to seed
// the next iterate's `ev`). TASK 8 MAKES THIS ONE VALUES-ONLY TOO, for
// exactly the same reason as the plain trial: judging soc_ctx never reads a
// derivative, and most re-solves that do reach kOptimal still have their
// corrected point rejected by the funnel (measured: 6 of 7 in this file's
// own suite -- see the SECOND-ORDER CORRECTION note), so that evaluation
// used to be a wasted FULL eval_nlp and is now a wasted values-only one --
// upgraded to full only on the minority that gets promoted. It is paid
// regardless, because judging the corrected point is the only way to find
// out.
//
// --- CONVERGENCE TEST (THE CONTRACT) ------------------------------------
//
// Write the Lagrangian gradient WITHOUT the bound term,
//
//     gL(x) = grad f(x) + Je(x)^T lambda_e + Ji(x)^T lambda_i,
//
// exactly nlp_model.h's convention. The NLP's bound multiplier z is not an
// independent unknown at this level -- it is whatever gL is at a variable
// sitting on a bound -- so the stationarity measure is the REDUCED (a.k.a.
// projected) form: per variable i, with the geometric activity test
//     at_lower(i) := x(i) - l(i) <= feas_tol,
//     at_upper(i) := u(i) - x(i) <= feas_tol,
//
//     s(i) = 0                 if at_lower(i) and at_upper(i)  (fixed/degenerate)
//     s(i) = max(0, -gL(i))    if at_lower(i) only   (z(i) = gL(i) must be >= 0)
//     s(i) = max(0, +gL(i))    if at_upper(i) only   (z(i) = gL(i) must be <= 0)
//     s(i) = |gL(i)|           if free
//
//     stationarity := max_i s(i).
//
// So the measure is ||gL||inf over the FREE variables, unioned with the
// BOUND-MULTIPLIER SIGN CONSISTENCY residual at the active ones. Equivalently
// it is the classical projected-gradient residual
// ||x - clamp(x - gL, l, u)||inf wherever that quantity does not saturate
// against the box, and unlike that form it does not saturate, which matters
// because this same scalar is what the contraction test reads.
//
// feas_tol IS DELIBERATELY REUSED as the activity tolerance rather than
// introducing a third knob: a variable is "on" a bound exactly when the
// primal feasibility measure could not tell it from being on the bound, so a
// separate tolerance could only ever create a window in which a point is
// simultaneously feasible-at-a-bound and stationary-as-if-free (or the
// reverse). The brief's option set has no such field and none is added.
//
//     feasibility := max( ||cE(x)||inf,
//                         max_j max(0, cI_j(x)),
//                         max_i max(0, l(i) - x(i), x(i) - u(i)) ).
//
// The bound term is NOT redundant, though it is inert on most rows: the
// subproblem's box is l - x .. u - x, so every iterate the driver PRODUCES is
// inside the bounds by construction and the term is zero there up to
// rounding. It is the CALLER-SUPPLIED x0 that can violate a bound -- solve()
// does not clamp it -- and then history[0] reports that violation honestly
// instead of claiming feasibility. (Measured: HS5 started at (10, -9),
// outside its [-1.5,4] x [-3,3] box, records feasibility = 6 on the first
// row; the first step pulls x into the box and it is zero from row 1 on.)
//
// CONVERGED := stationarity <= kkt_tol AND feasibility <= feas_tol.
//
// WHAT IS MEASURED BUT NOT GATED. lambda_i >= 0 and the LINEARIZED
// complementarity are properties of the QpSolution itself -- the engine
// certifies them for the subproblem it solved -- so re-testing them here
// would only re-check the engine. NLP complementarity
// max_j |lambda_i(j) * cI_j(x)| is a different quantity, and it is RECORDED
// in the history but not gated, because it is already implied at the rate the
// step vanishes: the subproblem's own complementarity says
// lambda_i(j) * (Ji p + cI_j)(j) = 0, hence
// |lambda_i(j) cI_j| = |lambda_i(j) (Ji p)(j)| = O(||lambda_i|| ||p||), which
// goes to zero with the step. Gating on it would add a failure mode (a large
// multiplier against a slowly vanishing step) without adding information the
// stationarity/feasibility pair does not already carry.
//
// WHICH KERNEL THAT IDENTITY BELONGS TO, AND WHAT MAKES IT TRUE IN THE OTHER
// ONE (Phase-7 Task 5, fix round 1 -- the note said "the subproblem's own
// complementarity" as though one kernel existed, and Task 5 added a second).
// The identity above is a property of an ACTIVE-SET solve and of nothing else:
// a row outside the working set is simply ABSENT from the KKT system, so its
// price is zero to machine precision, and a row inside it is driven to
// Ai_j p = bi_j. Both halves are exact by construction. Per mode:
//
//   qp_mode == kWalk. The identity holds VERBATIM, as it always has. The walk
//     IS that active-set solve; nothing below this line changes for it.
//
//   qp_mode == kSsn. The semismooth kernel stops when its Fischer-Burmeister
//     residual is under `fb_tol`, which is a DIFFERENT statement: phi(a, b) is
//     approximately min(a, b) up to a bounded factor, so |phi| <= fb_tol
//     certifies min(s_j, lambda_j) = O(fb_tol) and therefore
//
//         s_j * lambda_j  =  min * max  <=  O(fb_tol * ||lambda||inf),
//
//     an additive term that does NOT vanish with the step -- the same shape as
//     THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY's tolerance-scaled bound
//     below, and for the same underlying reason (an absolute tolerance against
//     an unbounded multiplier scale). Measured, before the repair below
//     existed: 6.311e-01 on a row with ||lambda||inf = 1e6 at fb_tol = 1e-6,
//     against a walk reading of 1.0e-04 on the identical fixture.
//
//     WHAT RESTORES IT is not a tolerance but THE TIER-3 STABLE-FACE
//     REFINEMENT: every certifying SSN exit hands its identified face to
//     QpEngine::refine_on_face for ONE EXACT equality-constrained solve, whose
//     answer satisfies the identity by construction because that solve IS the
//     active-set solve this paragraph is written about. On the fixture above
//     the refined arm reads 1.0000e-04 -- the walk's own value to five figures
//     (tests/test_b1_gate.cpp,
//     KSsnMatchesTheWalksComplementarityOnceTheFaceIsRefined).
//
//     THE HONEST RESIDUE, stated rather than elided: the refinement can be
//     REFUSED (a face of deficient rank, a failed inertia gate, or a refined
//     point outside the subproblem's own box / trust region / inactive rows).
//     A subproblem whose refinement was refused is back on the fb_tol bound
//     above, and `SqpCounters::ssn::ssn_refine_refused` is how a reader knows
//     how often that happened. On the 27-problem HS corpus 24 of 27 problems
//     take at least one refinement and 12 of 27 have at least one refusal, and
//     every one of the 27 passes the model-level KKT self-check at 1e-6
//     including complementarity.
//
// THAT ARGUMENT IS SUBPROBLEM-SCOPED, AND IT USED TO BE VACUOUS AT EXACTLY
// ONE PLACE -- see THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY below,
// which REPLACES it there with a geometric bound of the same family as the
// bound-activity treatment above (a tolerance-scaled bound, not this
// step-vanishing one -- that section says so explicitly).
//
// --- THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY (Phase-5 Task 7b) ----
//
// THE DEFECT THIS REPAIRS (finding B-1,
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §1; confirmed
// independently by the Task-7 review on a convex n = 3 problem of its own and
// on tests/sqp/support/parametric_families.h's F6). A warm ingest seeds lambda_e/
// lambda_i FROM A SOLVE OF A DIFFERENT PROBLEM (solve_impl's WARM-START
// INGEST), and the convergence test above runs at the TOP of the major loop --
// before any subproblem of THIS solve exists. The O(||lambda_i|| ||p||)
// argument in the paragraph above is an identity about THE SUBPROBLEM'S OWN
// complementarity, so at that first test there is no subproblem for it to be
// about and it says nothing at all. Consequence, measured: when a previously
// ACTIVE inequality goes STRICTLY SLACK at the new parameter while its stale,
// strictly positive multiplier still zeroes gL at the old point, both gated
// quantities pass and the solve returns kOptimal IN ZERO MAJORS at a point
// that is not a KKT point of the problem it was asked to solve (HS10 warm from
// p = 0 to p = 0.5: stationarity 4.2e-08, feasibility 0, COMPLEMENTARITY
// 0.25, objective 18 % from truth).
//
// THE REPAIR, in one sentence: ON A WARM (or hot) INGEST, AND ONLY THERE, ANY
// INGESTED lambda_i(j) WHOSE ROW IS NOT GEOMETRICALLY ACTIVE AT THE INGESTED x
// IS SET TO ZERO, BEFORE THE FIRST CONVERGENCE TEST READS IT. Exactly:
//
//     row j is geometrically active at x  :=  cI_j(x) >= -feas_tol
//     lambda_i(j) <- 0                     whenever it is not.
//
// This is the SAME TEST, WITH THE SAME TOLERANCE, that the reduced
// stationarity measure above already applies to BOUNDS: at_lower/at_upper
// decide activity BY DISTANCE, and a bound that goes slack under a parameter
// change therefore cannot be masked by a stale z. General inequality rows were
// the ONLY part of the KKT test with no such handling -- lambda_i enters gL
// unconditionally and cI_j's slackness was never tested against it -- so this
// is the structural analogue of working behaviour applied to the one row type
// that lacked it (Task-7 review, I-4), not a new mechanism.
//
// WHAT IT MAKES TRUE, AND EXACTLY WHAT IT DOES NOT. After the clear, the
// ingested (x, lambda_e, lambda_i) satisfies complementarity BY CONSTRUCTION,
// to the same standard the bound term is held to: every row is either strictly
// slack with lambda_i(j) == 0 exactly, or within feas_tol of its boundary, so
//
//     max_j |lambda_i(j) cI_j(x)| <= feas_tol * ||lambda_i||inf.
//
// **THAT IS A TOLERANCE-SCALED BOUND, NOT THE ONE IT REPLACES, and the
// difference is worth naming rather than eliding.** The vacated argument above
// bounded the same quantity by O(||lambda_i|| ||p||), which VANISHES WITH THE
// STEP; this one does not vanish with anything -- it is proportional to
// ||lambda_i||, exactly as the bound term's own sign-consistency residual is.
// Measured, on a fixture whose objective is scaled by s with the row released
// by 0.9 * feas_tol: at ||lambda_i|| = 1e3 a zero-major warm certificate
// carries complementarity 9.0e-4 against kkt_tol = 1e-6 (Task-7b review, probe
// P3). The PRIMAL error stays bounded by feas_tol regardless of that scaling,
// which is the defensible reading and is precisely the standard this header
// already holds the bound term to. So the honest statement is that the
// subproblem-scoped argument was REPLACED by a geometric one of the same
// family as the bounds treatment -- not that the old bound was restored.
//
// ON THE FOUR KKT CONDITIONS, ITEMIZED, BECAUSE AN EARLIER VERSION OF THIS
// PARAGRAPH SAID "COMPLETE" AND THAT WAS FALSE AS WRITTEN (Task-7b review,
// I-1). At the ingested point the driver stands in this position:
//
//   stationarity     GATED (kkt_tol), and it is what the clear un-masks.
//   primal feasib.   GATED (feas_tol).
//   complementarity  NOT gated, and does not need to be: established BY
//                    CONSTRUCTION by the clear, to the bound above.
//   DUAL FEASIBILITY NOT restored by the clear and NOT GATED ANYWHERE.
//     (lambda_i >= 0) evaluate_kkt folds a sign-consistency residual into the
//                    reduced stationarity measure for BOUNDS only; a general
//                    inequality row enters grad_lag unconditionally and is
//                    never sign-tested. **IT IS AN INGEST PRECONDITION, NOT A
//                    GUARANTEE** -- warm_start.h's SIGN CONVENTIONS paragraph
//                    states lambda_i >= 0 as part of what a WarmStart IS, and
//                    everything below leans on the caller honouring it.
//
// THE CONSEQUENCE OF VIOLATING THAT PRECONDITION, stated because a reader who
// hand-assembles a WarmStart deserves to know it. On
// `min x  s.t.  cI1 = x <= 0, cI2 = -x - 1 <= 0` (n = 1, no bounds, true
// solution x = -1, f = -1), a hand-built warm object at x = 0 carrying
// lambda_i = (-1, +0.5) -- a CONTRACT-VIOLATING negative price on the active
// row plus a stale positive price on the strictly slack one -- returns
// kOptimal in ZERO majors at f = 0, a 100 %-wrong answer, BECAUSE the clear
// removed the +0.5 that had been breaking stationarity (Task-7b review, probe
// P2). Three things scope this and none of them excuses it:
//   - the input is out of contract. THE CLOSURE "no in-repo producer emits
//     it" IS FALSE, and an earlier version of this note asserted it (Task-7b
//     review round 2, N-1). What each producer actually does:
//       * from_interior_point (warm_start.h) COPIES THE CALLER'S lambda_i
//         VERBATIM. Its dual_tol sign filter governs only WORKING-SET
//         MEMBERSHIP -- a row with lambda_i(j) <= dual_tol is left out of
//         ineq_active/qp_working_set, and its PRICE is carried into the
//         object unchanged. So a crossover from an interior-point solve that
//         has not fully converged is a LIVE PRODUCER of a negative price, and
//         tests/test_warm_start.cpp's WarmStart.CrossoverWrongSignDualLeaves-
//         RowFree constructs exactly one (lambda_i = -1e-8 against a slack of
//         -1e-9, i.e. GEOMETRICALLY ACTIVE -- so the clear does not touch it).
//       * the predictor's ratio test admits a negative lambda_i only within
//         kDualSignTol * max(1, |lambda|) = 1e-9 relative.
//       * mesh_transfer ingests at kSeeded from Phase-6 Task 5 (through Phase 5
//         it did not ingest at all: structure_hash == 0 resolved kCold), and at
//         that level THE SEEDED DUAL CLAMP gates the sign -- so this route can
//         no longer deliver an ungated negative price either.
//       * every SqpDriver exit is non-negative, though not all for the same
//         reason: an exit that solved a subproblem carries QP-priced duals; a
//         ZERO-MAJOR exit re-emits the ingested duals as cleared by this very
//         block; and a restoration exit carries the sub-solve's own
//         subgradient selectors. None of the three can go negative.
//     So the producible magnitude is bounded by the sign violation a producer
//     can commit -- 1e-9 relative for the predictor (whose output DOES reach
//     the clear via solve(), since predict() carries structure_hash forward),
//     and for the crossover whatever residual sign noise the caller's IP
//     solver hands over (1e-8 in the shipped fixture). **THE CROSSOVER FIGURE
//     WAS A PRODUCIBLE BOUND ONLY THROUGH PHASE 5 AND IS NOW REACHABLE**:
//     `from_interior_point` carries `structure_hash == 0` unconditionally, and
//     through Phase 5 that resolved `kCold` on every solve() so the object
//     never reached the clear at all -- the qualifier this bullet used to carry
//     ("it becomes reachable ONLY ONCE A NON-HASH-GATED INGEST LEVEL EXISTS
//     (Phase 6)") is DISCHARGED by Phase-6 Task 5's StartLevel::kSeeded, which
//     is that level. Reaching the clear is also where it meets THE SEEDED DUAL
//     CLAMP, so the newly reachable route is the one route on which the sign is
//     now checked. Orders below kkt_tol in both cases;
//   - the failure class is NOT NEW and its reachability is UNCHANGED: the
//     identical false certificate is reachable pre-repair from the adjacent
//     input lambda_i = (-1, 0). The hole is the ungated dual-sign condition,
//     which predates this task; the clear widens the set of malformed inputs
//     that land in it;
//   - the magnitude is bounded by the size of the sign violation.
// A DRIVER-SIDE `lambda_i >= 0` VALIDATION AT INGEST closes this outright and
// is cheap. **IT LANDED IN PHASE-6 TASK 5 AS THE SEEDED DUAL CLAMP** (this
// header's own note of that name, above the SqpDriver class), together with the
// level that made the hole reachable in the first place. The two arrived
// together on purpose: through Phase 5 the crossover hand-off was a shipped
// path that COULD PRODUCE a negative price on a geometrically active row --
// precisely the configuration the clear leaves alone -- but could not reach the
// clear (or a validation) via solve() at all, because `from_interior_point`
// carries `structure_hash == 0` unconditionally and the WARM-START INGEST rule
// resolved it to `kCold` first. StartLevel::kSeeded made it reachable; the
// clamp gates it in the same breath. WHAT IS AND IS NOT CLOSED:
//   - AT kSeeded the condition is GATED. A negative price within
//     kSeededDualClampTol is clamped to zero and counted; a larger one degrades
//     the object to kCold. The P2 probe below degrades.
//   - AT kWarm AND kHot it remains an INGEST PRECONDITION, exactly as this note
//     has always said, and deliberately so: those levels are hash-gated, and
//     every producer that can clear a hash gate is non-negative (every
//     SqpDriver exit) or bounded by 1e-9 relative (the predictor). Widening the
//     clamp there would move pinned trajectories for a defect no shipped
//     producer can reach.
// Carried as O-B1-4 in
// docs/notes/2026-07-31-nonconvex-sweep-adjudications.md §7.5, and closed by
// docs/notes/2026-08-04-kseeded-ingest.md.
//
// So: (stationarity, feasibility) is once again a complete test OF THE THREE
// CONDITIONS THE DRIVER OWNS at that point, given a WarmStart that honours its
// own sign convention. Two things follow, and the second is why this option
// was chosen over the other three:
//   (1) THE FALSE CERTIFICATE IS GONE. In the HS10 instance the cleared
//       multiplier stops cancelling grad f, stationarity reads 1.0 against
//       kkt_tol, the solve does not converge at iter 0 and the sweep moves.
//   (2) A CERTIFICATE THAT SURVIVES IS REPAIRED RATHER THAN MERELY ALLOWED.
//       Where clearing the multiplier does NOT move stationarity (a released
//       row whose constraint gradient is ~0 at x), the zero-major kOptimal is
//       CORRECT and the quadruple this driver returns is the self-consistent
//       one: lambda_i(j) = 0 on the slack row is the multiplier the KKT
//       system actually asks for there. A pure GATE (menu option 1) would
//       instead refuse to certify a genuine KKT point and spend majors
//       rediscovering it.
//
// NO NEW TOLERANCE KNOB, deliberately, and the argument is the one this
// header already makes for the bound-activity test: a row is "on" its boundary
// exactly when the primal feasibility measure could not tell it from being on
// the boundary. A separate constant could only create a window in which a row
// is simultaneously feasible-as-if-active and priced-as-if-slack. feas_tol
// does the double duty here for the same reason it does it there.
//
// WHY NOT THE OTHER THREE OPTIONS (the menu is the note's §1.5; the cost of
// this one is priced honestly below, since the precedent above lowers its
// NOVELTY and not its cost):
//   * "Gate the first convergence test on complementarity" needs a tolerance
//     nobody has derived, refuses genuine KKT points (see (2)), and -- the
//     disqualifying part -- "first test" is the wrong scope: a solve whose
//     first trial is REJECTED re-enters the loop at the SAME x with the SAME
//     ingested multipliers, so the hole reopens at iter 1 unless the gate
//     tracks whether a subproblem has re-priced the duals yet. That is a
//     second piece of state answering a question this option does not have to
//     ask.
//   * "Ingest x but not the multipliers" (test at lambda = 0) forfeits every
//     legitimate zero-major hand-off, since gL at a constrained solution is
//     not small at lambda = 0. Phase-5 Task 0 exists to make that hand-off
//     work.
//   * "Require major_iters >= 1" forfeits the same feature outright and
//     certifies nothing extra.
//
// WHAT IT COSTS, stated rather than waved at:
//   * IT CHANGES WarmStart INGEST SEMANTICS, a Phase-4 contract: the ingested
//     duals are no longer used verbatim. warm_start.h says so at the field.
//     A caller that hands back an object it built itself (from_interior_point,
//     a hand-assembled WarmStart) now has its slack-row prices dropped.
//   * IT IS NOT TRAJECTORY-NEUTRAL WHERE IT BINDS. The cleared multiplier also
//     leaves the FIRST subproblem's Lagrangian Hessian, so a warm solve that
//     releases a row takes a different first step than it did before. That is
//     the intended blast radius and it is bounded exactly: the clear is a
//     no-op unless some ingested lambda_i(j) is nonzero on a row that is
//     strictly slack at the ingested x, so COLD solves (lambda = 0), models
//     with mi() == 0, and warm solves whose active set survives the parameter
//     move are all BIT-IDENTICAL to Phase 4. Dropping curvature contributed by
//     a constraint that is not active is the right direction on its own terms.
//   * IT DOES NOT TOUCH THE SEEDED WORKING SET. warm.qp_working_set may still
//     nominate the released row, and that is left alone deliberately: an
//     active-set seed is a GUESS with its own correction mechanism (the engine
//     prices it and pivots it out, and qp_engine.h's WINDOW-CONSISTENCY RULE
//     drops what no longer fits), whereas a multiplier is DATA that the
//     convergence test reads and acts on directly. Re-deriving the seed from
//     geometry is a separate question and is not asked here.
//
// The clear is O(mi) and costs NO model evaluation: it reads the NlpEval the
// loop is about to take at the ingested x anyway.
//
// --- THE INGESTED CERTIFICATE IS GATED ON COMPLEMENTARITY (Phase-6 final
// --- fix wave, W1) -------------------------------------------------------
//
// THE HOLE THE CLEAR DOES NOT CLOSE, and it is a WRONG-ANSWER hole rather than
// a quality one. The clear establishes complementarity BY CONSTRUCTION only to
// the TOLERANCE-SCALED bound stated above,
//
//     max_j |lambda_i(j) cI_j(x)| <= feas_tol * ||lambda_i||inf,
//
// and that section already names the difference from the vanishing bound it
// replaced. **THE RIGHT-HAND SIDE IS UNBOUNDED IN ||lambda_i||**, so an ingest
// whose surviving price is large enough turns "within feas_tol of the boundary"
// into an arbitrarily large complementarity residual -- and, because that
// residual is (to first order) the objective the solve gives up by not moving
// onto the row, into an arbitrarily large OBJECTIVE ERROR under a kOptimal
// certificate. Reproduced (cross-vendor review, 2026-08-05; fixtured as
// tests/test_b1_gate.cpp's B1Gate.AScaledStalePriceIsRefusedAtIngest):
//
//     min -1e12 x + 1/2 x^2   s.t.  cI = x - 5e-7 <= 0,  x in [-1, 1]
//     seed x = 0, lambda_i = 1e12
//
// At that seed cI = -5e-7 >= -feas_tol, so the row is GEOMETRICALLY ACTIVE and
// the clear correctly leaves the price alone; gL = -1e12 + 1e12 = 0 exactly, so
// stationarity is 0; the point is strictly feasible. Both gated quantities pass
// and the solve returns kOptimal IN ZERO MAJORS at f = 0 where the truth is
// f = -5.0e5 -- carrying complementarity 5.0e5, exactly one half of the
// constructive bound feas_tol * ||lambda_i||inf = 1e6. The bound is honoured
// and the answer is still wrong, which is the whole finding: the bound is not
// by itself a certificate.
//
// THE GATE. While the multipliers this solve is standing on are still THE
// INGESTED ONES, the convergence test carries a THIRD conjunct:
//
//     kkt.complementarity <= kkt_tol.
//
// THE TOLERANCE IS kkt_tol, ABSOLUTE, AND THAT IS DERIVED RATHER THAN PICKED.
// max_j |lambda_i(j) cI_j(x)| has the units of the Lagrangian, i.e. of f, and
// to first order it IS the objective improvement available by taking up row j's
// remaining slack at the price the certificate itself quotes. kkt_tol is this
// driver's absolute first-order optimality standard (it gates ||gL||inf, the
// other first-order quantity), so holding the third residual to the same
// absolute standard adds no new user-facing knob and no new constant -- the
// requirement THE SEEDED DUAL CLAMP's derivation states, met here without even
// a compile-time one. **EVERY ||lambda_i||-RELATIVE FORM WAS REJECTED, and the
// reproduction is why**: it sits at 0.5 * feas_tol * ||lambda_i||inf, i.e.
// squarely INSIDE the constructive bound, so any threshold proportional to
// feas_tol * ||lambda_i||inf is blind to it by construction. An IPOPT-style
// capped scaling (threshold kkt_tol * max(1, ||lambda_i||inf / 100)) does catch
// this instance, by a factor of 50 -- a margin the same reproduction defeats by
// moving the slack two orders in, so it buys tolerance of large well-scaled
// duals at the cost of the property being bought. The absolute form rejects
// the reproduction by eleven orders.
//
// THE SCOPE IS "UNTIL A SOLVE OF THIS PROBLEM HAS RE-PRICED THE DUALS", not
// "the first test" and not "every major". Each of the three is a different
// claim and the middle one is the only defensible one:
//   * FIRST TEST ONLY is the scope the repair-menu section above rejects by
//     name, and the objection stands: a solve whose first trial is REJECTED
//     re-enters the loop at the SAME x with the SAME ingested multipliers, so
//     the hole reopens at iter 1. `duals_ingested` below is exactly the "second
//     piece of state" that objection said the gate would need; W1 is the
//     finding that says the state is worth carrying.
//   * EVERY MAJOR would re-litigate the WHAT IS MEASURED BUT NOT GATED
//     argument, which is SOUND once a subproblem exists -- the subproblem's own
//     complementarity gives |lambda_i(j) cI_j| = O(||lambda_i|| ||p||), which
//     vanishes with the step. Gating there adds the failure mode that paragraph
//     names (a large multiplier against a slowly vanishing step) and would move
//     pinned trajectories across the whole corpus for a defect that is not
//     reachable there. **AND IT IS REFUTED BY THE REPRODUCTION ITSELF, not only
//     argued against**: run the same model at S = 1e6 and the point this driver
//     converges to and certifies carries complementarity 1.0e-4 -- the QP lands
//     x a few 1e-10 outside a row priced at 1e6 -- which is 100 * kkt_tol. An
//     every-major gate would refuse the CORRECT answer to the very problem the
//     gate was added for. The fixture asserts that reading rather than leaving
//     it as prose.
//   * SO THE FLAG IS CLEARED WHEREVER A SOLVE OVERWRITES lambda_e/lambda_i:
//     an accepted step, a SOC-corrected step, a restoration RESUME (where the
//     multipliers are zeroed) and a restoration EXIT. The resume's clear is
//     behaviourally inert -- zeroed multipliers make complementarity 0, so the
//     armed conjunct would pass regardless -- and is there so the invariant
//     below is literally true rather than merely harmless (round 2, N4). It
//     is also CARRIED BY THE FULL-STEP WATCHDOG's best-iterate record, because
//     a restore can put the ingested duals back and the gate must come back
//     with them -- without that, a solve could be refused at iter 0 and then
//     certify the identical (x, lambda) at iter k.
//
// COLD SOLVES ARE UNTOUCHED BY CONSTRUCTION: lambda is zero there, so
// complementarity is 0 and `duals_ingested` is false from the start. A benign
// warm/hot hand-off is untouched for the same reason the B-1 clear is -- an
// ingest that certifies has its slack rows zeroed by the clear and its active
// rows at |c_j| ~ 1e-12, so the product is at the level of the previous
// solve's own residual, orders below kkt_tol.
//
// **THE EXPOSURE IS NAMED RATHER THAN WAVED AT, BECAUSE THIS NOTE PRINTS AN
// INSTANCE OF IT FOUR PARAGRAPHS UP** (round 2, N3). The gate DOES refuse a
// zero-major certificate whenever ||lambda_i||inf is much larger than
// kkt_tol / feas_tol and some priced row sits at O(feas_tol) rather than at
// O(1e-12) -- and the S = 1e6 reproduction's own converged point is exactly
// that shape, carrying complementarity 1.0e-4 = 100 * kkt_tol. Re-ingesting
// that solution as a warm start is therefore refused its free certificate and
// pays majors to re-derive it. THE COST IS MAJORS, NEVER ANSWERS, which is the
// trade this gate exists to make: the alternative on that same class of point
// is the false certificate the reproduction demonstrates. The INSIDE case --
// a residual at or below kkt_tol still buying its zero-major hand-off -- is
// pinned by TheComplementarityGateBoundaryIsKktTolOnBothSides, and the corpus
// evidence that no shipped hand-off is in the refused class is the byte-exact
// blast radius below. MEASURED
// BLAST RADIUS: the full suite (476 pre-existing tests, both build types) and
// `bench_scale --self-check` are byte-identical across this change; the only
// behaviour that moves is the reproduction's.
//
// REPORTED BOUND MULTIPLIER. SqpSolution::z is the MODEL-IMPLIED multiplier
// at the returned point -- z(i) = gL(i) at an active bound, 0 at a free
// variable -- and NOT the subproblem's QpSolution::z. Two reasons, and the
// first is disqualifying on its own: qp_problem.h's STATIONARITY CAVEAT says
// the QP's reported z is FORCED TO 0 at a TR-pinned index, so it does not
// satisfy stationarity there and a caller checking
// grad f + Je^T le + Ji^T li - z == 0 against it would see a spurious
// residual exactly when the radius binds. Second, the z above makes the
// returned quadruple (x, lambda_e, lambda_i, z) a self-consistent
// certificate: the stationarity measure this file reports IS the inf-norm of
// gL - z restricted to where that residual is not absorbed by an active
// bound. lambda_e/lambda_i are carried out from the subproblem unchanged, per
// the brief.
//
// --- SUBPROBLEM FAILURE ROUTING (Task 6b) --------------------------------
//
// A subproblem that does not return kOptimal is NOT automatically the end of
// the solve. Task 6 shipped it that way, and that was right for one of the
// three failing statuses and wrong for the other two, because two of them
// come back WITH AN ITERATE:
//
//   kInfeasible      the linearization has no feasible point. Shrinking the
//                    radius only REMOVES candidate points, so a retry at a
//                    smaller radius is guaranteed to fail. TASK 8 OWNS THIS
//                    STATUS ENTIRELY and it no longer reaches the routing
//                    below at all: the subproblem is REFORMULATED
//                    ELASTICALLY and re-solved at the SAME radius, which is
//                    a different question rather than a smaller version of
//                    the same one. See THE ELASTIC TIER. (Task 6b propagated
//                    it immediately to SqpStatus::kInfeasible; that is the
//                    behaviour the tier replaced.)
//   kNumericalError  the engine reached a point it refuses to certify. The
//                    canonical case is qp_engine.h section 4b's certification
//                    branch (an indefinite subproblem at a KKT point whose
//                    reduced Hessian is trustworthily not PSD), which returns
//                    the final iterate with the multipliers cleared.
//   kMaxIter         the engine ran out of minor iterations. Its x is a
//                    partial, uncertified answer.
//
// The last two are properties OF THIS SUBPROBLEM AT THIS RADIUS -- a
// different Delta is a different box, a different active-set walk and, on the
// indefinite path, a different set of second-order-inconsistent vertices to
// get stuck at. So the driver treats such a result exactly as it treats a
// REJECTED step: Delta shrinks by kTrShrinkFactor, rejections_at_iterate++,
// the iterate does not move, the SAME QpProblem is re-solved, and the retry
// is warm-seeded from the failed solve's ACTIVE SET (never its step -- see
// WARM SEEDING). The step itself is discarded and never judged: the
// globalization strategy is not consulted, because there is no certified step
// to judge.
//
// WHAT MAKES A RESULT USABLE is qp_failure_is_retryable below: a finite
// iterate, inside the subproblem's box. See there for why that particular
// test and not a feasibility one.
//
// THE RETRY IS ONE-SHOT, counted by qp_failures_in_a_row and reset by any
// solve that reaches kOptimal. A subproblem that fails AGAIN at the shrunken
// radius propagates:
//     QpStatus::kNumericalError  -> SqpStatus::kNumericalError
//     QpStatus::kMaxIter         -> SqpStatus::kNumericalError
// (map_status still names kInfeasible -> kInfeasible, and it is now
// UNREACHABLE from this path: Task 8's elastic tier consumes every
// kInfeasible before the routing is reached. The arm is kept because the
// mapping is a total function on QpStatus and a status that stops being
// possible here is not a status that stops existing.)
// The second is still the judgement call Task 6 called it -- an unconverged
// subproblem's x is not a certified step and this driver has no mechanism to
// take a partial one safely -- but the routing has changed what it costs:
// the mapping now fires only after a genuine retry at half the radius also
// failed, which is the pathological case it was written for rather than the
// first sign of trouble.
//
// WHY THE MULTIPLIERS SURVIVE THIS PATH. The failed subproblem's lambda_e/
// lambda_i are discarded exactly as a rejected step's are (they price a step
// that was not taken), and on kNumericalError the engine has already zeroed
// them anyway. The ITERATE's multipliers -- the last accepted step's -- are
// untouched, so the retry rebuilds nothing and the Hessian does not move.
//
// WHAT THE FUNNEL SEES, AND THE RULE THAT ACTUALLY GOVERNS IT.
// rejections_at_iterate is incremented on a routed failure, so it counts
// toward globalization.h's kRestoreMinRejections gate (conjunct (e)). That is
// deliberate, and it is the honest reading of what the counter means: the
// radius was shrunk at this iterate without any trial escaping the funnel,
// which is exactly the evidence conjunct (e) stands in for -- and a routed row
// satisfies that description as fully as a judged kReject does. Nothing about
// (e) requires the shrink to have been caused by a JUDGED trial.
//
// WHAT IS NOT TRUE, and an earlier version of this note claimed it was: "at
// most one of the three can come from a QP failure, since the retry is
// one-shot". THE ONE-SHOT BOUND IS PER FAILURE CHAIN, NOT PER ITERATE. The two
// counters have DIFFERENT reset rules --
//     qp_failures_in_a_row  resets on ANY subproblem that reaches kOptimal;
//     rejections_at_iterate resets only when a step is ACCEPTED --
// so an ALTERNATING sequence at one iterate (routed failure, judged kReject,
// routed failure, ...) starts a fresh chain after every successful solve while
// the rejection count keeps climbing. MEASURED (HS1 from its published start
// at tr_init = 12 with opts.qp.max_iter = 2, both algebra modes): accept,
// then kMaxIter-routed at Delta = 12, judged kReject at 6, kMaxIter-routed at
// 3 -- rejections_at_iterate = 3 = kRestoreMinRejections, of which TWO are
// routed QP failures rather than judged trials. So a restoration exit may rest
// on MAJORITY-QP-FAILURE evidence. That is the same mechanism as this header's
// standing note that an alternating fail/succeed sequence is bounded only by
// max_iter; the two are one fact seen from two sides.
// SqpDriverQpFailure.RoutedFailuresCanFillTheRestorationGate pins the
// sequence, and globalization.h says the same thing at both places Task 9 will
// read when it re-derives kRestoreMinRejections.
//
// THESE ARE STILL DISTINCT FROM A REJECTED STEP in the history: a kReject
// verdict means the subproblem SUCCEEDED and its step was not good enough,
// while a routed failure row carries a non-kOptimal qp_status. Both are
// radius events; only the former was judged. SqpIterate::verdict keeps its
// kReject default on a routed row, which is accurate -- nothing was accepted
// and the iterate did not move -- and it is what keeps a consumer
// reconstructing the ITERATE sequence from the history correct across this
// path.
//
// A FIFTH ROUTE IS NOT AN EXIT AT ALL FROM TASK 9: StepVerdict::kRestore ->
// THE RESTORATION PHASE. Three independent sources now raise the request, and
// all three enter the same phase (see THE RESTORATION PHASE for what happens
// next and for the decision table on the way back out):
//   (1) THE FUNNEL'S SIGNATURE (globalization.h's five conjuncts, Task 5),
//       which fires when the trial is funnel-incompatible at an infeasible
//       iterate whose radius has ALREADY been shrunk kRestoreMinRejections
//       times without escaping -- KLV Lemma 5 case 1's configuration. It is
//       a heuristic EARLY signal and can miss.
//   (2) THE ELASTIC TIER'S EXHAUSTION (Task 8) -- the ladder spent with the
//       relaxation still open and nothing left to reduce (THE ELASTIC TIER's
//       four-part signature). KLV Algorithm 5's authoritative trigger.
//   (3) THE RADIUS FLOOR (Task 9) -- a shrink that would take Delta below
//       SqpOptions::tr_min. KLV Algorithm 4's authoritative trigger, in its
//       trust-region form. See RADIUS MANAGEMENT.
// (1) and (2) are told apart on the triggering history row by
// SqpIterate::elastic_applied, exactly as they were when both were exits;
// (3) is told apart by that row's tr_radius sitting at the floor. Nothing
// propagates a raw QP kInfeasible any more (Task 8), so the old
// kReject/kInfeasible shape no longer exists at all.
//
// WHAT THE PHASE INHERITS, AND WHY IT IS WORTH SAYING: by the time any of the
// three fires, every CHEAP answer has already been eliminated -- the radius
// has been shrunk repeatedly (1, 3), elasticity has been tried at penalties up
// to rho_max (2), and SOC has been attempted on every qualifying rejection
// along the way. The restoration phase is the expensive instrument and it is
// asked last, which is exactly the state KLV Algorithm 5 hands to its own.
//
// A FOURTH ROUTE does not come from the subproblem at all: an iterate the
// model cannot be evaluated at (NaN/inf in f, grad f, cE or cI, or in x
// itself) is also SqpStatus::kNumericalError, decided at the top of the
// major before any subproblem is built. It is the one exit that CLEARS the
// multipliers, and it is the one that leaves a qp_solved == false row (see
// SqpCounters' two history shapes). Distinguish the two kNumericalError
// sources by the last history row: qp_solved == true means the subproblem
// failed, false means the iterate did.
//
// A non-finite x0, by contrast, is not a status at all -- it is caller input
// and solve() throws. See there.
//
// NOTHING HERE IS INTERIM ANY MORE. Task 6b closed the kNumericalError/
// kMaxIter half (the shrink-retry above, plus qp_engine.h section 4b's
// POST-PROBE RESTART, which makes the engine's own recovery reachable
// mid-solve so most of these failures never reach the driver at all). Task 8
// closed the kInfeasible half: a linearization can be locally infeasible at a
// point from which the NLP is perfectly solvable, which is not a reason to
// stop, and the elastic tier is the response. TASK 9 CLOSED THE LAST ONE --
// the response to an exhausted elastic tier, to a stalled funnel and to a
// radius at its floor is now one and the same mechanism, and every SqpStatus
// this driver can return is a statement about the PROBLEM or the BUDGET
// rather than about a mechanism that does not exist yet.
//
// --- SECOND-ORDER CORRECTION (Task 7) -------------------------------------
//
// WHY THIS EXISTS: THE MARATOS EFFECT. A pure QP linearization can produce a
// step that is a perfectly good DIRECTION -- second-order sufficient,
// superlinearly convergent -- and yet, at the actual nonlinear point x + p,
// BOTH f and the constraint violation h come out WORSE than at x. This is not
// a modelling bug; it is curvature the linearization cannot see (Je is
// evaluated at x, not along the arc to x + p), and it is the textbook
// obstruction to plain SQP achieving fast local convergence: a globalization
// test that judges (f, h) at the raw trial point rejects a great step for a
// reason that has nothing to do with its quality. The funnel above has no way
// to tell "the model was locally wrong" from "the step was bad" -- both look
// like a kReject with h_new > h_old. THE SPEC NAMES THIS AS THIS ENGINE'S
// DELIBERATE, CHEAP EDGE OVER UNO, WHICH OMITS SOC ENTIRELY: one extra,
// hot-started QP re-solve buys back exactly the class of rejection this
// section handles. "Near-free" describes the DESIGN REGIME this targets --
// a small residual near a solution, where the correction is genuinely cheap
// (see A FAILED SOC RE-SOLVE's measured cost-regime paragraph below for the
// honest split; far from a solution it is a real, full extra QP solve that
// usually fails outright). SqpOptions::enable_soc (default ON) is the A/B
// lever the spec asks be kept.
//
// THE SIGNATURE THAT TRIGGERS IT, tested ONLY on a strategy-judged kReject
// (never on a routed QP failure -- see below): h_new > h_old, i.e. the trial
// point is STRICTLY MORE infeasible than the iterate it was taken from. This
// is necessary-not-sufficient for "curvature, not a bad step" in exactly the
// way KLV Lemma 5's own hypothesis is (see globalization.h's restoration
// signature for the sibling argument) -- it is cheap to test, it is the one
// symptom SOC can actually repair (a constraint-linearization defect), and it
// excludes the common, unrelated case of an f-type Armijo failure at h == 0
// dropping to h_new == h_old == 0, where there is no constraint curvature to
// correct at all.
//
// WHY NOT ON A ROUTED QP FAILURE. sqp_driver.h's SUBPROBLEM FAILURE ROUTING
// discards the QP's returned x entirely -- it is not a certified step, the
// strategy never judged it, and there is no p to correct: correcting a step
// that was never a candidate would manufacture evidence out of noise. SOC is
// therefore gated on `verdict == StepVerdict::kReject`, which by construction
// excludes every routed-failure row (their verdict is never set by judge()).
//
// THE CORRECTION, KLV/Fletcher's classical construction. Let p be the
// rejected step, so x_trial = x + p and cE(x_trial) is known (it was just
// evaluated to build ctx). The ORIGINAL subproblem's linearization enforced
// Je p = -cE(x) (Ae/be built at x -- see build_subproblem), i.e. it predicted
// cE(x_trial) ~= cE(x) + Je p = 0. The SECOND-ORDER RESIDUAL is exactly how
// far that prediction missed:
//
//     r_e = cE(x_trial) - cE(x) - Je p            (0 to first order; nonzero
//                                                   here is the curvature)
//
// The correction re-solves the SAME QP (same H, g, Ae, Ai, bounds, radius --
// build_subproblem is not called again, and neither is eval_hess or any
// Jacobian: the model stays frozen at x) with the rhs shifted to cancel that
// residual:
//
//     be_soc = -cE(x) - r_e = -cE(x_trial) + Je p        (equalities)
//     bi_soc(j) = -cI(x)(j) - r_i(j) = -cI(x_trial)(j) + Ji p (j),
//         for j ACTIVE in the rejected solve's qs.ineq_active only --
//         an inactive row's own linearization was never the thing that
//         failed, and shifting it risks manufacturing a NEW active row the
//         correction was never meant to touch.  (inequalities)
//
// Both cE(x_trial)/cI(x_trial) are already sitting in `ev_trial`, the NlpEval
// the driver computed to judge the ORIGINAL trial (see MODEL EVALUATION --
// Task 8 makes this a VALUES-ONLY eval_nlp_values at this point, but ce/ci
// are exactly what a values-only evaluation still holds) -- so despite the
// formula's shape, CONSTRUCTING THIS RHS SHIFT COSTS NO EXTRA MODEL
// EVALUATION AT ALL: "one eval_ce/eval_ci, not a full NlpEval" is already
// paid for by the judged rejection itself. This is SCOPED TO THE RHS
// CONSTRUCTION ONLY -- the re-solve that follows, and the (values-only,
// since Task 8) evaluation it triggers on success, are a separate, real
// cost; see the MODEL EVALUATION section's own TASK 7 ADDS ONE MORE
// paragraph for the honest accounting of what SOC actually spends.
//
// THE RE-SOLVED QP'S OWN SOLUTION IS THE TOTAL CORRECTED STEP FROM x, NOT AN
// INCREMENT ON TOP OF p -- i.e. `qs_soc.x` below plays EXACTLY the role `p`
// played for the original QP, over the SAME box l - x .. u - x, and the new
// iterate is x + qs_soc.x, parallel to x + p on a plain acceptance. This is
// NOT a re-derivation from scratch: because the ORIGINAL QP satisfies
// Je p = -cE(x) exactly (an equality row is a hard constraint; likewise
// Ji p = bi on rows ACTIVE at the rejected solution), the second-order
// residual above collapses to r_e = cE(x_trial) exactly -- whatever
// violation remains at the trial point IS, in full, the curvature the
// linearization could not see. Substituting, be_soc = -cE(x) - cE(x_trial):
// the re-solved QP's linear prediction is asked to cancel BOTH the
// violation the ORIGINAL QP was built against and the one its own step then
// exposed. Equivalently, in terms of the INCREMENT (qs_soc.x - p): since
// be_soc = Je p - cE(x_trial), that increment alone satisfies
// Je(qs_soc.x - p) = -cE(x_trial), i.e. it is a frozen-Jacobian Newton
// correction FROM the trial point using the Jacobian frozen at x -- so
// x + qs_soc.x = x_trial + (qs_soc.x - p) is that correction applied to the
// REJECTED point, using no new derivative information. Both readings are
// the same arithmetic; the second is why the mechanism repairs exactly the
// discrepancy the frozen linearization introduced, and the first is what
// the code below actually computes.
//
// WARM START, NOT A NEW LINEARIZATION. The re-solve is seeded from the
// REJECTED solve's own QpSolution (its working set, x zeroed -- the WARM
// SEEDING discipline below applies unchanged), on the SAME engine, with
// SolveOverrides::tr_radius UNCHANGED. Per qp_engine.h's HOT-START REUSE
// note, K0 reuse needs H/Ae/Ai byte-identical (true: soc_qp is a copy of qp
// with only be/bi touched -- conditions (a)/(c)) and the effective
// (primal_delta, dual_mu) pair unchanged -- condition (d). THIS RE-SOLVE
// ALWAYS BUILDS DEFAULT SolveOverrides (dual_mu at its own sentinel,
// resolving to opts_.qp.dual_mu -- see the ADAPTIVE DUAL REGULARIZATION
// note's DISABLED paragraph at :2513-2519, which states plainly that this
// re-solve is never wired to the mu schedule in either mode: "they always
// run at the engine's own default mu"). So condition (d) holds exactly when
// the TRIAL being rescued ALSO ran at that same default -- true at iter == 0,
// and true for as long as opts_.adaptive_mu's clamped, quantized mu keeps
// rounding up to kAdaptiveMuMax (the engine's own default dual_mu). Once the
// residual has fallen far enough that the schedule quantizes to a SMALLER
// decade -- the convergence tail, empirically residual below ~2.2e-6 for the
// shipped kappa/decade spacing -- the trial ran at that smaller mu while this
// re-solve's default overrides still resolve to the engine default: a
// DIFFERENT effective pair, breaking condition (d) and forcing a K0
// refactorization regardless of (a)/(c). With adaptive_mu OFF (or never
// reaching the tail), condition (d) holds as originally documented. The
// THIRD condition, (b) the seed working set equal to the immediately-
// preceding solve's exit working set, holds BY CONSTRUCTION here (the SOC
// re-solve is the very next solve() call on this engine after the rejected
// one, seeded from exactly its result) in a way the plain shrink-retry's own
// version of (b) is not guaranteed to (there the NEXT major's seed has to
// survive an intervening judge() call and possibly a different radius). This
// is the same three-condition shape
// SqpDriverTrustRegion.RejectionShrinksAndRetriesHotly already pins for the
// plain shrink-retry -- SocDefeatsMaratos asserts the analogous
// factorization delta (== 0, observed) directly, for the same reason that
// test does, and for the SAME regime (small residual, near a solution);
// THAT FIXTURE RUNS WITH adaptive_mu ON (the project default, unset there),
// so its "0 extra factorizations" claim is protected only by the empirical
// fact that its SOC attempt fires early (trial 1 of ~4), before the schedule
// has any chance to leave the default decade -- NOT by condition (d) holding
// unconditionally with adaptive_mu on.
// THIS IS NOT A CLAIM THAT EVERY SOC RE-SOLVE IS FREE, NOR THAT CONDITION (D)
// ALWAYS HOLDS WITH adaptive_mu ON: border_candidate's own checks (perturbed
// pivots, schur_cap, and -- specific to SOC -- a SUFFICIENTLY LARGE rhs shift
// making the corrected problem's own active set differ from the rejected
// solve's, failing condition (b) after all) can still force a rebuild and
// MEASURABLY DO on large-residual attempts (see the cost-regime paragraph
// below A FAILED SOC RE-SOLVE for the measured split); the convergence-tail
// mu mismatch above is a SECOND, independent way condition (d) itself can
// fail. `HsBattery.AdaptiveMuNeverSharesAMajorWithSocOrElastic` measures
// that the two mechanisms never actually share a major on this project's
// 27-problem battery (the schedule leaves the default only on each solve's
// LAST subproblem, where SOC/elastic no longer fire) -- but, as that test's
// own comment says, THE ABSENCE IS CIRCUMSTANTIAL, NOT STRUCTURAL: true of
// these 27 problems, not a guarantee about the driver in general.
//
// JUDGED AS A FRESH TRIAL, WITH ONE DELIBERATE EXCEPTION. The corrected point
// x + qs_soc.x gets its OWN StepContext -- fresh h_new/f_new from a
// values-only eval_nlp_values there (Task 8; upgraded to a full evaluation
// only if the verdict below promotes it -- see MODEL EVALUATION and THE
// REJECTED-TRIAL FUNNEL EVALUATION's UPGRADE TO FULL sites) -- and its own
// strategy->judge() call, because it is a genuinely different candidate
// point that may legitimately be f-type where the original was h-type or
// vice versa. THE ONE FIELD DELIBERATELY NOT RECOMPUTED is pred_df: the SOC
// StepContext reuses the ORIGINAL QP's pred_df UNCHANGED. This is not an
// oversight -- the SOC literature's own framing is that the correction is a
// CONSTRAINT-RESTORATION step, not a re-optimization: the correction's job is
// to fix where p landed relative to the nonlinear constraints, and the
// MODEL'S promised objective decrease is a property of p (and the QP that
// produced it), not of the correction bolted onto it. Recomputing pred_df
// from qs_soc.x would credit the correction with an objective claim it never
// made, and would let a correction chosen purely to reduce h masquerade as a
// stronger f-type model than the QP actually solved. KLV Eq. (10)'s
// switching condition and Eq. (11)'s Armijo test therefore both see EXACTLY
// the decrease the original, rejected QP promised -- only f_new/h_new differ.
//
// GLOBALIZATION.JUDGE() IS THEREFORE CALLED UP TO TWICE FOR THIS ONE ROW,
// once for the raw trial (which is why it read kReject) and once for the
// corrected one. This is a documented, narrow exception to that header's
// "called once per trial" comment (globalization.h, GlobalizationStrategy::
// judge): it is benign because FunnelStrategy::judge only ever MUTATES state
// on a kAcceptH verdict, and the first call here is by construction a
// kReject, which mutates nothing -- so the second call sees the funnel in
// exactly the state the first one found it, and AT MOST ONE mutating verdict
// is produced for the row, matching every other row's one-mutation-or-none
// budget.
//
// ACCEPT vs REJECT, AND THE ROW THAT RESULTS. If the corrected point's
// verdict is kAcceptF/kAcceptH: this row's OWN verdict is OVERWRITTEN to that
// verdict (it is, after all, what the driver ends up doing with this trial),
// SqpIterate::soc_applied is set on it, and the driver moves to x + qs_soc.x
// exactly as it would move to x + p on a plain acceptance -- multipliers,
// seed and `ev` all come from the SOC re-solve, not the original one. If the
// corrected point is ALSO rejected (or, in principle, reads kRestore -- see
// below), the ORIGINAL row's kReject verdict stands and the driver falls
// through to the ORDINARY shrink: ONE rejection is charged to
// rejections_at_iterate for the pair, not two, because from the funnel's
// hypothesis-accumulation point of view (globalization.h conjunct (e)) this
// was one radius-shrinking event at this iterate, exactly as a routed QP
// failure is one event despite also costing an extra QP solve (Task 6b's
// same reasoning, ported). A kRestore read from the SOC judge() call is
// treated the same as a reject here -- deliberately NOT promoted to an early
// exit, because Task 8/9 own the authoritative restoration triggers and nothing
// about a heuristic correction's own heuristic signal should pre-empt them;
// the ORIGINAL trial's kReject is what the driver acts on.
//
// WHAT COUNTS AS "ONE ATTEMPT". At most one SOC re-solve per REJECTED trial
// -- there is no retry-of-the-correction, and a corrected point that is
// itself rejected does NOT chain into a second correction. counters.soc_steps
// increments on every attempt (success or not); the aggregate
// counters.qp_minor_iters/factorizations gain the SOC re-solve's own cost
// (see SqpCounters' note on why those two sums can now exceed the per-row
// total); the triggering row's OWN qp_status/qp_minor_iters/qp_factorizations
// stay exactly the ORIGINAL QP's, preserving every existing reader's
// assumption that those three fields describe one QpSolution (see
// SqpIterate::step_norm's SOC caveat for the same reasoning applied there).
//
// A FAILED SOC RE-SOLVE (qs_soc.status != kOptimal) is treated exactly like
// a rejected corrected point: no StepContext is even built for it (there is
// no certified corrected step to judge), counters.soc_steps still counts the
// attempt, and the ORIGINAL row's kReject stands.
//
// COUNTERS.SOC_STEPS ALONE DOES NOT TELL THESE TWO "still rejected" OUTCOMES
// APART FROM EACH OTHER OR FROM AN APPLIED ATTEMPT (Phase-4, Task 1): three
// mutually-exclusive counters -- soc_applied, soc_qp_infeasible, soc_rejected
// (sqp_types.h's SqpCounters) -- are incremented at exactly the three places
// above (the accepted-corrected-point branch, the corrected-point-still-
// rejected/kRestore branch and this failed-re-solve branch, respectively),
// so soc_steps == soc_applied + soc_qp_infeasible + soc_rejected on every
// solve and the 79%-kInfeasible measurement above is re-derivable from an
// SqpSolution rather than only from this file's own instrumented run.
//
// THIS IS NOT RARE -- IT IS THE MAJORITY OUTCOME, MEASURED. An earlier draft
// of this note called it "expected to be rare, given the hot start and the
// small rhs perturbation"; that was never measured and is false. Of the 19
// SOC attempts already present on this file's shipped suite before Task 7's
// fix round 1 (i.e. arising incidentally, not from a fixture built to
// exercise SOC), 15 (79%) returned kInfeasible from the re-solve itself --
// qs_soc.status != kOptimal, no corrected point, no eval_nlp paid. The
// rhs perturbation is NOT generally small: it is the FULL second-order
// residual, cE(x_trial) (or cI(x_trial) on active rows), which scales with
// h_new -- and h_new is exactly what triggered SOC in the first place
// (h_new > h_old), so a LARGE violation and a LARGE, box-busting rhs shift
// arrive TOGETHER. Measured on HS7 (tr_init = 10, published start): the
// rejected trial's own h_new is 141 against h_old = 25, and the SOC re-solve
// -- seeking a step that closes a residual of that size within the SAME
// trust-region box the original (already-truncated) step used -- is
// infeasible. On a LATER, larger violation in the same run: h_old = 48,
// h_new = 7160 -- three orders of magnitude beyond anything a linear,
// radius-bounded correction can repair. SOC is cheap to ATTEMPT (see below)
// precisely because it is expected to often fail fast; it is not expected
// to often succeed.
//
// WHAT "NEAR-FREE" ACTUALLY SCOPES TO. The WARM START paragraph's hot-start
// argument is about the RE-SOLVE'S OWN COST when it is attempted, not about
// how often it succeeds -- those are different claims, and only the first
// is a near-free-per-attempt argument. Measured, by regime:
//   SMALL RESIDUAL (the design regime SOC targets -- near a solution, where
//   the Maratos effect actually lives, h_new/h_old both small): 2-3 minor
//   iterations and AT MOST 1 EXTRA factorization, measured across both
//   SocDefeatsMaratos (0 extra, asserted directly, not merely observed) and
//   SocFiresOnACurvedActiveInequalityAndImprovesH's two attempts (0 and 1) --
//   genuinely near-free, as designed, though "0 every time" is NOT
//   guaranteed even in this regime (qp_engine.h's HOT-START REUSE note is
//   explicit that its five conditions are necessary, not sufficient).
//   LARGE RESIDUAL (far from a solution, h_new orders of magnitude beyond
//   h_old -- HS7's published-start trajectory and SecondOrderInfeasibleModel
//   are both this regime): re-solves that DO reach kOptimal there still cost
//   3-5 minor iterations and 1-3 EXTRA factorizations (measured on this
//   file's own suite) -- a REAL, FULL extra QP solve, not a free lookup;
//   condition (b) of qp_engine.h's HOT-START REUSE note (seed working set ==
//   exit working set) is far more likely to fail when the rhs shift is this
//   large, because the corrected problem's own active set can differ from
//   the rejected solve's.
// So "near-free" is a property of the DESIGN REGIME (small residual, close
// to a solution), not a universal one -- and Task 11's battery should read
// this note when it decides whether a magnitude gate is worth adding (see
// the named candidate below).
//
// A NAMED CANDIDATE FOR TASK 11, NOT ADOPTED HERE: gate the SOC ATTEMPT
// itself (not just its success) on the residual's magnitude relative to the
// current violation, e.g. h_raw <= kappa_soc * h_old for some kappa_soc > 1
// -- filterSQP (Fletcher & Leyffer) uses exactly this kind of magnitude test
// before committing to a second-order correction, rather than attempting on
// sign alone. NOT ADOPTED in this task because (a) every attempt this file
// measured is CHEAP even when it fails (a handful of minor iterations, at
// most 3 extra factorizations, bounded at one per rejected trial -- see
// WHAT COUNTS AS "ONE ATTEMPT" above), so the cost of NOT gating is small,
// and (b) no evidence in this suite shows a gate would change any outcome
// (nothing in the large-residual regime was ever going to be rescued
// regardless). Recorded here as a named, citable candidate for Task 11's own
// battery to justify or reject with its own measurements, not decided by
// fiat now.
//
// --- THE ELASTIC TIER (Task 8) --------------------------------------------
//
// WHY THIS EXISTS. A linearization can be INCONSISTENT at a point from which
// the NLP is perfectly solvable. That is not a pathology, it is ordinary
// nonlinear geometry: two constraints whose gradients are antiparallel AT x
// impose contradictory linear rows there even though the true feasible set
// between them is wide open a step away, because the curvature the
// linearization drops is exactly what opens it. Task 6b propagated such a
// subproblem straight out as SqpStatus::kInfeasible, which reports a fact
// about the MODEL AT ONE POINT as if it were a fact about the PROBLEM. This
// tier replaces that: the same subproblem is re-solved with the offending
// rows RELAXED at a price, the resulting step goes through the ordinary
// funnel judgment, and only an EXHAUSTED relaxation ends the solve.
//
// This is also KLV Algorithm 5's authoritative restoration trigger -- "if
// subproblem infeasible" -- and globalization.h has said all along that the
// funnel's own kRestore signature is ADDITIVE and cannot stand in for it.
// The trigger now lives here, which is what that note asked for.
//
// THE CONSTRUCTION (build_elastic_subproblem, which has the per-block
// details; this is the shape). For the subproblem at x,
//
//     min  g^T p + 1/2 p^T H p   s.t.  Ae p = be,  Ai p <= bi,  p in box
//
// let p_ref = clamp(0, box) -- which is p = 0 for every iterate the driver
// produces, since the box is l - x .. u - x. Relax exactly the rows VIOLATED
// at p_ref, one slack each, and price them:
//
//     min  g^T p + 1/2 p^T H p + rho * sum_j sigma_j s_j
//     s.t. Ae p + Sigma_E s_E = be,   Ai p - Sigma_I s_I <= bi,
//          0 <= s_j <= violation_l1 / sigma_j,   p in box
//
// with sigma_j = max(1, |residual_j|) the row's own COLUMN SCALE and
// Sigma = diag(+/- sigma_j) (signed by the residual on an equality, negative
// on an inequality). The slack VARIABLE is therefore the row's violation
// measured in units of sigma_j, and rho * sigma_j * s_j IS rho times the
// actual violation -- see FINITE, SCALED SLACKS below for why the variable
// cannot simply BE the violation. NO ENGINE CHANGE IS INVOLVED: this is a
// plain QpProblem with n + ns variables, the same me and mi (slacks add
// COLUMNS, not rows), and H extended by an all-zero block. The engine solves
// it exactly as it solves any other QP.
//
// FEASIBLE BY CONSTRUCTION -- the property everything else rests on. The
// witness point (p, s) = (p_ref, |residual_j| / sigma_j) satisfies every
// augmented row: a relaxed row is absorbed by its own slack BY THE DEFINITION
// of the slack's coefficient, an unrelaxed row is satisfied at p_ref BY THE
// DEFINITION of "not violated there", and the witness is inside the slack box
// because |r_j| <= sum_k |r_k| = violation_l1.
//
// SO THE ELASTIC QP HAS NO EMPTY-FEASIBLE-SET FAILURE MODE. That is NOT the
// same as "the elastic solve cannot fail", and an earlier version of this
// note said the stronger thing. The engine can still decline a feasible
// problem -- and did, on exactly one measured class, which is what FINITE,
// SCALED SLACKS below exists to keep out of reach. The honest statement is:
// feasible by construction, AND scaled and bounded so that its solution stays
// inside the envelope the engine will certify.
//
// AND CONVERSELY: IF THE SLACKS COME BACK ZERO, THE ORIGINAL QP WAS FEASIBLE.
// The elastic solution then satisfies every original row, so the kInfeasible
// that triggered the tier was FALSE. That is not a hypothetical -- it is the
// PHASE-2 CARRY-FORWARD, the promised second detection layer:
//
//     THE RIDE-LANDING FALSE-kInfeasible. Phase 2's ledger records a rare
//     class in which the engine's ride can land somewhere that reads
//     infeasible when the QP is not. Such a verdict now costs a second solve
//     instead of the whole NLP solve. The retry is structurally different
//     from a plain re-solve in the one way that matters: it is a problem with
//     a KNOWN feasible point, so the engine's feasibility phase cannot come
//     back empty-handed for the same reason twice. And the second half --
//     that the retry recovers the ORIGINAL subproblem's own answer rather
//     than some relaxed compromise -- is the l1 EXACT-PENALTY property: for
//     rho above the unrelaxed QP's own multiplier norm, paying the penalty is
//     strictly worse than satisfying the row, so s = 0 and the elastic
//     solution IS the unrelaxed solution. THAT is what the rho ladder is
//     searching for. Both halves are pinned by
//     SqpDriverElastic.ElasticReformulationRecoversAFeasibleQpsSolution,
//     which straddles the threshold on a hand-derived fixture (rho = 0.1 <
//     |lambda*| = 0.5 slides to a relaxed answer; rho = 100 recovers the
//     exact one). NO DRIVER-LEVEL FIXTURE REACHES THE ENGINE STATE ITSELF --
//     the class is rare by construction and this task did not manufacture an
//     engine mutation to force it -- so the coverage is structural, at the
//     level where the mechanism actually lives, and is stated as such rather
//     than implied by an end-to-end test that does not exist.
//
// FINITE, SCALED SLACKS -- THE TIER'S ONE ARITHMETIC CEILING, AND WHY THE
// SLACK IS NOT THE VIOLATION ITSELF (fix round 1). qp_engine.h's is_runaway
// guard reports kNumericalError when a FREE variable exceeds
// detail::unbounded_artifact_scale (kUnboundedArtifactFactor / primal_delta =
// 1e7 at the defaults) toward a bound that could not have restrained it --
// "the answer is the regularization talking rather than an optimum" (section
// 5's own words). A raw slack is exactly such a variable: its natural size is
// the LINEARIZED VIOLATION, which scales with the problem's constraint scale
// and has nothing to do with the regularization. So the first version of this
// tier had a second failure mode that this note claimed impossible -- on a
// FEASIBLE NLP whose rows are merely scaled up, the ELASTIC solve came back
// kNumericalError and the tier reported "exhausted" on a subproblem it had
// never been allowed to try.
//
// MEASURED, on SqpDriverElastic's own fixture with both cI rows multiplied by
// S (which changes neither the feasible set nor the solution):
//
//     S      before                                  after
//     1e6    kOptimal, 1 activation, 6 escalations   same
//     1e7    kOptimal, 1 activation, 6 escalations   same
//     1e8    kInfeasible, 0 escalations, elastic     kOptimal, 1 activation,
//            solve = kNumericalError at s = 5e7      6 escalations
//     1e9    kInfeasible, as above                   kOptimal, as above
//
// THE FIX IS A CHANGE OF UNITS, not a bound. Scaling the slack COLUMN by
// sigma_j = max(1, |r_j|) (and its penalty entry to rho*sigma_j, so the
// objective still prices the actual violation and the exact-penalty threshold
// is unmoved) makes the witness sit at s_j <= 1 AT ANY CONSTRAINT SCALE, so
// the slack is never a runaway candidate for a reason that has nothing to do
// with the problem. max(1, .) rather than |r_j| because a column must never be
// scaled UP: a residual below 1 would shrink the column entry and inflate the
// variable, trading this ceiling for the mirror-image one.
//
// WHY A FINITE CEILING ALONE IS NOT THE FIX, and WHY THE SCALING ACTUALLY
// WORKS. The first remedy tried in fix round 1 was a pure ceiling
// (upper = violation_l1, no scaling), and it does not repair S = 1e8: the
// solve still returns kNumericalError with both slacks FREE at 5e7, strictly
// inside a ceiling of 1e8. ROUND 2 CORRECTED THE EXPLANATION OF WHY, and the
// correction is the part worth carrying, because it says when this failure
// class recurs. The round-1 story was that the optimum genuinely sits in the
// interior of the face s1 + s2 = S, so no ceiling could pin it. THAT IS
// FALSE. The optimum is an ENDPOINT at every scale: on that face the penalty
// term is the constant rho*S, so the objective reduces to p1 + p1^2, minimized
// at p1 = -0.5, giving s = (0, S) -- hand-derivable, and the bound-only
// construction DOES return exactly that at small S. What changes with S is not
// the optimum but the engine's ability to FIND it. Measured, bound-only
// construction, swept through the real engine:
//
//     S     p1 returned    s = (s1, s2)        exit
//     1e1   -0.5           (0, 1e1)            kOptimal, s2 kAtUpper
//     1e2   -0.5           (5e-7, 1e2)         kOptimal
//     1e3   -0.49995       (0.049, 999.95)     kOptimal
//     1e4   -0.375         (1250, 8750)        kOptimal
//     1e5   -0.0099        (49015, 50985)      kOptimal
//     1e6   -1.0e-4        (4.999e5, 5.001e5)  kOptimal
//     1e7   -1.0e-6        (4.99999e6, 5e6)    kOptimal
//     1e8   -1.0e-8        (5e7, 5e7)          kNumericalError (is_runaway)
//
// A CONTINUOUS DRIFT off the endpoint toward the face's midpoint, not a jump
// -- the signature of information being lost in the linear solve rather than
// of a different geometry. The row is (S, -1): the constraint and penalty
// magnitudes are O(rho*S) = O(1e10) while the objective differential that
// decides WHERE on the face to stop is O(1) (exactly 0.25 between p1 = -0.5
// and p1 = 0). Once that ratio passes double precision's reach, the tie-break
// is gone and the corrupted solve reports the symmetric midpoint -- which is
// free, and above the runaway limit. Note the sum s1 + s2 = S is exact at
// EVERY row of the table: the FACE is resolved perfectly, only the POSITION
// on it is not.
//
// The sigma scaling fixes exactly that, by replacing the row (S, -1) with
// (S, -S/2) -- entries of comparable magnitude. The same sweep, scaled:
// p1 = -0.5 and s = (0, S) at every S from 1e1 to 1e8, kOptimal throughout,
// i.e. the hand-KKT endpoint recovered at every scale. SO THE MECHANISM IS
// CONDITIONING, and the runaway exit was the SYMPTOM: repairing the
// conditioning removes the artifact, and no bound was ever going to, because
// there was nothing wrong with where the optimum is.
//
// THE FINITE CEILING IS KEPT ALONGSIDE THE SCALING, at violation_l1 in actual
// units, but on its own merits rather than as the fix: it says the relaxation
// may not leave the linearization MORE violated in total than it already is at
// p_ref, it cannot cut off the witness, and a slack that saturates it is
// reported kAtUpper -- which is_runaway skips outright ("pinned at a bound: it
// did not run away"). On this fixture the scaled optimum DOES saturate it,
// which is belt and braces rather than the load-bearing part.
//
// WHEN TO SUSPECT THIS CLASS AGAIN: a constructed subproblem whose rows mix
// magnitudes by more than a few orders (here the slack column against the
// Jacobian row), especially where a flat face's position is decided by a much
// smaller objective term. The remedy is to scale the constructed column to the
// row it joins -- not to bound the variable, and not to loosen a guard.
//
// WHAT REMAINS UNCOVERED, stated rather than implied: a subproblem whose
// elastic optimum wants a violation more than ~1e7 times the one it starts
// with still trips the guard. That is genuinely artifact territory, and the
// engine's own advice for a caller who disagrees is to tune primal_delta
// (section 5), which SolveOverrides::primal_delta makes reachable per solve.
//
// THE TRUST REGION IS FOLDED INTO THE BOX, NOT PASSED AS SolveOverrides::
// tr_radius. This is a correctness requirement and it is the single least
// obvious thing in this tier. qp_engine.h's section 6 applies the radius to
// EVERY variable index of the problem it is given -- so a radius of Delta
// would also cap every slack at Delta, and the slack a violated row needs is
// the size of the VIOLATION, which has nothing to do with the radius. The
// elastic QP would come back infeasible and the tier would be a no-op with
// extra steps. So build_elastic_subproblem computes section 6's own window
// (about p_ref, which IS the engine's center for a zeroed seed) and applies
// it to the ORIGINAL block only, and the solve passes the default +inf
// sentinel. MEASURED (mutation, fix table in the report): passing the radius
// as an override instead makes InconsistentBoundedModel's very first elastic
// QP infeasible -- 1 activation, 0 escalations, 0 steps, dead at the first
// trial. The radius BIT is then re-derived in elastic_project, so
// SqpIterate::tr_binding and the radius growth rule behave identically across
// the two paths.
//
// ONE CONFIGURATION IS NOT COVERED, and it is named rather than papered over:
// an engine constructed with a FINITE QpOptions::tr_radius. The window folded
// above is min(delta, opts_.qp.tr_radius), so the original block is right,
// but the engine will then ALSO apply its own radius to the slacks (the +inf
// sentinel means "use opts_.tr_radius", and SolveOverrides has no way to say
// "+inf, overriding a finite default" -- qp_types.h's own single-sentinel
// tradeoff). In that configuration a large violation can leave the elastic QP
// infeasible and the tier degrades to Task 6b's behaviour. It is a
// configuration nothing else in the driver needs: the driver passes its own
// radius per solve through SolveOverrides on every other path, so a finite
// opts.qp.tr_radius is redundant there and is only ever read when
// tr_init == +inf.
//
// THE rho LADDER, and what "still nonzero" is allowed to mean. rho starts at
// kElasticRhoInit and is multiplied by kElasticRhoFactor while ANY relaxed
// row's VIOLATION (sigma_j * s_j, not the scaled variable -- feas_tol is a
// tolerance on constraint violation) is materially nonzero and the budget
// lasts, i.e. up to kElasticRhoMax -- six escalations, at most seven solves
// per activation.
//
// THE LADDER IS HOT-STARTED RUNG TO RUNG (fix round 1). Only the slack block
// of g changes, so qp_engine.h's HOT-START REUSE conditions (a)/(c) (H/Ae/Ai
// values and structure) and (d) (the effective regularization pair) hold
// across the whole ladder by construction -- g is not in that key. Condition
// (b), the seed working set equalling the IMMEDIATELY PRECEDING solve's exit
// working set, is the one the driver has to earn, and it earns it by CHAINING
// the seed: each rung is seeded from the previous rung's solution with x
// zeroed, not from the original kInfeasible solve. Re-seeding from the
// original fails (b) from rung 2 on. MEASURED on
// SqpDriverElastic.InconsistentLinearizationRecovers in border mode: the
// ladder's own share of the factorization count went 7 -> 1 (whole-solve
// total 9 -> 3), minor iterations 29 -> 27.
//
// TWO NORMS APPEAR IN THE SLACK TESTS, and the difference is deliberate but
// small enough to be worth stating. The ladder's stopping test is on the MAX
// violation ("is any row still materially open"); the usability test's
// `closed` arm below is on the L1 sum ("is the whole relaxation shut"), which
// is the norm violation_l1 is measured in and therefore the only one the
// comparison against it can use. Since max <= l1, the ladder can stop with
// max <= feas_tol while l1 slightly exceeds it (many rows, each just under
// tolerance), in which case `closed` is false and the step is judged on the
// `reduced` arm instead -- strictly the more conservative branch, and one
// that also holds there. No configuration in this file's suite distinguishes
// them.
//
// NOR DOES ANY FIXTURE DISTINGUISH THE LADDER'S CHOICE OF UNITS, stated
// rather than left implied: mutating the ladder's stopping test to read the
// SCALED variable instead of the violation leaves the whole suite green,
// because every relaxed row here has sigma_j >= 1 and a violation orders of
// magnitude above feas_tol at every rung, so the ladder runs to its end
// either way. The violation is nonetheless what the test MEANS -- feas_tol is
// a tolerance on constraint violation, not on an internal change of units --
// and the usability comparison against violation_l1 has no choice about it
// (that mutation does fail, on two tests).
//
// THE STALL EARLY-EXIT (Phase-5 Task 10, SqpOptions::elastic_ladder_early_exit,
// default FALSE -- an opt-in, not an A/B-style "on by default" lever; see
// that field's own note in sqp_types.h for the full argument, this is the
// mechanism side of it). Phase 3 measured the ladder never terminating
// early -- every activation across its own battery spent all six
// escalations -- and named the obvious-looking fix (stop the moment a rung
// changes nothing) but left it out, "to preserve hand-derivable escalation
// counts" (docs/notes/2026-07-29-phase-3-close-carries.md). Phase-5 Task 7b's
// nonconvex sweep corpus is the fixture that carry was waiting for: ALL 104
// elastic activations there ran the ladder to kElasticRhoMax, ratio EXACTLY
// 6.0 escalations/activation, no exceptions
// (docs/notes/2026-07-31-nonconvex-sweep-adjudications.md).
//
// WHAT THE EXIT ACTUALLY TESTS, stated precisely after fix round 1's review
// (I-2): kElasticStallScale is a NUMERICAL-ZERO threshold on two consecutive
// rungs' solutions -- it is NOT a test for an exact repeat, and the ladder's
// rungs are not, in general, bit-for-bit identical even on the SAFE class
// below. MEASURED on SqpDriverElastic.InconsistentLinearizationRecovers
// (the flagship safe fixture): the solution drifts a little every rung
// (|dx|inf 3.6e-13 at rho 1e2->1e3, growing roughly 10x per escalation to
// 3.6e-8 by rho 1e7->1e8), and the WORKING SET itself changes at rung 2 (one
// slack column flips from a real bound to free) -- so at the very rung the
// exit compares, rho genuinely is read by part of the reduced system. What
// makes the exit SAFE here is not that rungs repeat exactly; it is that the
// FIRST escalation's drift (3.6e-13) already falls under
// kElasticStallScale's 1e-12 threshold, and the drift the full ladder would
// go on to accumulate by rho = kElasticRhoMax (3.6e-8) stays four orders of
// magnitude below feas_tol's default 1e-6 regardless. Only g's slack block
// changes between rungs (set_elastic_penalty); when most of the relaxed
// slack columns are pinned at a REAL BOUND (SqpDriverElastic.
// InconsistentLinearizationRecovers's own worked example: the two slack
// columns satisfy s1 + s2 >= 1 identically, an antiparallel pair whose
// hand-derivation says the optimum sits at the SAME corner "for every
// rho >= 1"), that bounds how much of the reduced system CAN read rho at
// all, which is why the residual drift stays small rather than why it is
// zero. MEASURED on that fixture and on SqpDriverElastic.
// RhoEscalationIsBoundedAndSignals/ElasticSurvivesALargeConstraintScale (the
// same bound-pinned class): the escalation count drops 6 -> 1 (12 -> 2
// across two activations, and S-dependently on the third -- S = 1e6 needs 2
// escalations, not 1) with the certified outcome unchanged to noise
// (docs/notes/2026-08-01-tuning-elastic-tau.md has the exact readings).
//
// WHY IT IS NOT SAFE IN GENERAL, and why this is opt-in rather than the
// default. THE MECHANISM, corrected by fix round 1's review (I-1): this
// task's original text said a later rho "relocates to a different point of
// [a] face", implying later rungs find genuine further progress a
// one-repeat test misses. Measured (the review's own derivation): on
// tests/test_sqp_restoration.cpp's InfeasibleCircleLineModel at x = (1, 1),
// Delta = 1 -- whose Lagrangian Hessian is IDENTICALLY ZERO while its
// elastic relaxation is open (docs/notes/2026-07-31-schur-cap-policy.md
// section 4 derives why) -- the UNRELAXED row forces p0 + p1 = 0, on which
// set the RELAXED row's residual is identically 1 (s = 1 everywhere
// feasible) and the original gradient's contribution g.p = p0 + p1 is
// identically 0. So the augmented objective is EXACTLY CONSTANT, = rho, on
// the ENTIRE feasible set, at every rho -- no point on it is ever preferred
// over any other, at any rho. Rungs 0-4 (rho = 1e2..1e6) return the same
// point to several ULP; rung 5 (rho = 1e7) moves to a different point on
// the same set (MEASURED: 1-2 ULP disagreement even among rungs 0-4, so
// "bit-for-bit" is imprecise there too). That is not a later rung finding
// something rungs 0-4 missed -- NOTHING on this set is more optimal than
// anything else at any rho -- it is the flat objective's O(1) tie-break
// getting lost against rho's growing scale in the engine's own arithmetic,
// the SAME mechanism this file's ElasticSurvivesALargeConstraintScale
// banner already documents for the scale knob S rather than the rho knob
// ("past S ~ 1e3 that tie-break is lost in the linear solve and the
// returned point DRIFTS continuously"). So a caller who stopped after rung 1
// here was not wrong that nothing more was to be found -- nothing more
// EXISTED to find, at any rho -- but they still ended up at an ARBITRARY
// point of a face other rungs would have handed them a different arbitrary
// point of, and that is what is not stable under this lever: WHICH tie-break
// the driver acts on, not whether the ladder still had progress left. The
// effect is not confined to this one hand-built fixture: turning the lever
// on for tests/sqp/support/hs_sweeps.h's HS15 (cold arm) -- built from an
// unrelated published problem, not designed to exercise this -- reshapes the
// whole solve's trajectory (majors 86 -> 63, elastic_activations 48 -> 27),
// because an early tie-break returning a different point propagates into
// different subproblems for the rest of the run. Every configuration this
// task measured on both fixture classes still reached a CORRECT final
// answer -- and every measured trajectory change was in the IMPROVING
// direction (HS15 86->63 majors; InfeasibleCircleLineModel 60->2 majors to
// the same certificate; HS10, the corpus's OTHER elastic-active problem, is
// a free 6x escalation cut with identical majors and objective -- see
// docs/notes/2026-08-01-tuning-elastic-tau.md for the full corpus split) --
// so the case for off is "unpredictable which arbitrary optimum you get",
// not "measured harm" anywhere. That is nonetheless not the "cost only,
// ever" property an on-by-default lever needs, so this one ships off by
// default. Flipping the default would also not be a re-pin exercise:
// SqpDriverRadius.FloorRaisesTheRestorationRequest exists to exercise the
// RADIUS FLOOR trigger specifically, and with the lever on the SAME fixture
// instead certifies through the elastic tier's own exhaustion signature in
// 2 majors (measured: 60 -> 2) -- that test would need redesigning to keep
// testing what it names, not merely new numbers.
//
// A FUTURE task that wants this on by default would need a cheap, general,
// RUNTIME-CHECKABLE way to tell the flat-objective class apart from the
// bound-pinned one. QpSolution::bound_state on the slack columns does NOT
// do this, corrected by fix round 1's review (I-6): at the rung the decision
// is made, BOTH fixtures have a free slack column (InconsistentLinearizationModel's
// own working set at that rung is [upper, free, free, upper]; InfeasibleCircleLineModel's
// single slack column is free throughout) -- "any slack pinned" admits both
// classes and "all slacks pinned" would refuse the safe one too, so neither
// rule separates them at the point that matters. A signal that DOES appear to
// separate these two fixtures is the Hessian block itself (the flat-objective
// fixture's is measured identically zero, H(0,0) = H(1,1) = 0) -- offered
// here as a lead for that future task, not as a proven-sufficient condition.
//
// THE COMPARISON ITSELF is TIGHT (kElasticStallScale = 1e-12 relative, a
// numerical-zero threshold, NOT a bit-for-bit test -- see WHAT THE EXIT
// ACTUALLY TESTS above) deliberately: a looser comparison would only make
// the false-positive class above larger, not smaller. kElasticStallScale is
// declared alongside kElasticRhoInit/Max/Factor in
// detail/globalization/sqp/elastic.h, at the same NUMERICAL-ZERO value
// (1e-12 relative) kZeroStepScale uses in trust_region.h, for the same
// reason: two rungs that differ by less than that differ by rounding alone.
// See sqp_types.h's SqpOptions::elastic_ladder_early_exit for what DOES and
// does not move on the safe (bound-pinned) fixtures when this is turned on
// (the certified outcome is unchanged; an informational violation_l1 reading
// can move by ~1e-13, orders below feas_tol/kkt_tol).
//
// THE STEP IS THEN TAKEN EVEN WHEN THE SLACKS ARE NOT ZERO, and this is a
// deliberate reading of the brief rather than an oversight, so here is the
// argument. "Escalate until the slacks vanish, else restore" cannot be the
// whole rule, because THE SLACKS CANNOT VANISH ON A GENUINELY INCONSISTENT
// LINEARIZATION: s = 0 implies the original rows are all satisfied at p,
// i.e. the original QP was feasible -- the converse proved above. Under that
// rule the tier would take a step only in the FALSE-kInfeasible case and
// would abandon every real one, which is precisely the case it was built
// for. SqpDriverElastic.InconsistentLinearizationRecovers is the worked
// example: its two rows satisfy s1 + s2 >= 1 IDENTICALLY (they are
// antiparallel), no rho changes that, and the step taken with s2 = 1 lands
// exactly on the true feasible set because the TRUE constraints curve where
// their linearization does not. The funnel then judges that step on the true
// f and h like any other, which is this file's standing discipline: judge on
// the truth, not on the model.
//
// SO THE EXHAUSTION SIGNATURE IS A CONJUNCTION, and it is deliberately shaped
// like globalization.h's own restoration signature -- on MODEL quantities,
// because here there is no trial point to judge. The tier is EXHAUSTED when,
// after the ladder is spent:
//
//   (i)   the elastic solve did not reach kOptimal at all (nothing came
//         back), OR all three of
//   (ii)  the relaxation is still materially open (sum s > feas_tol) -- the
//         brief's literal condition, and Algorithm 5's own "subproblem
//         infeasible" in its quantitative form: this is the MINIMUM
//         achievable linearized violation once rho dominates,
//   (iii) no admissible step reduces that violation (sum s is not below the
//         violation at p_ref, which is what the WITNESS point already
//         achieves for free), and
//   (iv)  the model promises no objective decrease either
//         (predicted_decrease on the original QP <= 0).
//
// (iii)+(iv) say exactly "this point is an infeasible stationary point OF THE
// LINEARIZED PROBLEM": nothing here reduces infeasibility and nothing here
// reduces the objective. Compare globalization.h's conjuncts (c) pred_df <= 0
// and (d) h_new >= h_old -- the same two questions asked of the model instead
// of a trial. That is KLV Lemma 5 case 1's configuration, and it is the point
// at which a restoration PHASE is the only remaining move.
//
// THE KNIFE-EDGE RULING (Phase-5 Task 10). Conjunct (iv) tests
// predicted_decrease(qp, p_elastic) > 0.0 with NO tolerance.
// docs/notes/2026-07-31-schur-cap-policy.md section 4.2's instrumented print
// (on InfeasibleCircleLineModel's border/refactorize divergence) is the
// fixture this ruling was originally checked against, and fix round 1's
// review corrected this comment's own reading of it (I-3): the two values
// there, -6.4392935428259079e-15 (border) and -1.1102230246251565e-15
// (refactorize), are BOTH NEGATIVE -- the same side of zero, not opposite
// sides -- and conjunct (iv) (promises_f) reads FALSE in BOTH modes at the
// actual, untolerated test. The conjunct that differs between the two modes
// there is `reduced` (0 vs 1, from slack_l1 0.99999999999999556 vs
// 0.15484704377832298), which this ruling does not touch. So THAT
// instance was never actually a case of conjunct (iv) deciding anything --
// this comment's earlier claim that it "decided a whole trajectory" on
// those two values was a misreading of its own cited source, and the
// section 4.2 conclusion it should have led with is its own: "the
// divergence is not a knife-edge in the predicate; the two modes are being
// asked the question at different points." RULING, on the corrected
// reading: KEEP EXACT ZERO. Two reasons, not one:
//
//   (1) CONSISTENCY. This is the driver's THIRD copy of the identical test
//       "does the model promise a decrease" -- globalization.h's Eq. (10)/
//       (11) switching-condition machinery reads pred_df <= 0.0 (no
//       tolerance) and this file's radius-growth rule reads
//       ctx.pred_df > 0.0 (also no tolerance; the TRUST REGION note's growth
//       rule, later in this file). Giving
//       ONLY the elastic tier's copy a tolerance would make the SAME
//       quantity, computed the SAME way, decide "promises a decrease" by
//       three different rules depending on which of the driver's three call
//       sites asks -- a worse inconsistency than the knife-edge it would
//       fix.
//   (2) THE KNIFE-EDGE IS IN THE SUBPROBLEM, NOT THE PREDICATE.
//       docs/notes/2026-07-31-schur-cap-policy.md section 4's own finding
//       (independent of this task) is that border and refactorize are not
//       disagreeing about ONE point's pred_df -- the fixture's elastic
//       relaxation has a flat augmented objective (this file's STALL
//       EARLY-EXIT note above derives why: the Lagrangian Hessian is
//       identically zero and the augmented objective is EXACTLY CONSTANT on
//       the whole feasible set at every rho), so the two algebra modes land
//       on TWO DIFFERENT, EQUALLY OPTIMAL points and it is unsurprising that
//       both points' pred_df read as rounding noise near zero. A tolerance
//       widens the ACCEPTANCE WINDOW at whichever two points the modes
//       happen to be at; it does not make the two modes agree on WHICH
//       point to be at, so it trades one knife-edge (this predicate's own
//       zero) for another (which arbitrary point each mode's own tie-break
//       lands on) rather than removing one. THIS TASK MEASURED EXACTLY THIS
//       FAILURE MODE ELSEWHERE (the STALL EARLY-EXIT note's flat-objective
//       counter-example, once its mechanism is read correctly): a
//       numerically "helpful-looking" relaxation of a comparison against
//       zero, on this same fixture class, reshaped a trajectory by tens of
//       majors even though every reshaped trajectory still certified
//       correctly. That is the concrete reason a tolerance here is not a
//       free improvement, not merely a restated worry.
//
// A tolerance is therefore NOT ADDED. If a future task wants one, it needs a
// derivation of the RIGHT SCALE for pred_df (unlike feas_tol's constraint
// units, pred_df is in OBJECTIVE units, which this driver has no existing
// scale-invariant tolerance for) and a fixture that shows the current
// exact-zero test causing an actual WRONG certificate -- not merely a
// slower or faster path to the same one, which is all this task's own
// investigation found.
//
// WHAT AN EXHAUSTED TIER DOES: it raises the kRestore signal, which Task 9
// routes into THE RESTORATION PHASE (see that note) exactly as it routes the
// funnel's own kRestore and the radius floor's. Through Task 8 this was an
// exit -- SqpStatus::kInfeasible with StepVerdict::kRestore on the last
// history row -- and the row still looks the same when the phase cannot
// recover; what changed is that the phase now runs first and the status is
// its verdict rather than the tier's.
//
// MULTIPLIERS ARE NOT CARRIED OUT OF AN OPEN RELAXATION. At an elastic
// solution with s_j > 0 the row's multiplier is FORCED to rho by
// stationarity in s_j, and the contamination spreads through shared
// variables to rows whose own slack is zero. Feeding those to eval_hess
// would build the next Hessian out of the penalty parameter. MEASURED
// (mutation): carrying them on InconsistentLinearizationRecovers gives
// H = diag(2 - 4e8, 2) at the next iterate and the solve ends
// kNumericalError instead of kOptimal. elastic_project therefore zeroes them
// unless every slack closed -- the same thing qp_engine.h does on its own
// kInfeasible exits ("do not let those numbers leak out as if they were
// prices"), for the same reason.
//
// SOC IS NOT ATTEMPTED ON AN ELASTIC ROW. The correction's premise is that a
// CERTIFIED step was rejected for curvature the linearization could not see;
// on this path the linearization was inconsistent to begin with and the step
// came from a relaxed copy of it, so the residual SOC would try to cancel is
// not the quantity SOC's derivation is about. Gated on !elastic_applied.
// NOTHING IN THIS FILE'S SUITE DISTINGUISHES THE GATE (an elastic step that
// is then rejected with h_new > h_old does not arise on any shipped fixture),
// so this is scoping stated honestly, not a measured necessity -- the same
// status as Task 6b's probe_drop_made conjunct.
//
// WHAT IT COSTS, AND WHAT BOUNDS IT. Each activation costs one to seven QP
// solves and NO model evaluation of its own (the elastic problem is built
// from the QpProblem already in hand; eval_hess is not called, and the trial
// evaluation that follows is the one any accepted step pays). The tier may
// re-activate at every trial -- there is no one-shot bound like the routed
// failure's -- because unlike a shrink-retry it does not repeat the same
// question: the exhaustion signature terminates the genuinely stuck case, and
// max_iter bounds the rest, so the worst case is max_iter * 7 solves. The
// obvious refinement, NOT taken here, is a STALL TEST: stop escalating as
// soon as two consecutive rho values give the same slacks (which is every
// escalation on both fixtures in this file -- 12 of 12 solves on
// InconsistentBoundedModel change nothing), instead of always spending the
// whole ladder. It is left out because the ladder as specified is what makes
// the escalation counts in the tests hand-derivable, and the saving is
// bounded by 6 QP solves per activation.
//
// --- THE RESTORATION PHASE (Task 9) ---------------------------------------
//
// WHAT IT IS. When a restoration request is raised -- from any of the three
// sources in SUBPROBLEM FAILURE ROUTING -- the driver stops minimizing f and
// starts minimizing the infeasibility measure it has been judging all along,
//
//     h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)),      subject to l <= x <= u,
//
// from the iterate where the request was raised. Two things can happen, and
// they are the two outcomes KLV Sec. 5.1 names:
//
//   h < feas_tol      -> RESUME. The main loop restarts from the restored
//                        point with the funnel re-based, the radius reset and
//                        the multipliers dropped (all three below).
//   h stationary and  -> CERTIFIED SqpStatus::kInfeasible, and the ONLY exit
//   h > feas_tol         that sets SqpSolution::infeasibility_certified. (The
//                        decision table below has the other four exits, none
//                        of which set it -- and the flag is the only thing
//                        that distinguishes them, see sqp_types.h.) There is
//                        no direction from this point that reduces the
//                        constraint violation, so the point IS the answer:
//                        it is returned, with the multipliers that certify it
//                        (sqp_types.h's SqpSolution note). This is the
//                        Byrd-Curtis-Nocedal rapid-infeasibility-detection
//                        exit -- "rapid" because it reports the conclusion as
//                        soon as the FEASIBILITY problem converges, instead of
//                        letting the optimality loop grind to max_iter against
//                        constraints that cannot be satisfied.
//
// NO NEW SOLVER MACHINERY, AND THAT IS THE DESIGN. h is nonsmooth, so it is
// not an NlpModel and cannot be handed to this driver directly. Its standard
// smooth reformulation is (RestorationModel below):
//
//     min   sum_i sigmaE_i (sp_i + sm_i) + sum_j sigmaI_j si_j
//     s.t.  cE(x) + sigmaE.*(sp - sm) = 0
//           cI(x) - sigmaI.*si       <= 0
//           l <= x <= u,   sp, sm, si >= 0
//
// in the variables y = (x, sp, sm, si) -- an NlpModel WRAPPER around the
// caller's own model, whose derivatives are trivial extensions of it (the
// Jacobians gain constant diagonal blocks, the objective is linear, and the
// Hessian is the caller's OWN eval_hess called with obj_scale = 0, which is
// exactly the constraint part of the Lagrangian and precisely what the
// wrapper's Lagrangian Hessian is). It is then solved BY THIS SAME CLASS: a
// nested SqpDriver, with the same funnel, the same trust-region rules, the
// same elastic tier and the same SOC. The restoration phase is therefore about
// a hundred lines of model-wrapping plus a mode switch, not a second solver,
// and every improvement to the driver improves it automatically.
//
// EXACTNESS OF THE REFORMULATION, since everything else rests on it. At any
// y feasible for the wrapper, sp_i - sm_i = -cE_i/sigmaE_i with both >= 0, so
// sigmaE_i(sp_i + sm_i) >= |cE_i|; likewise sigmaI_j si_j >= max(0, cI_j).
// Hence f_w(y) >= h(x) always, with equality exactly when the slacks are
// minimal -- so minimizing f_w over the wrapper's feasible set minimizes h,
// and the START POINT (slacks set to the violations at the entry iterate) has
// f_w = h(x_entry) exactly. There is no penalty parameter and nothing to
// escalate: this is the EXACT reformulation, not an l1 penalty of the
// original problem.
//
// THE SIGMA SCALING IS TASK 8'S CARRY, APPLIED. sigmaE_i = max(1,
// ||grad cE_i(x_entry)||inf) and likewise sigmaI_j -- the slack COLUMN is
// scaled to the JACOBIAN ROW IT JOINS, fixed once at construction so the
// wrapper's variables have constant units. Task 8's elastic tier learned this
// the expensive way (its fix rounds 1-2: a unit slack column next to a row of
// magnitude S loses the O(1) objective tie-break that decides where on a flat
// face the optimum sits, and the solve drifts continuously off the true
// endpoint as S grows), and its carry names THIS construction as the next
// place to check. Both the objective coefficient and the column entry carry
// sigma, so h is still exactly what is minimized and the multiplier
// certificate below is untouched by the scaling. A second benefit falls out:
// the sub-solve's trust region applies to EVERY variable including the slacks
// (unlike the elastic tier, this problem's radius is a genuine SolveOverrides
// radius), and in scaled units the slack a row needs moves at the same rate
// as x rather than at the constraint's own scale.
//
// WHY THE MULTIPLIERS ARE A CERTIFICATE. Stationarity of the wrapper in
// sp_i reads sigmaE_i + sigmaE_i*lambda_e(i) - z = 0 with z >= 0 at the
// slack's lower bound, giving lambda_e(i) >= -1; in sm_i it gives
// lambda_e(i) <= 1; and in si_j it gives lambda_i(j) <= 1 against the
// engine's own lambda_i >= 0. The x-block of the wrapper's stationarity has
// NO objective term (f_w does not involve x), so it reads
//     Je^T lambda_e + Ji^T lambda_i - z = 0,   lambda_e in [-1,1]^me,
//                                              lambda_i in [0,1]^mi,
// which is exactly "0 is a subgradient of h at x, modulo the normal cone of
// the box". A converged restoration therefore hands back the certificate for
// free, in the multipliers it was going to return anyway -- no extra
// computation, and nothing for the driver to re-derive. The entries are
// pinned to sign(cE_i) on violated rows by the same stationarity conditions
// (a slack strictly off its bound has z = 0).
//
// WHAT IS CARRIED IN, AND WHAT IS DELIBERATELY NOT:
//   TRUST REGION: carried, Delta as it stood when the request was raised
//     (resolved to tr_max if it was +inf). The radius is a statement about
//     how far the MODEL is trusted at this point, and that does not change
//     because the objective did. The sub-solve then manages it on its own.
//   BUDGET: carried, and shared -- the sub-solve gets what is left of
//     max_iter (see SqpCounters), so restoration cannot double a solve's
//     worst-case cost.
//   OPTIONS: tolerances, enable_soc and the whole QpOptions block are carried
//     unchanged. tr_min is carried too, so the sub-solve has its own floor and
//     cannot spin.
//   THE STRATEGY FACTORY IS NOT CARRIED. SqpOptions::make_strategy is a
//     factory for judging THE CALLER'S problem; the restoration problem has
//     different variables, a different objective and a different h, and a
//     caller-supplied strategy holding state keyed to the original problem
//     would be judging something it was not written for. The sub-solve uses
//     the default FunnelStrategy. (The OUTER funnel is untouched by the
//     sub-solve and is re-based on the way back -- see below.)
//   NESTED RESTORATION IS NOT ALLOWED. The sub-driver is constructed with
//     restoration disabled, so a restoration request inside it takes the
//     pre-Task-9 exit: kInfeasible, with the last row's verdict carrying
//     whichever shape its own triggering source left (kRestore for the
//     funnel/elastic routes, kReject for the radius floor -- see THE
//     DECISION TABLE ON THE WAY OUT below), and the recursion is one level
//     deep by construction. A feasibility problem that cannot make progress
//     on its own feasibility is a genuine dead end, not something a third
//     level would resolve.
//
// WHAT RESUMING DOES, in the order it does it:
//   x       <- the restored point; its NlpEval is REUSED (the sub-solve's
//              final evaluation is at the same x, but on the WRAPPER, so the
//              main model is re-evaluated once here -- one eval_nlp, the same
//              price any accepted step pays).
//   funnel  <- strategy->resume_from_restoration(h_restored), which for the
//              shipped FunnelStrategy is KLV Algorithm 2's re-basing
//              tau_+ = (1-kappa)h + kappa*tau. NOT reset(h): see
//              globalization.h, where the difference (and why re-initializing
//              would forfeit the funnel's monotonicity) is spelled out.
//   Delta   <- kRestoreRadiusFactor * tr_init, floored at tr_min. A DOCUMENTED
//              FRACTION rather than either extreme: the radius that stalled is
//              meaningless at a point the solve has not been to, so carrying it
//              forward would hobble the resumed loop; restarting at tr_init
//              would repeat the overshoot that got the solve into trouble at a
//              point that is, if anything, more delicate. One order of
//              magnitude below the caller's own starting radius is the
//              conservative reading of "start again", and the growth rule
//              earns it back within a few accepted steps if the model deserves
//              it. (+inf tr_init resolves to tr_max first, as everywhere.)
//   lambda  <- ZEROED. The multipliers in hand price the wrapper's constraints
//              -- they are subgradient selectors in [-1,1], not NLP prices --
//              and feeding them to eval_hess would build the first
//              post-restoration Hessian out of them. The brief's instruction is
//              the right one: they are re-estimated by the first main-loop QP,
//              which is exactly what happens to a solve's very first
//              subproblem. (This is the same quarantine reasoning as the
//              elastic tier's, applied to a different contaminant.)
//   counts  <- rejections_at_iterate = 0 (the iterate moved), the subproblem
//              is marked stale (it must be rebuilt at the new point), and the
//              warm seed is DROPPED: the restoration sub-solve ran on a
//              different engine with a different problem shape, and the outer
//              engine's last seed describes an active set at the abandoned
//              iterate.
//
// THE DECISION TABLE ON THE WAY OUT, which is the contract sqp_types.h's
// SqpStatus note points at. `certified` is SqpSolution::infeasibility_certified
// and is the ONLY field that separates the three kInfeasible rows:
//
//   h(x_r) <= feas_tol             -> RESUME        certified = false
//   sub-solve kOptimal, h > tol    -> kInfeasible   certified = TRUE
//   sub-solve kInfeasible (stuck)  -> kInfeasible   certified = false
//   second request (cap)           -> kInfeasible   certified = false
//   sub-solve kMaxIter / no budget -> kMaxIter      certified = false
//   sub-solve kNumericalError      -> kNumericalError, multipliers cleared
//
// THE LAST HISTORY ROW'S VERDICT DOES NOT TRACK THIS TABLE, AND IS NOT
// UNIFORM. An earlier version of this note claimed all three kInfeasible rows
// above carry StepVerdict::kRestore on the last history row; that is false
// for one of the THREE REQUEST SOURCES that can raise ANY of these rows (see
// the request sites above, :376-395). The funnel's signature and the elastic
// tier's exhaustion both push their triggering row with verdict == kRestore,
// but THE RADIUS FLOOR pushes its triggering row with verdict == kReject
// (both of its routes: the judged-reject push and the routed-QP-failure
// push, further up in solve_impl) -- entering restoration from the floor is
// not itself a judged verdict, it is what happens AFTER a kReject (or a
// routed failure that never reaches judge() at all) hits tr_min. The floor
// route is told apart on that SAME row by tr_radius sitting at
// SqpOptions::tr_min (:391-393), not by verdict. A caller wanting to know
// WHICH of the three sources raised a given restoration exit should read
// (elastic_applied, tr_radius) on the last row, never verdict alone.
//
// A false on a kInfeasible row means "this driver could not make progress and
// makes NO claim about the model", which is a strictly weaker statement than
// the status alone reads as.
//
// THE DEGENERATE ENTRY IS BENIGN AND IS NOT SPECIAL-CASED. A request raised at
// an already-FEASIBLE iterate (the radius floor can do this: a collapsed radius
// says nothing about h) finds the feasibility problem solved at its own start
// point, so the phase costs at most one major and returns immediately with
// h < feas_tol -- and the RESUME then does the one thing that was actually
// wrong, which is to restart the radius. So "restoration" at a feasible point
// degrades exactly to a radius reset, which is the right response and needs no
// separate mechanism.
//
// ONE RESTORATION PER SOLVE, and it is a deliberate under-commitment. A second
// request after a resume returns SqpStatus::kInfeasible without running the
// phase again. THE REASON IS CYCLING: restoration moves the iterate to a point
// chosen for feasibility alone, from which the optimality phase may well walk
// straight back into the same stall, and a driver that restores every time has
// no mechanism to notice. The alternatives (a decreasing sequence of
// restoration thresholds, or requiring strict h-progress between entries)
// are real and standard, and both need a battery to tune -- so the cap is set
// at one and the case is REPORTED (the exit says restoration_iters > 0 with
// kInfeasible, which is exactly "we restored once and it was not enough").
// Task 11's battery is where this should be relaxed, with evidence.
//
// THE HISTORY ACROSS A RESTORATION. The phase produces NO history rows of its
// own (it is not the caller's problem being iterated), so the row that raised
// the request is followed directly by the first row at the RESTORED point.
// SqpCounters' "recovering the iterate sequence" predicate survives this
// unchanged and without a special case, because it reads the PREVIOUS row's
// verdict for `!= kReject` and a kRestore row is exactly a row after which the
// iterate moved. What a consumer cannot do is read a restoration's cost out of
// the history -- that is what counters.restoration_iters is for.
//
// --- WARM SEEDING --------------------------------------------------------
//
// One QpEngine instance serves the whole solve, and every subproblem after
// the first is solved through the warm overload with the PREVIOUS major's
// QpSolution as the seed. What that carries -- and what it is deliberately
// prevented from carrying -- matters:
//
//   CARRIED: bound_state and ineq_active, i.e. the previous ACTIVE SET.
//   Since the subproblem's box is l - x .. u - x, a variable active at an NLP
//   bound is active at the SAME index of the subproblem's box on every major,
//   so the labeling transfers directly, and so does a general row's activity.
//   This is the part that pays.
//
//   NOT CARRIED: the previous STEP as a primal start point. seed.x is ZEROED
//   before the seed is handed to the engine, and that is a correctness
//   requirement, not a tuning choice. Per qp_engine.h's section 6 the trust
//   region is applied about THE SOLVE'S OWN START POINT --
//   lo_eff = max(lower, x0 - Delta) with x0 = clamp(seed.x, lower, upper) on
//   a warm solve. In step variables the SQP trust region must be centered at
//   p = 0 (that is what "a radius Delta about the current NLP iterate x"
//   means); seeding x0 = p_prev instead re-centers it on the previous step,
//   so the box becomes p_prev +/- Delta and the driver silently takes steps
//   of up to |p_prev| + Delta while believing the radius is Delta.
//
//   MEASURED, on HS6 from its published start at tr_init = 1.0: with the
//   previous step left in the seed, major 1 took a step of inf-norm 1.6 at a
//   radius of 1.0, and the drift put major 2's linearized equality outside
//   its own (mis-centered) box, so the solve ended kInfeasible. Zeroing
//   seed.x holds every step to the radius and the same run converges.
//
//   ZEROING seed.x IS NECESSARY BUT WAS NOT SUFFICIENT, and the earlier text
//   here claimed otherwise -- it said "the variables the seed pins at a bound
//   are unaffected: at such an index the subproblem's own bound is
//   l(i) - x(i) == 0, so the snap puts x0(i) at 0 either way". That covers
//   only a SAME-bound pin, i.e. a variable sitting at the NLP bound the seed
//   pinned. It is false for the case the rejection retry actually produces:
//   the retry re-solves the SAME subproblem, whose box is l - x .. u - x, and
//   a step that ran into one of those bounds seeds a pin at a NONZERO offset
//   from p = 0. The engine then materialized that pin onto x0 BEFORE
//   computing the window about x0, so the trust region re-centered on the
//   bound and the zeroed seed.x was overwritten before it could do any good.
//   Measured (fix round 1): HS5 at tr_init = 10 returned the same step of
//   inf-norm 6 bit-identically on all 60 trials while the driver halved the
//   radius to 1.7e-17, ending kMaxIter at the wrong point; 6 of 48 shipped
//   configurations violated step_norm <= tr_radius. The fix is ENGINE-SIDE --
//   qp_engine.h's WINDOW-CONSISTENCY RULE, which computes the window about
//   the clamped seed primal and drops any seeded pin lying outside it --
//   because the driver-side alternative (never sending bound hints) would
//   forfeit the hot start the retry exists to keep.
//
// Note what this does NOT buy on a genuinely nonlinear model: qp_engine.h's
// HOT-START REUSE fast path additionally requires H/Ae/Ai's VALUES to be
// unchanged, and they change every major by construction, so K0 is rebuilt
// each time. The win here is minor iterations (the active-set walk), not
// factorizations -- which is exactly what
// SqpDriver.WarmSeedingKeepsLateSubproblemsCheap asserts.
//
// THE REJECTION RETRY IS THE EXCEPTION, and it is why qp_types.h has
// SolveOverrides at all. A rejected trial re-solves the SAME QpProblem
// object -- the iterate did not move, so H, g, Ae, Ai and the box are the
// same bytes -- with only SolveOverrides::tr_radius changed, on the same
// engine, seeded from the rejected solve's own working set. That is
// precisely qp_engine.h's HOT-START REUSE case: the values/structural hashes
// match, tr_radius is deliberately not in the reuse key (bounds never enter
// K0), and the effective (primal_delta, dual_mu) pair is untouched, so the
// retry can skip K0's assembly and factorization outright. It is the same
// shape as test_qp_engine_tr.cpp's ShrinkRadiusRetryReusesHotStart, now
// driven by the real loop.
//
// REUSE IS NOT ASSERTED UNCONDITIONALLY, per that header's own warning: the
// five conditions are NECESSARY, not sufficient (border_candidate's perturbed
// pivots and schur_cap checks can still force a rebuild), and condition (b)
// -- seed working set == previous exit working set -- fails by construction
// on the SECOND consecutive retry, because a TR-pinned index is reported
// kFree in bound_state (rule (a)) and so cannot be reproduced as a seed hint.
// SqpDriverTrustRegion.RejectionShrinksAndRetriesHotly therefore asserts the
// OBSERVED per-trial factorization counts with the conditions spelled out,
// not a blanket == 0.
//
// THE RETRY MUST NOT RE-CENTER THE TRUST REGION. seed.x is zeroed on the
// rejection path exactly as on the acceptance path, for the same reason: the
// engine centers the radius on clamp(seed.x, ...), and in step variables the
// SQP trust region is centered at p = 0. A retry that seeded the REJECTED
// step would center the shrunken box on the very step that was just judged
// too aggressive.
//
// --- FULL-STEP-FIRST WARM RULE, AND ITS WATCHDOG (Phase-4 Task 5) ---------
//
// [KD] Kungurtsev & Diehl, COAP 59:475-509 (2014): standard globalization
// ACTIVELY INTERFERES WITH WARM STARTS. A point handed in by a nearby
// problem's solve is (the warm-start premise) already inside the Newton domain
// of THIS problem's solution, where the undamped SQP step converges
// superlinearly -- and an acceptance test calibrated to make a COLD start
// globally convergent will spend majors refusing exactly that step.
// globalization.h's FULL-STEP MODE is the strategy-side half of the answer
// (unit steps, Algorithm 2 not consulted); this note is the driver's half: WHO
// TURNS IT ON, WHAT IS TRACKED WHILE IT IS ON, AND WHAT ENDS IT.
//
// IT IS A MODE OF THE STRATEGY, NOT A SECOND MAJOR LOOP, and that is a design
// requirement rather than an implementation convenience. Everything the loop
// already does around the verdict -- the elastic tier on a kInfeasible
// subproblem, the failure routing on a kNumericalError one, the radius floor,
// the restoration phase, the SOC gate, the zero-step short-circuit, the
// history -- is INHERITED unchanged, because from the loop's point of view a
// full-step major is just a major whose strategy happened to say kAcceptF. A
// forked loop would have had to re-implement all of it, and would have had to
// keep re-implementing it.
//
// ENGAGED WHEN ALL THREE HOLD, checked once, at the first measurable iterate:
//   (1) SqpOptions::warm_full_step (default true; the A/B lever),
//   (2) the WARM-START INGEST resolved to kWarm or above -- a cold solve has
//       no nearby-solution premise to lean on and gets the ordinary funnel.
//       "kWarm OR ABOVE" reads literally from Phase-6 Task 5 on: a kSeeded
//       resolution does NOT arm the mode, for the same reason a cold one does
//       not. The Kungurtsev-Diehl premise is a warm start ON THE SAME PROBLEM,
//       and a seeded object is by definition one that cannot say which problem
//       it came from,
//   (3) the strategy in hand IS a FunnelStrategy. The mode is that class's own
//       state, so a caller-supplied strategy simply never enters it. That is
//       fail-safe in the right direction (full globalization), and it is the
//       same dynamic_cast the Task-3 width re-basing already does.
//
// WHAT IS TRACKED: the BEST ITERATE BY ||KKT||inf, i.e. by kkt.residual() from
// the SAME evaluate_kkt call the convergence test reads. The driver snapshots
// (x, lambda_e, lambda_i, ev) at every new best -- an NlpEval copy, NOT a
// re-evaluation, so the watchdog costs no model calls at all -- and counts two
// things: consecutive majors whose residual GREW, and majors since the last
// new best.
//
// WHAT ENDS IT (either signal; sqp_types.h has the constants and their
// derivation):
//   (a) the residual grew kWarmResidualGrowthMax majors IN A ROW -- diverging;
//   (b) kWarmFullStepWindow majors passed with no new best -- stalled.
// In BOTH cases the driver RESTORES the best iterate first and only then hands
// the solve back to the funnel: that is the watchdog, and restoring is the
// whole point of it -- the mode's own steps are unjudged, so the last iterate
// it produced carries no guarantee whatever, while the best one is the best
// point ANY mechanism in this solve has reached.
//
// THE RE-BASING IS THE RESTORATION-RESUME ONE, DELIBERATELY. The exit calls
// strategy->resume_from_restoration(h at the restored point) -- KLV Eq. (13),
// tau_+ = (1-kappa)h + kappa*tau, the same call the restoration phase's own
// resume makes and for the same reason (globalization.h's note on that hook):
// re-entering the optimality phase at a point some non-funnel mechanism chose
// must NOT re-initialize the width by Eq. (9), which would push a funnel that
// had tightened back out to tau_bar = 100 and forfeit monotonicity.
//
// THE SAFETY INVARIANT, STATED AS A PROPERTY OF THIS FILE: the full-step mode
// CANNOT CERTIFY kOptimal AT A POINT FAILING THE STANDARD KKT CHECK. Nothing
// in the mode touches the CONVERGENCE TEST above -- `converged` is still
// kkt.stationarity <= kkt_tol && kkt.feasibility <= feas_tol on the very same
// SqpKkt the ordinary path uses, and the mode has no way to reach that
// statement at all. What the mode changes is which trials are ACCEPTED (which
// is what an acceptance test is for); a solve that runs the mode to
// convergence exits kOptimal through the identical code a cold solve does.
//
// THE TRUST REGION IS NOT SUSPENDED, ONLY THE FUNNEL TEST IS. The subproblem
// is still solved at a radius, and that radius still evolves by RADIUS
// MANAGEMENT's ordinary rules -- with one asymmetry worth naming because it is
// the whole of the "how does Delta evolve under the mode" question:
//   * IT NEVER SHRINKS ON A REJECTION, because the mode produces no
//     rejections. That is [KD]'s intent made mechanical: shrinking is how
//     globalization damps a step, and damping the full step is precisely the
//     interference the rule exists to remove. (Delta CAN still shrink on a
//     ROUTED QP FAILURE -- see SUBPROBLEM FAILURE ROUTING -- because that is
//     not a judgement about the step at all but a statement that the
//     subproblem could not be solved at this radius; the mode has no business
//     overriding it, and the watchdog's stall signal is what catches a run of
//     them.)
//   * IT STILL GROWS on a trust-region-active step whose actual decrease
//     matched the model's prediction, unchanged. Growth is what keeps the
//     radius from CAPPING a full step that a carried-over warm radius (Task
//     3 ingests warm.tr_radius) happens to be too small for -- a cap would
//     silently turn the "full step" into a damped one and the rule would be
//     inert.
// So under the mode the radius is, in practice, the ingested warm radius,
// possibly grown: exactly "the warm start's own trust region, not re-derived
// from cold defaults", which is the same carried-over-state argument [KD]
// makes for the multipliers and the funnel width.
//
// INTERACTION WITH THE SUSPECT GATE (Task 3b) AND WITH ANY OTHER SUBPROBLEM
// FAILURE, spelled out because the mode changes who is asked and who is not.
// If a full-step major's QP exits kNumericalError -- the suspect-escalation
// ladder in qp_engine.h exhausting itself is one way to get there -- the
// driver takes SUBPROBLEM FAILURE ROUTING, which does not consult the strategy
// AT ALL: no trial point exists, so the mode is never asked and cannot accept
// anything. The iterate does not move, the radius halves once (the one-shot
// retry), and the next pass measures THE SAME residual at THE SAME point --
// which is not a growth, so signal (a) stays put, but is not a new best
// either, so signal (b) advances. A subproblem that keeps failing therefore
// burns the window and the watchdog ends the mode after kWarmFullStepWindow
// majors, restoring an iterate that (in this case) is the one the driver is
// already standing on -- a no-op move, a real mode exit, and a counted restore
// (sqp_types.h's watchdog_restores note). A second CONSECUTIVE routed failure
// ends the solve outright, exactly as it does without the mode. Nothing here
// is special-cased: the mode neither suppresses nor triggers an escalation,
// and a suspect-gate refusal is fatal or survivable under the mode exactly as
// it is outside it.
//
// ENTERING RESTORATION ENDS THE MODE. All three restoration request sources
// remain reachable under it (the funnel's own signature is the one exception,
// since it cannot fire from a verdict the mode never asks for), and a resume
// clears the mode as part of resume_from_restoration -- see the RESTORATION
// PHASE note's resume ordering. A restoration is definitive evidence against
// the mode's premise, so the solve comes back out of it fully globalized.

// --- BUDGETED MODE (Phase-4 Task 6) ---------------------------------------
//
// A bounded-iteration solve for a CONTINUATION DRIVER: one that repeatedly
// re-solves a moving problem (or the same problem from successively better
// starting points) under a per-call iteration cap, and needs SOMETHING USABLE
// back even when that cap is hit before the KKT test fires. See
// SqpOptions::budget_mode (sqp_types.h) for the caller-visible contract --
// this note is the mechanics.
//
// WHAT CHANGES, AND WHAT DOES NOT. Only the MAIN LOOP's own max_iter
// exhaustion (`converged` false, iter + restoration_iters == max_iter, this
// header's exit right after THE FULL-STEP WATCHDOG) is affected: the status
// reported is SqpStatus::kBudgetExhausted rather than kMaxIter, and the
// (x, lambda_e, lambda_i, z, f) reported are the BEST ITERATE THIS SOLVE
// VISITED, not the last one. Every other exit -- kOptimal, kInfeasible, a
// routed subproblem failure, a restoration outcome -- is completely
// unaffected; budget_mode changes nothing about WHEN a solve stops, only
// what it reports when the stop reason is "ran out of majors here".
//
// THE ORDERING IS TRACKED SEPARATELY FROM THE FULL-STEP WATCHDOG'S
// (fs_best_*, above), by design, not by accident: the two answer different
// questions and can name different iterates as "best" on the very same run
// (see SqpOptions::budget_mode's WHY THIS ORDERING note for the reasoning,
// and BudgetReturnsUsableIterate in tests/test_warm_start.cpp for a fixture
// where they demonstrably disagree). mb_best_* (the loop's own state) is
// updated once per measured pass -- placed AFTER the full-step watchdog
// block so that, on a pass where the watchdog just restored an earlier
// point, the candidate considered is that restored point (the one `row` and
// the eventual history entry describe), never the abandoned point the
// watchdog moved away from. The comparison is a plain lexicographic one on
// (violation_l1, f); ties keep the EARLIER candidate (strict `<`, not
// `<=`), which matters only in that it is deterministic, not in which
// admissible point it picks.
//
// WHAT THE WARM OBJECT DESCRIBES ON THIS EXIT. `qp`/`qp_built` (hence
// structure_hash) are the same as any other exit's -- structure_hash is a
// pure function of the model's own sparsity, independent of which iterate is
// reported. The ACTIVITY (`seed`'s bound_state/ineq_active), however, is
// only attached when the reported best iterate IS the current one this pass
// (checked by comparing this pass's row to the tracked best, exactly the
// restoration_moved_x reasoning used elsewhere in this loop) -- otherwise no
// in-loop QpSolution describes the returned point and activity is left
// unset, exactly as a restoration-moved exit leaves it unset for the same
// reason.
//
// WHY THE RESTORATION SUB-SOLVE IS EXEMPT. `enter_restoration`'s ropts is a
// copy of opts_ with budget_mode forced back to false: this lever's contract
// is scoped to the MAIN loop only (SqpOptions' own SCOPE paragraph), and the
// restoration phase running out of ITS OWN slice of the shared max_iter
// budget already has a designated answer -- kMaxIter, "no budget left to
// restore with" -- that says something different from "here is a usable
// point" and is not worth overloading. Forcing the sub-solve's own lever off
// is also what lets the `switch (rs.status)` mapping that phase's outcome to
// restoration_exit_status stay exhaustive with an UNREACHABLE
// kBudgetExhausted arm, rather than a live one this note would then have to
// justify choosing a mapping for.

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/ledger.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/detail/globalization/sqp/trust_region.h>
#include <hven/detail/qp/qp_engine.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/detail/qp/ssn_engine.h>
#include <hven/detail/qp/working_set.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/sqp_types.h>
#include <hven/model/nlp_model.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// Everything the driver needs from ONE model point except the Hessian:
// f, grad f, cE, cI, Je, Ji, evaluated exactly once.
//
// WHY THIS EXISTS. The convergence test and the subproblem construction read
// the SAME five derivative quantities at the same x, and evaluating them
// independently doubles the model's work on every major -- measured at 12
// virtual calls per major where 7 suffice. On tycho's target workloads
// (collocated optimal control) model evaluation dominates the solve, and
// every one of Tasks 5-9 calls build_subproblem, so the bundle is threaded
// through the interface now, while there is one caller, rather than
// retrofitted later across six.
//
// THE HESSIAN IS DELIBERATELY NOT IN HERE. It depends on (lambda_e,
// lambda_i) as well as x, it is the single most expensive thing a model
// computes, and the convergence test never needs one -- so it stays a
// separate eval_hess call made once per ACCEPTED iterate, inside
// build_subproblem. Folding it in would force a Hessian at the final iterate,
// where no subproblem is ever built.
//
// ONE EXCEPTION, from Phase-5 Task 0: make_warm_start is a SECOND eval_hess
// call site, outside build_subproblem and outside that per-iterate rule. It
// fires only on a solve that built NO subproblem at all (see this header's
// MODEL EVALUATION note and make_warm_start's THE ZERO-MAJOR PROBE), which is
// precisely the "final iterate where no subproblem is ever built" case above
// -- so the exception costs exactly one Hessian on solves that would
// otherwise have evaluated none, and zero on every other solve.
struct NlpEval {
    double f = 0.0;
    Vec grad, ce, ci;
    Eigen::SparseMatrix<double, Eigen::RowMajor> Je, Ji;

    // False if any of f/grad/cE/cI is NaN or infinite. See evaluate_kkt's
    // NON-FINITE ITERATES note for why this is tracked explicitly.
    bool all_finite = true;
};

// Evaluates the model once at x. Empty constraint blocks are skipped
// entirely (a model with me() == 0 gets no eval_ce/eval_jac_e call at all),
// so the per-major call count is exactly 2 + 2*[me>0] + 2*[mi>0], plus one
// eval_hess per accepted iterate -- and, on a solve that accepts none
// because it builds no subproblem at all, the single eval_hess
// make_warm_start's zero-major probe pays instead (see NlpEval above).
//
// THE CALLBACK RETURNS ARE CHECKED, NOT ASSUMED (M3 final review, S-1), and
// this is a wrong-answer guard rather than a courtesy. The argument x is the
// caller's and was always size-checked; the five RETURNS are the MODEL's, and
// nothing downstream re-measures them. An eval_ci that returns an EMPTY vector
// on a model whose mi() is 1 propagates a 0-sized ev.ci past allFinite() --
// vacuously true on an empty vector, so the finiteness screen sees nothing
// wrong -- and into evaluate_kkt's `for j < model.mi()` loop, which reads
// ev.ci(0) out of bounds. Eigen's own assert catches that in Debug and is
// compiled out under NDEBUG (CLAUDE.md §4), so in Release it is an unguarded
// read of whatever follows the empty vector's buffer. Every check below is an
// O(1) integer comparison against a dimension the model already declared, and
// each throw NAMES THE CALLBACK, because "size 0, expected 1" is not
// actionable unless the reader knows which of six functions produced it.
inline NlpEval eval_nlp(const NlpModel &model, const Vec &x) {
    const Index n = model.n();
    if (x.size() != n) {
        throw std::invalid_argument(
            fmt::format("eval_nlp: x has size {}, expected {} (= model.n())", x.size(), n));
    }
    NlpEval ev;
    ev.f = model.eval_f(x);
    ev.grad = model.eval_grad(x);
    if (ev.grad.size() != n) {
        throw std::invalid_argument(
            fmt::format("eval_nlp: model.eval_grad returned size {}, expected {} (= model.n())",
                        ev.grad.size(), n));
    }
    ev.all_finite = std::isfinite(ev.f) && ev.grad.allFinite();

    if (model.me() > 0) {
        ev.ce = model.eval_ce(x);
        if (ev.ce.size() != model.me()) {
            throw std::invalid_argument(
                fmt::format("eval_nlp: model.eval_ce returned size {}, expected {} (= model.me())",
                            ev.ce.size(), model.me()));
        }
        ev.Je = model.eval_jac_e(x);
        if (ev.Je.rows() != model.me() || ev.Je.cols() != n) {
            throw std::invalid_argument(
                fmt::format("eval_nlp: model.eval_jac_e returned a {}x{} matrix, expected {}x{} "
                            "(= model.me() x model.n())",
                            ev.Je.rows(), ev.Je.cols(), model.me(), n));
        }
        ev.all_finite = ev.all_finite && ev.ce.allFinite();
    } else {
        ev.ce = Vec(0);
        ev.Je = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, n);
    }
    if (model.mi() > 0) {
        ev.ci = model.eval_ci(x);
        if (ev.ci.size() != model.mi()) {
            throw std::invalid_argument(
                fmt::format("eval_nlp: model.eval_ci returned size {}, expected {} (= model.mi())",
                            ev.ci.size(), model.mi()));
        }
        ev.Ji = model.eval_jac_i(x);
        if (ev.Ji.rows() != model.mi() || ev.Ji.cols() != n) {
            throw std::invalid_argument(
                fmt::format("eval_nlp: model.eval_jac_i returned a {}x{} matrix, expected {}x{} "
                            "(= model.mi() x model.n())",
                            ev.Ji.rows(), ev.Ji.cols(), model.mi(), n));
        }
        ev.all_finite = ev.all_finite && ev.ci.allFinite();
    } else {
        ev.ci = Vec(0);
        ev.Ji = Eigen::SparseMatrix<double, Eigen::RowMajor>(0, n);
    }
    return ev;
}

// VALUES ONLY -- f(x), cE(x), cI(x) via NlpModel::eval_values, none of
// eval_nlp's grad/Je/Ji (Phase-5 Task 8, the eval-economics carry). Returns
// an NlpEval shaped exactly like eval_nlp's, so it drops into every helper
// that only reads f/ce/ci (constraint_violation_l1, build_soc_subproblem) --
// grad/Je/Ji are set to n/(me x n)/(mi x n)-SIZED ZEROS (an honestly-empty
// linearization, not eval_nlp's PER-BLOCK 0 x n SKIP -- there is no "empty
// block" here, every model has a real n/me/mi, this NlpEval simply carries
// no derivative information for any of them), so a caller who mistakenly
// reads them gets zeros of the RIGHT shape rather than a size mismatch two
// calls downstream. all_finite covers f/cE/cI only, exactly as its name on
// eval_nlp's own ev promises for its wider set.
//
// THE CALLER'S OBLIGATION, not enforced here: use this only where the
// derivatives genuinely go unread (this header's THE REJECTED-TRIAL FUNNEL
// EVALUATION and THE WARM-RESOLUTION PROBE notes are the two places that
// holds), and upgrade to a real eval_nlp (or, for the probe, real
// eval_jac_e/eval_jac_i calls) the moment a caller decides it needs more --
// see solve_impl's own upgrade-on-accept sites.
inline NlpEval eval_nlp_values(const NlpModel &model, const Vec &x) {
    const Index n = model.n();
    if (x.size() != n) {
        throw std::invalid_argument(
            fmt::format("eval_nlp_values: x has size {}, expected {} (= model.n())", x.size(), n));
    }
    NlpEval ev;
    model.eval_values(x, ev.f, ev.ce, ev.ci);
    // S-1, exactly as in eval_nlp above: eval_values writes cE/cI through
    // out-parameters, so a model that sizes either one wrong hands the same
    // out-of-range read to every consumer of this bundle. One override, two
    // blocks, both checked -- and unconditionally, because eval_values is a
    // SINGLE call that must produce both, with no me()/mi() > 0 branch to hide
    // behind (a 0-row block must come back size 0, which this check demands
    // just as strictly as it demands a nonzero one come back full).
    if (ev.ce.size() != model.me()) {
        throw std::invalid_argument(
            fmt::format("eval_nlp_values: model.eval_values returned cE of size {}, expected {} "
                        "(= model.me())",
                        ev.ce.size(), model.me()));
    }
    if (ev.ci.size() != model.mi()) {
        throw std::invalid_argument(
            fmt::format("eval_nlp_values: model.eval_values returned cI of size {}, expected {} "
                        "(= model.mi())",
                        ev.ci.size(), model.mi()));
    }
    ev.all_finite = std::isfinite(ev.f) && ev.ce.allFinite() && ev.ci.allFinite();
    ev.grad = Vec::Zero(n);
    ev.Je = Eigen::SparseMatrix<double, Eigen::RowMajor>(model.me(), n);
    ev.Ji = Eigen::SparseMatrix<double, Eigen::RowMajor>(model.mi(), n);
    return ev;
}

// UPGRADES a VALUES-ONLY NlpEval (eval_nlp_values' output, at THIS x) to a
// FULL one, IN PLACE: fills grad/Je/Ji and folds their finiteness into
// all_finite, WITHOUT recomputing f/cE/cI a second time. The result is
// byte-identical to a fresh eval_nlp(model, x) call (same model, same x --
// nlp_model.h's eval_values contract requires its f/cE/cI to already agree
// with eval_f/eval_ce/eval_ci's, so there is nothing left for a second
// values computation to teach this NlpEval), at the cost of one call each to
// eval_grad/eval_jac_e/eval_jac_i instead of a second, wasted eval_f/eval_ce/
// eval_ci. Used wherever solve_impl's own values-only trial or SOC-corrected
// point turns out, after judging, to need its derivatives after all -- see
// THE REJECTED-TRIAL FUNNEL EVALUATION's UPGRADE TO FULL sites.
//
// S-8 (M3 final review, round 2): THIS IS AN EVAL BOUNDARY TOO, and it takes
// the same three returns eval_nlp takes -- so it checks them the same way, for
// the same reason. S-1 closed eval_nlp and eval_nlp_values and left this one,
// which is the THIRD door into the same NlpEval and the one an ACCEPTED trial
// comes through: a values-only trial or SOC-corrected point that the funnel
// judged worth keeping is upgraded here, and the resulting ev is what the NEXT
// major linearizes from. A short grad reaches evaluate_kkt's grad_lag(i) and a
// mis-shaped Je/Ji reaches build_subproblem's blocks, both unchecked, both
// out-of-bounds in Release where Eigen's asserts are gone. The cE/cI half of
// this bundle is already covered (it can only have come from a checked
// eval_nlp_values), so exactly the three derivative returns are checked here.
inline void upgrade_to_full(const NlpModel &model, const Vec &x, NlpEval &ev) {
    const Index n = model.n();
    ev.grad = model.eval_grad(x);
    if (ev.grad.size() != n) {
        throw std::invalid_argument(fmt::format(
            "upgrade_to_full: model.eval_grad returned size {}, expected {} (= model.n())",
            ev.grad.size(), n));
    }
    if (model.me() > 0) {
        ev.Je = model.eval_jac_e(x);
        if (ev.Je.rows() != model.me() || ev.Je.cols() != n) {
            throw std::invalid_argument(fmt::format(
                "upgrade_to_full: model.eval_jac_e returned a {}x{} matrix, expected {}x{} "
                "(= model.me() x model.n())",
                ev.Je.rows(), ev.Je.cols(), model.me(), n));
        }
    }
    if (model.mi() > 0) {
        ev.Ji = model.eval_jac_i(x);
        if (ev.Ji.rows() != model.mi() || ev.Ji.cols() != n) {
            throw std::invalid_argument(fmt::format(
                "upgrade_to_full: model.eval_jac_i returned a {}x{} matrix, expected {}x{} "
                "(= model.mi() x model.n())",
                ev.Ji.rows(), ev.Ji.cols(), model.mi(), n));
        }
    }
    ev.all_finite = ev.all_finite && ev.grad.allFinite();
}

// h(x) -- the l1 CONSTRAINT VIOLATION, i.e. the infeasibility measure the
// funnel of globalization.h is built around:
//
//     h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)).
//
// This is KLV's h (Kiessling/Leyffer/Vanaret, arXiv:2409.09208, Sec. 2.4.1),
// generalized from the paper's h(x) = ||c(x)||_1 -- KLV's NCO has equalities
// and BOUNDS only, so an inequality term does not arise there; adding the
// positive part of each cI row is the standard l1 violation and reduces to the
// paper's h exactly when mi() == 0.
//
// BOUNDS ARE DELIBERATELY EXCLUDED, on the paper's own grounds ("Without loss
// of generality, the bound constraints on x are always feasible throughout SQP
// iterations", KLV Sec. 2.4.1) -- and here that is a fact rather than an
// assumption, because the subproblem's box is l - x .. u - x, so every iterate
// the driver PRODUCES is inside the bounds by construction (see this header's
// feasibility note). A caller-supplied x0 can violate a bound; the driver's
// own `feasibility` measure reports that honestly and this one does not, which
// is the intended split of labour: `feasibility` is the CONVERGENCE test
// (inf-norm, all constraints including bounds), h is the GLOBALIZATION measure
// (l1, general constraints only, per KLV).
//
// NOT FINITENESS-GUARDED, on purpose: a NaN or infinite constraint value
// propagates into the sum and out to the caller, where FunnelStrategy::judge
// rejects the trial outright. Swallowing it here would hand the funnel a
// finite h at a point where nothing was measured.
inline double constraint_violation_l1(const NlpEval &ev) {
    double h = 0.0;
    for (Index i = 0; i < ev.ce.size(); ++i) {
        h += std::abs(ev.ce(i));
    }
    for (Index j = 0; j < ev.ci.size(); ++j) {
        // NOT std::max(0.0, v): std::max returns its FIRST argument when the
        // comparison is false, so max(0.0, NaN) is 0.0 and a NaN inequality row
        // would be silently dropped -- the exact swallowing this function is
        // documented not to do. Spelled out so the NaN survives to judge().
        const double v = ev.ci(j);
        h += (v > 0.0 || std::isnan(v)) ? v : 0.0;
    }
    return h;
}

// The KKT measurement of one NLP iterate. See this header's CONVERGENCE TEST
// note for the definition of every field; `residual()` is the single scalar
// the convergence test and the contraction test both read.
//
// When `finite` is false, stationarity/feasibility/complementarity/residual()
// are all NaN by construction -- see evaluate_kkt.
struct SqpKkt {
    double stationarity = 0.0;
    double feasibility = 0.0;
    double complementarity = 0.0;
    Vec grad_lag; // grad f + Je^T lambda_e + Ji^T lambda_i (bound term NOT subtracted)
    Vec z;        // model-implied bound multipliers: grad_lag at an active bound, else 0

    // False if x, or anything the model returned at x, or the resulting
    // grad_lag, is NaN or infinite. A caller MUST branch on this before
    // reading the residuals as numbers.
    bool finite = true;

    double residual() const { return std::max(stationarity, feasibility); }
};

// Measures (x, lambda_e, lambda_i) against the NLP, reusing an NlpEval taken
// at the SAME x. bound_tol is the geometric bound-activity tolerance (the
// driver passes SqpOptions::feas_tol; see the header note for why those are
// the same number).
//
// NON-FINITE ITERATES ARE NOT SILENTLY CLEAN, and getting this wrong is a
// wrong-answer bug rather than a robustness nicety. Both measures are folded
// out of per-entry terms by a running maximum, and IEEE says
// std::max(a, NaN) == a (the comparison a < NaN is false, so max returns its
// first argument) -- likewise Eigen's maxCoeff-based lpNorm<Infinity>(). A
// NaN gradient or constraint value is therefore SWALLOWED entry by entry,
// both measures read 0.0, and `stationarity <= kkt_tol` fires on a point at
// which nothing was measured at all. Measured before this guard existed: a
// 1-D model that returns NaN outside |x| <= 1e6 was walked out of its domain
// by one legitimate full Newton step and the resulting iterate was certified
// kOptimal with f = nan; and a caller-supplied NaN x0 was certified kOptimal
// in zero majors.
//
// So finiteness is checked EXPLICITLY, up front, and a non-finite point
// yields finite == false with every residual set to NaN -- NaN specifically,
// so that any `<= tol` gate written by any caller is false rather than
// accidentally true. SqpDriver routes this to kNumericalError.
//
// TWO ENTRIES, ONE BODY. The note above describes both the NlpModel-taking
// entry just below and the seam-taking one further down (past this header's
// positional include of detail/drivers/aggregate_eval_seam.h, which is where
// AggregateEvalSeam becomes a name). The arithmetic itself is
// detail::evaluate_kkt_over.
namespace detail {

// THE ARITHMETIC OF evaluate_kkt, over the five quantities it actually reads
// off its first argument: the three dimensions and the two bound vectors. It
// evaluates NOTHING -- every model quantity it uses arrives in `ev`.
//
// Factored out where the SQP driver's solve path moved onto the Level 2
// aggregate contract (detail/drivers/aggregate_eval_seam.h): the loop no longer
// holds an NlpModel, and the seam publishes exactly these five. Both public
// entries below forward here, so there is ONE body rather than two copies that
// could drift -- and the driver's own measurement is the same arithmetic, in
// the same order, on the same values as before the move (the seam's bounds are
// the model's own, materialized record by record).
//
// The diagnostics still name `model.n()`/`model.me()`/`model.mi()` because that
// is where the numbers come from in either direction, and because the messages
// are what callers read.
inline SqpKkt evaluate_kkt_over(Index n, Index me, Index mi, const Vec &lo, const Vec &up,
                                const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                                const Vec &lambda_i, double bound_tol) {
    if (x.size() != n) {
        throw std::invalid_argument(
            fmt::format("evaluate_kkt: x has size {}, expected {} (= model.n())", x.size(), n));
    }
    if (lambda_e.size() != me || lambda_i.size() != mi) {
        throw std::invalid_argument(
            fmt::format("evaluate_kkt: multipliers sized ({}, {}), expected ({}, {})",
                        lambda_e.size(), lambda_i.size(), me, mi));
    }

    SqpKkt out;
    out.grad_lag = ev.grad;
    if (me > 0) {
        out.grad_lag += ev.Je.transpose() * lambda_e;
    }
    if (mi > 0) {
        out.grad_lag += ev.Ji.transpose() * lambda_i;
    }
    out.z = Vec::Zero(n);

    out.finite = ev.all_finite && x.allFinite() && out.grad_lag.allFinite();
    if (!out.finite) {
        const double nan = std::numeric_limits<double>::quiet_NaN();
        out.stationarity = nan;
        out.feasibility = nan;
        out.complementarity = nan;
        return out;
    }

    for (Index i = 0; i < n; ++i) {
        const bool at_lower = (x(i) - lo(i)) <= bound_tol;
        const bool at_upper = (up(i) - x(i)) <= bound_tol;
        const double g = out.grad_lag(i);
        double s = 0.0;
        if (at_lower && at_upper) {
            s = 0.0; // fixed (or a degenerate/crossed box): nothing to certify
            out.z(i) = g;
        } else if (at_lower) {
            s = std::max(0.0, -g); // z(i) = g must be >= 0
            out.z(i) = g;
        } else if (at_upper) {
            s = std::max(0.0, g); // z(i) = g must be <= 0
            out.z(i) = g;
        } else {
            s = std::abs(g);
        }
        out.stationarity = std::max(out.stationarity, s);
    }

    if (me > 0) {
        out.feasibility = std::max(out.feasibility, ev.ce.lpNorm<Eigen::Infinity>());
    }
    for (Index j = 0; j < mi; ++j) {
        out.feasibility = std::max(out.feasibility, std::max(0.0, ev.ci(j)));
        out.complementarity = std::max(out.complementarity, std::abs(lambda_i(j) * ev.ci(j)));
    }
    for (Index i = 0; i < n; ++i) {
        out.feasibility = std::max(out.feasibility, std::max(0.0, lo(i) - x(i)));
        out.feasibility = std::max(out.feasibility, std::max(0.0, x(i) - up(i)));
    }
    return out;
}

} // namespace detail

inline SqpKkt evaluate_kkt(const NlpModel &model, const NlpEval &ev, const Vec &x,
                           const Vec &lambda_e, const Vec &lambda_i, double bound_tol) {
    return detail::evaluate_kkt_over(model.n(), model.me(), model.mi(), model.lower(),
                                     model.upper(), ev, x, lambda_e, lambda_i, bound_tol);
}

// Convenience overload that takes the evaluation itself. Costs one extra
// model evaluation over the bundle-taking form above, so the DRIVER never
// uses it -- it is for callers measuring a single point (tests, diagnostics).
inline SqpKkt evaluate_kkt(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                           const Vec &lambda_i, double bound_tol) {
    return evaluate_kkt(model, eval_nlp(model, x), x, lambda_e, lambda_i, bound_tol);
}

// The SQP subproblem at x, in the STEP variable p:
//
//     min   (obj_scale * grad f)^T p + 1/2 p^T W p
//     s.t.  Je p  = -cE(x)
//           Ji p <= -cI(x)
//           l - x <= p <= u - x
//
// with W = eval_hess(x, obj_scale, lambda_e, lambda_i), the exact Lagrangian
// Hessian. Because nlp_model.h's sign convention is qp_problem.h's verbatim,
// the returned QpSolution's lambda_e/lambda_i ARE the NLP's multipliers at
// this iterate with no flip -- that identity is the whole reason this
// construction is a one-liner per block.
//
// obj_scale scales BOTH the gradient and (through eval_hess) the objective
// part of the Hessian, so it scales the objective of the model problem
// consistently. Task 4 always passes 1.0; the parameter exists because Tasks
// 8-9's elastic/merit scaling reuse this helper.
//
// NO TRUST REGION IS BAKED IN. The radius is a per-solve engine override
// (SolveOverrides::tr_radius), never a tightening of `lower`/`upper` here, so
// the returned QpProblem is the pure linearization and a caller retrying at a
// different radius rebuilds nothing.
//
// `ev` must have been produced by eval_nlp at THIS x -- the whole point of
// the bundle is that the five derivative quantities are shared with the
// convergence test rather than recomputed (see NlpEval). eval_hess is the one
// call this makes itself. The caller is responsible for not handing a
// non-finite `ev` through: SqpDriver checks SqpKkt::finite and stops before
// ever reaching here.
//
// THAT PRECONDITION HOLDS FOR THE SECOND CALLER TOO (Phase-5 Task 0's
// zero-major probe in make_warm_start, which builds a throwaway subproblem
// purely to hash its sparsity). It is not exempt from the rule and does not
// rely on the hash being pattern-only to dodge it: the probe is SKIPPED
// entirely on the one exit whose `ev` can be non-finite -- the unevaluable
// start point -- so every subproblem this function is asked to build, hashed
// or solved, still comes from an evaluation SqpDriver already found finite.
// See make_warm_start's THE UNEVALUABLE EXIT note.
inline QpProblem build_subproblem(const NlpModel &model, const NlpEval &ev, const Vec &x,
                                  const Vec &lambda_e, const Vec &lambda_i,
                                  double obj_scale = 1.0) {
    QpProblem qp;
    qp.H = model.eval_hess(x, obj_scale, lambda_e, lambda_i);
    qp.H.makeCompressed();
    qp.g = obj_scale * ev.grad;

    qp.Ae = ev.Je;
    qp.Ae.makeCompressed();
    qp.be = -ev.ce;

    qp.Ai = ev.Ji;
    qp.Ai.makeCompressed();
    qp.bi = -ev.ci;

    qp.lower = model.lower() - x;
    qp.upper = model.upper() - x;
    return qp;
}

// Convenience overload that evaluates the model itself. Costs one extra model
// evaluation over the bundle-taking form, so the DRIVER never uses it.
inline QpProblem build_subproblem(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                                  const Vec &lambda_i, double obj_scale = 1.0) {
    return build_subproblem(model, eval_nlp(model, x), x, lambda_e, lambda_i, obj_scale);
}

// delta_m_f(p) of KLV Eq. (6b) -- the QP MODEL's predicted objective DECREASE
// along the step p, positive for a model that promises progress:
//
//     pred_df = -( g^T p + 1/2 p^T W p )
//
// with g and W the SUBPROBLEM'S OWN objective data at the CURRENT iterate
// (build_subproblem's qp.g and qp.H), never the model's raw gradient and never
// a Hessian taken anywhere else. H is read through selfadjointView<Upper>,
// matching qp_problem.h's storage convention.
//
// THIS IS A MODEL QUANTITY, NOT AN OBSERVED ONE. It is emphatically NOT the
// actual objective difference f(x) - f(x + p): the two routinely disagree in
// SIGN (that disagreement is what a rejection IS), and substituting either the
// actual difference or the negation of the expression above is silent. The
// negation in particular makes pred_df <= 0 on every healthy step, so KLV
// Eq. (10)'s switching condition pred_df >= delta*h^2 fails everywhere, every
// trial is classified h-type, and the Armijo condition Eq. (11) -- the one KLV
// Thm. 1 case 2 sums to prove convergence -- is never tested at all. The
// method would still "work" on many problems, which is exactly why the formula
// is pinned by a hand-computed unit test
// (SqpDriverTrustRegion.PredictedDecreaseIsTheQpModelDecrease) rather than left
// to integration behaviour.
//
// Sign convention check, for the reader: for an unconstrained convex model the
// QP's own solution p* = -W^{-1} g gives pred_df = 1/2 g^T W^{-1} g >= 0.
inline double predicted_decrease(const QpProblem &qp, const Vec &p) {
    if (p.size() != qp.n()) {
        throw std::invalid_argument(fmt::format(
            "predicted_decrease: p has size {}, expected {} (= qp.n())", p.size(), qp.n()));
    }
    const double linear = qp.g.dot(p);
    const double quadratic = p.dot(qp.H.template selfadjointView<Eigen::Upper>() * p);
    return -(linear + 0.5 * quadratic);
}

// THE SECOND-ORDER CORRECTION SUBPROBLEM (Task 7). See this header's
// THE CRASH BASIS (Phase-6 Task 4, SqpOptions::crash_basis -- read that
// field's note first; it carries the whole justification and the measured
// outcome, and this comment covers only the mechanics).
//
// Builds a QpSolution to hand `qp_engine.h` as a warm-start SEED for a COLD
// solve's FIRST subproblem: an estimate of the active set read off the
// activity geometry at x0, so the engine starts from that working set
// instead of rediscovering it one ratio test at a time.
//
// EVERYTHING IT NEEDS IS ALREADY IN `qp`, WHICH IS WHY IT COSTS NO MODEL
// EVALUATION. build_subproblem (above) sets `qp.bi = -cI(x)` and
// `qp.lower/qp.upper = model.lower()/upper() - x`, and the subproblem's own
// start point is the step p = 0, so at that point
//
//     row j's slack       =  bi(j)                =  -cI_j(x)
//     distance to l(i)    =  -qp.lower(i)         =  x(i) - l(i)
//     distance to u(i)    =   qp.upper(i)         =  u(i) - x(i)
//
// and the three geometric-activity tests SqpOptions::crash_basis names --
// `cI_j(x) >= -feas_tol`, `x(i) - l(i) <= feas_tol`, `u(i) - x(i) <=
// feas_tol` -- are exactly the three comparisons below. They are the SAME
// tests, at the SAME tolerance, evaluate_kkt already applies (this header's
// CONVERGENCE TEST note and its INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY
// note); no new tolerance is introduced anywhere.
//
// `seed.x` IS ZEROED, exactly as the kWarm ingest's own seed is (see
// solve_impl's kWarm SEEDS THE ENGINE'S WORKING SET note): the subproblem's
// trust region centers on p = 0 and the crash basis is a claim about the
// working SET, never about a primal point. The engine then materializes each
// seeded bound onto its own bound value, subject to its WINDOW-CONSISTENCY
// RULE.
//
// A BOUND IS SEEDED ONLY IF IT IS FINITE. qp.lower/qp.upper carry +/-inf (or
// the +/-1e20 convention) for an unbounded side, and `-inf >= -feas_tol` is
// false while `+inf <= feas_tol` is false, so the infinite sides fall out of
// the comparisons on their own -- no separate infinity test is needed, and
// an explicit one would be a second place for the convention to drift.
//
// Returns true iff it seeded anything at all; `rows`/`bounds` receive the
// two counts (SqpCounters::crash_seeded_rows/crash_seeded_bounds). A seed
// that names nothing is NOT offered to the engine by the caller -- passing an
// all-free, no-row seed is observationally identical to passing none, and not
// offering it keeps the cold path's `engine_.solve(qp, overrides)` call
// literally unchanged wherever the lever finds nothing, which is what makes
// the off-default byte-identity claim cheap to believe.
inline bool crash_basis_seed(const QpProblem &qp, double feas_tol, QpSolution &seed, Index &rows,
                             Index &bounds) {
    const Index n = qp.n();
    const Index mi = qp.mi();
    rows = 0;
    bounds = 0;

    seed.x = Vec::Zero(n);
    seed.bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
    seed.ineq_active.assign(static_cast<std::size_t>(mi), false);

    for (Index j = 0; j < mi; ++j) {
        if (qp.bi(j) <= feas_tol) { // cI_j(x0) >= -feas_tol
            seed.ineq_active[static_cast<std::size_t>(j)] = true;
            ++rows;
        }
    }
    for (Index i = 0; i < n; ++i) {
        // kAtLower wins a variable that satisfies both tests -- see
        // SqpOptions::crash_basis for why that choice is arbitrary and
        // recorded rather than derived.
        if (qp.lower(i) >= -feas_tol) { // x0(i) - l(i) <= feas_tol
            seed.bound_state[static_cast<std::size_t>(i)] = BoundState::kAtLower;
            ++bounds;
        } else if (qp.upper(i) <= feas_tol) { // u(i) - x0(i) <= feas_tol
            seed.bound_state[static_cast<std::size_t>(i)] = BoundState::kAtUpper;
            ++bounds;
        }
    }
    return rows > 0 || bounds > 0;
}

} // namespace hven::solvers

// THE SECOND-ORDER CORRECTION, ELASTIC TIER, and RESTORATION PHASE
// constructions were carved verbatim to detail/globalization/sqp/ (phase-C
// S3). They are included HERE, at the exact point the code stood, rather than
// in the top include block, because build_soc_subproblem and RestorationModel
// consume NlpEval (defined above) and their headers must not include this one
// back -- the positional include preserves the original definition order with
// no header cycle. The top-of-file notes those constructions cite
// (SECOND-ORDER CORRECTION, ELASTIC TIER, RESTORATION PHASE) remain in this
// header, where the carved comments still point.
//
// THE EVALUATION SEAM JOINS THEM ON THE SAME FOOTING, and for the same reason:
// detail/drivers/aggregate_eval_seam.h reproduces the evaluation moments
// declared above against an NlpAggregate, so it consumes NlpEval and states in
// its own banner that it is deliberately not self-contained. It is what the
// driver's solve path evaluates through.
#include <hven/detail/drivers/aggregate_eval_seam.h>
#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/globalization/sqp/restoration.h>
#include <hven/detail/globalization/sqp/soc.h>

namespace hven::solvers {

// evaluate_kkt over the SEAM's dimensions and materialized box, which are the
// model's own (model/nlp_model_aggregate.h lays one bound record per variable
// verbatim, and model/aggregate_declaration.h's materialization intersects a
// single record with (-inf, +inf), returning it). This is the entry the
// driver's major loop uses; the NlpModel-taking one above is for every caller
// measuring a single point from a model in hand. See that entry's note -- it
// documents both.
//
// The seam is taken by CONST reference: this measurement evaluates nothing, so
// it neither re-lays nor scatters.
inline SqpKkt evaluate_kkt(const AggregateEvalSeam &seam, const NlpEval &ev, const Vec &x,
                           const Vec &lambda_e, const Vec &lambda_i, double bound_tol) {
    return detail::evaluate_kkt_over(seam.n(), seam.me(), seam.mi(), seam.lower(), seam.upper(), ev,
                                     x, lambda_e, lambda_i, bound_tol);
}

// Is a FAILED subproblem's result usable as a rejected trial, i.e. may the
// driver shrink the radius and re-solve instead of giving up? See this
// header's SUBPROBLEM FAILURE ROUTING note for the algorithm; this is the
// whole of the decision, factored out so it can be tested away from any
// engine (SqpDriverQpFailure.RetryabilityIsFinitenessAndTheBox).
//
// THE STATUS ARM. kOptimal is not a failure. kInfeasible is a failure that
// shrinking cannot fix and must not be retried: a smaller radius only REMOVES
// candidate points from a linearization that already has none, so the retry is
// guaranteed to fail. FROM TASK 8 THAT STATUS NEVER ARRIVES HERE AT ALL --
// the elastic tier consumes it upstream and asks a DIFFERENT question at the
// SAME radius (see the header's ELASTIC TIER note) -- but the arm is kept,
// both because this predicate is a free function any caller may use and
// because "shrinking cannot fix it" remains true and is the reason the
// elastic tier exists. That leaves kNumericalError and kMaxIter, the two
// statuses the engine returns WITH an iterate.
//
// THE ITERATE ARM. `qs.x` must be finite and inside the subproblem's own box.
// This is not a feasibility test of the step -- the step is discarded either
// way, and its general rows may well be violated (the engine's homotopy shifts
// them deliberately). It is a test of whether ANYTHING came back: the engine
// holds every iterate inside `lower .. upper` by construction, so a
// non-finite or out-of-box `x` says the linear algebra produced a point the
// engine's own arithmetic does not vouch for -- and the RETRY IS SEEDED FROM
// THAT SOLUTION'S ACTIVE SET, so a labeling attached to a point that does not
// exist would be seeded into the next solve.
//
// `bound_tol` is the caller's bound-activity tolerance (SqpDriver passes
// SqpOptions::feas_tol), applied as a band OUTSIDE each bound so that an
// iterate sitting exactly on one, or a rounding step past it, still counts.
inline bool qp_failure_is_retryable(const QpProblem &qp, const QpSolution &qs, double bound_tol) {
    switch (qs.status) {
    case QpStatus::kNumericalError:
    case QpStatus::kMaxIter:
        break;
    case QpStatus::kOptimal:
    case QpStatus::kInfeasible:
        return false;
    }
    if (qs.x.size() != qp.n() || !qs.x.allFinite()) {
        return false;
    }
    for (Index i = 0; i < qp.n(); ++i) {
        if (qs.x(i) < qp.lower(i) - bound_tol || qs.x(i) > qp.upper(i) + bound_tol) {
            return false;
        }
    }
    return true;
}

// =============================================================================
// PHASE-7 TASK 5: THE SEMISMOOTH-NEWTON TIER, AND WHAT THE DRIVER DOES WITH IT
// =============================================================================
//
// SqpOptions::qp_mode selects which kernel solves each subproblem. kWalk is
// the shipped default and is byte-for-byte what this driver has always done.
// kSsn routes the MAIN subproblem of every major through ssn_engine.h and
// falls back to the walk whenever that kernel does not hand back a usable
// step. Everything in this section is the fall-back's contract.
//
// --- THE ROUTING RULE, IN ONE SENTENCE -------------------------------------
//
// EVERY SSN ESCAPE ROUTES TO THE WALK, ALL FIVE OF THEM, IDENTICALLY, ONCE.
//
// That is ssn_engine.h's own standing instruction (its SsnEscape banner: "Task
// 5 routes an escaped subproblem back to the walk; that routing is the only
// correct consumption of any value below"), and it is a rule with three
// separate justifications, each of which would be sufficient on its own:
//
//   (1) NO ESCAPE CERTIFIES ANYTHING. `SsnEscape::kInfeasibleSuspect` is a
//       DIAGNOSIS FROM BEHAVIOUR (a stalled residual while the multiplier norm
//       grows), never a Farkas certificate -- but it reports
//       `QpStatus::kInfeasible`, which the WALK issues as a certificate and
//       which this driver's elastic tier consumes as one. A driver that
//       branched on the status rather than on the escape would promote a
//       suspicion to a certificate at the driver layer, which is exactly the
//       defect ssn_engine.h's banner warns against by name.
//   (2) THE LABEL IS NOT RELIABLE ENOUGH TO BRANCH ON. Task 4's fix round 1
//       traded infeasibility RECALL for precision (99.74% -> 56.47%), so
//       genuinely infeasible subproblems now escape `kNoContraction` or
//       `kBudget` more often than `kInfeasibleSuspect`: 4 of that task's 5
//       infeasible fixtures do (docs/notes/2026-08-07-ssn-safeguards.md
//       13.7). A rule keyed on `kInfeasibleSuspect` would therefore MISS most
//       infeasible subproblems while ALSO mis-routing the 0.55% false
//       positives, which is the worst of both.
//   (3) UNIFORM ROUTING STILL REACHES THE ELASTIC TIER, and reaches it with
//       better evidence. An infeasible linearization handed to the walk comes
//       back `QpStatus::kInfeasible` -- the walk's own certificate -- and the
//       elastic tier fires on it exactly as it always has. So the destination
//       is unchanged; only the authority for the claim improves.
//
// ONE HAND-OFF PER QP, THEN THE WALK OWNS IT. There is no ping-pong and no
// second SSN attempt: after a hand-off the subproblem is the walk's, including
// its elastic ladder, its second-order correction and its own failure routing.
//
// --- WHY AN ESCAPED x IS NOT A STEP ----------------------------------------
//
// ssn_engine.h's trust region is a SOFT constraint (it enters as FB bound
// rows, satisfied only at a root), so an escaped iterate can sit anywhere:
// MEASURED at 133x to 160x the radius on `cycling_qp_3var` at
// `tr_radius = 0.01` (that header's `SsnResult::tr_violation` note). The
// funnel's ratio test presumes ||p||inf <= Delta. Feeding it an escaped x
// would break that presumption silently, so the escaped iterates are DISCARDED
// -- not clamped, not repaired, not re-used as a seed -- and the walk re-solves
// the same subproblem from the same seed the walk would have had anyway.
//
// --- WHY THE ADAPTIVE-mu SCHEDULE IS OFF UNDER kSsn ------------------------
//
// SqpOptions::adaptive_mu exists because the WALK's returned solution carries
// an irreducible `dual_mu * |lambda|` footprint (this header's ADAPTIVE DUAL
// REGULARIZATION note). The SSN kernel has no such footprint: delta and mu
// perturb only its JACOBIAN and never its residual, so the iteration is
// modified Newton on the EXACT F and its fixed points are exact, unregularized
// KKT points. There they cost iterations and buy nothing -- ssn_engine.h's own
// per-solve seam says so in those words and asks Task 5 to switch the schedule
// off DELIBERATELY rather than discover it. So it is off for the WHOLE solve
// under kSsn, walk fall-backs included, and `SqpIterate::mu` reports
// `opts_.qp.dual_mu` on every row of such a solve -- which is the truth about
// what every kernel on that solve was actually handed. At kWalk not one byte
// of the schedule changes.
//
// --- TIER 3: THE STABLE-FACE REFINEMENT ON A CERTIFYING EXIT ----------------
//
// A CERTIFYING SSN EXIT IS NOT THE END OF THE SUBPROBLEM. Every one of them is
// handed to QpEngine::refine_on_face for ONE EXACT equality-constrained solve
// on the face the kernel identified -- the tier-3 crossover of
// docs/superpowers/specs/2026-08-05-phase-7-design.md, reusing the walk's own
// solve_eqp rather than rebuilding it, and never re-entering the walk's search.
//
// IT IS NOT A POLISH STEP; it is what makes this driver's convergence test
// sound in this mode. The WHAT IS MEASURED BUT NOT GATED note at the top of
// this file declines to gate NLP complementarity on an identity that belongs to
// an ACTIVE-SET solve and to nothing else; an FB kernel stopping at
// |phi| <= fb_tol supplies only `min(s, lambda) = O(fb_tol)`, whose product
// form carries an additive `fb_tol * ||lambda||inf` that does not vanish with
// the step. Measured, before this tier existed: complementarity 6.311e-01 on a
// row with ||lambda||inf = 1e6, against the walk's 1.0e-04 on the identical
// fixture; with it, 1.0000e-04 -- the walk's own value to five figures. That
// note now states the per-mode guarantee, and this is the mechanism behind its
// kSsn half.
//
// IT CAN BE REFUSED, and a refusal is not an error: the caller keeps the
// certificate the SSN tier already gave it, exactly as it did before this tier
// existed. What it means is that THAT subproblem is back on the fb_tol bound,
// and SqpCounters::ssn::ssn_refine_refused is how a reader knows how often.
// refine_on_face's own contract lists the four refusal causes; each attempt
// costs one factorization whether or not it is used, folded into
// SqpCounters::factorizations like every other.
//
// --- WHAT IS NOT ROUTED THROUGH SSN, AND WHY --------------------------------
//
// Only the MAIN subproblem dispatches on qp_mode. The SECOND-ORDER CORRECTION
// re-solve, the ELASTIC ladder's rungs and the RESTORATION phase's own
// sub-solve all stay on the walk unconditionally. Each is a rescue path
// reached only after something already went wrong, and none of them is where
// the phase's cost law lives. Widening the dispatch to them is Task-6 material
// and wants its own measurement, not a guess here.
//
// HOW EACH ONE IS ACTUALLY KEPT THERE IS NOT THE SAME MECHANISM, and fix round
// 1 had to correct this paragraph on both counts (Task-5 review, C-1 and I-1 --
// it used to say all three were "hot-started off the immediately preceding WALK
// solve", which is false for two of them):
//
//   ELASTIC RUNGS are structurally walk-only: the ladder is entered on a
//     QpStatus::kInfeasible CERTIFICATE, which under kSsn can only have come
//     from the walk after a hand-off, and every rung calls engine_.solve
//     directly. The rung chaining does hot-start off the immediately preceding
//     WALK solve (this header's ELASTIC TIER note), so for this one the old
//     sentence was true.
//
//   THE SECOND-ORDER CORRECTION is structurally walk-only in its SOLVER and is
//     SEEDED FROM THE SSN's OWN SOLUTION -- `seed_soc = qs`, where `qs` may be
//     ssn_result_to_qp_solution's export. That is sound, and deliberately so:
//     the walk's seed ingest consumes exactly bound_state / ineq_active /
//     duals, and both kernels already agree on the two conventions that matter
//     (a TR pin reports kFree plus tr_active; a real lower == upper reports
//     kFixed). SO THERE IS NO CAPABILITY GAP TO DISCLOSE: SOC is available in
//     BOTH modes, which is what makes a mode comparison fair. Measured: HS77
//     takes 2 SOC re-solves under kSsn with 0 escapes, i.e. every seed came
//     from the SSN (tests/test_sqp_driver.cpp,
//     TheSecondOrderCorrectionIsReachableUnderKSsnFromAnSsnSeed).
//
//   THE RESTORATION SUB-SOLVE is a whole nested SqpDriver, so nothing about it
//     is structural: it runs whatever `qp_mode` its SqpOptions carry, and until
//     fix round 1 those were the caller's, copied wholesale. It is now reset to
//     kWalk EXPLICITLY at the point the sub-options are built, and the reset is
//     MEASURED rather than merely documented -- the sub-solve's own
//     `counters.ssn` is folded so that letting the mode leak back in moves a
//     pinned number (RestorationStaysOnTheWalkUnderKSsn).
//
// THE CRASH BASIS likewise stays a walk-only mechanism (SqpOptions::
// crash_basis): it builds a QpSolution seed, which is what the walk consumes.
// Under kSsn it is still derived and still counted -- the counters' own
// documented convention is SEEDS OFFERED, not seeds honoured -- and it is
// consumed if and only if the subproblem hands off.

// The largest `SsnResult::tr_violation` a CERTIFYING exit may report and still
// be handed to the funnel as a step, as a multiple of the SSN's own `fb_tol`.
//
// WHERE IT COMES FROM: ssn_engine.h derives the bound exactly --
// `|phi| <= fb_tol` permits a slack negative by O(fb_tol), so a certifying
// exit satisfies `tr_violation <= detail::kSsnComplementarityFactor * fb_tol`,
// which is ~1.71 fb_tol. The factor 2 below is that derived bound with one
// binary order of head-room for the floating-point arithmetic that produced
// it; it is NOT an independent tolerance and must not be tuned. Anything
// larger than this is, by the header's own contract, not a certifying exit,
// and is routed to the walk with the escapes.
//
// **PROVABLY INERT ON THE CERTIFYING PATH, AND DELIBERATELY KEPT.** No
// certifying exit ssn_engine.h can produce reaches this gate -- that is what
// the derivation above says -- so no NLP fixture can drive it, and no mutation
// of the driver loop can be killed by one. It is kept because it is the single
// point at which a step whose norm the funnel has not been told about could
// enter the acceptance test, and because ssn_engine.h states that the field
// exists precisely so that Task 5 can ASSERT it routed rather than assume it.
// It is made FIXTURABLE by being reachable through the free predicate below,
// which tests/test_sqp_driver.cpp drives in both polarities on a hand-built
// SsnResult.
// DERIVED FROM ssn_engine.h's OWN CONSTANT, never restated as a literal: this
// is `2 * detail::kSsnComplementarityFactor`, so a change to the tolerance
// derivation there moves this gate with it rather than leaving a stale copy
// behind. (`const double` rather than `constexpr` because the constant it is
// built from is itself a runtime-initialized `std::sqrt` expression.)
inline const double kSsnTrViolationFactor = 2.0 * detail::kSsnComplementarityFactor;

// Does this SSN exit hand back something the funnel may use as a trial step?
//
// THREE CONJUNCTS, and each one is a separate way for the answer to be no:
//   (a) the escape reason is kNone -- the routing rule above, applied to all
//       five escapes at once rather than to a list this function would have to
//       be kept in sync with as ssn_engine.h grows one;
//   (b) the status is kOptimal -- belt to (a)'s braces, and the conjunct that
//       would catch a future exit that forgot to set an escape;
//   (c) the point is finite and inside the trust region to within the
//       certifying exit's own derived bound (kSsnTrViolationFactor above).
//
// `fb_tol` is the `SsnOptions::fb_tol` the solve ran at, which is what the
// bound in (c) is stated relative to.
inline bool ssn_exit_is_a_usable_step(const SsnResult &res, double fb_tol) {
    if (res.escape_reason != SsnEscape::kNone || res.status != QpStatus::kOptimal) {
        return false;
    }
    if (res.x.size() == 0 || !res.x.allFinite()) {
        return false;
    }
    return res.tr_violation <= kSsnTrViolationFactor * fb_tol;
}

// An SSN exit, in the shape every consumer downstream of a subproblem already
// speaks. CALLED ONLY ON A CERTIFYING EXIT (ssn_exit_is_a_usable_step above
// true); an escaped result never reaches here, which is why the status below
// can be written as the constant it is rather than mapped.
//
// THE COUNTER MAPPING IS THE ONE JUDGEMENT IN THIS FUNCTION, and it is
// deliberately PARTIAL:
//
//   - `factorizations` and `symbolic_analyses` ARE the same physical quantity
//     in both kernels -- sparse numeric factorizations paid, Pardiso phase-11
//     analyses paid -- so they carry across and keep aggregating into the
//     driver's own totals as they always have. A kSsn solve's
//     `SqpCounters::factorizations` is therefore still the honest total cost.
//   - `minor_iters` DOES NOT. A walk minor is one working-set change; an SSN
//     iteration is a Newton step that flips the whole implied set at once.
//     They are not the same currency, and `SqpCounters::qp_minor_iters` is the
//     headline figure every published measurement in this repository is quoted
//     in (CLAUDE.md; docs/notes/2026-08-05-phase-6-results.md). Folding SSN
//     steps into it would silently corrupt every one of those comparisons. So
//     an SSN-solved subproblem contributes ZERO to qp_minor_iters, and its
//     work is reported in `SqpCounters::ssn` instead. A kSsn solve's
//     qp_minor_iters is thus exactly "the minors the WALK spent on this
//     solve", which on a solve with no hand-offs is 0 -- and that 0 is a
//     reading, not a gap.
//   - `schur_updates`, `eqp_refine_steps`, `border_refine_steps`,
//     `suspect_escalations` and `k0_reused` are walk mechanisms with no SSN
//     analogue at all and stay at 0.
//
// THE ACTIVITY EXPORT carries across verbatim, which is sound because both
// kernels already agree on the two conventions that matter: a TR-pinned
// variable reports kFree in `bound_state` with z = 0 and is flagged in
// `tr_active` (so a radius artefact can never be re-ingested as a genuine
// active bound), and a variable at a real lower == upper reports kFixed.
inline QpSolution ssn_result_to_qp_solution(const SsnResult &res) {
    QpSolution qs;
    qs.status = QpStatus::kOptimal;
    qs.x = res.x;
    qs.lambda_e = res.lambda_e;
    qs.lambda_i = res.lambda_i;
    qs.z = res.z;
    qs.bound_state = res.bound_state;
    qs.ineq_active = res.ineq_active;
    qs.tr_active = res.tr_active;
    qs.counters.factorizations = res.factorizations;
    qs.counters.symbolic_analyses = res.symbolic_analyses;
    return qs;
}

// The SSN start point for ONE subproblem, built from the walk's own seed
// object -- the SAME QpSolution the walk would have been handed, so the two
// kernels are warm-started off identical information by construction and a
// mode comparison is not confounded by a second seeding path.
//
// `seed` is null when this solve has no seed at all (a cold first subproblem),
// which yields a default-constructed SsnStart -- ssn_engine.h reads every
// empty vector as "zero of the right size", so that IS the cold start.
//
// x IS LEFT EMPTY (i.e. ZERO) EVEN WHEN A SEED EXISTS, and that is the same
// correctness requirement this header's WARM SEEDING note states for the walk:
// the subproblem is in STEP variables, the trust region is centred on the
// seed's own primal, and a remembered step from the previous major would move
// that centre off p = 0. `seed.x` is zeroed at every walk seeding site for
// exactly this reason; not reading it here is the same rule, stated once more.
//
// THE ACTIVITY HINT is the seed's binary activity, both halves. It can never
// carry a trust-region artefact: `QpSolution::bound_state` is a REAL-BOUND-ONLY
// view in both kernels (a TR-pinned variable reports kFree there and is flagged
// in `tr_active`, which this function does not read), so a pin created by a
// radius in one major cannot be asserted as a bound in the next.
//
// THE MULTIPLIERS come from the seed unchanged. Note what has ALREADY happened
// to them by the time any of this runs: on the FIRST subproblem the seed's
// duals are the solve's ingested duals, which the B-1 geometric clear and then
// the seeded dual clamp have already corrected, in that order (this header's
// THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY and THE SEEDED DUAL CLAMP
// notes, and the ordering is normative). This function is downstream of both
// and re-does neither.
inline SsnStart ssn_start_from_qp_seed(const QpSolution *seed) {
    SsnStart start;
    if (seed == nullptr) {
        return start;
    }
    start.lambda_e = seed->lambda_e;
    start.lambda_i = seed->lambda_i;
    start.z = seed->z;
    start.activity_hint.ineq = seed->ineq_active;
    start.activity_hint.bounds = seed->bound_state;
    return start;
}

// The FB residual tolerance one subproblem's SSN solve runs at, from the
// DRIVER's own two tolerances.
//
// IT IS THE TIGHTER OF THE TWO, and that is fix round 1's correction: this
// used to read `kkt_tol` alone. ssn_engine.h section 5 establishes that an FB
// residual at `t` buys stationarity and equality feasibility at `t` -- so a
// caller who asks for `feas_tol < kkt_tol` (a legitimate, documented
// configuration: the two are independent fields of SqpOptions) was getting a
// kernel whose own certificates could sit ~1.71 * kkt_tol outside the very
// feasibility bar the driver's convergence test then applied to them. The
// driver would refuse to converge at points the SSN kept re-certifying -- a
// DNF/stall asymmetry against kWalk, whose engine takes feas_tol directly. The
// 1e-10 arm never saw it because it tightens both together.
//
// THE OTHER DIRECTION IS ALREADY SAFE and is not changed: `feas_tol > kkt_tol`
// leaves the min at kkt_tol, which is what the stationarity bar needs.
//
// A FREE FUNCTION, so the rule is testable without a driver -- the same house
// pattern this file uses for the routing predicate above.
inline double ssn_fb_tol_for(double kkt_tol, double feas_tol) {
    return ssn_fb_tol_from_kkt_tol(std::min(kkt_tol, feas_tol));
}

// The COST an SSN subproblem paid, charged to the solve's own totals.
//
// TWO FIELDS AND ONLY TWO, and they are the two that mean the SAME PHYSICAL
// THING in both kernels: sparse numeric factorizations paid, and Pardiso
// phase-11 analyses paid. `minor_iters` is deliberately absent -- see
// ssn_result_to_qp_solution's counter-mapping note for why folding it would
// corrupt every published figure in this repository.
//
// WHY IT IS A FREE FUNCTION RATHER THAN TWO LINES AT ITS ONE CALL SITE. That
// site is the HAND-OFF, where `qs` becomes the WALK's solution and the escaped
// SSN attempt's cost therefore reaches no other accumulation path. Getting
// either field wrong there is silent -- the work simply vanishes, which is
// exactly the defect this function exists to close (see the call site) -- and
// the driver-scale fixtures that would notice happen to exercise
// `factorizations` far more sharply than `symbolic_analyses`. Stating the rule
// once, here, makes BOTH fields directly assertable
// (SqpDriverSsnMode.TheEscapedSubproblemsCostIsChargedInBothFields).
inline void charge_ssn_subproblem_cost(SqpCounters &total, const SsnResult &res) {
    total.factorizations += res.factorizations;
    total.symbolic_analyses += res.symbolic_analyses;
}

// R5's REFUSED-AND-THEN-WITHDRAWN face refinement, charged to the solve's own
// totals (Phase-B review fix round, finding F3). THE SAME VANISHING-WORK CLASS
// as the function above, on the one path the certifying branch cannot reach.
//
// WHY A SECOND SITE IS NEEDED AT ALL. Under
// SqpOptions::ssn_certify_from_face the tier-3 face solve is HOISTED ahead of
// the driver's usability gate, because it is now the thing that decides
// whether the certificate stands -- so its factorization is paid BEFORE the
// driver knows whether the exit will be used. On every path but one the
// certifying branch below consumes that solve and charges it (`refine_facts`
// there). The exception is the pair: the face solve REFUSED, and the deferred
// verification it then fell back to WITHDREW the certificate (kIndefinite or
// kSingular). `sres` becomes an escape, the usability gate that read true at
// the hoist now reads false, and the HAND-OFF branch runs instead -- where the
// refinement's factorization has no other accumulation site and was simply
// lost: not in SqpCounters::factorizations, not in `ssn_refine_factorizations`,
// not in `ssn_refine_refused`, and not in the probe budget it silently
// under-charged. Unreachable on every shipped corpus (0 withdrawals over the
// 27-problem HS battery and the 57 corpus cells), and INERT at the shipped
// default, where nothing is ever deferred and no face solve is ever hoisted.
//
// THE FOUR FIELDS ARE THE FOUR THE CERTIFYING BRANCH WRITES FOR A REFUSAL, for
// the same reasons and with the same meanings; `symbolic_analyses` is absent
// because QpEngine::refine_on_face reports only `factorizations` and
// `eqp_refine_steps` (its own COST AND STATE note), exactly as the certifying
// branch reads only those two. The probe budget is charged through the
// reference for the same reason `ssn_budget_charge`'s declaration gives: an
// escape's factorizations were spent either way.
//
// A FREE FUNCTION for the reason the one above is one -- so the rule is
// assertable without having to reproduce the rare dynamic that reaches it
// (SqpDriverSsnMode.ARefusedFaceRefinementIsChargedEvenWhenTheCertificateIsWithdrawn
// does reproduce it; this makes the rule itself falsifiable too).
inline void charge_refused_face_refinement(SqpCounters &total, const QpSolution &refined,
                                           Index &ssn_budget_charge) {
    total.factorizations += refined.counters.factorizations;
    total.eqp_refine_steps += refined.counters.eqp_refine_steps;
    total.ssn.ssn_refine_factorizations += refined.counters.factorizations;
    ++total.ssn.ssn_refine_refused;
    ssn_budget_charge += refined.counters.factorizations;
}

// One subproblem's SSN work, folded into a whole solve's running total.
//
// SEVEN FIELDS SUM AND ONE DOES NOT. `ssn_uncertain_peak` is a PEAK -- the
// largest uncertain set any single subproblem's Jacobian assembly held -- so
// it aggregates by max. Summing it would report a quantity with no meaning
// (the sum of peaks over subproblems is not the peak over the solve and is not
// a total of anything either).
//
// `ssn_escapes` sums, and at the driver scale it counts SUBPROBLEMS HANDED OFF
// TO THE WALK. That is one step wider than SsnResult's own reading ("this
// solve escaped"), because the driver has one refusal of its own -- the
// trust-region gate in ssn_exit_is_a_usable_step -- and a subproblem refused
// there went to the walk exactly like an escaped one did. The caller adds that
// case; this function only sums what the engine reported.
//
// `ssn_refinements`/`ssn_refine_refused` are DRIVER-SCALE ONLY: no SsnResult
// ever carries a nonzero one (the tier-3 refinement is a driver decision made
// after the kernel returned), so summing them here is inert on the
// per-subproblem call. They are summed anyway because this same function folds
// the RESTORATION sub-solve's totals, where they could be nonzero in principle
// -- see the restoration fold's own note for why they are zero in fact.
inline void accumulate_ssn_counters(SsnCounters &total, const SsnCounters &one) {
    total.ssn_iters += one.ssn_iters;
    total.ssn_bulk_flips += one.ssn_bulk_flips;
    total.ssn_backtracks += one.ssn_backtracks;
    total.ssn_prox_updates += one.ssn_prox_updates;
    total.ssn_escapes += one.ssn_escapes;
    total.ssn_refinements += one.ssn_refinements;
    total.ssn_refine_refused += one.ssn_refine_refused;
    // Task 6's two instrument counters are driver-scale for exactly the reason
    // the pair above is, and are folded here for exactly the same reason.
    total.ssn_refine_factorizations += one.ssn_refine_factorizations;
    total.ssn_refine_neg_duals += one.ssn_refine_neg_duals;
    // Phase-7 Task 6b's escape-reason census (docket D6). Five of the six sum
    // from the engine's own writes; `ssn_escape_gate_refused` is driver-scale
    // and is summed here for the same reason the refinement pair above is --
    // no SsnResult ever carries a nonzero one, so this line is inert on the
    // per-subproblem call and correct on the restoration fold.
    total.ssn_escape_budget += one.ssn_escape_budget;
    total.ssn_escape_singular += one.ssn_escape_singular;
    total.ssn_escape_no_contraction += one.ssn_escape_no_contraction;
    total.ssn_escape_infeasible_suspect += one.ssn_escape_infeasible_suspect;
    total.ssn_escape_indefinite += one.ssn_escape_indefinite;
    total.ssn_escape_gate_refused += one.ssn_escape_gate_refused;
    total.ssn_uncertain_peak = std::max(total.ssn_uncertain_peak, one.ssn_uncertain_peak);
}

// =============================================================================
// ADAPTIVE DUAL REGULARIZATION (Task 10). Caller-visible surface:
// SqpOptions::adaptive_mu (sqp_types.h) and SqpIterate::mu (same file); this
// is the mechanism behind both.
//
// THE PHASE-2 ACCURACY CEILING THIS ATTACKS. qp_engine.h's regularized KKT
// system carries an irreducible dual_mu*|lambda| footprint on every
// constraint row (see that header's row_tolerance/violation_is_structural
// notes), and eqp_solve.h's one step of iterative refinement only knocks the
// resulting error in x down by roughly the SQUARE of that footprint relative
// to the row's own conditioning -- on a badly scaled active set (a near-zero
// row coefficient amplifying its multiplier) the fixed engine default
// dual_mu = 1e-8 (QpOptions::dual_mu) still leaves a relative error around
// 1e-4 in the RETURNED x, well short of kkt_tol/feas_tol's default 1e-6. See
// AdaptiveMuRecoversTailAccuracy for the measured fixture and its fixed-mu
// contrast. Shrinking mu attacks the footprint directly (it is linear in mu),
// but shrinking it UNCONDITIONALLY is not free: a smaller mu is a
// worse-conditioned regularized system, which is a real hazard EARLY in a
// solve, far from whatever active set the iterate eventually settles on.
//
// THE SCHEDULE ties mu to how converged the driver already believes it is:
//     mu_k = clamp(kappa_mu * ||KKT residual||^1.5, mu_min, mu_max)
// with kappa_mu = 1 (kAdaptiveMuKappa), mu_min = 1e-12, mu_max = 1e-8 -- the
// LAST one being the engine's own QpOptions::dual_mu default, so an
// unconverged solve's early majors are byte-identical in behavior to the
// fixed-mu engine. "KKT residual" is `kkt.residual()` -- the SAME
// max(stationarity, feasibility) measure the convergence test and every
// history row already use -- READ AT THE ITERATE THIS TRIAL IS ABOUT TO BUILD
// A SUBPROBLEM FROM, i.e. the PREVIOUS major's own measurement: the loop
// computes `kkt` for the current iterate before reaching the subproblem code
// below, so that value already IS "the previous major's residual" by the time
// mu is chosen for THIS trial. A rejected retry re-measures the identical
// point and so gets the identical number, which is what keeps mu constant
// across a shrink-retry (see the quantization paragraph for why that
// matters). THE VERY FIRST major (iter == 0) has no previous major to read
// and uses mu_max unconditionally -- not the x0 residual, which would make
// the schedule's opening value depend on a point no subproblem has been
// built from yet.
//
// QUANTIZATION TO THE NEAREST DECADE (pow(10, round(log10(mu_k)))) is not
// cosmetic. qp_engine.h's hot-start reuse key compares the EFFECTIVE
// (primal_delta, dual_mu) pair by exact == (Task 2's CRITICAL reuse-key
// extension), so a raw mu formula returning a slightly different double every
// major (residual^1.5 essentially never repeats bit-for-bit) would force a
// refactorization on EVERY major -- including late in a solve, where the
// driver is taking tiny, warm-started corrective steps and a refactorization
// is pure waste. Rounding to a decade makes the schedule IDEMPOTENT once the
// residual stops crossing a decade boundary, so late majors reuse exactly
// like the fixed-mu driver always did. The clamp happens BEFORE
// quantization: rounding a value already inside [mu_min, mu_max] to the
// nearest power of ten can only land ON one of the four decades spanning
// [1e-12, 1e-8] or exactly at one end -- never outside that range (both ends
// are themselves exact decades). Per Task 2's own carry-forward: a
// non-idempotent mu formula would refactorize MORE than necessary, never
// less, which is the safe direction -- quantization is what makes it also
// the CHEAP direction.
//
// PRIMAL_DELTA IS DELIBERATELY NOT SCHEDULED. The spec's schedule -- and this
// task's brief -- governs dual_mu only; SolveOverrides::primal_delta is left
// at its own sentinel every major, i.e. the engine's QpOptions::primal_delta
// default, unconditionally. Coupling it to the same residual is plausible
// future work (it targets a DIFFERENT footprint -- Hessian-block
// regularization, which matters most on an indefinite subproblem, a regime
// this task's fixture does not exercise), but is out of scope here: this task
// attacks the accuracy CEILING, which is the CONSTRAINT-row phenomenon above
// (dual_mu), not a curvature one.
//
// DISABLED (SqpOptions::adaptive_mu == false) leaves SolveOverrides::dual_mu
// at its own sentinel every major -- the driver never touches the field at
// all -- which resolves to opts_.qp.dual_mu exactly as it did before this
// task existed. SOC's and the elastic tier's re-solves already build their
// own SolveOverrides from scratch (see those notes) and were never wired to
// the main trial's override, so they are unaffected by this task in either
// mode: they always run at the engine's own default mu.
// =============================================================================

// kappa_mu in the schedule above; 1.0 per the spec, named so a future
// re-derivation has somewhere to change it.
inline constexpr double kAdaptiveMuKappa = 1.0;
inline constexpr double kAdaptiveMuMin = 1e-12;
inline constexpr double kAdaptiveMuMax = 1e-8; // == QpOptions::dual_mu's own default

// =============================================================================
// THE SEEDED DUAL CLAMP (PHASE-6 TASK 5) -- the `lambda_i >= 0` enforcement
// that makes StartLevel::kSeeded safe to open, and the closure of O-B1-4
// (docs/notes/2026-07-31-nonconvex-sweep-adjudications.md Sec. 7.5).
// =============================================================================
//
// WHAT IT IS. On a kSeeded ingest, and ONLY there, every ingested lambda_i(j)
// that is still NEGATIVE after the B-1 geometric clear is judged:
//
//     -kSeededDualClampTol <= lambda_i(j) < 0   ->  set to 0, count it
//                                                   (SqpCounters::seeded_clamped)
//     lambda_i(j) < -kSeededDualClampTol        ->  DEGRADE THE WHOLE OBJECT
//                                                   to kCold
//
// THE ORDER -- B-1 CLEAR FIRST, CLAMP SECOND -- IS NORMATIVE, NOT INCIDENTAL,
// and reversing it would change answers. The B-1 clear zeroes the price on
// every row the destination model reports STRICTLY SLACK; only after it has run
// is a surviving negative price a statement about a row that is GEOMETRICALLY
// ACTIVE, which is the one configuration a sign violation can actually do
// damage in (a negative price on a slack row is stale bookkeeping the clear was
// always going to drop, and degrading an otherwise-good object over it would
// refuse a seed for a reason that had already been handled). Run the other way
// round, a mesh transfer or crossover carrying an ordinary stale negative price
// on a released row would degrade to kCold and the whole level would be dead on
// its two intended producers.
//
// WHY THE TWO OUTCOMES DIFFER, and this is the part worth being blunt about:
// a small negative price is NOISE -- an interior-point method handing over
// before full convergence, a predictor's ratio test at a weakly active row --
// and the honest repair is to say "this row is priced at zero", which is what
// the KKT system asks for at the boundary anyway. A LARGE negative price is not
// noise, it is a contradiction: it says a constraint is PAYING the objective to
// be satisfied. **A wrong-signed price at O(1) is not a seed, it is garbage**,
// and the level's whole premise -- trust the VALUES, since you cannot check the
// provenance -- fails for it. Degrading is the only answer that neither trusts
// it nor throws (warm_start.h's "safe even if stale" contract forbids the
// throw).
//
// THE VALUE, 1e-6, AND ITS DERIVATION (observed-value discipline: every number
// below is measured or is a shipped default, and the reasoning from them to
// this constant is stated rather than asserted).
//
//   (1) THE PRODUCIBLE SIGN-VIOLATION CLASS IS ~1e-8. The two in-repo producers
//       that can emit a negative lambda_i are bounded, and both were measured:
//       predictor.h's ratio test admits one only within
//       `kDualSignTol * max(1, |lambda|)` = 1e-9 RELATIVE (that constant's own
//       note derives it), and warm_start.h's from_interior_point copies its
//       caller's price verbatim, with the shipped fixture
//       (`WarmStart.CrossoverWrongSignDualLeavesRowFree`) at -1e-8 against a
//       slack of -1e-9. 1e-8 is the larger and is the class this constant has
//       to cover.
//   (2) THAT CLASS SCALES WITH THE PRODUCER'S TOLERANCE, NOT WITH OURS. An IP
//       method's residual sign noise near convergence sits about an order above
//       its own stopping tolerance -- the 1e-8 in (1) accompanies a
//       complementarity residue of mu = 1e-9 in that fixture, and the IPM
//       bridge's own matched-accuracy arm runs bar_tol = 1e-12
//       (docs/notes/2026-08-01-psiopt-first-comparison.md Sec. 4.6). A producer
//       converged to 1e-7 or better therefore lands inside 1e-6.
//   (3) 1e-6 IS THIS DRIVER'S OWN kkt_tol DEFAULT (SqpOptions::kkt_tol), and
//       that is the reading that makes the constant defensible rather than
//       arbitrary: a price of magnitude below kkt_tol contributes at most
//       kkt_tol * ||Ai_j|| to the Lagrangian gradient, i.e. it sits AT THE
//       STATIONARITY NOISE FLOOR THIS SOLVER ALREADY DECLARES. Calling such a
//       price zero cannot move a verdict the solver was entitled to make.
//       Against (1) it leaves two orders of margin; against an O(1) garbage
//       price it leaves six.
//
// IT IS ABSOLUTE, NOT RELATIVE, AND THAT IS THE WHOLE POINT. predictor.h's
// kDualSignTol scales by `max(1, |lambda|)` because it is asking "is this
// multiplier zero?"; this one asks "is this multiplier's SIGN VIOLATION small?",
// and a magnitude-relative band would clamp a -1e6 price as readily as a -1e-9
// one, which is precisely the garbage case the degradation exists for.
//
// IT DOES NOT TRACK opts_.kkt_tol AT RUNTIME, deliberately. The quantity being
// bounded is a property of the PRODUCER that assembled the object, not of the
// stopping tolerance the CONSUMER happens to be configured with, so coupling
// them would make ingest behaviour move with an unrelated knob -- a solve
// re-run at a tighter kkt_tol would start degrading seeds it previously
// accepted, with nothing about the seed having changed. Both directions of
// error here are safe (a too-wide band drops a price the QP re-derives; a
// too-narrow one degrades to a cold solve), which is what makes a fixed,
// documented band preferable to a coupled one.
inline constexpr double kSeededDualClampTol = 1e-6;

// WHERE THE DEFINITIONS LIVE (M3 phase-C task T5). Every member function of
// SqpDriver below EXCEPT THE TWO CONSTRUCTORS is declared here and DEFINED in
// the library TU `src/drivers/sqp_driver.cpp` -- solve_impl (the major loop)
// first among them, together with the trust-region update logic it drives
// (shrink_hits_floor / shrunk_radius / restoration_restart_radius), the ledger
// tail record_solve, the exit helpers finish / make_warm_start / map_status,
// and the SSN tier's ssn_engine / ssn_options. READ THAT FILE'S BANNER before
// changing anything about this class's structure: it carries the whole
// argument, including the object-file evidence that the loop's calls into
// eval_nlp / evaluate_kkt / build_subproblem were ALREADY out-of-line calls at
// -O3 before the carve (so the boundary costs them nothing), and the reason
// the TR update functions travel with the loop rather than staying with the TR
// policy constants in detail/globalization/sqp/.
//
// NOTHING ABOUT THE CONTRACT MOVED. Every signature, default argument, access
// specifier, data member and doc comment on this class is what it was; the
// bodies are the same lines one indentation level shallower. What moved is
// where the code is GENERATED: as inline bodies here, ~2 350 lines of driver
// orchestration were parsed and emitted in every TU that includes this header,
// for functions each such TU calls at most once per solve. CLAUDE.md section 5
// names drivers and orchestration as .cpp-TU code for exactly that shape.
//
// THE CONSTRUCTORS STAY INLINE, deliberately: T3 already moved their one piece
// of real work (the option validation) into src/drivers/sqp_options.cpp, and
// what is left is a member-initializer list and one call.
class SqpDriver {
  public:
    // Throws std::invalid_argument on an option that cannot be honoured
    // (non-positive tolerance, negative max_iter, non-positive/NaN radius).
    // The radius is additionally re-validated per solve by the engine
    // (qp_types.h's SolveOverrides PRECONDITION); it is checked here too so the
    // message names the driver option the caller actually set.
    //
    // tr_init == 0 IS REJECTED, not merely warned about: a zero radius pins
    // every subproblem's box to the single point p = 0, so every step is
    // zero, no iterate ever moves, and the solve stalls until max_iter and
    // reports kMaxIter -- a silent, expensive no-op that no caller can
    // possibly want. +inf remains legal and means "no trust region" (it is
    // SolveOverrides' own sentinel for deferring to the engine's
    // opts.qp.tr_radius); the requirement is tr_init > 0.
    explicit SqpDriver(const SqpOptions &opts) : SqpDriver(opts, /*allow_restoration=*/true) {}

  private:
    // THE RESTORATION PHASE'S OWN DRIVER is constructed through here with
    // restoration DISABLED, which is what bounds the recursion at one level
    // (see the RESTORATION PHASE note's NESTED RESTORATION paragraph). A
    // request raised inside a restoration solve takes the pre-Task-9 exit --
    // SqpStatus::kInfeasible, with the last row's verdict shaped by whichever
    // of the three sources raised it (kRestore for the funnel/elastic
    // routes, kReject for the radius floor -- see THE DECISION TABLE ON THE
    // WAY OUT) -- which the outer driver reads as "the feasibility problem is
    // itself stuck" regardless of that shape.
    SqpDriver(const SqpOptions &opts, bool allow_restoration)
        : opts_(opts), engine_(opts.qp), allow_restoration_(allow_restoration) {
        // M3 PHASE-C T3 MOVED THE CHECKS THEMSELVES into the library TU
        // src/drivers/sqp_options.cpp, leaving this call. Nothing about the
        // contract moved: the same six rejections fire in the same order with
        // the same messages, still before the driver is usable and still after
        // engine_ has been constructed from opts.qp (which validates its own
        // options on its own account). What moved is where the code is
        // generated -- as an inline body here, six comparison chains and six
        // fmt::format call chains were parsed and emitted in EVERY TU that
        // includes this header, for a function that runs exactly once per
        // driver construction. CLAUDE.md section 5 names options code as .cpp-TU
        // code for precisely that shape.
        //
        // READ THAT TU'S BANNER BEFORE TOUCHING A PREDICATE THERE. The checks
        // are written as `!(x > 0.0)` rather than `x <= 0.0` because that is the
        // form that rejects NaN, and that equivalence is broken on purpose by
        // the build's `-fno-finite-math-only` -- the banner states the premise
        // and T3's battery pins it by disassembly.
        validate_sqp_options(opts_);
    }

  public:
    // THE MODEL-TAKING OVERLOADS ARE THE CONVENIENCE PATH, and every one of
    // them pays one derivative-pattern walk -- the bridge lay, which is one
    // eval_hess plus one eval_jac_e/eval_jac_i per declared block, taken at
    // the model's start point (model/nlp_model_aggregate.h) -- on EVERY call;
    // a caller solving the same model in a hot loop holds one
    // NlpModelAggregate of its own and uses the aggregate-taking entries
    // below, which build no bridge.
    //
    // Both overloads route through the same validation, so a model whose
    // start_point() is malformed is rejected exactly like a caller-supplied
    // x0 that is.
    SqpSolution solve(const NlpModel &model);

    // TASK 12. Attach a ledger for instrumentation (nullptr = off, default
    // off) -- the driver-level analogue of QpEngine::attach_ledger
    // (ledger.h), one level up. Emits exactly one SqpSolveRecord per PUBLIC
    // solve() call on THIS instance, labeled `label_prefix_<n>` with a
    // per-instance counter, and ALSO forwards the same Ledger to this
    // driver's own internal QpEngine (engine_) under the suffixed prefix
    // `label_prefix_qp`, so one Ledger attached here captures both levels at
    // once: one SqpSolveRecord per driver solve, and the ordinary QP-level
    // SolveRecord entries (one per subproblem, SOC re-solve or elastic rung)
    // that engine_ has always been able to emit -- attaching here does not
    // disturb that mechanism, it only adds to the same Ledger.
    //
    // THE RESTORATION PHASE'S OWN NESTED SqpDriver (this header's private
    // constructor) is NEVER given this ledger: it is a fresh SqpDriver over a
    // fresh QpEngine, constructed on the fly, and attach_ledger is never
    // called on it. So a solve that enters restoration still emits exactly
    // ONE SqpSolveRecord (this driver's own), whose counters already fold in
    // every restoration major (SqpCounters::restoration_iters and the
    // aggregate qp_minor_iters/factorizations sums), rather than a second
    // record for the sub-solve -- consistent with SqpCounters' own "the work
    // was spent, folded into the aggregate" convention for SOC/elastic
    // re-solves.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    // Thin wrapper around solve_impl (the actual major loop, now private)
    // whose only job is the ledger record above: solve_impl is called
    // exactly once, and its result is recorded before being returned
    // unchanged. Every existing caller of the two-argument solve() -- there
    // are dozens across this project's tests -- is unaffected: the return
    // value and every side effect solve_impl had are identical to before this
    // task, whether or not a ledger is attached.
    SqpSolution solve(const NlpModel &model, const Vec &x0);

    // THE PRIMARY PATH, of which every NlpModel-taking overload on this class
    // is a wrapper (M4: the driver consumes the Level 2 contract, and a single
    // model is one bridge over it -- model/nlp_model_aggregate.h). It builds
    // one AggregateEvalSeam over `bridge` and runs the major loop against that;
    // the model-taking overloads differ only in that they build the bridge
    // themselves, from a `const NlpModel &` they do not own.
    //
    // WHAT `bridge` OWES, and it is the contract's own posture rather than
    // anything new: one operation at a time, structural mutation included. The
    // seam re-lays whenever the aggregate's structure epoch has moved, so a
    // renegotiation between solves is handled; a mutation DURING a solve is
    // not a thing this class defends against.
    //
    // THE BOX IS NOT RE-CHECKED HERE. The bridge validated the model's two
    // bound returns against its own declared n() when it laid its structures,
    // and the seam's lower()/upper() are materialized from that declaration --
    // n-sized by construction, with no model return left to disagree. The
    // model-taking overloads keep their own check, which runs BEFORE the bridge
    // exists; see solve_impl.
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0);

    // PHASE-4 TASK 3/4. Warm-start ingest, up to and including the HOT
    // level. `warm` is typically a PRIOR solve's own
    // SqpSolution::warm_start (warm_start.h), on a problem the caller
    // believes is the same or a nearby one to `model` -- see this class's
    // private solve_impl for the WARM-START INGEST note, which is the whole
    // resolution rule (cold/warm/hot level, what each level actually
    // ingests).
    //
    // THE CALLER'S OWN x0 IS IGNORED, LOUDLY, whenever `warm` resolves to
    // StartLevel::kSeeded, kWarm OR kHot: the solve then starts from
    // `warm.x`, NOT this parameter. **THAT INCLUDES A HASH MISMATCH, FROM
    // PHASE-6 TASK 5 ON** -- this paragraph used to say `x0` was honoured
    // there and it no longer is (final fix wave, W6). `x0` is used only as
    // the COLD fallback, and kCold is now reached only through the SEEDED
    // INGEST GATE below (invalid, dimensionally incompatible, or non-finite)
    // or through a start_level CAP -- never through provenance alone. A
    // caller that wants its own x0 honoured unconditionally must pass a
    // default-constructed WarmStart{} (valid == false), which always
    // resolves to kCold, or cap SqpOptions::start_level at kCold.
    //
    // NEVER THROWS ON A STALE OR FOREIGN `warm`: an invalid object, one
    // whose structure_hash does not match this model's, or one that
    // happens to share this model's (n, me, mi) shape without sharing its
    // structure is all handled silently -- warm_start.h's "safe even if
    // stale" contract, of which this ingest is the caller-facing half.
    // **WHAT "HANDLED" MEANS CHANGED IN PHASE-6 TASK 5 AND THIS PARAGRAPH
    // TRAILED IT** (final fix wave, W6): a valid, correctly sized, finite
    // object whose hash is USELESS (the 0 sentinel, or a mismatch) now
    // resolves StartLevel::kSeeded and CONTRIBUTES ITS VALUES -- x, the
    // duals, the activity hint -- rather than being discarded for a cold
    // solve. What a hash mismatch still refuses is unchanged and is the part
    // that mattered: the trust-region radius, the funnel-width re-base, the
    // Kungurtsev-Diehl full-step window, and above all any reuse of a
    // factorization built on another matrix. The exact take/refuse list is
    // warm_start.h's StartLevel note; solve_impl's WARM-START INGEST note has
    // this function's half. Objects that fail the seeded ingest gate --
    // `valid == false`, incompatible dimensions, or a non-finite value in
    // x/lambda_e/lambda_i -- still degrade all the way to kCold.
    //
    // KHOT (Phase-4 Task 4): a structural match PLUS a non-null `warm.hot`
    // additionally offers the engine THIS driver owns a chance to adopt a
    // PRIOR solve's retained K0 factorization -- possibly built on a
    // DIFFERENT SqpDriver/QpEngine instance entirely -- and skip rebuilding
    // it. What a caller can rely on:
    //   - IT NEVER PRODUCES A WRONG ANSWER. Whether the offered handle is
    //     actually reused is decided entirely by qp_engine.h's own
    //     reuse-eligibility gate (conditions (a)-(e); the fifth, added in
    //     Fix Round 1, specifically detects a handle whose underlying
    //     factorization has been rebuilt by ANOTHER holder since it was
    //     emitted -- see BorderState::generation and HotState's OWNERSHIP
    //     note in qp_engine.h). Any mismatch -- stale, foreign, or
    //     overtaken by a later solve on the engine that produced it --
    //     degrades SILENTLY to an ordinary kWarm solve: never a throw,
    //     never a numerically wrong result attributed to a reused
    //     factorization that no longer describes anything real.
    //   - `out.counters.start_level_used` (sqp_types.h's SqpCounters)
    //     reports what was OBSERVED to happen on this solve's first
    //     subproblem, not merely what `warm` offered: kHot only if the
    //     engine's own reuse gate actually passed, kWarm on any
    //     degradation.
    //   - HAND-OFF IS SINGLE-USE PER CONSUMER BUT SAFE TO CHAIN. Feeding
    //     the SAME `warm.hot` into more than one subsequent solve() call is
    //     not a lifetime error (shared_ptr keeps the underlying
    //     factorization alive for as long as anything references it -- see
    //     qp_engine.h's HotState note), and Fix Round 1 additionally makes
    //     it not a CORRECTNESS error either: a second consumer either
    //     reuses the same factorization (if nothing has touched it since)
    //     or degrades to kWarm (if something has) -- it is simply never
    //     the FIRST solve's job to protect a second one, and neither
    //     engine can corrupt what the other is relying on (a refused
    //     adoption detaches onto a fresh BorderState rather than rebuilding
    //     the shared one in place). CONCURRENT use across threads remains
    //     genuinely unsafe -- see qp_engine.h's THREAD SAFETY note.
    //
    // THE PROBE BUDGET (PHASE-6 TASK 1), the fourth argument, default 0 =
    // NO BUDGET. A POSITIVE `minor_budget` asks this solve to STOP EARLY --
    // at the top of the first major that finds `counters.qp_minor_iters >=
    // minor_budget` without having converged -- and to report that stop in
    // `counters.probe_budget_stops` (sqp_types.h). It exists for exactly one
    // caller shape: a driver of solves (continuation.h) that can tell, from
    // the cost of the solves it has already paid for, that THIS one has
    // stopped looking like them, and would rather re-pose the problem than
    // finish paying. What it buys is measured in
    // docs/notes/2026-08-02-controller-retry-economics.md.
    //
    // THE CONTRACT, in the four parts a caller has to know:
    //
    //   1. IT IS INERT AT 0 (and at any value <= 0), which is what every
    //      existing call site passes by omission. The test below is then a
    //      single false comparison per major and NOTHING else about this
    //      driver -- trajectory, counters, status, warm start -- can differ
    //      from the pre-Task-1 code. That inertness is the whole basis on
    //      which this task claims byte-identity on unbudgeted work.
    //   2. IT IS CHECKED BETWEEN MAJORS, NEVER INSIDE ONE. The QP engine is
    //      not told about it and its own QpOptions::max_iter is not touched,
    //      so a solve stopped this way has spent AT LEAST `minor_budget`
    //      minors and at most `minor_budget` plus whatever the crossing
    //      major cost (bounded in turn by that QP cap, plus any SOC/elastic
    //      re-solve on the same major). A caller that needs a HARD minor
    //      bound does not have one here; what it has is a bound on how many
    //      more majors it will buy, which is one: none.
    //   3. CONVERGENCE ALWAYS WINS. The budget test sits beside the max_iter
    //      test, after the convergence test has already been evaluated on
    //      the same pass, so a solve that is AT a KKT point is reported
    //      kOptimal and its answer kept no matter how far over budget it
    //      went. This is the property that makes an aggressive budget safe:
    //      it can throw away work about to be spent, never an answer already
    //      found.
    //   4. THE STATUS IS kMaxIter, deliberately, and NOT kBudgetExhausted
    //      even when SqpOptions::budget_mode is on. budget_mode's status
    //      carries a specific promise -- "here is the best iterate I saw,
    //      pick up from it AT THE SAME PARAMETER VALUE" (sqp_types.h) -- and
    //      a probe-budget stop promises the opposite: the caller asked to be
    //      told cheaply that this solve is not going where it hoped, and
    //      continuing it at the same point is precisely what it does not
    //      want. Reporting a stop that means "abandon this proposal" with a
    //      status that means "continue this proposal" would have made the
    //      two budgets fight. `counters.probe_budget_stops` is what
    //      distinguishes this exit from an ordinary max_iter one; the
    //      returned WarmStart is built exactly as any other stopped-AT-an-
    //      iterate exit's is, and is as safe to feed forward (warm_start.h)
    //      -- continuation.h simply does not, for the reason its own retry
    //      note gives.
    //   5. WHAT THE BUDGET IS SPENT IN, PER MODE (Phase-7 Task 5, fix round
    //      1). At `qp_mode == QpMode::kWalk` it is `qp_minor_iters`, exactly as
    //      it always has been, and clause 1's inertness claim is unchanged. At
    //      `qp_mode == QpMode::kSsn` it is that PLUS every factorization the
    //      SSN kernel and the tier-3 refinement paid -- because an SSN-solved
    //      subproblem contributes ZERO minors by design, so a minors-only
    //      budget could not trip AT ALL in that mode and this safeguard was
    //      silently void there. The derivation, and why factorizations are the
    //      right second term, is at `ssn_budget_charge`'s declaration in
    //      solve_impl. NOTE WHAT THIS DOES NOT CHANGE:
    //      `SqpCounters::qp_minor_iters` still publishes walk minors and
    //      nothing else, in both modes, so no measurement quoted in this
    //      repository moves. It DOES mean a kSsn budget and a kWalk budget of
    //      the same numeric value are not the same amount of work, which is the
    //      honest consequence of two kernels that do not share a minor.
    SqpSolution solve(const NlpModel &model, const Vec &x0, const WarmStart &warm,
                      Index minor_budget = 0);

    // The warm-start ingest against a bridge, on the same footing as the
    // 2-argument bridge overload above: same primary path, same seam, and the
    // whole of the ingest contract documented on the model-taking overload
    // just above this one.
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0, const WarmStart &warm,
                      Index minor_budget = 0);

  private:
    // The ledger-recording tail every public solve() overload shares:
    // record exactly one SqpSolveRecord (if a ledger is attached) and
    // return the result unchanged. Factored out (Phase-4 Task 3) so the
    // 3-arg overload does not duplicate attach_ledger's contract -- see
    // that note for what "exactly one SqpSolveRecord per public solve()
    // call" promises. `wall_seconds` (Phase-5 Task 2) is measured by each
    // caller above, around solve_impl alone, and simply carried through here
    // into the record.
    SqpSolution record_solve(SqpSolution out, double wall_seconds);

    // `minor_budget` <= 0 means NO BUDGET -- see the 4-argument solve()'s own
    // THE PROBE BUDGET note for the whole contract.
    //
    // EVERY MODEL QUANTITY THIS LOOP READS ARRIVES THROUGH `seam` (M4), which
    // is the whole of the change: the dimensions, the box, the six evaluation
    // moments and the subproblem. There is no NlpModel in scope here and no
    // `model.eval_*` call anywhere below -- the free functions over NlpModel
    // declared in this header remain for callers measuring a point of their
    // own, and the solve path does not use them.
    //
    // THE ONE PLACE A MODEL IS STILL NAMED is the restoration phase, which
    // builds a DIFFERENT NlpModel (RestorationModel, a wrapper in the
    // variables (x, sp, sm, si)) around the one behind the bridge and solves it
    // with a nested driver. That wrapper is a Level 1 construction with no
    // aggregate form of its own, so the entry point reaches the model through
    // the bridge -- an identity read, never an evaluation -- and the nested
    // solve then wraps the wrapper in its own bridge and seam through the
    // model-taking overload, exactly as the outer call did.
    SqpSolution solve_impl(AggregateEvalSeam &seam, const Vec &x0, const WarmStart &warm,
                           Index minor_budget);

    // See this header's SUBPROBLEM FAILURE ROUTING note. Reached only after the
    // one-shot retry has already been spent -- and, from Task 8, never with
    // kInfeasible (the elastic tier consumes that status upstream), which is
    // why the kInfeasible arm below is unreachable in this driver and kept
    // only to keep the mapping total.
    static SqpStatus map_status(QpStatus qp_status);

    // THE TWO HALVES OF THE SHRINK RULE (Task 9), kept as one pair so the
    // floor test and the shrink itself can never disagree about what "the next
    // radius" is. See RADIUS MANAGEMENT for both.
    //
    // A +inf radius does NOT hit the floor -- its next value is tr_max, which
    // the constructor has already checked is >= tr_min -- and this is the one
    // place the two functions differ in shape rather than in arithmetic.
    bool shrink_hits_floor(double delta) const;
    double shrunk_radius(double delta) const;

    // The radius the main loop RESUMES at after a restoration, and the floor
    // under the radius the restoration phase itself is given. A documented
    // fraction of the caller's own starting radius -- see the RESTORATION
    // PHASE note's WHAT RESUMING DOES for why a fraction rather than either
    // extreme -- with +inf resolved to tr_max exactly as everywhere else, and
    // never below the floor.
    double restoration_restart_radius() const;

    // PHASE-7 TASK 5. The SSN engine, constructed on first use. See the
    // member's own note for why it is deferred; `opts_.qp` is the SAME
    // QpOptions the walk was constructed with, so the two kernels are
    // regularized identically by construction (ssn_engine.h's SsnOptions
    // note makes that a requirement).
    SsnEngine &ssn_engine();

    // The SsnOptions ONE subproblem is solved under.
    //
    // `fb_tol` TRACKS THE TIGHTER OF THE DRIVER'S OWN kkt_tol AND feas_tol
    // (fix round 1 -- `ssn_fb_tol_for` above has the derivation and the defect
    // that forced feas_tol to join it), through ssn_engine.h's own
    // derivation function rather than by assignment: that header's section 5
    // establishes that an FB residual at `kkt_tol` buys stationarity and
    // equality feasibility at exactly kkt_tol and per-row complementarity
    // within a bounded factor of it. This is what makes the 1e-10 finish
    // REACHABLE from a kSsn-solved subproblem exactly as from a walk-solved
    // one: a caller who tightens kkt_tol to 1e-10 tightens the subproblem
    // kernel with it, and the driver's convergence test -- which this task does
    // not touch -- is then met at the same place either way.
    //
    // EVERY OTHER FIELD IS ssn_engine.h's OWN DEFAULT, deliberately.
    // `safeguards` stays kFull (kBare is a positive control, not a product
    // surface); `soft_budget`/`hard_budget` stay at 12/25, the values Task 4
    // derived and sized against an escaping subproblem's own worst case (up to
    // 25 factorizations on a full ladder climb); `uncertain_tol` stays at the
    // swept default. This task adds NO new tuning knob -- choosing among these
    // is Task 6's material and wants a corpus, not a guess here.
    //
    // `prox_sigma_init` is the ONE field this driver sets, and only on the
    // first subproblem of a solve that ingested a proximal carry. See
    // warm_start.h's `prox_sigma` note for the whole rule.
    SsnOptions ssn_options(double prox_sigma_init) const;

    // WARM-START POPULATION (Phase-4, Task 2). Builds the WarmStart every
    // exit of solve_impl attaches to SqpSolution::warm_start.
    //
    // `activity` is the best-known QpSolution WHOSE bound_state/ineq_active
    // still describe the point being returned -- nullptr when no such
    // solution exists (nothing was ever solved, or the point being returned
    // is not one any in-loop QpSolution was solved at, e.g. a restored
    // point). Every call site above documents, at the point it calls this,
    // exactly which QpSolution (if any) qualifies and why -- see
    // restoration_moved_x's own declaration note for the one case that
    // disqualifies an otherwise-fresh `qs`.
    //
    // `qp_built` says whether `qp` already holds a REAL subproblem (see this
    // function's caller-side note); passing false with a default-constructed
    // QpProblem is what keeps structure_hash from hashing zero-sized
    // matrices as if they meant something. `probe_ev`/`probe_x` are the
    // evaluation bundle and the point it was taken at -- read ONLY when
    // qp_built is false (see THE ZERO-MAJOR PROBE below), and NULL on the one
    // exit that stands at no evaluable point at all (see THE UNEVALUABLE
    // EXIT, further below). Every call site passes a CONSISTENT pair, with
    // the one documented exception noted at the restoration exits.
    //
    // THE ZERO-MAJOR PROBE (Phase-5 Task 0, repair A for the battery note's
    // O-1). Through Phase 4 this function wrote `structure_hash = 0` whenever
    // qp_built was false, i.e. on every exit that converged (or ran out of
    // budget, or found the start point unevaluable) before the first
    // subproblem was ever built. That 0 is warm_start.h's "no model was seen"
    // sentinel, which solve_impl's own ingest rule treats exactly like a
    // MISMATCH -- so a zero-major solve emitted a `valid` hand-off the very
    // next solve silently refused, and the better a caller's predictor got the
    // more often its chain broke (docs/notes/2026-07-30-warm-start-battery-
    // results.md, O-1).
    //
    // The hash such an exit "never computed" is nonetheless COMPUTABLE there,
    // because detail::structural_hash (qp_engine.h) reads H/Ae/Ai's SPARSITY
    // PATTERN ONLY, never their values: which entries the model's Hessian and
    // Jacobians structurally have is a property of the model's construction,
    // not of the point or the multipliers they are evaluated at. So this
    // builds a PROBE subproblem at the exit point and hashes that, through the
    // very same build_subproblem + structural_hash machinery the qp_built path
    // uses -- never a bespoke pattern walk that could drift from it.
    //
    // ZERO MULTIPLIERS, DELIBERATELY: this is the same recipe the INGEST
    // side's own probe uses (see solve_impl's WARM-START INGEST note, which
    // builds at the caller's x0 with zero duals), so the two hashes agree by
    // construction on any model whose pattern is point-independent -- the
    // exact assumption the ingest probe has always made.
    //
    // THE COST, and why it is paid here. One extra eval_hess, and nothing
    // else: `probe_ev` is the evaluation bundle the loop already computed at
    // this iterate, so Je/Ji come for free and no eval_nlp is repeated. It is
    // paid ONLY on the qp_built == false exits THAT ARE PROBED AT ALL (two of
    // the three -- see THE UNEVALUABLE EXIT below for the one that is not);
    // a solve that built even one subproblem hashes that subproblem and
    // evaluates nothing extra. Those
    // exits are by definition the ones that spent no major iteration at all,
    // so one Hessian is the cheapest thing such a solve does; the alternative
    // is the hand-off being unusable, which costs the NEXT solve a full
    // re-globalization from cold.
    //
    // THE UNEVALUABLE EXIT IS NOT PROBED, AND ITS HAND-OFF IS COLD
    // (`probe_ev == nullptr`). Fix round 1 of Phase-5 Task 0; the first cut
    // probed it too, on the true-but-insufficient ground that the hash reads
    // patterns only. What that missed is what the OBJECT then does: the
    // 3-arg solve() overload takes its x FROM the warm object on a kWarm
    // resolution (see the WARM-START INGEST note), so a hash-valid hand-off
    // from a solve that could not evaluate its own start point pins the NEXT
    // solve back onto that same unevaluable point and DISCARDS the corrected
    // x0 the caller supplied to retry with -- turning a recovery into a
    // repeat of the same kNumericalError. Measured on exactly that shape
    // before it was fixed.
    //
    // So that exit passes null, and this function emits a COLD object there:
    // `valid = false`, `structure_hash = 0`, no probe attempted. Both
    // consequences are decided HERE, from the one parameter, so they cannot
    // drift apart at the call site. That is the honest description of what
    // such a solve learned -- warm_start.h's "a failed solve's point is still
    // safe evidence" contract rests on the point being one the model can
    // evaluate, and this is the single exit where it is not. O-1 is about
    // solves that CONVERGED (or ran out of budget) at zero majors, standing
    // on a perfectly good point; neither loses anything here.
    //
    // It also keeps two other contracts intact rather than bending them:
    // build_subproblem's own precondition (it is never handed a non-finite
    // `ev`, since the only exit whose `ev` can be non-finite is this one),
    // and eval_hess's -- the one model entry point eval_nlp never calls,
    // which is now never called at a point the model has already reported
    // unevaluable.
    //
    // WHERE `probe_ev` AND `probe_x` ARE NOT A MATCHED PAIR, and why it does
    // not matter. At the four RESTORATION exits, `x` may already have been
    // moved to the restored point while `ev` still describes the point the
    // restoration was requested from. Those exits pass the pair anyway
    // because the probe is unreachable there: `qp_built` is unconditionally
    // true by then (a restoration can only be requested after a subproblem
    // has been built and solved), so the hash comes from `qp`. Were that ever
    // to change, the probe would still be CORRECT -- it reads only the
    // model's sparsity pattern, which is the same at both points -- but this
    // sentence is here so the mismatch is a recorded fact rather than a
    // silent assumption.
    //
    // Static (no `this`): every value it needs -- including opts_.qp.
    // primal_delta -- is passed in explicitly, so it can be called from a
    // context (this function and the raw non-finite-iterate return) that
    // does not otherwise need a `SqpDriver&`. `hot` (Phase-4 Task 4) is the
    // one exception to "every value it needs" being free of `this` --
    // every call site fetches it from `engine_.hot_state()` right before
    // calling in, since only the driver's own engine_ instance can produce
    // it; this function itself stays static and merely stores what it is
    // handed, exactly as it does for every other already-computed value.
    //
    // TAKES THE SEAM (M4), non-const, because the zero-major probe below builds
    // a subproblem through it -- an evaluation, which re-lays if the aggregate's
    // epoch has moved and scatters into the seam's own arena. Everything else it
    // reads off the seam is a dimension.
    static WarmStart make_warm_start(AggregateEvalSeam &seam, const QpSolution *activity,
                                     const QpProblem &qp, bool qp_built, const NlpEval *probe_ev,
                                     const Vec *probe_x, double delta, double dual_mu_eff,
                                     double primal_delta_eff, const GlobalizationStrategy *strategy,
                                     std::shared_ptr<const HotState> hot);

    static SqpSolution finish(SqpSolution out, SqpStatus status, const Vec &x, const Vec &lambda_e,
                              const Vec &lambda_i, const SqpKkt &kkt, double f, WarmStart warm);

    SqpOptions opts_;
    // ONE engine for the whole driver, deliberately: it is what makes warm
    // seeding (and, when the model happens to be a QP, qp_engine.h's
    // hot-start K0 reuse) possible across majors. It also makes SqpDriver
    // exactly as thread-unsafe as QpEngine -- use one driver per thread.
    QpEngine engine_;
    // PHASE-7 TASK 5. The semismooth-Newton tier's engine, LAZILY CONSTRUCTED
    // and never touched at the shipped default.
    //
    // WHY A POINTER RATHER THAN A MEMBER. SsnEngine owns a live KktFactor (and
    // through it a Pardiso/Accelerate backend session), exactly as QpEngine
    // does. Making
    // it a plain member would allocate that state on EVERY SqpDriver a caller
    // constructs, including the overwhelming majority that run `qp_mode ==
    // QpMode::kWalk` and will never solve an SSN subproblem -- and including
    // the driver the restoration phase constructs for itself on every
    // restoration entry. Deferring it to first use makes "kWalk touches no SSN
    // code at all" a structural fact rather than a claim: no SsnEngine is
    // constructed, so none of ssn_engine.h runs.
    //
    // ONE ENGINE FOR THE WHOLE DRIVER, for the same reason engine_ is one:
    // ssn_engine.h holds its KktFactor across solves, which is what makes its
    // "one symbolic analysis per structure, reused across QPs of identical
    // structure" property (that header's section 3) observable at all. A fresh
    // engine per subproblem would pay a phase-11 analysis on every major.
    std::unique_ptr<SsnEngine> ssn_engine_;
    // The proximal level to EXPORT on this solve's WarmStart, and the point it
    // was reached at -- warm_start.h's `prox_sigma` / `prox_center_*` block,
    // whose own note carries the whole contract (max over the solve's SSN
    // subproblems, applied to the FIRST subproblem of the next one).
    //
    // A MEMBER RATHER THAN A LOCAL because solve_impl has a dozen exits and
    // make_warm_start is static; record_solve is the one funnel every public
    // solve() overload passes through, so the stamp happens there, once. Reset
    // at the top of every solve_impl call so nothing leaks between solves.
    double ssn_prox_sigma_out_ = 0.0;
    // The CENTRE's own high-water mark, which is NOT ssn_prox_sigma_out_: the
    // level is taken over every SSN subproblem, the centre only over CERTIFYING
    // ones (fix round 1 -- the stamp site has the defect that split them).
    double ssn_prox_center_sigma_out_ = 0.0;
    Vec ssn_prox_center_x_out_, ssn_prox_center_lambda_out_;
    // False on the driver the RESTORATION PHASE constructs for itself, which
    // is what bounds the recursion at one level. See the private constructor.
    bool allow_restoration_ = true;

    // TASK 12. See attach_ledger's doc comment above for the whole contract;
    // nullptr (ledger_) is "off", exactly like QpEngine's own ledger_.
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;
};

// TASK 12. Human-readable names for the printer below. Used only there --
// nothing in the driver itself branches on a printable string.
//
// M3 PHASE-C T1 MOVED THE DEFINITION (and format_iteration_table's, below)
// into the library TU `src/drivers/sqp_print.cpp`. See that file's banner for
// the whole argument; in short, CLAUDE.md section 5 names printing as .cpp-TU
// code, and neither function is on any hot path. The declarations stay here,
// on the header that owns them, so every call site compiles unchanged.
const char *to_string(StepVerdict v);

// SqpStatus -> string is core/solver_status.h's to_string(SqpStatus) (hoisted
// out of here in the fix wave that closed N3 -- it used to be a second copy
// here -- and rehomed from drivers/sqp_types.h into core/ by phase-C S2).

// TASK 12. THE ITERATION PRINTER the brief asks for: an fmt-based table
// rendering of sol.history (one row per SqpIterate -- trial index, f, the
// KKT residual, h == violation_l1, the trust-region radius Delta, the
// verdict, and the QP counters of the subproblem solved from that row, if
// any) followed by the final status line. Returned as a plain std::string,
// per T6 (library code never prints to stdout on its own, and never folds a
// diagnostic into anything other than a thrown exception's message) -- a
// caller that wants this on a terminal does that itself.
//
// A ROW WHOSE qp_solved IS FALSE (at most one, always last -- see
// SqpCounters) prints "-" in the verdict/QP columns instead of the DEFAULTED
// kReject/0 those fields carry on such a row: SqpIterate's own note says
// verdict is MEANINGLESS there, and printing the default would read as a
// rejection that never happened.
//
// PHASE-4 TASK 7 adds two things, closing this task's own carries:
//   - the WD column (SqpIterate::watchdog_restored, "*"/"" -- see that
//     field's note for why a restored row needs a marker at all: without
//     one the KKT-Res column can jump backward with nothing explaining it).
//     Rendered on EVERY row, regardless of qp_solved, since a restore and a
//     stopped-AT-iterate exit can coincide on the same row (the restored
//     point can itself already satisfy the convergence test).
//   - the trailing "Start Level" line, alongside "Status" -- SqpCounters::
//     start_level_used (sqp_types.h) is a SOLVE-WIDE reading, not a
//     per-row one, so it belongs in the footer rather than repeated on
//     every row the way the WD marker is.
// M3 PHASE-C T1: the DEFINITION now lives in `src/drivers/sqp_print.cpp`.
// The rendering rules above are the contract; the file that implements them
// is the one to edit when a column changes.
std::string format_iteration_table(const SqpSolution &sol);

} // namespace hven::solvers
