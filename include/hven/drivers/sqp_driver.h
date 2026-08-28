// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

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
// --- THE RESTORATION PHASE -------------------------------------------------
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
// entry iterate) has f_w = h(x_entry) exactly. There is no penalty parameter
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
#include <hven/model/nlp_model_aggregate.h>
#include <hven/qp/qp_types.h>
#include <hven/warmstart/ipm_polish_extension.h>
#include <hven/warmstart/warm_start_data.h>

namespace hven::solvers {

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
/// @brief One model evaluation at one point: the five quantities a major
///        iteration reads, taken together so nothing is evaluated twice.
struct NlpEval {
    /// @brief Objective value at the evaluation point.
    double f = 0.0;
    /// Objective gradient, equality-constraint values, inequality-constraint
    /// values; sizes n(), me(), mi() respectively.
    Vec grad, ce, ci;
    /// @brief Equality and inequality constraint Jacobians; me() x n() and mi() x n().
    Eigen::SparseMatrix<double, Eigen::RowMajor> Je, Ji;

    /// False if any of f/grad/cE/cI is NaN or infinite. See evaluate_kkt's
    /// NON-FINITE ITERATES note for why this is tracked explicitly.
    bool all_finite = true;
};

/// Evaluates the model once at x. Empty constraint blocks are skipped
/// entirely (a model with me() == 0 gets no eval_ce/eval_jac_e call at all),
/// so the per-major call count is exactly 2 + 2*[me>0] + 2*[mi>0], plus one
/// eval_hess per accepted iterate -- and, on a solve that accepts none
/// because it builds no subproblem at all, the single eval_hess
/// make_warm_start's zero-major probe pays instead (see NlpEval above).
///
/// THE CALLBACK RETURNS ARE CHECKED, NOT ASSUMED, and this is a wrong-answer
/// guard rather than a courtesy: the five RETURNS are the MODEL's, and nothing
/// downstream re-measures them. A short return propagates past allFinite()
/// (vacuously true on an empty vector) into an out-of-bounds read in Release,
/// where Eigen's own asserts are compiled out. Each check is an O(1) integer
/// comparison against a declared dimension, and each throw NAMES THE CALLBACK.
///
/// @throws std::invalid_argument if `x` is mis-sized or any callback return's
///         shape contradicts the model's declared dimensions.
NlpEval eval_nlp(const NlpModel &model, const Vec &x);

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
/// @brief Evaluates f, cE and cI at `x` only -- no derivatives.
/// @param model The problem.
/// @param x     The point; must have size model.n().
/// @return An NlpEval whose grad/Je/Ji are left empty and whose `all_finite`
///         covers f/cE/cI only.
/// @throws std::invalid_argument if `x` is mis-sized, or if eval_values
///         returns a cE or cI whose size contradicts model.me()/model.mi().
NlpEval eval_nlp_values(const NlpModel &model, const Vec &x);

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
/// @brief Fills in the derivatives an eval_nlp_values() bundle is missing,
///        leaving its f/cE/cI untouched.
/// @param model The problem.
/// @param x     The point `ev` was taken at.
/// @param ev    The bundle to complete, in place.
/// @throws std::invalid_argument if eval_grad returns a size other than
///         model.n(), or eval_jac_e/eval_jac_i return a matrix whose shape
///         contradicts model.me()/model.mi() x model.n().
void upgrade_to_full(const NlpModel &model, const Vec &x, NlpEval &ev);

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
/// @brief The globalization violation measure h(x): the l1 norm of the
///        equality residuals plus the positive parts of the inequality rows.
/// @param ev An evaluation at the point of interest; bounds are NOT included.
/// @return h >= 0, or NaN if any constraint value is NaN -- non-finite values
///         propagate deliberately (see the note above).
double constraint_violation_l1(const NlpEval &ev);

// The KKT measurement of one NLP iterate. See this header's CONVERGENCE TEST
// note for the definition of every field; `residual()` is the single scalar
// the convergence test and the contraction test both read.
//
// When `finite` is false, stationarity/feasibility/complementarity/residual()
// are all NaN by construction -- see evaluate_kkt.
/// @brief The KKT measurement of one NLP iterate.
struct SqpKkt {
    /// @brief inf-norm stationarity residual.
    double stationarity = 0.0;
    /// @brief inf-norm constraint violation, bounds included.
    double feasibility = 0.0;
    /// @brief inf-norm complementarity residual.
    double complementarity = 0.0;
    /// @brief grad f + Je^T lambda_e + Ji^T lambda_i (bound term NOT subtracted).
    Vec grad_lag;
    /// @brief Model-implied bound multipliers: grad_lag at an active bound, else 0.
    Vec z;

