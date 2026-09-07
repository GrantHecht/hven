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
