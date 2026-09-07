# Header prose archive — M6 W5 T7

## What this is

The discussion prose removed from hven's ten largest headers at M6 W5 T7, kept
verbatim. The owner's rule for headers is that they are API documentation, not a
settlement record: what stays beside a declaration is the OPERATIVE contract —
ordering, ownership and lifetime, exceptions, numerical constraints, and what is
emitted when something is absent — stated in terse Doxygen. Everything else —
rationale, alternatives considered, derivations, worked examples, "why this
shape" essays, counter-factuals — is here.

Nothing in this file is a contract. If this file and a header disagree, the
header is right and this file is a record of what was once written next to the
code. Nothing here is paraphrased: the extracts are the bytes that were removed.

## How to read a stamp

Each entry opens with

```
**SOURCE** <sha> · <path> · lines <first>–<last>
```

The sha is `1997159`, the code head at T7's BASE, and the line numbers are that
commit's — never the line numbers of the moment. A later rename or move (M6 W5
T8's rename sweep, for one) cannot invalidate a stamp, because the stamp does
not point at the tree as it is now. `git show 1997159:<path>` reproduces the
extract exactly.

One line above each extract says which declaration the prose documented and what
replaced it in the header.

## Append-only

This is a historical record. Entries are appended, never edited and never
deleted — the same discipline CLAUDE.md §7 applies to a pinned artifact. A
correction goes in a new entry that says what it corrects.

## Sections

Ordered as the headers were processed, largest first by comment-line count at
`1997159`.

### include/hven/drivers/sqp_driver.h

3488 lines / 3146 comment lines at `1997159`; 1024 / 686 after.

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 6–22

File-level overview of the major loop and its four mechanisms. Replaced by a `@file`/`@brief` block naming the loop, the elastic reformulation, the second-order correction and the restoration phase.

```text
// sqp_driver.h — the SQP major loop:
//
//     min   f(x)   s.t.  cE(x) = 0,  cI(x) <= 0,  l <= x <= u      (nlp_model.h)
//
// solved under a trust-region globalization: solve at the current radius,
// evaluate the trial point, let the strategy judge it, move or shrink
// accordingly. A QP that fails but hands back a usable iterate is a rejected
// step, not an abort (SUBPROBLEM FAILURE ROUTING); a kReject whose constraint
// violation INCREASED gets one hot-started rescue attempt before the radius
// shrinks (SECOND-ORDER CORRECTION); a subproblem that returns kInfeasible is
// REFORMULATED with penalized slacks and re-solved instead of ending the solve
// (THE ELASTIC TIER -- KLV Algorithm 5's authoritative trigger); and a
// restoration REQUEST -- from any of three sources (the funnel's signature,
// the elastic tier's exhaustion, the radius floor) -- switches the driver to
// minimizing the infeasibility measure h on a wrapper model until it can
// resume or CERTIFY infeasibility (THE RESTORATION PHASE).
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 23–61

File-level: the six steps of ONE TRIAL. Nothing replaced it in the header — every step's own contract is stated on the declaration that implements it.

```text
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
//      short-circuit, which runs AHEAD of the acceptance test. A NON-FINITE
//      iterate is detected here too -- see evaluate_kkt's note on why that
//      check must be explicit.
//   2. MAX-ITER TEST. opts.max_iter bounds SUBPROBLEMS SOLVED (a rejected
//      trial costs one). The budget is SHARED with the restoration phase, so
//      the bounded quantity is counters.major_iters +
//      counters.restoration_iters.
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
//      route: there is no certified step to correct.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 62–107

File-level: RADIUS MANAGEMENT, the three trust-region constants and their provenance. The header keeps the pair rule at `shrink_hits_floor`/`shrunk_radius` and the resume radius at `restoration_restart_radius`.

```text
// --- RADIUS MANAGEMENT ---------------------------------------------------
//
// PORTED FROM STANDARD TRUST-REGION PRACTICE, not from [KLV]: the three
// constants are the classical ones -- Conn, Gould & Toint, "Trust-Region
// Methods" (MPS-SIAM, 2000), Algorithm BTR (Sec. 6.1) and Table 6.1.1: a
// very-successful threshold eta_2 = 0.75, an expansion factor gamma_2 = 2 and
// a contraction factor gamma_1 = 0.5. They are named constants below.
//
//   GROW: Delta <- min(2*Delta, tr_max), but ONLY when BOTH
//         (i) the radius was ACTIVE at the accepted step (QpSolution::
//             tr_active anywhere), and
//         (ii) the step was STRONG: actual/predicted >= kTrGrowThreshold,
//             with the predicted decrease positive. On an h-TYPE accepted
//             step (KLV Eq. (12)) f may legitimately RISE, so rho is negative
//             and the radius does not grow.
//   SHRINK: Delta <- Delta/2 on every kReject, and the SAME subproblem is
//         re-solved. Nothing else about the iterate changes.
//
// THE SHRINK FACTOR IS COUPLED TO globalization.h's kRestoreMinRejections,
// which is DERIVED from this one (4 = ceil(log2(10)) at a factor of 1/2). The
// derivation lives at that constant; CHANGING EITHER CONSTANT ALONE CHANGES
// WHAT THE RESTORATION SIGNATURE MEANS.
//
// THE RADIUS FLOOR, SqpOptions::tr_min. A shrink that would take Delta below
// the floor does NOT clamp and re-solve -- it RAISES A RESTORATION REQUEST.
// This is KLV Algorithm 4's "alpha < alpha_min" trigger in its trust-region
// form, one of the paper's two AUTHORITATIVE restoration entries (the other,
// Algorithm 5's infeasible subproblem, is the elastic tier's exhaustion). The
// floor is a DIAGNOSIS, not a runaway guard (max_iter already bounds that): a
// radius that small means every direction the model can still see has been
// tried and rejected.
//
// WHAT A SHRINK FROM +inf DOES: at tr_init = +inf the first rejection lands
// the radius on tr_max, and every subsequent one halves normally. tr_max is
// the landing value because it is ALREADY the caller's stated ceiling. The
// growth rule is still skipped while Delta is +inf -- min(inf*2, tr_max)
// would REDUCE it -- so this is the only way a +inf solve acquires a finite
// radius.
//
// CGT'S eta_1 SHRINK-ON-WEAK-ACCEPT BRANCH IS DELIBERATELY NOT PORTED: here
// ACCEPTANCE IS THE FUNNEL'S, and an h-type acceptance legitimately has
// rho < 0, so eta_1 would shrink the radius on exactly the steps KLV's proof
// relies on. The cost of leaving it out is that a sequence of weak-but-
// accepted steps keeps a too-large radius until the funnel's next rejection
// corrects it, one halving at a time.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 108–142

File-level: MODEL EVALUATION and what a rejected trial costs. The header keeps the per-call evaluation count on `eval_nlp` and the probe's one eval_hess on `make_warm_start`.

```text
// --- MODEL EVALUATION, AND WHAT A REJECTED TRIAL COSTS --------------------
//
// The funnel judges f and h AT THE TRIAL POINT, so every trial costs one
// model evaluation there. On an ACCEPTANCE that evaluation becomes the next
// iterate's NlpEval and the next trial's convergence test reuses it: an
// accepted step costs one full eval_nlp per iterate plus one eval_hess per
// subproblem built.
//
// A REJECTED trial costs no full eval_nlp: judge() reads only f/h
// (StepContext, globalization.h), so the trial is evaluated through
// eval_nlp_values and STAYS values-only for the rest of that trial's life on
// a reject, a restore, or an accept that arrived via a promoted SOC
// correction at a DIFFERENT point. A DIRECT acceptance -- where this same
// NlpEval becomes the next iterate's `ev` -- upgrades it in place
// (upgrade_to_full) rather than recomputing f/cE/cI.
// SqpCounters::evals_full/evals_values is the ledger of which happened.
//
// THE ONE eval_hess THIS ACCOUNTING DOES NOT COVER: a solve that exits WITHOUT
// EVER BUILDING A SUBPROBLEM (converged at its start point, or spent a zero
// budget) pays ONE eval_hess in make_warm_start, to hash the model's sparsity
// for the hand-off it emits -- see that function's THE ZERO-MAJOR PROBE note.
// A solve that built even one subproblem never makes it, so the total is
// bounded by max(1, subproblems built). The third qp_built == false exit (an
// unevaluable start point) pays nothing (THE UNEVALUABLE EXIT).
//
// A rejected trial costs NO eval_hess: the subproblem is not rebuilt. This
// still holds when SOC is attempted -- SOC never calls eval_hess, and
// build_soc_subproblem reads only quantities already in hand.
//
// ONE MORE MODEL-EVALUATION COST, from SOC: whenever an SOC re-solve reaches
// kOptimal (not on every attempt), the driver pays one evaluation at the
// corrected point, to judge it and, on acceptance, to seed the next iterate's
// `ev`. That evaluation is VALUES-ONLY too, upgraded to full only if the
// corrected point is promoted.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 143–213

File-level: the CONVERGENCE TEST contract — the reduced stationarity measure, the feasibility measure, and what is measured but not gated. The header keeps the field-level definitions on `SqpKkt` and the NaN rule on `SqpKkt::finite`.

```text
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
// against the box, and unlike that form it does not saturate -- which matters
// because this same scalar is what the contraction test reads.
//
// feas_tol IS DELIBERATELY REUSED as the activity tolerance rather than
// introducing a third knob: a variable is "on" a bound exactly when the
// primal feasibility measure could not tell it from being on the bound.
//
//     feasibility := max( ||cE(x)||inf,
//                         max_j max(0, cI_j(x)),
//                         max_i max(0, l(i) - x(i), x(i) - u(i)) ).
//
// The bound term is NOT redundant, though it is inert on most rows: the
// subproblem's box is l - x .. u - x, so every iterate the driver PRODUCES is
// inside the bounds by construction. It is the CALLER-SUPPLIED x0 that can
// violate a bound -- solve() does not clamp it -- and then history[0] reports
// that violation honestly instead of claiming feasibility.
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
// WHICH KERNEL THAT IDENTITY BELONGS TO. It is a property of an ACTIVE-SET
// solve and of nothing else: a row outside the working set is ABSENT from the
// KKT system, so its price is zero to machine precision, and a row inside it
// is driven to Ai_j p = bi_j. At qp_mode == kWalk it holds VERBATIM. At kSsn
// the FB kernel supplies only min(s_j, lambda_j) = O(fb_tol), whose product
// form carries an additive fb_tol * ||lambda||inf that does NOT vanish with
// the step; what restores the identity is TIER 3: THE STABLE-FACE REFINEMENT
// (below, in this file), and what a REFUSED refinement costs is stated there.
//
// THAT ARGUMENT IS SUBPROBLEM-SCOPED, AND IT IS VACUOUS AT EXACTLY ONE PLACE
// -- see THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY below, which
// REPLACES it there with a geometric bound of the same family as the
// bound-activity treatment above (a tolerance-scaled bound, not this
// step-vanishing one).
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 214–304

File-level: why an ingested lambda_i on a strictly slack row is cleared, and the bound that replaces the step-vanishing argument. The header keeps the rule itself in the seeded-dual-clamp banner, whose normative ordering depends on it.

```text
// --- THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY ---------------------
//
// THE DEFECT THIS REPAIRS. A warm ingest seeds lambda_e/lambda_i FROM A SOLVE
// OF A DIFFERENT PROBLEM (solve_impl's WARM-START INGEST), and the
// convergence test runs at the TOP of the major loop -- before any subproblem
// of THIS solve exists. The O(||lambda_i|| ||p||) argument above is an
// identity about THE SUBPROBLEM'S OWN complementarity, so at that first test
// there is no subproblem for it to be about and it says nothing at all.
// Without a repair, a previously ACTIVE inequality that goes STRICTLY SLACK
// at the new parameter, while its stale positive multiplier still zeroes gL
// at the old point, passes both gated quantities and certifies kOptimal in
// zero majors at a non-KKT point.
//
// THE REPAIR, in one sentence: ON A WARM (or hot) INGEST, AND ONLY THERE, ANY
// INGESTED lambda_i(j) WHOSE ROW IS NOT GEOMETRICALLY ACTIVE AT THE INGESTED x
// IS SET TO ZERO, BEFORE THE FIRST CONVERGENCE TEST READS IT. Exactly:
//
//     row j is geometrically active at x  :=  cI_j(x) >= -feas_tol
//     lambda_i(j) <- 0                     whenever it is not.
//
// This is the SAME TEST, WITH THE SAME TOLERANCE, that the reduced
// stationarity measure above already applies to BOUNDS.
//
// WHAT IT MAKES TRUE, AND EXACTLY WHAT IT DOES NOT. After the clear, the
// ingested (x, lambda_e, lambda_i) satisfies complementarity BY CONSTRUCTION,
// to the same standard the bound term is held to: every row is either strictly
// slack with lambda_i(j) == 0 exactly, or within feas_tol of its boundary, so
//
//     max_j |lambda_i(j) cI_j(x)| <= feas_tol * ||lambda_i||inf.
//
// **THAT IS A TOLERANCE-SCALED BOUND, NOT THE ONE IT REPLACES.** The vacated
// argument bounded the same quantity by O(||lambda_i|| ||p||), which VANISHES
// WITH THE STEP; this one is proportional to ||lambda_i||, exactly as the
// bound term's own sign-consistency residual is. The PRIMAL error stays
// bounded by feas_tol regardless of that scaling.
//
// ON THE FOUR KKT CONDITIONS, ITEMIZED. At the ingested point the driver
// stands in this position:
//
//   stationarity     GATED (kkt_tol), and it is what the clear un-masks.
//   primal feasib.   GATED (feas_tol).
//   complementarity  NOT gated by this clear alone: established BY
//                    CONSTRUCTION to the bound above, and then GATED
//                    separately -- see the next note.
//   DUAL FEASIBILITY NOT restored by the clear and NOT GATED at kWarm/kHot.
//     (lambda_i >= 0) evaluate_kkt folds a sign-consistency residual into the
//                    reduced stationarity measure for BOUNDS only; a general
//                    inequality row enters grad_lag unconditionally and is
//                    never sign-tested. **IT IS AN INGEST PRECONDITION, NOT A
//                    GUARANTEE** -- warm_start.h's SIGN CONVENTIONS paragraph
//                    states lambda_i >= 0 as part of what a WarmStart IS.
//
// THE CONSEQUENCE OF VIOLATING THAT PRECONDITION: a hand-assembled WarmStart
// carrying a negative price on an active row plus a stale positive price on a
// strictly slack one can certify kOptimal in zero majors at a wrong answer,
// because the clear removes the term that had been breaking stationarity. The
// failure class is not new -- the identical false certificate is reachable
// without the clear from an adjacent malformed input -- and the magnitude is
// bounded by the size of the sign violation, which no shipped producer can
// make large. WHAT IS AND IS NOT CLOSED:
//   - AT kSeeded the condition is GATED (THE SEEDED DUAL CLAMP, below).
//   - AT kWarm AND kHot it remains an INGEST PRECONDITION, deliberately:
//     those levels are hash-gated, and every producer that can clear a hash
//     gate is non-negative (every SqpDriver exit) or bounded by 1e-9 relative
//     (the predictor).
//
// So (stationarity, feasibility) is once again a complete test OF THE THREE
// CONDITIONS THE DRIVER OWNS at that point, given a WarmStart that honours its
// own sign convention. Where clearing the multiplier does NOT move
// stationarity, the zero-major kOptimal is CORRECT and the quadruple returned
// is the self-consistent one; a pure GATE would instead refuse to certify a
// genuine KKT point. NO NEW TOLERANCE KNOB, deliberately: a row is "on" its
// boundary exactly when the primal feasibility measure could not tell it from
// being on the boundary.
//
// WHAT IT COSTS. It CHANGES WarmStart INGEST SEMANTICS -- the ingested duals
// are no longer used verbatim, and warm_start.h says so at the field. It is
// NOT TRAJECTORY-NEUTRAL WHERE IT BINDS, since the cleared multiplier also
// leaves the FIRST subproblem's Lagrangian Hessian; the blast radius is
// bounded exactly, because the clear is a no-op unless some ingested
// lambda_i(j) is nonzero on a row that is strictly slack at the ingested x, so
// COLD solves, models with mi() == 0, and warm solves whose active set
// survives the parameter move are all BIT-IDENTICAL. It DOES NOT TOUCH THE
// SEEDED WORKING SET: an active-set seed is a GUESS with its own correction
// mechanism (the engine prices it out, and qp_engine.h's WINDOW-CONSISTENCY
// RULE drops what no longer fits), whereas a multiplier is DATA the
// convergence test reads and acts on.
//
// The clear is O(mi) and costs NO model evaluation: it reads the NlpEval the
// loop is about to take at the ingested x anyway.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 305–370

File-level: the complementarity gate on an ingested certificate, its tolerance derivation and its scope; and the reported bound multiplier. No header replacement — `SqpKkt::complementarity` and `SqpSolution::z` carry the definitions.

```text
// --- THE INGESTED CERTIFICATE IS GATED ON COMPLEMENTARITY -----------------
//
// THE HOLE THE CLEAR DOES NOT CLOSE, and it is a WRONG-ANSWER hole rather than
// a quality one. The clear establishes complementarity only to the
// TOLERANCE-SCALED bound stated above,
//
//     max_j |lambda_i(j) cI_j(x)| <= feas_tol * ||lambda_i||inf,
//
// whose RIGHT-HAND SIDE IS UNBOUNDED IN ||lambda_i||. An ingest whose
// surviving price is large enough turns "within feas_tol of the boundary"
// into an arbitrarily large complementarity residual -- and, because that
// residual is to first order the objective given up by not moving onto the
// row, into an arbitrarily large OBJECTIVE ERROR under a kOptimal
// certificate. The bound is honoured and the answer is still wrong: the bound
// is not by itself a certificate.
//
// THE GATE. While the multipliers this solve is standing on are still THE
// INGESTED ONES, the convergence test carries a THIRD conjunct:
//
//     kkt.complementarity <= kkt_tol.
//
// THE TOLERANCE IS kkt_tol, ABSOLUTE, AND THAT IS DERIVED RATHER THAN PICKED.
// max_j |lambda_i(j) cI_j(x)| has the units of the Lagrangian, i.e. of f, and
// to first order it IS the objective improvement available by taking up row
// j's remaining slack at the price the certificate itself quotes. kkt_tol is
// this driver's absolute first-order optimality standard, so holding the third
// residual to the same absolute standard adds no new user-facing knob. EVERY
// ||lambda_i||-RELATIVE FORM WAS REJECTED: the wrong-answer class sits inside
// the constructive bound feas_tol * ||lambda_i||inf, so any threshold
// proportional to it is blind by construction.
//
// THE SCOPE IS "UNTIL A SOLVE OF THIS PROBLEM HAS RE-PRICED THE DUALS", not
// "the first test" (a solve whose first trial is REJECTED re-enters the loop
// at the SAME x with the SAME ingested multipliers, so the hole reopens at
// iter 1 -- `duals_ingested` is that second piece of state) and not "every
// major" (which would re-litigate WHAT IS MEASURED BUT NOT GATED, sound once
// a subproblem exists, and would refuse correct converged answers whose
// complementarity is legitimately above kkt_tol). SO THE FLAG IS CLEARED
// WHEREVER A SOLVE OVERWRITES lambda_e/lambda_i: an accepted step, a
// SOC-corrected step, a restoration RESUME (where the multipliers are zeroed)
// and a restoration EXIT. It is also CARRIED BY THE FULL-STEP WATCHDOG's
// best-iterate record, because a restore can put the ingested duals back and
// the gate must come back with them.
//
// COLD SOLVES ARE UNTOUCHED BY CONSTRUCTION: lambda is zero there, so
// complementarity is 0 and `duals_ingested` is false from the start.
//
// **THE EXPOSURE IS NAMED RATHER THAN WAVED AT**: the gate DOES refuse a
// zero-major certificate whenever ||lambda_i||inf is much larger than
// kkt_tol / feas_tol and some priced row sits at O(feas_tol) rather than at
// O(1e-12). Re-ingesting such a solution as a warm start is refused its free
// certificate and pays majors to re-derive it. THE COST IS MAJORS, NEVER
// ANSWERS, which is the trade this gate exists to make.
//
// REPORTED BOUND MULTIPLIER. SqpSolution::z is the MODEL-IMPLIED multiplier
// at the returned point -- z(i) = gL(i) at an active bound, 0 at a free
// variable -- and NOT the subproblem's QpSolution::z. Two reasons, and the
// first is disqualifying on its own: qp_problem.h's STATIONARITY CAVEAT says
// the QP's reported z is FORCED TO 0 at a TR-pinned index, so a caller
// checking grad f + Je^T le + Ji^T li - z == 0 against it would see a spurious
// residual exactly when the radius binds. Second, the z above makes the
// returned quadruple (x, lambda_e, lambda_i, z) a self-consistent
// certificate: the stationarity measure this file reports IS the inf-norm of
// gL - z restricted to where that residual is not absorbed by an active
// bound. lambda_e/lambda_i are carried out from the subproblem unchanged.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 371–475

File-level: SUBPROBLEM FAILURE ROUTING, the one-shot retry, the three restoration sources and the fourth (unevaluable) route. The header keeps the retry predicate's contract on `qp_failure_is_retryable` and the totality note on `map_status`.

```text
// --- SUBPROBLEM FAILURE ROUTING ------------------------------------------
//
// A subproblem that does not return kOptimal is NOT automatically the end of
// the solve, because two of the three failing statuses come back WITH AN
// ITERATE:
//
//   kInfeasible      the linearization has no feasible point. Shrinking the
//                    radius only REMOVES candidate points, so a retry at a
//                    smaller radius is guaranteed to fail. THE ELASTIC TIER
//                    OWNS THIS STATUS ENTIRELY and it never reaches the
//                    routing below at all: the subproblem is REFORMULATED
//                    ELASTICALLY and re-solved at the SAME radius.
//   kNumericalError  the engine reached a point it refuses to certify. The
//                    canonical case is qp_engine.h section 4b's certification
//                    branch, which returns the final iterate with the
//                    multipliers cleared.
//   kMaxIter         the engine ran out of minor iterations. Its x is a
//                    partial, uncertified answer.
//
// The last two are properties OF THIS SUBPROBLEM AT THIS RADIUS. So the driver
// treats such a result exactly as it treats a REJECTED step: Delta shrinks by
// kTrShrinkFactor, rejections_at_iterate++, the iterate does not move, the
// SAME QpProblem is re-solved, and the retry is warm-seeded from the failed
// solve's ACTIVE SET (never its step -- see WARM SEEDING). The step itself is
// discarded and never judged: there is no certified step to judge.
//
// WHAT MAKES A RESULT USABLE is qp_failure_is_retryable below: a finite
// iterate, inside the subproblem's box. See there for why that test and not a
// feasibility one.
//
// THE RETRY IS ONE-SHOT, counted by qp_failures_in_a_row and reset by any
// solve that reaches kOptimal. A subproblem that fails AGAIN at the shrunken
// radius propagates:
//     QpStatus::kNumericalError  -> SqpStatus::kNumericalError
//     QpStatus::kMaxIter         -> SqpStatus::kNumericalError
// (map_status still names kInfeasible -> kInfeasible, and that arm is now
// UNREACHABLE from this path -- the elastic tier consumes every kInfeasible
// before the routing is reached. The arm is kept because the mapping is a
// total function on QpStatus.)
//
// WHY THE MULTIPLIERS SURVIVE THIS PATH. The failed subproblem's lambda_e/
// lambda_i are discarded exactly as a rejected step's are (they price a step
// that was not taken), and on kNumericalError the engine has already zeroed
// them. The ITERATE's multipliers -- the last accepted step's -- are
// untouched, so the retry rebuilds nothing and the Hessian does not move.
//
// WHAT THE FUNNEL SEES: rejections_at_iterate IS incremented on a routed
// failure, so it counts toward globalization.h's kRestoreMinRejections gate
// (conjunct (e)). The radius was shrunk at this iterate without any trial
// escaping the funnel, which is exactly the evidence conjunct (e) stands in
// for; nothing about (e) requires the shrink to have been caused by a JUDGED
// trial.
//
// THE ONE-SHOT BOUND IS PER FAILURE CHAIN, NOT PER ITERATE. The two counters
// have DIFFERENT reset rules --
//     qp_failures_in_a_row  resets on ANY subproblem that reaches kOptimal;
//     rejections_at_iterate resets only when a step is ACCEPTED --
// so an ALTERNATING sequence at one iterate (routed failure, judged kReject,
// routed failure, ...) starts a fresh chain after every successful solve while
// the rejection count keeps climbing. A restoration exit may therefore rest on
// MAJORITY-QP-FAILURE evidence.
//
// THESE ARE STILL DISTINCT FROM A REJECTED STEP in the history: a kReject
// verdict means the subproblem SUCCEEDED and its step was not good enough,
// while a routed failure row carries a non-kOptimal qp_status. Both are radius
// events; only the former was judged. SqpIterate::verdict keeps its kReject
// default on a routed row, which is accurate -- nothing was accepted and the
// iterate did not move -- and it is what keeps a consumer reconstructing the
// ITERATE sequence from the history correct across this path.
//
// A FIFTH ROUTE IS NOT AN EXIT AT ALL: StepVerdict::kRestore -> THE
// RESTORATION PHASE. Three independent sources raise the request, and all
// three enter the same phase:
//   (1) THE FUNNEL'S SIGNATURE (globalization.h's five conjuncts), which
//       fires when the trial is funnel-incompatible at an infeasible iterate
//       whose radius has ALREADY been shrunk kRestoreMinRejections times
//       without escaping -- KLV Lemma 5 case 1's configuration. It is a
//       heuristic EARLY signal and can miss.
//   (2) THE ELASTIC TIER'S EXHAUSTION -- the ladder spent with the relaxation
//       still open and nothing left to reduce. KLV Algorithm 5's
//       authoritative trigger.
//   (3) THE RADIUS FLOOR -- a shrink that would take Delta below
//       SqpOptions::tr_min. KLV Algorithm 4's authoritative trigger, in its
//       trust-region form. See RADIUS MANAGEMENT.
// (1) and (2) are told apart on the triggering history row by
// SqpIterate::elastic_applied; (3) is told apart by that row's tr_radius
// sitting at the floor. Nothing propagates a raw QP kInfeasible any more.
//
// WHAT THE PHASE INHERITS: by the time any of the three fires, every CHEAP
// answer has already been eliminated -- the radius has been shrunk repeatedly
// (1, 3), elasticity has been tried at penalties up to rho_max (2), and SOC
// has been attempted on every qualifying rejection along the way.
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
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 476–587

File-level: SECOND-ORDER CORRECTION — the Maratos effect, the trigger signature, the construction and its counters. No header replacement; the construction is documented at build_soc_subproblem.

```text
// --- SECOND-ORDER CORRECTION ----------------------------------------------
//
// WHY THIS EXISTS: THE MARATOS EFFECT. A pure QP linearization can produce a
// step that is a perfectly good DIRECTION -- second-order sufficient,
// superlinearly convergent -- and yet, at the actual nonlinear point x + p,
// BOTH f and the constraint violation h come out WORSE than at x. This is
// curvature the linearization cannot see (Je is evaluated at x, not along the
// arc to x + p). The funnel has no way to tell "the model was locally wrong"
// from "the step was bad" -- both look like a kReject with h_new > h_old.
// One extra, hot-started QP re-solve buys back exactly that class of
// rejection. SqpOptions::enable_soc (default ON) is the A/B lever.
//
// THE SIGNATURE THAT TRIGGERS IT, tested ONLY on a strategy-judged kReject
// (never on a routed QP failure): h_new > h_old, i.e. the trial point is
// STRICTLY MORE infeasible than the iterate it was taken from. This is
// necessary-not-sufficient for "curvature, not a bad step", exactly as KLV
// Lemma 5's own hypothesis is: it is cheap to test, it is the one symptom SOC
// can repair, and it excludes the common f-type Armijo failure at
// h_new == h_old == 0, where there is no constraint curvature to correct.
//
// WHY NOT ON A ROUTED QP FAILURE. SUBPROBLEM FAILURE ROUTING discards the
// QP's returned x entirely -- it is not a certified step, the strategy never
// judged it, and there is no p to correct. SOC is therefore gated on
// `verdict == StepVerdict::kReject`, which by construction excludes every
// routed-failure row.
//
// THE CORRECTION, KLV/Fletcher's classical construction. The ORIGINAL
// subproblem's linearization enforced Je p = -cE(x), i.e. it predicted
// cE(x + p) = 0; the SECOND-ORDER RESIDUAL is how far that missed. The
// correction re-solves the SAME QP -- same H, g, Ae, Ai, bounds, radius;
// build_subproblem is NOT called again, and neither is eval_hess or any
// Jacobian, so the model stays frozen at x -- with only the rhs shifted to
// cancel that residual. build_soc_subproblem (detail/globalization/sqp/soc.h)
// has the two shifted right-hand sides and the ACTIVE-ROWS-ONLY rule for the
// inequality block. Both cE(x+p)/cI(x+p) are already sitting in `ev_trial`, so
// CONSTRUCTING THE SHIFT COSTS NO EXTRA MODEL EVALUATION AT ALL -- scoped to
// the rhs construction only; the re-solve, and the values-only evaluation it
// triggers on success, are a separate, real cost.
//
// THE RE-SOLVED QP'S OWN SOLUTION IS THE TOTAL CORRECTED STEP FROM x, NOT AN
// INCREMENT ON TOP OF P -- `qs_soc.x` plays EXACTLY the role `p` played for
// the original QP, over the SAME box l - x .. u - x, and the new iterate is
// x + qs_soc.x. Equivalently the INCREMENT (qs_soc.x - p) alone satisfies
// Je(qs_soc.x - p) = -cE(x + p), a frozen-Jacobian Newton correction FROM the
// trial point -- which is why the mechanism repairs exactly the discrepancy
// the frozen linearization introduced.
//
// WARM START, NOT A NEW LINEARIZATION. The re-solve is seeded from the
// REJECTED solve's own QpSolution (its working set, x zeroed -- the WARM
// SEEDING discipline applies unchanged), on the SAME engine, with
// SolveOverrides::tr_radius UNCHANGED. qp_engine.h's HOT-START REUSE
// conditions mostly hold by construction: H/Ae/Ai byte-identical ((a)/(c)),
// and the seed working set equal to the immediately-preceding solve's exit
// working set ((b)). CONDITION (d) HOLDS ONLY CONDITIONALLY -- this re-solve
// always builds DEFAULT SolveOverrides, so the pair matches whenever the
// rescued TRIAL also ran at that default (always with adaptive_mu off; with it
// on, until the schedule quantizes to a smaller decade in the convergence
// tail, after which (d) breaks and K0 is refactorized). NEITHER IS EVERY SOC
// RE-SOLVE FREE EVEN THEN: border_candidate's own checks (perturbed pivots,
// schur_cap, and -- specific to SOC -- a rhs shift large enough to move the
// corrected problem's active set, failing (b)) can still force a rebuild.
//
// JUDGED AS A FRESH TRIAL, WITH ONE DELIBERATE EXCEPTION. The corrected point
// x + qs_soc.x gets its OWN StepContext -- fresh h_new/f_new from a
// values-only eval_nlp_values there (upgraded to a full evaluation only if
// the verdict below promotes it) -- and its own strategy->judge() call. THE
// ONE FIELD DELIBERATELY NOT RECOMPUTED is pred_df: the SOC StepContext
// reuses the ORIGINAL QP's pred_df UNCHANGED, because the correction is a
// CONSTRAINT-RESTORATION step, not a re-optimization. KLV Eq. (10)'s
// switching condition and Eq. (11)'s Armijo test therefore both see EXACTLY
// the decrease the original, rejected QP promised -- only f_new/h_new differ.
//
// GLOBALIZATION.JUDGE() IS THEREFORE CALLED UP TO TWICE FOR THIS ONE ROW,
// once for the raw trial and once for the corrected one. This is a documented,
// narrow exception to globalization.h's "called once per trial" comment: it is
// benign because FunnelStrategy::judge only ever MUTATES state on a kAcceptH
// verdict, and the first call here is by construction a kReject.
//
// ACCEPT vs REJECT, AND THE ROW THAT RESULTS. If the corrected point's
// verdict is kAcceptF/kAcceptH: this row's OWN verdict is OVERWRITTEN to that
// verdict, SqpIterate::soc_applied is set on it, and the driver moves to
// x + qs_soc.x exactly as it would move to x + p on a plain acceptance --
// multipliers, seed and `ev` all come from the SOC re-solve. If the corrected
// point is ALSO rejected (or reads kRestore), the ORIGINAL row's kReject
// stands and the driver falls through to the ORDINARY shrink: ONE rejection
// is charged to rejections_at_iterate for the pair, not two, because from the
// funnel's hypothesis-accumulation point of view this was one radius-shrinking
// event at this iterate. A kRestore read from the SOC judge() call is treated
// as a reject here -- deliberately NOT promoted to an early exit, because the
// elastic tier and the restoration phase own the authoritative triggers.
//
// WHAT COUNTS AS "ONE ATTEMPT". At most one SOC re-solve per REJECTED trial;
// a corrected point that is itself rejected does NOT chain into a second
// correction.
//
// A FAILED SOC RE-SOLVE (qs_soc.status != kOptimal) is treated exactly like a
// rejected corrected point -- no StepContext is built for it, there being no
// certified corrected step to judge, and the ORIGINAL row's kReject stands.
// It is also the MAJORITY outcome, because the rhs perturbation is the FULL
// second-order residual, so a large violation arrives together with a large,
// box-busting shift. SOC is cheap to ATTEMPT precisely because it is expected
// to often fail fast.
//
// THE COUNTERS. counters.soc_steps increments on every attempt; the aggregate
// counters.qp_minor_iters/factorizations gain the SOC re-solve's own cost; the
// triggering row's OWN qp_status/qp_minor_iters/qp_factorizations stay exactly
// the ORIGINAL QP's, preserving every reader's assumption that those three
// fields describe one QpSolution. soc_steps ALONE DOES NOT TELL THE OUTCOMES
// APART: three mutually-exclusive counters -- soc_applied, soc_qp_infeasible,
// soc_rejected (sqp_types.h's SqpCounters) -- are incremented at exactly the
// three branches above, so soc_steps is their sum on every solve.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 588–776

File-level: THE ELASTIC TIER — the construction, the scaled slacks, the rho ladder, the stall early-exit and the exhaustion signature. The header keeps the placement bound and the validation rules on `run_elastic_ladder`.

```text
// --- THE ELASTIC TIER ------------------------------------------------------
//
// WHY THIS EXISTS. A linearization can be INCONSISTENT at a point from which
// the NLP is perfectly solvable: two constraints whose gradients are
// antiparallel AT x impose contradictory linear rows there even though the
// true feasible set between them is wide open a step away. Propagating such a
// subproblem straight out as SqpStatus::kInfeasible reports a fact about the
// MODEL AT ONE POINT as if it were a fact about the PROBLEM. This tier
// replaces that: the same subproblem is re-solved with the offending rows
// RELAXED at a price, the resulting step goes through the ordinary funnel
// judgment, and only an EXHAUSTED relaxation ends the solve. This is also KLV
// Algorithm 5's authoritative restoration trigger.
//
// THE CONSTRUCTION lives at build_elastic_subproblem
// (detail/globalization/sqp/elastic.h), which has the augmented problem, the
// per-block details and the witness argument. What the driver relies on:
// exactly the rows VIOLATED at p_ref = clamp(0, box) are relaxed, one slack
// each, priced at rho * sigma_j with sigma_j = max(1, |residual_j|) the row's
// own COLUMN SCALE, so rho * sigma_j * s_j IS rho times the actual violation;
// NO ENGINE CHANGE IS INVOLVED (a plain QpProblem with n + ns variables, the
// same me and mi -- slacks add COLUMNS, not rows); and the augmented problem
// is FEASIBLE BY CONSTRUCTION, the property everything below rests on.
//
// SO THE ELASTIC QP HAS NO EMPTY-FEASIBLE-SET FAILURE MODE. That is NOT "the
// elastic solve cannot fail" -- the engine can still decline a feasible
// problem, which FINITE, SCALED SLACKS below exists to keep out of reach.
//
// AND CONVERSELY: IF THE SLACKS COME BACK ZERO, THE ORIGINAL QP WAS FEASIBLE.
// The elastic solution then satisfies every original row, so the kInfeasible
// that triggered the tier was FALSE -- a real detection layer for the
// ride-landing false kInfeasible, at the cost of a second solve rather than
// the whole NLP solve. That the retry recovers the ORIGINAL subproblem's own
// answer rather than a relaxed compromise is the l1 EXACT-PENALTY property:
// for rho above the unrelaxed QP's own multiplier norm, paying the penalty is
// strictly worse than satisfying the row, so s = 0 and the elastic solution IS
// the unrelaxed solution. THAT is what the rho ladder searches for.
//
// FINITE, SCALED SLACKS -- THE TIER'S ONE ARITHMETIC CEILING. qp_engine.h's
// is_runaway guard reports kNumericalError when a FREE variable exceeds
// detail::unbounded_artifact_scale toward a bound that could not have
// restrained it. A raw slack is exactly such a variable: its natural size is
// the LINEARIZED VIOLATION, which scales with the problem's constraint scale,
// so on a FEASIBLE NLP whose rows are merely scaled up, an unscaled elastic
// solve can come back kNumericalError and the tier would report "exhausted"
// on a subproblem it never got to try.
//
// THE FIX IS A CHANGE OF UNITS, not a bound: scaling the slack COLUMN by
// sigma_j (and its penalty entry to rho*sigma_j, so the objective still prices
// the actual violation and the exact-penalty threshold is unmoved) puts the
// witness at s_j <= 1 AT ANY CONSTRAINT SCALE. max(1, .) rather than |r_j|
// because a column must never be scaled UP. The mechanism is CONDITIONING,
// not magnitude: it keeps the O(1) objective term that decides WHERE on a flat
// face to stop from being lost against the O(rho*S) constraint-and-penalty
// scale.
//
// THE FINITE CEILING IS KEPT ALONGSIDE THE SCALING, at violation_l1 in actual
// units, on its own merits rather than as the fix: it says the relaxation may
// not leave the linearization MORE violated in total than it already is at
// p_ref, it cannot cut off the witness, and a slack that saturates it is
// reported kAtUpper -- which is_runaway skips outright. WHAT REMAINS
// UNCOVERED: a subproblem whose elastic optimum wants a violation more than
// ~1e7 times the one it starts with still trips the guard; the remedy there is
// SolveOverrides::primal_delta.
//
// THE TRUST REGION IS FOLDED INTO THE BOX, NOT PASSED AS SolveOverrides::
// tr_radius. Correctness requirement, and the least obvious thing in this
// tier: qp_engine.h's section 6 applies the radius to EVERY variable index of
// the problem it is given -- so a radius of Delta would also cap every slack
// at Delta, and the slack a violated row needs has nothing to do with the
// radius; the elastic QP would come back infeasible and the tier a no-op.
// build_elastic_subproblem computes section 6's own window (about p_ref,
// which IS the engine's center for a zeroed seed) and applies it to the
// ORIGINAL block only, passing the default +inf sentinel. The radius BIT is
// then re-derived in elastic_project, so SqpIterate::tr_binding and the growth
// rule behave identically across the paths. ONE CONFIGURATION IS NOT COVERED:
// an engine constructed with a FINITE QpOptions::tr_radius will ALSO apply its
// own radius to the slacks (SolveOverrides has no "+inf overriding a finite
// default" sentinel); there a large violation can leave the elastic QP
// infeasible and the tier degrades to plain failure routing.
//
// THE rho LADDER: rho starts at kElasticRhoInit and is multiplied by
// kElasticRhoFactor while ANY relaxed row's VIOLATION (sigma_j * s_j, not the
// scaled variable -- feas_tol is a tolerance on constraint violation) is
// materially nonzero and the budget lasts, i.e. up to kElasticRhoMax -- six
// escalations, at most seven solves per activation.
//
// THE LADDER IS HOT-STARTED RUNG TO RUNG. Only the slack block of g changes,
// so HOT-START REUSE conditions (a)/(c)/(d) hold across the whole ladder by
// construction; condition (b) (seed working set == immediately preceding
// solve's exit working set) is earned by CHAINING the seed: each rung is
// seeded from the previous rung's solution with x zeroed, not from the
// original kInfeasible solve.
//
// TWO NORMS APPEAR IN THE SLACK TESTS, deliberately: the ladder's stopping
// test is on the MAX violation ("is any row still materially open"); the
// usability test's `closed` arm is on the L1 sum ("is the whole relaxation
// shut"), the norm violation_l1 is measured in. Since max <= l1, the ladder
// can stop with max <= feas_tol while l1 slightly exceeds it, in which case
// `closed` is false and the step is judged on the `reduced` arm instead --
// strictly the more conservative branch. The ladder reads the VIOLATION, not
// the scaled variable: feas_tol is a tolerance on constraint violation, not
// on an internal change of units.
//
// THE STALL EARLY-EXIT (SqpOptions::elastic_ladder_early_exit, default FALSE;
// sqp_types.h carries the caller-facing argument, this is the mechanism).
// kElasticStallScale is a NUMERICAL-ZERO threshold (1e-12 relative, declared
// alongside kElasticRhoInit/Max/Factor in detail/globalization/sqp/elastic.h)
// on two consecutive rungs' solutions -- NOT a bit-for-bit test: the solution
// drifts a little every rung even on the safe class. What makes the exit safe
// there is that most relaxed slacks are pinned at a REAL BOUND, which bounds
// how much of the reduced system CAN read rho. The UNSAFE class is a FLAT
// AUGMENTED OBJECTIVE: where the objective is exactly constant on the feasible
// set at every rho, a later rung finds nothing a one-repeat test missed -- an
// O(1) tie-break getting lost against rho's growing scale, ending at an
// ARBITRARY point of many equally optimal ones. The lever is off because
// "unpredictable which arbitrary optimum you get", not because of measured
// harm. A cheap runtime test separating the classes is NOT known: slack
// bound_state does not do it; a Hessian-based signal is a lead, not a proven
// condition. A looser comparison would only enlarge the false-positive class.
//
// THE STEP IS THEN TAKEN EVEN WHEN THE SLACKS ARE NOT ZERO, deliberately:
// "escalate until the slacks vanish" cannot be the whole rule because THE
// SLACKS CANNOT VANISH ON A GENUINELY INCONSISTENT LINEARIZATION -- s = 0
// implies the original QP was feasible (the converse proved above), so under
// that rule the tier would take a step only in the false-kInfeasible case and
// abandon every real one. A step taken with a slack still open can land
// exactly on the true feasible set, because the TRUE constraints curve where
// their linearization does not; the funnel then judges it on the true f and h
// like any other step.
//
// SO THE EXHAUSTION SIGNATURE IS A CONJUNCTION, shaped like globalization.h's
// own restoration signature -- on MODEL quantities, because here there is no
// trial point to judge. The tier is EXHAUSTED when, after the ladder is
// spent:
//
//   (i)   the elastic solve did not reach kOptimal at all (nothing came
//         back), OR all three of
//   (ii)  the relaxation is still materially open (sum s > feas_tol) -- the
//         MINIMUM achievable linearized violation once rho dominates,
//   (iii) no admissible step reduces that violation (sum s is not below the
//         violation at p_ref, which is what the WITNESS point already
//         achieves for free), and
//   (iv)  the model promises no objective decrease either
//         (predicted_decrease on the original QP <= 0).
//
// (iii)+(iv) say exactly "this point is an infeasible stationary point OF THE
// LINEARIZED PROBLEM" -- KLV Lemma 5 case 1's configuration, at which a
// restoration PHASE is the only remaining move.
//
// THE KNIFE-EDGE RULING: conjunct (iv) tests predicted_decrease(qp,
// p_elastic) > 0.0 with NO tolerance, and EXACT ZERO IS KEPT. Two reasons.
// (1) CONSISTENCY: this is the driver's THIRD copy of "does the model promise
//     a decrease" -- globalization.h's Eq. (10)/(11) machinery reads pred_df
//     <= 0.0 untolerated and the radius-growth rule reads ctx.pred_df > 0.0
//     untolerated; giving only this copy a tolerance would make the SAME
//     quantity decide "promises a decrease" three different ways.
// (2) THE KNIFE-EDGE IS IN THE SUBPROBLEM, NOT THE PREDICATE: on a
//     flat-objective relaxation the two algebra modes land on two different,
//     equally optimal points whose pred_df both read as rounding noise near
//     zero; a tolerance widens the acceptance window without making the modes
//     agree on WHICH point to be at. A future tolerance needs a derivation of
//     pred_df's right SCALE (objective units) and a fixture showing an actual
//     WRONG certificate from exact zero.
//
// WHAT AN EXHAUSTED TIER DOES: raises the kRestore signal, routed into THE
// RESTORATION PHASE exactly like the funnel's signature and the radius floor.
//
// MULTIPLIERS ARE NOT CARRIED OUT OF AN OPEN RELAXATION. At an elastic
// solution with s_j > 0 the row's multiplier is FORCED to rho by stationarity
// in s_j, and the contamination spreads through shared variables to rows
// whose own slack is zero; feeding those to eval_hess would build the next
// Hessian out of the penalty parameter. elastic_project zeroes them unless
// every slack closed -- the same thing qp_engine.h does on its own
// kInfeasible exits, for the same reason.
//
// SOC IS NOT ATTEMPTED ON AN ELASTIC ROW: the correction's premise is that a
// CERTIFIED step was rejected for curvature; here the linearization was
// inconsistent to begin with and the step came from a relaxed copy of it.
// Gated on !elastic_applied. NOTHING IN THIS FILE'S SUITE DISTINGUISHES THE
// GATE -- scoping stated honestly, not a measured necessity.
//
// WHAT IT COSTS, AND WHAT BOUNDS IT. Each activation costs one to seven QP
// solves and NO model evaluation of its own (built from the QpProblem already
// in hand; eval_hess is not called). The tier may re-activate at every trial
// -- there is no one-shot bound like the routed failure's -- because unlike a
// shrink-retry it does not repeat the same question: the exhaustion signature
// terminates the genuinely stuck case and max_iter bounds the rest, so the
// worst case is max_iter * 7 solves.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 777–943

File-level: THE RESTORATION PHASE — the reformulation, its exactness, the multiplier certificate, what is carried in, and the decision table on the way out. The header keeps the recursion bound at the private constructor and the outcome tag on `RestorationOutcome`.

```text
// --- THE RESTORATION PHASE -------------------------------------------------
//
// WHAT IT IS. When a restoration request is raised -- from any of the three
// sources in SUBPROBLEM FAILURE ROUTING -- the driver stops minimizing f and
// starts minimizing the infeasibility measure it has been judging all along,
//
//     h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)),      subject to l <= x <= u,
//
// from the iterate where the request was raised, or from a measured-better
// candidate that site offered (THE START POINT below). Two things can happen,
// and they are the two outcomes KLV Sec. 5.1 names:
//
//   h < feas_tol      -> RESUME. The main loop restarts from the restored
//                        point with the funnel re-based, the radius reset and
//                        the multipliers dropped (all three below).
//   h stationary and  -> CERTIFIED SqpStatus::kInfeasible, and the ONLY exit
//   h > feas_tol         that sets SqpSolution::infeasibility_certified. There
//                        is no direction from this point that reduces the
//                        constraint violation, so the point IS the answer: it
//                        is returned, with the multipliers that certify it
//                        (sqp_types.h's SqpSolution note). This is the
//                        Byrd-Curtis-Nocedal rapid-infeasibility-detection
//                        exit.
//
// NO NEW SOLVER MACHINERY, AND THAT IS THE DESIGN. h is nonsmooth, so it is
// not an NlpModel and cannot be handed to this driver directly. Its standard
// smooth reformulation is (RestorationModel below):
//
// RestorationModel (detail/globalization/sqp/restoration.h), in the variables
// y = (x, sp, sm, si) -- an NlpModel WRAPPER around the caller's own model,
// whose derivatives are trivial extensions of it (the Jacobians gain constant
// diagonal blocks, the objective is linear, and the Hessian is the caller's
// OWN eval_hess called with obj_scale = 0). That header carries the augmented
// problem itself. It is then solved BY THIS SAME CLASS: a nested SqpDriver,
// with the same funnel, the same trust-region rules, the same elastic tier and
// the same SOC.
//
// EXACTNESS OF THE REFORMULATION: at any y feasible for the wrapper,
// sp_i - sm_i = -cE_i/sigmaE_i with both >= 0, so sigmaE_i(sp_i + sm_i)
// >= |cE_i|; likewise sigmaI_j si_j >= max(0, cI_j). Hence f_w(y) >= h(x)
// always, with equality exactly when the slacks are minimal -- so minimizing
// f_w minimizes h, and the START POINT (slacks set to the violations at the
// iterate the phase STARTS from, in CALLER units -- the W2 T4 guard's own
// measure) has f_w = h(x_start) exactly. There is no penalty parameter
// and nothing to escalate: this is the EXACT reformulation.
//
// THE SIGMA SCALING IS THE ELASTIC TIER'S CARRY, APPLIED: the slack COLUMN is
// scaled to the JACOBIAN ROW IT JOINS, fixed once at construction so the
// wrapper's variables have constant units. Both the objective coefficient and
// the column entry carry sigma, so h is still exactly what is minimized and
// the multiplier certificate below is untouched by the scaling. A second
// benefit: the sub-solve's trust region applies to EVERY variable including
// the slacks (unlike the elastic tier, this problem's radius is a genuine
// SolveOverrides radius), and in scaled units the slack a row needs moves at
// the same rate as x.
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
// the box". A converged restoration hands back the certificate for free, in
// the multipliers it was going to return anyway. The entries are pinned to
// sign(cE_i) on violated rows by the same stationarity conditions.
//
// WHAT IS CARRIED IN, AND WHAT IS DELIBERATELY NOT:
//   TRUST REGION: carried, Delta as it stood when the request was raised
//     (resolved to tr_max if it was +inf): a statement about how far the
//     MODEL is trusted at this point, which does not change because the
//     objective did.
//   THE START POINT: the entry x, unless a request site offered a CANDIDATE
//     whose CALLER-scale h is lower (W2 T4) -- x + p_elastic clamped on the
//     exhausted-ladder route, the rejected trial at the judged floor and kRestore.
//   BUDGET: carried, and shared -- the sub-solve gets what is left of
//     max_iter (see SqpCounters), so restoration cannot double a solve's
//     worst-case cost.
//   OPTIONS: tolerances, enable_soc and the whole QpOptions block are carried
//     unchanged; tr_min too, so the sub-solve has its own floor and cannot
//     spin. qp_mode is NOT carried: the sub-solve always runs kWalk.
//   THE STRATEGY FACTORY IS NOT CARRIED. SqpOptions::make_strategy judges
//     THE CALLER'S problem; the restoration problem has different variables,
//     a different objective and a different h. The sub-solve uses the default
//     FunnelStrategy. The OUTER funnel is untouched by the sub-solve and is
//     re-based on the way back.
//   NESTED RESTORATION IS NOT ALLOWED. The sub-driver is constructed with
//     restoration disabled, so a restoration request inside it takes the
//     plain exit -- SqpStatus::kInfeasible -- and the recursion is one level
//     deep by construction.
//
// WHAT RESUMING DOES, in the order it does it:
//   x       <- the restored point; its NlpEval is RE-EVALUATED on the main
//              model (the sub-solve's final evaluation was on the WRAPPER) --
//              one eval_nlp, the same price any accepted step pays.
//   funnel  <- strategy->resume_from_restoration(h_restored), which for the
//              shipped FunnelStrategy is KLV Algorithm 2's re-basing
//              tau_+ = (1-kappa)h + kappa*tau. NOT reset(h): re-initializing
//              would forfeit the funnel's monotonicity (globalization.h).
//   Delta   <- kRestoreRadiusFactor * tr_init, floored at tr_min (+inf
//              tr_init resolves to tr_max first). A DOCUMENTED FRACTION
//              rather than either extreme: carrying the stalled radius
//              forward would hobble the resumed loop; restarting at tr_init
//              would repeat the overshoot at a more delicate point.
//   lambda  <- ZEROED. The multipliers in hand price the wrapper's
//              constraints -- they are subgradient selectors in [-1,1], not
//              NLP prices -- and feeding them to eval_hess would build the
//              first post-restoration Hessian out of them. They are
//              re-estimated by the first main-loop QP.
//   counts  <- rejections_at_iterate = 0 (the iterate moved), the subproblem
//              is marked stale (it must be rebuilt at the new point), and the
//              warm seed is DROPPED: the sub-solve ran on a different engine
//              with a different problem shape.
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
// THE LAST HISTORY ROW'S VERDICT DOES NOT TRACK THIS TABLE AND IS NOT
// UNIFORM. The funnel's signature and the elastic tier's exhaustion both push
// their triggering row with verdict == kRestore, but THE RADIUS FLOOR pushes
// its triggering row with verdict == kReject (both of its routes) -- entering
// restoration from the floor is not itself a judged verdict. The floor route
// is told apart on that SAME row by tr_radius sitting at SqpOptions::tr_min,
// not by verdict. A caller wanting to know WHICH of the three sources raised a
// given restoration exit should read (elastic_applied, tr_radius) on the last
// row, never verdict alone.
//
// A false certified flag on a kInfeasible row means "this driver could not
// make progress and makes NO claim about the model", strictly weaker than the
// status alone reads as.
//
// THE DEGENERATE ENTRY IS BENIGN AND IS NOT SPECIAL-CASED. A request raised
// at an already-FEASIBLE iterate (the radius floor can do this: a collapsed
// radius says nothing about h) finds the feasibility problem solved at its
// own start point, costs at most one major, returns immediately with
// h < feas_tol -- and the RESUME then does the one thing that was actually
// wrong, which is to restart the radius.
//
// ONE RESTORATION PER SOLVE, deliberately under-committed. A second request
// after a resume returns SqpStatus::kInfeasible without running the phase
// again. THE REASON IS CYCLING: restoration moves the iterate to a point
// chosen for feasibility alone, from which the optimality phase may walk
// straight back into the same stall, and a driver that restores every time
// has no mechanism to notice. The alternatives (a decreasing sequence of
// restoration thresholds, or requiring strict h-progress between entries) are
// real and standard, and both need a battery to tune -- the cap is set at one
// and the case is REPORTED (restoration_iters > 0 with kInfeasible).
//
// THE HISTORY ACROSS A RESTORATION. The phase produces NO history rows of its
// own (it is not the caller's problem being iterated), so the row that raised
// the request is followed directly by the first row at the RESTORED point.
// SqpCounters' "recovering the iterate sequence" predicate survives unchanged
// and without a special case, because it reads the PREVIOUS row's verdict for
// `!= kReject` and a kRestore row is exactly a row after which the iterate
// moved. What a consumer cannot do is read a restoration's cost out of the
// history -- that is what counters.restoration_iters is for.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 944–996

File-level: WARM SEEDING — what the seed carries, why `seed.x` is zeroed, and the rejection retry's hot-start case. No header replacement; the zeroing rule is qp_engine.h's window-consistency rule.

```text
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
//   p = 0; seeding x0 = p_prev instead re-centers it on the previous step, so
//   the box becomes p_prev +/- Delta and the driver silently takes steps of
//   up to |p_prev| + Delta while believing the radius is Delta.
//
//   ZEROING seed.x IS NECESSARY BUT NOT SUFFICIENT ON ITS OWN: the rejection
//   retry re-solves the SAME subproblem, and a step that ran into one of its
//   bounds seeds a pin at a NONZERO offset from p = 0, which would re-center
//   the trust region if the engine materialized the pin before computing its
//   window. The fix is ENGINE-SIDE -- qp_engine.h's WINDOW-CONSISTENCY RULE --
//   because the driver-side alternative (never sending bound hints) would
//   forfeit the hot start the retry exists to keep.
//
// Note what warm seeding does NOT buy on a genuinely nonlinear model:
// qp_engine.h's HOT-START REUSE fast path additionally requires H/Ae/Ai's
// VALUES to be unchanged, and they change every major by construction, so K0
// is rebuilt each time. The win here is minor iterations (the active-set
// walk), not factorizations.
//
// THE REJECTION RETRY IS THE EXCEPTION, and it is why qp_types.h has
// SolveOverrides at all. A rejected trial re-solves the SAME QpProblem object
// -- the iterate did not move, so H, g, Ae, Ai and the box are the same bytes
// -- with only SolveOverrides::tr_radius changed, on the same engine, seeded
// from the rejected solve's own working set. That is precisely qp_engine.h's
// HOT-START REUSE case: hashes match, tr_radius is deliberately not in the
// reuse key (bounds never enter K0), and the effective (primal_delta, dual_mu)
// pair is untouched, so the retry can skip K0's assembly and factorization.
// REUSE IS NOT ASSERTED UNCONDITIONALLY, per that header's own warning: the
// five conditions are NECESSARY, not sufficient, and condition (b) fails by
// construction on the SECOND consecutive retry, because a TR-pinned index is
// reported kFree in bound_state and so cannot be reproduced as a seed hint.
//
// THE RETRY MUST NOT RE-CENTER THE TRUST REGION. seed.x is zeroed on the
// rejection path exactly as on the acceptance path, and for the same reason.
//
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 997–1089

File-level: the full-step-first warm rule and its watchdog — when it engages, what is tracked, and what ends it. The constants and their derivation live on sqp_types.h's own fields.

```text
// --- FULL-STEP-FIRST WARM RULE, AND ITS WATCHDOG --------------------------
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
// IT IS A MODE OF THE STRATEGY, NOT A SECOND MAJOR LOOP, by design. Everything
// the loop already does around the verdict -- the elastic tier, the failure
// routing, the radius floor, the restoration phase, the SOC gate, the
// zero-step short-circuit, the history -- is INHERITED unchanged, because from
// the loop's point of view a full-step major is just a major whose strategy
// happened to say kAcceptF.
//
// ENGAGED WHEN ALL THREE HOLD, checked once, at the first measurable iterate:
//   (1) SqpOptions::warm_full_step (default true; the A/B lever),
//   (2) the WARM-START INGEST resolved to kWarm or above -- a cold solve has
//       no nearby-solution premise to lean on and gets the ordinary funnel,
//       and a kSeeded resolution does NOT arm it either: the Kungurtsev-Diehl
//       premise is a warm start ON THE SAME PROBLEM, and a seeded object by
//       definition cannot say which problem it came from,
//   (3) the strategy in hand IS a FunnelStrategy. The mode is that class's own
//       state, so a caller-supplied strategy simply never enters it. That is
//       fail-safe in the right direction (full globalization).
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
// the solve back to the funnel: that is the watchdog -- the mode's own steps
// are unjudged, so the last iterate it produced carries no guarantee whatever,
// while the best one is the best point ANY mechanism in this solve has
// reached.
//
// THE RE-BASING IS THE RESTORATION-RESUME ONE, DELIBERATELY. The exit calls
// strategy->resume_from_restoration(h at the restored point) -- KLV Eq. (13),
// the same call the restoration phase's own resume makes and for the same
// reason: re-entering the optimality phase at a point some non-funnel
// mechanism chose must NOT re-initialize the width by Eq. (9), which would
// push a funnel that had tightened back out to tau_bar and forfeit
// monotonicity.
//
// THE SAFETY INVARIANT, STATED AS A PROPERTY OF THIS FILE: the full-step mode
// CANNOT CERTIFY kOptimal AT A POINT FAILING THE STANDARD KKT CHECK. Nothing
// in the mode touches the CONVERGENCE TEST above. What it changes is which
// trials are ACCEPTED; a solve that runs the mode to convergence exits
// kOptimal through identical code.
//
// THE TRUST REGION IS NOT SUSPENDED, ONLY THE FUNNEL TEST IS. The subproblem
// is still solved at a radius, and that radius still evolves by RADIUS
// MANAGEMENT's ordinary rules -- with one asymmetry:
//   * IT NEVER SHRINKS ON A REJECTION, because the mode produces no
//     rejections; damping the full step is precisely the interference the rule
//     exists to remove. (Delta CAN still shrink on a ROUTED QP FAILURE, which
//     is not a judgement about the step but a statement that the subproblem
//     could not be solved at this radius.)
//   * IT STILL GROWS on a trust-region-active step whose actual decrease
//     matched the model's prediction, so a carried-over warm radius that is
//     too small cannot silently CAP the "full step" into a damped one.
// So under the mode the radius is, in practice, the ingested warm radius,
// possibly grown.
//
// INTERACTION WITH THE SUSPECT GATE AND ANY OTHER SUBPROBLEM FAILURE: a
// full-step major whose QP exits kNumericalError takes SUBPROBLEM FAILURE
// ROUTING, which does not consult the strategy AT ALL, so the mode is never
// asked and cannot accept anything. The next pass measures THE SAME residual
// at THE SAME point -- not a growth, so signal (a) stays put; not a new best
// either, so signal (b) advances. A subproblem that keeps failing therefore
// burns the window and the watchdog ends the mode, restoring an iterate that
// is the one the driver is already standing on: a no-op move, a real mode
// exit, and a counted restore (sqp_types.h's watchdog_restores note). A second
// CONSECUTIVE routed failure ends the solve outright, exactly as it does
// without the mode.
//
// ENTERING RESTORATION ENDS THE MODE. All three restoration request sources
// remain reachable under it except the funnel's own signature (it cannot fire
// from a verdict the mode never asks for), and a resume clears the mode as
// part of resume_from_restoration. A restoration is definitive evidence
// against the mode's premise, so the solve comes back out of it fully
// globalized.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1091–1134

File-level: BUDGETED MODE — what changes on a max_iter exhaustion, the best-iterate ordering, and why the restoration sub-solve is exempt. `SqpOptions::budget_mode` carries the caller-visible contract.

```text
// --- BUDGETED MODE ---------------------------------------------------------
//
// A bounded-iteration solve for a CONTINUATION DRIVER: one that repeatedly
// re-solves a moving problem (or the same problem from successively better
// starting points) under a per-call iteration cap, and needs SOMETHING USABLE
// back even when that cap is hit before the KKT test fires. See
// SqpOptions::budget_mode (sqp_types.h) for the caller-visible contract --
// this note is the mechanics.
//
// WHAT CHANGES, AND WHAT DOES NOT. Only the MAIN LOOP's own max_iter
// exhaustion (`converged` false, iter + restoration_iters == max_iter) is
// affected: the status reported is SqpStatus::kBudgetExhausted rather than
// kMaxIter, and the (x, lambda_e, lambda_i, z, f) reported are the BEST
// ITERATE THIS SOLVE VISITED, not the last one. Every other exit -- kOptimal,
// kInfeasible, a routed subproblem failure, a restoration outcome -- is
// completely unaffected; budget_mode changes nothing about WHEN a solve
// stops, only what it reports when the stop reason is "ran out of majors".
//
// THE ORDERING IS TRACKED SEPARATELY FROM THE FULL-STEP WATCHDOG'S
// (fs_best_*), by design: the two answer different questions and can name
// different iterates as "best" on the very same run. mb_best_* (the loop's
// own state) is updated once per measured pass -- placed AFTER the full-step
// watchdog block so that, on a pass where the watchdog just restored an
// earlier point, the candidate considered is that restored point (the one
// `row` and the eventual history entry describe), never the abandoned point
// the watchdog moved away from. The comparison is a plain lexicographic one
// on (violation_l1, f); ties keep the EARLIER candidate (strict `<`, not
// `<=`).
//
// WHAT THE WARM OBJECT DESCRIBES ON THIS EXIT. `qp`/`qp_built` (hence
// structure_hash) are the same as any other exit's -- structure_hash is a
// pure function of the model's own sparsity, independent of which iterate is
// reported. The ACTIVITY (`seed`'s bound_state/ineq_active), however, is only
// attached when the reported best iterate IS the current one this pass --
// otherwise no in-loop QpSolution describes the returned point and activity
// is left unset, exactly as a restoration-moved exit leaves it unset.
//
// WHY THE RESTORATION SUB-SOLVE IS EXEMPT. The sub-solve's options are a copy
// of opts_ with budget_mode forced back to false: this lever's contract is
// scoped to the MAIN loop only, and the restoration phase running out of ITS
// OWN slice of the shared max_iter budget already has a designated answer --
// kMaxIter, "no budget left to restore with". Forcing the sub-solve's own
// lever off is also what lets the status mapping stay exhaustive with an
// UNREACHABLE kBudgetExhausted arm.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1168–1184

`struct NlpEval`: why the bundle exists and why the Hessian is not in it. The `@brief` kept says what the bundle is.

```text
// Everything the driver needs from ONE model point except the Hessian:
// f, grad f, cE, cI, Je, Ji, evaluated exactly once.
//
// WHY THIS EXISTS: the convergence test and the subproblem construction read
// the SAME five derivative quantities at the same x, and evaluating them
// independently doubles the model's work on every major.
//
// THE HESSIAN IS DELIBERATELY NOT IN HERE. It depends on (lambda_e,
// lambda_i) as well as x, it is the single most expensive thing a model
// computes, and the convergence test never needs one -- so it stays a
// separate eval_hess call made once per ACCEPTED iterate, inside
// build_subproblem.
//
// ONE EXCEPTION: make_warm_start is a SECOND eval_hess call site, and it
// fires only on a solve that built NO subproblem at all (this header's MODEL
// EVALUATION note and make_warm_start's THE ZERO-MAJOR PROBE) -- one Hessian
// on solves that would otherwise have evaluated none, zero on every other.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1208–1213

`eval_nlp`: why the five callback returns are shape-checked. The `@throws` clause kept states the refusal.

```text
/// THE CALLBACK RETURNS ARE CHECKED, NOT ASSUMED, and this is a wrong-answer
/// guard rather than a courtesy: the five RETURNS are the MODEL's, and nothing
/// downstream re-measures them. A short return propagates past allFinite()
/// (vacuously true on an empty vector) into an out-of-bounds read in Release,
/// where Eigen's own asserts are compiled out. Each check is an O(1) integer
/// comparison against a declared dimension, and each throw NAMES THE CALLBACK.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1219–1230

`eval_nlp_values`: the values-only contract and the caller's obligation. Replaced by a `@return` that states the correctly-sized-zeros shape — which the terse docstring in place had wrong, saying grad/Je/Ji were left empty.

```text
// VALUES ONLY -- f(x), cE(x), cI(x) via NlpModel::eval_values, none of
// eval_nlp's grad/Je/Ji. Returns an NlpEval shaped exactly like eval_nlp's,
// so it drops into every helper that only reads f/ce/ci. grad/Je/Ji are set
// to n/(me x n)/(mi x n)-SIZED ZEROS -- an honestly-empty linearization, NOT
// eval_nlp's per-block 0 x n skip -- so a caller who mistakenly reads them
// gets zeros of the RIGHT shape rather than a size mismatch two calls
// downstream. all_finite covers f/cE/cI only.
//
// THE CALLER'S OBLIGATION, not enforced here: use this only where the
// derivatives genuinely go unread (the rejected-trial funnel evaluation and
// the warm-resolution probe), and upgrade to a real eval_nlp the moment a
// caller needs more -- see solve_impl's own upgrade-on-accept sites.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1240–1255

`upgrade_to_full`: why the upgrade is byte-identical to a fresh eval_nlp, and why exactly three returns are checked. The `@brief`/`@throws` kept state both.

```text
// UPGRADES a VALUES-ONLY NlpEval (eval_nlp_values' output, at THIS x) to a
// FULL one, IN PLACE: fills grad/Je/Ji and folds their finiteness into
// all_finite, WITHOUT recomputing f/cE/cI a second time. The result is
// byte-identical to a fresh eval_nlp(model, x) call (nlp_model.h's
// eval_values contract requires its f/cE/cI to already agree with
// eval_f/eval_ce/eval_ci's), at the cost of one call each to
// eval_grad/eval_jac_e/eval_jac_i instead of a second, wasted
// eval_f/eval_ce/eval_ci. Used wherever a values-only trial or SOC-corrected
// point turns out, after judging, to need its derivatives after all.
//
// THIS IS AN EVAL BOUNDARY TOO, and it checks the same three returns eval_nlp
// checks, for the same reason: the resulting ev is what the NEXT major
// linearizes from, and a short grad or a mis-shaped Je/Ji reaches
// evaluate_kkt and build_subproblem unchecked. The cE/cI half is already
// covered (it can only have come from a checked eval_nlp_values), so exactly
// the three derivative returns are checked here.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1266–1289

`constraint_violation_l1`: h(x)'s provenance, why bounds are excluded and why it is not finiteness-guarded. The `@brief`/`@return` kept state the measure and the NaN propagation.

```text
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
// BOUNDS ARE DELIBERATELY EXCLUDED, on the paper's own grounds -- and here
// that is a fact rather than an assumption, because the subproblem's box is
// l - x .. u - x, so every iterate the driver PRODUCES is inside the bounds by
// construction. A caller-supplied x0 can violate a bound; the driver's own
// `feasibility` measure reports that honestly and this one does not, which is
// the intended split of labour: `feasibility` is the CONVERGENCE test
// (inf-norm, all constraints including bounds), h is the GLOBALIZATION measure
// (l1, general constraints only).
//
// NOT FINITENESS-GUARDED, on purpose: a NaN or infinite constraint value
// propagates into the sum and out to the caller, where FunnelStrategy::judge
// rejects the trial outright. Swallowing it here would hand the funnel a
// finite h at a point where nothing was measured.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1297–1302

`struct SqpKkt`: a pointer at the file-level convergence-test note plus the NaN rule. The NaN rule stays on `SqpKkt::finite`.

```text
// The KKT measurement of one NLP iterate. See this header's CONVERGENCE TEST
// note for the definition of every field; `residual()` is the single scalar
// the convergence test and the contraction test both read.
//
// When `finite` is false, stationarity/feasibility/complementarity/residual()
// are all NaN by construction -- see evaluate_kkt.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1326–1347

`evaluate_kkt`: why non-finite iterates are checked explicitly rather than folded through a running maximum. The `@return` kept states the NaN outcome.

```text
// Measures (x, lambda_e, lambda_i) against the NLP, reusing an NlpEval taken
// at the SAME x. bound_tol is the geometric bound-activity tolerance (the
// driver passes SqpOptions::feas_tol; see the header note for why those are
// the same number).
//
// NON-FINITE ITERATES ARE NOT SILENTLY CLEAN, and getting this wrong is a
// wrong-answer bug rather than a robustness nicety. Both measures are folded
// out of per-entry terms by a running maximum, and IEEE says
// std::max(a, NaN) == a -- likewise Eigen's maxCoeff-based
// lpNorm<Infinity>(). A NaN gradient or constraint value would therefore be
// SWALLOWED entry by entry, both measures would read 0.0, and
// `stationarity <= kkt_tol` would fire on a point at which nothing was
// measured at all.
//
// So finiteness is checked EXPLICITLY, up front, and a non-finite point
// yields finite == false with every residual set to NaN -- NaN specifically,
// so that any `<= tol` gate written by any caller is false rather than
// accidentally true. SqpDriver routes this to kNumericalError.
//
// TWO ENTRIES, ONE BODY. This note describes both the NlpModel-taking entry
// below and the seam-taking one further down; the arithmetic itself is
// detail::evaluate_kkt_over.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1350–1357

`detail::evaluate_kkt_over`: why the arithmetic is factored out of both public entries. Replaced by a terse Doxygen block with the same statement in one sentence.

```text
// THE ARITHMETIC OF evaluate_kkt, over the five quantities it actually reads
// off its first argument: the three dimensions and the two bound vectors. It
// evaluates NOTHING -- every model quantity it uses arrives in `ev`.
//
// Factored out because the SQP driver's solve path runs on the Level 2
// aggregate contract: the loop no longer holds an NlpModel, and the seam
// publishes exactly these five. Both public entries forward here, so there is
// ONE body rather than two copies that could drift.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1392–1419

`build_subproblem`: the subproblem statement, the obj_scale rule, and why no trust region is baked in. The `@return` kept carries the statement and the box rule.

```text
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
// this iterate with no flip.
//
// obj_scale scales BOTH the gradient and (through eval_hess) the objective
// part of the Hessian, so it scales the model problem's objective
// consistently. The driver passes 1.0; the parameter exists for callers that
// rescale.
//
// NO TRUST REGION IS BAKED IN. The radius is a per-solve engine override
// (SolveOverrides::tr_radius), never a tightening of `lower`/`upper` here, so
// the returned QpProblem is the pure linearization and a caller retrying at a
// different radius rebuilds nothing.
//
// `ev` must have been produced by eval_nlp at THIS x (see NlpEval);
// eval_hess is the one call this makes itself. NON-FINITENESS IS THE CALLER'S
// OBLIGATION, not checked here: SqpDriver checks SqpKkt::finite and stops
// before reaching here, and make_warm_start's zero-major probe is SKIPPED
// entirely on the one exit whose `ev` can be non-finite (THE UNEVALUABLE
// EXIT).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1447–1465

`predicted_decrease`: the sign convention, why it is a model quantity, and the reader's check. The `@return` kept carries the formula.

```text
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
// SIGN (that disagreement is what a rejection IS). Dropping the negation makes
// pred_df <= 0 on every healthy step, so KLV Eq. (10)'s switching condition
// would fail everywhere and the Armijo condition Eq. (11) -- the one KLV
// Thm. 1 case 2 sums to prove convergence -- would never be tested at all.
//
// Sign convention check, for the reader: for an unconstrained convex model the
// QP's own solution p* = -W^{-1} g gives pred_df = 1/2 g^T W^{-1} g >= 0.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1473–1516

`crash_basis_seed`: the three geometric activity tests, why the seed's x is zeroed, and why an infinite bound falls out on its own. The `@param`/`@return` kept state the tolerance and the empty-seed rule.

```text
// THE CRASH BASIS SEED (SqpOptions::crash_basis -- read that field's note in
// sqp_types.h first; it carries the whole justification, the tolerance story
// and the measured outcome; this comment covers only the mechanics).
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
// feas_tol` -- are exactly the three comparisons this function makes (its
// definition is in src/drivers/sqp_driver.cpp). They are the SAME
// tests, at the SAME tolerance, evaluate_kkt already applies (this header's
// CONVERGENCE TEST note and its INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY
// note); no new tolerance is introduced anywhere.
//
// `seed.x` IS ZEROED, exactly as the kWarm ingest's own seed is: the
// subproblem's trust region centers on p = 0 and the crash basis is a claim
// about the working SET, never about a primal point. The engine then
// materializes each seeded bound onto its own bound value, subject to its
// WINDOW-CONSISTENCY RULE.
//
// A BOUND IS SEEDED ONLY IF IT IS FINITE. qp.lower/qp.upper carry +/-inf (or
// the +/-1e20 convention) for an unbounded side, and `-inf >= -feas_tol` is
// false while `+inf <= feas_tol` is false, so the infinite sides fall out of
// the comparisons on their own -- no separate infinity test is needed, and
// an explicit one would be a second place for the convention to drift.
//
// Returns true iff it seeded anything at all; `rows`/`bounds` receive the
// two counts (SqpCounters::crash_seeded_rows/crash_seeded_bounds). A seed
// that names nothing is NOT offered to the engine by the caller: passing an
// all-free, no-row seed is observationally identical to passing none, and not
// offering it keeps the cold path's call literally unchanged wherever the
// lever finds nothing.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1531–1544

The mid-file include block: why these four headers are included at this position. Replaced by three lines saying the position is load bearing.

```text
// The SECOND-ORDER CORRECTION, ELASTIC TIER and RESTORATION PHASE
// constructions live in detail/globalization/sqp/. They are included HERE,
// at this exact position rather than in the top include block, because
// build_soc_subproblem and RestorationModel consume NlpEval (defined above)
// and their headers must not include this one back -- the positional include
// preserves the original definition order with no header cycle. The
// top-of-file notes those constructions cite remain in this header, where
// the carved comments still point.
//
// The evaluation seam joins them on the same footing:
// detail/drivers/aggregate_eval_seam.h reproduces the evaluation moments
// declared above against an NlpAggregate, so it consumes NlpEval and states
// in its own banner that it is deliberately not self-contained. It is what
// the driver's solve path evaluates through.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1552–1558

The seam-taking `evaluate_kkt`: which entry the driver uses and why the seam is const. Both statements are in the `@brief`/`@param` kept.

```text
// evaluate_kkt over the SEAM's dimensions and materialized box, which are the
// model's own. This is the entry the driver's major loop uses; the
// NlpModel-taking one above is for every caller measuring a single point from
// a model in hand. See that entry's note -- it documents both.
//
// The seam is taken by CONST reference: this measurement evaluates nothing,
// so it neither re-lays nor scatters.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1572–1601

`qp_failure_is_retryable`: the status arm and the iterate arm, in full. The `@return` kept states the predicate.

```text
// Is a FAILED subproblem's result usable as a rejected trial, i.e. may the
// driver shrink the radius and re-solve instead of giving up? See this
// header's SUBPROBLEM FAILURE ROUTING note for the algorithm; this is the
// whole of the decision, factored out so it can be tested away from any
// engine.
//
// THE STATUS ARM. kOptimal is not a failure. kInfeasible is a failure that
// shrinking cannot fix and must not be retried: a smaller radius only REMOVES
// candidate points from a linearization that already has none, so the retry is
// guaranteed to fail. IN THIS DRIVER THAT STATUS NEVER ARRIVES HERE AT ALL --
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
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1613–1731

The semismooth-Newton tier's section banner: the routing rule and its three justifications, why an escaped x is not a step, why the adaptive-mu schedule is off under kSsn, tier 3's stable-face refinement, and what is not routed through SSN. Replaced by a three-line banner naming the rule.

```text
// =============================================================================
// THE SEMISMOOTH-NEWTON TIER, AND WHAT THE DRIVER DOES WITH IT
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
// That is ssn_engine.h's own standing instruction (its SsnEscape banner), and
// it is a rule with three separate justifications, each sufficient alone:
//
//   (1) NO ESCAPE CERTIFIES ANYTHING. `SsnEscape::kInfeasibleSuspect` is a
//       DIAGNOSIS FROM BEHAVIOUR (a stalled residual while the multiplier norm
//       grows), never a Farkas certificate -- but it reports
//       `QpStatus::kInfeasible`, which the WALK issues as a certificate and
//       which this driver's elastic tier consumes as one. A driver that
//       branched on the status rather than on the escape would promote a
//       suspicion to a certificate at the driver layer.
//   (2) THE LABEL IS NOT RELIABLE ENOUGH TO BRANCH ON. The safeguard traded
//       infeasibility RECALL for precision, so genuinely infeasible
//       subproblems escape `kNoContraction` or `kBudget` more often than
//       `kInfeasibleSuspect`. A rule keyed on `kInfeasibleSuspect` would MISS
//       most infeasible subproblems while ALSO mis-routing its false
//       positives.
//   (3) UNIFORM ROUTING STILL REACHES THE ELASTIC TIER, and reaches it with
//       better evidence: an infeasible linearization handed to the walk comes
//       back QpStatus::kInfeasible -- the walk's own certificate -- and the
//       elastic tier fires on it exactly as it always has. The destination is
//       unchanged; only the authority for the claim improves.
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
// an irreducible `dual_mu * |lambda|` footprint (the ADAPTIVE DUAL
// REGULARIZATION note below). The SSN kernel has no such footprint: delta and
// mu perturb only its JACOBIAN and never its residual, so the iteration is
// modified Newton on the EXACT F and its fixed points are exact, unregularized
// KKT points -- there they cost iterations and buy nothing. So the schedule is
// off for the WHOLE solve under kSsn, walk fall-backs included, and
// `SqpIterate::mu` reports `opts_.qp.dual_mu` on every row of such a solve --
// which is the truth about what every kernel on that solve was actually
// handed. At kWalk not one byte of the schedule changes.
//
// --- TIER 3: THE STABLE-FACE REFINEMENT ON A CERTIFYING EXIT ----------------
//
// A CERTIFYING SSN EXIT IS NOT THE END OF THE SUBPROBLEM. Every one of them is
// handed to QpEngine::refine_on_face for ONE EXACT equality-constrained solve
// on the face the kernel identified, reusing the walk's own solve_eqp rather
// than rebuilding it, and never re-entering the walk's search.
//
// IT IS NOT A POLISH STEP; it is what makes this driver's convergence test
// sound in this mode. The WHAT IS MEASURED BUT NOT GATED note at the top of
// this file declines to gate NLP complementarity on an identity that belongs
// to an ACTIVE-SET solve and to nothing else; an FB kernel stopping at
// |phi| <= fb_tol supplies only `min(s, lambda) = O(fb_tol)`, whose product
// form carries an additive `fb_tol * ||lambda||inf` that does not vanish with
// the step. The refinement's answer satisfies the identity by construction.
//
// IT CAN BE REFUSED, and a refusal is not an error: the caller keeps the
// certificate the SSN tier already gave it. What it means is that THAT
// subproblem is back on the fb_tol bound, and
// SqpCounters::ssn::ssn_refine_refused is how a reader knows how often.
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
// the phase's cost law lives. HOW EACH IS KEPT THERE DIFFERS:
//
//   ELASTIC RUNGS are structurally walk-only: the ladder is entered on a
//     QpStatus::kInfeasible CERTIFICATE, which under kSsn can only have come
//     from the walk after a hand-off, and every rung calls engine_.solve
//     directly. The rung chaining does hot-start off the immediately
//     preceding WALK solve (the ELASTIC TIER note).
//
//   THE SECOND-ORDER CORRECTION is structurally walk-only in its SOLVER and is
//     SEEDED FROM THE SSN's OWN SOLUTION -- `seed_soc = qs`, where `qs` may be
//     ssn_result_to_qp_solution's export. That is sound and deliberate: both
//     kernels agree on the two conventions that matter (a TR pin reports kFree
//     plus tr_active; a real lower == upper reports kFixed). THERE IS NO
//     CAPABILITY GAP: SOC is available in BOTH modes, which is what makes a
//     mode comparison fair.
//
//   THE RESTORATION SUB-SOLVE runs whatever `qp_mode` its SqpOptions carry,
//     so it is reset to kWalk EXPLICITLY where the sub-options are built. The
//     sub-solve's own `counters.ssn` is folded, so letting the mode leak back
//     in moves a pinned number.
//
// THE CRASH BASIS likewise stays a walk-only mechanism (SqpOptions::
// crash_basis): it builds a QpSolution seed, which is what the walk consumes.
// Under kSsn it is still derived and still counted -- the counters'
// documented convention is SEEDS OFFERED, not seeds honoured -- and it is
// consumed if and only if the subproblem hands off.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1733–1752

`kSsnTrViolationFactor`: where the bound comes from and why it must not be tuned. The `@brief` kept says it is derived from ssn_engine.h's own constant.

```text
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
// **PROVABLY INERT ON THE CERTIFYING PATH, AND DELIBERATELY KEPT**: no
// certifying exit ssn_engine.h can produce reaches this gate, so no NLP
// fixture can drive it. It is kept because it is the single point at which a
// step whose norm the funnel has not been told about could enter the
// acceptance test, and it is made fixturable through the free predicate below.
// DERIVED FROM ssn_engine.h's OWN CONSTANT, never restated as a literal
// (`const double` rather than `constexpr` because the constant it is built
// from is itself a runtime-initialized `std::sqrt` expression).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1758–1770

`ssn_exit_is_a_usable_step`: the three conjuncts and why each is separate. The `@return` kept states all three.

```text
// Does this SSN exit hand back something the funnel may use as a trial step?
//
// THREE CONJUNCTS, each a separate way for the answer to be no:
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
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1780–1812

`ssn_result_to_qp_solution`: the counter mapping, field by field, and why minor_iters is deliberately not folded. The `@return` kept states the status and the activity export; `charge_ssn_subproblem_cost`'s `@param` keeps the minor_iters exclusion.

```text
// An SSN exit, in the shape every consumer downstream of a subproblem already
// speaks. CALLED ONLY ON A CERTIFYING EXIT (ssn_exit_is_a_usable_step above
// true); an escaped result never reaches here, which is why the definition
// (src/drivers/sqp_driver.cpp) writes the status as the constant it is rather
// than mapping it.
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
//     in. Folding SSN steps into it would silently corrupt every one of those
//     comparisons. So an SSN-solved subproblem contributes ZERO to
//     qp_minor_iters, and its work is reported in `SqpCounters::ssn` instead.
//     A kSsn solve's qp_minor_iters is thus exactly "the minors the WALK spent
//     on this solve", which on a solve with no hand-offs is 0 -- and that 0 is
//     a reading, not a gap.
//   - `schur_updates`, `eqp_refine_steps`, `border_refine_steps`,
//     `suspect_escalations` and `k0_reused` are walk mechanisms with no SSN
//     analogue at all and stay at 0.
//
// THE ACTIVITY EXPORT carries across verbatim, which is sound because both
// kernels already agree on the two conventions that matter: a TR-pinned
// variable reports kFree in `bound_state` with z = 0 and is flagged in
// `tr_active` (so a radius artefact can never be re-ingested as a genuine
// active bound), and a variable at a real lower == upper reports kFixed.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1820–1863

`sweep_negative_face_prices`: why the sweep exists, why it is at the export boundary rather than the adoption site, and what it leaves. The `@param` kept states the two counters.

```text
// THE R6 SIGN SWEEP ON EXPORTED FACE PRICES (M6 W0.3).
//
// A certifying SSN exit's inequality prices are NOT sign-constrained where
// they are produced -- the FB kernel bounds them only by
// `lambda >= -O(fb_tol)`, and the tier-3 refinement's `price(...)` is
// unbounded in sign outright -- and nothing downstream of this driver
// re-gates them. This is the gate: every strictly negative price is clamped
// to 0, which is dual feasible for an inequality row at any activity.
//
// CALLED FROM `SqpDriver::finish` AND NOWHERE ELSE. That function is this
// driver's single export boundary -- every exit of `solve_impl` returns
// through it but the non-finite-start exit, which zeroes its multipliers
// outright -- so one call there covers `SqpSolution::lambda_i` and
// `WarmStart::lambda_i`, hence the currency's `iq_lmults_` and every
// interior-point seed downstream of it.
//
// **NOT AT THE ADOPTION SITE, AND THAT IS A MEASURED RULING RATHER THAN A
// PREFERENCE.** Sweeping where a face price is adopted INTO THE ITERATION --
// on `qs`, or on `lambda_i` before the next subproblem is built -- injects a
// stationarity floor equal to the clamped magnitude: the row stays on the
// identified face at zero price, the term the stationarity condition wanted
// from it is gone, and the next subproblem re-derives the same negative price.
// That costs a corpus cell its certificate outright, turning a converged solve
// into a budget-exhausted one, so the adoption site is not available to this
// repair. The export boundary moves no counter, no status and no iterate on
// either arm. See the M6 ledger's W0.3 entry for the placement measurement and
// its section 7 declaration.
//
// WHAT IT LEAVES, STATED SO IT IS NOT DISCOVERED LATER: `SqpSolution::kkt`
// was computed at the multipliers the solve actually reached, i.e. BEFORE
// this sweep. On a solve with `ssn_sign_swept > 0` the reported stationarity
// is therefore optimistic by at most `ssn_sign_sweep_max * ||Ji||inf` over
// the swept rows, and an independent re-scoring of the returned point reads
// the larger value. The counters are what make that gap computable; see their
// own note in solver_counters.h.
//
// NOT A TOLERANCE TEST: the comparison is `< 0.0`, matching
// `ssn_refine_neg_duals`. A NaN price compares false and is therefore NOT
// swept -- the same discipline the B-1 ingest clear states, and for the same
// reason: it belongs to the non-finite exit, not to a quiet repair.
//
// UNCONDITIONAL IN `qp_mode`, and inert under kWalk: an active-set price is
// non-negative by the walk's own drop rule. The invariant belongs to the
// exported vector rather than to one kernel's path to it.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1871–1898

`ssn_start_from_qp_seed`: why the start is built from the walk's own seed, why x is left empty, and what has already happened to the multipliers. The `@param`/`@return` kept state the null case.

```text
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
// that centre off p = 0.
//
// THE ACTIVITY HINT is the seed's binary activity, both halves. It can never
// carry a trust-region artefact: `QpSolution::bound_state` is a REAL-BOUND-ONLY
// view in both kernels (a TR-pinned variable reports kFree there and is flagged
// in `tr_active`, which this function does not read), so a pin created by a
// radius in one major cannot be asserted as a bound in the next.
//
// THE MULTIPLIERS come from the seed unchanged. Note what has ALREADY happened
// to them by the time any of this runs: on the FIRST subproblem the seed's
// duals are the solve's ingested duals, which the geometric complementarity
// clear and then the seeded dual clamp have already corrected, IN THAT ORDER
// (this header's THE INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY and THE
// SEEDED DUAL CLAMP notes; the ordering is normative). This function is
// downstream of both and re-does neither.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1905–1917

`ssn_fb_tol_for`: why the tolerance is the tighter of the two. The `@return` kept states the min.

```text
// The FB residual tolerance one subproblem's SSN solve runs at, from the
// DRIVER's own two tolerances.
//
// IT IS THE TIGHTER OF THE TWO. ssn_engine.h section 5 establishes that an FB
// residual at `t` buys stationarity and equality feasibility at `t` -- so a
// caller who asks for `feas_tol < kkt_tol` (a legitimate, documented
// configuration: the two are independent fields of SqpOptions) would otherwise
// get a kernel whose own certificates can sit ~1.71 * kkt_tol outside the very
// feasibility bar the driver's convergence test then applies, a DNF/stall
// asymmetry against kWalk. THE OTHER DIRECTION IS ALREADY SAFE and is not
// changed: `feas_tol > kkt_tol` leaves the min at kkt_tol.
//
// A FREE FUNCTION, so the rule is testable without a driver.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1926–1938

`charge_ssn_subproblem_cost`: why two fields and only two, and why it is a free function. The `@param` kept states the two fields and the exclusion.

```text
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
// either field wrong there is silent -- the work simply vanishes -- so the
// rule is stated once here, where both fields are directly assertable.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1945–1970

`charge_refused_face_refinement`: why a second charging site is needed at all, and which four fields it writes. The `@brief`/`@param` kept state what it charges.

```text
// The REFUSED-AND-THEN-WITHDRAWN face refinement under
// SqpOptions::ssn_certify_from_face, charged to the solve's own totals. THE
// SAME VANISHING-WORK CLASS as the function above, on the one path the
// certifying branch cannot reach.
//
// WHY A SECOND SITE IS NEEDED AT ALL. Under
// SqpOptions::ssn_certify_from_face the tier-3 face solve is HOISTED ahead of
// the driver's usability gate, because it is now the thing that decides
// whether the certificate stands -- so its factorization is paid BEFORE the
// driver knows whether the exit will be used. On every path but one the
// certifying branch consumes that solve and charges it. The exception is the
// pair: the face solve REFUSED, and the deferred verification it then fell
// back to WITHDREW the certificate (kIndefinite or kSingular). The result
// becomes an escape, the usability gate now reads false, and the HAND-OFF
// branch runs instead -- where the refinement's factorization has no other
// accumulation site and was simply lost: not in SqpCounters::factorizations,
// not in `ssn_refine_factorizations`, not in `ssn_refine_refused`, and not in
// the probe budget it silently under-charged. INERT at the shipped default,
// where nothing is ever deferred and no face solve is ever hoisted.
//
// THE FOUR FIELDS ARE THE FOUR THE CERTIFYING BRANCH WRITES FOR A REFUSAL;
// `symbolic_analyses` is absent because QpEngine::refine_on_face reports only
// `factorizations` and `eqp_refine_steps`. The probe budget is charged through
// the reference for the same reason: an escape's factorizations were spent
// either way. A FREE FUNCTION, so the rule itself is falsifiable without
// reproducing the rare dynamic that reaches it.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 1982–1989

`census_major_activity`: why the definitions are `SqpIterate`'s and why the function is exposed rather than file-local. The first clause is kept in two lines; the rest of the block was tightened in place.

```text
/// EVERY DEFINITION IS `SqpIterate`'s -- the delta's per-variable-state-change
/// rule, the empty-set first major, the nonnegative slack `s = bi - Ai p`, and
/// why a TR-held variable is not a bound side here. Nothing is decided in the
/// function that is not stated on the fields.
///
/// EXPOSED RATHER THAN FILE-LOCAL so the arithmetic can be pinned on hand-built
/// active-set pairs, where the two sets are known by construction instead of
/// predicted from a trajectory. The driver calls it at exactly one site.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2016–2036

`accumulate_ssn_counters`: the max-fold rule stated as a rule, and the driver-scale readings of three fields. The `@param` kept states the fold rule.

```text
// One subproblem's SSN work, folded into a whole solve's running total.
//
// TWO PEAKS FOLD BY MAX; EVERYTHING ELSE SUMS. `ssn_uncertain_peak` is the
// largest uncertain set any single subproblem's Jacobian assembly held, and
// `ssn_sign_sweep_max` is the largest magnitude the R6 sign sweep clamped.
// Summing either would report a quantity with no meaning. Stated as a rule
// rather than a count, so a field added beside them does not silently falsify
// this paragraph.
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
// the RESTORATION sub-solve's totals, where they could be nonzero in principle.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2044–2064

`accumulate_ipqp_counters`: the fold rule field by field, with its derivation. The `@param` kept states the fold rule without the derivation.

```text
// One subproblem's IP-PMM tier work, folded into a whole solve's running
// total -- the `accumulate_ssn_counters` of M6 W1's third QP kernel, same
// shape. LANDS INERT (task 2): unreachable from any dispatch until task 6.
//
// THE FOLD RULE (`IpqpCounters`'s own doc comment states it in full):
// - `ipqp_rho_demanded_max`, `ipqp_restart_shift_max` fold by MAX (the
//   `ssn_sign_sweep_max` model);
// - `ipqp_tier_retired_after` also folds by MAX (fix round 1, Codex
//   co-review I1): it is a once-per-solve MAJOR INDEX, not a count, so
//   summing two nonzero readings (e.g. majors 4 and 7) would report an
//   impossible major ("11"); 0 (never retired) is the fold identity, same
//   as the peak fields above. Driver-scale only -- no `IpqpCounters`
//   produced by a real subproblem solve ever carries it nonzero -- but
//   max-folded anyway for the one real call site (a restoration sub-solve's
//   totals onto an already-populated running total);
// - `ipqp_alpha_p_min`, `ipqp_alpha_d_min` fold by MIN (the same model,
//   mirrored);
// - `ipqp_rho_demanded_last`, `ipqp_final_inertia_read` are OVERWRITTEN by
//   `one`'s value -- categorical per-subproblem status, not an additive
//   quantity, the `SqpCounters::start_level_used` convention;
// - every other field SUMS, including the five-way escape census.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2070–2073

The interior-point seam functions' banner: why each is a free function. Deleted from the header; the same statement is in the SSN banner's archive entry.

```text
// THE INTERIOR-POINT TIER'S SEAM FUNCTIONS, one for one with the SSN set
// declared above (spec section 9). Free functions for the same reason those
// are: each is a pure mapping between two published shapes, so each is
// testable without constructing a driver.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2075–2082

`ipqp_exit_is_a_usable_step`: why this gate differs in kind from the SSN one and why it does not read `status`. The `@brief` kept states the predicate.

```text
// The gate on a tier exit, section 2.3 item 1. DIFFERENT IN KIND from
// `ssn_exit_is_a_usable_step` and deliberately: the SSN gate re-checks a
// trust-region violation because that kernel solves the TR as a penalty and
// can exit outside it, while this tier keeps the radius as a HARD BOX no
// iterate ever leaves. It does NOT read `status` -- `IpqpResult`'s standing
// rule -- so a downgraded certificate (`kNumericalError`, `escape_reason ==
// kNone`) is usable, which is exactly what a downgrade means: the point
// converged, the second-order certificate did not stand.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2090–2093

`ipqp_result_to_qp_solution`: why `status` is forced to kOptimal. The `@brief` kept states the mapping.

```text
// The tier's answer in the QP layer's currency. `status` is forced to
// kOptimal exactly as the SSN mapping forces it: the caller has already judged
// the exit usable, and the tier's own `status` is not the currency the
// driver's routing reads (`certificate_downgraded` and `escape_reason` are).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2098–2101

`charge_ipqp_subproblem_cost`: why it is charged only where the tier's answer is discarded. The `@brief` kept names the abandoned result.

```text
// The cost an abandoned tier subproblem paid, charged to the solve's totals --
// `charge_ssn_subproblem_cost`'s counterpart, the same two fields and the same
// rule: charged ONLY where the tier's own answer is discarded, because on the
// adopted path the two travel across inside `ipqp_result_to_qp_solution`.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2107–2109

The elastic ladder's banner: one implementation serving two callers. Deleted from the header; `run_elastic_ladder`'s own block says what it does.

```text
// THE ELASTIC LADDER AND ITS JUDGE, one implementation serving the driver's
// elastic tier and W2's certified fallback both. Every design choice below is
// this header's ELASTIC TIER / STALL EARLY-EXIT / EXHAUSTION SIGNATURE notes.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2127–2134

`ElasticLadderReport::rho_0` and `::rho0_ceiling_hit`: the two field notes, rewritten in place as `@brief` blocks carrying the same statements.

```text
    /// The FIRST rung's penalty, as THE PLACEMENT BOUND below resolved it. Not invertible from
    /// the report's `elastic` block once the ladder has climbed, and the retry rule's own input.
    double rho_0 = 0.0;
    /// True iff THIS LADDER's placement was CLAMPED -- headroom or dual_mu (THE PLACEMENT BOUND)
    /// -- i.e. the evidence priced the violation above the rung the rule allows. Aggregated over
    /// a solve by `SqpCounters::elastic_rho0_ceiling_hits`. On the report the certified fallback
    /// returns, it is THE ENTRY's clamp: a declined clamped attempt retried at the floor carries
    /// its flag onto the retry's report, which is what the row and the trace event then read.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2155–2194

`run_elastic_ladder`: the placement bound with its derivation, and why the ladder's top is deliberately not capped. The `@param rho_0_override` kept carries the bound formula and the first-rung-only scope.

```text
/// @brief Runs the rho ladder to exhaustion and judges the rung it stopped on.
/// `window` is the radius folded into the elastic box: nonnegative -- 0 is legal
/// (`tr_radius` permits it), FINITE on the shipped path, and VALIDATED (a negative
/// or NaN one throws std::invalid_argument). Every rung's counters fold into `out`.
/// THE FIRST RUNG'S PENALTY is THE PLACEMENT BOUND below, or `rho_0_override`
/// clamped into `[kElasticRhoInit, kElasticRhoMax]` when the caller names one --
/// the retry rule's route back to the floor, which reads no evidence at all and
/// leaves `rho0_ceiling_hit` false. An override is VALIDATED like `window`: NaN
/// or non-positive throws std::invalid_argument rather than resolving silently
/// to the floor (+inf is legal, and clamps to `kElasticRhoMax`).
///
/// THE PLACEMENT BOUND (amendment H, plus W2 T5's safety margin):
///     rho_0 = max(kElasticRhoInit,
///                 min(evidence-priced start,
///                     kElasticRhoMax / kElasticRhoFactor,
///                     kElasticRhoDualMuSafety / opts.qp.dual_mu))
/// with the evidence-priced start `max(kElasticRhoInit, evidence->dual_norm_start)` and
/// `kElasticRhoInit` when no arm FIRED at all. The FLOOR keeps the ladder from being entered
/// cheaper than W1's; the headroom cap leaves at least one escalation above the placement, where
/// amendment H's ceiling left none; the dual_mu cap is a MARGIN against the walk's false
/// `kInfeasible` on the elastic copy (elastic.h's `kElasticRhoDualMuSafety` carries the measured
/// law, W2 T6b carries the cover for it). A non-finite or non-positive `dual_mu` disables that
/// cap rather than degrading the placement. Either cap binding sets `rho0_ceiling_hit`, which is
/// read off the RESULT: where the floor outranks a cap (`dual_mu >= 1e-4`, whose cap is below
/// `kElasticRhoInit`) AND the evidence's own price is at the floor, nothing was clamped and the
/// flag is false; a price ABOVE the floor still reads clamped, because the placement is below it.
///
/// THE MARGIN BINDS THE FIRST RUNG ONLY. The ladder escalates x10 with no knowledge of the cap,
/// so a clamped ladder still climbs to `kElasticRhoMax` -- product 1 at the shipped `dual_mu`,
/// which was inside the BASE-era misfiring band and, measured after W2 T6b, is not -- and the
/// retry buys a solve, not safety. Capping the ladder's TOP is deliberately NOT done here: a
/// ladder climbs only while the relaxation is OPEN, so a top cap would turn a closable row into
/// an EXHAUSTION and send it to restoration off a false signal. W2 T6b covers the misfire at the
/// QP engine's own verdict site, in border mode; the margin is the cheaper first-rung guard.
///
/// @param sink  OPTIONAL trace sink, DEFAULTED so every direct caller (the W2
///        pins call this function without a driver) keeps compiling unchanged.
///        Non-null makes each RUNG's walk write its own `qp.mode` line at
///        `site` `elastic_rung` -- the only way the stream reconciles against a
///        climb whose length is not known in advance (M6 W4 T5).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2201–2224

`certified_feasibility_fallback`: the two-rung structure, the retry rule, the refusal cost and why it is a named free function. The `@brief` kept carries the structure and the retry.

```text
// THE W2 HOOK, AND THE ESCAPE BRANCH'S SINGLE ENTRY POINT (spec 2.3 item 5,
// section 6.3's Amendment C registration).
//
// ITS BODY IS A BOUNDED TWO-RUNG LADDER (W2 plan section 2): RUNG A is the
// elastic QP at the evidence's own rho_0, always; RUNG B is W1's COLD walk,
// reached only on a rung A the engine DECLINED, and the refusal path's carrier.
//
// A DECLINED RUNG A PLACED ABOVE THE FLOOR IS RETRIED THERE ONCE (W2 T5) before rung B is
// considered: the evidence's price is a hint, and a hint that costs a solve is worth one
// re-entry at W1's own penalty. The retry is a second activation and is counted as one.
//
// REFUSAL COSTS ONE EXTRA DECLINED SOLVE, and if rung B's walk then certifies
// kInfeasible the driver runs its OWN ladder: two activations, one subproblem.
// IT EXISTS AS A NAMED FUNCTION BECAUSE W2 REPLACES ITS BODY, not its call
// site, and it is FREE rather than a member for the reason the three seam
// functions above are: it can then be pinned without constructing a driver.
// `ev` and `seed` are UNUSED STILL -- rung A seeds from the evidence, rung B
// is COLD -- and are named rather than omitted. `evidence` is section 6.3's own
// block -- the least-infeasible point and the optional Farkas corroboration --
// which is what lets W2's elastic l1-penalized reformulation answer "is this
// subproblem infeasible" by SOLVING something always-feasible rather than by
// accumulating symptoms. `fired` gates rung A, the least-infeasible point AND
// ITS DUALS seed it, and `dual_norm_start` places rho_0; nothing else is read
// (P7 mutates the verdict-side fields only).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2259–2314

The adaptive dual regularization banner: the accuracy ceiling it attacks, the schedule, the quantization argument, and why primal_delta is not scheduled. Replaced by a banner carrying the schedule itself.

```text
// =============================================================================
// ADAPTIVE DUAL REGULARIZATION. Caller-visible surface:
// SqpOptions::adaptive_mu (sqp_types.h) and SqpIterate::mu (same file); this
// is the mechanism behind both.
//
// THE ACCURACY CEILING THIS ATTACKS. qp_engine.h's regularized KKT system
// carries an irreducible dual_mu*|lambda| footprint on every constraint row,
// and eqp_solve.h's one step of iterative refinement only knocks the resulting
// error in x down by roughly the SQUARE of that footprint relative to the
// row's own conditioning -- on a badly scaled active set the fixed engine
// default dual_mu = 1e-8 still leaves a relative error around 1e-4 in the
// RETURNED x, well short of kkt_tol/feas_tol's default 1e-6.
// Shrinking mu attacks the footprint directly (it is linear in mu),
// but shrinking it UNCONDITIONALLY is not free: a smaller mu is a
// worse-conditioned regularized system, a real hazard EARLY in a solve, far
// from whatever active set the iterate eventually settles on.
//
// THE SCHEDULE ties mu to how converged the driver already believes it is:
//     mu_k = clamp(kappa_mu * ||KKT residual||^1.5, mu_min, mu_max)
// with kappa_mu = 1 (kAdaptiveMuKappa), mu_min = 1e-12, mu_max = 1e-8 -- the
// LAST one being the engine's own QpOptions::dual_mu default, so an
// unconverged solve's early majors are byte-identical in behavior to the
// fixed-mu engine. "KKT residual" is `kkt.residual()` -- the SAME
// max(stationarity, feasibility) measure the convergence test and every
// history row already use -- READ AT THE ITERATE THIS TRIAL IS ABOUT TO BUILD
// A SUBPROBLEM FROM, i.e. the PREVIOUS major's own measurement. A rejected
// retry re-measures the identical point and so gets the identical number,
// which keeps mu constant across a shrink-retry (see quantization below). THE
// VERY FIRST major (iter == 0) has no previous major to read and uses mu_max
// unconditionally -- not the x0 residual.
//
// QUANTIZATION TO THE NEAREST DECADE (pow(10, round(log10(mu_k)))) is not
// cosmetic. qp_engine.h's hot-start reuse key compares the EFFECTIVE
// (primal_delta, dual_mu) pair by exact ==, so a raw mu formula returning a
// slightly different double every major would force a refactorization on EVERY
// major -- including late in a solve, where the driver is taking tiny,
// warm-started corrective steps and a refactorization is pure waste. Rounding
// to a decade makes the schedule IDEMPOTENT once the residual stops crossing a
// decade boundary. THE CLAMP HAPPENS BEFORE QUANTIZATION: rounding a value
// already inside [mu_min, mu_max] to the nearest power of ten can only land ON
// one of the four decades spanning [1e-12, 1e-8] or exactly at one end, never
// outside it.
//
// PRIMAL_DELTA IS DELIBERATELY NOT SCHEDULED: SolveOverrides::primal_delta is
// left at its own sentinel every major (the engine default), unconditionally.
// Coupling it to the same residual targets a DIFFERENT footprint (Hessian-block
// regularization, which matters most on an indefinite subproblem) and is out
// of scope here: this lever attacks the CONSTRAINT-row phenomenon above.
//
// DISABLED (SqpOptions::adaptive_mu == false) leaves SolveOverrides::dual_mu
// at its own sentinel every major -- the driver never touches the field --
// which resolves to opts_.qp.dual_mu exactly as without the schedule. SOC's
// and the elastic tier's re-solves build their own SolveOverrides from scratch
// and were never wired to the main trial's override, so they are unaffected by
// this lever in either mode: they always run at the engine's own default mu.
// =============================================================================
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2327–2393

The seeded dual clamp banner: the two outcomes, the normative ordering, and the derivation of 1e-6. Replaced by a banner carrying the rule and the ordering; `kSeededDualClampTol`'s own `@brief` keeps the band.

```text
// =============================================================================
// THE SEEDED DUAL CLAMP -- the `lambda_i >= 0` enforcement that makes
// StartLevel::kSeeded safe to open.
// =============================================================================
//
// WHAT IT IS. On a kSeeded ingest, and ONLY there, every ingested lambda_i(j)
// that is still NEGATIVE after the geometric complementarity clear is judged:
//
//     -kSeededDualClampTol <= lambda_i(j) < 0   ->  set to 0, count it
//                                                   (SqpCounters::seeded_clamped)
//     lambda_i(j) < -kSeededDualClampTol        ->  DEGRADE THE WHOLE OBJECT
//                                                   to kCold
//
// THE ORDER -- GEOMETRIC CLEAR FIRST, CLAMP SECOND -- IS NORMATIVE, NOT
// INCIDENTAL, and reversing it would change answers. The clear zeroes the
// price on every row the destination model reports STRICTLY SLACK; only after
// it has run is a surviving negative price a statement about a row that is
// GEOMETRICALLY ACTIVE, which is the one configuration a sign violation can
// actually do damage in (a negative price on a slack row is stale bookkeeping
// the clear was always going to drop, and degrading an otherwise-good object
// over it would refuse a seed for a reason that had already been handled).
// Run the other way round, a mesh transfer or crossover carrying an ordinary
// stale negative price on a released row would degrade to kCold and the whole
// level would be dead on its two intended producers.
//
// WHY THE TWO OUTCOMES DIFFER: a small negative price is NOISE -- an
// interior-point method handing over before full convergence, a predictor's
// ratio test at a weakly active row -- and the honest repair is to say "this
// row is priced at zero", which is what the KKT system asks for at the
// boundary anyway. A LARGE negative price is not noise, it is a contradiction:
// it says a constraint is PAYING the objective to be satisfied, and the
// level's whole premise -- trust the VALUES, since you cannot check the
// provenance -- fails for it. Degrading is the only answer that neither trusts
// it nor throws (warm_start.h's "safe even if stale" contract forbids the
// throw).
//
// THE VALUE 1e-6 AND ITS DERIVATION:
//   (1) THE PRODUCIBLE SIGN-VIOLATION CLASS IS ~1e-8. predictor.h's ratio test
//       admits one only within `kDualSignTol * max(1, |lambda|)` = 1e-9
//       RELATIVE; warm_start.h's from_interior_point copies its caller's price
//       verbatim, and a crossover from an unconverged IP solve lands around
//       1e-8.
//   (2) THAT CLASS SCALES WITH THE PRODUCER'S TOLERANCE, NOT OURS: an IP
//       method's residual sign noise near convergence sits about an order above
//       its own stopping tolerance. A producer converged to 1e-7 or better
//       therefore lands inside 1e-6.
//   (3) 1e-6 IS THIS DRIVER'S OWN kkt_tol DEFAULT, and that is the reading
//       that makes the constant defensible: a price of magnitude below kkt_tol
//       contributes at most kkt_tol * ||Ai_j|| to the Lagrangian gradient,
//       i.e. it sits AT THE STATIONARITY NOISE FLOOR THIS SOLVER ALREADY
//       DECLARES. Calling such a price zero cannot move a verdict the solver
//       was entitled to make. Against (1) it leaves two orders of margin;
//       against an O(1) garbage price, six.
//
// IT IS ABSOLUTE, NOT RELATIVE, AND THAT IS THE WHOLE POINT. predictor.h's
// kDualSignTol scales by `max(1, |lambda|)` because it asks "is this multiplier
// zero?"; this one asks "is this multiplier's SIGN VIOLATION small?", and a
// magnitude-relative band would clamp a -1e6 price as readily as a -1e-9 one,
// precisely the garbage case the degradation exists for.
//
// IT DOES NOT TRACK opts_.kkt_tol AT RUNTIME, deliberately: the quantity being
// bounded is a property of the PRODUCER that assembled the object, not of the
// stopping tolerance the CONSUMER happens to be configured with, and coupling
// them would make ingest behaviour move with an unrelated knob. Both directions
// of error are safe (a too-wide band drops a price the QP re-derives; a
// too-narrow one degrades to a cold solve), which makes a fixed, documented
// band preferable to a coupled one.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2400–2415

`class SqpDriver`: where the definitions live and why the constructors stay inline. The class `@brief` kept states both.

```text
// WHERE THE DEFINITIONS LIVE. Every member function of SqpDriver below EXCEPT
// THE TWO CONSTRUCTORS is declared here and DEFINED in the library TU
// src/drivers/sqp_driver.cpp -- solve_impl (the major loop) first among them,
// together with the trust-region update logic it drives (shrink_hits_floor /
// shrunk_radius / restoration_restart_radius), the ledger tail record_solve,
// the exit helpers finish / make_warm_start / map_status, and the SSN tier's
// ssn_engine / ssn_options. THE FREE FUNCTIONS DECLARED ABOVE ARE DEFINED IN
// THAT SAME TU, for the same §5 reason and so that solve_impl and its
// neighbours in that TU -- every in-library caller any of them has -- still
// see across the call exactly as they did when they were header siblings. READ THAT FILE'S BANNER
// before changing anything about this structure: it carries the reason the TR update functions
// travel with the loop, and the counter-identity bar any redraw must clear.
//
// THE CONSTRUCTORS STAY INLINE, deliberately: their one piece of real work
// (the option validation) lives in src/drivers/sqp_options.cpp, and what is
// left is a member-initializer list and one call.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2423–2441

`SqpDriver::SqpDriver(const SqpOptions&)`: why tr_init == 0 is rejected rather than warned about. The `@throws` kept lists every refusal including that one.

```text
    /// Constructs a driver over the given options.
    ///
    /// @throws std::invalid_argument on an option that cannot be honoured --
    ///         everything validate_sqp_options rejects: a non-positive or NaN
    ///         kkt_tol/feas_tol, a negative max_iter, a non-positive or NaN
    ///         tr_init, a tr_max below tr_init (unless tr_init is +inf), or a
    ///         tr_min that is non-positive or above tr_init or tr_max. The
    ///         radius is additionally re-validated per solve by the engine
    ///         (qp_types.h's SolveOverrides PRECONDITION); it is
    ///         checked here too so the message names the driver option the
    ///         caller actually set.
    ///
    /// tr_init == 0 IS REJECTED, not merely warned about: a zero radius pins
    /// every subproblem's box to the single point p = 0, so every step is
    /// zero, no iterate ever moves, and the solve stalls until max_iter and
    /// reports kMaxIter -- a silent, expensive no-op that no caller can
    /// possibly want. +inf remains legal and means "no trust region" (it is
    /// SolveOverrides' own sentinel for deferring to the engine's
    /// opts.qp.tr_radius); the requirement is tr_init > 0.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2445–2454

The private constructor: what a request raised inside a restoration solve does, and the `!(x > 0.0)` predicate form. Replaced by two lines stating the recursion bound.

```text
    // THE RESTORATION PHASE'S OWN DRIVER is constructed through here with
    // restoration DISABLED, which bounds the recursion at one level (see the
    // RESTORATION PHASE note). A request raised inside a restoration solve
    // takes the plain exit -- SqpStatus::kInfeasible, with the last row's
    // verdict shaped by whichever of the three sources raised it -- which the
    // outer driver reads as "the feasibility problem is itself stuck"
    // regardless of that shape. The validation call runs the same checks as
    // validate_sqp_options, in the same order with the same messages; its
    // predicates are written as `!(x > 0.0)` because that is the form that
    // rejects NaN under the build's `-fno-finite-math-only`.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2461–2471

The model-taking overloads: what the bridge lay costs on every call. No header replacement; nlp_model_aggregate.h documents the lay.

```text
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
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2481–2499

`attach_ledger`: the two-level forwarding and the nested driver's exclusion. Both are kept, tightened, in the rewritten block.

```text
    /// Attach a ledger for instrumentation (nullptr = off, default off) --
    /// the driver-level analogue of QpEngine::attach_ledger, one level up.
    /// Emits exactly one SqpSolveRecord per PUBLIC solve() call on THIS
    /// instance, labeled `label_prefix_<n>` with a per-instance counter, and
    /// ALSO forwards the same Ledger to this driver's own internal QpEngine
    /// under the suffixed prefix `label_prefix_qp`, so one Ledger attached
    /// here captures both levels at once: one SqpSolveRecord per driver
    /// solve, and the ordinary QP-level SolveRecord entries (one per
    /// subproblem, SOC re-solve or elastic rung) engine_ has always been able
    /// to emit.
    ///
    /// THE RESTORATION PHASE'S OWN NESTED SqpDriver (the private constructor)
    /// is NEVER given this ledger: it is a fresh SqpDriver over a fresh
    /// QpEngine, constructed on the fly. So a solve that enters restoration
    /// still emits exactly ONE SqpSolveRecord (this driver's own), whose
    /// counters already fold in every restoration major, rather than a second
    /// record for the sub-solve -- consistent with SqpCounters' own "the work
    /// was spent, folded into the aggregate" convention for SOC/elastic
    /// re-solves.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2507–2534

`solve(const NlpModel&, const Vec&)`: the five-and-a-half refusal classes with the argument for widening them. The `@throws` kept enumerates the classes without the argument.

```text
    /// Solves from an explicit start point; thin wrapper around solve_impl
    /// (the actual major loop, now private) whose only job is the ledger
    /// record above: solve_impl is called exactly once, and its result is
    /// recorded before being returned unchanged.
    ///
    /// @throws std::invalid_argument on a model that cannot describe a problem.
    ///         Wrapping the model in a bridge runs the aggregate declaration's
    ///         own validation (model/aggregate_declaration.h) before the loop
    ///         starts, and that boundary refuses five classes this entry used to
    ///         accept in silence: a CROSSED BOX (lower(i) > upper(i) on two
    ///         finite sides, or an intersection that is empty); a NaN on either
    ///         side of a bound; a start_point() whose size is not n(); an
    ///         eval_hess entry BELOW THE DIAGONAL, which nlp_model.h already
    ///         forbids; and a Jacobian or Hessian whose dimensions contradict
    ///         the declared me()/mi()/n(). A fifth-and-a-half, checked per
    ///         evaluation rather than at entry: a stored-element count that
    ///         moves with x, contradicting nlp_model.h's
    ///         structural-pattern-invariance precondition. A sixth, also
    ///         checked per evaluation: a return presenting its stored elements
    ///         at coordinates the claim pass did not record for that slot --
    ///         the same count in a different order, or a different pattern at
    ///         an unchanged count -- reported against the claim-time
    ///         coordinates (src/model/nlp_model_aggregate.cpp's
    ///         scatter_matrix). THE WIDENING IS
    ///         DELIBERATE -- each class is an out-of-contract model that
    ///         previously reached the major loop and produced an answer of no
    ///         defined meaning, and a crossed box in particular is not a
    ///         feasible region a solver can be asked about.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2537–2559

`solve(NlpModelAggregate&, const Vec&)`: why the bridge is the parameter, what it owes, and why the box is not re-checked. Both are kept in the rewritten `@param`/`@throws`.

```text
    // THE PRIMARY PATH, of which every NlpModel-taking overload on this class
    // is a wrapper (the driver consumes the Level 2 contract, and a single
    // model is one bridge over it -- model/nlp_model_aggregate.h). It builds
    // one AggregateEvalSeam over `bridge` and runs the major loop against that;
    // the model-taking overloads differ only in that they build the bridge
    // themselves, from a `const NlpModel &` they do not own.
    //
    // THE PARAMETER IS THE CONCRETE BRIDGE, not the claim-stream interface the
    // seam itself binds. The driver keeps the bridge because the restoration
    // phase needs the Level 1 model behind it, and hands the seam a base
    // reference on the way past.
    //
    // WHAT `bridge` OWES: one operation at a time, structural mutation
    // included. The seam re-lays whenever the aggregate's structure epoch has
    // moved, so a renegotiation between solves is handled; a mutation DURING a
    // solve is not a thing this class defends against.
    //
    // THE BOX IS NOT RE-CHECKED HERE. The bridge validated the model's two
    // bound returns against its own declared n() when it laid its structures,
    // and the seam's lower()/upper() are materialized from that declaration --
    // n-sized by construction, with no model return left to disagree. The
    // model-taking overloads keep their own check, which runs BEFORE the bridge
    // exists; see solve_impl.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2575–2684

`solve(const NlpModel&, const Vec&, const WarmStart&, Index)`: the whole warm-start ingest contract — x0's fate, the stale/foreign rule, kHot's guarantees, and the probe budget's five parts. The rewritten block keeps the ignored-x0 rule, the never-throws rule and the budget's operative terms.

```text
    // Warm-start ingest, up to and including the HOT level. `warm` is
    // typically a PRIOR solve's own SqpSolution::warm_start (warm_start.h),
    // on a problem the caller believes is the same or a nearby one to `model`
    // -- see solve_impl's WARM-START INGEST note for the whole resolution
    // rule (cold/seeded/warm/hot level, what each level actually ingests).
    //
    // THE CALLER'S OWN x0 IS IGNORED, LOUDLY, whenever `warm` resolves to
    // StartLevel::kSeeded, kWarm OR kHot: the solve then starts from
    // `warm.x`, NOT this parameter. THAT INCLUDES A HASH MISMATCH -- `x0` is
    // used only as the COLD fallback, and kCold is reached only through the
    // seeded ingest gate (invalid, dimensionally incompatible, or non-finite)
    // or through a start_level CAP -- never through provenance alone. A
    // caller that wants its own x0 honoured unconditionally must pass a
    // default-constructed WarmStart{} (valid == false), which always
    // resolves to kCold, or cap SqpOptions::start_level at kCold.
    //
    // NEVER THROWS ON A STALE OR FOREIGN `warm`: an invalid object, one
    // whose structure_hash does not match this model's, or one that happens
    // to share this model's (n, me, mi) shape without sharing its structure
    // are all handled silently -- warm_start.h's "safe even if stale"
    // contract, of which this ingest is the caller-facing half. A valid,
    // correctly sized, finite object whose hash is USELESS (the 0 sentinel,
    // or a mismatch) resolves StartLevel::kSeeded and CONTRIBUTES ITS VALUES
    // -- x, the duals, the activity hint -- rather than being discarded for a
    // cold solve. What a hash mismatch still refuses: the trust-region
    // radius, the funnel-width re-base, the Kungurtsev-Diehl full-step
    // window, and above all any reuse of a factorization built on another
    // matrix. Objects that fail the seeded ingest gate -- `valid == false`,
    // incompatible dimensions, or a non-finite value in x/lambda_e/lambda_i
    // -- degrade all the way to kCold.
    //
    // KHOT: a structural match PLUS a non-null `warm.hot` additionally offers
    // the engine THIS driver owns a chance to adopt a PRIOR solve's retained
    // K0 factorization -- possibly built on a DIFFERENT SqpDriver/QpEngine
    // instance entirely -- and skip rebuilding it. What a caller can rely on:
    //   - IT NEVER PRODUCES A WRONG ANSWER. Whether the offered handle is
    //     actually reused is decided entirely by qp_engine.h's own
    //     reuse-eligibility gate (conditions (a)-(e); condition (e)
    //     specifically detects a handle whose underlying factorization has
    //     been rebuilt by ANOTHER holder since it was emitted -- see
    //     BorderState::generation and HotState's OWNERSHIP note in
    //     qp_engine.h). Any mismatch -- stale, foreign, or overtaken by a
    //     later solve on the engine that produced it -- degrades SILENTLY to
    //     an ordinary kWarm solve: never a throw, never a numerically wrong
    //     result attributed to a reused factorization that no longer
    //     describes anything real.
    //   - `out.counters.start_level_used` reports what was OBSERVED to happen
    //     on this solve's first subproblem, not merely what `warm` offered:
    //     kHot only if the engine's own reuse gate actually passed, kWarm on
    //     any degradation.
    //   - HAND-OFF IS SINGLE-USE PER CONSUMER BUT SAFE TO CHAIN. Feeding
    //     the SAME `warm.hot` into more than one subsequent solve() call is
    //     not a lifetime error (shared_ptr keeps the underlying
    //     factorization alive) and not a CORRECTNESS error either: a second
    //     consumer either reuses the same factorization (if nothing has
    //     touched it since) or degrades to kWarm (if something has); a
    //     refused adoption detaches onto a fresh BorderState rather than
    //     rebuilding the shared one in place. CONCURRENT use across threads
    //     remains genuinely unsafe -- see qp_engine.h's THREAD SAFETY note.
    //
    // THE PROBE BUDGET, the fourth argument, default 0 = NO BUDGET. A
    // POSITIVE `minor_budget` asks this solve to STOP EARLY -- at the top of
    // the first major that finds `counters.qp_minor_iters >= minor_budget`
    // without having converged -- and to report that stop in
    // `counters.probe_budget_stops`. It exists for exactly one caller shape:
    // a driver of solves (continuation.h) that can tell, from the cost of
    // the solves it has already paid for, that THIS one has stopped looking
    // like them, and would rather re-pose the problem than finish paying.
    //
    // THE CONTRACT, in the five parts a caller has to know:
    //
    //   1. IT IS INERT AT 0 (and at any value <= 0), which is what every
    //      existing call site passes by omission. The test is then a single
    //      false comparison per major and NOTHING else about this driver --
    //      trajectory, counters, status, warm start -- differs.
    //   2. IT IS CHECKED BETWEEN MAJORS, NEVER INSIDE ONE. The QP engine is
    //      not told about it and its own QpOptions::max_iter is not touched,
    //      so a solve stopped this way has spent AT LEAST `minor_budget`
    //      minors and at most `minor_budget` plus whatever the crossing
    //      major cost (bounded in turn by that QP cap, plus any SOC/elastic
    //      re-solve on the same major). A caller that needs a HARD minor
    //      bound does not have one here.
    //   3. CONVERGENCE ALWAYS WINS. The budget test sits beside the max_iter
    //      test, after the convergence test has already been evaluated on
    //      the same pass, so a solve that is AT a KKT point is reported
    //      kOptimal and its answer kept no matter how far over budget it
    //      went. An aggressive budget can throw away work about to be spent,
    //      never an answer already found.
    //   4. THE STATUS IS kMaxIter, deliberately, and NOT kBudgetExhausted
    //      even when SqpOptions::budget_mode is on. budget_mode's status
    //      carries a specific promise -- "here is the best iterate I saw,
    //      pick up from it AT THE SAME PARAMETER VALUE" -- and a probe-budget
    //      stop promises the opposite: the caller asked to be told cheaply
    //      that this solve is not going where it hoped. Reporting a stop
    //      that means "abandon this proposal" with a status that means
    //      "continue this proposal" would make the two budgets fight.
    //      `counters.probe_budget_stops` distinguishes this exit from an
    //      ordinary max_iter one; the returned WarmStart is built exactly as
    //      any other stopped-AT-an-iterate exit's is.
    //   5. WHAT THE BUDGET IS SPENT IN, PER MODE. At `qp_mode == QpMode::
    //      kWalk` it is `qp_minor_iters`, exactly as always. At
    //      `qp_mode == QpMode::kSsn` it is that PLUS every factorization the
    //      SSN kernel and the tier-3 refinement paid -- because an SSN-solved
    //      subproblem contributes ZERO minors by design, so a minors-only
    //      budget could not trip AT ALL in that mode. The derivation is at
    //      `ssn_budget_charge`'s site in solve_impl. NOTE WHAT THIS DOES NOT
    //      CHANGE: `SqpCounters::qp_minor_iters` still publishes walk minors
    //      and nothing else, in both modes. It DOES mean a kSsn budget and a
    //      kWalk budget of the same numeric value are not the same amount of
    //      work.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2706–2709

The bridge-taking warm overload: a pointer at its sibling's contract. Kept as one `@param` sentence.

```text
    // The warm-start ingest against a bridge, on the same footing as the
    // 2-argument bridge overload above: same primary path, same seam, and the
    // whole of the ingest contract documented on the model-taking overload
    // just above this one.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2743–2780

`export_warm_start`: the block map, the sign identity, what a non-converged exit exports, and the stamp's provenance. All four are kept, compressed, in the `@return`.

```text
    /// @brief The warm-start value of the last completed solve, in DECLARED
    ///        space.
    ///
    /// Blocks, all at declared dimensions: `primal_` is `SqpSolution::x`,
    /// `eq_lmults_` its `lambda_e`, `iq_lmults_` its `lambda_i`, and
    /// `bound_lmults_` its `z`. Model space IS declared space on this engine
    /// -- no reduced space and no fixed-variable treatment anywhere in this
    /// driver -- so the blocks are the solution's own vectors verbatim, with
    /// no mapping in between.
    ///
    /// SIGN: `z` already is the currency's z = zL - zU, under the identity
    /// warm_start.h's SIGN CONVENTIONS paragraph states -- grad f + Ae^T le +
    /// Ai^T li - z = 0, so z >= 0 at an active LOWER bound and z <= 0 at an
    /// active UPPER one. Nothing is converted on the way out.
    ///
    /// What is exported is what the solve reported, a non-converged exit
    /// included; SqpSolution's own exit-dependent contract says what that
    /// evidence means at each exit -- including the certified-infeasible one,
    /// where the multipliers are a subgradient certificate rather than
    /// prices. Nothing here re-reads or re-judges them.
    ///
    /// The stamp is the bridge's DECLARATION key (model/structure_identity.h's
    /// `declaration_key` over `declaration()`) as of that solve's COMPLETION,
    /// not as of this call. Not the bridge's `model_structure_key()`, which
    /// stays what it always was, the layout/epoch key;
    /// warmstart/warm_start_data.h carries the argument.
    ///
    /// NO EXTENSIONS: this engine produces none, so an exported value is
    /// always core-only and `extensions_` reads as an empty list -- a
    /// capability statement, not an omission.
    ///
    /// @return The captured value, by copy.
    /// @throws std::logic_error if no solve has COMPLETED on this instance --
    ///         "completed" meaning a public solve() that RETURNED; a call
    ///         that threw does not count, and neither does the restoration
    ///         phase's own nested driver, which is a different instance.
    ///         Never an empty payload, which would stage cleanly against
    ///         anything and then silently cold-start.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2783–2884

`stage_warm_start`: the one-shot rule, what is checked here versus at solve entry, what a stamp mismatch means, and the two ways the value is applied. The rewritten block keeps the one-shot rule, the check split, the level and the `@throws` list.

```text
    /// @brief Stages a warm start for the NEXT public solve() on this
    ///        instance.
    ///
    /// ONE-SHOT. The value applies to the next public solve() -- whichever
    /// overload -- and is consumed by it, applied or refused. It is not
    /// attached to a bridge, so it survives any re-lay in between. A second
    /// warm solve needs a second stage.
    ///
    /// CHECKED HERE: FINITENESS of every core block; the core's own internal
    /// consistency (`primal_` and `bound_lmults_` describe one space and must
    /// be one length); and, when the value carries the
    /// `"hven.ipm.polish.v1"` tag, that the payload DECODES and that its
    /// three blocks are at the core's own widths.
    ///
    /// CHECKED AT THE NEXT SOLVE instead, against the problem that call
    /// binds: every block's length against the declared dimensions, refusing
    /// `std::invalid_argument` naming the block, the length held and the
    /// length declared; then the stamp, refusing naming both DECLARATION key
    /// digests. Either refusal has ALREADY consumed the staged value, so a
    /// caller that logs the refusal and solves anyway cold-starts rather than
    /// silently warm-starting off a value this engine just rejected.
    ///
    /// A STAMP MISMATCH means the caller transcribed a DIFFERENT PROBLEM --
    /// different declared dimensions, or a different declared bound STRUCTURE
    /// (which sides are finite, and which variables are fixed). It does not
    /// mean a different engine and it does not mean a different
    /// fixed-variable treatment: a value the interior-point engine exported,
    /// under any treatment, stages and applies here on the same declaration.
    /// A MATCH does not promise the pieces' row structure or the bound
    /// VALUES, neither of which the key hashes; warmstart/warm_start_data.h
    /// states the whole guarantee.
    ///
    /// NON-CONSUMING: `data` is taken by const reference and copied. Staging
    /// the same value twice from the same cold state produces the same start
    /// state.
    ///
    /// CLEARS FIRST: this call, whether it succeeds or refuses, first drops
    /// any value staged before it. A caller whose staging is refused holds
    /// nothing, not the previous payload.
    ///
    /// AN EXPLICIT `warm` ARGUMENT AND A STAGED VALUE ARE REFUSED TOGETHER: a
    /// solve() overload taking a `WarmStart` while a value is staged throws
    /// `std::invalid_argument` naming both sources -- including when the
    /// argument is a default-constructed (cold) object, which is this class's
    /// documented way of ASKING for a cold solve. That refusal does NOT
    /// consume the staged value: it judges the CALL's arguments, and the call
    /// binds no problem and runs nothing. It is the one refusal on this
    /// surface that leaves something staged, and its message says so.
    ///
    /// WHAT IS APPLIED. The value becomes the `warm` object the next solve
    /// runs against -- the same object an explicit argument would have been
    /// -- built one of exactly two ways:
    ///
    ///   * WITH THE `"hven.ipm.polish.v1"` EXTENSION: the interior-point
    ///     crossover, built by warmstart/ipm_polish_extension.h's
    ///     `to_sqp_warm_start` against that solve's own declared box. The
    ///     activity inference, the sign conventions and the resulting
    ///     `structure_hash == 0` are all that function's.
    ///   * CORE-ONLY: `primal_`/`eq_lmults_`/`iq_lmults_`/`bound_lmults_`
    ///     copied verbatim into `x`/`lambda_e`/`lambda_i`/`z`, with NO
    ///     activity attributed -- an all-free `WorkingSet(n, mi)` and
    ///     all-zero activity vectors, which is how "no activity was
    ///     attributed" is spelled on this type (WarmStart::ineq_active's own
    ///     field note) -- and `structure_hash == 0`.
    ///
    /// EITHER WAY THE LEVEL IS StartLevel::kSeeded, and it cannot be higher:
    /// kWarm and kHot are gated (core/start_level.h) on a hash of this
    /// driver's own assembled H/Ae/Ai, which the currency does not carry. The
    /// consequences are kSeeded's own: the point, the duals and any activity
    /// hint are ingested; THE SEEDED DUAL CLAMP applies (a negative
    /// `lambda_i` within `kSeededDualClampTol` of zero is clamped and counted
    /// in `SqpCounters::seeded_clamped`, a larger one degrades the whole
    /// object to kCold); and the trust-region radius, the funnel-width
    /// re-base and the Kungurtsev-Diehl full-step window are NOT taken. A
    /// core-only value's empty hint additionally re-arms
    /// `SqpOptions::crash_basis`, by kSeeded's own AN EMPTY HINT IS NO HINT
    /// rule. `SqpOptions::start_level` still CAPS the result, so a driver
    /// capped at kCold ignores a staged value exactly as it ignores an
    /// argument-passed one.
    ///
    /// THE STAGED PRIMAL REPLACES THE `x0` the call would have used, through
    /// the ingest rule the 3-argument solve() overload already documents:
    /// `x0` is the COLD fallback and is ignored whenever the warm object
    /// resolves above kCold. A caller that wants its own `x0` honoured does
    /// not stage.
    ///
    /// @param data The value to stage, in DECLARED space.
    /// @throws std::invalid_argument if `primal_` and `bound_lmults_` are not
    ///         one length, if any core block holds a non-finite value, if the
    ///         value carries the polish tag MORE THAN ONCE, if a payload
    ///         under that tag is malformed or is not at the core's own widths,
    ///         or if either of that payload's bound-dual blocks holds a
    ///         non-finite or NEGATIVE entry (all naming the tag; a negative
    ///         also names the block, the coordinate and the value). The sign
    ///         refusal is the IPM staging path's too, in the same terms:
    ///         prices are non-negative by the extension's contract, so a
    ///         negative one is corruption, and THE SEEDED DUAL CLAMP below --
    ///         which governs `lambda_i`, not these blocks -- is not a licence
    ///         to carry one through. An UNKNOWN tag is skipped silently: a
    ///         capability downgrade, not an error. Sizes against a problem
    ///         and the stamp are refused at solve entry, not here. Every one
    ///         of these refusals leaves this instance with nothing staged.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 2923–2940

`solve_impl`: why every model quantity arrives through the seam and why `bridge` rides alongside. Kept in four lines.

```text
    // `minor_budget` <= 0 means NO BUDGET -- see the 4-argument solve()'s own
    // THE PROBE BUDGET note for the whole contract.
    //
    // Every model quantity this loop reads arrives through `seam`: the
    // dimensions, the box, the six evaluation moments and the subproblem.
    // There is no NlpModel in scope here and no `model.eval_*` call anywhere
    // below -- the free functions over NlpModel declared in this header remain
    // for callers measuring a point of their own; the solve path does not use
    // them.
    //
    // WHICH IS WHY `bridge` RIDES ALONGSIDE `seam`: the one place a model is
    // still named is the restoration phase, which builds a different NlpModel
    // (RestorationModel, in the variables (x, sp, sm, si)) around the one
    // behind the bridge and solves it with a nested driver. The seam binds the
    // claim-stream interface, which carries no model, so that single Level 1
    // read -- an identity read, never an evaluation -- is taken from the
    // caller's own bridge handle. Both name the same object; only the
    // restoration phase reads the second.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3094–3097

`enter_restoration`: why the default argument is spelled out rather than written `= {}`. Deleted from the header as a language note rather than a contract.

```text
    /// The default is spelled out rather than written `= {}`: a default
    /// argument in the enclosing class's own body may not reach a nested
    /// class's default member initializers, and naming both members here keeps
    /// them where a reader can see them.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3193–3226

`route_through_ssn_warm_grade`: why one function serves both routes, what the grade is, why the primal is carried here and not on the kSsn arm, and which lever is forced off. The `@param`/`@return` kept state the consumption and the outcome.

```text
    // SECTION 2.3 ITEM 4's SECOND-CHOICE SUCCESSOR: the SSN tier, warm-graded
    // from the interior-point tier's own (x, lambda).
    //
    // ONE FUNCTION FOR BOTH ROUTES INTO IT -- a refused refinement and a
    // saddle-suspect (kIndefinite) exit -- because they are the same hand-off
    // and section 2.3 gives them the same successor. "SSN's uncertain band is
    // designed to absorb exactly the tie rows the ratio rule left UNCERTAIN,
    // and its bulk flip changes the whole implied active set at once."
    //
    // THE GRADE IS (x, lambda), section 2.3 item 4's own words -- the duals,
    // the bound prices, the binary activity AND the tier's primal iterate,
    // handed to SSN inside the TIER'S window through `SsnStart::box_center`
    // (settler ruling, fix round 2). That is the one place this route departs
    // from `ssn_start_from_qp_seed`'s rule for the kSsn arm, and the reason is
    // that the two seeds are different objects: the kSsn arm's is the PREVIOUS
    // major's answer, in a subproblem whose trust region is centred on p = 0,
    // so carrying its primal would move that centre; this one is THIS
    // subproblem's own iterate, reached inside THIS subproblem's window, and
    // starting at the origin instead throws away the acquisition the tier just
    // paid for. The window stays the tier's either way, which is what
    // `box_center` is for and what `assert_ssn_warm_grade_window` checks.
    //
    // ONE SETTING IS DELIBERATELY NOT THE kSsn ARM'S, and one that used to be
    // is no longer. The R5 deferred-certification lever is forced OFF (a
    // kSsn-mode research lever whose pending-evidence state has no owner on
    // this path). The PROXIMAL CARRY, by contrast, participates exactly as it
    // does on the kSsn arm -- consume an incoming carry once, export the max
    // over every subproblem, stamp the centre only from a usable exit --
    // because `warm_start.h`'s own contract scopes it to "any SSN subproblem"
    // and one reached through this chain is one.
    //
    // A USABLE EXIT IS THEN REFINED ON ITS OWN FACE, as the kSsn arm refines
    // every certifying exit; see the definition's note for the parity rule.
    //
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3243–3317

`make_warm_start`: the zero-major probe, the unevaluable exit, the probe_ev/probe_x pairing, and why the function is static. The rewritten block keeps the probe cost and the cold-object rule.

```text
    // WARM-START POPULATION. Builds the WarmStart every exit of solve_impl
    // attaches to SqpSolution::warm_start.
    //
    // `activity` is the best-known QpSolution WHOSE bound_state/ineq_active
    // still describe the point being returned -- nullptr when no such
    // solution exists (nothing was ever solved, or the point being returned
    // is not one any in-loop QpSolution was solved at, e.g. a restored
    // point).
    //
    // `qp_built` says whether `qp` already holds a REAL subproblem; passing
    // false with a default-constructed QpProblem keeps structure_hash from
    // hashing zero-sized matrices as if they meant something. `probe_ev`/
    // `probe_x` are the evaluation bundle and the point it was taken at --
    // read ONLY when qp_built is false (see THE ZERO-MAJOR PROBE below), and
    // NULL on the one exit that stands at no evaluable point at all (THE
    // UNEVALUABLE EXIT, further below).
    //
    // THE ZERO-MAJOR PROBE: an exit that converged (or ran out of budget)
    // before the first subproblem was ever built would otherwise write
    // `structure_hash = 0`, warm_start.h's "no model was seen" sentinel,
    // which the ingest rule treats exactly like a MISMATCH -- so a zero-major
    // solve emitted a `valid` hand-off the very next solve silently refused,
    // breaking predictor chains. The hash such an exit "never computed" is
    // nonetheless COMPUTABLE there, because structural_hash (qp_engine.h)
    // reads H/Ae/Ai's SPARSITY PATTERN ONLY, never their values. So this
    // builds a PROBE subproblem at the exit point and hashes that, through
    // the same build_subproblem + structural_hash machinery the qp_built path
    // uses -- never a bespoke pattern walk that could drift from it.
    //
    // ZERO MULTIPLIERS, DELIBERATELY: the same recipe the INGEST side's own
    // probe uses, so the two hashes agree by construction on any model whose
    // pattern is point-independent -- the exact assumption the ingest probe
    // has always made.
    //
    // THE COST: one extra eval_hess, nothing else -- `probe_ev` is the bundle
    // the loop already computed at this iterate, so Je/Ji come for free. Paid
    // ONLY on the qp_built == false exits THAT ARE PROBED AT ALL. Those exits
    // spent no major iteration, so one Hessian is the cheapest thing such a
    // solve does; the alternative is an unusable hand-off, which costs the NEXT
    // solve a full re-globalization from cold.
    //
    // THE UNEVALUABLE EXIT IS NOT PROBED, AND ITS HAND-OFF IS COLD
    // (`probe_ev == nullptr`). What probed it would miss: the 3-arg solve()
    // takes its x FROM the warm object on a kWarm resolution, so a
    // hash-valid hand-off from a solve that could not evaluate its own start
    // point pins the NEXT solve back onto that same unevaluable point and
    // DISCARDS the corrected x0 the caller supplied to retry with -- turning
    // a recovery into a repeat of the same kNumericalError. So that exit
    // passes null, and
    // this function emits a COLD object there: `valid = false`,
    // `structure_hash = 0`, no probe attempted. Both consequences are decided
    // HERE, from the one parameter, so they cannot drift apart at the call
    // site. It also keeps two other contracts intact: build_subproblem's own
    // precondition (never handed a non-finite `ev`) and eval_hess's (never
    // called at a point the model has already reported unevaluable).
    //
    // WHERE `probe_ev` AND `probe_x` ARE NOT A MATCHED PAIR, and why it does
    // not matter. At the RESTORATION exits, `x` may already have been moved to
    // the restored point while `ev` still describes the point the restoration
    // was requested from. Those exits pass the pair anyway because the probe is
    // unreachable there (`qp_built` is unconditionally true by then), and were
    // that to change the probe would still be CORRECT -- it reads only the
    // model's sparsity pattern, which is the same at both points.
    //
    // Static (no `this`): every value it needs -- including opts_.qp.
    // primal_delta -- is passed in explicitly, so it can be called from a
    // context that does not otherwise need a `SqpDriver&`. `hot` is the one
    // exception: every call site fetches it from `engine_.hot_state()` right
    // before calling in, since only the driver's own engine_ instance can
    // produce it; this function merely stores what it is handed.
    //
    // Takes the seam non-const because the zero-major probe builds a
    // subproblem through it -- an evaluation, which re-lays if the aggregate's
    // epoch has moved and scatters into the seam's own arena. Everything else
    // it reads off the seam is a dimension.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3324–3340

`finish`: why it is not static and why it takes the seam. The rewritten block keeps the caller-scale flag's contract.

```text
    /// Assembles the final SqpSolution (status, x, multipliers, KKT record,
    /// f, warm start) at an exit, AND maps every exported quantity back to the
    /// caller's units when the solve ran scaled.
    ///
    /// NOT STATIC, and it takes the seam, for one reason each: it reads
    /// `opts_.feas_tol` for the caller-scale re-measurement's bound tolerance,
    /// and that re-measurement is an EVALUATION, which only the seam can serve.
    /// Both are inert at the shipped default -- an unscaled solve reads neither.
    ///
    /// @param seam the solve's evaluation seam; carries the installed factors.
    /// @param multipliers_are_caller_scale TRUE only at the restoration exit
    ///        that ADOPTS the sub-solve's own multipliers and bound prices.
    ///        That sub-solve runs unscaled over the raw model, so those blocks
    ///        are already in the caller's units and the ENGINE->CALLER map is
    ///        skipped for them -- applying it would divide by `sf` twice. Every
    ///        other exit leaves it false, including the restoration exits that
    ///        return the loop's own (or cleared) multipliers.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3361–3393

`engine_`, `ssn_engine_` and `ipqp_engine_`: why one engine per driver and why the two tiers are lazily constructed. Kept in three lines each.

```text
    // ONE engine for the whole driver, deliberately: it is what makes warm
    // seeding (and, when the model happens to be a QP, qp_engine.h's
    // hot-start K0 reuse) possible across majors. It also makes SqpDriver
    // exactly as thread-unsafe as QpEngine -- use one driver per thread.
    QpEngine engine_;
    // The semismooth-Newton tier's engine, LAZILY CONSTRUCTED and never
    // touched at the shipped default.
    //
    // WHY A POINTER RATHER THAN A MEMBER. SsnEngine owns a live KktFactor (and
    // through it a Pardiso/Accelerate backend session), exactly as QpEngine
    // does. Making it a plain member would allocate that state on EVERY
    // SqpDriver a caller constructs, including the overwhelming majority that
    // run `qp_mode == QpMode::kWalk` and will never solve an SSN subproblem --
    // and including the driver the restoration phase constructs for itself on
    // every restoration entry. Deferring it to first use makes "kWalk touches
    // no SSN code at all" a structural fact: no SsnEngine is constructed, so
    // none of ssn_engine.h runs.
    //
    // ONE ENGINE FOR THE WHOLE DRIVER, for the same reason engine_ is one:
    // ssn_engine.h holds its KktFactor across solves, which is what makes its
    // "one symbolic analysis per structure, reused across QPs of identical
    // structure" property observable at all. A fresh engine per subproblem
    // would pay a phase-11 analysis on every major.
    std::unique_ptr<SsnEngine> ssn_engine_;
    // The interior-point tier's engine, LAZILY CONSTRUCTED and never touched
    // at the shipped default -- `ssn_engine_`'s note above is this member's
    // note verbatim, including its two reasons: an IpqpEngine owns a live
    // KktFactorization (and through it a backend session), so making it a
    // plain member would allocate that state on every SqpDriver including the
    // overwhelming majority that never leave kWalk; and ONE engine for the
    // whole driver is what makes spec 4.1's "one symbolic analysis per SQP
    // solve" observable at all, since the analysis and the IpqpKktLayout
    // scatter plan live on the instance.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_driver.h · lines 3464–3481

`format_iteration_table`: what the table renders, and the two column rules. The `@brief`/`@return` kept name the table and the row.

```text
// THE ITERATION PRINTER: an fmt-based table rendering of sol.history (one row
// per SqpIterate -- trial index, f, the KKT residual, h == violation_l1, the
// trust-region radius Delta, the verdict, and the QP counters of the
// subproblem solved from that row, if any) followed by the final status line,
// plus a trailing "Start Level" line (SqpCounters::start_level_used is a
// SOLVE-WIDE reading, so it belongs in the footer). Returned as a plain
// std::string -- library code never prints to stdout on its own; a caller
// that wants this on a terminal does that itself.
//
// A ROW WHOSE qp_solved IS FALSE (at most one, always last) prints "-" in the
// verdict/QP columns instead of the DEFAULTED kReject/0 those fields carry on
// such a row: verdict is MEANINGLESS there, and printing the default would
// read as a rejection that never happened.
//
// The WD column (SqpIterate::watchdog_restored, "*"/"") renders on EVERY row,
// regardless of qp_solved, since a restore and a stopped-AT-iterate exit can
// coincide on the same row (the restored point can itself already satisfy the
// convergence test).
```

### include/hven/drivers/interior_point_solver.h

2619 lines / 1912 comment lines at `1997159`; 2061 / 1354 after. Carries R3.

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 49–83

The per-harness descriptions attached to the twelve test-fixture forward declarations, twelve of which name classes that no longer exist. Replaced by one two-line banner; the declarations themselves are untouched here (the friend declarations they pair with are removed in this task's one non-comment commit).

```text
// Test harness for the nested feasibility-restoration eval/step seam: reaches
// private eval_nlp / alg_impl / restoration_ / dims to drive the seam directly.
class NestedSeamHarness;
// Inequality-row variant of the seam harness: drives the eval seam on a problem
// with an inequality constraint so the slack-completed inequality condensation is
// verified through the assembled KKT.
class NestedSeamIneqHarness;
// Test harness for the nested feasibility-restoration LIFECYCLE (entry
// orchestration, exit ratchet, multiplier re-entry): reaches the private
// enter_/exit_feasibility_restoration helpers, the stashed-μ / ratchet state,
// restoration_, and alg_impl to drive the whole phase end-to-end.
class NestedLifecycleHarness;
// Test harness for the persistence-based divergence classification in
// converge_check(): reaches the private converge_check() and settings_ so the
// trailing-window logic can be exercised directly on synthetic iterate
// histories.
class DivergencePersistenceHarness;
// Test harness for the SOC / extended-backtracking recovery links under the
// generic-path acceptance strategies: reaches the private nlp_ / kkt_sol_ /
// dims / scratch / restoration_ / acceptance_ / recovery_ so it can build a
// live SolverContext and drive the mechanism's acceptance-backtrack seam with a
// generic acceptance strategy.
class SocGenericHarness;
class InertiaRegularizationSolve_ClassicDegeneracyLatchTracksSingularity_Test;
// Composition sentinels for native variable bounds against the inertia
// machinery: a solution sitting ON a bound drives the condensed bound curvature
// on the primal diagonal very large, and these read dc_latched_ / bounds_ /
// bound_duals_ to check that a healthy system's factorization is still accepted
// on its own inertia.
class InertiaRegularizationSolve_ActiveBoundCurvatureNeverTripsSingularitySignal_Test;
class InertiaRegularizationSolve_NarrowBoxCurvatureNeverTripsSingularitySignal_Test;
// Test harness for the native variable-bound machinery: reaches the private
// interior push, the bound-multiplier direction/update helpers and the
// bound_duals_/bounds_ state so each can be checked against a hand calculation
// without a full bounded solve (there is no fraction-to-boundary leg yet).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 101–124

`kDivergencePersistIters`: the Maratos-class worked example, the choice of three, and the corpus differential behind it. The `@brief` kept states the rule and the finite-overshoot scope.

```text
/// Number of consecutive trailing iterates that must ALL exceed a divergence
/// threshold before converge_check() declares DIVERGING on a finite (but large)
/// residual. A single iterate breaching a threshold no longer aborts the solve;
/// the breach must persist across this many iterations in a row.
///
/// Non-finite residuals (NaN/Inf) remain an immediate hard abort — no iterate
/// recovers from a corrupted state — so this window governs only the
/// finite-overshoot case, where a single blown-up iterate can be a recoverable
/// transient rather than true divergence. The classic Maratos-effect example
/// (min 2(x1²+x2²−1)−x1 s.t. x1²+x2²−1=0, started on the constraint manifold)
/// makes the case concrete: under every solver configuration it takes one step
/// whose equality residual momentarily explodes to ~5e15, then converges in
/// roughly forty iterations to the textbook optimum (obj −1) with no recovery
/// machinery engaged; a per-iterate abort mistakes that single-iteration
/// excursion for divergence and kills an otherwise convergent solve.
///
/// Three is the smallest window that survives the observed one- and
/// two-iteration recoverable excursions (Maratos-class overshoots,
/// restoration-entry transients) while still failing fast — within three
/// iterations of the onset — on genuine divergence. It is this engine's own
/// policy choice with no external reference: Ipopt ships no divergence abort at
/// all. The supporting evidence is the corpus differential — the same
/// literature problem diverges at iteration two with the per-iterate abort and
/// converges to the optimum without it.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 127–144

The globalization-component forward declarations: which concrete type each holds and why RestorationStrategy is the one not always constructed. Kept in six lines.

```text
// InteriorPointSolver owns its globalization machinery through unique_ptr
// members whose concrete types are complete only in interior_point_solver.cpp:
// AcceptanceStrategy (concrete ClassicMeritAcceptance or the generic modernized
// merit), GlobalizationMechanism (BacktrackingLineSearch), BarrierGovernor
// (ClassicAdaptiveGovernor or MonitoredBarrierGovernor), RecoveryChain
// (NoopRecovery installed only on the all-default path -- max_soc_ == 0,
// ls_extended_iters_ == 0, watchdog_ == false, restoration_mode_ == off; live
// SocRecovery/ExtendedBacktrackRecovery/WatchdogRecovery/
// FeasibilitySwitchRecovery links exist for every opt-in -- see
// rebuild_globalization_components()), and RestorationStrategy (ProximalSwitchRestoration
// or NestedL1Restoration). Because those members are unique_ptr to incomplete
// types, the constructors and destructor are declared here and defined
// out-of-line in interior_point_solver.cpp. The detail/globalization/
// acceptance_strategy.h header includes THIS header, so it must not be included
// back here. Unlike the four always-built components, RestorationStrategy is NOT
// always constructed: rebuild_globalization_components() leaves it null unless
// restoration_mode_ != off, so on the default path every restoration branch
// guards on `restoration_ != nullptr` and is provably dead.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 153–156

`class TraceSink`: why it is forward-declared rather than included. Kept in three lines, without the task provenance.

```text
/// @brief FORWARD-DECLARED, NOT INCLUDED (M6 W4 T4; W5 T4 moved and renamed it):
/// `drivers/trace.h` pulls `sqp_types.h`, `qp_types.h`, `solver_status.h` and the
/// evidence blocks, none of which this driver uses. Only `attach_trace`'s
/// parameter and one member pointer name the type here; the .cpp includes it.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 259–267

`Settings::max_feas_rest_`: which strategy reads the budget. Kept: the budget, the 0 case, the off case and the validate() rule.

```text
        /// Per-phase feasibility-restoration entry budget: the maximum number
        /// of times restoration mode may be entered within a single phase.
        /// Read by ProximalSwitchRestoration::entry_permitted()
        /// (globalization/proximal_restoration.h) or
        /// NestedL1Restoration::entry_permitted()
        /// (globalization/l1_restoration.h), whichever restoration_mode_
        /// selects. 0 refuses restoration entirely (budget exhausted before
        /// the first entry). Ignored when restoration_mode_ == off.
        /// validate() requires >= 0. Default 2.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 316–321

`Settings::acceptance_strategy_`: the bit-identity claim for the default. Kept: which enum selects what, and where the enums live.

```text
        /// classic_merit (default) reproduces today's fused backtracking merit
        /// line search bit-identically. merit selects the modernized merit
        /// family driven through the GENERIC AcceptanceStrategy path, with the
        /// penalty rule chosen by merit_penalty_rule_ (only read when
        /// acceptance_strategy_ == merit). Both enums live in
        /// interior_point_solver_fwd.h.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 327–335

`Settings::barrier_governor_`: the same, plus the funnel/filter combination rule, which is kept.

```text
        // --- Barrier-parameter governor (opt-in monitored free<->monotone) ---
        /// classic_adaptive (default) reproduces today's PROBE/LOQO free-mode
        /// barrier update bit-identically. monitored selects the free<->monotone
        /// MonitoredBarrierGovernor, which composes a ClassicAdaptiveGovernor as
        /// its free-mode delegate — so it may pair with any acceptance_strategy_.
        /// The funnel/filter acceptance strategies are designed to operate above
        /// a monotone barrier safeguard; validate() rejects them combined with
        /// classic_adaptive unless never_monotone_ is explicitly set. Enum lives
        /// in interior_point_solver_fwd.h.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 346–363

`Settings::restoration_mode_`: the two modes described at length. Kept in eight lines: what each does, that both compose with every strategy, and the shared budget.

```text
        // --- Feasibility restoration (opt-in proximal mode-switch / nested l1) ---
        /// off (default) reproduces today's behavior bit-identically: no
        /// RestorationStrategy is constructed and every restoration branch in
        /// the solver is provably dead. proximal_switch selects the proximal
        /// feasibility mode-switch (ProximalSwitchRestoration), which — on a
        /// ladder-exhausted step rejection at a not-near-feasible point — swaps
        /// the true objective for a proximal term until infeasibility is
        /// sufficiently reduced, then resumes optimality mode. l1_nested
        /// selects the nested l1 elastic feasibility restoration
        /// (NestedL1Restoration, globalization/l1_restoration.h) instead: the
        /// same trigger, but the l1 elastic reformulation runs as a condensed
        /// in-place phase reusing the outer barrier algorithm's KKT system
        /// rather than swapping the outer objective. Both modes compose with
        /// every acceptance_strategy_ and barrier_governor_ (no matrix
        /// restrictions — every shipped acceptance strategy implements the
        /// restoration exit test the modes rely on). Enum lives in
        /// interior_point_solver_fwd.h; the per-phase entry budget is
        /// max_feas_rest_ above, shared by both modes.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 397–409

`Settings::fixed_variable_treatment_`: the three treatments. Kept, compressed, including that all three reach the same solution.

```text
        // --- Fixed-variable treatment ---
        /// How a primal variable whose declared lower and upper bounds are
        /// equal is handed to the solver. MakeParameter (the default)
        /// eliminates it, so the factorized system is one row and column
        /// narrower per fixed variable and the variable's value in the returned
        /// solution is exact. MakeConstraint keeps it and adds one internal
        /// equality row per fixed variable, appended after every row the
        /// transcription declared, so the system is one row and column WIDER
        /// instead. RelaxBounds keeps it as a two-sided bounded variable whose
        /// bounds have been pushed apart by bound_relax_factor_, holding it
        /// near its value through the barrier. All three reach the same
        /// solution on a well-posed problem. Closed-set enum; it lives in
        /// non_linear_program.h alongside the classification that reads it.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 433–446

`Settings::inertia_mode_`: the two modes and the constants each uses. Kept, compressed.

```text
        /// KKT inertia-correction / regularization mode. classic (default) runs
        /// the on-demand inertia ladder under the full inertia condition (accept
        /// only (kkt_dim − m, m, 0)); on a singularity signal it engages the
        /// on-demand dual shift −δ_c, at most once per phase (then latched, see
        /// dc_latched_), and an exhausted ladder fails the step — SINGULAR_KKT
        /// when nothing resolves it. proximal_regularization bakes a persistent,
        /// decaying primal base shift ρ_k and an always-on barrier-scaled dual
        /// shift −δ_c into the base matrix each iteration (the same ladder still
        /// escalates on top when the base attempt has wrong inertia or is
        /// singular). ρ_k starts at kProxRegFloor and decays by decr_h_ toward
        /// that floor; δ_c uses the δ_c-ladder constants in
        /// globalization/inertia_regularization.h and is suppressed while a
        /// nested l1 restoration phase is active. Closed-set enum, so validate()
        /// needs no range check. Enum lives in interior_point_solver_fwd.h.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 460–465

`Settings::qp_scaling_`: the measured effect of enabling it, restated at the code site — a measurement in a header, which the comment rules exclude. Replaced by the direction of the effect and a pointer here.

```text
        /// MKL Pardiso MPS scaling (iparm[10]) flag, 0/1. OFF by default:
        /// enabling it measured -16% wall on PolarLT-class collocation problems
        /// and dropped perturbed pivots 95/120 -> ~0, but on the full example
        /// suite it deterministically degraded convergence elsewhere
        /// (Delta3Launch CONVERGED->ACCEPTABLE, TopputtoLowThrust 5.4x
        /// iterations, intermittent MultiSpacecraft divergence).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 546–563

`SolveResult::obj_val_`: the objective-scale seam and what a throw leaves behind. Both kept, compressed.

```text
        /// @brief Objective value at the returned point, on the CALLER's
        ///        scale: f(x), never Settings::obj_scale_ * f(x).
        ///
        /// The solver minimizes the scaled objective and every evaluation it
        /// takes reports the scaled value; the scale is divided back out once,
        /// at the end of the call, so this field and the multiplier blocks
        /// below describe the problem the caller posed.
        ///
        /// ON A COMPLETED CALL. That one division sits on the success path, so
        /// a call that threw part-way through its phase sequence leaves
        /// whatever the last phase wrote -- which is the SCALED value -- and
        /// the same holds for the multiplier blocks. Reading a result after a
        /// throw was never contractual; the scale is named here so it is not
        /// mistaken for a guarantee that survives one.
        ///
        /// The scale divided out is the one the call RAN at, captured at its
        /// entry: a scale written while a solve is in flight takes effect on
        /// the next call.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 572–584

`SolveResult::fixed_variable_treatment_`: why it is recorded and that configure_variable_treatment never substitutes. Both kept, compressed.

```text
        /// @brief Which Settings::fixed_variable_treatment_ this call actually
        ///        ran under (MakeParameter, MakeConstraint or RelaxBounds; see
        ///        NonLinearProgram::configure_variable_treatment). configure_
        ///        variable_treatment never substitutes a different treatment
        ///        than the one requested -- it either runs the requested one
        ///        or throws -- so this always equals the Settings field's
        ///        value at the time this call ran; it is recorded here so a
        ///        caller reading a SolveResult later does not have to have
        ///        kept its own copy of the setting to know which treatment
        ///        produced eq_lmults_'s shape (see that field's own doc).
        ///        Overwritten unconditionally by run_phase_sequence at the
        ///        start of every solve/optimize call, whether or not the
        ///        treatment actually changed anything on the NLP.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 588–600

`SolveResult::eq_lmults_`: the multiplier convention and the treatment-dependent tail. Both kept, compressed.

```text
        /// @brief Equality-constraint multipliers at the returned point, on
        ///        the CALLER's scale -- against L = f + lambda_e^T cE +
        ///        lambda_i^T cI - z, with no Settings::obj_scale_ factor.
        ///        Sized equal_cons_: the user's own declared equality rows,
        ///        PLUS -- only under fixed_variable_treatment_ ==
        ///        MakeConstraint -- one internal fixing row per bound-fixed
        ///        variable, appended after the user's own rows (see
        ///        NonLinearProgram's internal-fixing-row note and the
        ///        reinsertion-seam comment at the end of this class's
        ///        optimize()/solve()). Under MakeParameter or RelaxBounds no
        ///        such rows exist, so this is exactly the user's own equality
        ///        multiplier block. eq_cons_ (below) shares the same shape and
        ///        the same treatment-dependent tail.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 609–637

`SolveResult::bound_lmults_`: the sign identity, the reduced space, the emptiness rule and the return_best_ snapshot. All four kept, compressed.

```text
        /// @brief Variable-bound multipliers (z) at the returned point, on the
        ///        caller's scale (see eq_lmults_), combining
        ///        BoundDualState's separate z_lower_/z_upper_ into the single
        ///        signed z that nlp_model.h's stationarity convention (and this
        ///        solver's own z-form dual-infeasibility residual,
        ///        accumulate_bound_dual_terms in barrier_math.h) uses: z =
        ///        z_lower_ - z_upper_, so a component is >= 0 when that variable
        ///        sits at an active lower bound, <= 0 at an active upper bound,
        ///        and 0 when free. Dense over the SOLVER's reduced primal space
        ///        (size primal_vars_, index-aligned 1:1 with the solver's own
        ///        primal vectors) -- unlike primals_, this is NOT expanded to the
        ///        caller's full space: an eliminated (bound-fixed) variable has no
        ///        row in the reduced problem, so it has no multiplier to report
        ///        here (see the reinsertion-seam comment in
        ///        interior_point_solver.cpp's optimize()/solve() return path).
        ///        Empty when the problem has no finite variable bounds
        ///        (bounds_ == nullptr for the whole solve); reset alongside
        ///        bounds_ itself everywhere it goes null -- set_nlp(),
        ///        release(), and run_phase_sequence()'s entry (which also
        ///        covers a fixed-variable-treatment switch or a caller's own
        ///        clear_variable_bounds() call emptying the bound set on a
        ///        reused solver instance with no intervening set_nlp()) -- so
        ///        that stays true across every path that can drop the bound
        ///        set, not just a fresh NLP.
        ///        Included in the return_best_ snapshot/restore
        ///        (best_bound_duals_scratch_) alongside primals_/eq_lmults_/
        ///        iq_lmults_, so a non-converged return_best_ exit reports this
        ///        from the SAME best iterate as the rest of SolveResult, not
        ///        the last one evaluated.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 640–661

The terminal KKT residuals' banner: the scale rule, the restoration-active caveat and the NaN convention. All three kept, compressed.

```text
        // --- Terminal KKT residuals ---
        //
        // The four scalars of the iterate primals_ and the multiplier blocks
        // above describe, with IterateInfo's definitions -- the quantities
        // converge_check() gates on, so each is directly comparable against
        // its matching Settings tolerance. alg_impl selects the row.
        //
        // SCALE: kkt_inf_ and barr_inf_ are on the SOLVER's objective scale
        // (they carry Settings::obj_scale_); econ_inf_ and icon_inf_ are
        // constraint residuals and carry no scale. Unlike obj_val_ and the
        // multiplier blocks above, nothing here is unscaled on the way out.
        //
        // On a restoration-active exit -- restoration_mode_ != off with
        // converge_flag_ NOTCONVERGED or DIVERGING -- the four describe the
        // restoration subproblem on its proximal scale, not the NLP, and a
        // comparison against a Settings tolerance is meaningless there. The
        // warm-start value such a solve exports carries NO polish extension,
        // for the same reason (export_warm_start's EXTENSIONS note).
        //
        // Last phase wins on a multi-phase call, like the last_* diagnostics
        // below. reset_accumulators() sets them to NaN per call; NaN means
        // UNMEASURED, never "zero residual".
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 674–678

`SolveResult::total_time_`: the cross-engine timing-field comparison. Kept: informational-never-asserted, and the clock.

```text
        /// INFORMATIONAL, NEVER ASSERTED -- a timing is not this project's
        /// currency of correctness, counters are. No test asserts a value
        /// here. The SQP engine's SqpSolution::wall_seconds carries the same
        /// contract, on the same clock (std::chrono::steady_clock, which
        /// utils::Timer wraps), so the two engines' timing fields read alike.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 718–729

`recovery_depth_histogram_`: the five buckets. Kept, compressed, including that it counts rejections.

```text
        /// Per-rejection recovery-chain outcome depth, indexed by the
        /// kRecoveryDepth* constants in globalization/recovery_chain.h:
        /// [0] SOC, [1] extended backtracking, [2] watchdog, [3] unresolved
        /// (today's classic give-up: the originally-rejected step was simply
        /// taken; the ONLY bucket that increments when
        /// SOC/extended/watchdog are all off), [4] restoration (a
        /// feasibility-restoration mode-switch was taken — increments only
        /// when restoration_mode_ != off). Counts rejections — every
        /// should_dispatch_recovery-gated chain call plus the
        /// exhausted-inertia-correction dispatch that runs instead of the
        /// chain — not just ones where a recovery link actually intervened.
        /// Reset per solve alongside the other accumulators.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 732–743

`last_funnel_width_`: the sentinel cases and the per-solve/per-phase reset split. Kept, compressed.

```text
        /// Final funnel width (τ) reported by FunnelAcceptance::
        /// append_diagnostics() (globalization/funnel_acceptance.h) at the end
        /// of the most recent solve's LAST PHASE. Sentinel -1.0 when the
        /// selected acceptance strategy does not report this field (every
        /// strategy except funnel — the default AcceptanceStrategy::
        /// append_diagnostics() no-op leaves this untouched); -1.0 also reports
        /// when no acceptance test ran in the selected phase (e.g. the phase
        /// converged at its initial iterate). A multi-phase call (e.g.
        /// solve_optimize()) reports only the LAST phase's value, not a running
        /// total across phases. Reset per solve alongside the other
        /// accumulators; NOT touched by AcceptanceStrategy::reset() (the
        /// per-phase hook), only by reset_accumulators() (the per-solve hook).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 754–769

`last_filter_resets_`: the per-phase and per-barrier-subproblem scoping argument. The scope itself is kept.

```text
        /// Total number of filter-reset-heuristic clears
        /// (FilterAcceptance::filter_resets(), Ipopt n_filter_resets_ — see
        /// filter_acceptance.h rule (4)) reported at the end of the most
        /// recent solve's LAST PHASE. Sentinel -1 when the selected acceptance
        /// strategy is not filter. PER-PHASE semantics: the counter is cleared
        /// by FilterAcceptance::reset_bounds() at every phase boundary (via
        /// AcceptanceStrategy::reset(), called at the top of each
        /// run_phase_sequence() loop iteration), and append_diagnostics() is
        /// collected once per phase right before that reset runs for the NEXT
        /// phase — so a multi-phase call (e.g. solve_optimize()) reports only
        /// the LAST phase's total resets, not a running total across phases
        /// within the same solve() call. Under barrier_governor_ == monitored,
        /// each mu-event ALSO clears the counter (the acceptance strategy is
        /// reset per barrier subproblem), so this reports resets since the
        /// last mu-event of the last phase — the Ipopt-faithful
        /// per-subproblem scope, not a whole-phase total.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 772–782

`last_monotone_switches_`: the per-phase mechanism. Kept by reference to last_filter_resets_.

```text
        /// Number of free -> monotone handoffs during the most recent solve's
        /// LAST PHASE, reported by MonitoredBarrierGovernor::
        /// append_diagnostics() (globalization/monitored_governor.h). Sentinel
        /// -1 when the selected barrier_governor_ is not monitored.
        /// PER-PHASE semantics matching last_filter_resets_ above:
        /// MonitoredBarrierGovernor::reset() clears its own counters at every
        /// phase boundary (via BarrierGovernor::reset(), called at the top of
        /// each run_phase_sequence() loop iteration), and append_diagnostics()
        /// is collected once per phase right before that reset runs for the
        /// NEXT phase — so a multi-phase call reports only the LAST phase's
        /// totals, not a running total across phases.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 792–806

`last_feas_rest_entries_`: the reporters and the cross-mode counting argument. The conclusion — counting is identical across both modes — is kept.

```text
        /// Number of times feasibility restoration was entered during the most
        /// recent solve's LAST PHASE, reported by RestorationStrategy::
        /// append_diagnostics() (globalization/restoration.h;
        /// ProximalSwitchRestoration and NestedL1Restoration are today's
        /// concrete reporters — globalization/proximal_restoration.h,
        /// globalization/l1_restoration.h). WRITE-ONLY diagnostics field: no
        /// algorithm code reads it back. Sentinel -1 when no restoration
        /// strategy is constructed, i.e. restoration_mode_ == off. Same
        /// last-phase-wins semantics as last_monotone_switches_. Counting is
        /// identical across both modes: entries_ increments once per
        /// enter_restoration()/enter_nested() call, and iterations_in_mode_
        /// once per note_iteration() call while active — the nested mode has
        /// no separate inner/outer iteration split (its phase shares the outer
        /// loop's own iteration counter; see l1_restoration.h disclosure (a)),
        /// so this field means the same thing under both modes.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 817–831

`last_prox_reg_primal_`/`last_prox_reg_dual_`: why they are written from mode-local state rather than from a component hook. The two quantities and both sentinel cases are kept.

```text
        /// Proximal primal-dual regularization shifts applied at the LAST
        /// FACTORIZED ITERATION of the most recent solve's LAST PHASE, written
        /// by alg_impl() at phase close from mode-local state (there is no
        /// dedicated component object with its own append_diagnostics() hook,
        /// unlike the acceptance/governor/restoration fields above; and the
        /// trailing iterate-history entry is the wrong source because a
        /// converged exit appends a non-factorized convergence probe).
        /// last_prox_reg_primal_ is the persistent primal base shift ρ_k added
        /// to the Hessian diagonal at that iteration; last_prox_reg_dual_ is
        /// the barrier-scaled dual shift δ_c subtracted from the
        /// constraint-row diagonals (0.0 when suppressed inside a nested l1
        /// restoration phase). Sentinel -1.0 for BOTH fields when inertia_mode_
        /// != proximal_regularization — the classic path never writes them —
        /// and when a mode-on phase converged before its first factorization.
        /// Same last-phase-wins semantics as the fields above.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 854–870

`reset_accumulators()`: the field-by-field account of what it does and does not reset. Kept, compressed.

```text
        /// Resets the accumulated timing/iteration counters, the convergence
        /// flag, last_kkt_info_, the four terminal KKT residuals (to NaN --
        /// see their own note), the
        /// SOC/watchdog/recovery counters and every last_* diagnostic
        /// (including last_eval_exception_). primals_ and
        /// obj_val_ are overwritten unconditionally by alg_impl each phase, as
        /// is fixed_variable_treatment_ by run_phase_sequence at call entry.
        /// The four constraint-indexed blocks are emptied at solve entry (see
        /// clear_reported_constraint_blocks) and then written by alg_impl:
        /// eq_lmults_ and eq_cons_ when equal_cons_ > 0, iq_lmults_ and
        /// iq_cons_ when inequal_cons_ > 0 -- so a block the current problem
        /// has no rows for is empty rather than left over from an earlier
        /// call;
        /// bound_lmults_ is overwritten when the solve has finite variable
        /// bounds (bounds_ != nullptr).
        /// factor_mem_ and factor_flops_ reflect the last factorization's stats
        /// (set by init_impl) and are not accumulated across phases.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 905–983

`EarlyCallBackType`: the callback's variable space, the KKT matrix's three-part contract, the verify gate, and the read-only break with its argument about what a write used to reach. The rewritten block keeps the space rule, the value/structure split, the verify gate and the borrowed-view lifetime. THIS IS ALSO R3: the clause at 978–979 ("every one of those happens AFTER the step this iteration computes") was FALSE — the nested-restoration pre-exit inside the pre-factorization convergence check re-initialises XSL's multiplier blocks and abandons the iteration before any step is computed (src/drivers/interior_point_solver.cpp:2190–2191 selects that arm, :2199–2203 exits and continues). The replacement states the true lifetime and cites the line.

```text
    /// Type of the per-iteration early callback.
    ///
    /// CALLBACK VARIABLE SPACE. Both callbacks are handed the solver's own
    /// iterate, right-hand side and (for the early one) KKT matrix. On a
    /// problem with bound-fixed variables that space is the REDUCED one:
    /// variables whose bounds fix them are eliminated, so the primal block is
    /// narrower than the initial guess the caller passed to
    /// optimize()/solve(), and every segment offset inside these vectors
    /// follows the narrowed width. That is the only internally consistent
    /// choice -- the early callback receives the KKT matrix itself, so a
    /// full-space iterate beside it would have every block boundary in the
    /// wrong place. A callback that needs the caller's own numbering maps
    /// through NonLinearProgram::reduced_to_full(), and can rebuild a
    /// full-space primal vector with scatter_full_x(). The returned solution,
    /// by contrast, is always in the caller's space. print_stats() likewise
    /// reports the solver's primal count, i.e. the width of the system being
    /// factorized.
    ///
    /// THE KKT MATRIX ARGUMENT. The early callback is handed the solver's own KKT
    /// assembly buffer, by mutable reference, after this iteration's values have
    /// been assembled and immediately before the factorization that consumes them.
    /// What may be done with it has three parts.
    ///
    /// VALUE MUTATION IS SUPPORTED. Writing new coefficients into the entries the
    /// matrix already carries is a use this callback exists for: the factorization
    /// that follows reads what the callback left, and the symbolic analysis the
    /// solve is holding still describes the matrix, because a value never changed
    /// what the analysis was taken over.
    ///
    /// STRUCTURE MUTATION IS NOT SUPPORTED. Inserting an entry, removing one, or
    /// otherwise handing back a different sparsity pattern is outside what this
    /// callback offers. A structural edit is not a model event -- nothing was
    /// re-laid, so the program's structure epoch does not move -- and the symbolic
    /// analysis the solve is holding was taken over the pattern that has just been
    /// replaced. A caller that needs a different structure re-declares the problem
    /// and solves again; there is no in-flight route to one.
    ///
    /// THE VERIFY GATE IS WHAT CATCHES A STRUCTURAL EDIT. From this callback's
    /// first invocation in a call through the end of that call, every numeric
    /// factorization runs under the full pattern check rather than under the
    /// structure epoch's word for it: the factorization re-derives the buffer's
    /// pattern and compares it against the analyzed one. This holds no matter
    /// when the callback was armed -- including from inside the late callback,
    /// mid-call -- because it is the hand-out itself that turns the check on,
    /// not the fact of having called set_early_callback() at some earlier point.
    /// An edit that changed the structure is therefore refused by name,
    /// deterministically, at the first factorization that sees it -- not
    /// factorized against stale symbolics, and not left to surface as a backend
    /// error or worse. That check is the cost of holding the matrix: every
    /// factorization from the first hand-out onward pays one full pattern hash,
    /// which is what every call paid before the epoch gate existed. A call in
    /// which this callback never runs is unaffected and keeps the skip
    /// throughout.
    ///
    /// THE THREE VECTOR ARGUMENTS ARE READ-ONLY (M6 W5 T2, a DECLARED BREAK).
    /// XSL, PGX and RHS are handed over as `ConstEigenRef` -- borrowed views of
    /// the solver's own storage, valid for the duration of the call, showing
    /// this iteration at its EVALUATION stage: the model has been evaluated and
    /// the KKT matrix assembled, and the factorization has not run.
    ///
    /// The break is real and it is worth stating exactly, because the previous
    /// mutable spelling was not an idle one. There was never a documented
    /// mutation contract for these three -- the contract above is the KKT
    /// matrix's alone -- but a write to any of them DID reach the solve:
    /// PGX is READ six lines after this call (`v_rhs.prim_grad() += PGX` folds
    /// the objective gradient into the Newton right-hand side); RHS's
    /// constraint blocks are not written again between this call and the
    /// factorization, so they ARE the right-hand side the step is computed
    /// from, and its primal block is added to rather than replaced; and XSL is
    /// the LIVE ITERATE, so a write to it landed in the solve in flight. (That
    /// storage is not the callback's last word on it either way: the step
    /// commits with `XSL += alpha*DXSL`, the restoration entry re-initialises
    /// its two multiplier blocks, and a return-best exit replaces the whole
    /// vector -- but every one of those happens AFTER the step this iteration
    /// computes from what the callback left there.)
    /// T2 removes that undocumented ability. No callback in this
    /// repository or in tycho relied on it -- every one of them only reads --
    /// but a consumer that did would change behaviour, not merely fail to
    /// compile.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 993–997

The constructor/destructor banner. Kept, compressed.

```text
    // --- Constructors / destructor ---
    // All three are defined out-of-line in interior_point_solver.cpp: the
    // unique_ptr members with incomplete element types force even the
    // constructors' exception-cleanup paths (and the destructor) to see the
    // complete types, which are only available in the .cpp.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1009–1013

The deleted copy/move members: why they are deleted explicitly rather than left implicit. The reason they cannot be defined is kept.

```text
    // Neither copyable nor movable: the kkt_sol_ factorization and the
    // unique_ptr<...> globalization components have no defined transfer
    // semantics. The out-of-line destructor above already suppresses the
    // implicit move members; deleting all four explicitly puts the constraint
    // at the declaration rather than at a failed call site.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1036–1044

`kkt_analysis_count()`: why it is not a SolveResult field. Kept, compressed.

```text
    /// @brief How many times this solver has laid and analyzed the KKT
    ///        sparsity pattern, over this object's LIFETIME.
    ///
    /// Deliberately not a SolveResult field: that struct is reset per call,
    /// and the question this answers -- did a second solve against unchanged
    /// structures analyze again? -- is a cross-call one. Moves once per
    /// set_nlp() and once more per solve entry that finds the structures
    /// re-laid since the last analysis. release() returns it to zero along
    /// with the analysis it counts.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1331–1353

`set_obj_scale`: the two-boundary inverse argument and why a negative scale is refused. Both kept, compressed.

```text
    /// @brief Sets Settings::obj_scale_, the factor the objective is multiplied
    ///        by at evaluation.
    ///
    /// INTERNAL ONLY, in the sense that matters to a caller: the scale governs
    /// what the solver minimizes and therefore which iterates it takes, but it
    /// does not move what the solve REPORTS. SolveResult's objective value and
    /// its three multiplier blocks are divided back out before they leave, and
    /// a multiplier seed handed to set_initial_multipliers() is multiplied in
    /// on the way through -- so both boundaries speak the caller's convention
    /// and a seed round-tripped through a solve means the same thing at any
    /// scale.
    ///
    /// STRICTLY POSITIVE. A positive scale leaves the minimizer where it was
    /// and rescales the multipliers, which is what makes the two boundaries
    /// above exact inverses. A negative one would reverse the problem --
    /// minimizing s*f for s < 0 maximizes f -- while leaving the multiplier
    /// cones the solve reports against unchanged, so a sign-constrained dual
    /// would come back with a sign its own convention rules out. Maximization
    /// is a different problem statement rather than a scale, and is not what
    /// this setting offers.
    ///
    /// @param scale Dimensionless scale; any finite, strictly positive value.
    /// @throws std::invalid_argument if scale is not finite or is not > 0.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1419–1431

`apply_preset`: the nine fields and where the table lives, both kept; the Python-binding docstring note is dropped.

```text
    // --- Named configuration presets ---
    /// @brief Applies a named globalization preset.
    ///
    /// Assigns exactly nine Settings fields (acceptance_strategy_,
    /// merit_penalty_rule_, barrier_governor_, never_monotone_,
    /// restoration_mode_, inertia_mode_, max_soc_, ls_extended_iters_,
    /// watchdog_); every other field (tolerances, iteration caps, QP
    /// parameters, ...) is left untouched. The preset table -- field values,
    /// evidence-of-record citations, and the name list the error message
    /// dispatches against -- lives in
    /// detail/drivers/interior_point_solver_presets.h. The Python binding's
    /// docstring repeats the preset names by hand; a Python test pins it
    /// against this table.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1458–1467

`attach_trace`: the borrowed-sink rule, kept, and the comparison against SqpDriver::attach_trace, dropped.

```text
    // --- Machine trace (schema v0) ---
    /// @brief Attaches a trace sink; `nullptr` (the default) is off.
    ///
    /// THE SINK IS BORROWED and must outlive every solve made while it is
    /// attached. Every emit site null-checks, and an unattached solve builds no
    /// event and does no census -- it pays exactly what it paid before.
    ///
    /// Mirrors `SqpDriver::attach_trace` with one difference the schema names:
    /// this driver has no sub-engine to forward to, and its `ipm.solve` pair
    /// moves no `depth` because it nests no driver of its own.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1485–1497

`staged_eq_mults_`: the consumption point, the validation point and the at-most-once application. All three kept, compressed.

```text
    /// Staged constraint-multiplier seeds. Consumed -- moved into run-local
    /// state and mults_staged_ cleared -- at the very start of the NEXT
    /// run_phase_sequence() call, before anything in that call (settings
    /// validation, variable-treatment reconfiguration, ...) gets a chance to
    /// throw and leave this armed for an unrelated later call.
    /// validate_staged_multipliers() then rejects a mis-sized or non-finite
    /// seed immediately once equal_cons_/inequal_cons_/user_equal_cons_ are
    /// final for the call -- before the entry init_impl/factorization, and
    /// before any phase runs, on every entry point. Applied at most once
    /// within the call, to whichever XSL is current when the phase loop
    /// reaches the first OPT/OPTNO-mode phase in the requested sequence --
    /// never applied at all when the sequence has no such phase (e.g. a bare
    /// solve()). An unseeded solve does not touch any of this.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1552–1596

`export_warm_start`: the block map, the sign identity, the declaration-key argument and the extension's conditions. All kept, compressed.

```text
    /// @brief The warm-start value of the last completed solve, in DECLARED
    ///        space.
    ///
    /// Blocks, all at declared dimensions: `primal_` is the returned primal
    /// vector, an eliminated variable carrying the value the treatment holds
    /// it at; `eq_lmults_` is the USER's equality rows only, so the
    /// MakeConstraint treatment's internal fixing rows are dropped;
    /// `iq_lmults_` is the inequality block as reported; `bound_lmults_` is
    /// result().bound_lmults_ mapped out of the solver's reduced space, an
    /// exact zero at every eliminated variable and at every entry a solve with
    /// no finite variable bounds reports nothing for.
    ///
    /// SIGN: z = z_lower - z_upper, verbatim from SolveResult::bound_lmults_ --
    /// the engine's convention and the currency's, so a value round-tripped
    /// through the currency means the same thing at both ends.
    ///
    /// The stamp is the bound program's DECLARATION key
    /// (model/structure_identity.h's declaration_key over declaration()) AS OF
    /// that solve's completion, not as of this call. The declaration key and
    /// not the layout key: what the value claims is the PROBLEM it was taken
    /// on, which is the only thing a hand-off crossing engines or
    /// fixed-variable treatments can be held to -- warmstart/warm_start_data.h
    /// carries the ruling and the argument. ModelStructureKey stays what it
    /// always was, the layout/epoch key, and is not this stamp.
    ///
    /// EXTENSIONS: exactly one, `"hven.ipm.polish.v1"`
    /// (warmstart/ipm_polish_extension.h), and only when the solve had a
    /// non-empty variable-bound set AND did not end on a restoration-active
    /// exit (SolveResult's own caveat on the four residuals names that
    /// condition). It carries what the signed core block cannot: the
    /// invertible (z_lower, z_upper) pair at declared width, the inequality
    /// values cI(x) the crossover judges rows against, and the barrier
    /// parameter the solve ended at -- all on the caller's objective scale,
    /// like the core blocks beside them. A problem with no finite variable
    /// bounds carries no extension, because there is no pair to carry; a
    /// restoration-active exit carries none because its pair, its barrier
    /// level and (under l1_nested) its inequality values are the RESTORATION
    /// subproblem's, not the declared problem's. In both cases the core-only
    /// value is the whole hand-off, which stages and applies exactly as any
    /// other core-only value does.
    ///
    /// @return The captured value, by copy.
    /// @throws std::logic_error if no solve has completed on this instance --
    ///         never an empty payload, which would stage cleanly and then
    ///         silently cold-start.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1599–1677

`stage_warm_start`: the one-shot rule, what a stamp mismatch does and does not mean, the check split, the precedence against a multiplier seed, and what is applied including the polish extension. All kept, compressed.

```text
    /// @brief Stages a warm start for the NEXT solve on this instance.
    ///
    /// ONE-SHOT AND LOUD. The value applies to the next run_phase_sequence()
    /// call and is consumed by it, applied or refused; it survives any
    /// re-bind or re-lay in between; and a live stamp mismatch at that call
    /// REFUSES rather than being silently dropped or silently cold-started
    /// over. A caller wanting a second warm solve stages again.
    ///
    /// WHAT A STAMP MISMATCH MEANS HERE: the caller transcribed a DIFFERENT
    /// PROBLEM -- different declared dimensions, or a different declared bound
    /// STRUCTURE (which sides are finite, and which variables are fixed). It
    /// does NOT mean a different fixed-variable treatment or a different
    /// layout: the stamp is the declaration key, so a value exported under one
    /// treatment stages and applies under another on the same declaration,
    /// which is safe because the blocks are declared-space and application
    /// ignores the coordinates an eliminating treatment holds.
    ///
    /// AND WHAT A MATCH DOES NOT PROMISE: the stamp hashes neither the pieces'
    /// row structure nor bound VALUES, so a re-transcription that re-splits the
    /// same rows, or that moves a finite bound without changing which sides are
    /// finite, matches. warmstart/warm_start_data.h states the whole
    /// guarantee.
    ///
    /// CHECKED HERE: every block's length against the declared dimensions, and
    /// finiteness. NOT checked here: the stamp -- it is compared once, at
    /// solve entry, and a mismatch refuses there naming both DECLARATION key
    /// digests.
    ///
    /// NON-CONSUMING (R5): the argument is taken by const reference and
    /// copied. Staging the same value twice from the same cold state produces
    /// the same start state.
    ///
    /// CLEARS FIRST: this call, WHETHER IT SUCCEEDS OR REFUSES, first drops any
    /// warm start and any multiplier seed staged before it. A caller whose
    /// staging is refused is cold, not still holding the previous payload.
    ///
    /// PRECEDENCE: staging a warm start REPLACES any staged multiplier seed,
    /// and a seed staged AFTER a warm start is discarded unapplied at solve
    /// entry. The two describe the same multiplier blocks.
    ///
    /// WHAT IS APPLIED: `primal_` becomes the solve's starting point, mapped
    /// declared -> reduced (values at eliminated variables are ignored -- the
    /// treatment holds those coordinates and nothing is written to them), and
    /// then pushed into the interior of the declared bounds like any starting
    /// point. `eq_lmults_`/`iq_lmults_` are installed through the same staged-
    /// seed path set_initial_multipliers() feeds, with the same clamps and the
    /// same objective-scale handling. `bound_lmults_` is validated and carried
    /// but NOT installed, and it never will be: the signed core block does not
    /// invert into the (z_lower, z_upper) pair the barrier state needs at a
    /// two-sided bound. The invertible form travels instead in the
    /// `"hven.ipm.polish.v1"` extension, which THIS ENGINE CONSUMES: when the
    /// staged value carries it, the pair seeds the bound multipliers in place
    /// of the fresh `Settings::init_mu_`-and-distance seed, after the starting
    /// point has been pushed into the interior and under the same
    /// [kSeededIqMultFloor, kSeededMultInitMax] clamps and the same
    /// objective-scale multiply-in the constraint-multiplier seed takes. The
    /// payload's barrier parameter is NOT consumed: the barrier schedule is a
    /// Settings decision the caller owns, and a payload silently overriding
    /// `init_mu_` would be a value rewriting a setting. A value WITHOUT the
    /// extension behaves exactly as a core-only value always has -- the point
    /// and the constraint multipliers are restarted and the bound multipliers
    /// are seeded fresh.
    ///
    /// UNKNOWN extension tags are ignored (R3): a capability downgrade, not an
    /// error. A MALFORMED payload under the KNOWN tag is neither, and is
    /// refused HERE, at staging, naming the tag -- corruption is not a foreign
    /// tag, and a payload that cannot be read must not reach a solve that
    /// would then silently cold-seed its bound multipliers.
    ///
    /// @param data The value to stage, in DECLARED space.
    /// @throws std::runtime_error if no NLP has been set.
    /// @throws std::invalid_argument if any block's length is not the matching
    ///         declared dimension (naming the block, the length held and the
    ///         length declared), if any block holds a non-finite value, if the
    ///         value carries the polish tag more than once, or if a payload
    ///         under that tag is malformed or is not at the declared widths
    ///         (naming the tag). A stamp mismatch is refused at solve entry,
    ///         not here. Every one of these refusals still leaves this
    ///         instance with nothing staged.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1737–1750

`recovery_`: the composition order of the opt-in links. The default's identity and the opt-in route are kept.

```text
    // Post-rejection recovery chain (a hook point wired with a no-op
    // implementation on the default path). Held through the RecoveryChain
    // interface (forward-declared above); rebuilt by
    // rebuild_globalization_components() alongside
    // acceptance_/mechanism_/governor_. Never null once run_phase_sequence has
    // run it once, which every solve entry point guarantees before any
    // iteration. With max_soc_ == 0, ls_extended_iters_ == 0, and watchdog_ ==
    // false (all defaults), rebuild_globalization_components() installs plain
    // NoopRecovery, which always returns kAcceptAsIs and is stateless —
    // bit-identical to pre-recovery-chain behavior. Opt in to any subset of
    // SocRecovery/ExtendedBacktrackRecovery (composed in that order by
    // ChainedRecovery) and WatchdogRecovery (an outer decorator over whatever
    // chain results) via the corresponding Settings fields — see
    // globalization/soc.h and globalization/watchdog.h.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1753–1767

`restoration_`: which concrete type each mode holds and where the guards are. Kept, compressed.

```text
    // Optional feasibility-restoration mode-switch. Held through the
    // RestorationStrategy interface (forward-declared above). Unlike
    // acceptance_/mechanism_/governor_/recovery_ this is NOT always
    // constructed: rebuild_globalization_components() leaves it null unless
    // restoration_mode_ != off, in which case it holds a
    // ProximalSwitchRestoration (restoration_mode_ == proximal_switch) or a
    // NestedL1Restoration (restoration_mode_ == l1_nested), and
    // FeasibilitySwitchRecovery is wrapped as the outermost recovery link
    // either way. On the default path (off) it stays null and every
    // restoration branch in eval_nlp / the classic+generic trial-eval seams /
    // alg_impl guards on `restoration_ != nullptr` (or
    // `ctx.restoration_ != nullptr`) and is provably dead.
    // run_phase_sequence() resets it (when present) at each phase boundary
    // alongside the other components, and collects its diagnostics into
    // SolveResult::last_feas_rest_entries_/last_feas_rest_iters_.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1777–1786

`rebuild_globalization_components`: the live-at-next-solve argument. Kept, compressed.

```text
    // (Re)builds acceptance_/mechanism_/governor_/recovery_ from the current
    // Settings. Called once per run_phase_sequence(), right after the
    // variable-treatment configuration and before the first phase (i.e.
    // once per solve invocation — optimize()/solve()/etc. all route through
    // it), NOT from set_nlp(): construction-time knobs (acceptance_strategy,
    // max_soc, ls_extended_iters, watchdog, merit_penalty_rule) must take
    // effect on the very next solve even without a re-transcription in
    // between, matching every other Settings field's live-at-next-solve
    // semantics. See interior_point_solver.cpp's definition for the neutrality
    // argument on the default (all-off) path.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1797–1804

`unscale_reported_outputs`: why the caller's problem is what leaves the class. Kept in two lines.

```text
    /// @brief Divides Settings::obj_scale_ back out of the reported objective
    ///        value and the three multiplier blocks, once per solve call.
    ///
    /// The solver minimizes obj_scale * f, so its multipliers and its reported
    /// objective are the scaled problem's. What leaves this class is the
    /// caller's problem: nlp_model.h's stationarity convention is stated at a
    /// unit scale, and SolveResult's fields are read against it. A unit scale
    /// -- the default -- returns without touching anything.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1816–1824

`kkt_pattern_check`: what the guard trades and where it is moved back. The trade is kept in one line.

```text
    /// @brief The pattern-guard mode a numeric factorization runs under right
    ///        now: kAssumeAnalyzed while kkt_pattern_is_analyzed() holds and
    ///        this call is not verifying throughout, kVerify otherwise.
    ///
    /// One epoch read per factorization in place of one full-KKT pattern hash
    /// per factorization. The guard is not dropped -- it is moved onto the
    /// signal that actually answers the question it asks, and moved back onto
    /// the hash for any call that hands the matrix out (see
    /// verify_kkt_pattern_for_solve_).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1827–1839

`clear_reported_constraint_blocks`: the reused-solver failure it prevents. Kept, compressed.

```text
    /// @brief Empties the four constraint-indexed result blocks -- the
    ///        equality and inequality multipliers and residuals.
    ///
    /// Called wherever result_.bound_lmults_ is cleared, and for the same
    /// reason. alg_impl writes the equality pair only when the current problem
    /// has equality rows and the inequality pair only when it has inequality
    /// rows, so without this a solver reused across a constrained problem and
    /// then an unconstrained one would keep the earlier call's block standing
    /// -- a nonempty block that the current problem has no rows to justify,
    /// and one the objective-scale seam would then divide a second time on
    /// every subsequent call. Emptied rather than resized to the current
    /// counts: a block with no rows behind it is empty, which is what these
    /// fields' own documentation promises.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1856–1861

The scratch buffers' serial-invocation argument. Kept in three lines.

```text
    // --- Reusable per-iteration scratch buffers (avoid per-call heap allocation) ---
    // complementarity()/barrier_hessian() are only ever invoked serially from
    // alg_impl's single-threaded control loop for this InteriorPointSolver instance (no
    // partition-level concurrency at this level -- that only happens inside
    // NLP eval calls). Sized to inequal_cons_/slack_vars_ (resize-in-place;
    // a no-op once the size matches, which it does for the lifetime of a solve).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1865–1877

`best_xsl_scratch_`/`best_rhs_scratch_`/`best_bound_duals_scratch_`: the hoist argument and the reason the bound pair needs its own snapshot. Both kept, compressed.

```text
    // alg_impl's return_best_ path (off by default, settings_.return_best_)
    // copies the full XSL/RHS iterate on every improving iteration. Hoisted so
    // repeated alg_impl calls (one per phase in run_phase_sequence) reuse the
    // same backing store instead of starting from an empty vector each time;
    // resize-on-assign is then a no-op once kkt_dim_ is stable across a solve.
    Eigen::VectorXd best_xsl_scratch_; ///< @internal alg_impl() return_best_ XSL snapshot.
    Eigen::VectorXd best_rhs_scratch_; ///< @internal alg_impl() return_best_ RHS snapshot.
    // bound_duals_ (the z_lower_/z_upper_ pair SolveResult::bound_lmults_ is
    // built from) has no XSL/RHS-carried counterpart -- it is separate solver
    // state -- so the return_best_ substitution needs its own snapshot of it,
    // taken and restored alongside best_xsl_scratch_/best_rhs_scratch_, or a
    // non-converged return_best_ exit would report bound_lmults_ from the
    // LAST iterate beside primals_/eq_lmults_/iq_lmults_ from the BEST one.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1880–1889

The nested-restoration eval-seam scratch: the field-by-field map. Kept, compressed.

```text
    // Nested feasibility-restoration eval-seam scratch (all dead unless a
    // nested restoration strategy is active). The seam runs in the
    // per-iteration hot path, so these back the condensed-elastic outputs
    // without per-call heap allocation, following the *_scratch_ discipline
    // above: resize-on-assign is a no-op once dims are stable across a solve.
    // resto_pdiag_scratch_ holds the proximal Hessian diagonal η(μ)·D_R²
    // (primal_vars_); resto_epiv_/ipiv_scratch_ hold the NEGATED constraint-row
    // pivots scattered into the KKT (y,y) blocks (equal_cons_/inequal_cons_);
    // resto_ec_/ic_scratch_ copy the raw constraint residuals out before the
    // condensed r̃ overwrites the RHS segments in place.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1896–1909

The nested-restoration lifecycle state: the field map and the mu-event reset invariant. Both kept, compressed.

```text
    // Nested feasibility-restoration lifecycle state (all dead unless a nested
    // restoration strategy is active). stashed_mu_ holds the outer barrier
    // parameter captured at entry; the governor drives a fresh in-phase schedule
    // in between, and the multiplier re-entry restores it on exit. resto_first_iter_
    // guards the first phase iteration (take at least one step before any
    // exit test fires). resto_theta_orig_prev_ carries the previous phase
    // iteration's original-problem infeasibility for the per-iteration κ_resto
    // ratchet (seeded at entry with the entry-point value, ratcheted each
    // iteration — NOT frozen at entry). resto_dz_scratch_ backs the re-entry
    // slack-multiplier Newton step, following the *_scratch_ no-per-call-alloc
    // discipline. This state obeys the same reset invariant as the acceptance
    // stash: a μ-event reset() mid-phase does NOT touch it (only the phase-
    // boundary reset in run_phase_sequence() clears it), so the stashed outer μ
    // survives a barrier subproblem restart inside the phase.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1914–1920

`resto_bound_dz_*_scratch_`: why they are not bound_duals_.dz_*. Kept in four lines.

```text
    // The bound families' re-centring steps at that same return, backing the
    // two sides separately because they index different lists. Deliberately
    // NOT bound_duals_.dz_*: that pair is the ITERATE's Newton direction,
    // consumed by the commit and by the fraction-to-boundary rule, and the
    // restoration return is a different event that applies no dz — see the
    // two-event note on bound_duals_ below. Empty unless a nested restoration
    // phase returns on a problem with variable bounds.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1924–1935

`bounds_`: when it is set and what null means. Both kept, compressed.

```text
    // --- Native primal variable bounds (all inert on a problem without any) ---
    //
    // bounds_ points at the NonLinearProgram's classification of the finite
    // variable bounds this solve must keep barrier terms for, in the solver's
    // REDUCED index space. It is set ONLY on the configuration success path in
    // run_phase_sequence() and ONLY when the set is non-empty: a configuration
    // that threw leaves a rejected classification behind on the NLP, so the
    // pointer is cleared before the configuration attempt and re-read after it,
    // and set_nlp()/release() clear it too. Null therefore means "this solve has
    // no variable-bound barrier terms", which every bound branch in this class
    // and in the globalization components tests -- and which is what makes the
    // KKT assembly on such a problem byte-identical to the pre-bounds solver.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1937–1950

`bound_duals_`: the two-event write discipline and its parity with the slack multipliers. The two events are kept.

```text
    // The bound multipliers and their Newton step, index-aligned to bounds_'s
    // two lists. Iterate state, so solver-owned rather than NLP-owned; sized by
    // the interior push at solve entry and empty whenever bounds_ is null.
    //
    // TWO EVENTS write z, and only two. It MOVES ALONG dz at exactly one site,
    // the iterate commit (apply_bound_dual_step), once per committed iterate and
    // with the κ_Σ clip against the new x. It is RE-ANCHORED at exactly one
    // other, the nested restoration return (exit_feasibility_restoration_nested),
    // which applies no dz and moves no x — it re-centres z on the stashed outer
    // barrier parameter because the phase it is returning from ran on a
    // different one. The two are different event classes: different formula,
    // different damping, different trigger. The slack multipliers have always
    // lived under exactly this discipline (the same restoration return rewrites
    // them); the bound family matches it rather than being the exception.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 1989–2004

`analyzed_structure_epoch_`: why an epoch rather than the treatment call's outcome. Kept, compressed.

```text
    // The structure epoch the KKT sparsity analysis was laid against, and
    // whether there has been one at all.
    //
    // WHY AN EPOCH RATHER THAN THE OUTCOME OF THE TREATMENT CALL: a re-lay
    // resets the NLP's location table to -1 and drops its analyzed-destination
    // capture, and the treatment call reports only whether IT rebuilt
    // anything. Every other structural event -- a partition renegotiation, a
    // re-transcription, a declaration adoption replaying identical bounds --
    // re-lays without moving treatment, relax factor or bounds revision, so
    // the treatment call takes its idempotence shortcut and reports no change
    // while the table it left behind names no destination at all. The epoch is
    // the model's own record that its structures were re-laid, and it moves
    // for all of them.
    //
    // Reset with the analysis it describes: release() drops both, and
    // set_nlp() re-lays and re-records.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2026–2034

`solve_exit_mu_`: the scale, the write cadence and its single reader. All kept, compressed.

```text
    // The barrier parameter the last phase of this solve ended at, on the
    // CALLER's objective scale (the capture divides solve_obj_scale_ out, like
    // every other quantity that stands in a complementarity relation with a
    // multiplier). Written once per phase at the same point result_ takes the
    // rest of its per-phase fields, so a multi-phase call ends with the LAST
    // phase's value -- the same last-phase-wins semantics every other
    // diagnostic there has. Read by exactly one thing: the polish extension's
    // `mu_`, which is the hand-off's own statement of how loose it is. Nothing
    // in the solve reads it back.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2037–2060

`solve_exit_restoration_active_`: why the extension and not the core is suppressed, and why the suppression is keyed on the exit rather than on the values. Both kept, compressed.

```text
    // Did the last phase of this solve end with feasibility restoration still
    // active (SolveResult's restoration-active-exit condition, the four
    // residuals' own caveat)? Written once per phase beside solve_exit_mu_,
    // same last-phase-wins semantics. Read by exactly one thing: the capture,
    // which SUPPRESSES the polish extension when it is true.
    //
    // WHY THE EXTENSION AND NOT THE CORE. Every block the extension carries is
    // restoration-space on such an exit: under l1_nested the RHS constraint
    // rows hold the condensed r-tilde, so result_.iq_cons_ is not cI(x) at all,
    // and under either mode the bound-dual pair and mu describe the
    // restoration subproblem's own barrier. The extension's contract states
    // those blocks as cI(x) and as non-negative prices at a barrier level
    // (warmstart/ipm_polish_extension.h), and the crossover bridge infers
    // activity from them -- a payload must not claim that with
    // restoration-space values. The CORE blocks make no such claim: they are
    // "the point and multipliers this solve returned", which is exactly what
    // they are, and the caveat on the four residuals above documents the scale
    // they are on.
    //
    // KEYED ON THE EXIT, NOT ON THE VALUES. return_best_ substitutes an
    // optimality-mode iterate on these exits (best-iterate tracking is
    // suspended while restoration is active), so its blocks would be clean --
    // the suppression applies there too rather than resting on a chain of
    // reasoning about which iterate a substitution happened to leave behind.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2092–2098

The deleted rvalue `kkt_view` overload: the history of when the mistake became spellable. The refusal's reason and the const-qualification are kept.

```text
    /// @brief Refused for a TEMPORARY. The overload above binds an rvalue --
    ///        `kkt_view(expr.eval())` compiles -- and ConstKKTVector holds a
    ///        reference, so the view would outlive its storage. Before T2 the
    ///        only overload took `Eigen::VectorXd &` and the mistake could not
    ///        be spelled; this keeps it that way. Const-qualified so the refusal
    ///        also covers a call from a const member function, where the
    ///        non-const candidate above is not viable.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2124–2134

`validate_staged_multipliers`: the two admissible equality sizes and the call point. Both kept, compressed.

```text
    // Rejects a mis-sized or non-finite staged seed: eq_mults must be sized
    // to either the problem's user-facing equality row count or the
    // post-treatment count that additionally counts one internal fixing row
    // per fixed variable under the MakeConstraint treatment
    // (NonLinearProgram::user_equal_cons_ vs. equal_cons_ -- see
    // install_fixed_variable_rows), iq_mults must be sized to inequal_cons_,
    // and every entry must be finite. Called once, from run_phase_sequence,
    // right after refresh_nlp_dimensions() would have run (equal_cons_/
    // inequal_cons_/user_equal_cons_ are final for this call at that point)
    // -- so a bad seed is rejected before the entry init_impl/factorization,
    // and before any phase runs and mutates result_, on every entry point.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2147–2157

`validate_warm_start_blocks`: the declared-space rule, the treatment-invariance argument and where the dimensions are read from. All kept, compressed.

```text
    // Rejects a warm-start value whose blocks are not at the DECLARED
    // dimensions -- primal_ and bound_lmults_ at the program's primal variable
    // count, eq_lmults_ at its USER equality row count (never the
    // post-treatment count: the currency is declared-space, and the
    // MakeConstraint treatment's internal fixing rows are not declared rows),
    // iq_lmults_ at its inequality row count -- or which holds a non-finite
    // value. Every dimension it reads is treatment-invariant, which is what
    // makes this checkable at staging time while the stamp is not. Reads them
    // off the program rather than off this solver's own cached copies, which
    // are refreshed only at set_nlp() and at solve entry and so may predate a
    // re-lay. `entry` names the public entry in the refusal.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2160–2171

`capture_completed_warm_start`: the defensive-but-not-fatal rule, the call point and the memoization cost. All kept, compressed.

```text
    // Captures completed_warm_ from result_ and the bound program, and arms
    // solve_completed_. DEFENSIVE BUT NOT FATAL: an internal-consistency check
    // that fails skips the capture and leaves solve_completed_ false (export
    // then refuses "no completed solve") rather than throwing one line before a
    // completed solve's return -- see the banner at the definition.
    // Called once, at the end of run_phase_sequence, AFTER
    // the reinsertion seam (so result_.primals_ is already in declared space)
    // and after the objective-scale seam (so every multiplier block is on the
    // caller's scale). One structural-key read per solve: both of its digests
    // are memoized per lay by the program, so a solver solving repeatedly
    // against unmoved structures pays the O(claims)/O(variables) digests once,
    // not once per solve. Nothing per iteration.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2174–2192

`build_polish_extension`: what it builds, the scale-divided-on-a-copy rule and the precondition. All kept, compressed.

```text
    // Builds the "hven.ipm.polish.v1" extension for the value being captured:
    // the (z_lower, z_upper) pair scattered out of the solver's reduced space
    // into declared coordinates, the inequality values `iq_values` (already
    // reduced to the declared block by the caller), and the barrier parameter
    // the solve ended at. Returns false, writing nothing, if any
    // internal-consistency check on the reduced->declared mapping fails, on
    // exactly the DEFENSIVE-BUT-NOT-FATAL terms capture_completed_warm_start
    // itself is built on.
    //
    // THE OBJECTIVE SCALE IS DIVIDED OUT HERE, not at the seam
    // unscale_reported_outputs owns: the pair is read live out of
    // bound_duals_, which is the SOLVER's state at the SOLVER's scale and must
    // not be mutated by a side product of the solve. The capture copies and
    // divides; the live state is untouched.
    //
    // PRECONDITION: bounds_ != nullptr AND solve_exit_restoration_active_ ==
    // false (the caller's own gate -- a problem with no finite variable bounds
    // has no pair to carry, and a restoration-active exit has one that
    // describes the wrong problem; both carry no extension at all).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2195–2208

`validate_staged_polish`: the four refusals, the foreign-tag rule and why the decoded value is discarded. All kept, compressed.

```text
    // Rejects a staged value whose "hven.ipm.polish.v1" payload cannot be
    // read, is not at the declared widths, holds a non-finite entry, or holds
    // a NEGATIVE entry in either bound-dual block -- the refusal names the tag
    // and, for the last two, the block (and for a negative, the coordinate and
    // its value). The sign check is the SQP staging path's too, in the same
    // terms: prices are non-negative by the extension's contract, so a
    // negative one is corruption rather than a seed. A value carrying NO
    // such extension is accepted silently (core-only is a supported hand-off);
    // a FOREIGN tag is ignored entirely (R3's capability downgrade). The
    // decoded value is DISCARDED: the bytes are the one source of truth, and
    // they are decoded again at application -- once per solve, against a
    // factorization, which is not a cost worth a second copy of the state and
    // the clearing discipline it would need. `entry` names the public entry in
    // the refusal.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2211–2222

`apply_polish_bound_duals`: the three steps, the call point and the magnitude-guard reading of the clamp. All kept, compressed.

```text
    // Installs a staged polish hand-off's bound multipliers over the fresh
    // seed push_initial_point_interior just wrote. Declared -> reduced by the
    // bound set's own index lists (the stamp guarantees both ends agree on
    // which sides are finite), clamped into [kSeededIqMultFloor,
    // kSeededMultInitMax] and multiplied by this call's objective scale --
    // the same three steps a staged constraint-multiplier seed takes, for the
    // same three reasons. Called once per solve, after the push (so the
    // distances the barrier divides by are already positive) and before any
    // evaluation. The clamp is a MAGNITUDE guard and stays one: validate_
    // staged_polish has already refused a negative entry, so what reaches the
    // floor here is a legitimate zero or near-zero, never a wrong sign.
    // PRECONDITION: bounds_ != nullptr.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2232–2247

`factor_impl`: the four scalar parameters' contracts. All kept, compressed.

```text
    // `finalpert` is the last perturbation DELTA applied via Perturb() -- this is
    // the exact value alg_impl's Hpert0 warm-start consumes today and must keep
    // consuming byte-identically (see the comment at its call site). `cumpert` is
    // a separate, display-only accumulator: the running SUM of every Perturb()
    // delta applied during this call (i.e. the actual total added to the KKT
    // diagonal), used only for the HPert iteration-table column. Neither
    // `finalpert` nor any control-flow decision in factor_impl reads `cumpert`.
    // `base_prox` is the proximal-regularization base shift (ρ_k on the Hessian
    // diagonal), read only when inertia_mode_ == proximal_regularization.
    // `dual_shift` is the δ_c magnitude AVAILABLE to this call for both modes:
    // the proximal branch applies it up-front; the classic branch applies it on
    // demand at the singularity signal (rank deficiency, or neigs < m), or up-front once
    // dc_latched_ is set (0.0 = suppressed, e.g. during nested l1 restoration).
    // `exhausted` is set (never cleared) when the ladder runs out of attempts
    // with inertia still wrong -- the return value alone cannot distinguish
    // that from success on the final attempt.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2267–2274

`complementarity_pair_count`: why the count is the union weight. Kept, compressed.

```text
    // How many pairs complementarity() reduced into its aggregates, given the
    // slack block length it was handed: the slack/multiplier pairs plus one per
    // finite variable bound. This is the weight the union average carries, and
    // therefore the base_count any FURTHER fold-in (augment_complementarity_nested)
    // has to re-weight against -- that helper reconstructs the base sum
    // as avgcomp*base_count, so a count that omitted the bound pairs would
    // reconstruct the wrong sum. Returns the slack count unchanged off the bound
    // path.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2276–2284

`augment_complementarity_nested`: the reconstruction rule and the reduction-ordering guarantee. Both kept, compressed.

```text
    // Folds an active nested restoration phase's elastic complementarity pairs
    // into complementarity()'s aggregates. base_count is the number of original
    // slack/multiplier pairs already reduced into avgcomp (so their sum can be
    // reconstructed as avgcomp*base_count and re-averaged over the union). A pure
    // no-op unless a nested restoration is active — the aggregates are returned
    // untouched off that path, so the default/proximal barrier machinery is
    // byte-identical. Only ever combines separately-computed aggregates (min of
    // mins, max of maxes, count-weighted average); it never re-reduces the
    // original pairs, so complementarity()'s reduction ordering is preserved.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2292–2301

`push_initial_point_interior`: the push formula, the crossing argument and the seeding rule. All kept, compressed.

```text
    // Projects `x` into the strict interior of the recorded bounds and seeds the
    // bound multipliers there. Per bounded variable the push away from a bound is
    // p = bound_push_ * max(1, |bound|), additionally capped at
    // bound_interval_push_ * (upper - lower) when the variable is two-sided
    // (Ipopt's kappa1/kappa2 rule); the lower push is applied before the upper,
    // and with bound_interval_push_ below one half the two can never cross. A
    // guess at or outside a bound is projected, never rejected. The multipliers
    // are then seeded at min(kBoundMultInitCap, mu0 / distance) -- after the
    // push, so the distance is safely interior. Runs once per solve, at entry,
    // after the reduced gather and before any evaluation.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2311–2326

`apply_bound_dual_step`: the clamp and the monotone/free-mu selection argument. Both kept, compressed.

```text
    // Commits the bound multipliers for an accepted iterate: z += alphad*dz,
    // then the kappa_sigma safeguard clamps each into
    // [mu_clip/(kKappaSigma*d), kKappaSigma*mu_clip/d] for the distance d
    // measured at the NEW x. Exactly one call per committed iterate; `xsl_new`
    // is the already-committed iterate.
    //
    // `monotone_mu` selects which barrier parameter the clamp is taken at,
    // transcribing Ipopt's correct_bound_multiplier: under a MONOTONE schedule
    // the clamp uses the barrier parameter itself (`mu`), and under a FREE-mu
    // schedule it uses the average complementarity at the new point, capped at
    // kFreeModeClipMuCap. The two differ because a free-mode barrier parameter
    // is an oracle's proposal for the NEXT step rather than a description of
    // where the iterate currently sits, and it is the latter the safeguard
    // needs. The classic_adaptive governor is the free-mu case, the monitored
    // governor reports its live mode, and a phase with no inequality
    // constraints runs no governor at all and so holds mu fixed.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2336–2346

`dual_infeasibility_inf`: the two forms and why `prim_base` is passed explicitly. Both kept, compressed.

```text
    // The dual infeasibility whose infinity norm is the solver's kkt_inf_. Off
    // the bound path this is exactly the base block's norm, as it always was;
    // with bounds it is that block plus the z-FORM terms (-z_L + z_U), built in
    // scratch. It is deliberately NOT accumulated into the RHS itself: the same
    // primal block is the condensed Newton right-hand side, which carries the
    // mu-form instead -- see the staging comments in alg_impl().
    //
    // `prim_base` is the BASE-form primal stationarity block (grad f + J'lambda)
    // for the point being measured, passed explicitly rather than read off a
    // KKTVector because the live RHS's own block is staged in the mu-form for
    // part of each iteration; a caller inside that bracket passes the snapshot.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2415–2425

`build_restoration_exit_measures`: the scale-incomparability argument. Kept, compressed.

```text
    // --- Feasibility-restoration exit measures (defined in interior_point_solver.cpp) ---
    // Shared by every restoration exit/teardown site (the two continuing-exit
    // arms, the in-loop locally-infeasible break, and the post-loop teardown).
    // While restoration is active, the loop's own prim_obj_ is φ_prox (the
    // proximal objective substituted by the eval seam) — never valid outside
    // restoration, since the OPTIMALITY filter/funnel's accumulated pairs are
    // all true-objective-scale (see the cross-phase pair-incomparability
    // disclosure in globalization/filter_acceptance.h). This helper re-evaluates the TRUE
    // objective once at the live primals so every exit site hands
    // notify_switch_to_optimality (and, ultimately, obj_val_) a measures
    // triple in the same scale as the filter/funnel it is augmenting into.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2430–2444

`enter_feasibility_restoration`: the entry sequence for both families. Kept, compressed.

```text
    // --- Feasibility-restoration lifecycle (defined in interior_point_solver.cpp) ---
    // Shared entry orchestration for the kSwitchToFeasibility case. Builds the
    // (θ,f) entry measures from the current RHS/primals, then dispatches on the
    // strategy family: the proximal switch takes enter_restoration; the nested
    // l1 phase takes enter_nested (with the current equality/inequality residual
    // vectors) and additionally stashes the outer μ, sets μ ← entry_mu(), resets
    // the governor for a fresh in-phase barrier schedule, and applies the
    // verified entry multiplier init (equality constraint multipliers ← 0; the
    // slack/bound multipliers clamped to min(ρ, current)). Both families then
    // notify the acceptance strategy of the switch and reset the recovery chain.
    // Passed the raw XSL/RHS blocks (KKTVector views are rebuilt inside) so it is
    // directly drivable from a friend test harness. `mu` is updated in place.
    // RHS is READ ONLY -- the entry measures its constraint block, seeds the
    // nested phase and the raw-residual scratch from it, and never writes it --
    // and since M6 W5 T2 the signature says so.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2460–2473

`leave_restoration`: the exit order and why it is load-bearing. Both kept, compressed.

```text
    // The restoration EXIT protocol, in the one order every exit site must use:
    // (optionally restore the stashed outer μ and reset the governor, which only
    // a nested phase ever needs), exit_restoration(), notify the acceptance
    // strategy of the switch back to optimality, reset the recovery chain.
    //
    // The order is load-bearing. exit_restoration() flips is_active() false, so
    // any μ/governor work that belongs to the phase must precede it.
    // notify_switch_to_optimality augments `measures` into the restored OPTIMALITY
    // filter/funnel, whose accumulated pairs are all true-objective-scale — so
    // callers build `measures` through build_restoration_exit_measures() rather
    // than passing the loop's own prim_obj (which is φ_prox/φ_l1 while active).
    // The recovery-chain reset runs last and exactly once per transition: the
    // watchdog's objective-scale-bound snapshot and counters must not survive back
    // into the optimality phase.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2476–2500

`exit_feasibility_restoration_nested`: the five-step sequence and the scope note on which multiplier families it reaches. Both kept, compressed.

```text
    // The nested phase's multiplier re-entry sequence — shared byte-for-byte by
    // the κ_resto ratchet exit and the near-feasible stall exit (Ipopt
    // MinC_1NrmRestorationPhase::PerformRestoration, strict order): (1) keep the
    // phase's final x/s; (2) slack-multiplier Newton complementarity step under
    // the STASHED outer μ, damped by the dual fraction-to-boundary rule; (3) if
    // max|z| over ALL inequality multipliers exceeds kBoundMultResetThreshold,
    // reset every inequality multiplier to 1; (4) equality constraint
    // multipliers ← 0; (5) restore the stashed outer μ, reset the governor,
    // exit_restoration, notify the acceptance strategy of the switch back to
    // optimality (with true-objective exit measures), reset the recovery chain.
    // `theta_orig` is the current original-problem infeasibility (∞-norm),
    // carried into the exit measures. `mu` is restored in place.
    //
    // SCOPE: steps (2) and (3) reach EVERY bound-multiplier family the solver
    // carries — the inequality (slack) multipliers and, when the problem
    // declares variable bounds, both sides of those — matching Ipopt's
    // PerformRestoration, which applies its ComputeBoundMultiplierStep to all
    // four of its families under ONE shared dual fraction-to-boundary damping
    // and takes its reset-threshold max over all four. The shared damping is
    // the detail worth naming: a per-family fraction is the plausible wrong
    // implementation, and it is what the exit's unit pin exists to catch.
    //
    // This is the second of the two events that write the bound multipliers,
    // and the only one that applies no dz and moves no x — see the two-event
    // note on bound_duals_ above.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2513–2520

`constraint_violation_l1`: the one-home argument and the read-only view. Both kept, compressed.

```text
    // ‖c‖₁ over a KKT vector's constraint block — the L1 constraint violation the
    // restoration entry guards, the proximal exit test and the stall detector all
    // measure. One home for the reduction (v.all_cons() is exactly the
    // tail(equal_cons_ + inequal_cons_) of either spelling).
    //
    // Takes the READ-ONLY view (M6 W5 T2): it reduces and never writes, and its
    // mutable parameter was why enter_feasibility_restoration held a mutable
    // RHS. A KKTVector converts implicitly, so no call site changes.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2531–2541

`try_recenter_elastics`: the trigger, the one-shot budget and the re-arm rule. All kept, compressed.

```text
    // Second-level elastic re-centering fallback for the nested l1 phase
    // (disclosure (f) in l1_restoration.h). Invoked by alg_impl's kAcceptAsIs case
    // when an in-phase line search exhausts the recovery ladder (a nested phase is
    // active and no recovery link resolved the rejection). Re-centers the elastic
    // pairs in closed form at the current phase μ from the raw residuals held in
    // resto_ec_/ic_scratch_ (this iteration's eval seam), INSTEAD of taking the
    // failed step. One-shot per consecutive-failure run: returns true and consumes
    // the resto_recentered_ budget on the first call; returns false (fall through
    // to accept-as-is) while the flag is still set. The flag re-arms on any
    // accepted step and at each phase entry. Reachable only with restoration_
    // non-null, active, and nested (the call site gates on nested_active).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2544–2561

`primal_dual_error`: the residual definition, the Ipopt mapping and the `prim_base` argument. The definition and the argument are kept.

```text
    // Primal-dual system error at barrier parameter `mu`: the ∞-norm of the full
    // KKT residual — primal stationarity (rhs.prim_grad, the Lagrangian gradient
    // as assembled for the current iterate), primal infeasibility (equality and
    // slack-completed inequality residuals), and the complementarity deviation
    // max|s·z − μ| — as one scalar. Maps Ipopt's primal_dual_system_error(μ)
    // (coin-or/Ipopt 72a29c9, src/Algorithm/IpBacktrackingLineSearch.cpp
    // TrySoftRestoStep) onto this solver's single unscaled max-norm KKT measure.
    // Read-only; the caller passes vectors already populated the same way the
    // main loop populates the current iterate's RHS (stationarity including the
    // objective/barrier gradient contribution, inequality residual slack-
    // completed). Used only by the nested soft feasibility pre-stage.
    //
    // The stationarity term is the z-FORM dual infeasibility, matching Ipopt's
    // error, which norms the undamped Lagrangian gradient at whichever point it
    // measures. `prim_base` carries that point's BASE primal block for the same
    // reason dual_infeasibility_inf takes one: the comparison this feeds comes
    // from two points, and both must be measured in the same form or the
    // reduction test acquires a direction.
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2565–2574

`try_soft_feasibility_step`: what it forms, what it compares and what each return means. All kept, compressed.

```text
    // Nested soft feasibility pre-stage trial (defined in interior_point_solver.cpp). Forms the
    // full fraction-to-boundary trial point XSL + DXSL (DXSL already carries the
    // fraction-to-boundary scaling from compute_step), evaluates the original
    // problem there (into the caller-supplied XSL2/RHS2/GX scratch), and returns
    // whether its primal-dual error is at most kSoftRestoPdErrorReductionFactor
    // times the current point's. A true return means the soft step is accepted
    // (alg_impl takes the full step and stays in the pre-stage); a false return
    // means alg_impl escalates to the full restoration switch. Dead on the
    // default path (only reached with a nested restoration strategy configured,
    // via the kSoftFeasibilityStep recovery action).
```

**SOURCE** 1997159 · include/hven/drivers/interior_point_solver.h · lines 2581–2589

`fill_residual_info`: the shared-formula argument and the two fields it deliberately does not set. Both kept, compressed.

```text
    // The residual formulas shared by the pre-factorization early
    // convergence check and the post-line-search fill_iter_info() call live here
    // ONCE, so neither call site can drift out of sync. fill_residual_info() sets
    // every IterateInfo field derivable from rhs/xsl alone (valid immediately after
    // eval + the barrier/complementarity block, before any factorization). It
    // deliberately does NOT set barr_obj_/mu_ (only settled once the barrier-
    // parameter update runs, later this iteration) or p_pivots_ (kkt_sol_.ppivs(),
    // which only reflects a real value once this iteration's factorization has
    // actually run).
```

### include/hven/core/solver_counters.h

2107 lines / 1690 comment lines at `1997159`; 1380 / 963 after.

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 6–9

The file banner. Kept verbatim in substance; only its wording was tightened.

```text
// The solver counter contract's types: QpCounters (one QP engine solve),
// SsnCounters (the semismooth-Newton kernel's own work), IpqpCounters (the
// IP-PMM interior-point tier's own work) and SqpCounters (one whole SQP
// driver solve, which aggregates all three).
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 20–49

`struct QpCounters`: the two EQP refinement counters' different baselines, and the evidence that eqp_refine_steps is identically zero. The definitional half and the "KEPT" rule are kept; the measurement is here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 57–84

`verdict_refine_steps`: the COUNTS/EXCLUDES account and the per-entry bound. Both kept; the "zero is the common reading" paragraph is here.

```text
    /// Refinement steps kept by the VERDICT-SITE FACE REFINEMENT (qp_engine.h
    /// section 5) across this solve. A THIRD, disjoint quantity, deliberately
    /// not folded into border_refine_steps: those two count the refinement
    /// every EQP solve pays, this one counts refinement bought at a
    /// would-be-kInfeasible dead end to decide a verdict.
    ///
    /// COUNTS: steps KEPT, in the same sense as the two fields above -- a
    /// candidate rejected by the strict-decrease safeguard is discarded and
    /// not counted, and a refinement whose result the engine then declines to
    /// adopt contributes nothing at all.
    ///
    /// EXCLUDES: every refinement step taken inside an EQP solve (those are
    /// border_refine_steps/eqp_refine_steps); and every dead end whose
    /// classification was going to be kOptimal anyway, which is why an ordinary
    /// solve reads 0 here. NOTHING IS EXCLUDED BY ALGEBRA (M6 W2 T7 and its fix
    /// round 1): both paths refine at a would-be-kInfeasible dead end, and
    /// which twin runs is decided by the candidate's provenance rather than by
    /// QpOptions::ws_algebra, so a border-mode solve served by a fallback or by
    /// the latch counts here too.
    ///
    /// THE PER-ENTRY BOUND IS THE READING THAT TRANSFERS, not any observed
    /// total: at most detail::kMaxVerdictRefineSteps steps are bought per dead
    /// end, and a whole-solve figure is only as large as the fixture's dead
    /// ends make it.
    ///
    /// ZERO IS THE OVERWHELMINGLY COMMON READING and a nonzero one says the
    /// walk was about to certify infeasibility and paid for a closer look --
    /// which the verdict then either overturned or confirmed.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 97–111

`k0_reused`: why it is not `factorizations == 0`. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 114–133

`symbolic_analyses`: the regression it exists to catch, with its measured figures. The expected readings are kept; the measurement is here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 136–191

The working-set walk counters' banner: why they exist, the net identity, the churn/degeneracy fractions, and the start-repair exception. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 220–229

`degenerate_steps`: the textbook reading and the no-anti-cycling statement. The threshold and the no-decision statement are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 232–253

`degenerate_run_max`: the two degeneracy pictures and their different remedies. The run-breaking rule and the warning about what a large value means are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 256–261

The six traceability fields' banner. Kept, compressed.

```text
    // THE SIX FIELDS THAT MAKE THE ABOVE TRACEABLE RATHER THAN INFERRED:
    // ws_adds and ws_drops MERGE inequality rows with variable bounds and
    // carry no constraint IDENTITY, so "the walk RE-DISCOVERS its rows"
    // cannot be concluded from ws_adds >> |W*| alone -- a walk touching many
    // DISTINCT bounds once each fits the same numbers. These six settle it
    // by direct measurement.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 272–282

`distinct_ineq_added`: the multiplicity formula and the off-by-one warning. Both kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 310–324

`struct SsnCounters`: the two scales and the kBare/kFull split. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 326–340

`ssn_iters`: the two reasons the invariant is an inequality. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 343–352

`ssn_bulk_flips`: the two failure pictures it distinguishes. The definition and the first-step rule are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 373–398

The tier-3 refinement pair's banner: the sum's meaning under both lever states, and what a refusal means. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 408–436

`ssn_refine_factorizations`/`ssn_refine_neg_duals`: why neither is answerable by arithmetic on the pair above, and the sign-measurement argument. The definitions and the no-tolerance rule are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 447–491

The sign sweep's banner: the leak it closes, where it runs, what it costs and its scope. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 504–556

The escape-reason census: why a count per reason, the partition invariant, the driver's own refusal bucket and the CSV sentinel warning. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 565–577

The field tables' banner. Kept, compressed.

```text
// ===========================================================================
// THE FIELD TABLES (M6 W4 T2(d)) -- one X-macro per aggregate
// ===========================================================================
// The trace's `sqp.solve.end` counters object is GENERATED from these, in table
// order, so the JSON's key order is the declaration order and a field added to
// a struct without a table entry does not silently vanish from the stream.
//
// EACH TABLE IS PINNED TO ITS STRUCT by an aggregate-ARITY static_assert (plan
// amendment E): the largest N for which the struct is brace-initializable with
// N arguments. Not sizeof, which is padding-dependent.
//
// TO ADD A COUNTER: declare it in the struct, add its `X(name)` here IN THE
// SAME POSITION, and the assert passes again. Nothing else needs editing.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 599–612

The absence column: the per-field rule and the sweep's three findings. Both kept, compressed.

```text
// --- the tables' second column: ABSENCE ----------------------------------
// Plan section 2 rule 5 -- absence is `null`, never a value -- needs a per-FIELD
// answer: a counter's own doc says whether a reading is a value or a sentinel.
//
// These predicates are that answer, carried IN the table so the serializer has
// no special cases.
//
// THE SWEEP (fix round 1, R7) found three: `ipqp_alpha_p_min` and
// `ipqp_alpha_d_min`, whose `+infinity` default is documented "no step observed
// yet", and `ipqp_tier_retired_after`, whose `0` is "never retired".
//
// `ipqp_final_inertia_read`'s `3` is NOT one: its doc keeps `3` distinct from
// `2` as a categorical outcome, and `0` is structurally reported where no read
// was due, so `null` would erase a distinction the census depends on.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 629–634

`counters_offsets_increase`: what the count assert cannot see. Kept, compressed.

```text
/// @brief Are the offsets strictly increasing across a table's entries?
///
/// THE COUNT ASSERT CANNOT SEE A REORDER (fix round 1, R8(b)): a field INSERTED
/// mid-struct whose `X()` entry is appended at the tail keeps the count right
/// and silently emits a JSON key order that is no longer declaration order.
/// Walking `offsetof` closes that direction at compile time.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 666–695

`struct IpqpCounters`: the two scales, the DNF sentinel and the fold rule with its four exceptions. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 718–729

`ipqp_solves`: the exact-2 rule and the gap identity. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 732–748

`ipqp_pattern_verifies`: the one-of-{verify, analyze} contract. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 751–765

`ipqp_rho_demanded_max`: the fold, the exclusions and the high-water-mark caveat. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 768–783

`ipqp_rho_demanded_last`: why OVERWRITE is the normative fold. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 786–800

`ipqp_inertia_retries`: the two rejection classes and the ceiling-terminated rung. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 803–814

`ipqp_iters_at_elevated_rho`: the measurement point and the exclusion. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 817–869

`ipqp_ladder_reclimbs`: the two routes to the retried value, the expected-cost argument, the field it replaced, and the perturbed-driven exclusion. All kept except the replaced field's history.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 872–892

`ipqp_pivot_reroute_primal`: why the re-route exists, with its worked example. The mechanism and the exclusions are kept; the example is here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 895–913

`ipqp_pivot_reroute_dual_fallback`: the provenance-approximation argument. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 916–936

`ipqp_iters_ladder_armed_no_advance`: the negative-curvature walk it instruments. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 939–994

`ipqp_final_inertia_read`: the four values, their mapping onto the escape census, and the caveat on `0`. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 997–1016

`ipqp_reg_decreases`: the identity and what the remaining "nothing moved" class is. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1044–1054

`ipqp_mu_adopted`: the clamp, the summed-flag convention and the not-the-trajectory caveat. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1073–1103

`ipqp_tier_retired_after`: the marker-not-a-count argument, the max fold and the driver-scale note. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1156–1187

`ipqp_to_walk`: the routing partition and why `ipqp_to_ssn` cannot stand in it. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1195–1208

The five-way escape census banner, including the dropped stall sub-counters. The partition and the stall field's scope are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1227–1240

`ipqp_escape_indefinite`: the reading, its mapping and its exclusions. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1243–1256

`ipqp_escape_numerical`: the complement statement. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1267–1277

`ipqp_alpha_p_min`: the +infinity default's argument. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1287–1296

`ipqp_read_kept_tight_sides` and `ipqp_read_barrier_noise_sides`: the two field notes, with their report references dropped.

```text
    /// T4c: KEPT bound sides in the item 4 read's disclosure band. Additive
    /// fold. U0 corpus 0/0 is EXPECTED (near-equality-constrained first
    /// QPs), meaningful only on activity-taxonomy/path-bound cells. See
    /// `.superpowers/w1-t4c-report.md`.
    Index ipqp_read_kept_tight_sides = 0;

    /// T4c: band-counted sides `ipqp_classify_barrier_noise` (ipqp_math.h)
    /// classifies `kSuspect`; `0` on the band-only fallback. Additive fold;
    /// same U0 corpus caveat as `ipqp_read_kept_tight_sides` above. See
    /// `.superpowers/w1-t4c-report.md`.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1363–1555

`struct SqpCounters`: the whole 193-line banner — major_iters' meaning, the two history shapes, the terminal-restoration caveat with its measured figures, the SOC fold, rejected_steps/steps_accepted and the eval_hess accounting, the false identity, the SOC outcome partition, the elastic counters and the restoration fold. Every contract is kept in the rewritten banner; the derivations and the measured figures are here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1570–1584

The certified fallback's partition banner. Kept, compressed.

```text
    // THE CERTIFIED FALLBACK'S PARTITION (M6 W2 T5, plan amendment G). The counters below are
    // written by `certified_feasibility_fallback` -- the ONE judge, and the only
    // site that writes any of them. Their block discipline is the five-way escape census's
    // (:1062-1072), restated for this partition over ENTRIES:
    //
    //     ipqp_suspicion_disproved + <relaxed> + <exhausted> + ipqp_fallback_rung_b
    //         == the fallback entries whose evidence block FIRED
    //         == elastic_from_ipqp_escape - elastic_floor_retries
    // where <relaxed> and <exhausted> are the rung-A-owned outcomes that carry no counter of
    // their own -- they are read off the returned `qp_status` (kOptimal vs the synthesized
    // kInfeasible), which is why plan section 5 states the partition "with qp_status alongside".
    //
    // AN ENTRY WHOSE BLOCK NEVER FIRED CHARGES NONE OF THEM: it is W1's single cold walk, with
    // no rung A entered and no activation charged, so it is outside the partition by
    // construction rather than by arithmetic.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1586–1599

`elastic_from_ipqp_escape`: the two-term split. Kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1621–1635

`elastic_rho0_ceiling_hits`: the two caps, and the measured clamp frequencies on two different fixture sets — a measurement at the code site. The caps are kept; the figures are here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1648–1658

`eqp_refine_steps`/`border_refine_steps` at solve scale: the aggregation set and the inherited meanings. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1667–1684

`suspect_escalations` at solve scale: the expected reading and the exhaustion diagnostic. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1687–1706

`symbolic_analyses` at solve scale: what it does not see, with its measured example. The under-report and the regression signal are kept.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1709–1743

`start_level_used`: the resolution rule level by level, and why `k0_reused` is read rather than a factorization count. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1746–1757

`full_step_majors`: what it counts and the not-unit-steps caveat. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1760–1773

`watchdog_restores`: the binary reading and the no-op restore. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1776–1837

The eval-economics banner: the partition, what each side counts, the exception, and the warning that the fields are not a self-verifying trace (with the hand-run mutation that established it). Every rule is kept; the mutation account is here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1841–1872

`probe_budget_stops`: what it counts and the three cases it does not. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1875–1904

`crash_seeded_rows`/`crash_seeded_bounds`: why the halves are separate (with their measured repeat rates), the seeds-offered rule, and the restoration fold. The rules are kept; the rates are here.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1908–1918

`n_seeded`: the summed-flag convention and what it is not redundant with. Both kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1921–1941

`seeded_clamped`: the scope, the ordering against the geometric clear, and what neither field counts. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1944–1979

`ip_activity_inferred`: write-only, the kSeeded scope, the naming note, the rows-only decision and the seeds-offered rule. All kept, compressed.

```text
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
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 1982–1995

`active_set_delta_total`: why it is not folded from the restoration sub-solve. Kept, compressed.

```text
    /// Sum of `SqpIterate::active_set_delta` over this solve's REPORTING
    /// majors -- the total working-set churn the QP sequence went through,
    /// counting the first major's census against the empty set (M6 W4 T3).
    ///
    /// INSTRUMENTATION ONLY: nothing in this library reads it.
    ///
    /// NOT FOLDED FROM THE RESTORATION SUB-SOLVE, and that is the
    /// `steps_accepted` / `rejected_steps` argument rather than the
    /// `qp_minor_iters` one: an active-set delta is a statement about WHICH
    /// constraints of THIS problem were in the working set, and the sub-solve's
    /// QP lives in the feasibility wrapper's own variables and rows. Summing
    /// the two would report a churn over a set that never existed. The
    /// sub-solve's own value is on its own `SqpSolution` and in the trace at
    /// depth 1.
```

**SOURCE** 1997159 · include/hven/core/solver_counters.h · lines 2021–2036

`SqpCounters::ipqp`: the aggregation, the structural zero at kWalk and the two driver-scale routing fields. All kept, compressed.

```text
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
```

### include/hven/detail/qp/ssn_engine.h

1782 lines / 1451 comment lines at `1997159`; 1100 / 769 after.

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 6–391

The whole file banner: the residual and its FB reformulation, the generalized Jacobian and why the assembled matrix is symmetric, the fixed sparsity pattern, the activity hint as one PDAS step, the tolerance and what it certifies, the per-solve seam, the safeguarded iteration's ten steps, the certifying exit and its cost, and the references. The rewritten `@file` block keeps what a caller needs -- the two modes, the certification rule under kFull, the fixture scope and the clean-room provenance -- and every constant's own block below keeps the derivation it is responsible for.

```text
// ssn_engine.h -- THE SEMISMOOTH-NEWTON QP KERNEL.
//
// A Newton method on a Fischer-Burmeister (FB) reformulation of the QP's own
// KKT conditions, solving the SAME subproblem qp_engine.h's primal active-set
// WALK solves (qp_problem.h's QpProblem, same sign convention, same
// regularization knobs), by a different mechanism: every step changes the
// WHOLE implied active set at once, where the walk changes one working-set
// member per minor iteration. The walk's cost at scale is in the NUMBER of
// minor iterations, so a method whose iteration count does not grow with |W*|
// is the only kind of change that can move that figure.
//
// SCOPE -- READ THIS BEFORE JUDGING A DIVERGENCE. THIS ENGINE HAS TWO MODES:
//
//   SsnSafeguards::kBare -- the LOCAL METHOD. Full, undamped Newton steps;
//     no line search, no merit function, no proximal term, no uncertain
//     set, no dual projection, no inertia gate. Locally superlinearly
//     convergent UNDER BD-REGULARITY AT THE SOLUTION, which a WEAKLY ACTIVE
//     row (lambda* = 0 AND c* = 0) can violate. GLOBALLY nothing at all --
//     it can cycle, and it can certify a SADDLE POINT as kOptimal. A
//     POSITIVE CONTROL, not a product surface; every safeguard is scored
//     against it.
//
//   SsnSafeguards::kFull -- the production iteration, and the default.
//     Section 7 below is its specification; 7b is the contract that makes
//     the saddle claim above a claim about kBare ALONE: under kFull no
//     kOptimal is issued anywhere -- cold start, warm hand-off, or a seed
//     placed exactly on the saddle -- without an inertia verdict read AT THE
//     CERTIFIED POINT.
//
// DRIVER WIRING: sqp_driver.h dispatches its MAIN subproblem call on
// SqpOptions::qp_mode, constructing an SsnEngine (kFull) under QpMode::kSsn.
// The SOC re-solve, elastic ladder rungs and restoration sub-solve stay on
// the WALK unconditionally regardless of qp_mode (each is a rescue path
// hot-started off the immediately preceding walk solve). With
// `qp_mode = kWalk` every existing test, pin and battery remains
// byte-identical.
//
// The only convergence claims either mode makes are on the ANALYTIC FIXTURES,
// all of them at most six variables. The corpus gates score the safeguarded
// engine; nothing here should be quoted against them.
//
// 1. THE RESIDUAL.
//
// The QP (qp_problem.h) is
//
//     min  g^T x + 1/2 x^T H x   s.t.  Ae x = be,  Ai x <= bi,  l <= x <= u
//
// with the project's KKT sign convention
//
//     grad(f) + Ae^T lambda_e + Ai^T lambda_i - z = 0,   lambda_i >= 0
//
// where grad(f) = Hx + g and z is the SIGNED bound multiplier (>= 0 at an
// active lower bound, <= 0 at an active upper bound).
//
// BOUNDS ARE ROWS -- each finite bound becomes a one-sided inequality row
// with its own NON-NEGATIVE multiplier, exactly like a row of Ai (lower:
// l_j - x_j <= 0, gradient -e_j; upper: x_j - u_j <= 0, gradient +e_j) --
// and the signed z the rest of the project speaks is recovered as
// z_j = lambda^lo_j - lambda^up_j. A bound at or beyond detail::kSsnInfBound
// in magnitude is ABSENT and contributes no row at all.
//
// Writing s for the SLACK of an inequality-shaped row (s = bi_k - (Ai x)_k
// for a row of Ai, s = x_j - l_j / u_j - x_j for a bound row), the KKT
// conditions per row are s >= 0, lambda >= 0, s * lambda = 0, which the
// Fischer-Burmeister function phi(a, b) = a + b - sqrt(a^2 + b^2) encodes
// exactly: phi(a, b) = 0 <=> a >= 0, b >= 0, a*b = 0. The whole KKT system
// becomes ONE nonlinear equation F(w) = 0 in
// w = (x, lambda_e, lambda_i, lambda_bound):
//
//     F_x  = H x + g + Ae^T lambda_e + Ai^T lambda_i + B^T lambda_b   (n rows)
//     F_e  = Ae x - be                                               (me rows)
//     F_i  = phi(s_k, lambda_i_k)          per row of Ai             (mi rows)
//     F_b  = phi(s_r, lambda_b_r)          per finite bound          (mb rows)
//
// where B is the bound-row matrix (one +/-1 per row). Equalities enter
// LINEARLY -- no FB pair, nothing to select -- so on a QP with no
// inequalities and no finite bounds F is AFFINE and one Newton step is the
// exact solve of the regularized system.
//
// "ONE STEP" IS A STATEMENT ABOUT F, NOT A PROMISE OF ONE FACTORIZATION,
// and the gap is delta/mu: the step solves K = [H + delta I, Ae^T; Ae,
// -mu I], not the unregularized KKT matrix, so it lands
// O(delta*||x*|| + mu*||lambda*||) away from the true solution, and that
// residual SCALES WITH THE ITERATE -- an equality-heavy subproblem
// warm-started far from its solution is NOT a one-factorization solve.
//
// 2. THE GENERALIZED JACOBIAN, AND WHY THE ASSEMBLED MATRIX IS SYMMETRIC.
//
// phi is not differentiable at the origin but IS semismooth everywhere, with
//
//     rho    = sqrt(s^2 + lambda^2)
//     alpha  = d phi / d s      = 1 - s / rho        (in [0, 2])
//     beta   = d phi / d lambda = 1 - lambda / rho   (in [0, 2])
//
// away from s = lambda = 0, where any (1 - xi, 1 - eta) with xi^2 + eta^2 <= 1
// is an element of the C-subdifferential and select_branch selects
// xi = eta = 1/sqrt(2), i.e. alpha = beta = detail::kSsnDegenerateFbDeriv.
//
// alpha AND beta CAN NEVER BOTH BE SMALL: alpha + beta = 2 - (s+lambda)/rho
// >= 2 - sqrt(2) ~ 0.586 for every (s, lambda). alpha = 0 means exactly
// "lambda = 0 and s > 0" (a strictly INACTIVE row) and beta = 0 means
// exactly "s = 0 and lambda > 0" (a strictly ACTIVE one), so the partition
//
//     row k is ACTIVE  <=>  alpha_k > beta_k  <=>  lambda_k > s_k
//
// is read off the Jacobian itself rather than maintained as separate state
// -- it IS the primal-dual active set of PDAS, whose equivalence to
// semismooth Newton is the design's whole point.
//
// Since s_k = b_k - a_k^T x the FB row is
// -alpha_k (a_k^T dx) + beta_k d lambda_k = -phi_k, which is NOT symmetric
// against the stationarity block's a_k column. Multiplying through by
// (-1 / alpha_k) gives
//
//     a_k^T dx - (beta_k / alpha_k) d lambda_k = phi_k / alpha_k
//
// whose off-diagonal is EXACTLY the constraint gradient a_k, so the assembled
// matrix is the project's ordinary symmetric KKT shape with a PER-ROW
// negative diagonal in place of the walk's uniform -mu:
//
//     K = [ H + delta I   Ae^T    Ai^T    B^T   ]
//         [ Ae           -mu I     0       0    ]
//         [ Ai             0     -D_i      0    ]
//         [ B              0       0     -D_b   ]
//
//     D_k = beta_k / alpha_k + mu   (mu = QpOptions::dual_mu, delta =
//                                    QpOptions::primal_delta -- the SAME two
//                                    regularizers kkt_assembly.h applies)
//
// stored as the UPPER TRIANGLE (SpMatRM) and factored by the SAME sparse
// factor (MKL Pardiso / Apple Accelerate) the walk uses.
//
// D IS LARGE WHERE THE ROW IS INACTIVE AND ~mu WHERE IT IS ACTIVE, which is
// why the division is by alpha and not by beta: alpha -> 0 sends the diagonal
// to +infinity (a decoupled, diagonally dominant row -- numerically benign),
// while beta -> 0 sends it to mu (the ordinary active-constraint KKT row the
// walk already factors every minor).
//
// THE alpha FLOOR. alpha is EXACTLY zero on a strictly inactive row, so the
// division carries alpha_f = max(alpha, detail::kSsnAlphaFloor). It
// introduces no active/inactive decision; detail::kSsnAlphaFloor derives why.
//
// 3. THE FIXED SPARSITY PATTERN -- THE LOAD-BEARING PROPERTY.
//
// K's PATTERN depends on (H, Ae, Ai)'s patterns and on WHICH BOUNDS ARE
// FINITE, and on nothing else -- not the active set, the iterate, or which
// branch any FB row is in (a branch change moves the VALUE of one diagonal
// entry and nothing else). Concretely:
//
//   * every row of K at index >= n holds EXACTLY ONE stored entry, its own
//     diagonal (all the constraint coupling lives in the upper triangle, in
//     the x-rows), so that entry's position in the compressed value array is
//     K.outerIndexPtr()[row] with no bookkeeping;
//   * the diagonal slots are built with a nonzero PLACEHOLDER so they exist
//     structurally regardless of what value a branch later wants there
//     (an exact-zero triplet is not a slot one may rely on);
//   * consequently ONE Newton iteration costs one numeric factorization and
//     ZERO symbolic analyses, and a SECOND QP of identical structure costs
//     zero symbolic analyses too -- the analysis decision sees the same
//     hash and factorize_checked() skips the symbolic analysis (kkt_calls.h).
//
// SsnResult::symbolic_analyses reports this directly, counted at the call
// site before factorize(), because factorize() decides internally and does
// not report back.
//
// 4. THE ACTIVITY HINT = ONE PDAS STEP.
//
// SsnStart::activity_hint, when supplied, replaces the FB branch selection ON
// THE FIRST NEWTON STEP ONLY:
//
//     hinted ACTIVE    (alpha, beta) = (1, 0), row residual s_k
//                      => a_k^T (x + dx) = b_k   -- drive the slack to zero
//     hinted INACTIVE  (alpha, beta) = (alpha_floor, 1), row residual lambda_k
//                      => d lambda_k = -lambda_k -- drive the multiplier to zero
//
// which is EXACTLY one primal-dual active-set (equality-constrained KKT)
// solve on the hinted set, up to delta/mu and the O(1e-12) floor coupling.
// Every step after the first uses the FB branch. This is what makes a
// correctly hinted, correctly seeded QP converge in ONE iteration, and it
// is the seam the driver's warm-start activity hands through.
//
// "DRIVE THE SLACK TO ZERO" IS EXACT ONLY AT dual_mu = 0: beta = 0 leaves the
// hinted-active row's diagonal at -mu, so the landing slack is
// s_k^+ = -mu * d lambda_k -- exactly zero when mu = 0, O(mu * |d lambda|)
// under the shipped default. Never use an exactly-zero slack as a premise.
//
// 5. THE TOLERANCE, AND WHAT IT CERTIFIES.
//
// Convergence is ||F(w)||_inf <= SsnOptions::fb_tol, measured on the
// UNSCALED residual of section 1 (never on the row-scaled linear system's
// right-hand side). For s, lambda >= 0 one has the two-sided bound
//
//     (2 - sqrt(2)) * min(s, lambda)  <=  phi(s, lambda)  <=  min(s, lambda)
//
// (both tight), so ||F||_inf <= fb_tol gives
//
//     ||grad L||_inf <= fb_tol,  ||Ae x - be||_inf <= fb_tol,
//     min(s_k, lambda_k) <= fb_tol / (2 - sqrt(2)) = 1.7071 * fb_tol
//
// per row -- AND THAT LAST LINE CARRIES ITS HYPOTHESIS WITH IT: the
// two-sided bound is derived for s, lambda >= 0, which an inexact exit point
// need not satisfy (|phi| <= fb_tol permits either component to be negative
// by O(fb_tol)). The honest reading: every row with a NON-NEGATIVE pair
// satisfies min(s_k, lambda_k) <= 1.7071 * fb_tol, and any row that does not
// has both components within O(fb_tol) of the non-negative orthant. A caller
// needing a strictly feasible point must project.
//
// THE DEFAULT: fb_tol = kkt_tol, which buys stationarity and equality
// feasibility at exactly kkt_tol and complementarity within
// detail::kSsnComplementarityFactor (~1.71) of it.
// ssn_fb_tol_from_kkt_tol() is that derivation in code.
//
// 6. THE PER-SOLVE SEAM: TRUST REGION AND REGULARIZERS.
//
// solve() has an overload taking qp_types.h's SolveOverrides -- THE WALK'S
// OWN struct, not a parallel one, because the funnel driver already builds
// one per subproblem and per SOC re-solve. Sentinels and resolution are
// qp_types.h's rules unchanged.
//
// THE TRUST REGION IS A BOX, AND THAT IS THE WHOLE DESIGN. lo_eff =
// max(lower, x0 - Delta), up_eff = min(upper, x0 + Delta), about this solve's
// own start point, resolved once. Because it is a box it changes only the
// bound rows' VALUES -- never which rows exist, once the radius is finite at
// all -- so K's sparsity pattern is INVARIANT ACROSS RADIUS CHANGES (a
// shrink-retry loop re-solves at zero pattern cost and zero symbolic
// analyses; detail::SsnBoundRow states the key's exclusion at the field), and
// Delta = +inf reproduces the no-trust-region solve BIT FOR BIT.
//
// TR PINS ARE NOT BOUNDS, AND THE EXPORT KEEPS THEM APART:
//
//   tr_active[j]     true iff a TR-tight row is implied active at j
//   bound_state[j]   REAL bounds only -- a TR-pinned variable reads kFree
//   z(j)             REAL bounds only -- a TR-pinned row's dual is dropped
//   start.z          priced against REAL bounds; TR rows seed at zero
//
// This is qp_problem.h's QpSolution::tr_active contract verbatim: a driver
// detects a binding radius by reading tr_active, never z or bound_state.
//
// AND THE RADIUS IS A SOFT CONSTRAINT: ON AN ESCAPED EXIT THE RETURNED x
// MAY LIE FAR OUTSIDE IT -- the TR is bound ROWS and an FB row holds only at
// a root. A certifying exit respects the box to O(1.71 * fb_tol) and is the
// ONLY exit whose x may be used as a step. SsnResult::tr_violation carries
// the full contract.
//
// THE REGULARIZERS RESOLVE TOO, BUT THE DRIVER'S ADAPTIVE-mu SCHEDULE BUYS
// NOTHING HERE and is switched off deliberately: delta and mu perturb ONLY
// the Jacobian and never the residual, which residual() computes
// unregularized. The iteration is therefore modified Newton on the EXACT F --
// fixed points exact, unregularized KKT points -- so (delta, mu) cost
// ITERATIONS rather than ACCURACY, the opposite trade from the walk.
//
// 7. THE SAFEGUARDED ITERATION.
//
// ONE ATTEMPT, in order. Everything marked [G] is skipped entirely under
// SsnSafeguards::kBare, which is what keeps bare mode the bare local method
// rather than a re-implementation of it.
//
//   1. Evaluate F at the iterate. TWO norms come out of the one walk: ||F||inf,
//      the CERTIFICATE of section 5, and 1/2||F||_2^2, the LINE SEARCH's
//      merit. Different on purpose -- detail::kSsnArmijoSigma says why.
//   2. Converged? ||F||inf <= fb_tol. [G] A CERTIFYING EXIT IS NOT FREE: it
//      runs the SECOND-ORDER VERIFICATION first (section 7b), which is the one
//      place the safeguarded iteration pays a factorization the bare one does
//      not.
//   3. [G] DIVERGENCE TELEMETRY, and it runs BEFORE the budget test. An
//      infeasible QP that also runs out of budget must report the DIAGNOSIS,
//      because kBudget is a value a caller answers by raising the budget, which
//      on an infeasible QP is exactly the wrong move. Criteria and their
//      derivation: detail::kSsnStallWindow.
//   4. Out of budget? hard_budget attempts -> kBudget.
//   5. [G] SOFT BUDGET: crossing it arms the proximal term. That is the whole of
//      "soft_budget warns", and the warning is SsnCounters::ssn_prox_updates
//      leaving zero rather than anything printed.
//   6. CLASSIFY every FB row. [G] adds the third set: see select_branch for the
//      band, the hysteresis, the tie policy and why the damped element is still
//      a generalized Jacobian element.
//   7. Refresh the FB diagonals, build the right-hand side, factorize.
//   8. [G] INERTIA GATE. K's inertia is (n, me+mi+mb) IFF the primal block is
//      positive definite -- an identity, not a hope, PROVIDED delta+sigma > 0
//      -- so the gate is provably inert on any convex subproblem under that
//      hypothesis. kWrong escalates the proximal ladder and retries the SAME
//      iterate; kSuspect does NOT act, matching qp_engine.h.
//   9. [G] LINE SEARCH: Armijo on the merit, backtracking by
//      detail::kSsnBacktrackFactor to the floor detail::kSsnMinStep, over
//      trial points that carry the DUAL PROJECTION. Iteration 0 is exempt when
//      a HINT governed it -- see the exemption's own block at the call site.
//  10. Accepted -> apply. Not accepted -> the escape ladder: the infeasibility
//      DIAGNOSIS outranks the repair, then a proximal rung, then
//      kNoContraction.
//
// FOUR OF THESE STEPS HAVE AN OPT-IN ALTERNATIVE, each shipping at the value
// that reproduces the list above BIT FOR BIT; none is a default. Each field
// carries its own contract:
//
//   step 2/7b -- SsnOptions::defer_certification
//   step 5/10 -- SsnOptions::sigma_rule
//   step 9    -- SsnOptions::hint_rule + watchdog_q
//   step 3    -- SsnOptions::infeasibility_rule
//
// WHAT THE PROXIMAL TERM IS, PRECISELY. sigma is added to the SAME two slots
// primal_delta and dual_mu already occupy, anchored at the CURRENT iterate,
// so it perturbs only the Jacobian and section 6's property survives
// verbatim. It is NOT FBstab's outer proximal loop with a lagging centre --
// see detail::kSsnProxInit for the ladder and SsnStart::prox_center_x for the
// anchor ruling.
//
// ATTEMPTS ARE NOT STEPS. An attempt can pay its factorization and then
// take no step (a rejected line search, a wrong inertia, or the second-order
// verification of 7b), so bare mode's "factorizations == iters" is now
// iters <= factorizations, with equality UNREACHABLE under kFull (7b's
// verification adds one) and holding under kBare exactly when nothing was
// rejected. SsnResult::factorizations documents it at the field.
//
// 7b. THE CERTIFYING EXIT, AND WHY IT COSTS A FACTORIZATION.
//
// NO kOptimal IS ISSUED UNDER kFull WITHOUT AN INERTIA VERDICT READ AT THE
// POINT BEING CERTIFIED. A solve seeded at (or within fb_tol of) a stationary
// point that only ran the convergence test would return kOptimal having gated
// nothing -- including at a SADDLE, reachable by exactly the warm hand-off
// the driver builds.
//
// So the convergence test opens a VERIFICATION ATTEMPT instead of exiting:
// classify the rows at the converged point, refresh K's diagonals, factorize,
// read the inertia -- and only then certify. It takes no step, forms no
// right-hand side and runs no triangular solve; it pays ONE numeric
// factorization on the cached pattern and moves no counter but
// SsnResult::factorizations.
//
//   verdict kOk       -> QpStatus::kOptimal. The primal block is positive
//                        definite at the certified point, which on the active
//                        face is the QP's own second-order condition.
//   verdict kSuspect  -> QpStatus::kOptimal, and this is the ONE residual hole
//                        (see the Accelerate note below). kSuspect does not
//                        act, matching qp_engine.h's ruled policy exactly.
//   verdict kWrong    -> SsnEscape::kIndefinite. NOT an escalation: sigma
//                        perturbs the Jacobian only, so at a point where F is
//                        already zero the step stays zero at every rung and a
//                        ladder climb could only spend factorizations on its
//                        way to the same answer.
//
// THE VERDICT IS READ AT THE CALLER'S OWN REGULARIZATION. If the proximal
// ladder is armed when the solve converges, sigma is dropped to 0 for the
// verification and restored for reporting: In(K) = (n, m) iff
// H + (delta+sigma) I + A^T D^{-1} A is positive definite, so at a large
// sigma the test is about H + sigma I and says nothing about the QP the
// caller handed in. An earlier attempt's verdict is not reused for the same
// reason: D is a function of the ITERATE.
//
// THE COST IS ONE FACTORIZATION PER CERTIFIED SOLVE -- a real product cost: a
// one-iteration warm hand-off goes from one factorization to two, and the
// zero-step hand-off from zero to one. THE GATE IS INERT ON A CONVEX
// SUBPROBLEM ONLY WHILE delta+sigma > 0: SolveOverrides::primal_delta = 0.0
// with dual_mu = 0.0 is legal and non-sentinel, and under it an inactive FB
// row's diagonal is exactly 0, so the identity's hypothesis fails. With the
// hypothesis held the verdict is kOk and the verification can never turn a
// good answer into an escape.
//
// ACCELERATE COROLLARY. The gate reads the factorization evidence's
// perturbed-pivot count, which means PERTURBED pivots on MKL and ZERO
// pivots on Accelerate, and the verdict tests it FIRST. An Accelerate
// factorization of an indefinite K that reports a zero pivot therefore
// lands on kSuspect, which does not act -- so the verification CERTIFIES
// THE SADDLE there, in the cold case as well as the seeded one. "Degrades
// toward not firing" is the safe direction for a REPAIR trigger and the
// UNSAFE one for a certificate.
//
// 8. REFERENCES (public mathematics, clean-room).
//
// The PDAS/semismooth-Newton equivalence is Hintermueller, Ito & Kunisch,
// "The primal-dual active set strategy as a semismooth Newton method", SIAM J.
// Optim. 13(3), 2003 -- which is also where the M-matrix hypothesis under which
// PLAIN PDAS converges comes from. The FB residual formulation of a QP's KKT
// system follows Liao-McPherson & Kolmanovsky, "FBstab: A proximally stabilized
// semismooth algorithm for convex quadratic programming", Automatica 113, 2020.
// The three-set partition scaffolds on Curtis, Han & Robinson, "A globally
// convergent primal-dual active-set framework for large-scale convex quadratic
// optimization", Comput. Optim. Appl. 60(2), 2015; the n <= 2 converges /
// n = 3 cycles boundary for the min-map form is Ben Gharbia & Gilbert,
// "Nonconvergence of the plain Newton-min algorithm for linear complementarity
// problems with a P-matrix", Math. Program. 134, 2012. The merit 1/2||Phi||^2
// and its Armijo form are De Luca, Facchinei & Kanzow, "A semismooth equation
// approach to the solution of nonlinear complementarity problems", Math.
// Program. 75, 1996. NO PAPER'S NUMERICAL EXAMPLE IS REPRODUCED HERE -- the
// cycling fixtures were found by search over the failing family, since none
// of the published instances is an instance for THIS (FB, not min-map)
// iteration. Published mathematics only -- no third-party source was read.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 425–436

`kSsnAlphaFloor`: the cancellation argument and the 1e-12 sizing. The cancellation is kept; the sizing is here.

```text
// Floor applied to alpha = d phi / d s before it is divided by (banner
// section 2). alpha_f appears in BOTH the FB diagonal and the right-hand
// side, so it CANCELS in the recovered multiplier step:
//
//     d lambda_k ~ -(r_k / alpha_f) / (beta_k / alpha_f + mu)
//                = -r_k / (beta_k + mu * alpha_f)  ->  -r_k / beta_k
//
// and what it DOES perturb is the dx-coupling of rows whose true coupling is
// numerically zero. 1e-12 keeps mu * alpha_f (1e-8 * 1e-12 = 1e-20)
// negligible against the beta ~ 1 that always accompanies a floored alpha,
// while the resulting diagonal (beta / alpha_f, at most 2e12) stays far from
// any overflow or conditioning cliff.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 457–472

`ssn_fb`: the IEEE argument for the cancellation-free form, with its worked far-slack example. The rule and the regime split are kept.

```text
// phi(a, b) = a + b - sqrt(a^2 + b^2), evaluated in the CANCELLATION-FREE
// form 2ab / (a + b + rho) whenever a + b > 0. The two are algebraically
// identical -- multiply by (a+b+rho)/(a+b+rho) and use (a+b)^2 - rho^2 = 2ab
// -- but not numerically: the naive form has an absolute error floor of
// ~ulp(rho)/2, because fl(sqrt(fl(s*s))) == s exactly in IEEE double and
// fl(s + lambda) == s for any |lambda| < ulp(s)/2, so a FAR-SLACK row
// (s = 1e6, lambda = 1e-14, true phi ~1e-14) evaluates to EXACTLY 0. That
// under-reports rather than adding noise, so it cannot stall the iteration,
// but it puts an additive ulp(s_row)/2 slop on the exit CERTIFICATE and the
// merit 1/2||F||^2 inherits the same quantization.
//
// The stable form removes that floor for one extra multiply, and is used
// only when a + b > 0 -- the same-sign regime where the cancellation lives.
// When a + b <= 0 the naive expression sums two non-positive terms and
// cannot cancel, and it also covers a = b = 0 (where the stable form's
// denominator vanishes).
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 485–506

The uncertain band: the geometry, both constants and the hysteresis. All kept, compressed.

```text
// ---- THE UNCERTAIN BAND (the three-set partition's only tolerance) --------
//
// THE MARGIN IS MEASURED ON THE FB DERIVATIVE PAIR ITSELF, which is what
// makes it dimensionless and scale-free, so no second tolerance is needed.
// Writing (s, lambda) = rho (cos theta, sin theta),
// alpha - beta = (lambda - s) / rho = sqrt(2) * sin(theta - pi/4): 0 exactly
// on the kink ray s = lambda (where the classification is undecidable), 1 at
// either PURE state. The partition rule alpha > beta is the SIGN of this
// quantity; the uncertain set is the band around its zero.
//
// kSsnUncertainEnter = 0.1 -- a row enters the uncertain set when its pair is
// within 0.1 of the kink in this measure, i.e. within
// arcsin(0.1/sqrt(2)) = 4.05 degrees of the kink ray. The largest swept value
// that costs no iterations on the benign set; 0 reproduces the binary
// partition exactly.
//
// kSsnUncertainLeaveRatio = 3 -- the HYSTERESIS. A row LEAVES the uncertain set
// only once its margin reaches 3x the entering threshold, so a row whose margin
// hovers in [0.1, 0.3] keeps whatever class it already had and cannot chatter.
// The smallest ratio that is a band at all is > 1; 3 is large enough that a
// row must move materially to be re-decided and small enough that a genuinely
// decided row is never trapped.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 510–528

The line search: the Armijo form, the constant's provenance, and the merit-vs-certificate split. All kept, compressed.

```text
// ---- THE LINE SEARCH -----------------------------------------------------
//
// Armijo on the FB merit psi(w) = 1/2 ||F(w)||_2^2, accepted when
//
//     psi(w + t d) <= (1 - 2 * kSsnArmijoSigma * t) * psi(w),
//
// which is the standard sufficient-decrease form for a semismooth Newton
// direction on an FB reformulation (De Luca-Facchinei-Kanzow's merit; the
// exact Newton step of the UNregularized system gives
// grad psi^T d = -||F||^2, so the condition above is Armijo with that
// derivative substituted). 1e-4 is the textbook Armijo constant, loose enough
// that a full Newton step in the local regime always passes it -- which is
// what keeps the safeguarded trajectory equal to the bare one on every benign
// fixture.
//
// **THE MERIT IS THE 2-NORM SQUARE, THE CERTIFICATE IS THE INF-NORM**, on
// purpose: fb_tol certifies per-row quantities and must stay the inf-norm of
// banner section 5, while a merit function has to be smooth where phi is and
// max_k |F_k| is not.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 543–574

The LM-regularized ladder: the naming ruling, the current-iterate anchor's consequence, the rung count and the relative ceiling slack. All kept, compressed.

```text
// ---- THE LM-REGULARIZED LADDER ----
//
// NAMING: *proximal* properly means FBstab's outer loop with a LAGGING
// centre -- the thing this ladder is NOT. sigma is Wachter-Biegler-style
// regularization escalation on a CURRENT-iterate anchor, not a
// proximal-point method. The `ssn_prox_*` identifiers are shipped surface and
// are not renamed to match.
//
// The CURRENT-iterate anchor makes sigma an additive increment to the two
// regularizers the engine already applies: K's primal block gets
// H + (delta + sigma) I and every dual diagonal gets -(... + mu + sigma).
// The residual is untouched, so banner section 6's property survives intact:
// sigma costs ITERATIONS, never ACCURACY, and the iteration's fixed points
// remain exact, unregularized KKT points. A LAGGING centre would make the
// exit certificate two-level; SsnStart::prox_center_x carries that ruling.
//
// THE LADDER IS PER-SOLVE AND MONOTONE -- sigma never decreases inside one
// solve, except for the second-order verification's drop to the caller's own
// regularization (banner section 7b), which is not a rung. Two decades per
// rung and a 1e6 ceiling give SEVEN rungs from 1e-6 (1e-6, 1e-4, 1e-2, 1,
// 1e2, 1e4, 1e6), enough to dominate any Hessian the engine can be handed at a
// sane scaling and bounded by the step budget in any case.
//
// **THE CEILING TEST NEEDS A RELATIVE SLACK.** Repeated multiplication by 100
// does not reproduce 1e6 in binary floating point -- the sequence ends at
// 999999.9999999998, strictly below the cap -- so an exact `>= kSsnProxMax`
// guard grants an extra rung that raises sigma by 2.3e-10 relative and buys a
// full numeric factorization plus a full backtracking schedule for it.
// kSsnProxCapSlack closes the guard against that drift: 1e-9 is seven orders
// above the accumulated relative error (~2e-16) and eleven orders below one
// rung (a factor 100), so it can neither miss the ceiling nor merge two
// genuine rungs.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 584–641

The infeasibility telemetry: the mechanism, the three properties the stall test must have, the two routes' different growth references, and all four constants. All kept, compressed.

```text
// THE INFEASIBILITY TELEMETRY.
//
// An infeasible convex QP has NO KKT point, so F has no root and the
// iteration cannot converge. Instead ||F||inf FLATTENS onto a positive floor
// (the least-squares distance between the contradictory rows) while the
// multipliers of those rows GROW without bound, because
// phi(s, lambda) -> s as lambda -> +infinity on a row with s < 0. BOTH
// HALVES ARE REQUIRED: growth alone is ordinary early behaviour, and a stall
// alone is a hard-but-feasible problem or a budget too small (kBudget).
//
// THREE PROPERTIES THE STALL TEST MUST HAVE:
//
//   (i) THE WINDOW ADVANCES ON ACCEPTED STEPS, NEVER ON ATTEMPTS. A
//       proximal retry re-evaluates the IDENTICAL residual at the IDENTICAL
//       iterate, so counting attempts would let one rough patch fill the
//       window with copies of one point.
//   (ii) THE IMPROVEMENT DEMAND IS OVER THE WHOLE WINDOW, NOT PER STEP. A
//       proximally damped iteration legitimately crawls, which a per-step
//       demand reads as a stall on a perfectly FEASIBLE subproblem; over a
//       window the same crawl improves measurably and re-arms, while the
//       exactly-flat residual an infeasible QP produces still fills it.
//   (iii) THE GROWTH CONJUNCT DIFFERS ON THE TWO ROUTES, because they see
//       different evidence. Measuring both against the START POINT (floored
//       at 1) would make the test VACUOUS for a cold start on any feasible
//       QP whose true multipliers exceed 1e4.
//         - THE STANDING ROUTE re-arms its reference with the window, so
//           "the duals grew 1e4x" means the divergence happened WHILE
//           nothing improved.
//         - THE EXHAUSTION ROUTE cannot use a windowed reference at all:
//           the divergence and the last progress are the SAME accepted
//           step, so any "growth since the last progress" is 1. It keeps
//           the start-point reference and adds kSsnDualStepGrowth: the
//           multiplier norm must have multiplied by an ORDER OF MAGNITUDE
//           across the most recently accepted step. A trajectory converging
//           to large multipliers has settled (per-step ratio -> 1) by the
//           time its line search dies.
//
// A window in which the PROXIMAL LADDER escalated cannot declare a stall at
// all -- it is discarded and a fresh one starts. Slow progress under a sigma
// that just changed is the safeguard's doing, not the problem's.
//
// kSsnStallWindow = 5 ACCEPTED STEPS over which ||F||inf must improve on the
// window's reference by kSsnStallImproveFactor. Five is an order of magnitude
// above the local regime this method targets, so no healthy trajectory can
// reach it. The 1% improvement demand per WINDOW is deliberately feeble, and
// RELATIVE so that an exactly-flat residual cannot be mistaken for
// improvement by rounding noise.
//
// kSsnDualGrowthFactor = 1e4 against the multiplier norm at the reference
// point the route selects (floored at 1, so a zero-multiplier reference is
// measured absolutely). Four orders is far above any legitimate dual growth
// between two points that made no progress on each other, and far below the
// 1e150 scale at which IEEE double arithmetic itself breaks down.
//
// kSsnDualStepGrowth = 10, the EXHAUSTION route's second conjunct, on the
// growth across ONE accepted step: an order of magnitude in a single step is
// not a trajectory settling onto its multipliers, it is one whose multipliers
// have no limit to settle onto.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 654–673

The Levenberg-Marquardt sizing: the min(r, r^2) choice with its references, and the normalization. Both kept, compressed.

```text
// THE LEVENBERG-MARQUARDT SIZING (SsnOptions::sigma_rule).
//
// sigma_k = kSsnLmSigmaC * min(r_k, r_k^2), r_k = ||F_k||inf / f_scale, with
// f_scale = max(1, ||F_0||inf) the residual at THIS solve's own start point.
//
// WHY min(r, r^2) AND NOT r^2. Yamashita & Fukushima (Computing Suppl. 15,
// 2001) prove quadratic local convergence for mu_LM = ||F||^2 under a local
// error bound; Fan & Yuan (Computing 74, 2005) sharpen the exponent to any
// delta in [1, 2]. r^2 is the right size CLOSE to the solution and far too
// small FAR from it, where r is what the same theory wants; min(r, r^2) joins
// the two regimes at the point where they agree.
//
// WHY THE NORMALIZATION IS THE START RESIDUAL. This file's shift sits inside
// a KKT block whose primal diagonal is H's, so the exponent and the constant
// are TUNABLE rather than derived; dividing by the start residual makes the
// lever's first sigma independent of the problem's absolute scaling. Floored
// at 1 so an already-converged start point cannot manufacture a large r.
//
// c = 1 -- the neutral constant: with the floor and the cap below, c only
// selects WHERE between them sigma sits.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 685–708

The Farkas residual test's constants: the lemma, both tolerances and the absolute-floor reading. All kept, compressed.

```text
// THE FARKAS RESIDUAL TEST (SsnOptions::infeasibility_rule).
//
// The system {Ae x = be, a_k^T x <= b_k} is infeasible IFF there is
// (y_e free, y >= 0) with Ae^T y_e + sum_k y_k a_k = 0 and
// be^T y_e + sum_k y_k b_k < 0 (Farkas). This file tests that pair on the
// NORMALIZED DUAL INCREMENT projected onto the sign cone -- one matvec plus
// O(m), and no factorization.
//
// kSsnFarkasResidualTol = 1e-6 on the RELATIVE residual
// ||A^T y||inf / max(1, ||(|A|^T |y|)||inf). Relative because the test must
// survive bad scaling; 1e-6 because the direction tested is an FB dual
// increment rather than an exact recession direction, so demanding more would
// test the iteration's convergence rather than the certificate's existence.
//
// BOTH QUANTITIES ARE RELATIVE ONLY ABOVE 1. The `max(1.0, .)` in each
// denominator is an ABSOLUTE FLOOR: below 1 the test is on the ABSOLUTE
// residual, which is what stops a near-zero denominator from manufacturing a
// certificate out of rounding noise. Same reading for the gap.
//
// kSsnFarkasGapTol = 1e-8 on the RELATIVE Farkas objective
// <b, y> / max(1, sum_k |b_k y_k|), which must be at most -kSsnFarkasGapTol.
// A strictly negative <b, y> is the certificate's own second half; the
// tolerance exists only so a rounding-noise negative cannot fire it, eight
// orders below the O(1) values a genuine contradiction produces.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 750–760

`SsnInertia`: why the verdict is restated rather than included, and the rule order. Both kept, compressed.

```text
// What one factorization's reported inertia says about the system factorized.
//
// **THIS IS qp_engine.h's detail::InertiaVerdict, RESTATED RATHER THAN
// INCLUDED**, for the same reason kSsnInfBound is restated: this header
// depends on the QP data types and on the KKT factor helper, never on the
// walk's internals. The verdict rules are qp_engine.h's exactly
// (docs/retarget-design-sqp.md SS4.1): non-kObserved evidence routes to
// kSuspect explicitly, then perturbed pivots first -- a factorization whose
// pivots were perturbed reports an inertia that looks exactly like a genuine
// one, so its counts carry no information AT ALL, including when they happen
// to match -- then the short-sum rule.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 785–818

`enum class SsnEscape`: the five reasons, the certifies-nothing rule and the branch-on-escape_reason rule. All kept, compressed.

```text
// Why a solve stopped somewhere other than a converged point. kNone is the
// converged case.
//
// ALL FIVE ARE REACHABLE:
//
//   kBudget            hard_budget attempts were spent. Says nothing about the
//                      problem -- only that this budget was too small.
//   kSingular          the factorization threw, the step came back non-finite,
//                      or the inertia was TRUSTWORTHY AND WRONG at the top of
//                      the ladder. escape_detail names which.
//   kIndefinite        THE SECOND-ORDER VERIFICATION's verdict: the residual
//                      satisfies fb_tol -- so the point IS first-order KKT --
//                      but the inertia of K there is trustworthy and NOT
//                      (n, me+mi+mb), i.e. the primal block is not positive
//                      definite. The point is a saddle or a maximizer of the
//                      QP, which no residual-based test can see.
//   kNoContraction     THE LINE SEARCH's verdict: the Armijo schedule ran down
//                      to detail::kSsnMinStep without the merit accepting, at
//                      the top of the ladder. The direction is not a descent
//                      direction for the merit and damping did not make it one.
//   kInfeasibleSuspect THE DIVERGENCE TELEMETRY's verdict: ||F||inf stalled
//                      (detail::kSsnStallWindow) WHILE the multiplier norm grew
//                      by kSsnDualGrowthFactor. SUSPECT because it is a
//                      diagnosis from behaviour, never a Farkas certificate.
//
// **NONE OF THEM CERTIFIES ANYTHING.** An escaped SsnResult reports where the
// solve STOPPED. The driver routes an escaped subproblem back to the walk;
// that routing is the only correct consumption of any value below.
//
// **BRANCH ON escape_reason, NEVER ON status.** kInfeasibleSuspect reports
// QpStatus::kInfeasible, which the walk issues as a CERTIFICATE, and
// kIndefinite/kNoContraction/kSingular all report QpStatus::kNumericalError.
// A driver switching on status alone would promote a suspicion to a
// certificate at the driver layer.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 866–884

`SsnStart::prox_center_x`/`prox_center_lambda`: the anchor ruling with the lagging-centre counter-factual. The ruling and its consequence are kept.

```text
    // **STILL IGNORED, AND THIS IS A RULING RATHER THAN AN OMISSION.** The
    // proximal term does not read them: it anchors at the CURRENT ITERATE
    // instead of at a lagging centre.
    //
    // WHY THAT ANCHOR. With the centre at the current point the proximal term
    // perturbs only the JACOBIAN, so banner section 6's property survives --
    // the iteration stays modified Newton on the EXACT F, its fixed points
    // stay exact unregularized KKT points, and ONE residual per attempt serves
    // both the certificate and the merit. A LAGGING centre changes the
    // residual (F_sigma = F + sigma(w - wbar), the FB rows carrying a shifted
    // slack), which splits the exit certificate into an inner and an outer one
    // and forces two residual evaluations per attempt. The one thing it
    // uniquely buys -- the divergence-of-the-proximal-step infeasibility
    // certificate -- this engine does not claim to produce anyway
    // (SsnEscape::kInfeasibleSuspect is behavioural).
    //
    // The fields stay, still size-checked when non-empty, so the interface is
    // the right shape if a cell ever needs a lagging centre. A caller wanting
    // a warm proximal sequence today uses SsnOptions::prox_sigma_init.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 888–919

`SsnStart::box_center`: what it does, why it exists, and why it is an optional rather than an empty vector. All kept, compressed.

```text
    // **THE TRUST REGION'S CENTRE, AND IT IS HONOURED** -- which is the whole
    // reason it sits beside two fields that are not, and carries a different
    // shape from every other field in this struct.
    //
    // WHAT IT DOES. The trust region is built as `[c - Delta, c + Delta]`
    // intersected with the QP's own bounds (build_bound_rows). `c` is
    // `start.x` when this is empty -- the historical rule, unchanged -- and
    // the value here when it is set. NOTHING ELSE READS IT: it is not a
    // starting iterate, not a proximal anchor, and not part of the structure
    // key (the window's SHAPE is, through SsnBoundRow, exactly as a radius
    // change already was).
    //
    // WHY IT EXISTS (M6 W1 task 6 fix round 2). A caller that has already
    // solved this subproblem in ITS OWN window and wants SSN to continue from
    // the point it reached needs to say "start HERE, in THAT window" -- and
    // those are two different vectors. The interior-point tier is that
    // caller: section 2.3 item 4 hands SSN the finished IPQP iterate as a
    // warm grade, while the window both kernels and `QpEngine::refine_on_face`
    // must agree on is the CLAMP-CENTRED one, `c = clamp(0, l, u)`
    // (detail/qp/ipqp_engine.h's IpqpBox contract, and qp_engine.h's
    // refine_on_face precondition). Without this field, passing the iterate
    // would silently recentre the window on it -- the exact failure that
    // precondition warns about.
    //
    // `std::optional` RATHER THAN THIS STRUCT'S EMPTY-VECTOR CONVENTION, and
    // the difference is load-bearing: everywhere else here "empty" means "zero
    // of the right size", and for a CENTRE the origin is a perfectly
    // legitimate value a caller may want. Absent and "at the origin" have to
    // be distinguishable.
    //
    // Validated for size when engaged (`solve()` throws on a mismatch); a
    // non-finite entry is refused for the same reason a radius is.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 925–939

`enum class SsnSafeguards`: the kBare definition and the runtime-switch argument. Both kept, compressed.

```text
// WHICH ITERATION A SOLVE RUNS. Two modes, and the bare one exists to be a
// POSITIVE CONTROL rather than a product surface.
//
// **kBare IS THE BARE LOCAL METHOD, BIT FOR BIT** -- full undamped steps, the
// binary alpha > beta partition with no uncertain set and no hysteresis, no
// line search, no dual projection, no proximal term and no inertia gate. The
// two modes are the same function, so the reproduction claim is checked on
// every suite run rather than argued.
//
// **A RUNTIME SWITCH, DELIBERATELY.** A compile-time switch could only be a
// macro (un-co-testable in one TU) or a template parameter on SsnEngine
// (propagating into every driver signature and into SqpCounters). A runtime
// enum costs one predictable branch per solve on a path that has already paid
// a sparse factorization, and keeps both modes in ONE binary so a single test
// can compare them directly.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 953–965

`SsnOptions::soft_budget`: the attempts-not-steps rule, the arming behaviour and the no-warn-counter decision. The first two are kept.

```text
    // THE ESCALATION THRESHOLD, and it is a threshold on ATTEMPTS rather than
    // on accepted steps -- an attempt that ends in a rejected step is exactly
    // the kind of trouble the escalation exists for, so it must count.
    //
    // On reaching it a solve that is still running unregularized turns the
    // proximal term ON at detail::kSsnProxInit. That is the whole of the
    // "soft_budget warns" contract, and the warning is OBSERVABLE rather than
    // printed: SsnCounters::ssn_prox_updates leaves zero. No separate warn
    // counter, deliberately -- one would widen SqpCounters for a fact
    // ssn_prox_updates already carries.
    //
    // 12 is above every benign fixture's attempt count, so the escalation is
    // provably inert on all of them.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 968–977

`SsnOptions::hard_budget`: the cap and the verification's exemption from it. Both kept, compressed.

```text
    // THE HARD CAP, on ATTEMPTS rather than on accepted steps
    // (SsnResult::factorizations states the invariant). A solve that reaches
    // it stops with QpStatus::kMaxIter and SsnEscape::kBudget. Must be >= 0;
    // 0 means "test the start point and take no step".
    //
    // **THE SECOND-ORDER VERIFICATION IS NOT AN ATTEMPT AND IS NOT CAPPED BY
    // THIS FIELD** (banner section 7b). Under kFull a start point that is
    // already converged still pays the ONE verification factorization at
    // hard_budget = 0, because the alternative is certifying a point nothing
    // ever looked at. It takes no step, so the field's contract is intact.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1009–1035

`SsnOptions::defer_certification`: both states, why a caller would want it, and the two-certificates warning. All kept, compressed.

```text
    // DEFER THE CERTIFYING EXIT'S INERTIA EVIDENCE (SsnOptions::defer_certification).
    //
    // false (default) -- banner section 7b verbatim: the convergence test opens a
    // VERIFICATION ATTEMPT that factorizes K at the converged point and reads
    // its inertia before kOptimal is issued.
    //
    // true -- the verification attempt is built (rows classified at the
    // converged point, sigma dropped to the caller's own regularization, K's
    // diagonals refreshed) but NOT FACTORIZED. The solve returns kOptimal with
    // `SsnResult::certification_deferred` set, and the caller owes the engine
    // exactly one of finish_deferred_certification() or
    // discard_deferred_certification() before the next solve.
    //
    // **WHY A CALLER WOULD WANT THAT** is Gould's lemma (Gould, Math. Prog. 32,
    // 1985): the KKT matrix of the face EQP has inertia (n_f, m_f, 0) IFF the
    // reduced Hessian on that face is positive definite, so a caller about to
    // re-solve the identified face EXACTLY -- QpEngine::refine_on_face, which
    // gates on that very verdict -- already buys the second-order evidence.
    // THE TWO CERTIFICATES ARE NOT THE SAME STATEMENT: this engine's
    // verification tests (n, me+mi+mb) on the FULL augmented block at the
    // caller's regularization; the face route tests positive definiteness of
    // the reduced Hessian ON THE IDENTIFIED FACE. They can disagree.
    //
    // **NOTHING IS WEAKENED WHEN THE FACE SOLVE IS REFUSED.** A caller whose
    // refinement is refused calls finish_deferred_certification() and gets
    // exactly the shipped verdict at exactly the shipped cost, because the
    // matrix it factorizes is the one this loop already built.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1038–1058

`SsnOptions::sigma_rule`: the three rules, the formula and the monotone-floor retention. All kept, compressed.

```text
    // HOW sigma IS SIZED (sqp_types.h's SsnSigmaRule).
    //
    // kLadder (default) is banner section 7's failure-reactive ladder verbatim. The
    // two residual rules replace the CLIMB with a size read off the residual:
    //
    //     sigma_k = max(ladder_k, clamp(c * min(r_k, r_k^2),
    //                                   kSsnProxInit, kSsnProxMax))
    //
    // with r_k = ||F_k||inf / max(1, ||F_0||inf) and c = detail::kSsnLmSigmaC.
    //
    // THE LADDER IS RETAINED AS A MONOTONE FLOOR, NOT REPLACED: every
    // escalation trigger still climbs a rung, so a solve that cannot be
    // repaired still reaches the ceiling and escapes with the same reason.
    // Between triggers sigma is sized from the evidence rather than the
    // history, and a residual that FALLS lowers sigma back toward the floor --
    // which the monotone ladder structurally cannot do.
    //
    // kResidualArmed is INERT until the ladder arms, so it is provably inert
    // on every fixture whose ladder never arms. kResidualAlways sizes sigma
    // from attempt 0, carries at least kSsnProxInit everywhere, and is inert
    // NOWHERE.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1061–1073

`SsnOptions::hint_rule`: the two rules and where they agree and differ. Both kept, compressed.

```text
    // WHAT PROTECTS THE HINTED FIRST STEP (sqp_types.h's SsnHintRule).
    //
    // kIterationZeroFree (default) is banner section 7 step 9's exemption verbatim.
    // kWatchdog replaces it with the published rule the exemption is an
    // unsafeguarded special case of: up to `watchdog_q` relaxed (unsearched)
    // steps from the hinted start, a stored BEST point, and -- if the merit has
    // not achieved Armijo decrease against the watchdog's own reference by then
    // -- a RETURN TO THAT BEST POINT followed by a monotone step.
    //
    // The two agree exactly whenever the hint was right (the relaxed step lands
    // at the solution and the convergence test fires before the watchdog can
    // judge it) and differ exactly where the exemption has no answer: a wrong
    // hint whose relaxed step is accepted and bad. `watchdog_q` must be >= 1.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1077–1093

`SsnOptions::infeasibility_rule`: the arming/firing split and the more-reluctant guarantee. Both kept, compressed.

```text
    // WHAT TURNS A SUSPICION INTO AN EXIT (sqp_types.h's SsnInfeasibilityRule).
    //
    // kSymptoms (default) is the stall/growth conjunct pair of the
    // kSsnStallWindow block verbatim: the symptoms ARE the exit test.
    //
    // kFarkasGated keeps every symptom conjunct as the ARMING condition and
    // adds a CERTIFICATE as the firing condition -- the normalized dual
    // increment, projected onto the sign cone, tested as an approximate Farkas
    // direction (detail::kSsnFarkasResidualTol / kSsnFarkasGapTol). The
    // increment is measured against the STANDING route's window reference on
    // that route and against the PREVIOUS accepted step on the EXHAUSTION
    // route, mirroring exactly the reference each route's growth conjunct
    // already uses and for the same reason.
    //
    // It can only make the engine MORE reluctant to report kInfeasible: an
    // armed check that finds no Farkas direction falls through to the ordinary
    // budget/no-contraction routes.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1125–1136

The lever instruments: why they live on SsnResult rather than SsnCounters, and what each counts. Both kept, compressed.

```text
    // THE LEVER INSTRUMENTS live here rather than on SsnCounters DELIBERATELY:
    // they measure an opt-in arm, not the product, and SqpCounters::ssn is
    // serialized into the corpus CSV whose schema is a pinned artifact.
    //
    // `watchdog_returns` -- times the q-step watchdog exhausted its relaxed
    // window without Armijo decrease and RETURNED TO THE BEST STORED POINT.
    // Structurally 0 under SsnHintRule::kIterationZeroFree.
    //
    // `farkas_fired` / `farkas_refusals` -- infeasibility exits the Farkas
    // certificate CONFIRMED, and armed symptom pairs it REFUSED (i.e. the
    // shipped rule would have declared kInfeasible and this one did not).
    // Both structurally 0 under SsnInfeasibilityRule::kSymptoms.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1150–1165

`SsnResult::factorizations`: the inequality chain and the deferred-certification exception. Both kept, compressed.

```text
    // Numeric factorizations paid, one per ATTEMPT.
    //
    // **THE SAFEGUARDS SEPARATED THESE TWO**, and the gap is their cost read
    // directly: an attempt can pay its factorization and take NO step (the
    // schedule was exhausted, or the inertia came back wrong), so
    //
    //     iters <= factorizations <= min(attempts, hard_budget) + 1,
    //
    // where the +1 is the CERTIFYING EXIT's second-order verification (banner
    // section 7b) -- which is why equality on the left is unreachable under
    // kFull. **UNDER SsnOptions::defer_certification THE +1 IS NOT PAID BY
    // THIS SOLVE**: it moves to finish_deferred_certification(), which adds it
    // to THIS field when the caller runs it, or is never paid at all when the
    // caller's face solve supersedes it -- so a deferring solve can read
    // iters == factorizations at a certifying exit. Under kBare equality is
    // exact.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1181–1199

`ineq_active`/`bound_state`: the derivation rule, the write-only note, the always-populated rule and the kFixed/kFree conventions. All kept, compressed.

```text
    // THE IMPLIED ACTIVE SET AT THE RETURNED POINT, in the two shapes
    // QpSolution reports it (qp_problem.h): one flag per row of Ai, one
    // BoundState per variable. Derived from the FINAL iterate by the engine's
    // own partition rule -- row active iff lambda > s, equivalently
    // alpha > beta (banner section 2) -- so it is the same partition the last
    // Jacobian would have selected, not a separately maintained working set.
    //
    // WRITE-ONLY, LIKE THE COUNTERS: nothing here reads either vector back,
    // and computing them cannot move a trajectory. They exist for the activity
    // export (warm-start hand-off and stable-face refinement).
    //
    // ALWAYS POPULATED, INCLUDING ON AN ESCAPE and on a solve that took zero
    // steps: the derivation reads the iterate, not the loop's history. On an
    // escaped solve it describes where the solve STOPPED and certifies
    // nothing.
    //
    // A variable whose lower AND upper bound rows are both implied active
    // reports kFixed; one with neither reports kFree. A variable with no finite
    // bound at all always reports kFree, since it has no row to be active.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1203–1217

`tr_active`: the contract, the stationarity caveat and the all-false cases. All kept, compressed.

```text
    // TRUST-REGION ACTIVITY, size n, qp_problem.h's QpSolution::tr_active
    // contract verbatim. True at index i iff the variable is held by a
    // TR-tight effective bound rather than a real one. Such a variable reports
    // kFree in bound_state above -- a REAL-BOUND-ONLY view -- and z(i) is 0,
    // because TR duals are internal to this solve and are never exposed.
    //
    // SAME STATIONARITY CAVEAT AS THE WALK'S: at a TR-pinned index the
    // reported quantities do NOT satisfy stationarity, since the multiplier
    // that balanced that row was dropped on the way out. A kFree entry there
    // is evidence that no REAL bound constrains the coordinate, NOT evidence
    // that the point is stationary in it. **Read tr_active, never z or
    // bound_state, to detect a binding radius.**
    //
    // All-false whenever the solve ran without a trust region (the default
    // SolveOverrides), and all-false when a finite radius never bound.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1220–1248

`tr_violation`: the definition and the three-part contract, including the measured 133x-160x escaped violation on cycling_qp_3var. The contract is kept; the measurement is here.

```text
    // **HOW FAR x LIES OUTSIDE THE TRUST REGION, AND IT CAN BE FAR.**
    // max_j max(0, x_j - up_eff_j, lo_eff_j - x_j) over the
    // variables whose effective bound came from the RADIUS -- 0.0 when no
    // radius was supplied, and 0.0 when the radius held.
    //
    // THE CONTRACT, stated here because the driver's funnel is the consumer
    // and its ratio test presumes ||d||inf <= Delta:
    //
    //   * THE TRUST REGION IS A SOFT CONSTRAINT IN THIS KERNEL. It enters as FB
    //     bound ROWS, and an FB row is satisfied only at a root -- so no
    //     intermediate iterate is confined to the box, the line search does not
    //     confine it either (the TR rows are four more terms in the same merit,
    //     not a step-length cap), and a solve that stops early can stop
    //     anywhere. MEASURED on cycling_qp_3var from x0 = 0 at
    //     tr_radius = 0.01: hard_budget 1..5 return ||x||inf between 1.33 and
    //     1.60, i.e. 133x to 160x the radius.
    //   * ON A CERTIFYING EXIT the violation is bounded by the tolerance and
    //     nothing else: |phi| <= fb_tol permits a slack negative by O(fb_tol)
    //     (banner section 5's hypothesis caveat, applied to the TR rows), so
    //     tr_violation <= kSsnComplementarityFactor * fb_tol ~ 1.71 fb_tol.
    //     That is the only exit at which this kernel's x may be used as a
    //     trust-region step.
    //   * ON ANY ESCAPE the value is UNBOUNDED and is reported rather than
    //     repaired. It is deliberately NOT clamped: clamping would break the
    //     export's one invariant -- that x, fb_residual, ineq_active,
    //     bound_state, tr_active and the uncertain flags all describe ONE
    //     point -- and hand the funnel a point whose residual it had never
    //     been measured at. The driver routes every escape to the walk; this
    //     field is what lets it ASSERT that it did.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1251–1270

`ineq_uncertain`/`bound_uncertain`: the pessimistic reading, the last-assembly rule and the hysteresis argument. All kept, compressed.

```text
    // THE UNCERTAIN SET AT THE RETURNED POINT, the third leg of the CHR
    // partition that ineq_active/bound_state cannot express -- those two
    // report the BINARY reading (lambda > s), because the driver's hint ingest
    // and QpSolution's own contract are binary.
    //
    // ineq_uncertain[k] is true iff row k of Ai was in the uncertain set when
    // the LAST generalized Jacobian was assembled; bound_uncertain[j] is true
    // iff EITHER of variable j's bound rows was -- the pessimistic reading,
    // since the flag exists to warn a consumer off trusting the binary one.
    //
    // **THE LAST ASSEMBLY**, which under kFull is the RETURNED ITERATE on every
    // certifying exit (the second-order verification classifies there, banner
    // section 7b) and is one iterate back on an escape. The distinction
    // matters because the classification carries HYSTERESIS: it is a function
    // of the whole trajectory and cannot be recomputed from the final point.
    // On a solve that assembled no Jacobian at all both vectors are all-false
    // -- the honest reading of "no classification was ever made", not a claim
    // that every row was decided. Always sized (mi / n).
    //
    // ALWAYS ALL-FALSE UNDER SsnSafeguards::kBare, which has no uncertain set.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1323–1344

`solve` (the overrides overload): the seam, the trust-region resolution and the regularizer note. All kept, compressed.

```text
    // THE PER-SOLVE SEAM. Takes the WALK'S OWN SolveOverrides (qp_types.h)
    // rather than a parallel type, because the funnel driver already builds
    // one per subproblem and per SOC re-solve. Every field's sentinel and
    // resolution rule is qp_types.h's, unchanged: tr_radius +inf means "no
    // radius", primal_delta/dual_mu negative means "use the engine's".
    //
    // TRUST REGION. lo_eff = max(lower, x0 - Delta), up_eff = min(upper,
    // x0 + Delta), about THIS solve's own start point, resolved once here and
    // used for every bound row thereafter -- see build_bound_rows for the
    // pattern-invariance property that makes a shrink-retry loop free, and
    // export_activity/recombine_bound_multipliers for the TR-pin/real-bound
    // separation that keeps radius artefacts out of z and bound_state.
    //
    // REGULARIZERS. primal_delta/dual_mu are honoured too -- ignoring a field
    // the caller deliberately set is the silent-drop antipattern. **BUT THE
    // DRIVER'S ADAPTIVE-mu SCHEDULE BUYS NOTHING HERE**: delta and mu perturb
    // only the JACOBIAN (for_each_entry's emissions and the FB diagonal) and
    // never the residual, so the iteration is modified Newton on the EXACT F
    // and its fixed points are exact, unregularized KKT points. They cost
    // iterations, not accuracy -- the opposite trade from the walk, whose
    // returned solution carries the mu|lambda| footprint that motivated the
    // schedule.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1418–1437

`bound_hint_active`: how a kFixed hint on an l < u variable arises, and why it is documented recovery rather than rejection. Both kept, compressed.

```text
    // A kFixed hint marks BOTH of a variable's rows active, which is right for
    // a genuinely fixed variable (l == u) and is a DOCUMENTED DEGRADED MODE
    // when l < u.
    //
    // HOW IT ARISES: export_activity reports kFixed whenever both of a
    // variable's bound rows come out implied-active, which a noisy or ESCAPED
    // iterate can produce on an l < u variable -- and that export is exactly
    // what the driver re-ingests as a warm-start hint. The first step then
    // solves the contradictory pair {x_j = l_j, x_j = u_j}, which the dual-mu
    // block regularizes into the midpoint rather than a singular
    // factorization.
    //
    // **THE CHOICE IS DOCUMENTED RECOVERY, NOT REJECTION.** Rejecting would
    // make a warm-start hand-off THROW because the previous solve happened to
    // stop somewhere noisy, and push every caller into sanitising a hint the
    // kernel itself produced. A hint is ADVICE about the first step, not a
    // constraint: every later step re-derives the partition from the FB
    // branch, so a contradictory hint costs iterations and nothing else.
    // Masking kFixed at EXPORT was rejected too -- it would suppress a real
    // signal and cannot help a caller who builds the hint elsewhere.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1441–1463

`split_bound_multipliers`: the caller-error refusal, the exact-zero test and the real-bound reading. All kept, compressed.

```text
    // Splits the signed z into the two non-negative bound-row multipliers.
    //
    // **A SEEDED z THAT PRICES A BOUND THIS QP DOES NOT HAVE IS A CALLER ERROR
    // AND THROWS.** z(j) > 0 prices variable j's LOWER bound and z(j) < 0 its
    // UPPER one; if that side is absent (at or beyond kSsnInfBound, so there
    // is no row for the multiplier to live on) the mass has nowhere to go.
    // A real hazard rather than a tidiness point: warm_start.h records exactly
    // this shape (a z priced against a 1e20 bound) poisoning a downstream
    // estimate, and an ingest handing one model's z to a differently bounded
    // model would produce it.
    //
    // THE TEST IS EXACT ZERO, not a tolerance: an absent side has no
    // multiplier at all, so the only correct value there is 0, and any nonzero
    // -- however small -- means the caller believes in a bound this QP does
    // not have.
    //
    // **THE ABSENT-SIDE TEST READS THE REAL BOUND, NOT THE EFFECTIVE ONE**, and
    // a TR row is seeded at zero. A caller's z prices the QP's OWN bounds; the
    // trust region is this solve's private construction, so a finite radius
    // must not turn "you priced a bound that does not exist" into silence
    // (every variable has finite effective bounds under a finite radius, which
    // would make the check vacuous). TR duals are internal, exactly as
    // qp_problem.h's tr_active contract requires on the way out.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1474–1525

`for_each_entry`: the placeholder, the two sync paths, the accumulation rule, the composite structure key and the guarded collision exposure. All kept, compressed.

```text
    // THE FIXED PATTERN, AND ITS REUSE ACROSS SOLVES.
    //
    // THE FB DIAGONALS GET A NONZERO PLACEHOLDER (-1.0) rather than their
    // eventual value, so their slots exist regardless of what any later branch
    // wants there (beta/alpha_f + mu is legitimately EXACTLY 0 on a strictly
    // active row when the caller zeroes dual_mu), and the diagonal refresh
    // addresses those slots positionally (K.outerIndexPtr()[row]) rather than
    // by search. Defensive rather than strictly required by the vendored Eigen
    // -- setFromTriplets preserves explicit zeros today, but prune()/assignment
    // paths drop them elsewhere -- and the cost of not depending on it is one
    // literal.
    //
    // sync_matrix() decides between two paths and reports which:
    //
    //   REBUILD (returns true) -- the structure differs from the cached one:
    //     emit every entry as a triplet, setFromTriplets, then record each
    //     emitted entry's position in the compressed value array (one binary
    //     search per entry, over its own row) so the reuse path can address
    //     them positionally.
    //   REFRESH (returns false) -- same structure: zero the value array and
    //     re-emit, accumulating into the recorded positions. O(nnz), no sort,
    //     no allocation, and Pardiso's cached symbolic analysis survives
    //     because K's index arrays were never touched.
    //
    // THE TWO PATHS SHARE ONE EMISSION ORDER BY CONSTRUCTION -- for_each_entry()
    // is the single walk both drive -- so they cannot drift apart.
    //
    // ACCUMULATION, NOT ASSIGNMENT, on the refresh path: setFromTriplets SUMS
    // duplicates, and for_each_entry emits duplicates deliberately (H's diagonal and
    // the separate primal_delta entry land on the same slot). Zeroing first and
    // then += reproduces that summation exactly; a straight = would silently
    // drop primal_delta wherever H has a stored diagonal.
    //
    // THE STRUCTURE KEY is a COMPOSITE of two conjuncts:
    //
    //   1. structure_hash -- hven's combined pattern key (feed_pattern,
    //      docs/pattern-hash.md), the same instrument the KKT factor helper's
    //      pattern compare uses to decide whether to skip the symbolic
    //      analysis (kkt_calls.h).
    //   2. the bound-row (var, sign) list, compared EXACTLY against the copy
    //      cached when the current pattern was built (bound_rows_match_cached
    //      / structure_bound_key_) -- not hashed, so no collision exposure at
    //      all on this half.
    //
    // A COLLISION IN CONJUNCT 1 IS GUARDED, NOT MERELY ASSERTED AWAY: the
    // refresh path bounds-checks every write and requires that the emission
    // consumed the map EXACTLY (t == value_pos_.size()), converting a
    // structure-mismatched refresh into a thrown std::runtime_error rather
    // than undefined behaviour. A collision between two structures with the
    // SAME entry count remains undetected -- a 64-bit FNV coincidence, and the
    // honest residual exposure; the guard is unreachable by any fixture, so it
    // is a knowingly-unkillable defensive line like the placeholder above.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1530–1541

`structure_hash`: why the pattern is fed through feed_pattern and why the digest is portable. Both kept, compressed.

```text
    // CONJUNCT 1 of the composite structure key: hven's combined pattern key
    // over the dimensions and the three input patterns. Fed **through
    // feed_pattern, NOT off the raw index arrays**: qp.H/Ae/Ai are
    // CALLER-SUPPLIED and QpProblem imposes no compression requirement, and
    // feed_pattern's contract is that either storage state produces the
    // compressed digest -- same O(nnz), no compressed copy, exact in both
    // states. Every ingredient goes through Fnv1a::feed_index (64-bit widened,
    // LSB-first), so the digest depends on neither host byte order nor
    // SpMatRM::StorageIndex's width. The bound-row list is DELIBERATELY not
    // here: it is conjunct 2 (bound_rows_match_cached), compared exactly
    // rather than hashed. The digest's VALUE is only ever compared against
    // another computation of this same function on this same engine.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1544–1565

`bound_rows_match_cached`: why `var` is load-bearing, why `sign` is conservative, and why `rhs`/`from_tr` are excluded. All kept, compressed.

```text
    // CONJUNCT 2: the bound-row (var, sign) list, compared EXACTLY against
    // the copy cached when the current pattern was built.
    //
    // **br.var IS THE LOAD-BEARING HALF, AND mb ALONE DOES NOT COVER IT.**
    // The case mb misses is a DIFFERENT ASSIGNMENT of the same NUMBER of bound
    // rows to variables -- two rows both on variable 0 versus one row each on
    // variables 0 and 1 -- which puts the bound block's off-diagonal entries
    // in different columns of K. Reusing a pattern across that would write
    // B's values into A's slots.
    //
    // sign participates too, CONSERVATIVELY rather than necessarily: a sign
    // flip (a variable trading its lower bound for its upper) moves no slot,
    // only a value, and the refresh path re-emits br.sign. It is kept because
    // an over-conservative key costs one avoidable rebuild in that one case,
    // and because rebuild COUNTS are pinned currency.
    //
    // rhs and from_tr are EXCLUDED: both are value attributes of a row whose
    // slot they do not move (see SsnBoundRow), and comparing either would
    // rebuild the pattern on bound VALUE moves -- from_tr on every radius
    // change of a shrink-retry loop, which is exactly the tax this cache
    // exists to remove. The cache is a (var, sign) pair list, not a
    // SsnBoundRow copy, so neither CAN be compared by accident.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1599–1616

`trial_point`: the dual projection as the wrong-hint mitigation, with its confinement argument. The mitigation and the convexity argument are kept.

```text
    // w + step * dw, with THE DUAL PROJECTION applied to the two non-negative
    // multiplier blocks.
    //
    // **THE PROJECTION IS THE WRONG-HINT MITIGATION**, and the only cheap one
    // available: the landing configuration (s, lambda) = (0, lambda < 0) is a
    // DIFFERENTIABLE point of phi -- subdifferential the singleton {(1, 2)} --
    // so no derivative re-selection can help. A wrongly hinted active row
    // lands with a NEGATIVE multiplier and is confined to the line
    // s+ + 2 lambda+ = 2 s0 - (2 s0/|lambda| + mu) d lambda, on which a
    // non-degenerate row's root does not lie; clipping lambda to 0 moves the
    // pair onto the kink, where the symmetric subdifferential element applies
    // and the confinement dissolves.
    //
    // It is a PROJECTION ONTO A CONVEX SET THAT CONTAINS EVERY SOLUTION
    // (lambda >= 0 is a KKT condition), so it cannot move the iterate away
    // from the solution set, and it is inert at any point that already
    // satisfies it. lambda_e is untouched: equality multipliers are free-sign
    // and clipping them would be a wrong answer, not a safeguard.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1643–1668

`farkas_certificate`: the lemma, what is tested, the relative scaling and the cost. All kept, compressed.

```text
    // -----------------------------------------------------------------------
    // THE FARKAS RESIDUAL TEST, matvec-only
    // -----------------------------------------------------------------------
    //
    // The QP's constraint system is {Ae x = be} together with the
    // inequality-shaped rows a_k^T x <= b_k of banner section 1 -- the rows of
    // Ai and every finite bound row alike. Farkas' lemma: that system is
    // INFEASIBLE if and only if there is (y_e free, y >= 0) with
    //
    //     Ae^T y_e + sum_k y_k a_k = 0    and    <be, y_e> + sum_k y_k b_k < 0.
    //
    // `dle`/`dli`/`dlb` is the DUAL INCREMENT to be tested. It is projected
    // onto the sign cone (the equality block passes through; the two
    // non-negative blocks are clipped at 0), normalized by its own inf-norm,
    // and the two Farkas quantities are evaluated: the RESIDUAL
    // ||Ae^T y_e + Ai^T y_i + B^T y_b||inf and the OBJECTIVE <b, y>, each
    // reported RELATIVE to the same combination taken in absolute value -- the
    // cancellation-free scale of the sum, so a badly scaled row cannot make a
    // non-certificate look like one or vice versa.
    //
    // One matvec over Ae/Ai plus O(mb) plus O(n): no factorization, no solve,
    // and no allocation beyond two n-vectors.
    //
    // Returns true iff both tolerances are met, i.e. iff the increment IS an
    // approximate Farkas direction. `resid_out`/`gap_out` carry the two
    // relative quantities for the caller's diagnostic message.
```

**SOURCE** 1997159 · include/hven/detail/qp/ssn_engine.h · lines 1683–1726

`select_branch`: the three-set partition, the hysteresis, the tie policy, what an uncertain row gets, why it is still a generalized Jacobian element, and why the hint overrides the classification. All kept, compressed.

```text
    // BRANCH SELECTION (banner sections 2 and 4).
    //
    // Writes alpha/beta/row_resid/klass at slot `slot`. `hinted` says a hint
    // governs THIS row (first step, and the relevant half of the hint was
    // supplied); `hint_active` is that hint's verdict. `guarded` selects the
    // three-set classification and the uncertain damping; under kBare this
    // function IS the bare local method, statement for statement.
    //
    // THE THREE-SET PARTITION (CHR 2015's scaffold). The FB pair's own margin
    // |alpha - beta| measures how far the row is from the kink ray s = lambda,
    // where its classification is undecidable (detail::kSsnUncertainEnter
    // derives the geometry). The classification is read with HYSTERESIS --
    // enter the uncertain set at `tau`, leave it only at
    // kSsnUncertainLeaveRatio * tau -- so a row whose margin hovers inside the
    // band keeps whatever class it had and cannot chatter.
    //
    // THE TIE POLICY IS A CONSEQUENCE OF THE BAND, NOT A SEPARATE RULE: an
    // exact tie (alpha == beta) has margin 0, inside every band with tau > 0,
    // so a tie NEVER decides anything -- a previously decided row becomes
    // uncertain and a previously uncertain row stays uncertain. (kBare's
    // binary rule, and the activity export, break an exact tie toward INACTIVE
    // via a strict >, since QpSolution's contract is binary.)
    //
    // WHAT AN UNCERTAIN ROW GETS: BOTH BRANCHES DAMPED. The FB pair is
    // replaced wholesale by the SYMMETRIC element
    // (alpha, beta) = (1 - 1/sqrt(2), 1 - 1/sqrt(2)) -- the same
    // detail::kSsnDegenerateFbDeriv the kink itself uses: the row's diagonal
    // becomes -(1 + mu + sigma) instead of racing toward -mu (a hard equality)
    // or toward -2e12 (a decoupled row), and its coupling to dx keeps a
    // moderate 0.293 weight instead of 1 or ~0.
    //
    // AND IT IS STILL A GENERALIZED JACOBIAN ELEMENT, which is what keeps this
    // a semismooth Newton method rather than a heuristic: with
    // (xi, eta) = (1 - alpha, 1 - beta), the C-subdifferential of phi at the
    // kink is exactly {(1 - xi, 1 - eta) : xi^2 + eta^2 <= 1}, the symmetric
    // element sits ON that unit circle, and the row it is applied to is within
    // O(tau) of the kink by construction -- an inexact generalized Jacobian
    // with a bounded, stated error, not a branch.
    //
    // WHY THE HINT OVERRIDES THE CLASSIFICATION. A hinted row takes its hint,
    // uncertain or not: an exact-solution seed can put an active row exactly
    // at the kink (s, lambda) = (0, 0), so a classification that outranked the
    // hint would damp precisely the row the hint exists to snap onto its face.
    // The hint governs step 0 only; every step after it is classified.
```

### include/hven/drivers/sqp_types.h

1541 lines / 1328 comment lines at `1997159`; 1055 / 842 after.

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 26–46

`enum class QpMode`: the three kernels, their dispatch history and the opt-in scope. The kernel descriptions and the kWalk default rule are kept.

```text
/// Which kernel solves each SQP subproblem. kWalk is the primal active-set
/// WALK (qp_engine.h): one working-set change per minor iteration, one
/// blocking constraint at a time. kSsn is the SEMISMOOTH-NEWTON kernel
/// (ssn_engine.h): a Newton method on a Fischer-Burmeister reformulation of
/// the QP's own KKT conditions, whose step changes the whole implied active
/// set AT ONCE (the PDAS "bulk flip") -- the property whose value shows up in
/// the NUMBER of minors, not in the cost of one. kIpm is the INTERIOR-POINT
/// tier (M6 W1, `detail/qp/ipqp_engine.h`): an IP-PMM (Cipolla-Gondzio)
/// Mehrotra predictor-corrector that acquires the whole active set from an
/// interior start rather than walking or flipping onto it, then hands a
/// converged iterate to the tier-3 exact face refinement. **DISPATCHABLE
/// SINCE W1 TASK 6**, which landed the section 2.3 routing chain and removed
/// task 1's temporary "not yet dispatchable" refusal from
/// `validate_sqp_options`: selecting it runs the tier on the MAIN subproblem
/// of every major, with the SSN warm grade and the walk as its successors.
/// **OPT-IN AND DEFAULT-OFF FOR M6** (spec section 9): no pinned artifact is
/// measured on it, and the acceptance battery is W1 task 9's.
///
/// THE DEFAULT IS kWalk AND MUST STAY SO absent an explicit ruling otherwise:
/// the walk is what every pin, battery and published figure in this
/// repository was measured on.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 52–64

`QpMode::kQpModeCount`: what it catches and why the assertion is on the sentinel rather than on kIpm's value. Both kept, compressed.

```text
    /// **NOT A MODE. THE ENUMERATOR COUNT**, and the only thing in this tree
    /// that can catch a fourth kernel added without an arm in the driver's QP
    /// kernel dispatch (M6 W1 task 6 fix round 3). A `static_assert` beside
    /// that switch pins this value; appending a real mode ABOVE this line
    /// moves it and stops the build, which is what an assertion on `kIpm == 2`
    /// could not do -- a value appended after `kIpm` left it untouched.
    ///
    /// It is a legal `QpMode` value that names no kernel, so
    /// `validate_sqp_options` REFUSES it like any other out-of-range setting,
    /// and the dispatch enumerates it in an arm that throws rather than
    /// omitting it (an omission would make `-Wswitch` warn on every build, and
    /// a `default:` label would silence the warning this sentinel exists to
    /// keep alive).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 68–75

The Ssn*Rule declaration banner. Kept, compressed.

```text
// Declared HERE rather than in ssn_engine.h for the same reason QpMode is:
// SqpOptions carries them and sqp_types.h is the header ssn_engine.h
// includes, not the other way round. Their SEMANTICS live at their SsnOptions
// fields (ssn_engine.h), which is also where each one's mechanism is derived;
// this file only declares the alphabet and the defaults.
//
// **EVERY DEFAULT BELOW IS THE SHIPPED ITERATION, BIT FOR BIT**, and no
// default is flipped by the change that adds its lever.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 77–93

`enum class SsnSigmaRule`: the three rules with their references. Kept, compressed.

```text
/// HOW THE PROXIMAL/LEVENBERG-MARQUARDT SHIFT sigma IS SIZED.
///
/// - kLadder: the shipped failure-reactive ladder -- arm at 1e-6, x100 per
///   escalation trigger, cap 1e6 (ssn_engine.h's
///   detail::kSsnProxInit/Growth/Max).
/// - kResidualArmed: once the ladder has ARMED, size sigma from the residual
///   instead of climbing: sigma_k = c*min(r, r^2) with r = ||F_k||inf
///   scale-normalized, floored at the ladder's own first rung and capped at
///   its ceiling. The ladder is retained underneath as a monotone floor, so a
///   kSingular factorization still escalates and the escape route is
///   unchanged. INERT on any solve whose ladder never arms, which is every
///   benign fixture.
/// - kResidualAlways: as kResidualArmed, but sigma is sized from the residual
///   from the FIRST attempt rather than from the first arming -- the
///   proactive form the Levenberg-Marquardt theory is stated for
///   (Yamashita-Fukushima 2001, Fan-Yuan 2005). NOT inert anywhere: every
///   solve carries at least the floor.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 100–109

`enum class SsnHintRule`: the two rules with the watchdog's citation. Kept, compressed.

```text
/// WHAT PROTECTS THE HINTED FIRST STEP.
///
/// - kIterationZeroFree: the shipped rule -- iteration 0 is exempt from the
///   Armijo test when a hint governed it, with no safety net.
/// - kWatchdog: Chamberlain-Powell-Lemarechal-Pedersen (Math. Prog. Study 16,
///   1982) -- up to q relaxed steps, and if sufficient decrease has not
///   materialized by then, RETURN TO THE BEST STORED POINT and take a
///   monotone step from there. Reproduces the exemption exactly when the
///   hinted step works and closes the failure it cannot see when it does
///   not.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 115–123

`enum class SsnInfeasibilityRule`: the two rules. Kept, compressed.

```text
/// WHAT TURNS AN INFEASIBILITY SUSPICION INTO AN EXIT.
///
/// - kSymptoms: the shipped conjuncts -- a stalled residual window plus
///   diverging multipliers (ssn_engine.h's kSsnStallWindow block).
/// - kFarkasGated: the symptoms ARM the check and a CERTIFICATE fires it: the
///   dual increment is projected onto the sign cone, normalized, and tested
///   as an approximate Farkas direction (one matvec plus O(m), no
///   factorization). A symptom not accompanied by a Farkas direction no
///   longer exits.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 129–142

`struct IpqpOptions`: the declaration-site argument and the unconditional-validation rule. Both kept, compressed.

```text
/// THE `kIpm` TIER'S OWN SETTINGS (M6 W1, spec `docs/notes/2026-08-m6-w1-ipqp-spec.md`
/// section 9). Declared HERE for the same reason `QpMode` and the three
/// `Ssn*Rule` enums are: `SqpOptions` carries this struct as `ipqp`, and
/// `sqp_types.h` is the header the engine (`detail/qp/ipqp_engine.h`)
/// includes, not the reverse. Full mechanism for each field is derived at
/// the engine that consumes it, landing in later W1 tasks; this struct only
/// declares the alphabet and the shipped defaults, exactly as this file does
/// for the SSN levers. Forwarded onto the tier verbatim, as the SSN levers
/// are onto `SsnOptions`.
///
/// EVERY DEFAULT BELOW IS THE SPEC'S OWN TABLE VALUE. `qp_mode` stays `kWalk`
/// by default (above), so nothing here is reachable in M6 until a caller
/// opts in -- but the fields are VALIDATED UNCONDITIONALLY at every mode,
/// because a field is out of range whether or not this solve will read it.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 150–158

`ipqp_hard_iter_cap`: the last-resort reading and the per-attempt scope. Both kept, compressed.

```text
    /// The hard cap of LAST RESORT (spec 3.4/6.1's `IpqpEscape::kBudget`):
    /// fires only when the early-stall detector (6.2) should have fired
    /// first but did not. Unlike `ipqp_max_iter` this is not a
    /// size-derived budget and has no sentinel reading, so it must be a
    /// genuine, positive iteration count. Default 60. Must be > 0.
    ///
    /// PER ATTEMPT, not per subproblem: the section 5.5 warm-kill re-bases
    /// both caps at the cold restart, so an abandoned attempt may total up
    /// to 2x this value. `.superpowers/w1-t7-report.md` FIX ROUND 2.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 161–174

`ipqp_max_factorizations`: the sentinel's derivation and the per-attempt scope. Both kept, compressed.

```text
    /// Factorization budget for the tier, enforced BEFORE EVERY
    /// factorization -- ladder rungs and the section 2.2 item 4 final
    /// inertia read included, not merely once per iteration. `<= 0` is a
    /// SENTINEL meaning "3 x the tier's EFFECTIVE ITERATION BUDGET", which is
    /// `min(effective ipqp_max_iter, ipqp_hard_iter_cap)` and therefore 180
    /// at the shipped defaults -- NOT 3 x the size-derived `ipqp_max_iter`
    /// (M6 W1 task 4 fix round 1, M3: this row previously described the
    /// wrong one of the two). Three per iteration is the ladder headroom the
    /// sentinel grants before it calls a solve pathological; one iteration
    /// costs one factorization plus its rungs. Every `Index` value is legal
    /// and nothing here is validated, exactly like `ipqp_max_iter` above.
    /// PER ATTEMPT for the same reason `ipqp_hard_iter_cap` is: a warm-killed
    /// subproblem may total up to 2 x this value, and every factorization is
    /// charged to the driver's probe budget either way.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 177–185

`ipqp_init_mu`: the two readings and the validation. Both kept, compressed.

```text
    /// The COLD starting barrier parameter (spec 5.6: `mu_0 = ipqp_init_mu`
    /// on a cold start) and the CEILING of the warm-restart clamp (spec 5.3:
    /// `mu_0 = clamp(max(mu_meas, kappa_mu * mu_payload), ipqp_min_mu,
    /// ipqp_init_mu)` on a warm one) -- a SETTING in both readings, never
    /// overwritten by payload evidence, only clamped against it. Default 1e-2,
    /// the Q4 sweep's measured winner and RUIZ-SCALED (measured with the
    /// shipped equilibration on): docs/notes/data/2026-08-m6-w1-acceptance/.
    /// Must be finite, > 0, and >= `ipqp_min_mu` (the clamp band below is
    /// otherwise inverted).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 195–217

`ipqp_rho_init`: the schedule, the spec-text divergence recorded as an amendment, and the band. The schedule and the band are kept.

```text
    /// The (rho, delta) proximal regularization schedule's INITIAL values
    /// (spec 3.2): `rho_0 = ipqp_rho_init`, `delta_0 = ipqp_delta_init`,
    /// Cipolla-Gondzio's own choice (arXiv:2205.01775). (Spec 3.2's OWN TEXT
    /// literally reads "rho_0 = delta_0 = ipqp_rho_init" -- both from the ONE
    /// field -- while its section 9 table lists `ipqp_rho_init` and
    /// `ipqp_delta_init` as two separate settings. The table governs: it is
    /// the settings-surface contract this file implements, the two fields
    /// default to the same value so nothing observable changes today, and
    /// the divergence is recorded as a dated spec-text amendment, plan
    /// section 7 note (g).) `rho`/`delta` THEMSELVES enter the regularized
    /// KKT matrix's diagonal blocks (`H + rho I` on the primal block, `-delta
    /// I` on each dual block -- spec 3.1's Newton system); it is the
    /// PROXIMAL CENTERING terms `+ rho (x - zeta)` and `- delta (y -
    /// lambda_est)` that enter the right-hand side
    /// (`detail/globalization/inertia_regularization.h` supplies the shift
    /// mechanism; W1 supplies the estimates `zeta`/`lambda_est` this
    /// schedule updates). Default 8.0 each.
    ///
    /// `ipqp_rho_init` must be finite and lie in `[ipqp_reg_floor,
    /// ipqp_reg_max]` -- the starting value of a quantity the ladder only
    /// ever moves within that band must itself start inside it (spec 2.2's
    /// monotone floor was stated for `rho`; M6 W1 T4b deleted that floor,
    /// and the band survives it because it is the SCHEDULE's band).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 219–226

`ipqp_delta_init`: the relaxed band and its argument. Both kept, compressed.

```text
    /// The dual-block counterpart of `ipqp_rho_init` immediately above, same
    /// schedule. RELAXED relative to `ipqp_rho_init`'s own band: spec 2.2's
    /// monotone floor was stated for `rho` only (and M6 W1 T4b deleted it
    /// outright), so `ipqp_delta_init` need
    /// only be finite and in `(0, ipqp_reg_max]` -- positive (a
    /// non-positive delta would not regularize the dual block at all) and no
    /// larger than the ceiling every regularized quantity in this schedule
    /// shares, but not tied to `ipqp_reg_floor`. Default 8.0.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 259–270

`ipqp_face_kappa`: the ratio rule and the strict-(0,1) argument. Both kept, compressed.

```text
    /// The RATIO threshold `kappa` of the section 2.3 face-classification
    /// rule: row `j` is active iff `s_j < kappa * z_j` and INACTIVE iff the
    /// REVERSE holds by the same ratio, i.e. `z_j < kappa * s_j` (scale-free
    /// by construction, which is the entire point of a ratio rule over an
    /// absolute one). Default 1e-2. Must be finite and lie STRICTLY inside
    /// (0, 1): a non-positive kappa can never classify anything active, and
    /// at kappa >= 1 the two tests are not mutually exclusive -- `s_j ==
    /// z_j` would satisfy `s_j < kappa * z_j` (active) AND `z_j < kappa *
    /// s_j` (inactive) simultaneously the moment kappa exceeds 1, and at
    /// exactly 1 satisfies neither test's STRICT inequality but the two
    /// tests still coincide at equality, leaving no room for the routing
    /// chain's UNCERTAIN class the rule is built to carve out.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 294–302

`ipqp_warm_iter_budget`: the clamp and the legal-zero reading. Both kept, compressed.

```text
    /// The WARM-KILL rule's (spec 5.5) iteration budget: a warm-started
    /// solve overrunning this many iterations is restarted COLD exactly
    /// once. THE EFFECTIVE BUDGET IS CLAMPED (a dated spec section 9
    /// amendment, plan section 7 note c): `min(ipqp_warm_iter_budget,
    /// effective ipqp_max_iter)`, so a caller-chosen value larger than the
    /// solve's own iteration budget can never itself become the binding
    /// limit. Default 15, approximately the measured cold median. Must be
    /// >= 0 (0 is a legal, if extreme, choice: every warm restart is killed
    /// on its first iteration).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 350–382

The two full-step watchdog constants: their derivations, including the measured perturbed-HS7 residual sequences at eps = 1 and eps = 10. The thresholds, their arguments and the behaviour-change warning are kept; the sequences are here.

```text
// THE FULL-STEP-FIRST WARM RULE'S TWO CONSTANTS. Both are WATCHDOG thresholds:
// they bound how long the driver may keep taking undamped steps under
// globalization.h's full-step mode before restoring the best iterate it saw
// and handing the solve back to the funnel. Neither is a paper constant --
// [KD] (globalization.h's THE FULL-STEP MODE note) argues for the rule and
// for a watchdog fallback but gives no schedule, exactly as [KLV] gives none
// for its own alpha_min -- so both are implementation choices, chosen against
// what the residual sequence of a healthy warm solve actually looks like on
// this project's own fixtures.
//
// kWarmResidualGrowthMax = 2 (CONSECUTIVE majors with a GROWING ||KKT||inf).
// One growth is not evidence: a warm SQP step routinely overshoots once and
// then contracts quadratically. MEASURED on the perturbed-HS7 family of
// tests/sqp/test_warm_start.cpp, warm from HS7's own solution: at eps = 1 the
// residual sequence 1.0 -> 2.1e-1 -> 3.8e-3 -> 1.3e-6 is monotone, while at
// eps = 10 the same family goes 1.0e1 -> 6.7 -> 7.1 (a rise) -> 2.1 -> ...
// and still converges in 7 majors. A single-growth threshold would abort that
// run for nothing. TWO IN A ROW is the smallest count no single overshoot can
// produce, and every count above it buys a full major spent moving AWAY from
// a solution.
//
// kWarmFullStepWindow = 5 (majors without a NEW BEST ||KKT||inf). The other
// failure shape: not divergence but a stall -- unit steps that neither
// improve on the best iterate nor grow monotonically (a cycle, or a
// repeatedly routed QP failure at an iterate that never moves). Five is one
// order of magnitude above the local regime this mode targets: a warm start
// inside a nearby solution's Newton domain converges in 1-2 majors and cannot
// spend five without a new best, so the window can only fire on a run the
// mode's own premise has already failed for.
//
// CHANGING EITHER IS A BEHAVIOUR CHANGE ON EVERY WARM SOLVE, not a tuning
// detail: tests/sqp/test_warm_start.cpp pins the majors of both a converging and
// a watchdog-restored run against these exact values.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 392–412

`kWeakActivityMargin`: the constant-not-a-setting ruling, what it measures, and why it is not the SSN's uncertain set. All kept, compressed.

```text
/// Relative margin below which an active row's multiplier -- or an inactive
/// row's slack -- counts as weak/near activity in the per-major census
/// (`SqpIterate::weak_active_rows`, `near_active_rows`).
///
/// A CONSTANT, NOT A SETTING (M6 W4 T3). Nothing in this library reads or
/// branches on either count, so there is no behaviour for a caller to tune; a
/// future heuristic that DOES act on them is what would promote it.
///
/// WHAT IT MEASURES: strict complementarity, BY MAGNITUDE. A row is weakly
/// active when it is in the working set at a price negligible beside the
/// largest one, and nearly active when it is out of the working set at a slack
/// negligible beside the largest one -- `min(s_k, lambda_k)` small in a
/// RELATIVE sense, on both sides of the pair.
///
/// IT IS NOT THE SSN's UNCERTAIN SET, and the two are not interchangeable.
/// That set is the dimensionless kink band `|(lambda - s)/rho| <=
/// kSsnUncertainEnter` with leave hysteresis (ssn_engine.h), which reads a
/// PURE state at `s == 0` for any lambda and so never flags a small multiplier
/// on an active row. The two readings coincide only where both halves vanish
/// -- which is exactly the tie fixture, and is why both flag it there for
/// different reasons.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 415–511

`struct SqpOptions`: the tolerances, trust region, radius floor, max_iter, globalization strategy, `qp`, second-order correction and adaptive dual regularization notes — including the measured 79% kInfeasible SOC re-solve rate and the ~1e-4 relative error. Every contract is kept in the rewritten banner; the measurements are here.

```text
/// Driver options for the whole SQP solve.
///
/// TOLERANCES. kkt_tol gates the STATIONARITY measure and feas_tol gates the
/// FEASIBILITY measure; both measures are defined precisely in sqp_driver.h's
/// CONVERGENCE TEST note, which is the contract. feas_tol does double duty as
/// the geometric BOUND-ACTIVITY tolerance of that measure (a variable within
/// feas_tol of a bound is treated as sitting on it), so there is no separate
/// activity-tolerance knob -- see that note for why the two are deliberately
/// the same number.
///
/// TRUST REGION. tr_init is the l-infinity radius the FIRST subproblem of a
/// solve is given, through SolveOverrides::tr_radius (qp_types.h); from there
/// the driver's radius-management loop takes over (sqp_driver.h's RADIUS
/// MANAGEMENT note) -- doubling it on a strong accepted step up to the tr_max
/// ceiling, halving it on a rejected one, and never below the tr_min FLOOR.
/// tr_max must be >= tr_init. Setting tr_init to +inf resolves, per
/// SolveOverrides' sentinel convention, to `qp.tr_radius` (itself +inf by
/// default), i.e. no trust region -- legitimate, but an indefinite
/// subproblem is then generally unbounded and the engine reports
/// kNumericalError rather than a step. THE FIRST SHRINK FROM +inf RESOLVES TO
/// tr_max (inf/2 is inf, so the shrink rule needs a finite landing value the
/// first time it fires), after which ordinary halving applies and the solve
/// behaves exactly like one started at tr_max (sqp_driver.h's RADIUS
/// MANAGEMENT note explains why tr_max is the landing value). A solve that
/// never rejects a trial still never materializes a radius at all, so the
/// "no trust region" reading of +inf is intact wherever it was meaningful.
///
/// tr_min IS THE RADIUS FLOOR, and reaching it is an EVENT rather than a
/// clamp: KLV Algorithm 4's restoration trigger is "alpha < alpha_min (step
/// size too small)", whose trust-region analogue is exactly this, so a shrink
/// that would go below tr_min instead enters the RESTORATION PHASE
/// (sqp_driver.h). The radius is therefore never left spinning at the floor.
///
/// THE DEFAULT tr_min (1e-10) IS AN IMPLEMENTATION CHOICE, like alpha_min
/// itself, which KLV also leaves without a value. It is chosen against two
/// requirements: far enough below the tolerances that it can never pre-empt a
/// legitimately small step (with kkt_tol/feas_tol at 1e-6, 1e-10 is four
/// orders below the resolution at which any residual is judged), and far
/// enough below tr_init that reaching it is evidence rather than noise (from
/// tr_init = 1 it takes 34 consecutive halvings, i.e. 34 rejections at ONE
/// iterate, which no healthy solve produces). A caller whose problem is
/// scaled so that meaningful steps are smaller than 1e-10 should lower it;
/// one who wants restoration entered sooner should raise it.
///
/// MAX_ITER BOUNDS SUBPROBLEMS SOLVED, not accepted steps -- a rejected trial
/// costs one, exactly like an accepted one, because it costs a QP solve. THE
/// BUDGET IS SHARED WITH THE RESTORATION PHASE: the quantity bounded is
/// major_iters + restoration_iters, not major_iters alone, because a
/// restoration major costs a QP solve on the feasibility problem exactly as
/// an optimality major costs one on the subproblem. A solve that spends 12
/// majors restoring has 12 fewer available to the main loop, and the total
/// work of a solve is bounded by max_iter subproblems however it is split.
/// history.size() still tracks major_iters ALONE (restoration produces no
/// history rows of its own -- see SqpIterate), so history.size() ==
/// major_iters + 1 on an iterate exit is unchanged.
///
/// GLOBALIZATION STRATEGY. make_strategy is called ONCE PER solve() call and
/// must return a non-null, freshly resettable GlobalizationStrategy
/// (globalization.h); the driver owns the returned object for that solve, so
/// two solves never share funnel state and solve() stays repeatable. Empty
/// (the default) means FunnelStrategy -- KLV's funnel, this project's default
/// globalization. A factory that returns nullptr is a caller error and
/// solve() throws std::invalid_argument.
///
/// qp is copied into the driver's single QpEngine instance at construction,
/// so per-solve variation goes through SolveOverrides, never through this
/// struct (see qp_types.h's PER-INSTANCE, CONST note on QpOptions::tr_radius).
///
/// SECOND-ORDER CORRECTION. enable_soc defaults ON: it is this project's
/// cheap edge over Uno, which omits SOC entirely. When a kReject trial's
/// constraint violation INCREASED (h_new > h_old -- the Maratos signature;
/// sqp_driver.h's SECOND-ORDER CORRECTION note), the driver spends one extra
/// hot-started QP re-solve before shrinking the radius, to try to rescue the
/// step from a pure curvature artifact rather than discard it.
///
/// "NEAR-FREE" IS SCOPED TO THE DESIGN REGIME -- a small residual near a
/// solution (the Maratos regime this feature targets), where the re-solve is
/// measured to cost 0 extra factorizations (SqpDriverSoc.SocDefeatsMaratos).
/// Far from a solution -- a large residual, the common case when this fires
/// incidentally rather than by design -- it is a REAL, full extra QP solve
/// that MOST OFTEN (measured: 79% on this file's own pre-existing suite)
/// returns kInfeasible outright: see sqp_driver.h's A FAILED SOC RE-SOLVE
/// note for the measured cost table and the reason (the rhs shift scales with
/// the very violation that triggered SOC, so a large h_new brings a large,
/// often box-busting correction target). Every attempt is still bounded and
/// cheap-to-FAIL (a handful of minor iterations, at most a few factorizations,
/// one attempt per rejected trial), which is why no magnitude gate is applied
/// before attempting.
///
/// ADAPTIVE DUAL REGULARIZATION. adaptive_mu defaults ON: it schedules
/// dual_mu down with the KKT residual, closing the accuracy ceiling a fixed
/// engine dual_mu leaves on a badly-scaled active set (iterative refinement
/// alone leaves measured relative error ~1e-4 -- see sqp_driver.h's ADAPTIVE
/// DUAL REGULARIZATION note and tests/sqp/test_sqp_driver.cpp's
/// AdaptiveMuRecoversTailAccuracy). primal_delta is NOT scheduled by this
/// lever or by anything else in the driver; see that same header note for why
/// the schedule is deliberately dual_mu-only.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 562–571

`adaptive_mu`: the lever and the measured error it closes. The lever is kept.

```text
    /// Adaptive dual regularization A/B lever: when ON, schedules
    /// SolveOverrides::dual_mu down with the KKT residual instead of leaving
    /// every subproblem at the engine's fixed QpOptions::dual_mu, which is
    /// what closes the fixed-mu accuracy ceiling on a badly-scaled active
    /// set (measured relative error ~1e-4). Default true. Set false to
    /// recover exact engine-default-mu behaviour -- SolveOverrides::dual_mu
    /// is then left at its sentinel every major. primal_delta is NOT
    /// scheduled by this lever or by anything else in the driver. See the
    /// ADAPTIVE DUAL REGULARIZATION note above (and sqp_driver.h's, for the
    /// schedule itself).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 589–612

`start_level`: the ceiling's effect at each level and what each setting is for. All kept, compressed.

```text
    /// A CEILING on the level solve(model, x0, warm) (the 3-arg overload,
    /// sqp_driver.h's WARM-START INGEST note) is allowed to resolve to,
    /// independent of what `warm` itself would otherwise justify -- the same
    /// kind of A/B lever enable_soc/adaptive_mu already are.
    ///
    /// At StartLevel::kSeeded a driver ingests a hash-less object's values
    /// (warm_start.h's StartLevel note) but never a factorization, never the
    /// funnel/TR state and never the Kungurtsev-Diehl window, EVEN when the
    /// object would have earned kWarm. It also short-circuits the
    /// structural-hash resolution probe entirely -- the probe's answer could
    /// only raise the level above the ceiling, so it is never paid.
    ///
    /// Default StartLevel::kWarm: kHot is reachable -- a `warm` carrying a
    /// non-null `hot` handle whose structure matches resolves there -- but
    /// is opt-in, not the default, since it hands this instance's engine a
    /// factorization possibly built by a DIFFERENT engine instance (see
    /// qp_engine.h's HotState OWNERSHIP note for the single-use,
    /// chained-hand-off invariant that comes with accepting one). Raise this
    /// to StartLevel::kHot to let the resolution reach it; set it to
    /// StartLevel::kCold to force EVERY solve through the 3-arg overload to
    /// behave exactly as the 2-arg one does, regardless of `warm` -- e.g.
    /// for an A/B comparison of warm vs. cold on the same problem sequence.
    /// Does NOT affect solve(model, x0): that overload is always cold by
    /// construction (there is no `warm` object to resolve).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 615–637

`warm_full_step`: the rule, the A/B lever reading, the three engagement conditions and the never-certifies rule. All kept, compressed.

```text
    /// The KUNGURTSEV-DIEHL FULL-STEP-FIRST RULE: when ON, a solve whose
    /// warm-start level RESOLVED to kWarm or above (never a cold one, and
    /// never the 2-arg solve overload) begins in globalization.h's
    /// full-step mode -- unit steps, the funnel test bypassed -- with a
    /// WATCHDOG that restores the best iterate by ||KKT||inf and hands the
    /// solve back to ordinary funnel globalization the moment the residual
    /// grows kWarmResidualGrowthMax majors in a row, or kWarmFullStepWindow
    /// majors pass without a new best (both constants above). Default true:
    /// [KD]'s observation that standard globalization actively INTERFERES
    /// with warm starts is the reason the warm-start subsystem exists at
    /// all, and a warm solve that re-globalizes from scratch forfeits most
    /// of the advantage warm-starting buys.
    ///
    /// THE A/B LEVER, exactly like enable_soc/adaptive_mu: set false and
    /// every warm solve behaves precisely as with the mode off (the funnel
    /// judges the first trial like any other).
    ///
    /// IT CANNOT AFFECT a cold solve, a solve whose `warm` failed to
    /// resolve, or a solve running a caller-supplied make_strategy that is
    /// not a FunnelStrategy -- the mode is that class's own state
    /// (sqp_driver.h's FULL-STEP-FIRST WARM RULE note has all three
    /// engagement conditions). NOR DOES IT EVER CERTIFY ANYTHING: it changes
    /// which trials are ACCEPTED, never the KKT test that decides kOptimal.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 640–681

`budget_mode`: the ordering, why it is not min-KKT, the default and the scope. All kept, compressed.

```text
    /// BUDGETED MODE: when true, a solve that exhausts max_iter in the MAIN
    /// optimality loop (the "converged" test never fires and iter +
    /// restoration_iters reaches max_iter) reports
    /// SqpStatus::kBudgetExhausted rather than kMaxIter, and
    /// SqpSolution::x/lambda_e/lambda_i/z/f are the BEST ITERATE THIS SOLVE
    /// VISITED BY THE FUNNEL'S OWN ORDERING -- feasibility-first: min h(x)
    /// (violation_l1), tie-break min f -- rather than the last iterate
    /// reached. warm_start is populated from that same best iterate rather
    /// than the last one FOR x/lambda_e/lambda_i/z AND THE ACTIVITY VECTORS
    /// ONLY; its OWN globalization/regularization fields -- tr_radius,
    /// funnel_width, primal_delta, dual_mu -- describe the LAST pass this
    /// solve measured (the one at which max_iter was hit), NOT the best
    /// iterate's own row (sqp_driver.h's BUDGETED MODE note has the
    /// mechanics).
    ///
    /// WHY THIS ORDERING AND NOT MIN-||KKT||inf (the full-step watchdog's
    /// own ordering): the two exist for different consumers asking different
    /// questions. The watchdog asks "which iterate is closest to a KKT
    /// point", because that is exactly what the convergence test it protects
    /// gates on. Budgeted mode asks "what point should a CONTINUATION DRIVER
    /// pick up from" -- and the next solve's own globalization has to
    /// re-earn feasibility from scratch regardless of where it starts, so a
    /// point with a small stationarity residual but a large constraint
    /// violation is a WORSE hand-off than a clearly feasible point with a
    /// mediocre objective: it is exactly the feasible-ish starting position
    /// a warm start exists to provide. A min-KKT choice can therefore hand
    /// back a point the funnel itself would refuse to accept a step from --
    /// tests/sqp/test_warm_start.cpp's BudgetReturnsUsableIterate is built on a
    /// fixture where the two orderings disagree for exactly this reason.
    ///
    /// DEFAULT FALSE: off reproduces plain kMaxIter-at-the-last-iterate
    /// behaviour byte-identically. The same kind of A/B lever
    /// enable_soc/adaptive_mu/warm_full_step already are.
    ///
    /// SCOPE: this lever governs ONLY the main loop's own max_iter
    /// exhaustion (the "stopped AT an iterate" exit family, SqpCounters'
    /// note). It does NOT change what happens when the RESTORATION PHASE
    /// itself runs out of the shared budget mid-restoration
    /// (sqp_driver.h's RESTORATION PHASE note, the "no budget left to
    /// restore with" exit) -- that stays kMaxIter regardless of this flag,
    /// because it answers a different question ("restoration could not
    /// finish"), not "here is a usable point to continue from".
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 684–766

`elastic_ladder_early_exit`: the safe and unsafe classes with their measured drifts, the two corpus trajectories (HS15's 86->63 majors and HS10's identical-with-a-6x-cut), and the when-safe-to-turn-on rule. The classes and the rule are kept; the measurements are here.

```text
    /// THE ELASTIC LADDER'S STALL EARLY-EXIT, OPT-IN: when ON, the rho
    /// escalation ladder (sqp_driver.h's THE ELASTIC TIER note,
    /// kElasticRhoInit -> kElasticRhoMax) stops the moment one escalation
    /// leaves the augmented solution within kElasticStallScale (a
    /// NUMERICAL-ZERO tolerance, not a literal bit-for-bit test) of where
    /// the PREVIOUS rung left it, instead of always spending all six.
    /// Default FALSE -- OFF is the default and reproduces the driver's
    /// behaviour without the lever EXACTLY, everywhere (the
    /// crash_basis pattern, not the enable_soc one).
    ///
    /// WHY OFF BY DEFAULT, because the alternative was tried and measured
    /// unsafe as a default. The obvious argument for "one repeat is enough
    /// evidence to stop" is that ONLY g's slack block reads rho, so a rung
    /// whose solution repeats the previous one looks rho-invariant.
    /// MEASURED, this is closer to "the drift stays small" than to "the
    /// corner never moves": on SqpDriverElastic.InconsistentLinearizationRecovers
    /// the rungs are NOT bit-for-bit identical -- the solution drifts
    /// (|dx|inf 3.6e-13 at rho 1e2->1e3, growing roughly 10x per escalation
    /// to 3.6e-8 by rho 1e7->1e8) and the working set changes at rung 2 (one
    /// slack column flips from a real bound to free) -- but the drift the
    /// WHOLE ladder would accumulate stays four orders of magnitude below
    /// feas_tol/kkt_tol's default 1e-6, and the FIRST escalation's drift is
    /// already sub-threshold, which is why the exit fires there safely. That
    /// safety margin comes from most of the relaxed slacks being pinned at a
    /// REAL BOUND (the fixtures' hand-derivations say so explicitly: "p0
    /// runs to its bound for every rho >= 1"), which bounds how much of the
    /// reduced system CAN read rho at all -- not a proof that it reads none
    /// of it.
    ///
    /// It is UNSAFE in general, with a concrete counter-example in this
    /// project's own suite: tests/sqp/test_sqp_restoration.cpp's
    /// InfeasibleCircleLineModel, whose elastic relaxation's augmented
    /// objective is EXACTLY CONSTANT on the ENTIRE feasible set at every rho
    /// (Lagrangian Hessian identically zero, and the unrelaxed row forces
    /// the relaxed row's residual to be identically 1 on the feasible set --
    /// see sqp_driver.h's STALL EARLY-EXIT note for the derivation). No
    /// point on that set is ever more optimal than any other, at any rho, so
    /// a later rung is not finding progress a one-repeat test missed -- it
    /// is an O(1) tie-break getting lost against rho's growing scale in the
    /// engine's own arithmetic (the SAME mechanism
    /// ElasticSurvivesALargeConstraintScale's banner documents for the scale
    /// knob S). Traced by hand at one of its own majors: rungs 0-4 (rho =
    /// 1e2..1e6) agree to a few ULP and rung 5 (rho = 1e7) moves to a
    /// different point of the same flat set. A caller stopping after rung 1
    /// was not wrong that no further optimality progress existed -- none
    /// did, at any rho -- but they ended up at an ARBITRARY point among many
    /// equally optimal ones, and that choice is not stable under this lever.
    /// MEASURED on the corpus this fixture's class comes from
    /// (tests/sqp/support/hs_sweeps.h's HS15, cold arm): turning this lever
    /// on reshapes the TRAJECTORY -- majors 86 -> 63, elastic_activations
    /// 48 -> 27, accepted/rejected 70/16 -> 49/14 -- because an early
    /// tie-break returning a different point propagates into different
    /// subproblems for the rest of the solve. The corpus's OTHER
    /// elastic-active problem, HS10, is the opposite case: majors,
    /// accept/reject split and objective are IDENTICAL at every grid point
    /// with the lever on, for a free 6x escalation cut (336 -> 56) and 560
    /// fewer minor iterations -- so that corpus is one cost-only case and
    /// one reshaped case, not two reshaped cases. Every configuration
    /// measured, on every fixture class, still reaches a CORRECT final
    /// answer (no wrong kOptimal, no wrong certificate), and every measured
    /// trajectory change was in the IMPROVING direction -- so the case for
    /// off is "unpredictable which arbitrary optimum you get", not "measured
    /// harm" anywhere. That is nonetheless not the "cost only, ever"
    /// property needed to justify an on-by-default flip, and no cheap,
    /// general runtime test tells the two fixture classes apart:
    /// QpSolution::bound_state on the slack columns does NOT do it (both
    /// fixtures have a FREE slack column at the rung the decision is made
    /// on -- see sqp_driver.h's STALL EARLY-EXIT note for what a future
    /// change would need to establish before flipping the default, and for a
    /// Hessian-based lead offered instead).
    ///
    /// WHEN IT IS SAFE TO TURN ON: a problem whose elastic relaxation's
    /// relevant slacks are pinned at a REAL bound for the rho range in play
    /// -- hand-derivable the way InconsistentLinearizationModel's and
    /// InconsistentBoundedModel's are, where it is MEASURED to cut the
    /// escalation count 6 -> 1 and 12 -> 2 across the two activations of
    /// RhoEscalationIsBoundedAndSignals with certified outcomes
    /// byte-identical except an informational SqpIterate::violation_l1
    /// reading that can move by ~1e-13 (four to five orders below
    /// feas_tol/kkt_tol's default 1e-6). On a flat-objective (H === 0-like)
    /// relaxation, turning it on is a DELIBERATE trajectory choice, not a
    /// free cost cut -- leave it off unless the caller has checked which
    /// class their own problem falls in.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 769–840

`crash_basis`: the two predicates, the no-new-constant argument, the rejected wider threshold, the cold-only scope, the no-extra-evaluation property, and the measured corpus outcome (F7's 2 of 850 minors, the five bounds across HS30/HS33/HS45, HS25's zero-major case). Every rule is kept; the measurements are here.

```text
    /// THE CRASH BASIS, OPT-IN: when ON, a COLD solve seeds its FIRST QP
    /// subproblem's working set from the activity geometry at x0 -- every
    /// inequality row that is geometrically active there, and every variable
    /// sitting on a finite bound there -- instead of handing the engine an
    /// empty working set and making it rediscover them one ratio test at a
    /// time. Default FALSE, and OFF is byte-identical to the driver's
    /// behaviour without the lever, everywhere (the
    /// elastic_ladder_early_exit pattern, not the enable_soc one).
    ///
    /// THE PREDICATES ARE NOT NEW, AND THAT IS THE WHOLE POINT OF THE
    /// TOLERANCE STORY. Both are the tests this driver ALREADY applies at
    /// every convergence check, at the SAME tolerance
    /// (SqpOptions::feas_tol), reused verbatim rather than re-derived:
    ///
    ///     row j seeded      :=  cI_j(x0) >= -feas_tol
    ///                           (evaluate_kkt's geometric-activity test,
    ///                            sqp_driver.h's INGESTED MULTIPLIERS ARE MADE
    ///                            COMPLEMENTARY note)
    ///     var i at lower    :=  x0(i) - l(i) <= feas_tol,  l(i) finite
    ///     var i at upper    :=  u(i) - x0(i) <= feas_tol,  u(i) finite
    ///                           (evaluate_kkt's at_lower/at_upper, the
    ///                            reduced-stationarity measure's own test)
    ///
    /// So there is NO new constant to calibrate and no second notion of
    /// "active" in the driver: a row the crash basis seeds is exactly a row
    /// the convergence test would price a multiplier on at that point, and a
    /// bound it pins is exactly a bound the stationarity measure would treat
    /// as active there. A variable that satisfies BOTH tests without being
    /// literally fixed is seeded kAtLower (the engine's own start_center
    /// already flips l == u to kFixed before this seed is ingested, so the
    /// only reachable case is a box narrower than 2*feas_tol; the choice is
    /// arbitrary and recorded so it is not mistaken for a derivation).
    ///
    /// A MORE GENEROUS THRESHOLD WAS REJECTED ON EVIDENCE, not on taste. The
    /// QP engine's Dantzig drop rule deliberately SKIPS shifted
    /// (homotopy-admitted) rows, but it does NOT skip a crash-seeded one --
    /// so a row seeded that should not have been can only leave through a
    /// drop, i.e. an OVER-GENEROUS crash basis costs strictly more than no
    /// crash basis at all, while a tight one is at worst free. The threshold
    /// therefore sits exactly at the driver's own definition of active and
    /// no wider.
    ///
    /// COLD ONLY, BY CONSTRUCTION. The seed is built at the first subproblem
    /// of a solve whose resolved start level is kCold, and only when no warm
    /// seed exists; a kWarm/kHot ingest already seeds that same working set
    /// from WarmStart::qp_working_set and this lever cannot touch it.
    /// tests/sqp/test_sqp_driver.cpp's CrashBasisIsInertOnAWarmIngest pins that
    /// a warm solve is bit-identical with the lever on and off.
    ///
    /// NO EXTRA MODEL EVALUATION. Both predicates are read off the FIRST
    /// SUBPROBLEM ITSELF -- build_subproblem sets qp.bi = -cI(x) and
    /// qp.lower/qp.upper = model bounds - x, so `cI_j(x0) >= -feas_tol` is
    /// `qp.bi(j) <= feas_tol` and the two bound tests are
    /// `qp.lower(i) >= -feas_tol` / `qp.upper(i) <= feas_tol` -- never from
    /// a fresh eval_ci call. See sqp_driver.h's crash_basis_seed().
    ///
    /// MEASURED OUTCOME, AND WHY THE DEFAULT IS OFF. On this project's two cold
    /// corpora the lever is close to inert, for a reason that is a MECHANISM
    /// rather than a tuning miss -- on F7 the first subproblem costs 2
    /// minors of 850 (all the identification happens on the SECOND major,
    /// where the engine's own homotopy already supplies a seed within 1.06x
    /// of |W*| at zero minor cost), and no F7 or Hock-Schittkowski start
    /// point has an inequality row within feas_tol of its boundary that the
    /// homotopy would not have admitted anyway. It moves counters only
    /// where the start point genuinely sits on constraints, which the note's
    /// own fixture demonstrates and 5 variable BOUNDS across 3
    /// Hock-Schittkowski problems (HS30, HS33, HS45) exercise. (HS25's
    /// published start point satisfies the bound predicate too, but it
    /// converges in ZERO majors, so no subproblem is ever built for a seed
    /// to be offered to and its counter reads 0;
    /// tests/sqp/test_hs_battery.cpp's corpus A/B asserts the 5, and exercises
    /// HS25 as the "never builds a subproblem" case.)
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 858–905

`ssn_prox_carry`: what it gates, the 23-row sweep with its per-column tallies, the counter-fix that corrected the factorization column's sign, the mechanism, and the rejected second export policy. The gate, the default's reason and the mechanism are kept; the sweep is here.

```text
    /// READ THE PROXIMAL CARRY OFF AN INGESTED WarmStart.
    ///
    /// WHAT IT GATES, AND ONLY IT: whether the FIRST SSN subproblem of a
    /// solve starts its proximal ladder at `WarmStart::prox_sigma` instead
    /// of at 0. The EMISSION side is unconditional and this flag does not
    /// touch it -- a solve always exports what its ladder found, so a caller
    /// can measure the carry without first turning it on. Under
    /// `qp_mode == QpMode::kWalk` the flag is inert in both directions (no
    /// SSN subproblem is solved, and `has_prox_center` is never set on the
    /// object such a solve emits).
    ///
    /// **THE DEFAULT IS OFF BECAUSE THE MEASUREMENT SAYS SO**, not because
    /// the mechanism is unfinished: a lever whose sweep is a null with a
    /// negative tail does not become a default. The sweep -- 7 cells of a
    /// parametric indefinite family (IndefiniteBoxModel at c = 0.25, 0.5,
    /// 0.75, 1.0, 1.5, 2.0, 3.0) plus every Hock-Schittkowski problem whose
    /// ladder arms at all, 23 rows in total, each run with the carry off and
    /// on -- reads: `ssn_prox_updates` DOWN on 2 rows (HS27 7 -> 0, HS11 6
    /// -> 5), UP on 1 (HS15 7 -> 13), unchanged on 20; `ssn_escapes` UP on
    /// 17 of 23 rows and DOWN ON NONE; total factorizations UP on 13 rows,
    /// down on 5, unchanged on 5. TWO of the 23 rows are committed as tests
    /// (tests/sqp/test_sqp_driver.cpp's TheProximalCarryLeverMovesTheLadderAndIsOff-
    /// ByDefault (HS27) and TheProximalCarryHasACommittedCounterExample
    /// (HS15)), deliberately the row that moves and the row that moves the
    /// wrong way; the remaining 21 rows are an un-committed probe.
    ///
    /// THE FACTORIZATION COLUMN'S SIGN WAS CORRECTED BY A COUNTER FIX, worth
    /// stating rather than quietly replacing: an original narrower read of
    /// "factorizations DOWN on 9 rows, up on 1" was measured against a
    /// counter that did NOT count an ESCAPED SSN subproblem's own
    /// factorizations at all (they reached no accumulation site -- see
    /// sqp_driver.h's hand-off note), so the arm that escapes more looked
    /// cheaper for the arithmetic reason that its escapes were free.
    /// Charged honestly, the carry is not cheaper: it escapes more on 17 of
    /// 23 rows and costs MORE factorizations on 13.
    ///
    /// THE MECHANISM is what the escape row always said: starting a
    /// subproblem at a large sigma damps its Newton step toward the current
    /// iterate, so the SSN tier stops contracting sooner and hands the
    /// subproblem to the WALK -- i.e. the carry's dominant measured effect
    /// is to DISABLE the kernel the SSN mode exists to move work onto.
    ///
    /// A second export policy was also measured and rejected: carrying only
    /// a sigma an SSN subproblem CERTIFIED at (rather than the max over the
    /// solve, which is dominated by exhausted ladders that escaped anyway).
    /// Strictly better motivated, strictly less useful -- it leaves 10 of
    /// the 13 factorization-negative rows with nothing to carry, and on the
    /// 3 that survive it costs 1-2 factorizations rather than saving any.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 908–938

`ssn_certify_from_face`: the two factorizations, Gould's lemma, the fallback on a refusal and the unit-predictable saving. All kept, compressed.

```text
    /// GOULD'S LEMMA: READ THE CERTIFYING EXIT'S SECOND-ORDER EVIDENCE
    /// OFF THE FACE-EQP FACTORIZATION THE TIER-3 REFINEMENT ALREADY PAYS
    /// FOR.
    ///
    /// At false (the default) a certifying SSN subproblem pays TWO
    /// factorizations beyond its Newton steps: ssn_engine.h's own
    /// second-order verification (its section 7b) and
    /// QpEngine::refine_on_face's exact face solve. Those two factorize
    /// different matrices but answer the same question, and refine_on_face
    /// already REFUSES on a face whose KKT system fails the inertia gate --
    /// so on every ACCEPTED refinement the first one is redundant.
    ///
    /// At true the kernel runs with SsnOptions::defer_certification: the
    /// verification attempt is built but not factorized, the face solve
    /// runs, and an ACCEPTED refinement IS the certificate (the face KKT's
    /// inertia (n_f, m_f, 0) is positive definiteness of the reduced
    /// Hessian on the identified face -- Gould, Math. Prog. 32, 1985),
    /// saving exactly one factorization; a REFUSED refinement falls back to
    /// the deferred verification, at exactly the shipped cost and on exactly
    /// the shipped matrix, so the refusal residue is certified no more
    /// weakly than before.
    ///
    /// **THE SAVING IS PREDICTABLE TO THE UNIT AND THAT IS THE
    /// MEASUREMENT**: total factorizations drop by
    /// `SqpCounters::ssn.ssn_refinements` plus the driver's own trust-region
    /// gate refusals (structurally zero on the shipped corpora -- see
    /// sqp_driver.h's kSsnTrViolationFactor note), and nothing else moves.
    ///
    /// Inert in both directions at `qp_mode == QpMode::kWalk`: no SSN
    /// subproblem is solved, so no certificate is issued and nothing is
    /// deferred.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 956–976

`enable_scaling`: the layer, the off-means-untouched rule and the contract difference when on. All kept, compressed.

```text
    /// THE PROBLEM-SCALING LAYER, OPT-IN. When ON, the engine solves a
    /// diagonally rescaled problem -- the objective and each constraint row put
    /// into units taken from the derivatives at the start point -- and maps
    /// every exported quantity back to the caller's units at the export
    /// boundary. The declared problem is never modified; see
    /// detail/drivers/problem_scaling.h for the transformation and its inverse.
    ///
    /// DEFAULT FALSE, and off means arithmetically untouched: no factor is
    /// installed, every apply site in the seam is behind one false predicate,
    /// and the solve is the one this driver has always run. The `crash_basis`
    /// pattern, not the `enable_soc` one.
    ///
    /// WHAT CHANGES WHEN IT IS ON, stated plainly because it is a real
    /// contract difference and not only a performance one: the CONVERGENCE TEST
    /// gates on the SCALED residuals -- that is the entire point of the layer --
    /// while `SqpSolution`'s four terminal KKT fields, its `f`, its multiplier
    /// blocks and every `history` row report CALLER-scale values. Those two can
    /// differ, so a kOptimal solve may report a caller-scale `kkt_residual`
    /// above `kkt_tol`. `SqpScalingReport` carries both the factors and the
    /// scaled residual the gate actually read, which is what makes the gap
    /// computable rather than merely disclosed.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 997–1022

`validate_sqp_options`: the caller note, the NaN-rejecting predicate form and the full refusal list. All kept, compressed.

```text
/// THE BOUNDARY VALIDATION SqpDriver's constructor runs over SqpOptions.
/// Returns normally on an options object the driver accepts.
///
/// Callers other than the driver may use it -- it is the cheapest way for a
/// front end to reject an options object before building a solver around it
/// -- but the driver validates unconditionally, so calling it first is an
/// optimization, never a prerequisite.
///
/// EVERY PREDICATE IN IT IS WRITTEN AS THE NEGATION OF THE ACCEPTANCE
/// CONDITION so that NaN is rejected rather than admitted, which rests on the
/// build's `-fno-finite-math-only`; that TU's banner carries the argument and
/// the battery pins it by disassembly.
///
/// @param opts The options object to validate.
/// @throws std::invalid_argument, with a message naming the option and the
/// value it had, on any option the driver cannot honour: a non-positive or
/// NaN kkt_tol/feas_tol, a negative max_iter, a non-positive or NaN tr_init,
/// a tr_max below tr_init (a +inf tr_init is exempt from that one check), a
/// tr_min that is non-positive or above either end of the range it floors,
/// an ill-formed `IpqpOptions` field (see each field's own doc comment for
/// its acceptance condition -- validated UNCONDITIONALLY, like the scaling
/// fields, since `qp_mode` is a value a caller may change later). Task 1's
/// TEMPORARY refusal of `opts.qp_mode == QpMode::kIpm` was removed by W1 task
/// 6, which landed the routing chain, so every mode that names a kernel is
/// accepted; `QpMode::kQpModeCount` names none and is refused (see its own
/// doc comment).
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1025–1073

`struct SqpIterate`: the additive-only contract, the qp_solved rule, the NaN rule, the one-row-is-one-trial rule, the iterate-sequence predicate and the caller-scale rule. All kept, compressed.

```text
/// One row of the per-major history -- the record of ONE ITERATE and of the
/// subproblem solved from it (if any). Per-major record fields: KKT
/// residual, f, violation_l1 (h), tr_radius (Delta), verdict, and the QP
/// counters. Ledger integration is a SEPARATE, aggregate record --
/// ledger.h's SqpSolveRecord is one row per whole driver solve, not per
/// major. THIS FIELD SET IS THE FORMAL CONTRACT and is additive-only: a new
/// field may be appended, but none below may change meaning, since dozens of
/// existing tests read sol.history against exactly this shape.
///
/// qp_solved == false marks an iterate the solve stopped AT without building
/// a subproblem from it -- converged, out of major iterations, or found
/// non-finite. Its qp_* fields are meaningless and left at their defaults.
/// There is AT MOST ONE such row and it is always the last; a solve that
/// stopped ON a failing subproblem instead has NONE (see SqpCounters for the
/// two history shapes and why indexing by major_iters is unsafe).
///
/// NaN IS A LEGAL VALUE for the four residual fields, on exactly the
/// non-finite-iterate row: sqp_driver.h's evaluate_kkt deliberately reports
/// NaN rather than a swallowed 0.0 there, so a consumer aggregating these (a
/// plot, a ledger, a convergence table) must expect it.
///
/// ONE ROW IS ONE TRIAL, NOT ONE ITERATE -- which is why the index field is
/// named `trial` and not `major`. A rejected step leaves the iterate where
/// it was and the driver re-solves the SAME subproblem at a smaller radius,
/// so CONSECUTIVE ROWS CAN DESCRIBE THE SAME POINT: their f, stationarity,
/// feasibility, complementarity, kkt_residual and violation_l1 are
/// bit-identical and only tr_radius, the qp_* fields, step_norm and verdict
/// differ.
///
/// RECOVERING THE ITERATE SEQUENCE. Row 0 is always an iterate the solve
/// stood on; after that, row k is a NEW point iff row k-1's step was
/// accepted:
///
///     bool is_new_point = k == 0 ||
///                         history[k-1].verdict == StepVerdict::kAcceptF ||
///                         history[k-1].verdict == StepVerdict::kAcceptH;
///
/// Note the predicate reads the PREVIOUS row, not this one. "Filter out rows
/// whose own verdict is kReject" is WRONG in both directions: it would drop
/// the rejected row that still describes a real (repeated) iterate, and it
/// would also drop the final stopped-AT-iterate row, whose verdict field is
/// meaningless and sits at its kReject default.
///
/// EVERY SCALE-CARRYING COLUMN IS ON THE CALLER'S SCALE -- f, stationarity,
/// feasibility, complementarity, kkt_residual, violation_l1 -- whether or not
/// the solve ran with `SqpOptions::enable_scaling` on, so a row is directly
/// comparable with `SqpSolution`'s own terminal fields and with another solve's
/// history. The number a SCALED solve's convergence test actually gated on is
/// not in this table; it is `SqpScalingReport::scaled_kkt_residual`.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1114–1130

`step_norm`: the routed-failure reading and the SOC-corrected reading. Both kept, compressed.

```text
    /// ||p||inf of the step taken FROM this iterate. ON A ROUTED
    /// QP-FAILURE ROW (qp_solved && qp_status != kOptimal) NO STEP WAS
    /// TAKEN: the field records the |p|inf of the iterate the FAILED solve
    /// returned, which the driver discarded. It is diagnostic there -- it
    /// says how far the subproblem got before giving up -- and must not be
    /// summed into a path length.
    ///
    /// ON A SOC-CORRECTED ROW (soc_applied == true) THIS IS STILL ||p||inf
    /// OF THE ORIGINAL, REJECTED QP STEP -- NOT the norm of the SOC
    /// re-solve's own step, which is the one the iterate actually moved by.
    /// See sqp_driver.h's SECOND-ORDER CORRECTION note for why: it keeps
    /// this field, and every existing reader of it (including the
    /// step_norm <= tr_radius invariant sweep), describing exactly one
    /// QpSolution's own box-respecting step, unconditionally -- the SOC
    /// re-solve is a SEPARATE solve at the SAME radius and so is ALSO <=
    /// tr_radius on its own, but that is not what this field reports on
    /// such a row.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1135–1154

The interior-point evidence fields' banner: why they are on the row and that they are not a certificate. Both kept, compressed.

```text
    // --- THE INTERIOR-POINT TIER'S SECTION 6.3 EVIDENCE, WHERE IT ARRIVES ---
    //
    // TWO SCALARS OFF `IpqpInfeasibilityEvidence`, written by the escape
    // branch's W2 hook (`certified_feasibility_fallback`) on the major it
    // fires on, and zero/false on every other row -- including every row of a
    // kWalk or kSsn solve, where no tier runs at all.
    //
    // WHY THEY ARE HERE (M6 W1 task 6 fix round 3). The hook RECEIVES the
    // whole evidence block, because W2's elastic reformulation needs the
    // least-infeasible point to start from and RECORDS, NEVER JUDGES, the
    // Farkas corroboration; but W1's body is the cold walk, which uses neither, so nothing
    // downstream could tell a real evidence block from a default-constructed
    // one. These two make the arrival OBSERVABLE: they are the block's own
    // headline scalars, and a call site that substituted default evidence
    // would report 0 and false here.
    //
    // NOT A CERTIFICATE, and the field names say so rather than the values:
    // section 6.3's signature is a SUSPICION, `farkas_corroborated == false`
    // means "not corroborated" rather than "withdrawn", and the escape fires
    // either way. Read them as telemetry about why a subproblem was handed on.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1165–1177

`verdict`: when it is the strategy's own, and why the kReject default is more than a safe default. Both kept, compressed.

```text
    /// The globalization strategy's verdict on this trial. It is the
    /// STRATEGY'S OWN verdict iff `qp_solved && qp_status ==
    /// QpStatus::kOptimal` -- on any other row no strategy was consulted,
    /// because there was no certified step to judge.
    ///
    /// The default below is deliberately kReject rather than a value a
    /// consumer could mistake for an acceptance, and on a FAILED-subproblem
    /// row it is more than a safe default: the row really was a rejection
    /// (the iterate did not move and the radius shrank), whether the driver
    /// retried it (sqp_driver.h's SUBPROBLEM FAILURE ROUTING) or propagated
    /// it. So a consumer reconstructing the ITERATE sequence from the
    /// history may read `verdict != kReject` on the PREVIOUS row as "the
    /// iterate moved" across every row shape.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1190–1209

`elastic_applied`: what sets it, what the qp_* fields describe on such a row, and why tr_binding stays meaningful. All kept, compressed.

```text
    /// True iff AN ELASTIC SOLVE SUPPLIED THIS MAJOR: the elastic tier's re-solve after a plain
    /// QP returned kInfeasible, OR the certified fallback's rung A after a kIpm escape (see
    /// sqp_driver.h's ELASTIC TIER note and `certified_feasibility_fallback`). Set on
    /// BOTH outcomes -- the row whose step came from the elastic solve, and
    /// the row that ended the solve because the ladder was exhausted
    /// (verdict kRestore). It is the branch input SOC is gated on.
    ///
    /// ON SUCH A ROW THE qp_* FIELDS DESCRIBE THE FINAL ELASTIC SOLVE, not
    /// the kInfeasible one that triggered it: qp_status is that solve's
    /// status (kOptimal on every row whose step was taken), and
    /// qp_minor_iters/qp_factorizations are its own counts, with the
    /// ESCALATION re-solves' costs folded into SqpCounters' aggregates only
    /// (the SOC convention, ported). On a rung-A-owned major the qp_*
    /// fields are the LADDER's own counts, read from its report.
    ///
    /// tr_binding IS STILL MEANINGFUL on such a row: the elastic solve gets
    /// its trust region as REAL bounds on the original variables rather
    /// than through SolveOverrides (so that the radius cannot also cap the
    /// slacks -- see the ELASTIC TIER note), and the driver re-derives the
    /// radius bit exactly as qp_engine.h's section 6 would have.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1211–1219

`elastic_rho0_ceiling_hit`: the clamp, the per-entry scope and the unpinned row. The first two are kept.

```text
    /// True iff THIS row's elastic ladder started at a CLAMPED placement -- the evidence priced
    /// the violation above the escalation headroom or above the dual_mu safety margin
    /// (sqp_driver.h's THE PLACEMENT BOUND, `ElasticLadderReport::rho0_ceiling_hit`), including
    /// a clamp whose declined ladder was then retried at the floor. False on every other row.
    /// Per-ENTRY, where `elastic_rho0_ceiling_hits` is per-ladder; they agree only because the
    /// retry's override placement can never clamp. The clamped-and-retried ROW itself is unpinned:
    /// it needs a retry that SUCCEEDS at the floor on a row whose ladder was clamped, which the
    /// placement bound still forbids -- T6b covered the walk's misfire, not the bound's arithmetic,
    /// so the row stays unpinned and the forward reference is discharged as NOT delivered.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1225–1244

`watchdog_restored`: what it labels, why the label is needed, and the per-row reading. All kept, compressed.

```text
    /// True iff the FULL-STEP WATCHDOG (SqpOptions::warm_full_step)
    /// restored an earlier best-||KKT||inf iterate ON THIS PASS, i.e. this
    /// row's f/stationarity/feasibility/kkt_residual/violation_l1 describe
    /// the RESTORED point, not the diverged-or-stalled one the previous row
    /// left off at -- sqp_driver.h's THE FULL STEP WATCHDOG block
    /// re-measures the row via `measure_iterate()` before this flag is set,
    /// so the two are never out of step. WITHOUT this flag the history's
    /// residual column can jump backward (a later row reporting a SMALLER
    /// kkt_residual than the row before it, the opposite of every other
    /// row-to-row transition) with nothing in the row itself explaining
    /// why; this is the cheapest honest label for that jump, cheaper than a
    /// full enum since the watchdog is the ONLY thing in this driver that
    /// can rebase the iterate backward mid-solve. False on every other row,
    /// including every row of a solve where the mode never engaged or ran
    /// to convergence without restoring
    /// (SqpCounters::watchdog_restores == 0 there). At most one row per
    /// solve is true today -- the mode restores at most once
    /// (SqpCounters::watchdog_restores is bounded by 1) -- but the flag is
    /// read per-row, not solve-wide, so a future relaxation of that bound
    /// needs no format change here.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1252–1291

`active_set_delta`: the counting rule, the empty-set first major, the meaningful-where-tr_binding-is rule, the TR-held-variable separator and the SOC convention. All kept, compressed.

```text
    /// The symmetric-difference count between THIS major's QP active set and
    /// the previous REPORTING major's: one for every inequality row that
    /// entered or left `QpSolution::ineq_active`, plus one for every variable
    /// whose `bound_state` changed at all (lower <-> upper <-> free <-> fixed
    /// counts ONCE, not twice -- it is a per-variable state change, not a
    /// two-element set edit). The MAJOR-level reading of the bulk flip that
    /// `SsnCounters::ssn_bulk_flips` measures one level down, inside a single
    /// SSN solve.
    ///
    /// THE FIRST REPORTING MAJOR COUNTS AGAINST THE EMPTY SET -- every active
    /// row and every non-free variable -- so a solve's first row is a census of
    /// where the first subproblem landed rather than a 0. The restoration
    /// sub-solve is a SEPARATE sequence with its own empty start, because it is
    /// a separate `solve()` on a different (wrapper) problem.
    ///
    /// ALL SIX FIELDS ARE MEANINGFUL EXACTLY WHERE `tr_binding` IS, which is
    /// the rule the qp_* fields above already carry: they read 0 on any row
    /// whose `QpSolution` never became this major's answer -- the
    /// stopped-AT-iterate and non-finite-iterate rows (`qp_solved == false`),
    /// and the EXHAUSTED-LADDER row, whose qp_* fields describe the ladder
    /// while the solution in hand describes the original kInfeasible
    /// subproblem. Such a row also leaves the previous set UNCHANGED, so the
    /// next reporting row's delta is measured against the last set that was
    /// really solved for.
    ///
    /// A TR-HELD VARIABLE IS NOT A BOUND SIDE HERE, and this is the reader's
    /// separator rather than a choice made here: every producer of a
    /// `QpSolution` reports `kFree` in `bound_state` for a variable held by a
    /// TR-tight effective bound and flags it in `tr_active` instead
    /// (qp_engine.h's section 6 reporting exclusions, re-derived by
    /// `ssn_engine.cpp`'s TR/real split, by `ipqp_engine.cpp` and by
    /// `elastic_project`). So these fields count REAL bound sides only, and
    /// `tr_binding` on the same row is the separate reading of "the radius was
    /// binding somewhere". A change of radius that moves a variable between a
    /// real bound and a TR pin therefore DOES move this delta.
    ///
    /// ON A SOC-CORRECTED ROW THE SET IS THE ORIGINAL QP's, not the SOC
    /// re-solve's -- `tr_binding`'s own convention above, for the same reason:
    /// the chain then compares one main subproblem against the next, rather
    /// than alternating between two different subproblems' working sets.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1320–1375

`struct SqpSolution`: the multiplier contract at each exit, the subgradient certificate in full, the z-on-that-exit argument with its measured residual, and the cleared-multiplier exception. Every contract is kept; the measured residual is here.

```text
/// Result of a whole SQP solve.
///
/// MULTIPLIERS. lambda_e/lambda_i are the LAST SUBPROBLEM'S multipliers,
/// carried out unchanged -- nlp_model.h's sign convention makes them the
/// NLP's multipliers at the returned point with no flip anywhere (that
/// header's MULTIPLIER SIGN CONVENTION note). z is NOT the subproblem's z;
/// it is the MODEL-IMPLIED bound multiplier at the returned point, computed
/// from grad L there. See sqp_driver.h's REPORTED BOUND MULTIPLIER note for
/// why (the QP's z is forced to 0 at a TR-pinned index and would not satisfy
/// NLP stationarity).
///
/// On a non-kOptimal exit x is the FINAL ITERATE reached, and the
/// multipliers are whatever the last successful subproblem priced -- NOT
/// cleared, unlike QpSolution's kInfeasible/kNumericalError convention,
/// because here they are the caller's evidence about where the driver
/// stopped.
///
/// ON A CERTIFIED kInfeasible EXIT THE MULTIPLIERS MEAN SOMETHING STRONGER
/// AND DIFFERENT, and a caller must not read them as prices of the NLP's own
/// constraints: they are the RESTORATION problem's multipliers, which are
/// precisely a SUBGRADIENT CERTIFICATE that the returned x is a stationary
/// point of the infeasibility measure
///     h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)).
/// Concretely they satisfy
///     Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,
///     lambda_e in [-1, 1]^me,  lambda_i in [0, 1]^mi,
///     lambda_e(i) = sign(cE_i(x)) wherever cE_i(x) != 0,
///     lambda_i(j) = 1 wherever cI_j(x) > 0 and 0 wherever cI_j(x) < 0,
/// i.e. the vanishing of a subgradient of h at x -- note there is NO grad f
/// term, which is what distinguishes this quadruple from an ordinary KKT
/// point and is why it certifies infeasibility rather than optimality. The
/// interval-valued entries are the free subgradient selectors on rows that
/// are exactly satisfied (|cE_i| = 0, cI_j = 0), which is where the
/// certificate gets the slack it needs.
///
/// z ON THIS EXIT IS THE RESTORATION PROBLEM'S BOUND PRICE, not the usual
/// model-implied one. The difference is exactly the objective: the ordinary
/// z is grad L = grad f + Je^T le + Ji^T li at an active bound, and grad f
/// is precisely the term a subgradient of h must not contain. Returning the
/// usual z would make the identity above fail by ||grad f|| at every
/// infeasible stationary point that sits ON a bound -- measured at a
/// residual of 1 on the box-blocked fixture, where the certificate calls for
/// z = -1 and the ordinary measure reports 0. See sqp_driver.h's
/// RESTORATION PHASE note. tests/sqp/test_sqp_restoration.cpp's
/// InfeasibleNlpCertifies re-derives this from the model at the returned
/// point rather than trusting the driver's own measurement.
///
/// THE ONE EXCEPTION is the non-finite-iterate kNumericalError exit, where
/// lambda_e/lambda_i/z ARE all cleared: at a NaN iterate nothing was
/// measured, so there is no evidence to preserve and a leaked price would be
/// noise wearing the shape of a multiplier. f is still reported as the model
/// returned it, which is to say possibly NaN.
///
/// history describes every iterate visited; whether the LAST row is an
/// iterate row or a failing-subproblem row depends on the exit -- see
/// SqpCounters.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1376–1386

`struct SqpScalingReport`: why it is a diagnostic rather than a counter, and the identity-when-inactive rule. Both kept, compressed.

```text
/// @brief What the W0.2 problem-scaling layer did to a solve, reported on its
///        solution.
///
/// A DIAGNOSTIC, NOT A COUNTER. It is deliberately not a member of SqpCounters:
/// that struct's documented aggregation rule is that every field sums except
/// two peaks, and a unit factor neither sums nor peaks meaningfully across the
/// restoration fold -- a solve and its restoration sub-solve are two problems,
/// and the sub-solve runs unscaled by design.
///
/// AN INACTIVE REPORT IS THE IDENTITY, field for field, so a reader never has to
/// branch on `active` before using the factors.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1426–1455

The terminal KKT measurement's banner: why the fields exist, the NaN rule, the certified-infeasible reading and the sign-sweep qualification. All kept, compressed.

```text
    // THE TERMINAL KKT MEASUREMENT, taken at the RETURNED (x, lambda_e,
    // lambda_i) by the same evaluate_kkt call the convergence test read.
    // They exist so a consumer can fill an outcome record without
    // reconstructing the history's exit shape: the last `history` row is NOT
    // reliably the returned point (a restoration exit returns the RESTORED
    // point, which has no row of its own), so scanning `history` for these is
    // wrong on exactly the exits where they matter most.
    //
    // All four are NaN on the non-finite-iterate kNumericalError exit, for
    // the reason evaluate_kkt reports NaN there: nothing was measured at that
    // point, and a 0.0 would read as a converged residual.
    //
    // On a certified kInfeasible exit they measure the NLP's own KKT
    // conditions at the returned point -- an INFEASIBLE point, so
    // `feasibility` is large by construction and `stationarity` is the
    // ordinary grad-L measure, NOT the subgradient certificate's residual
    // (that certificate is the multiplier quadruple, read under this struct's
    // own note above). `z` is the one field of this solution that comes from
    // the restoration problem instead; these four do not.
    //
    // ONE QUALIFICATION ON "AT THE RETURNED MULTIPLIERS", and it is exact:
    // when `counters.ssn.ssn_sign_swept > 0` the R6 sign sweep clamped
    // negative inequality prices AFTER this measurement was taken, so these
    // four describe the PRE-SWEEP multipliers and `lambda_i` holds the swept
    // ones. `stationarity` (and hence `kkt_residual`) is then OPTIMISTIC by at
    // most `counters.ssn.ssn_sign_sweep_max * ||Ji||inf` over the swept rows;
    // `complementarity` can only be over-stated, never under-stated, since
    // clamping removes product terms. Both counters are reported precisely so
    // this gap is computable; at `ssn_sign_swept == 0` there is no gap.
    /// @brief Reduced/projected ||grad L||inf at the returned point.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1468–1484

`wall_seconds`: what is measured and the informational-never-asserted standing. Both kept, compressed.

```text
    /// Wall-clock seconds spent inside this solve, measured with
    /// std::chrono::steady_clock around the driver's solve_impl ALONE --
    /// never around model construction, the bridge/seam lay, the staged-value
    /// ingest or the ledger bookkeeping, all of which are setup. The same
    /// measurement SqpSolveRecord::wall_seconds carries (ledger.h), taken
    /// once and reported in both places.
    ///
    /// INFORMATIONAL, NEVER ASSERTED -- counters, not timings, are this
    /// project's currency of correctness. No test in this repository asserts
    /// a VALUE here; the pins on it assert only that it is populated and
    /// non-negative. A consumer may report it and may compare it against
    /// another reading taken under the same measurement discipline; nothing
    /// may gate on it. Same standing as
    /// InteriorPointSolver::SolveResult::total_time_.
    ///
    /// Defaults to 0.0; every public solve() that RETURNS writes a value
    /// >= 0.0 onto the solution it hands back.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1497–1524

`infeasibility_certified`: what it means, why it is not derivable from the status, the counters or the last row's verdict, and the one-way-guarantee reading. All kept, compressed.

```text
    /// TRUE ONLY ON THE CERTIFIED INFEASIBILITY EXIT: the restoration phase
    /// ran to its own KKT test, that test passed (residual <= kkt_tol) and h
    /// at the returned point is still above feas_tol -- so
    /// (x, lambda_e, lambda_i, z) is the subgradient certificate documented
    /// above and the returned point is a stationary point of h.
    ///
    /// FALSE ON EVERY OTHER EXIT, INCLUDING OTHER kInfeasible ONES, and
    /// that is what this flag is for. It is NOT reliably derivable from the
    /// status or the counters: a solve that restored once and then met a
    /// SECOND request (the once-per-solve cap), and a solve whose
    /// restoration was itself stuck, both report kInfeasible with
    /// restoration_iters > 0 -- exactly like the certified exit -- so the
    /// counters never tell the three apart. NOR IS THE LAST ROW'S VERDICT A
    /// RELIABLE DISCRIMINATOR: it is kRestore when the request came from
    /// the funnel's signature or the elastic tier's exhaustion, but kReject
    /// when it came from the radius floor (sqp_driver.h's RESTORATION PHASE
    /// note has the decision table and the floor's own tr_radius-at-the-
    /// floor marker), and any of the three kInfeasible outcomes above can
    /// pair with either shape. Trusting either the counters or the verdict
    /// produces the same caller-visible wrong answer: a FEASIBLE problem
    /// that stalls twice reports kInfeasible, and a caller following either
    /// rule would announce local infeasibility with no certificate behind
    /// it.
    ///
    /// READ IT AS A ONE-WAY GUARANTEE. true means the certificate holds.
    /// false means NO CLAIM IS MADE about the model -- the driver could not
    /// make progress, which is a statement about this solve, not about the
    /// problem.
```

**SOURCE** 1997159 · include/hven/drivers/sqp_types.h · lines 1527–1537

`warm_start`: the population rule and the populated-is-not-valid exception. Both kept, compressed.

```text
    /// The solve's exit state in warm_start.h's shape, for a LATER solve of
    /// a nearby problem to feed back in. Populated on EVERY exit of
    /// SqpDriver::solve() -- including a failed one -- from the best-known
    /// iterate; see warm_start.h's own note for the valid/cold contract and
    /// sqp_driver.h's POPULATION note for exactly what "best known"
    /// resolves to at each exit. POPULATED IS NOT THE SAME AS `valid`: one
    /// exit (a start point the model could not evaluate) fills these fields
    /// for inspection but reports valid == false, because feeding that
    /// point back would override the caller's own corrected x0 --
    /// warm_start.h's `valid` note states the exception and why it is the
    /// only one.
```

### include/hven/detail/qp/qp_engine.h

1773 lines / 1298 comment lines at `1997159`; 1527 / 1052 after. The smallest reduction of the ten, deliberately: this header's banner is a contract other headers cite by section name.

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 6–505

The 500-line loop contract. The header keeps it in compressed form, because other headers cite its numbered sections by name -- the five hot-start reuse conditions, the window-consistency rule, section 6's reporting exclusions, section 4b's gates and the export invariant on z are all operative contracts a consumer reasons against. What was removed and is kept here: the counter-identity's task-by-task amendment history, the derivation of section 4c's Schur-complement algebra, and the trustworthy-range measurement record -- the M6 W2 T6b 30-cell dual_mu x rho table with its five high-rung residue cells, the 21-cell stiffened-fixture table, the |lambda| = 5 bite and its row_tolerance figures, and the eight kRefactorize cases M6 W2 T7 covered.

```text
// qp_engine.h — the primal active-set loop for the QP
//
//     min   g^T x + 1/2 x^T H x
//     s.t.  Ae x  = be,   Ai x <= bi,   l <= x <= u
//
// H is not required to be positive semidefinite; sections 4b/4c add what an
// INDEFINITE H needs on top of the ordinary convex loop. Both are inert (and
// free) on a convex H.
//
// The numerics live below this file: the working set (working_set.h), the
// regularized bound-eliminated KKT assembly (kkt_assembly.h), the sparse
// factorization (hven::linear::SymmetricFactor via kkt_calls.h's KktFactor),
// and the equality-QP solve with iterative refinement (eqp_solve.h). This
// file is only the loop that walks between working sets.
//
// --- Loop contract ---
//
// 0. CROSSED BOUNDS. lower(i) > upper(i) + feas_tol is an empty box: verdict
//    kInfeasible immediately, x = the clamped start point, multipliers zero.
//    Checked here rather than in QpProblem::validate() because a caller may
//    legitimately hand this engine an empty box (a trust-region box
//    intersected with the real bounds) as a normal runtime outcome.
//
// 1. START POINT. Cold: x = clamp(0, l, u). Warm: clamp(seed.x, l, u) plus
//    seed.bound_state/seed.ineq_active as the initial working set. l(i) ==
//    u(i) is kFixed and never leaves the working set. A variable merely
//    sitting at a bound is NOT pinned at start -- the ratio test pins it the
//    first time it actually blocks.
//
//    General inequalities violated at the start enter via a SHIFTED-
//    CONSTRAINT HOMOTOPY rather than a phase-1 LP: shift(j) =
//    max(0, Ai_j x - bi_j), and every row with shift(j) > 0 joins the working
//    set. The EQP always solves against the TRUE rhs bi, so a shifted row's
//    shift decays with the step. Shifts are recomputed from x every step (not
//    propagated) and clamped to zero below a scale-aware tolerance
//    (refresh_shifts). A working row whose shift is still positive is EXEMPT
//    from step 2's drop rule: it is being driven to feasibility, so its
//    multiplier's sign says nothing yet.
//
// 2. EQP CANDIDATE + DROP RULE. Each iteration solves the EQP on the current
//    working set, giving x* and multipliers lambda_e/lambda_w. If p = x*-x is
//    negligible, x is a KKT point of the working set and the multipliers
//    decide: a working inequality needs lambda_i >= -opt_tol; a variable at
//    its lower bound needs z >= -opt_tol (upper: z <= opt_tol), where
//    z = (Hx + g + Ae^T lambda_e + Ai^T lambda_i)(i) (eqp_solve.h); kFixed
//    variables are unconstrained. No violation => kOptimal; otherwise the
//    MOST NEGATIVE multiplier leaves (Dantzig rule), ties broken by largest
//    angle (violation scaled by the constraint gradient's 2-norm) then lowest
//    index.
//
//    TR-PINNED STATIONARITY CAVEAT (section 6). z is priced and consulted
//    internally the same way at every index, TR-pinned or not, but the z
//    REPORTED in QpSolution is forced to 0 at a TR-pinned index, so there the
//    reported quantities do not satisfy stationarity. A kFree report at a
//    TR-pinned index means only "unconstrained by any REAL bound"; read
//    tr_active, not z or bound_state, for TR constraint status.
//
// 3. RATIO TEST. If p is not negligible, step along it: alpha = min(1,
//    min_j ratio_j), stopping at the first non-working inequality or bound
//    that blocks; that constraint joins the working set. A ratio landing at 1
//    within kEngineStepTieTol counts as blocking too.
//
//    KNOWN LABELING DIVERGENCE. That tie-break only fires for a constraint
//    the step travels toward -- a variable already sitting on its bound with
//    p(i) == 0 is never pinned (the ratio test only considers |p(i)| >
//    kEngineDenomTol), so it is reported kFree/z==0 where a dense oracle
//    reports kAtLower/kAtUpper with a zero multiplier. x, the objective and
//    the duals agree; only the active-set LABEL differs. A caller needing
//    activity by geometry rather than working-set membership must test the
//    residual itself.
//
// 4. WORKING-SET UPDATE. Under QpOptions::ws_algebra == kRefactorize, every
//    working-set change is followed by a fresh assemble_kkt() +
//    factorize_checked() (solve_eqp does both).
//
//    BORDER MODE (kSchurBorder, the DEFAULT) leaves the loop unchanged and
//    swaps only the linear algebra: one K0 (assemble_kkt_full, spanning all n
//    variables) is factorized from the seed working set, and every later
//    working-set change becomes a GMSW border over that fixed factorization
//    (border_ops.h/schur_complement.h) rather than a refactorization. K0 is
//    rebuilt -- clearing the border stack -- when
//    SchurComplement::needs_refactorization() trips or K0's own factorization
//    needed a perturbed pivot; iterations where a rebuild cannot help fall
//    back to the elimination path. See border_candidate, rebuild_k0,
//    sync_borders and latch_still_holds for the mechanism.
//
//    The two modes are OBSERVATIONALLY EQUIVALENT for a CONVEX H (same
//    status, active set, x, multipliers), with the refactorize path as the
//    oracle. They are NOT equivalent for an indefinite H: the bound-
//    eliminated K and the full-variable K0 can have different inertia, so the
//    two modes can legitimately reach different working sets and statuses.
//
//    COUNTER SEMANTICS (relied on downstream, e.g. warm-start assertions).
//    QpCounters::minor_iters increments exactly ONCE per major iteration, so
//    a run stopping at opts.max_iter reports minor_iters == max_iter. Under
//    kRefactorize, factorizations counts solve_eqp calls (one per major
//    iteration, except the empty-reduced-system short-circuit -- every
//    variable pinned, no equalities, no working rows -- which touches no
//    factorization) and schur_updates stays 0. Under kSchurBorder,
//    factorizations counts K0 factorizations plus elimination-path fallbacks
//    (a single iteration can spend two), and schur_updates counts individual
//    add_border/drop_border calls INCLUDING re-adds after a rebuild, but not
//    the rebuild's own wholesale clear.
//    ONE FURTHER CONTRIBUTOR, ON EITHER MODE (M6 W2 T7, amended by its fix
//    round 1): the verdict-site face refinement's ELIMINATED twin charges one
//    factorization -- and one symbolic_analyses -- when its working-set guard
//    MISSES and it has to assemble and factorize a system of its own. On a
//    guard HIT it reuses the incumbent factorization and charges NEITHER, and
//    a solve that never dead-ends charges neither because the twin never runs.
//    So the identities above are exact per major iteration and become "+1 per
//    guard miss" over a whole solve; the miss is rare by construction (only a
//    working-set change between the candidate solve and the verdict) and was
//    measured at zero over the whole suite when the rule landed.
//    4c's ride costs one minor_iter like
//    any other step. 4b's repair costs one EXTRA minor_iter (the kWrong
//    iteration is counted, then retried) plus, per pin/release it probes,
//    schur_updates under kSchurBorder or one factorization under
//    kRefactorize; neither is reachable on a convex H.
//    border_refine_steps/eqp_refine_steps accumulate at the same two EQP call
//    sites: every solve_bordered_eqp call adds its total kept steps (>= 1),
//    every solve_eqp call adds its extra kept steps (always 0) -- see
//    core/solver_counters.h.
//
//    HOT-START REUSE (border mode only). border_ is an ENGINE-INSTANCE
//    member, not a per-solve local, so a warm re-solve on the same QpEngine
//    can skip K0's assembly/factorization when FIVE conditions all hold at
//    the seed working set, checked once before the loop's first iteration:
//      (a)/(c) H/Ae/Ai's structural pattern AND values are byte-identical to
//          the previous trustworthy solve (detail::structural_hash /
//          detail::values_hash). K0's values depend on H/Ae/Ai and the
//          EFFECTIVE (primal_delta, dual_mu) this solve resolved to -- never
//          on g/be/bi.
//      (d) that effective (primal_delta, dual_mu) pair is identical to the
//          previous trustworthy solve's. tr_radius is NOT part of the key at
//          all -- bounds (real or TR-derived) never enter K0.
//      (b) the seed working set (start_center()/ingest_seed_working_set()
//          plus the pre-loop refresh_shifts()) equals the EXIT working set of
//          that same previous solve.
//      (e) border_'s factor's own live (session_id, epoch) identity equals
//          the pair THIS engine last saw as trustworthy, AND the factor's
//          numerics are usable (inertia().state == kObserved).
//
//    (b) IS ONLY APPROXIMATE, AND THAT IS SAFE: refresh_shifts() can add a
//    row to ws after border_candidate()'s last sync_borders() call, so
//    border_exit_active_ineq_ can understate the true exit ws. What makes
//    reuse safe is that sync_borders() is an UNCONDITIONAL, FULL
//    reconciliation of ws against border_'s ledger, run on EVERY iteration of
//    EVERY solve -- the reuse fast path skips rebuild_k0's assembly and
//    factorization, never sync_borders(). That first post-reuse
//    sync_borders() call may not be skipped on the theory that a matching
//    seed ws leaves nothing to reconcile.
//
//    All five conditions are NECESSARY but NOT SUFFICIENT for
//    `factorizations == 0`: border_candidate's own checks (a carried-over
//    perturbed-pivot count, or a border stack already past
//    needs_refactorization()) can still force a rebuild. Callers must not
//    assert `counters.factorizations == 0` unconditionally on a warm
//    re-solve; assert it only alongside control of these conditions.
//
//    INVALIDATION POLICY. border_'s cache is committed (border_valid_ set)
//    ONLY on a clean kOptimal exit, and is pessimistically cleared at the top
//    of every solve() call before anything else runs -- including before
//    qp.validate(). kMaxIter/kInfeasible/kNumericalError exits and any
//    exception thrown mid-solve therefore all leave the cache invalidated;
//    the next solve() reassembles and refactorizes from scratch.
//
//    THREAD SAFETY. QpEngine is NOT thread-safe for concurrent solve() calls
//    on the same instance: border_ and the reuse-fingerprint members are
//    mutable state shared across calls despite solve() being const from the
//    caller's view. "One QpEngine per thread" is not the whole rule either:
//    two DIFFERENT QpEngine instances, on any threads, can share one
//    BorderState object (Pardiso pt_ array included) if one adopts a hot
//    handle the other produced. SEQUENTIAL hand-off (produce a handle, then
//    feed it to a different engine after that call returns) is safe -- the
//    session/epoch identity detects a producer that mutated the object again
//    and degrades to kWarm. CONCURRENT use of a shared BorderState is
//    UNDEFINED: that identity is unsynchronized state, not atomic. A caller
//    sharing a hot handle across threads must ensure no two engines holding a
//    copy of it ever call solve() concurrently.
//
// 4b. INERTIA GATE AND TEMPORARY-VERTEX START REPAIR (indefinite H).
//
//    Every EQP solve above is a MINIMIZATION over the current working set
//    only for a convex H. For an indefinite H the regularized KKT system is
//    still nonsingular but its answer can be a saddle or a maximizer the loop
//    would certify kOptimal without noticing. The signature is the KKT
//    matrix's INERTIA: where the reduced Hessian is positive definite on the
//    null space of the working constraints, [H+delta*I A^T; A -mu*I] has
//    inertia exactly (#variables, #constraint rows, 0) -- unconditional for a
//    convex H, which is why this gate is a no-op there. The per-path
//    expectations are derived at eliminated_candidate and
//    border_inertia_verdict; the gate must read the BORDERED system's
//    inertia, never K0's numbers against a fixed expectation.
//
//    PERTURBED PIVOTS ARE NOT A PASS AND NOT A REPAIR TRIGGER. Pardiso's
//    (n_pos, n_neg) is trustworthy IFF perturbed_pivots == 0: on an exactly
//    singular matrix it fabricates a pivot sign and reports an inertia
//    indistinguishable from the nonsingular case, without raising error -4.
//    detail::InertiaVerdict has THREE verdicts:
//      kOk      trustworthy and matching -- proceed.
//      kSuspect inertia UNKNOWN -- never a pass, and never evidence that a
//               repair is needed. Handled by step 4's refactorization
//               machinery and, at a would-be-kOptimal exit, by the
//               suspect-stall gate below.
//      kWrong   trustworthy and DISAGREEING -- the working set is
//               second-order inconsistent; at solve start this triggers the
//               temporary-vertex repair (repair_temporary_vertex), which pins
//               free variables one at a time until the gate returns kOk and
//               unwinds entirely if it cannot reach kOk.
//
//    SECOND-ORDER CERTIFICATION (step 5's classification). When the loop
//    reaches a point it can neither improve nor drop from, and the gate's
//    verdict for the system just solved is a TRUSTED kWrong, the point is
//    reported kNumericalError rather than kOptimal (multipliers cleared).
//    Only kWrong triggers this; kSuspect does not.
//
//    ZERO-MULTIPLIER PROBE. The gate tests the null space of the FULL active
//    labeling, so negative curvature excluded only by a WEAKLY ACTIVE
//    constraint stays hidden. probe_zero_multiplier_drops runs at the
//    would-be-kOptimal exit and tentatively drops each such member,
//    most-recently-added first, re-running the gate; a kWrong makes the drop
//    real and RESUMES the loop. ONE-AT-A-TIME IS THE KNOWN REMAINING
//    APPROXIMATION: a critical cone that opens only when TWO OR MORE weakly
//    active constraints drop simultaneously is not covered.
//
//    SUSPECT-STALL GATE. When the verdict for the system the FINAL iterate
//    was solved from is kSuspect, kOptimal may be certified only after an
//    explicit free-block stationarity check on the QP model
//    (detail::free_block_stationarity, which is NaN-aware and applies a
//    SCALED rather than absolute opt_tol). It runs AFTER the zero-multiplier
//    probe, independently of it. On failure primal_delta is escalated one
//    decade (detail::kSuspectDeltaFactor), the border state is discarded, and
//    the iteration resumes; the ladder is bounded
//    (detail::kMaxSuspectEscalations rungs, QpCounters::suspect_escalations)
//    and reports kNumericalError when exhausted. NEVER kOptimal off a stalled
//    suspect loop, and never a hang. CAVEAT (known, unmeasured): r is built
//    from prices computed at the top of the iteration, but refresh_shifts()
//    between there and here can add rows with lambda_i == 0, which can
//    inflate the residual on a point that is in fact stationary -- a spurious
//    escalation bounded by the ladder, not a wrong answer.
//
//    POST-PROBE RESTART. A probe-driven drop leaves 4c's ride nothing to arm
//    off (its direction is identically zero), so the temporary-vertex repair
//    is spent ONCE per solve at the certification branch's trusted-kWrong arm
//    instead; see that branch for the trigger, budget and failure rule.
//
// 4c. NEGATIVE-CURVATURE RIDES AFTER A DROP (indefinite H).
//
//    Section 4b makes an indefinite START safe; this makes an indefinite DROP
//    safe. By Cauchy interlacing (adding a working row can only raise the
//    reduced Hessian's smallest eigenvalue), a drop is the ONLY way negative
//    curvature can reappear once a working set is second-order consistent.
//
//    For a KKT point of working set W with positive definite reduced Hessian
//    B = Z'HZ, releasing constraint c (lambda_c < 0) leaves W' with
//    null(A_W') = null(A_W) + span{d}, a_c.d = -1, w = Z'Hd, gamma = d'Hd,
//    and Schur complement sigma = gamma - w' B^-1 w -- by interlacing the
//    only eigenvalue of the new reduced Hessian that can be negative. The
//    next EQP's step is p = (-lambda_c/sigma) * q, q = d - Z B^-1 w, with
//    q'Hq = sigma and a_c.q = -1, so p'Hp = (lambda_c/sigma)^2 * sigma has
//    the SIGN of sigma: the curvature of the ordinary EQP step already IS the
//    test, with no extra solve. When sigma < 0, p points back into the
//    just-released constraint, the ratio test answers alpha = 0, and without
//    this section the loop cycles between W and W' until max_iter.
//
//    THE CHECK. On the iteration after a drop, and only there, the Rayleigh
//    quotient p'Hp/p'p is compared against curvature_tol =
//    kCurvatureTolFactor * hessian_scale(qp). Above it, the ordinary EQP step
//    is taken. At or below it, the loop RIDES: the admissible sign of p
//    (ride_sign), the ratio test with its unit cap DISABLED, and a step to
//    the nearest blocking constraint or bound. A negligible p is excluded
//    before the check. NO BLOCKER => kNumericalError, multipliers cleared,
//    reported immediately from inside the loop; the SQP driver's trust-region
//    bounds make that branch unreachable in SQP use.
//
//    ARMING IS ONE-SHOT: the drop record is consumed on the very next
//    iteration whether the ride fires, declines, or is never reached. A
//    DECLINED ride leaves the curvature un-retested until some later drop;
//    section 4b's certification branch is the fallback, not equivalent
//    coverage. THIS SECTION IS VACUOUS for a probe-driven drop; 4b's
//    POST-PROBE RESTART intercepts that case. 4c's no-blocker branch and 4b's
//    certification branch are MUTUALLY EXCLUSIVE by construction and report
//    the same status and semantics.
//
//    For STRICTLY CONVEX H the ride branch is UNREACHABLE (the Rayleigh
//    quotient is at least lambda_min(H) > curvature_tol whenever H's 2-norm
//    condition number is below ~1e12) and the engine is bit-for-bit
//    unchanged. For PSD-SINGULAR H (including H == 0, an LP) the branch IS
//    reachable and behavior is NOT identical to the capped path: the ride
//    takes the uncapped ratio while the ordinary path clamps at alpha = 1.
//    That is intended on the PSD-singular path.
//
//    NO ANTI-CYCLING RULE IS NEEDED HERE: a ride's objective decrease is
//    strict whenever alpha > 0, so the working set it lands in cannot be
//    revisited at the same objective. alpha == 0 is still possible at a
//    degenerate vertex, covered by the Degeneracy note below.
//
// 5. TERMINATION. The loop reaches a KKT point of its working set with
//    nothing left to drop, and classifies it:
//      - kInfeasible if any inequality or equality row carries a STRUCTURAL
//        violation (violation_is_structural). Both blocks are checked -- the
//        shift machinery only watches inequalities.
//      - kNumericalError if the point is feasible but some free component
//        unbounded on the side it grew toward exceeds
//        detail::unbounded_artifact_scale() (is_runaway) -- the answer is the
//        regularization talking, not an optimum.
//      - kNumericalError if the inertia gate's verdict for the system just
//        solved is a TRUSTED kWrong (unreachable on convex H). ONE ESCAPE:
//        4b's POST-PROBE RESTART, if unspent.
//      - otherwise 4b's ZERO-MULTIPLIER PROBE gets a veto: a trusted kWrong
//        there makes the drop real and RESUMES the loop without assigning a
//        status on this pass -- the only branch here that does not end the
//        solve.
//      - kOptimal otherwise.
//    kMaxIter once eff_max_iter major iterations have been spent -- an
//    explicit opts.max_iter, or, at its default sentinel, the size-derived
//    cap (detail::effective_qp_max_iter). A FOURTH kNumericalError exit
//    bypasses this classification entirely: 4c's ride finding no blocker
//    stops the loop mid-iteration.
//
//    ORDERING IS LOAD-BEARING: the drop rule is consulted BEFORE
//    infeasibility may be declared, because a bound pinned by the ratio test
//    commonly blocks a shift from closing, and releasing it is what lets the
//    homotopy finish.
//
//    ON kInfeasible/kNumericalError the returned x is the FINAL ITERATE the
//    loop stopped at (not the least-violating point seen; no argmin is kept),
//    and multipliers are CLEARED on both -- an inconsistent or runaway system
//    prices them at O(1/dual_mu), which are regularization artifacts, not
//    prices.
//
//    TRUSTWORTHY RANGE, both directions, both known and accepted:
//    (i) FALSE kInfeasible on a feasible row whose residual at the classified
//        point still clears kStructuralResidualFrac. MEASURED at M6 W2 T6b, it
//        bit at |lambda| = 5 on a well-scaled elastic penalty subproblem --
//        nowhere near any large-multiplier regime, and with
//        kInfeasibilityAbsorbTol's cap never binding (row_tolerance's min()
//        selected dual_mu*|lambda| = 5e-8 against a cap of 1.75e-5). The
//        residue was BORDER MODE'S ALONE: solve_bordered_eqp writes each
//        pinned variable's exact bound value over a solve that only satisfies
//        it to O(dual_mu*|y|), and its refinement loop stops on a footprint
//        and a floor measured in the PENALTY'S units, so on an elastic copy
//        that displacement survives into a row read in the ROW'S units.
//        kRefactorize eliminates a pinned variable exactly and left residual
//        0 on the whole family.
//        COVERED at M6 W2 T6b by the VERDICT-SITE FACE REFINEMENT below.
//        Measured after it, border mode, at the classified point: the elastic
//        copy of a boxed equality CLOSES under the refinement on 25 of the 30
//        cells of dual_mu in {1e-3..1e-8} x rho in {1e2..1e8}; the other 5
//        (dual_mu >= 1e-4 with rho >= 1e6) read kOptimal on a corner point
//        whose row is off by 15, via (ii)'s blind spot -- a residue, pinned as
//        one (the band's HIGH-rung pin), not coverage. The same fixture
//        stiffened by c in {1, 10, 30, 1e2, 1e3, 1e4, 4e5} reads kOptimal on
//        all 21 cells of c x rho in {1e6, 1e7, 1e8} -- the Hessian-scale
//        direction included, because the refinement fixes the POINT and needs
//        no bound read off the data.
//        kRefactorize HAD ITS OWN (i) CASES -- eight at the shipped dual_mu,
//        from the dual regularization the two modes share rather than from any
//        bordering residue -- and M6 W2 T7 covers them with the SYMMETRIC
//        refinement (refine_eliminated_face_for_verdict). So does a BORDER-mode
//        solve that fell back or LATCHED to the elimination path, which T7's
//        first cut left refining neither way: the dispatch keys on the PATH
//        that produced the candidate, not on ws_algebra. STILL UNCOVERED: any
//        (i) case whose face the refinement cannot close inside the budget,
//        which is left to the classifier exactly as before.
//    (ii) FALSE kOptimal where a genuine contradiction's gap hides beneath
//        kStructuralResidualFrac*dual_mu*|lambda|. Tightening the fraction to
//        catch it re-breaks (i). The SQP driver is the second detection
//        layer; a caller using the QP engine alone should scale its rows or
//        shrink dual_mu.
//    (iii) FALSE kNumericalError where a free variable's TRUE optimum lies
//        beyond unbounded_artifact_scale() =
//        kUnboundedArtifactFactor/primal_delta. A caller expecting very large
//        optima should tune primal_delta rather than treat this
//        kNumericalError as load-bearing.
//
//    THE VERDICT-SITE FACE REFINEMENT (M6 W2 T6b, extended to BOTH PATHS at
//    M6 W2 T7) runs between the two: once the classification above has said
//    kInfeasible, the closed working face is refined further -- against the
//    same unregularized system the candidate solve targets, but to a target
//    stated in ROW units (see refine_face_for_verdict, its eliminated twin,
//    working_face_measure and face_row_target). WHICH TWIN RUNS IS DECIDED BY
//    PROVENANCE, NOT BY ws_algebra: EqpResult::refine_steps is >= 1 on every
//    bordered candidate (solve_bordered_eqp counts its mandatory step) and is
//    identically 0 on every eliminated one (solve_eqp sets it so), so it is an
//    exact witness of the path that produced the candidate -- and a border-mode
//    iteration served by the elimination path is refined by the ELIMINATED
//    twin, on the system it was actually solved from. The refined point is
//    adopted only if the face CLOSES, and it is then both what the classifier
//    re-reads and what a kInfeasible exit returns. An ADOPTING dead end
//    re-enters the ordinary would-be-kOptimal path -- refresh_shifts, the
//    zero-multiplier probe, the runaway guard -- and may continue the walk;
//    that is the intent. A solve that never dead-ends, and every dead end
//    already headed for kOptimal, is untouched.
//
// 6. TRUST-REGION SOFT BOUNDS -- an l-infinity trust region around the
//    current SQP iterate, expressed the same way every other bound is.
//
//    EFFECTIVE BOUNDS, computed ONCE about the clamped seed primal x0
//    (start_center()'s cold clamp(0,l,u) or warm clamp(seed.x,l,u)) -- BEFORE
//    the seed's bound-state hints are materialized onto x0 (step 1b; see
//    WINDOW-CONSISTENCY RULE below):
//        lo_eff(i) = max(lower(i), x0(i) - Delta),
//        up_eff(i) = min(upper(i), x0(i) + Delta),   Delta = opts.tr_radius.
//    Every subsequent bound read in the engine sees lo_eff/up_eff, never
//    lower/upper directly -- via a SHADOWED QpProblem reference: `qp` inside
//    run() names either the caller's own problem unchanged (Delta == +inf) or
//    a local copy with lower/upper replaced, so every function taking
//    `const QpProblem &qp` is already TR-aware.
//
//    CROSSED EFFECTIVE BOUNDS CANNOT HAPPEN: step 0 already rejected a
//    crossed real box, so lower(i) <= x0(i) <= upper(i) gives lo_eff(i) <=
//    x0(i) <= up_eff(i) always (asserted).
//
//    THE kFixed FLIP. lo_eff(i) == up_eff(i) can only happen at Delta == 0
//    for a variable not already genuinely kFixed. Every such variable is
//    flipped to BoundState::kFixed before the loop runs, with tr_active set;
//    an already-kFixed real bound is left alone (tr_active stays false).
//
//    tr_active (size n, parallel to bound_state) IS A SEPARATE ACTIVITY SET,
//    NOT A NEW BoundState: true at i iff the ratio test, the temporary-vertex
//    repair, or the zero-radius flip pinned i at the TR side of its effective
//    bound rather than the real one (a coincidental tie is attributed to the
//    real bound). Applied once at the end of run(), on the FINAL working set
//    only:
//      (a) a TR-pinned variable's bound_state reports kFree, never
//          kAtLower/kAtUpper/kFixed;
//      (b) its z entry is forced to 0 -- the ratio test and drop_worst still
//          see and act on the real priced multiplier while the loop runs, but
//          that number is never exposed;
//      (c) none of this changes what the loop DID, only how the final ws/z
//          are reported.
//
//    WINDOW-CONSISTENCY RULE. A seeded bound-state hint is applied against
//    the window, and a hint whose bound falls outside [lo_eff(i), up_eff(i)]
//    is DROPPED -- index i arrives kFree and the loop re-derives its activity
//    from the effective bounds. Clamping such a hint to the window edge
//    instead would break bound_state's meaning (kAtLower/kAtUpper means
//    sitting on the REAL bound); tr_active is the documented channel for a
//    TR-tight edge. A hint INSIDE the window is honoured unchanged.
//
//    WARM-START SEED INGESTION IGNORES tr_active BY CONSTRUCTION:
//    ingest_seed_working_set() reads only seed.bound_state/seed.ineq_active.
//    Combined with rule (a), a variable TR-pinned on the solve that produced
//    `seed` already arrives as bound_state == kFree and is left untouched --
//    the TR pin is not carried into the new solve's working set.
//
//    HOT-START REUSE INTERACTION (border mode). K0's structural/values hashes
//    never depend on lower/upper, so a bound change (real or TR-driven) can
//    NEVER poison K0 reuse on fingerprint grounds: a pin's border column is
//    e_i (BorderOps::pin_variable), independent of the bound value, and only
//    the border's RHS entry carries that value, rebuilt from the CURRENT `qp`
//    on every solve_bordered_eqp call. What a bound change CAN do is change
//    ws.bound_state() at the seed, which reuse condition (b) already treats
//    as an ordinary working-set change.
//
//    PER-SOLVE RADIUS VARIATION. tr_radius lives on QpOptions, which is
//    per-instance const, but every solve() overload also takes a
//    `const SolveOverrides &` (qp_types.h) resolved ONCE at the top of run()
//    into effective tr_radius/primal_delta/dual_mu (a sentinel field resolves
//    to the corresponding opts_ value); every read site in qp_engine.cpp consults those
//    effective values, not opts_ directly. A shrink-radius retry loop
//    therefore shares one QpEngine across every retry radius and keeps
//    hot-start reuse wherever the ordinary eligibility conditions allow.
//
//    UNBOUNDED-ARTIFACT GUARD AND REPAIR SYNERGY. is_runaway() and
//    repair_temporary_vertex() both read bounds through the same shadowed
//    `qp`, so with a finite tr_radius every variable has a finite effective
//    bound on both sides: is_runaway() can never fire for a TR-bounded
//    variable, and repair_temporary_vertex()'s "no finite bound to pin"
//    failure mode cannot occur.
//
//    BIT-IDENTICAL OFF PATH. opts.tr_radius defaults to +inf; at that value
//    no QpProblem copy and no effective-bounds vector is ever materialized,
//    and the shadowed `qp` aliases the caller's problem directly (the "zero
//    new work" claim is scoped to exactly those two allocations).
//    QpSolution::tr_active is still allocated unconditionally on every solve
//    (all false) -- part of QpSolution's contract.
//
// 6b. THE EXPORT INVARIANT ON z: A FREE VARIABLE CARRIES NO BOUND PRICE.
//    For every index i, on EVERY status this engine can return:
//        bound_state[i] == kFree  =>  z(i) == 0.0
//    Enforced at the point of export in run(). `refine_on_face` -- the
//    engine's other public QpSolution producer -- satisfies the same
//    invariant by a DIFFERENT route (price()'s own postcondition plus its own
//    TR-exclusion pass); a third producer must re-derive the invariant rather
//    than assume it is inherited.
//
//    ONE-WAY GUARANTEE: a PINNED index's z is the price the last price() call
//    computed, which on a kMaxIter exit may be one working set stale -- a
//    caller needing fresh prices needs a converged solve. `kFixed`
//    (lower == upper) is silently outside this invariant.
//
// --- Degeneracy ---
//
// Linearly dependent working sets (e.g. duplicated inequality rows) do not
// break the loop: the delta/mu regularization in assemble_kkt keeps the KKT
// matrix factorizable, and the dependent rows simply split the multiplier
// between them. No anti-cycling rule (Bland/least-index) is implemented;
// a degenerate stall is bounded by max_iter.
```

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 891–927

`struct EliminatedFace`: the two-half key and the fix-1 failure that established the second half. The key and the entry condition are kept.

```text
// THE FACE THE ELIMINATION PATH'S `kkt` CURRENTLY HOLDS (M6 W2 T7, fix 2).
// Captured where solve_eqp factorizes, read at the verdict site so the face
// refinement can REUSE that factorization instead of buying its own.
//
// THE KEY FOLLOWS THE FACTOR. The signature has two halves and BOTH are
// necessary: WHICH SYSTEM was factorized (the working set) and WHICH
// FACTORIZATION `kkt` is holding now (the factor's own identity). Fix 1
// recorded only the first, and probe_inertia -- which re-factorizes `kkt` for
// a HYPOTHETICAL working set -- could leave the key armed on a signature the
// live working set still matched: the guard then reused a factorization of a
// different, differently-SIZED system, and solve() threw out of
// SymmetricFactor::solve. Both halves are now structural.
//
// HALF ONE, THE SYSTEM. assemble_kkt keys K on exactly two things once the
// problem and the effective (primal_delta, dual_mu) are fixed: which variables
// are FREE -- the elimination partition, everything else being substituted out
// -- and which inequality rows are working, in order. Both are recorded WHOLE
// rather than as counts: refresh_shifts ADDS rows and drop_worst REMOVES them,
// and neither changes n, so no pair of sizes distinguishes the faces.
//
// HALF TWO, THE FACTORIZATION. (session_id, epoch) is the identity the FACTOR
// OBJECT owns and advances inside its own factorize path -- symmetric_factor.h
// names it the identity triple's live half, analyze() moving the session id
// and every successful factorize() advancing the epoch, the same mechanism
// BorderState's stale-handle note relies on. Nothing here stamps it, so it is
// bumped by EVERY path that factorizes `kkt` and not by an enumerated set of
// call sites: today that is solve_eqp (through eliminated_candidate) reached
// either from the loop's own eqp_candidate or from probe_inertia, and any
// future writer is covered by construction. rebuild_k0 factorizes
// `border.kkt`, never this one. The strength of the invariant IS this: no
// enumeration of writers, an identity the factor advances itself.
//
// `factorized` is the entry condition, not part of the signature: it is true
// only on an iteration whose candidate came from solve_eqp on THIS `kkt`. A
// bordered candidate leaves it false (`kkt` then holds whatever the last
// fallback left), and so does solve_eqp's empty-reduced-system short-circuit,
// which factorizes nothing at all.
```

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 951–991

`struct HotState`: the ownership rule, the two mechanisms that close the stale-contents gap, the concurrency caveat and the post-elastic/post-SOC readings. All kept, compressed.

```text
// HOT-START LEVEL. The opaque handle behind warm_start.h's WarmStart::hot --
// forward-declared there and DEFINED here. A frozen copy of the
// fingerprint/exit-state members QpEngine::run() tracks per instance, plus
// shared ownership of the BorderState those fingerprints describe (the K0
// symbolic analysis and last numeric factorization, Pardiso pt handle
// included).
//
// OWNERSHIP. BorderState wraps a move-only SymmetricFactor by value and is
// itself non-copyable and non-movable. `border_` is a
// std::shared_ptr<BorderState>; HotState::border is a COPY of that same
// shared_ptr. The backend session is released exactly once, when the LAST
// shared_ptr is destroyed, so an engine adopting a hot handle can
// factorize/solve against it safely even if the producing engine is gone.
//
// LIFETIME SAFETY ALONE DOES NOT GUARANTEE THE CONTENTS STILL MATCH A
// HOLDER'S FROZEN FINGERPRINT: a producer that solves again after emitting a
// handle can mutate the SAME shared BorderState. TWO MECHANISMS close this:
//   - DETACH (`border_ = std::make_shared<BorderState>()` at a refused-reuse
//     site whose `border_.use_count() > 1`; a sole-owned object is wiped in
//     place instead, to keep its KktFactor's symbolic analysis reusable) is
//     the LOAD-BEARING fix: the only engine that can ever write into a SHARED
//     BorderState is one whose own conditions (a)-(e) already passed for it,
//     so every K0 written into a shared object already carries the values any
//     handle's fingerprint describes, and sync_borders()'s unconditional
//     reconciliation absorbs the rest.
//   - The factor's IDENTITY pair (session_id/epoch) plus this engine's own
//     committed copy is DEFENSE-IN-DEPTH: reuse condition (e) compares a
//     HotState's frozen pair against a LIVE read off the shared object on
//     every solve() call, and its usable-numerics conjunct closes the
//     failed-rebuild case (no epoch advance). A mismatch degrades to kWarm
//     silently, never a throw.
//
// CONCURRENCY, NOT SEQUENCING, IS WHAT REMAINS UNSAFE -- see the header
// contract's THREAD SAFETY note. SAME-PROCESS ONLY (per warm_start.h):
// HotState is never serialized.
//
// AFTER AN ELASTIC/SOC RE-SOLVE: an ELASTIC re-solve builds an AUGMENTED
// (original-plus-slack) K0, so a handle emitted right after it silently
// forfeits kHot for the next major (safe, degraded to kWarm); an SOC re-solve
// shifts only be/bi, so a post-SOC handle remains an ordinary (a)-(e)-gated
// reuse candidate.
```

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 1069–1110

`refine_on_face`: what it does, what is gated and what is not, the rank pre-screen, the cost and state rules, the return contract and the public-API precondition. All kept, compressed.

```text
    // TIER 3: EXACT REFINEMENT ON AN EXTERNALLY IDENTIFIED FACE.
    //
    // ONE exact equality-constrained solve on the face `face` names, plus this
    // engine's ordinary iterative-refinement step -- i.e. `solve_eqp`, the
    // same function the walk's per-minor `eqp_candidate` calls, reached here
    // WITHOUT a walk. A kernel that IDENTIFIES an active set to its own
    // tolerance (today: the semismooth-Newton tier, ssn_engine.h) hands that
    // set here and gets back the point the set determines EXACTLY -- an
    // ACTIVE-SET solve's complementarity is an EXACT identity, where an FB
    // kernel stopping at |phi| <= fb_tol bounds its own only by
    // `fb_tol * ||lambda||inf`.
    //
    // GATED: the refined point must be a legal answer to the SUBPROBLEM --
    //   finite, inside the real box, inside the trust region, and satisfying
    //   every inequality row NOT on the face to the row-scaled feasibility
    //   tolerance. On failure this function REFUSES and the caller keeps the
    //   certificate it already had.
    // NOT GATED: the sign of the refined multipliers. The face is the
    //   CALLER's; this function re-solves it exactly and does not re-judge it,
    //   exactly as `eqp_candidate` does not.
    //
    // THE RANK PRE-SCREEN, before anything is factorized: a face with more
    // equality rows (model equalities + working inequalities) than free
    // variables cannot be a regular face, and handing its singular K to the
    // backend would trade a usable answer for a thrown Pardiso error.
    // Numerical singularity is caught one step later by the same
    // `detail::inertia_verdict` gate the walk applies.
    //
    // COST AND STATE. `out.counters` reports ONLY what THIS call paid (at most
    // one factorization). NOTHING PERSISTENT IS TOUCHED: no `border_`, no
    // hash, no `border_valid_`, no ledger record, no `solve_counter_`.
    //
    // Returns true iff the refinement was ACCEPTED. `out` is written either
    // way: on refusal it is `face` verbatim (with this call's own cost), so a
    // caller may use it unconditionally.
    //
    // PUBLIC-API PRECONDITION: the trust-region gate below assumes its window
    // is centred at `clamp(0, l, u)` -- true today only because every seeding
    // site zeroes `seed.x` before it reaches this function. This function
    // takes no centre parameter and does NOT validate that assumption. A
    // caller that seeds from a non-zeroed point gets a window gated about the
    // wrong centre, silently.
```

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 1349–1374

`refine_face_for_verdict`: the entry condition, the provenance rule and the closed-or-nothing adoption rule. All kept, compressed.

```text
    // THE VERDICT-SITE FACE REFINEMENT (section 5's dead-end classification).
    // Re-forms the live bordered system, refines it against a target expressed
    // in ROW units rather than the bordered loop's penalty-scaled footprint,
    // and OVERWRITES `x` with the result -- returning true iff it did.
    //
    // ON A BORDERED CANDIDATE ONLY (EqpResult::refine_steps > 0), and only at a
    // dead end whose classification would otherwise be kInfeasible: the caller
    // gates on both. An iteration that reached the elimination path -- under
    // kRefactorize, or through any of border mode's fallbacks and its latch --
    // is the eliminated twin's, because the residue this one removes is the
    // BORDERING residue (the two paths solve DIFFERENT regularized problems on
    // a closed face: elimination pins a variable exactly, border mode realizes
    // it as a -dual_mu row). A point already headed for kOptimal has no verdict
    // to buy.
    //
    // CLOSED, OR NOTHING. A candidate is adopted only if the face reaches its
    // target AND strictly improves on the walk's own point. A refinement that
    // spends its budget with the face still open has shown the face is not
    // closable, which is evidence FOR the classifier: adopting the better-but-
    // still-open point would push the violation under condition (b)'s fraction
    // of the footprint without making the row feasible, certifying a genuine
    // contradiction kOptimal. That rule is what keeps the whole inconsistent
    // population's verdicts identical to BASE, measured.
    //
    // It never touches a solve that does not reach that branch, so every
    // trajectory the walk reports elsewhere is untouched by construction.
```

**SOURCE** 1997159 · include/hven/detail/qp/qp_engine.h · lines 1380–1455

`refine_eliminated_face_for_verdict`: why it exists (with the measured eight-cell family), what it costs and when, why the factorization reuse is sound, the never-swallow argument with its registered backend caveat, and the carried D1/D4 clauses. Every rule is kept; the measurement is here.

```text
    /// @brief `refine_face_for_verdict`'s ELIMINATED twin (M6 W2 T7, DECLARED
    /// contract change; the dispatch and the cost rule amended by its fix
    /// round 1). Same entry condition, same target, same closed-or-nothing
    /// adoption rule, same counter -- reached whenever the candidate came off
    /// `solve_eqp`, which is every kRefactorize iteration AND every border-mode
    /// iteration served by a fallback or by the latch.
    ///
    /// WHY IT EXISTS. The border twin's own note says the eliminated path
    /// "leaves no bordering residue to remove", and that is true: there is no
    /// SCATTER here, because a pinned variable is eliminated exactly rather
    /// than realized as a `-dual_mu` row. What the two paths DO share is the
    /// dual regularization on the working rows, whose footprint is
    /// `dual_mu * |lambda|` on a row residual -- and `solve_eqp` takes exactly
    /// ONE step of iterative refinement against it, which on a face whose
    /// multipliers are inflated by the objective is not enough. Measured: at
    /// the SHIPPED `dual_mu` the ill-scaled feasible family reads kInfeasible
    /// under kRefactorize while kSchurBorder, post-T6b, reads kOptimal --
    /// eight cells in which the EQUIVALENCE ORACLE is the less accurate of the
    /// two. Iterating the step `solve_eqp` already takes closes all eight.
    ///
    /// WHAT IT COSTS, AND WHEN. The system is re-assembled from the CURRENT
    /// working set -- `assemble_kkt` is a triplet build over H/Ae/Ai's working
    /// rows plus one sort, no backend call and no session, so the assembly is
    /// not the cost. The FACTORIZATION is, and it is bought only when it has
    /// to be: the twin REUSES `kkt` and pays `solve_vec` calls alone whenever
    /// `face` HOLDS -- the live working set matching the captured one AND
    /// `kkt`'s factor still standing at the captured (session_id, epoch). Two
    /// things break that, and the key catches both: a working-set change
    /// between the candidate solve and this call (`refresh_shifts` adding a
    /// row), and a RE-FACTORIZATION of `kkt` by any other path in between
    /// (`repair_temporary_vertex`'s inertia probes, whose working set is
    /// hypothetical and whose system is a different SIZE). On such a MISS the
    /// twin assembles and factorizes its own `KktFactor`, charging one
    /// `factorizations` and one `symbolic_analyses` (rebuild_k0's accounting
    /// rule: the decision taken before the factorize and handed to it). A hit
    /// charges NEITHER.
    ///
    /// WHY REUSE IS SOUND, AND WHAT WOULD BREAK IT. `assemble_kkt` keys K on
    /// the elimination partition and the working rows -- which `face` records
    /// whole -- and on the problem and the effective (primal_delta, dual_mu),
    /// which are FIXED across one iteration: the suspect-stall ladder's
    /// `eff_opts.primal_delta *= kSuspectDeltaFactor` fires in the
    /// would-be-kOptimal branch, strictly AFTER this classification, and then
    /// restarts the iteration. A future reorder that moved an options change
    /// ahead of the verdict site would silently break the identity, so it must
    /// not: `face` guards the working set and the factor, not the options.
    ///
    /// NEITHER BRANCH DECLINES, AND NEITHER SWALLOWS. On a miss the twin
    /// factorizes a system THE WALK NEVER SOLVED -- the candidate's face plus
    /// whatever `refresh_shifts` added -- and that system can be exactly
    /// singular at a LEGAL setting: `dual_mu = 0` means no dual
    /// regularization, so a row dependent on the face leaves K singular rather
    /// than quasi-definite. NO DECLINE EXISTS FOR THAT, because the backend
    /// never reports it: `FactorizeOutcome::Status` is kOk or kBackendError
    /// with nothing between them, and MKL Pardiso's default static pivoting
    /// PERTURBS an exactly singular K and returns success -- measured by
    /// test_kkt_calls.cpp's
    /// `SqpKktOptions.ARankDeficientKktIsPerturbedRatherThanFailedOnThisBackend`,
    /// with the walk-level `dual_mu = 0` cell pinning that nothing escapes
    /// `solve()` either. What is left in `factorize_checked`'s
    /// std::runtime_error here is therefore GENUINE backend fault alone (out
    /// of memory, reordering, a failed `analyze()`) -- section 4's
    /// never-swallow class -- so it PROPAGATES, as does the hit branch's
    /// `solve_vec`. REGISTERED: a backend that reports singularity AS A STATUS
    /// gets the decline back, discriminated on that status the way the border
    /// twin discriminates on `needs_refactorization()`; Accelerate's behaviour
    /// on an exactly singular KKT is UNOBSERVED (macOS lane). The attempted
    /// factorization stays charged, eliminated_candidate's convention.
    ///
    /// D1/D4, AND THEY NOW HOLD IN BOTH ALGEBRAS (M6 W2 T6b's clauses, carried
    /// here): a kInfeasible exit RETURNS the refined point when it was adopted,
    /// so D1's consumers -- the elastic seed, the refusal return and the
    /// restoration trial -- read the refined point on either path; "closed" is
    /// the CLASSIFIER's own tolerance and nothing tighter; and DUALS ARE NOT
    /// REFINED, `best`'s multipliers being discarded exactly as the border
    /// twin discards its own.
```

### include/hven/model/non_linear_program.h

1629 lines / 919 comment lines at `1997159`; 1595 / 885 after. The smallest change of the ten: this header was already close to the terse-contract style, and almost every block in it is an ordering, ownership, absence or refusal contract.

**SOURCE** 1997159 · include/hven/model/non_linear_program.h · lines 44–65

`enum class FixedVariableTreatments`: the three treatments described one paragraph each. Kept, compressed into one.

```text
/// How a primal variable whose declared lower and upper bounds are equal is
/// handed to the solver. All three are implemented, and all three reach the same
/// solution on a well-posed problem; they differ in the size of the system the
/// solver factorizes and in how exactly the variable sits at its value.
///
/// MakeParameter (the default) removes the variable from the optimization
/// entirely: it is pinned at its bound value for every evaluation and the Newton
/// system the solver factorizes is the system of the REMAINING variables, one
/// row and column narrower per fixed variable. Its value in the returned
/// solution is exact.
///
/// MakeConstraint keeps the variable free and adds one internal equality row
/// x_i - c = 0 per fixed variable, appended AFTER every row the transcription
/// declared, so the solved system is one row and one column WIDER per fixed
/// variable than MakeParameter's and every user row keeps its own index. The
/// variable reaches its value to equality-constraint tolerance rather than
/// exactly.
///
/// RelaxBounds keeps the variable as an ordinary two-sided bounded variable with
/// its bounds pushed apart by the relax factor, so it is held near its value by
/// the barrier, within the relaxation, and the system is the same size as the
/// declared problem's.
```

**SOURCE** 1997159 · include/hven/model/non_linear_program.h · lines 98–138

`struct NonLinearProgram`: the two fill paths and the determinism argument for the right-hand-side intermediate, including the mode-dependence carve-out. The two paths, the bit-identical guarantee and the layout-determinism rule are kept.

```text
/// @brief The partitioned evaluation engine, and a Level 2 provider.
///
/// assemble() reaches the same per-shape passes the eval_ entry points do,
/// call for call, over the same machinery; the only step moved out to the
/// consumer is the one the mapping table transfers (the solver-coefficient
/// scatter, which assemble deliberately does not do and the eval_ entries
/// still do).
///
/// THE FILL PATH, both halves, because the capability declaration turns on
/// the difference:
///
///   * The KKT fill is DIRECT. Each piece writes the consumer's value array in
///     place, at offsets its claim recorded (kkt_locations_), under the
///     canonical-column lock protocol. There is no provider-owned matrix and no
///     copy: this is the per-minor cost center and it is not paying for one.
///   * The RIGHT-HAND-SIDE fill goes through a provider-owned intermediate --
///     each piece accumulates into its own claim slots in rhs_coeffs_, and
///     fill_pgx/fill_agx/fill_fxe/fill_fxi then fold those slots into the
///     consumer's vectors through rhs_coeff_rows_.
///
/// THAT INTERMEDIATE IS REQUIRED, not merely tolerated, and the reason is
/// determinism rather than convenience. Several pieces claim rows of one
/// gradient, so an in-place scatter would have to lock per row, and the order
/// in which contending threads won those locks would decide the order the
/// floating-point additions happened in -- making the assembled right-hand
/// side depend on scheduling, and therefore on the evaluation-thread count.
/// Claim slots are contention-free by construction (one piece owns each), and
/// the fold that follows walks them in claim order, so the accumulation order
/// is a property of the layout alone: the same problem produces bit-identical
/// right-hand sides at any thread count. This library's pins rest on that
/// stability, and ON THE DETERMINISTIC PATH -- the default, and the path
/// every pin and measurement runs on -- it outranks the no-copy property:
/// removing the intermediate there would be a regression, not an
/// optimization. Accumulation-VALUE determinism is a property of this path,
/// not a library absolute: a future user-selectable max-performance fill may
/// relax it, exactly as threaded MKL already does, behind an explicit mode
/// choice -- never silently, and never as this path's default. What stays hard
/// everywhere, on every path and for every provider, is LAYOUT determinism:
/// claim order, structural keys, and location tables are untouched by that
/// option; keys, byte-stable pins, and warm-start identity rest on them, and
/// only the floating-point summation order is ever mode-dependent.
```

**SOURCE** 1997159 · include/hven/model/non_linear_program.h · lines 146–156

The master piece lists' banner: everything derived from them, and the two guards. Kept, compressed.

```text
    // THE THREE MASTER PIECE LISTS ARE PUBLIC, AND WRITING ONE IS A STRUCTURAL
    // MUTATION. Everything derived from them -- the element counts, the work
    // partitioning, the claim arrays, the location tables, both digests and the
    // published claim stream -- describes the lists AS LAID, so a write here is
    // declared by re-laying: make_nlp(), or adopting a declaration that carries
    // the new pieces. Reading the declaration or the structural key without one
    // is REFUSED by name (require_master_lists_unmoved), and the published claim
    // stream is rebuilt rather than retained at the next lay whenever a piece on
    // one of these lists is not one this layout laid. Neither guard can see a
    // master entry assigned FROM ANOTHER LAID PIECE of the same problem, which
    // is the one case this sentence, and not a mechanism, has to carry.
```

**SOURCE** 1997159 · include/hven/model/non_linear_program.h · lines 378–405

The bound-fixed variable treatment banner: the elimination's input/output map split, the claim invariance and the identity fast path. All kept, compressed.

```text
    // Bound-fixed variable treatment
    //
    // A variable declared with lower == upper carries no degree of freedom.
    // Under the MakeParameter treatment it is ELIMINATED: it does not appear in
    // the solver's variable space at all, and the KKT system the solver
    // factorizes is the system of the remaining variables -- narrower by exactly
    // one row and column per eliminated variable. The elimination splits each
    // function's index map by role: the INPUT map is left alone -- a function
    // still reads exactly the variables it was declared over, out of a
    // full-space buffer built from the reduced iterate plus the pinned values --
    // and only the OUTPUT map is rewritten, at configuration time, from the
    // pristine input map: retained variables renumbered into the reduced space,
    // eliminated ones marked -1, and the KKT/RHS location tables, sparsity
    // pattern, clash marks and solver-coefficient ranges rebuilt over it.
    //
    // Element CLAIMS stay exactly as they were -- same count, same contiguous
    // per-application ranges -- because the scatters walk their claims in
    // lockstep with the function's own loop bounds; a -1 element keeps its claim
    // and simply names no matrix entry.
    //
    // Identity fast path. With no fixed variables nothing is rewritten at all:
    // no output map is installed and no expansion buffer is built. The other two
    // treatments take that path throughout -- neither eliminates anything -- and
    // differ only in what classification records: MakeConstraint records no
    // bound for the variable (it goes to the solver free) and appends one
    // internal equality row per fixed variable (re-laying the layout over the
    // widened row space), RelaxBounds records an ordinary relaxed bound pair
    // (changing nothing structural).
```

**SOURCE** 1997159 · include/hven/model/non_linear_program.h · lines 1102–1125

The published claim stream's banner: why it exists, what the views are valid under, and that it is a surface beside the raw one. All kept, compressed.

```text
    // =======================================================================
    // THE PUBLISHED CLAIM STREAM -- the same laid slots the arrays above carry,
    // RESTATED into the claim convention a claim-stream consumer reads.
    //
    // WHY IT EXISTS AT ALL. The raw arrays are laid PARTITION-MAJOR, in the
    // square space the solver factorizes: n + slacks + me + mi on a side, with
    // Hessian pairs left in the walk order the piece claimed them in. The
    // claim-stream contract (model/claim_stream_source.h) states its claims in
    // the DECLARATION's square space -- n + me + mi, no slack block, Hessian
    // upper triangle -- and wants one contiguous run per domain. A consumer that
    // wants those claims therefore had to build them itself, per transcription,
    // out of a copy of these arrays. It does not any more: the layout builds
    // them once at the lay, and publishes VIEWS.
    //
    // WHAT THE VIEWS ARE VALID UNDER is claim_stream_epoch(), which is NOT the
    // structure epoch -- see its own comment, and the VIEW VALIDITY term in
    // model/claim_stream_source.h. Nothing here owns storage; every accessor is
    // a view into one arena.
    //
    // THIS IS A SURFACE BESIDE THE RAW ONE, NOT OVER IT. Nothing here renumbers
    // a raw slot, moves the emission order, or is fed to claim_digest(): the
    // interior-point engine reaches this layout through get_mat_space /
    // get_kkt_space and the location tables, and never through these.
    // =======================================================================
```

### include/hven/linear/symmetric_factor.h

1040 lines / 799 comment lines at `1997159`; 1017 / 776 after. Deliberately the lightest pass of the ten — see the note below.

**Why this header was left almost entirely alone.** Its comment mass is the MKL
`iparm` and Apple Accelerate contract surface: for every option, the vendor entry
it writes, the DON'T-WRITE-BY-DEFAULT mechanic, the quoted capability condition
from Intel's own reference, the dependency that is validated at construction, the
backend that throws and why, and what was OBSERVED on the MKL version this was
verified against as against what the vendor documents. CLAUDE.md §6 makes an
`iparm` change conditional on citing validation evidence — these comments ARE
that evidence, in the place the rule expects to find it, so compressing them
would remove the record a future change is required to cite. Nothing in the
Options struct was touched. Two lifecycle blocks that carried an argument rather
than a contract were compressed.

**SOURCE** 1997159 · include/hven/linear/symmetric_factor.h · lines 697–735

`set_num_threads`: the call-scope argument for why this is the only Option with a setter, and the four contracts around it (no re-analysis, the throw, shared sessions, the synchronization rule, the Accelerate treatment). Every contract is kept; the argument is compressed.

```text
    // Repoint this engine's thread count, effective from the NEXT backend
    // call. Nothing else moves: the backend session, the analyzed pattern,
    // the symbolic analysis and the current numerics all survive, so this
    // costs no re-analysis and no refactorization.
    //
    // That is a statement about where the count lives, not a convenience:
    // the thread count is applied AT CALL SCOPE (see Options::num_threads),
    // so it was never part of the symbolic factorization in the first place.
    // Every other option IS baked into the session the analysis lives in and
    // therefore has no setter -- changing one means building a new engine.
    //
    // Throws std::invalid_argument for a negative count, the same rule the
    // constructor applies to Options::num_threads; the engine is left
    // untouched when it throws.
    //
    // SHARED SESSIONS. The count belongs to the backend session, which this
    // engine may be co-owning with handles emitted by share() and with
    // engines built by adopt(). A change here therefore governs every
    // co-owner's subsequent calls on that session, which is the same
    // one-session-between-them rule the class's THREAD SAFETY and SHARING
    // notes already state; it also means a later share()/adopt() round trip
    // reports the CURRENT count rather than the one the session was analyzed
    // with. Before the first analyze() there is no session yet, and the value
    // set here is what the next analyze() builds one with.
    //
    // NOT INTERNALLY SYNCHRONIZED, exactly like a solve. This writes state
    // the session reads on every backend call, so a caller must serialize it
    // against any call in flight on that session -- including one issued by a
    // different co-owner on another thread. It is the class's THREAD SAFETY
    // rule extended to the one mutation that crosses co-owners: solves across
    // co-owners are the caller's to serialize, and so is this.
    //
    // BEST-EFFORT-ABSENT ON ACCELERATE, exactly as Options::num_threads
    // itself is: that backend exposes no per-instance thread control, so the
    // value is stored (keeping the Options round trip honest) and applied to
    // nothing. The call is accepted and validated there rather than
    // rejected -- a plain thread count is a request a backend may honestly
    // not be able to honor, unlike Options::cnr_threads, whose reproducibility
    // GUARANTEE that backend refuses outright rather than silently drop.
```

**SOURCE** 1997159 · include/hven/linear/symmetric_factor.h · lines 738–762

`num_threads()`: why the read is through the session rather than a snapshot, with the counter-factual. The read-through rule and the pre-analyze and post-analyze readings are kept.

```text
    // The thread count that governs THIS ENGINE'S NEXT BACKEND CALL.
    //
    // Once there is a session, that is the SESSION's current count, read
    // through on every query rather than snapshotted -- because the session is
    // where a backend call reads it from, and because the session may be
    // co-owned (see set_num_threads()' SHARED SESSIONS note). A
    // set_num_threads() on any ONE co-owner moves the count for all of them,
    // and each of them reports the moved value here; a snapshot of this
    // engine's own Options would have kept reporting the count this engine
    // last agreed to, which is not the count its next solve will run at.
    //
    // Before the first analyze() there is no session yet, so this reports what
    // the constructor was given as moved by set_num_threads() -- which is
    // exactly what the next analyze() will build a session with.
    //
    // A LATER analyze() ON THIS ENGINE therefore RESETS what this reports, and
    // deliberately so: analyze() forks a fresh session from this engine's OWN
    // Options (a co-owner's count was never this engine's to inherit past the
    // session it was set on), so from that point the read-through and the
    // snapshot agree again on this engine's own value.
    //
    // The only Option with a reader, for the same reason it is the only one
    // with a setter: it is the only one that can change after construction,
    // so it is the only one a caller cannot already know from the Options it
    // passed in.
```