    /// False if x, or anything the model returned at x, or the resulting
    /// grad_lag, is NaN or infinite. A caller MUST branch on this before
    /// reading the residuals as numbers.
    bool finite = true;

    /// @brief The single scalar the convergence and contraction tests read.
    /// @return max(stationarity, feasibility); NaN when `finite` is false.
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
namespace detail {

// THE ARITHMETIC OF evaluate_kkt, over the five quantities it actually reads
// off its first argument: the three dimensions and the two bound vectors. It
// evaluates NOTHING -- every model quantity it uses arrives in `ev`.
//
// Factored out because the SQP driver's solve path runs on the Level 2
// aggregate contract: the loop no longer holds an NlpModel, and the seam
// publishes exactly these five. Both public entries forward here, so there is
// ONE body rather than two copies that could drift.
SqpKkt evaluate_kkt_over(Index n, Index me, Index mi, const Vec &lo, const Vec &up,
                         const NlpEval &ev, const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                         double bound_tol);

} // namespace detail

/// @brief Measures (x, lambda_e, lambda_i) against the model, reusing an
///        evaluation already taken at the SAME x.
/// @param model     The problem.
/// @param ev        An evaluation at `x`.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers, size model.me().
/// @param lambda_i  Inequality multipliers, size model.mi().
/// @param bound_tol Geometric bound-activity tolerance (the driver passes
///                  SqpOptions::feas_tol).
/// @return The measurement; every residual is NaN when SqpKkt::finite is false.
SqpKkt evaluate_kkt(const NlpModel &model, const NlpEval &ev, const Vec &x, const Vec &lambda_e,
                    const Vec &lambda_i, double bound_tol);

// Convenience overload that takes the evaluation itself. Costs one extra
// model evaluation over the bundle-taking form above, so the DRIVER never
// uses it -- it is for callers measuring a single point (tests, diagnostics).
/// @brief Convenience overload that evaluates the model itself.
/// @param model     The problem.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param bound_tol Geometric bound-activity tolerance.
/// @return The measurement.
/// @throws std::invalid_argument through eval_nlp, on a mis-sized `x` or a
///         callback return whose shape contradicts the declared dimensions.
SqpKkt evaluate_kkt(const NlpModel &model, const Vec &x, const Vec &lambda_e, const Vec &lambda_i,
                    double bound_tol);

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
/// @brief Builds the SQP subproblem at `x`, in the step variable p, from an
///        evaluation already taken at that x.
/// @param model     The problem.
/// @param ev        An evaluation at `x`; must be finite (the caller's
///                  obligation, not checked here).
/// @param x         The linearization point.
/// @param lambda_e  Equality multipliers the Hessian is formed at.
/// @param lambda_i  Inequality multipliers the Hessian is formed at.
/// @param obj_scale Objective scale applied to the gradient and the Hessian.
/// @return The pure linearization; no trust region is folded into the box.
QpProblem build_subproblem(const NlpModel &model, const NlpEval &ev, const Vec &x,
                           const Vec &lambda_e, const Vec &lambda_i, double obj_scale = 1.0);

// Convenience overload that evaluates the model itself. Costs one extra model
// evaluation over the bundle-taking form, so the DRIVER never uses it.
/// @brief Convenience overload that evaluates the model itself.
/// @param model     The problem.
/// @param x         The linearization point.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param obj_scale Objective scale.
/// @return The subproblem.
/// @throws std::invalid_argument through eval_nlp, on a mis-sized `x` or a
///         callback return whose shape contradicts the declared dimensions.
QpProblem build_subproblem(const NlpModel &model, const Vec &x, const Vec &lambda_e,
                           const Vec &lambda_i, double obj_scale = 1.0);

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
/// @brief The QP model's predicted objective decrease along a step.
/// @param qp The subproblem the step came from.
/// @param p  The step; must have size qp.n().
/// @return delta_m_f(p), positive on a model that promises progress.
/// @throws std::invalid_argument if `p` is mis-sized.
double predicted_decrease(const QpProblem &qp, const Vec &p);

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
/// @brief Builds a cold-start active-set seed from the subproblem's own
///        geometry, naming the rows and bounds that are already tight at x0.
/// @param qp       The first subproblem of a cold solve.
/// @param feas_tol Activity tolerance; a row or bound within it counts as tight.
/// @param seed     Receives the seed, populated in place.
/// @param rows     Receives the count of seeded inequality rows.
/// @param bounds   Receives the count of seeded bounds.
/// @return True iff the seed names anything at all; a seed naming nothing is
///         not offered to the engine.
bool crash_basis_seed(const QpProblem &qp, double feas_tol, QpSolution &seed, Index &rows,
                      Index &bounds);

} // namespace hven::solvers

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
#include <hven/detail/drivers/aggregate_eval_seam.h>
#include <hven/detail/globalization/sqp/elastic.h>
#include <hven/detail/globalization/sqp/restoration.h>
#include <hven/detail/globalization/sqp/soc.h>

namespace hven::solvers {

// evaluate_kkt over the SEAM's dimensions and materialized box, which are the
// model's own. This is the entry the driver's major loop uses; the
// NlpModel-taking one above is for every caller measuring a single point from
// a model in hand. See that entry's note -- it documents both.
//
// The seam is taken by CONST reference: this measurement evaluates nothing,
// so it neither re-lays nor scatters.
/// @brief evaluate_kkt over a SEAM's dimensions and materialized box -- the
///        entry the driver's major loop uses.
/// @param seam      The aggregate seam; taken by const reference because this
///                  measurement evaluates nothing.
/// @param ev        An evaluation at `x`.
/// @param x         The iterate.
/// @param lambda_e  Equality multipliers.
/// @param lambda_i  Inequality multipliers.
/// @param bound_tol Geometric bound-activity tolerance.
/// @return The measurement, under the model-taking overload's contract.
SqpKkt evaluate_kkt(const AggregateEvalSeam &seam, const NlpEval &ev, const Vec &x,
                    const Vec &lambda_e, const Vec &lambda_i, double bound_tol);

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
/// @brief May the driver shrink the radius and re-solve after a failed
///        subproblem, rather than giving up?
/// @param qp        The subproblem that failed.
/// @param qs        Its result.
/// @param bound_tol Bound-activity tolerance, applied as a band OUTSIDE each
///                  bound (the driver passes SqpOptions::feas_tol).
/// @return True only for a kNumericalError or kMaxIter exit whose `x` is
///         correctly sized, finite and inside the box; false for kOptimal and
///         kInfeasible, which are answers rather than failures.
bool qp_failure_is_retryable(const QpProblem &qp, const QpSolution &qs, double bound_tol);

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
/// @brief Trust-region slack the SSN certifying exit is allowed, as a multiple
///        of `fb_tol`; derived from ssn_engine.h's own constant rather than
///        restated as a literal.
inline const double kSsnTrViolationFactor = 2.0 * detail::kSsnComplementarityFactor;

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
/// @brief Does this SSN exit hand back something the funnel may use as a
///        trial step?
/// @param res    The kernel's result.
/// @param fb_tol The SsnOptions::fb_tol the solve ran at.
/// @return True only when the escape reason is kNone, the status is kOptimal,
///         and the point is finite and inside the trust region to within
///         kSsnTrViolationFactor * fb_tol.
bool ssn_exit_is_a_usable_step(const SsnResult &res, double fb_tol);

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
/// @brief Re-presents an SSN result as the QpSolution the driver's step and
///        warm-start paths consume.
/// @param res A usable SSN exit (see ssn_exit_is_a_usable_step).
/// @return The equivalent QpSolution, status kOptimal, with the activity
///         export carried across verbatim.
QpSolution ssn_result_to_qp_solution(const SsnResult &res);

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
/// @brief Clamp every strictly negative exported inequality price to 0.
/// @param lambda_i The exported inequality prices, swept IN PLACE.
/// @param counters Written, never read: `ssn_sign_swept` gains one per row
///        clamped, `ssn_sign_sweep_max` is raised to the largest magnitude
///        clamped. Both are cumulative across calls.
void sweep_negative_face_prices(Vec &lambda_i, SsnCounters &counters);

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
/// @brief Builds the SSN kernel's start object from the previous
///        subproblem's solution.
/// @param seed The prior QpSolution, or nullptr for no seed.
/// @return The start object; an empty one when `seed` is nullptr.
SsnStart ssn_start_from_qp_seed(const QpSolution *seed);

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
/// @brief The SSN kernel's `fb_tol` for a driver configured at these two
///        tolerances.
/// @param kkt_tol  SqpOptions::kkt_tol.
/// @param feas_tol SqpOptions::feas_tol.
/// @return The kernel tolerance derived from min(kkt_tol, feas_tol), so a
///         feas_tol tighter than kkt_tol is honoured.
double ssn_fb_tol_for(double kkt_tol, double feas_tol);

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
/// @brief Charges one SSN subproblem's cost to a solve's running totals.
/// @param total The solve's counters, updated in place.
/// @param res   The subproblem's result. Only factorizations and symbolic
///              analyses are folded -- `minor_iters` deliberately is not.
void charge_ssn_subproblem_cost(SqpCounters &total, const SsnResult &res);

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
/// @brief Charges a face refinement that was paid for and then refused.
/// @param total             The solve's counters, updated in place.
/// @param refined           The refinement's result.
/// @param ssn_budget_charge The probe budget, charged by the same
///                          factorization count.
void charge_refused_face_refinement(SqpCounters &total, const QpSolution &refined,
                                    Index &ssn_budget_charge);

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
/// @brief Folds one subproblem's SSN counters into a solve's running total.
/// @param total The running total, updated in place.
/// @param one   The subproblem's counters. The two peaks
///              (`ssn_uncertain_peak`, `ssn_sign_sweep_max`) aggregate by max;
///              every other field sums.
void accumulate_ssn_counters(SsnCounters &total, const SsnCounters &one);

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

// kappa_mu in the schedule above; 1.0, named so a future re-derivation has
// somewhere to change it.
/// @brief kappa_mu in the adaptive dual-regularization schedule above; named
///        so a future re-derivation has somewhere to change it.
inline constexpr double kAdaptiveMuKappa = 1.0;
/// @brief Floor of the schedule's dual regularization.
inline constexpr double kAdaptiveMuMin = 1e-12;
/// @brief Ceiling of the schedule's dual regularization; equals
///        QpOptions::dual_mu's own default.
inline constexpr double kAdaptiveMuMax = 1e-8;

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
/// @brief Width of the seeded-dual clamp band: an ingested lambda_i in
///        [-kSeededDualClampTol, 0) is clamped to 0, anything more negative
///        degrades the whole object to kCold. Fixed by design -- it does not
///        track opts_.kkt_tol at runtime.
inline constexpr double kSeededDualClampTol = 1e-6;

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
/// @brief The SQP driver: a trust-region SQP major loop over one QpEngine.
///
/// Every member below EXCEPT THE TWO CONSTRUCTORS -- and every free function
/// declared above -- is defined in src/drivers/sqp_driver.cpp; read that
/// file's banner before changing this class's structure.
class SqpDriver {
  public:
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
    explicit SqpDriver(const SqpOptions &opts) : SqpDriver(opts, /*allow_restoration=*/true) {}

  private:
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
    SqpDriver(const SqpOptions &opts, bool allow_restoration)
        : opts_(opts), engine_(opts.qp), allow_restoration_(allow_restoration) {
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
    /// @brief Solves from the model's own start_point().
    /// @param model The problem; wrapped in a bridge built here.
    /// @return The solution.
    /// @throws std::invalid_argument on a model that cannot describe a
    ///         problem, through the same aggregate-declaration validation the
    ///         explicit-start-point overload below documents -- a malformed
    ///         start_point() included.
    SqpSolution solve(const NlpModel &model);

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
    void attach_ledger(Ledger *ledger, std::string label_prefix);

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
    SqpSolution solve(const NlpModel &model, const Vec &x0);

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
    /// @brief Solves against an already-built bridge -- the primary path every
    ///        NlpModel-taking overload wraps.
    /// @param bridge the aggregate to solve over; caller-owned, per the notes
    ///               above.
    /// @param x0     the starting point.
    /// @return the solution.
    ///
    /// A value staged through stage_warm_start applies to this call and is
    /// consumed by it; `x0` is then only the cold fallback.
    /// @throws std::invalid_argument only through `bridge` itself (this entry
    ///         does not re-check the box -- see above), or through a staged
    ///         warm-start value whose block sizes or stamp do not match the
    ///         problem this call binds (see stage_warm_start).
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0);

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
    /// @brief Warm-start ingest against a model, wrapped in a bridge built here.
    /// @param model        the problem to solve.
    /// @param x0           the starting point; ignored per the ingest rule above
    ///                     whenever `warm` resolves above kCold.
    /// @param warm         the prior solve's warm-start object; the whole ingest
    ///                     contract is in the notes above.
    /// @param minor_budget the probe budget; 0 (default) = no budget, see above.
    /// @return the solution.
    /// @throws std::invalid_argument on a model that cannot describe a problem --
    ///         the same classes the 2-argument model-taking overload above
    ///         enumerates, refused by the same aggregate-declaration boundary.
    ///         NOTE that this happens BEFORE `warm` is looked at, so a malformed
    ///         model is reported as such rather than as a failed ingest.
    /// @throws std::invalid_argument if a warm-start value is staged on this
    ///         driver (stage_warm_start) when this overload is called: two
    ///         warm-start sources for one solve, refused naming both. That
    ///         refusal fires before the model's box is checked and before the
    ///         bridge is laid, and leaves the staged value standing.
    SqpSolution solve(const NlpModel &model, const Vec &x0, const WarmStart &warm,
                      Index minor_budget = 0);

    // The warm-start ingest against a bridge, on the same footing as the
    // 2-argument bridge overload above: same primary path, same seam, and the
    // whole of the ingest contract documented on the model-taking overload
    // just above this one.
    /// @brief Warm-start ingest against an already-built bridge, on the same
    ///        footing as the 2-argument bridge overload above.
    /// @param bridge       the aggregate to solve over; caller-owned.
    /// @param x0           the starting point; ignored per the ingest rule
    ///                     above whenever `warm` resolves above kCold.
    /// @param warm         the prior solve's warm-start object; see the
    ///                     model-taking overload just above this one for the
    ///                     whole ingest contract.
    /// @param minor_budget the probe budget; 0 (default) = no budget, see
    ///                     above.
    /// @return the solution.
    /// @throws std::invalid_argument only through `bridge` itself (this entry
    ///         does not re-check the box -- see the 2-argument overload
    ///         above), or if a warm-start value is staged on this driver when
    ///         this overload is called: two warm-start sources for one solve,
    ///         refused naming both. That refusal fires before the seam is
    ///         laid, and leaves the staged value standing.
    SqpSolution solve(NlpModelAggregate &bridge, const Vec &x0, const WarmStart &warm,
                      Index minor_budget = 0);

    // --- Warm-start currency ---
    //
    // The same two entries the interior-point engine carries
    // (drivers/interior_point_solver.h). That engine BINDS a problem once
    // (set_nlp); this one binds nothing until a solve() call names one, so
    // the two checks the currency owes -- the block SIZES and the STAMP --
    // are made at SOLVE ENTRY, against the problem that call binds, rather
    // than at staging.
    //
    // The staged value and the last completed solve's captured value are the
    // only state this class keeps between solves that a caller can see.
    // Neither is touched per iteration: the export is one capture at
    // completion, the staged value one branch at entry.
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
    WarmStartData export_warm_start() const;

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
    void stage_warm_start(const WarmStartData &data);

  private:
    // The ledger-recording tail every public solve() overload shares:
    // record exactly one SqpSolveRecord (if a ledger is attached) and
    // return the result unchanged. Factored out so the 3-arg overload does
    // not duplicate attach_ledger's contract. `wall_seconds` is measured by
    // each caller above, around solve_impl alone, and carried through into
    // the record.
    SqpSolution record_solve(SqpSolution out, double wall_seconds);

    // Refuses `std::invalid_argument` when a WarmStart argument arrives while
    // a value is staged, naming both sources and leaving the staged value
    // standing. Called FIRST by both WarmStart-taking overloads -- including
    // the model-taking one, which would otherwise pay a bridge lay before
    // discovering the contradiction.
    void refuse_two_warm_sources() const;

    // The staged value's one branch at solve entry, shared by both
    // bridge-taking overloads (the model-taking ones wrap those, so one
    // public call consumes at most once). Returns the `warm` object the solve
    // runs against: a default-constructed COLD one when nothing is staged,
    // otherwise the staged value, built through to_sqp_warm_start when it
    // carries the polish tag and assembled core-only when it does not. The
    // staged value is consumed BEFORE any check on it can throw.
    WarmStart consume_staged_warm_start(const AggregateEvalSeam &seam,
                                        const NlpModelAggregate &bridge);

    // The export's one capture at completion, taken after record_solve has
    // returned -- the last thing each bridge-taking overload does before its
    // own return. A failed internal-consistency check SKIPS the capture and
    // clears the marker rather than throwing, so export_warm_start() refuses
    // with the refusal it already has for "there is nothing to export" and no
    // PREVIOUS solve's payload is left standing. None of the conditions is
    // reachable today; see the definition.
    void capture_completed_warm_start(const SqpSolution &out, const AggregateEvalSeam &seam,
                                      const NlpModelAggregate &bridge);

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
    SqpSolution solve_impl(AggregateEvalSeam &seam, NlpModelAggregate &bridge, const Vec &x0,
                           const WarmStart &warm, Index minor_budget);

    // See this header's SUBPROBLEM FAILURE ROUTING note. Reached only after the
    // one-shot retry has already been spent -- and never with kInfeasible (the
    // elastic tier consumes that status upstream), which is why the
    // kInfeasible arm below is unreachable in this driver and kept only to
    // keep the mapping total.
    static SqpStatus map_status(QpStatus qp_status);

    // THE TWO HALVES OF THE SHRINK RULE, kept as one pair so the floor test
    // and the shrink itself can never disagree about what "the next radius"
    // is. See RADIUS MANAGEMENT for both.
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

    // The SSN engine, constructed on first use. See the member's own note for
    // why it is deferred; `opts_.qp` is the SAME QpOptions the walk was
    // constructed with, so the two kernels are regularized identically by
    // construction (ssn_engine.h's SsnOptions note makes that a requirement).
    SsnEngine &ssn_engine();

    // The SsnOptions ONE subproblem is solved under.
    //
    // `fb_tol` TRACKS THE TIGHTER OF THE DRIVER'S OWN kkt_tol AND feas_tol,
    // through ssn_engine.h's own derivation function rather than by
    // assignment: that header's section 5 establishes that an FB residual at
    // `kkt_tol` buys stationarity and equality feasibility at exactly kkt_tol
    // and per-row complementarity within a bounded factor of it (`ssn_fb_tol_for`
    // above has the derivation). This is what makes a tightened kkt_tol
    // REACHABLE from a kSsn-solved subproblem exactly as from a walk-solved
    // one.
    //
    // EVERY OTHER FIELD IS ssn_engine.h's OWN DEFAULT, deliberately.
    // `safeguards` stays kFull (kBare is a positive control, not a product
    // surface); `soft_budget`/`hard_budget` stay at 12/25, sized against an
    // escaping subproblem's own worst case; `uncertain_tol` stays at the swept
    // default. NO new tuning knob here.
    //
    // `prox_sigma_init` is the ONE field this driver sets, and only on the
    // first subproblem of a solve that ingested a proximal carry. See
    // warm_start.h's `prox_sigma` note for the whole rule.
    SsnOptions ssn_options(double prox_sigma_init) const;

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
    static WarmStart make_warm_start(AggregateEvalSeam &seam, const QpSolution *activity,
                                     const QpProblem &qp, bool qp_built, const NlpEval *probe_ev,
                                     const Vec *probe_x, double delta, double dual_mu_eff,
                                     double primal_delta_eff, const GlobalizationStrategy *strategy,
                                     std::shared_ptr<const HotState> hot);

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
    SqpSolution finish(AggregateEvalSeam &seam, SqpSolution out, SqpStatus status, const Vec &x,
                       const Vec &lambda_e, const Vec &lambda_i, const SqpKkt &kkt, double f,
                       WarmStart warm);

    /// Computes THIS solve's scaling factors and installs them on @p seam, or
    /// leaves the seam unscaled when `opts_.enable_scaling` is false.
    ///
    /// Called from solve_impl AFTER its argument validation and BEFORE the
    /// major loop, so a mis-sized or non-finite x0 is still refused with the
    /// message it has always been refused with rather than through an
    /// evaluation this layer added. Costs ONE extra model evaluation, and only
    /// when the toggle is on.
    ///
    /// @param seam the solve's evaluation seam, unscaled on entry.
    /// @param x0   the start point the factors are read at.
    /// @return the count of extra derivative evaluations spent (0 or 1), for
    ///         the caller to charge to its own counters.
    Index install_solve_scaling(AggregateEvalSeam &seam, const Vec &x0) const;

    SqpOptions opts_;
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
    // ones.
    double ssn_prox_center_sigma_out_ = 0.0;
    Vec ssn_prox_center_x_out_, ssn_prox_center_lambda_out_;
    // False on the driver the RESTORATION PHASE constructs for itself, which
    // bounds the recursion at one level. See the private constructor.
    bool allow_restoration_ = true;

    // See attach_ledger's doc comment above for the whole contract;
    // nullptr (ledger_) is "off", exactly like QpEngine's own ledger_.
    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;

    // --- Warm-start currency state ---
    // Both per-instance: the restoration phase's nested driver is a different
    // SqpDriver and shares neither.
    //
    // The value captured at the end of the last COMPLETED solve, valid only
    // while solve_completed_ is true. Built at that point rather than at
    // export so the stamp and the blocks describe the same solve.
    WarmStartData completed_warm_;
    // True once a public solve() has RETURNED on this instance. A call that
    // threw never reaches the capture and so does not arm this; convergence
    // is NOT required (the caller reads the verdict from SqpSolution::status).
    bool solve_completed_ = false;
    // The staged value, valid only while warm_staged_ is true. Consumed --
    // moved out and warm_staged_ cleared -- at the very start of the NEXT
    // public solve(), before any check on it can throw.
    WarmStartData staged_warm_;
    // True while a staged value is waiting to be applied.
    bool warm_staged_ = false;
};

// Human-readable names for the iteration printer below. Used only there --
// nothing in the driver itself branches on a printable string; defined in
// src/drivers/sqp_print.cpp alongside format_iteration_table.
/// @brief Human-readable name of a step verdict, for the iteration printer.
/// @param v The verdict.
/// @return A static string; nothing in the driver branches on it.
const char *to_string(StepVerdict v);

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
/// @brief Renders `sol.history` as an fmt-based iteration table, one row per
///        SqpIterate.
/// @param sol A finished solve.
/// @return The table, ready to print.
std::string format_iteration_table(const SqpSolution &sol);

} // namespace hven::solvers
