// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// =============================================================================
// ssn_engine.h -- THE SEMISMOOTH-NEWTON QP KERNEL (Phase 7, Tasks 3 and 4)
// =============================================================================
//
// A Newton method on a Fischer-Burmeister (FB) reformulation of the QP's own
// KKT conditions, solving the SAME subproblem qp_engine.h's primal active-set
// WALK solves (qp_problem.h's QpProblem, same sign convention, same
// regularization knobs), by a different mechanism: every step changes the
// WHOLE implied active set at once, where the walk changes one working-set
// member per minor iteration.
//
// WHY IT EXISTS. Phase 5/6 established that the walk's cost at scale is in the
// NUMBER of minor iterations, not in the cost of one
// (docs/notes/2026-07-30-scale-study-cold.md Sec. 4.2 -- "the pathology is
// entirely in HOW MANY, not in how expensive each one is"; the identification
// cost law is docs/notes/2026-08-03-identification-stall-study.md Sec. 3).
// A method whose iteration count does not grow with |W*| is therefore the only
// kind of change that can move that figure, and the semismooth-Newton /
// primal-dual active-set family is the standard one.
//
// -----------------------------------------------------------------------------
// SCOPE -- READ THIS BEFORE JUDGING A DIVERGENCE
// -----------------------------------------------------------------------------
//
// **THIS FILE NOW HAS TWO MODES, AND THE DIFFERENCE IS THE WHOLE OF TASK 4.**
//
//   SsnSafeguards::kBare -- Task 3's LOCAL METHOD, exactly. Full, undamped
//     Newton steps; no line search, no merit function, no proximal term, no
//     uncertain set, no dual projection, no inertia gate. A semismooth Newton
//     method with those omissions is LOCALLY superlinearly convergent -- UNDER
//     BD-REGULARITY AT THE SOLUTION (every element of the B-subdifferential
//     there nonsingular), which is a real hypothesis on this reformulation
//     rather than a formality: a WEAKLY ACTIVE row (lambda* = 0 AND c* = 0,
//     this file's own weakly_active_qp) is exactly what can violate it -- and
//     GLOBALLY nothing at all -- it can cycle, it can diverge, and on this
//     project's own analytic fixtures it does BOTH (it orbits on
//     tests/test_ssn_engine.cpp's two cycling instances and it certifies a
//     SADDLE POINT as kOptimal on the indefinite one). It is kept as a POSITIVE
//     CONTROL, not as a product surface, and it is the reason every safeguard
//     below can be scored as a comparison rather than asserted.
//
//   SsnSafeguards::kFull -- the production iteration, and the default. Section
//     7 below is its specification, and section 7b is the contract that makes
//     the saddle claim above a claim about kBare ALONE: under kFull no
//     kOptimal is issued anywhere -- cold start, warm hand-off, or a seed
//     placed exactly on the saddle -- without an inertia verdict read AT THE
//     CERTIFIED POINT. The first implementation of this file ran the
//     convergence test before every factorization and returned kOptimal at the
//     saddle when SEEDED there, which is the defect review fix round 1 closed.
//
// **DRIVER WIRING LANDED IN TASK 5**: sqp_driver.h dispatches its MAIN
// subproblem call on SqpOptions::qp_mode, constructing an SsnEngine (kFull)
// under QpMode::kSsn -- ingest, escape handling and tier-3 refinement are all
// wired (docs/notes/2026-08-08-phase-7-results.md Sec. 7.2). The SOC re-solve,
// elastic ladder rungs and restoration sub-solve stay on the WALK
// unconditionally regardless of qp_mode (a deliberate scope line, not a gap --
// each is a rescue path hot-started off the immediately preceding walk solve).
// With `qp_mode = kWalk` every existing test, pin and battery remains
// byte-identical (Sec. 10.4); the corpus battery (Task 6) and the Task 6b
// repair both ran the shipped kSsn configuration through this wiring.
//
// The only convergence claims either mode makes are on the ANALYTIC FIXTURES in
// tests/test_ssn_engine.cpp, all of them at most six variables. Task 6 scores
// the safeguarded engine against the pre-registered gates; nothing in this file
// should be quoted against them. The safeguard set's own evidence -- trigger,
// fixture and cost per safeguard, every constant's derivation, the
// counter-example search record and the honest negatives -- is
// docs/notes/2026-08-07-ssn-safeguards.md.
//
// -----------------------------------------------------------------------------
// 1. THE RESIDUAL
// -----------------------------------------------------------------------------
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
// **BOUNDS ARE ROWS.** Each finite bound becomes a one-sided inequality row
// with its own NON-NEGATIVE multiplier, exactly like a row of Ai:
//
//     lower:  l_j - x_j <= 0,   gradient -e_j,   multiplier lambda^lo_j >= 0
//     upper:  x_j - u_j <= 0,   gradient +e_j,   multiplier lambda^up_j >= 0
//
// and the signed z the rest of the project speaks is recovered as
// z_j = lambda^lo_j - lambda^up_j. (Substituting the two rows' gradient
// contributions into stationarity gives ... + lambda^up_j - lambda^lo_j = 0,
// i.e. "- z_j" with that definition -- the two conventions agree exactly.)
// A bound at or beyond detail::kSsnInfBound in magnitude is ABSENT and
// contributes no row at all, which is what keeps the +/-1e20 sentinel from
// entering any arithmetic.
//
// Writing s for the SLACK of an inequality-shaped row (s = bi_k - (Ai x)_k for
// a row of Ai, s = x_j - l_j / u_j - x_j for a bound row) the KKT conditions
// are, for every such row,
//
//     s >= 0,  lambda >= 0,  s * lambda = 0
//
// and the Fischer-Burmeister function
//
//     phi(a, b) = a + b - sqrt(a^2 + b^2)
//
// encodes exactly that: phi(a, b) = 0  <=>  a >= 0, b >= 0, a*b = 0. So the
// whole KKT system becomes ONE nonlinear equation F(w) = 0 in
// w = (x, lambda_e, lambda_i, lambda_bound):
//
//     F_x  = H x + g + Ae^T lambda_e + Ai^T lambda_i + B^T lambda_b   (n rows)
//     F_e  = Ae x - be                                               (me rows)
//     F_i  = phi(s_k, lambda_i_k)          per row of Ai             (mi rows)
//     F_b  = phi(s_r, lambda_b_r)          per finite bound          (mb rows)
//
// where B is the bound-row matrix (one +/-1 per row). Equalities enter
// LINEARLY -- no FB pair, nothing to select -- so on a QP with no inequalities
// and no finite bounds F is AFFINE and one Newton step is the exact solve of
// the regularized system (tests/test_ssn_engine.cpp's
// EqualityOnlyIsOneKktSolve).
//
// **"ONE STEP" IS A STATEMENT ABOUT F, NOT A PROMISE OF ONE FACTORIZATION**,
// and the gap is delta/mu (review fix round 1, M2). The step solves
// K = [H + delta I, Ae^T; Ae, -mu I], not the unregularized KKT matrix, so it
// lands O(delta*||x*|| + mu*||lambda*||) away from the true solution -- and
// that residual SCALES WITH THE ITERATE. Measured on the fixture's own QP: the
// seeds x = 0 and x = 5 finish in 1 step, while x = lambda_e = -100 takes 2,
// because 1e-8 * 100 is above the default fb_tol. An equality-heavy subproblem
// warm-started far from its solution is therefore NOT a one-factorization
// solve, which is a thing a Task-5 caller sizing a budget needs to know.
//
// -----------------------------------------------------------------------------
// 2. THE GENERALIZED JACOBIAN, AND WHY THE ASSEMBLED MATRIX IS SYMMETRIC
// -----------------------------------------------------------------------------
//
// phi is not differentiable at the origin but IS semismooth everywhere, with
//
//     rho    = sqrt(s^2 + lambda^2)
//     alpha  = d phi / d s      = 1 - s / rho        (in [0, 2])
//     beta   = d phi / d lambda = 1 - lambda / rho   (in [0, 2])
//
// away from s = lambda = 0, where any (1 - xi, 1 - eta) with xi^2 + eta^2 <= 1
// is an element of the C-subdifferential and this file selects
// xi = eta = 1/sqrt(2), i.e. alpha = beta = detail::kSsnDegenerateFbDeriv.
//
// **alpha AND beta CAN NEVER BOTH BE SMALL**: alpha + beta = 2 - (s+lambda)/rho
// >= 2 - sqrt(2) ~ 0.586 for every (s, lambda). alpha = 0 means exactly
// "lambda = 0 and s > 0" (a strictly INACTIVE row) and beta = 0 means exactly
// "s = 0 and lambda > 0" (a strictly ACTIVE one). So the partition
//
//     row k is ACTIVE  <=>  alpha_k > beta_k  <=>  lambda_k > s_k
//
// is read off the Jacobian itself rather than maintained as separate state --
// it IS the primal-dual active set of Hintermueller-Ito-Kunisch's PDAS, whose
// equivalence to semismooth Newton is the design's whole point.
//
// Since s_k = b_k - a_k^T x, the FB row of the Newton system is
//
//     - alpha_k (a_k^T dx) + beta_k d lambda_k = - phi_k
//
// which is NOT symmetric against the stationarity block's a_k column.
// Multiplying the row through by (-1 / alpha_k) gives
//
//     a_k^T dx - (beta_k / alpha_k) d lambda_k = phi_k / alpha_k
//
// and now the off-diagonal is EXACTLY the constraint gradient a_k, so the
// assembled matrix is the project's ordinary symmetric KKT shape with a
// PER-ROW negative diagonal in place of the walk's uniform -mu:
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
// stored as the UPPER TRIANGLE (SpMatRM) and factored by the SAME sparse factor
// (MKL Pardiso / Apple Accelerate) the walk uses. Nothing about the linear
// algebra is new; only the diagonal's contents are.
//
// **D IS LARGE WHERE THE ROW IS INACTIVE AND ~mu WHERE IT IS ACTIVE**, which
// is the interior-point-like conditioning this shape always has, and is why
// the division is by alpha and not by beta: alpha -> 0 sends the diagonal to
// +infinity (a decoupled, diagonally dominant row -- numerically benign),
// while beta -> 0 sends it to mu (the ordinary active-constraint KKT row the
// walk already factors every minor).
//
// THE alpha FLOOR. alpha is EXACTLY zero whenever lambda_k = 0 and s_k > 0,
// which is the common case at a cold-ish seed, so the division needs a floor:
// alpha_f = max(alpha, detail::kSsnAlphaFloor). The floor is close to free,
// and the reason is that it appears in BOTH the diagonal and the right-hand
// side, so it CANCELS in the recovered multiplier step:
//
//     d lambda_k  ~  - (r_k / alpha_f) / (beta_k / alpha_f + mu)
//                 =  - r_k / (beta_k + mu * alpha_f)   ->  - r_k / beta_k
//
// for any alpha_f small enough that mu * alpha_f << beta_k. What the floor
// DOES perturb is the row's coupling to dx, by O(kSsnAlphaFloor) -- and the
// rows it perturbs are precisely the ones that should not couple to dx at all.
// So a floored row is a Newton step for a generalized Jacobian element
// perturbed by O(1e-12) in the one entry that is numerically zero; it is not a
// heuristic branch and it introduces no active/inactive decision.
//
// -----------------------------------------------------------------------------
// 3. THE FIXED SPARSITY PATTERN -- THE LOAD-BEARING PROPERTY
// -----------------------------------------------------------------------------
//
// K's PATTERN depends on (H, Ae, Ai)'s patterns and on WHICH BOUNDS ARE
// FINITE, and on nothing else. It does NOT depend on the active set, on the
// iterate, or on which branch any FB row is in -- a branch change moves the
// VALUE of one diagonal entry and nothing else. Concretely:
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
// SsnResult::symbolic_analyses reports this directly (counted at the call
// site, before factorize(), exactly as qp_engine.h's rebuild_k0() counts
// QpCounters::symbolic_analyses and for the same reason: factorize() decides
// internally and does not report back). tests/test_ssn_engine.cpp asserts
// both halves -- one analysis for the first solve of a structure, zero for
// every iteration after it and zero for the next QP of the same shape.
//
// -----------------------------------------------------------------------------
// 4. THE ACTIVITY HINT = ONE PDAS STEP
// -----------------------------------------------------------------------------
//
// SsnStart::activity_hint, when supplied, replaces the FB branch selection ON
// THE FIRST NEWTON STEP ONLY:
//
//     hinted ACTIVE    (alpha, beta) = (1, 0), row residual s_k
//                      => a_k^T (x + dx) = b_k   -- drive the slack to zero
//     hinted INACTIVE  (alpha, beta) = (alpha_floor, 1), row residual lambda_k
//                      => d lambda_k = -lambda_k -- drive the multiplier to zero
//
// which is EXACTLY one primal-dual active-set (equality-constrained KKT) solve
// on the hinted set, up to delta/mu and the O(1e-12) floor coupling. Every
// step after the first uses the FB branch. This is what makes a correctly
// hinted, correctly seeded QP converge in ONE iteration, and it is the seam
// Task 5 will hand the driver's warm-start activity through.
//
// **"DRIVE THE SLACK TO ZERO" IS EXACT ONLY AT dual_mu = 0** (review round 2).
// beta = 0 makes the hinted-active row's diagonal -(beta/alpha_f + mu) = -mu
// rather than 0, so the row solves a_k^T dx - mu*d lambda_k = s_k and the
// landing slack is
//
//     s_k^+ = -mu * d lambda_k,
//
// i.e. EXACTLY zero when mu = 0 and O(mu * |d lambda|) under the shipped
// default. Measured on tests/test_ssn_engine.cpp's own two-row fixture, whose
// hinted-active row takes d lambda = -2.5 on its first step: s^+ = 0 exactly at
// mu = 0, 2.500000e-08 at the default mu = 1e-8, and 2.499999e-06 at mu = 1e-6
// -- tracking -mu*d lambda to every digit printed. The distinction matters
// wherever an exactly-zero slack is used as a premise rather than as a
// description; section 2's "beta = 0 sends the diagonal to mu" is the same
// fact stated from the other side.
//
// -----------------------------------------------------------------------------
// 5. THE TOLERANCE, AND WHAT IT CERTIFIES
// -----------------------------------------------------------------------------
//
// Convergence is ||F(w)||_inf <= SsnOptions::fb_tol, measured on the UNSCALED
// residual of section 1 (never on the row-scaled linear system's right-hand
// side). For s, lambda >= 0 one has the two-sided bound
//
//     (2 - sqrt(2)) * min(s, lambda)  <=  phi(s, lambda)  <=  min(s, lambda)
//
// (both are tight: equality on the left at s = lambda, on the right as
// min/max -> 0), so ||F||_inf <= fb_tol gives
//
//     ||grad L||_inf <= fb_tol,  ||Ae x - be||_inf <= fb_tol,
//     min(s_k, lambda_k) <= fb_tol / (2 - sqrt(2)) = 1.7071 * fb_tol
//
// per row -- **AND THAT LAST LINE CARRIES ITS HYPOTHESIS WITH IT** (review fix
// round 1, M7): the two-sided bound above is derived for s, lambda >= 0, which
// is exactly what an inexact exit point is NOT guaranteed to satisfy. phi = 0
// forces s >= 0 and lambda >= 0 exactly, but |phi| <= fb_tol permits either
// component to be negative by O(fb_tol). So the honest joint reading is: at an
// exit point every row satisfies min(s_k, lambda_k) <= 1.7071 * fb_tol **among
// the rows whose pair is non-negative**, and any row that is not has both
// components within O(fb_tol) of the non-negative orthant. A caller that needs
// a strictly feasible point must project, exactly as it must after any inexact
// solve; the two readings differ by O(fb_tol) and never by more, but they are
// different claims and this file states both rather than the tidier one.
//
// THE DERIVATION OF THE DEFAULT is therefore: fb_tol = kkt_tol, which buys
// stationarity and equality feasibility at exactly kkt_tol and complementarity
// within detail::kSsnComplementarityFactor (~1.71) of it -- a constant factor
// rather than a new tolerance to tune, which is why no separate knob is
// introduced. ssn_fb_tol_from_kkt_tol() is that derivation in code;
// SsnOptions::fb_tol's own default is the same number for SqpOptions::kkt_tol's
// default.
//
// -----------------------------------------------------------------------------
// 6. THE PER-SOLVE SEAM: TRUST REGION AND REGULARIZERS
// -----------------------------------------------------------------------------
//
// solve() has an overload taking qp_types.h's SolveOverrides -- THE WALK'S OWN
// struct, not a parallel one, because the funnel driver already builds one per
// subproblem and per SOC re-solve and a second type would be a translation
// layer for Task 5 to maintain. Sentinels and resolution are qp_types.h's rules
// unchanged.
//
// THE TRUST REGION IS A BOX, AND THAT IS THE WHOLE DESIGN. lo_eff =
// max(lower, x0 - Delta), up_eff = min(upper, x0 + Delta), about this solve's
// own start point, resolved once. Because it is a box it changes only the
// bound rows' VALUES -- never which rows exist, once the radius is finite at
// all -- so:
//
//   * K's sparsity pattern is INVARIANT ACROSS RADIUS CHANGES, and a
//     shrink-retry loop re-solves at zero pattern cost and zero symbolic
//     analyses. The structure key must therefore never mix the radius or the
//     from_tr flags (detail::SsnBoundRow says so at the field, and
//     tests/test_ssn_engine.cpp's TrustRegionRadiusIsNotPartOfTheStructureKey
//     pins it);
//   * Delta = +inf reproduces the no-trust-region solve BIT FOR BIT, since
//     max(lower, -inf) is lower exactly -- the same guarantee QpOptions::
//     tr_radius's default carries for the walk.
//
// TR PINS ARE NOT BOUNDS, AND THE EXPORT KEEPS THEM APART. Every bound row
// records whether its bound is the trust region's (strictly tighter than the
// real one) and on the way out:
//
//   tr_active[j]     true iff a TR-tight row is implied active at j
//   bound_state[j]   REAL bounds only -- a TR-pinned variable reads kFree
//   z(j)             REAL bounds only -- a TR-pinned row's dual is dropped
//   start.z          priced against REAL bounds; TR rows seed at zero
//
// This is qp_problem.h's QpSolution::tr_active contract verbatim, and it is
// not a stylistic match: warm_start.h records a real incident of a multiplier
// priced against a bound that was not there poisoning a downstream estimate,
// and this file's own seeded-z check throws over the same shape. A driver
// detects a binding radius by reading tr_active, never z or bound_state.
//
// **AND THE RADIUS IS A SOFT CONSTRAINT: ON AN ESCAPED EXIT THE RETURNED x MAY
// LIE FAR OUTSIDE IT** (review fix round 1, I3). The TR is bound ROWS, an FB
// row holds only at a root, and the line search damps the step rather than
// projecting it -- so a solve that stops before converging can return a point
// 160x the radius away (measured; SsnResult::tr_violation carries the full
// contract and the numbers). A certifying exit respects the box to
// O(1.71 * fb_tol) and is the ONLY exit whose x may be used as a step.
//
// THE REGULARIZERS RESOLVE TOO, BUT THE DRIVER'S ADAPTIVE-mu SCHEDULE BUYS
// NOTHING HERE, and Task 5 should switch it off deliberately rather than
// discover that. delta and mu perturb ONLY the Jacobian -- for_each_entry's
// emissions and the FB diagonal -- and never the residual, which residual()
// computes unregularized. The iteration is therefore modified Newton on the
// EXACT F: its fixed points are exact, unregularized KKT points, and (delta,
// mu) cost ITERATIONS rather than ACCURACY. That is the opposite trade from
// the walk, whose returned solution carries the mu|lambda| footprint that
// motivated the schedule in the first place, and it means SSN should be
// expected to reach tighter KKT residuals than the walk at identical settings.
//
// -----------------------------------------------------------------------------
// 7. THE SAFEGUARDED ITERATION (Task 4)
// -----------------------------------------------------------------------------
//
// ONE ATTEMPT, in order. Everything marked [G] is skipped entirely under
// SsnSafeguards::kBare, which is what makes bare mode Task 3's kernel rather
// than a re-implementation of it.
//
//   1. Evaluate F at the iterate. TWO norms come out of the one walk: ||F||inf,
//      which is the CERTIFICATE of section 5, and 1/2||F||_2^2, which is the
//      LINE SEARCH's merit. They are different on purpose -- fb_tol certifies
//      per-row quantities and must stay the inf-norm, while a merit function has
//      to be smooth where phi is and max_k |F_k| is not.
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
//      (D > 0 on every FB diagonal; final branch review WAVE #7, T4 N5/M8) --
//      so the gate is provably inert on any convex subproblem under that
//      hypothesis. kWrong escalates the proximal ladder and retries the SAME
//      iterate; kSuspect does NOT act, matching qp_engine.h.
//   9. [G] LINE SEARCH: Armijo on the merit, backtracking by
//      detail::kSsnBacktrackFactor to the floor detail::kSsnMinStep, over trial
//      points that carry the DUAL PROJECTION. **Iteration 0 is exempt when a
//      HINT governed it** -- see the exemption's own block at the call site;
//      it is the single most load-bearing line in the safeguard set, because a
//      wrongly hinted first step legitimately raises ||F|| and a monotone rule
//      would reject it along with the one-iteration correct-hint payoff.
//  10. Accepted -> apply. Not accepted -> the escape ladder: the infeasibility
//      DIAGNOSIS outranks the repair, then a proximal rung, then
//      kNoContraction.
//
// **FOUR OF THESE STEPS HAVE AN OPT-IN ALTERNATIVE AS OF PHASE-7 TASK 6b PHASE
// B**, and every one of them ships at the value that reproduces the list above
// BIT FOR BIT. They exist so that the research pass's recommendations
// (docs/notes/research/fable_fb_ssn_globalization_second_pass_claude.md) are
// ratified on evidence rather than on argument, and none of them is a default:
//
//   step 2/7b -- SsnOptions::defer_certification (R5). The verification attempt
//                is BUILT and not factorized; the caller closes it, and a
//                caller that is about to re-solve the identified face exactly
//                already owns the evidence (Gould's lemma).
//   step 5/10 -- SsnOptions::sigma_rule (R1). sigma sized from the RESIDUAL
//                instead of climbed, with the ladder retained underneath as a
//                monotone floor.
//   step 9    -- SsnOptions::hint_rule + watchdog_q (R2). The iteration-0
//                exemption replaced by a q-step watchdog with return-to-best.
//   step 3    -- SsnOptions::infeasibility_rule (R4). The symptom conjuncts
//                demoted to an ARMING condition, with a matvec-only Farkas
//                certificate as the firing condition.
//
// **WHAT THE PROXIMAL TERM IS, PRECISELY.** sigma is added to the SAME two
// slots primal_delta and dual_mu already occupy, and it is anchored at the
// CURRENT iterate. So it perturbs only the Jacobian, section 6's property
// survives verbatim -- the iteration stays modified Newton on the EXACT F and
// its fixed points stay exact, unregularized KKT points -- and one residual per
// attempt serves both the certificate and the merit. It is NOT FBstab's outer
// proximal loop with a lagging centre; SsnStart::prox_center_x carries that
// decision and its cost.
//
// **ATTEMPTS ARE NOT STEPS.** An attempt can pay its factorization and then
// take no step (a rejected line search, a wrong inertia, or the second-order
// verification of 7b), so Task 3's "factorizations == iters" is now
// iters <= factorizations, with equality UNREACHABLE under kFull (7b's
// verification adds one) and holding under kBare exactly when nothing was
// rejected -- which is every benign fixture. SsnResult::factorizations
// documents it at the field.
//
// -----------------------------------------------------------------------------
// 7b. THE CERTIFYING EXIT, AND WHY IT COSTS A FACTORIZATION (fix round 1, C1)
// -----------------------------------------------------------------------------
//
// **NO kOptimal IS ISSUED UNDER kFull WITHOUT AN INERTIA VERDICT READ AT THE
// POINT BEING CERTIFIED.** The first implementation ran the convergence test
// BEFORE every factorization, so a solve seeded at (or within fb_tol of) a
// stationary point returned kOptimal having gated nothing: seeded at
// tests/test_ssn_engine.cpp's own saddle (0.5, 0.125) of indefinite_qp it
// reported kOptimal in ZERO factorizations, which is the same wrong-answer
// class as the Phase-5 Task-7 zero-major defect and reachable by exactly the
// hand-off Task 5 builds (re-solve from the previous solve's answer).
//
// So the convergence test now opens a VERIFICATION ATTEMPT instead of exiting:
// classify the rows at the converged point, refresh K's diagonals, factorize,
// read the inertia -- and only then certify. It takes no step, forms no
// right-hand side and runs no triangular solve; it pays ONE numeric
// factorization on the cached pattern, and it moves no counter but
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
// **THE VERDICT IS READ AT THE CALLER'S OWN REGULARIZATION.** If the proximal
// ladder is armed when the solve converges, sigma is dropped to 0 for the
// verification and restored for reporting: In(K) = (n, m) iff
// H + (delta+sigma) I + A^T D^{-1} A is positive definite, so at a large sigma
// the test is about H + sigma I and says nothing at all about the QP the
// caller handed in. An earlier attempt's verdict is not reused for the same
// class of reason -- D is a function of the ITERATE, so a verdict taken at
// w_k is a statement about w_k, and the whole failure this closes is a
// statement about the wrong point.
//
// **THE COST IS ONE FACTORIZATION PER CERTIFIED SOLVE**, and it is a real
// product cost, not a rounding error: a one-iteration warm hand-off goes from
// one factorization to two, and the zero-step hand-off from zero to one. It is
// paid because the alternative is a wrong answer, and because the gate is
// PROVABLY INERT (section 6) rather than merely usually right, PROVIDED
// delta+sigma > 0 -- SolveOverrides::primal_delta = 0.0 with dual_mu = 0.0 is
// legal and non-sentinel, and under it an inactive FB row's diagonal is
// exactly 0 (D not > 0), so the theorem's hypothesis fails (final branch
// review WAVE #7, T4 N5/M8; no shipped convex fixture has been found where
// this changes the verdict -- documentation debt, not a demonstrated defect).
// With the hypothesis held, on a convex subproblem the verdict is kOk by the
// identity, so the verification can never turn a good answer into an escape.
// Task 5's stable-face refinement wants a
// factorization at the returned point anyway; Task 6 owns the ruling on
// whether a caller that convexifies its Hessian may declare so and skip it.
//
// **ACCELERATE (fix round 1, C1 corollary).** The gate reads
// the factorization evidence's perturbed-pivot count, which means PERTURBED pivots on MKL and
// ZERO pivots on Accelerate, and the verdict tests it FIRST. An Accelerate
// factorization of an indefinite K that reports a zero pivot therefore lands on
// kSuspect, which does not act -- so the verification CERTIFIES THE SADDLE
// there, in the cold case as well as the seeded one. "Degrades toward not
// firing" is the safe direction for a REPAIR trigger and the UNSAFE one for a
// certificate, and this file now says so in both places rather than only the
// first. docs/notes/2026-07-28-accelerate-audit-checklist.md section (h) owns
// the hardware item; NO APPLE VALUE IS QUOTED ANYWHERE HERE.
//
// -----------------------------------------------------------------------------
// 8. REFERENCES (public mathematics, clean-room)
// -----------------------------------------------------------------------------
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
// Program. 75, 1996. **NO PAPER'S NUMERICAL EXAMPLE IS REPRODUCED HERE** -- the
// cycling fixtures were found by search over the failing family, because none
// of the published instances is an instance for THIS (FB, not min-map)
// iteration; docs/notes/2026-08-07-ssn-safeguards.md section 3 is the record.
// Full
// citations: docs/notes/research/fable_research_qp_sqp_scaling_survey_claude.md
// Sec. 2.1. Published mathematics only -- no third-party source was read.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_types.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// Bounds at or beyond this magnitude are ABSENT and contribute no FB row.
// Identical value and meaning to qp_engine.h's kEngineInfBound and to the
// +/-1e20 sentinel nlp_model.h/qp_problem.h document; restated here rather
// than reached for across headers so ssn_engine.h depends on the QP data
// types only, not on the walk's internals.
inline constexpr double kSsnInfBound = 1e20;

// Floor applied to alpha = d phi / d s before it is divided by. See the
// header's THE alpha FLOOR paragraph: it cancels out of the recovered
// multiplier step and perturbs only the dx-coupling of rows whose true
// coupling is numerically zero. 1e-12 is chosen so that mu * alpha_f
// (1e-8 * 1e-12 = 1e-20) is negligible against the beta ~ 1 that always
// accompanies a floored alpha, while the resulting diagonal (beta / alpha_f,
// at most 2e12) stays far from any overflow or conditioning cliff.
inline constexpr double kSsnAlphaFloor = 1e-12;

// Pure division guard for rho = sqrt(s^2 + lambda^2): below this the pair is
// the FB function's one non-differentiable point and the C-subdifferential
// selection below is used instead. Deliberately at the far bottom of the
// double range -- this is NOT a "treat as degenerate" tolerance (which would
// be a branch, and would need a justification of its own); it exists only so
// that 0/0 cannot be formed.
inline constexpr double kSsnRhoFloor = 1e-300;

// The C-subdifferential element selected at s = lambda = 0: xi = eta =
// 1/sqrt(2), giving alpha = beta = 1 - 1/sqrt(2). The symmetric choice, and
// the one that keeps alpha strictly above the floor so the degenerate row
// still couples to dx.
inline const double kSsnDegenerateFbDeriv = 1.0 - 1.0 / std::sqrt(2.0);

// 1 / (2 - sqrt(2)) ~ 1.7071: the factor by which a satisfied FB residual
// bounds the per-row complementarity min(s, lambda). See section 5.
inline const double kSsnComplementarityFactor = 1.0 / (2.0 - std::sqrt(2.0));

// phi(a, b) = a + b - sqrt(a^2 + b^2), evaluated in the CANCELLATION-FREE
// form 2ab / (a + b + rho) whenever a + b > 0 (Fable kernel review, M-2).
// The two are algebraically identical -- multiply by (a+b+rho)/(a+b+rho)
// and use (a+b)^2 - rho^2 = 2ab -- but they are not numerically identical,
// and the naive one has an absolute error floor of ~ulp(rho)/2:
//
//   fl(sqrt(fl(s*s))) == s exactly in IEEE double whenever s^2 neither
//   overflows nor underflows, and fl(s + lambda) == s for any
//   |lambda| < ulp(s)/2. So on a FAR-SLACK row -- s = 1e6, lambda = 1e-14,
//   whose true phi is ~1e-14 -- the naive form evaluates to EXACTLY 0. It
//   under-reports rather than adding noise, so it cannot stall the
//   iteration (the review measured convergence to 7e-16 at slacks of
//   1.9e7 under fb_tol = 1e-10, at n = 50 and n = 2000), but it does put an
//   additive ulp(s_row)/2 slop on the exit CERTIFICATE, and Task 4's merit
//   1/2||F||^2 would inherit the same quantization.
//
// The stable form removes that floor for one extra multiply. It is used
// only when a + b > 0, which is exactly the same-sign regime where the
// cancellation lives; when a + b <= 0 the naive expression sums two
// non-positive terms and cannot cancel, and it also covers a = b = 0
// (where the stable form's denominator vanishes).
inline double ssn_fb(double a, double b) {
    const double rho = std::sqrt(a * a + b * b);
    const double sum = a + b;
    return sum > 0.0 ? 2.0 * a * b / (sum + rho) : sum - rho;
}

// =============================================================================
// TASK 4 -- THE SAFEGUARD CONSTANTS
// =============================================================================
//
// Every value below is an IMPLEMENTATION CHOICE, not a paper constant, and is
// stated as such with its own argument -- the same standard sqp_types.h's
// kWarmResidualGrowthMax / kWarmFullStepWindow are held to. The measurements
// each one cites live in docs/notes/2026-08-07-ssn-safeguards.md.

// ---- THE UNCERTAIN BAND (the three-set partition's only tolerance) --------
//
// THE MARGIN IS MEASURED ON THE FB DERIVATIVE PAIR ITSELF, which is what makes
// it dimensionless and scale-free and is the reason no second tolerance is
// introduced. Writing (s, lambda) = rho (cos theta, sin theta),
//
//     alpha - beta = (lambda - s) / rho = sqrt(2) * sin(theta - pi/4),
//
// so |alpha - beta| is 0 exactly on the kink ray s = lambda (where the row's
// classification is undecidable), and 1 at either PURE state (a strictly
// active row has (alpha, beta) = (1, 0); a strictly inactive one (0, 1)).
// The partition rule alpha > beta is the SIGN of this quantity; the uncertain
// set is the band around its zero.
//
// kSsnUncertainEnter = 0.1 -- a row enters the uncertain set when its pair is
// within 0.1 of the kink in this measure, i.e. within
// arcsin(0.1/sqrt(2)) = 4.05 degrees of the kink ray. Swept on the fixture set
// (the note's section 4): 0 reproduces the binary partition exactly, 0.05 and
// 0.1 leave every benign fixture's iteration count unmoved, and 0.5 and above
// start damping rows that are not in fact ambiguous and cost iterations. 0.1
// is the largest swept value that is free on the benign set.
//
// kSsnUncertainLeaveRatio = 3 -- the HYSTERESIS. A row LEAVES the uncertain set
// only once its margin reaches 3x the entering threshold, so a row whose margin
// hovers in [0.1, 0.3] keeps whatever class it already had and cannot chatter.
// This is the Phase-6 Task-3 lesson applied (docs/notes/
// 2026-08-03-identification-stall-study.md sec. 7.9 and continuation.h's
// suspend_growth_after_failure: "do not re-propose the thing that just
// failed"); the smallest ratio that is a band at all is > 1, and 3 is one
// natural step above it -- large enough that a row must move materially to be
// re-decided, small enough that a genuinely decided row is never trapped.
inline constexpr double kSsnUncertainEnter = 0.1;
inline constexpr double kSsnUncertainLeaveRatio = 3.0;

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
// derivative substituted). 1e-4 is the textbook Armijo constant and is chosen
// as such -- it is loose enough that a full Newton step in the local regime
// always passes it, which is exactly the property that keeps the safeguarded
// engine's trajectory equal to the bare one on every benign fixture.
//
// **THE MERIT IS THE 2-NORM SQUARE, THE CERTIFICATE IS THE INF-NORM.** They are
// different quantities on purpose: fb_tol certifies per-row quantities and must
// stay the inf-norm of section 5's derivation, while a merit function has to be
// smooth where phi is, and max_k |F_k| is not.
inline constexpr double kSsnArmijoSigma = 1e-4;

// Halving. The standard factor; a coarser one (0.1) throws away the
// intermediate step lengths that a nearly-accepted Newton step needs, and a
// finer one (0.8) pays more residual evaluations for the same reduction.
inline constexpr double kSsnBacktrackFactor = 0.5;

// The step FLOOR. Below this the backtracking schedule is declared exhausted
// and the escape ladder takes over (proximal escalation, then
// SsnEscape::kNoContraction). 1e-4 with a halving factor is at most 14 trial
// points -- 14 residual evaluations, each O(nnz), against the ONE factorization
// they are protecting, so the whole schedule costs a small fraction of the step
// it guards even in its worst case.
inline constexpr double kSsnMinStep = 1e-4;

// ---- THE LM-REGULARIZED LADDER (formerly headlined "PROXIMAL LADDER") ----
//
// NAMING: the phase's normative vocabulary (results note preamble; safeguards
// note rev B Sec. 5; safeguards note Sec. 12.9's closing paragraph) reserves
// *proximal* for FBstab's outer loop with a LAGGING centre -- the thing this
// ladder is NOT, per the paragraph immediately below. Sec. 12.9 records the
// better reading: sigma is Wachter-Biegler-style regularization escalation on
// a CURRENT-iterate anchor, not a proximal-point method. The `ssn_prox_*`
// identifiers (SsnOptions::prox_sigma_init, SsnCounters::ssn_prox_updates) are
// shipped surface and are NOT renamed here -- that is a Task-5-interface
// decision, not a documentation one (safeguards note Sec. 12.9).
//
// sigma is anchored at the CURRENT iterate, which makes it an additive
// increment to the two regularizers this file already applies: K's primal
// block gets H + (delta + sigma) I and every dual diagonal gets
// -(... + mu + sigma). Because the anchor is the current point, the residual
// is untouched -- so section 6's property survives intact: sigma costs
// ITERATIONS, never ACCURACY, and the iteration's fixed points remain exact,
// unregularized KKT points. (This is deliberately NOT FBstab's outer proximal
// loop with a LAGGING centre, which would make the exit certificate two-level;
// see SsnStart::prox_center_x for that decision and its cost.)
//
// THE LADDER IS PER-SOLVE AND MONOTONE -- sigma never decreases inside one
// solve, except for the second-order verification's documented drop to the
// caller's own regularization (section 7b), which is not a rung -- exactly like
// qp_engine.h's kSuspectDeltaFactor escalation, so the project has one
// escalation convention rather than two. Two decades per rung and a 1e6 ceiling
// give SEVEN rungs from 1e-6 (1e-6, 1e-4, 1e-2, 1, 1e2, 1e4, 1e6), which is
// enough to dominate any Hessian this file can be handed at a sane scaling and
// is bounded by the step budget in any case.
//
// **THE CEILING TEST NEEDS A RELATIVE SLACK, AND WITHOUT ONE THE LADDER HAD
// EIGHT RUNGS** (review fix round 1, I1). Six multiplications by 100 do not
// reproduce 1e6 in binary floating point: the sequence runs 1e-06,
// 9.999999999999999e-05, ..., 999999.9999999998 -- and 999999.9999999998 is
// STRICTLY BELOW the 1e6 cap, so a `sigma >= kSsnProxMax` guard granted one
// more rung that raised sigma by 2.3e-10 relative and cost a full numeric
// factorization plus a full backtracking schedule (measured: 13 backtracks) on
// every escape that reached the ceiling -- i.e. on exactly the solves Task 5
// must budget for. kSsnProxCapSlack closes the guard against the ladder's own
// drift: 1e-9 is seven orders above the accumulated relative error (~2e-16)
// and eleven orders below one rung (a factor 100), so it can neither miss the
// ceiling nor merge two genuine rungs.
inline constexpr double kSsnProxInit = 1e-6;
inline constexpr double kSsnProxGrowth = 100.0;
inline constexpr double kSsnProxMax = 1e6;
inline constexpr double kSsnProxCapSlack = 1e-9;
// The ladder's DOCUMENTED length, asserted by tests/test_ssn_engine.cpp rather
// than read by the iteration -- the rungs come from Init/Growth/Max above, and
// this constant exists so that the documented count and the delivered count
// cannot drift apart again silently (they did: review fix round 1, I1).
inline constexpr Index kSsnProxRungs = 7;

// ---- THE INFEASIBILITY TELEMETRY -----------------------------------------
//
// An infeasible convex QP has NO KKT point, so F has no root and the iteration
// cannot converge. What it does instead is measured and specific (the note's
// section 5): ||F||inf FLATTENS onto a positive floor -- the least-squares
// distance between the contradictory rows -- while the multipliers of those
// rows GROW without bound, because phi(s, lambda) -> s as lambda -> +infinity
// on a row with s < 0, so the only way the FB rows can shrink is by pushing
// lambda up. Both halves are required:
//
//   * growth alone is ordinary early behaviour (a cold start on a QP with large
//     multipliers grows the duals by orders of magnitude in two steps);
//   * a stall alone is a hard-but-feasible problem, or a budget that is simply
//     too small -- which is what kBudget is for.
//
// **ALL THREE OF THE STALL CONJUNCT'S DEGREES OF FREEDOM WERE WRONG IN THE
// FIRST IMPLEMENTATION, AND EACH ONE COULD LABEL A FEASIBLE QP INFEASIBLE**
// (review fix round 1, I2; Fable review F2). What ships now:
//
//   (i) THE WINDOW ADVANCES ON ACCEPTED STEPS, NEVER ON ATTEMPTS. A proximal
//       retry re-evaluates the IDENTICAL residual at the IDENTICAL iterate, so
//       counting attempts let one rough patch fill the window with five copies
//       of one point. (This is also the doc drift the Fable review's M5 named:
//       the constant said "steps", the counter counted attempts. Now they
//       agree, and the escape message says "accepted steps" too.)
//   (ii) THE IMPROVEMENT DEMAND IS OVER THE WHOLE WINDOW, NOT PER STEP. A
//       proximally damped iteration legitimately crawls: measured 0.99010 per
//       step on this file's own indefinite_qp once the ladder parks at
//       sigma = 100, which a per-step 0.99 demand reads as a stall on a
//       perfectly FEASIBLE subproblem -- the review reproduced it, and the
//       mutation record corroborated it (S18, dropping the growth conjunct,
//       is killed by that fixture reporting infeasibility). Over a window the
//       same crawl improves 4.85% and re-arms after two steps, while the
//       exactly-flat residual an infeasible QP produces still fills it.
//   (iii) THE GROWTH CONJUNCT IS DIFFERENT ON THE TWO ROUTES, because the two
//       routes see different evidence, and measuring both against the START
//       POINT (floored at 1) made the test VACUOUS for a cold start on any
//       feasible QP whose true multipliers exceed 1e4: they cross the threshold
//       on the way to their own values and never come back, leaving the other
//       conjunct to decide alone.
//         - THE STANDING ROUTE re-arms its reference with the window, so
//           "the duals grew 1e4x" means the divergence happened WHILE nothing
//           improved. Measured on slow_infeasible_qp, whose duals grow
//           geometrically (46 -> 159 -> 641 -> 2 539 -> 1.27e6): still
//           diagnosed, on the same route, two accepted steps later than the
//           start-point form reported it.
//         - THE EXHAUSTION ROUTE cannot use a windowed reference at all, and
//           the fixtures say why: on contradictory_qp the duals reach 4.0e7 on
//           the SAME accepted step that improves the residual 0.797 -> 0.400,
//           and on inconsistent_equality_qp they reach 5.0e7 on the FIRST step
//           from a cold start -- the divergence and the last progress are the
//           same event, so any "growth since the last progress" is 1. It keeps
//           the start-point reference and adds kSsnDualStepGrowth instead: the
//           multiplier norm must have multiplied by an ORDER OF MAGNITUDE
//           across the most recently accepted step. That is what separates
//           diverging duals from merely large ones -- a trajectory whose
//           multipliers are converging to values above 1e4 has settled by the
//           time its line search dies (per-step ratio -> 1), while the two
//           infeasible fixtures grow 154x and 5.0e7x on the step in question.
//
// And a window in which the PROXIMAL LADDER escalated cannot declare a stall at
// all -- it is discarded and a fresh one starts. Slow progress under a sigma
// that just changed is the safeguard's doing, not the problem's.
//
// kSsnStallWindow = 5 ACCEPTED STEPS over which ||F||inf must improve on the
// window's reference by kSsnStallImproveFactor. Five mirrors sqp_types.h's
// kWarmFullStepWindow and for the same reason: it is an order of magnitude
// above the local regime this method targets (a healthy solve reaches its
// answer in under ten steps), so no healthy trajectory can reach it.
// The improvement demand of 1% per WINDOW is deliberately feeble -- a Newton
// method that cannot manage 1% over five accepted steps is not converging --
// and it is a RELATIVE test so that the exactly-flat residual an infeasible QP
// produces cannot be mistaken for improvement by rounding noise.
//
// kSsnDualGrowthFactor = 1e4 against the multiplier norm at the reference point
// the route selects (floored at 1, so a zero-multiplier reference is measured
// absolutely). Four orders is far above any legitimate dual growth between two
// points that made no progress on each other and far below the 1e150 the
// iterate would need to reach an overflow escape.
//
// kSsnDualStepGrowth = 10, the EXHAUSTION route's second conjunct, on the
// growth across ONE accepted step. An order of magnitude in a single step is
// not a trajectory settling onto its multipliers; it is a trajectory whose
// multipliers have no limit to settle onto. Measured on the two exhaustion-route
// fixtures: 154x (contradictory_qp, 2.6e5 -> 4.0e7) and 5.0e7x
// (inconsistent_equality_qp, a cold 0 -> 5.0e7 in one step).
inline constexpr Index kSsnStallWindow = 5;
inline constexpr double kSsnStallImproveFactor = 0.99;
inline constexpr double kSsnDualGrowthFactor = 1e4;
inline constexpr double kSsnDualStepGrowth = 10.0;

// =============================================================================
// PHASE-7 TASK 6b PHASE B -- THE RESEARCH LEVERS' OWN CONSTANTS
// =============================================================================
//
// Every value here is reachable ONLY through a non-default SsnOptions field, so
// none of them can move a shipped trajectory. They are stated to the same
// standard as the constants above nonetheless, because a lever measured on an
// unjustified constant measures the constant rather than the lever.

// ---- R1: THE LEVENBERG-MARQUARDT SIZING ----------------------------------
//
// sigma_k = kSsnLmSigmaC * min(r_k, r_k^2), r_k = ||F_k||inf / f_scale, with
// f_scale = max(1, ||F_0||inf) the residual at THIS solve's own start point.
//
// WHY min(r, r^2) AND NOT r^2. Yamashita & Fukushima (Computing Suppl. 15,
// 2001) prove quadratic local convergence for mu_LM = ||F||^2 under a local
// error bound; Fan & Yuan (Computing 74, 2005) sharpen the exponent to any
// delta in [1, 2]. r^2 is the right size CLOSE to the solution (it vanishes
// fast enough not to spoil the Newton rate) and far too small FAR from it,
// where r is the size the same theory wants. min(r, r^2) is exactly "r^2 when
// r < 1, r when r > 1", i.e. the two regimes joined at the point where they
// agree, and it is the form the research pass registered.
//
// WHY THE NORMALIZATION IS THE START RESIDUAL. The theory sizes the shift in
// the units of the least-squares system; this file's shift sits inside a KKT
// block whose primal diagonal is H's, so the exponent and the constant are
// TUNABLE rather than derived (the research pass says so explicitly). Dividing
// by the start residual makes the lever's first sigma independent of the
// problem's absolute scaling, which is what makes an arm over an
// obj_scale = 1e9 population a measurement of the RULE and not of the scaling.
// Floored at 1 so a start point that is already converged cannot divide by a
// tiny number and manufacture a large r.
//
// c = 1 -- the neutral constant. With the floor and the cap below, c only
// selects WHERE between them sigma sits, and 1 is the value at which sigma is
// exactly the registered expression. A sweep over c belongs to the arm, not to
// this file.
inline constexpr double kSsnLmSigmaC = 1.0;

// ---- R2: THE WATCHDOG -----------------------------------------------------
//
// q = 1 relaxed step is the DEFAULT, and it is the value at which the watchdog
// reproduces the shipped iteration-0 exemption exactly on a hint that works:
// step 0 is taken unsearched, and if the residual has not come down by step 1
// the iteration returns to the best point it has stored and takes a monotone
// step from there. The research pass registers q = 1-2; 2 is reachable through
// SsnOptions::watchdog_q and is the second arm.
inline constexpr Index kSsnWatchdogQ = 1;

// ---- R4: THE FARKAS RESIDUAL TEST ----------------------------------------
//
// The system {Ae x = be, a_k^T x <= b_k} is infeasible IFF there is
// (y_e free, y >= 0) with Ae^T y_e + sum_k y_k a_k = 0 and
// be^T y_e + sum_k y_k b_k < 0 (Farkas). This file tests that pair on the
// NORMALIZED DUAL INCREMENT projected onto the sign cone -- one matvec plus
// O(m), and no factorization.
//
// kSsnFarkasResidualTol = 1e-6 on the RELATIVE residual
// ||A^T y||inf / max(1, ||(|A|^T |y|)||inf). Relative because the absolute
// residual scales with the problem's rows and the whole point of the test is
// that it survives bad scaling; 1e-6 because the direction being tested is an
// FB dual increment rather than an exact recession direction, so demanding
// more would test the iteration's convergence rather than the certificate's
// existence.
//
// **BOTH QUANTITIES ARE RELATIVE ONLY ABOVE 1** (Phase-B review, minor 6). The
// `max(1.0, .)` in each denominator is an ABSOLUTE FLOOR: when the
// cancellation-free scale of the sum is below 1 -- a small, well-scaled row
// block, or a `y` whose normalization left it small -- the denominator is 1 and
// the test is on the ABSOLUTE residual. The bad-scaling argument above is
// therefore about the direction that matters (the floor cannot be reached by
// scaling a row UP) and the floor is what stops a near-zero denominator from
// manufacturing a certificate out of rounding noise. Same reading for the gap.
//
// **SWEPT, AND THE SWEEP IS COMMITTED** (Phase-B review, Fable finding D). This
// used to say "swept on the probe's infeasible and badly-scaled-feasible
// populations" with no artifact behind it, on a value that -- because M11's A/B
// shows the residual conjunct does 100% of the refusing -- IS the certificate
// on every population this repository has. The curve is
// `docs/notes/data/2026-08-10-task6b-phaseB/r4_tolerance_sweep/`: nine points
// over 1e-10..1e-1, recall and all three false-positive populations at each.
//
// kSsnFarkasGapTol = 1e-8 on the RELATIVE Farkas objective
// <b, y> / max(1, sum_k |b_k y_k|), which must be at most -kSsnFarkasGapTol.
// A strictly negative <b, y> is the certificate's own second half; the
// tolerance exists only so that a rounding-noise negative cannot fire it, and
// it is eight orders below the O(1) values a genuine contradiction produces.
inline constexpr double kSsnFarkasResidualTol = 1e-6;
inline constexpr double kSsnFarkasGapTol = 1e-8;

// The three-set partition of CHR 2015's scaffold. kUncertain is unreachable
// with SsnSafeguards::kBare, which is what makes bare mode reproduce Task 3's
// binary partition exactly.
enum class SsnRowClass : std::uint8_t {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

// One bound of one variable, recast as an inequality row
//
//     c = sign * x(var) - rhs <= 0,   slack s = rhs - sign * x(var)
//
// so sign = -1, rhs = -lower(var) is the lower bound and sign = +1,
// rhs = upper(var) the upper one. Held in a flat, ascending-by-variable list
// (both rows of a two-sided variable adjacent, lower first) which IS the
// bound block's row order in K.
struct SsnBoundRow {
    Index var = 0;
    double sign = 1.0;
    double rhs = 0.0;
    // True iff this row's bound is the TRUST REGION's, strictly tighter than
    // whatever real bound (if any) the QP declares on this side. It is an
    // EXPORT attribute, not a structural one: a TR row occupies the same slot
    // in K as the real row it displaced, so this field must never enter the
    // structure key (see structure_hash / bound_rows_match_cached, the key's
    // two conjuncts) -- if it did, two different radii
    // would look like two different structures and the pattern cache would
    // rebuild on every major of a shrink-retry loop, which is exactly the tax
    // the cache exists to remove.
    bool from_tr = false;
};

// The two norms of F one iteration needs. Separate fields rather than two
// passes because they are accumulated in the same walk over the same blocks.
struct SsnNorms {
    double inf_norm = 0.0; // ||F||inf -- the CERTIFICATE (header section 5)
    double merit = 0.0;    // 1/2 ||F||_2^2 -- the LINE SEARCH's function
};

// What one factorization's reported inertia says about the system factorized.
//
// **THIS IS qp_engine.h's detail::InertiaVerdict, RESTATED RATHER THAN
// INCLUDED**, for the same reason kSsnInfBound is restated: ssn_engine.h
// depends on the QP data types and on the KKT factor helper, never on the
// walk's internals, and reaching across for a nine-line enum would couple
// this file to a 6000-line header. The verdict rules are qp_engine.h's
// exactly (docs/retarget-design-sqp.md SS4.1; the original justification,
// docs/notes/2026-07-27-pardiso-inertia-findings.md, applies unchanged):
// non-kObserved evidence routes to kSuspect explicitly, then perturbed
// pivots first (a factorization whose pivots were perturbed reports an
// inertia that looks exactly like a genuine one, so its counts carry no
// information AT ALL -- including when they happen to match; absent
// evidence on Accelerate triggers nothing), then the short-sum rule.
enum class SsnInertia {
    kOk,      // trustworthy AND equal to the expectation
    kSuspect, // untrustworthy: perturbed pivots, or counts that do not sum to n
    kWrong,   // trustworthy AND different from the expectation
};

inline SsnInertia ssn_inertia_verdict(const hven::linear::InertiaEvidence &e, Index expected_pos,
                                      Index expected_neg) {
    if (e.state != hven::linear::InertiaEvidence::State::kObserved) {
        return SsnInertia::kSuspect;
    }
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) {
        return SsnInertia::kSuspect;
    }
    const Index pos = static_cast<Index>(e.n_pos);
    const Index neg = static_cast<Index>(e.n_neg);
    if (pos + neg != expected_pos + expected_neg) {
        return SsnInertia::kSuspect;
    }
    return (pos == expected_pos && neg == expected_neg) ? SsnInertia::kOk : SsnInertia::kWrong;
}

} // namespace detail

// Why a solve stopped somewhere other than a converged point. kNone is the
// converged case.
//
// **ALL FIVE ARE NOW REACHABLE**, and Task 4 owns the two that were declared
// unreachable in Task 3 (the banner that promised them is discharged here):
//
//   kBudget            hard_budget attempts were spent. Says nothing about the
//                      problem -- only that this budget was too small.
//   kSingular          the factorization threw, the step came back non-finite,
//                      or the inertia was TRUSTWORTHY AND WRONG at the top of
//                      the proximal ladder. escape_detail names which.
//   kIndefinite        THE SECOND-ORDER VERIFICATION's verdict (review fix
//                      round 1, C1): the residual satisfies fb_tol -- so the
//                      point IS first-order KKT -- but the inertia of K at
//                      that very point is trustworthy and NOT (n, me+mi+mb),
//                      i.e. the primal block is not positive definite there.
//                      The point is a saddle or a maximizer of the QP, and no
//                      residual-based test can see that, which is the whole
//                      reason this verdict exists.
//   kNoContraction     THE LINE SEARCH's verdict: the Armijo schedule ran down
//                      to detail::kSsnMinStep without the merit 1/2||F||^2
//                      accepting, at the top of the proximal ladder. The step
//                      direction is not a descent direction for the merit and
//                      no amount of damping made it one.
//   kInfeasibleSuspect THE DIVERGENCE TELEMETRY's verdict: ||F||inf stalled
//                      (detail::kSsnStallWindow steps without a
//                      kSsnStallImproveFactor improvement) WHILE the multiplier
//                      norm grew by kSsnDualGrowthFactor. That is the signature
//                      an infeasible QP produces and a hard feasible one does
//                      not; the name says SUSPECT because it is a diagnosis
//                      from behaviour, never a Farkas certificate.
//
// **NONE OF THEM CERTIFIES ANYTHING.** An escaped SsnResult reports where the
// solve STOPPED. Task 5 routes an escaped subproblem back to the walk; that
// routing is the only correct consumption of any value below.
//
// **BRANCH ON escape_reason, NEVER ON status** (Fable review, M2). Two escapes
// map onto a QpStatus the WALK uses for a stronger statement than this file
// ever makes: kInfeasibleSuspect reports QpStatus::kInfeasible, which the walk
// issues as a CERTIFICATE, and kIndefinite/kNoContraction/kSingular all report
// QpStatus::kNumericalError. A Task-5 driver that switched on status alone
// would promote a suspicion to a certificate at the driver layer.
enum class SsnEscape {
    kNone = 0,
    kBudget = 1,
    kSingular = 2,
    kNoContraction = 3,
    kInfeasibleSuspect = 4,
    kIndefinite = 5,
};

// Which rows a caller believes are active, used for the FIRST Newton step only
// (header section 4). The two halves are INDEPENDENT: supply either, both, or
// neither. An empty half means "no hint for these rows" and those rows take
// the ordinary FB branch on step one like every later step; a non-empty half
// must be exactly sized or solve() throws.
struct SsnActivityHint {
    std::vector<bool> ineq;         // size mi, or empty
    std::vector<BoundState> bounds; // size n, or empty

    bool empty() const { return ineq.empty() && bounds.empty(); }
};

// The point a solve starts from. Every vector may be left EMPTY, which means
// "zero of the right size" -- so a default-constructed SsnStart is the cold
// start from the origin with no multipliers and no hint.
struct SsnStart {
    Vec x;        // n
    Vec lambda_e; // me
    Vec lambda_i; // mi, >= 0 expected but not required (the FB residual is
                  // defined for any sign; a negative seed simply reads as a
                  // large complementarity residual)
    // SIGNED bound multiplier, qp_problem.h's QpSolution::z convention: >= 0
    // at an active lower bound, <= 0 at an active upper bound. Split into the
    // two non-negative bound-row multipliers on ingest (lambda^lo = max(z, 0)
    // against a finite lower bound, lambda^up = max(-z, 0) against a finite
    // upper one) and recombined into SsnResult::z on exit.
    //
    // NOT IN THE TASK-3 BRIEF'S FIELD LIST, added because without it a warm
    // start cannot carry bound activity at all and Task 5 would have to widen
    // the struct on its first day.
    Vec z; // n

    // **IGNORED BY THE LOCAL METHOD, AND STRUCTURALLY SO.** For a QP the slack
    // of a row is a FUNCTION of x (s = bi - Ai x), so a separately seeded
    // slack block can only agree with x or contradict it; this file always
    // derives it. The field is present because Task 4's proximally stabilized
    // formulation carries an independent slack/dual block that a caller may
    // want to hand back in, and fixing the interface now is cheaper than
    // widening it then. Validated for size when non-empty, so a caller who
    // fills it wrongly is told; never read otherwise.
    Vec slacks; // mi, or empty

    // **STILL IGNORED AFTER TASK 4, AND THIS IS A RULING RATHER THAN AN
    // OMISSION.** Task 3 recorded them as "Task 4's proximal-point term reads
    // them"; Task 4's proximal term does not, because it anchors at the CURRENT
    // ITERATE instead of at a lagging centre.
    //
    // WHY THAT ANCHOR. With the centre at the current point the proximal term
    // perturbs only the JACOBIAN -- it is exactly an additive increment to the
    // (delta, mu) pair this file already applies -- so section 6's property
    // survives: the iteration stays modified Newton on the EXACT F, its fixed
    // points stay exact unregularized KKT points, and ONE residual per attempt
    // serves both the certificate and the merit. A LAGGING centre changes the
    // residual (F_sigma = F + sigma(w - wbar), with the FB rows carrying a
    // shifted slack), which splits the exit certificate into an inner and an
    // outer one and forces two residual evaluations per attempt. Nothing
    // measured in Task 4 needs that: the one thing a lagging centre uniquely
    // buys -- the divergence-of-the-proximal-step infeasibility certificate --
    // is a certificate this file does not claim to produce anyway
    // (SsnEscape::kInfeasibleSuspect is behavioural, and its stall/growth
    // telemetry sees the infeasible fixture without any of it).
    //
    // The fields stay, still size-checked when non-empty, because the decision
    // above is a Task-4 measurement rather than a permanent one: if Task 5 or
    // Task 6 finds a cell where a lagging centre is what closes it, the
    // interface is already the right shape. A caller who wants a warm proximal
    // sequence today uses SsnOptions::prox_sigma_init.
    Vec prox_center_x;      // n, or empty
    Vec prox_center_lambda; // me + mi, or empty

    SsnActivityHint activity_hint;
};

// WHICH ITERATION A SOLVE RUNS. Two modes, and the bare one exists to be a
// POSITIVE CONTROL rather than a product surface.
//
// **kBare IS TASK 3's KERNEL, BIT FOR BIT** -- full undamped steps, the binary
// alpha > beta partition with no uncertain set and no hysteresis, no line
// search, no dual projection, no proximal term and no inertia gate. Every
// iteration count Task 3 pinned reproduces under it exactly
// (tests/test_ssn_engine.cpp's BareModeReproducesTheTask3Trajectories), which
// is a much stronger statement than a compile-time switch could make: the two
// code paths are the same function, so the claim is checked on every ctest run
// rather than argued.
//
// **DEVIATION FROM THE BRIEF, DELIBERATE.** The brief writes this switch as
// "safeguards compile-time-off in the test". A compile-time switch in a
// header-only engine can only be a macro (which tycho style avoids and which
// makes the two variants un-co-testable in one TU) or a template parameter on
// SsnEngine (which would propagate into every Task-5 driver signature and into
// SqpCounters). A runtime enum costs one predictable branch per solve on a path
// that has already paid a sparse factorization, keeps both modes in ONE binary
// so a single test can compare them directly, and hands Task 6 an ablation
// lever it would otherwise have to rebuild the library to get.
enum class SsnSafeguards {
    kBare = 0, // Task 3's local method, exactly
    kFull = 1, // the production iteration (default)
};

// Per-solve knobs. The regularizers delta/mu are NOT here: they come from the
// QpOptions the SsnEngine was constructed with, exactly as QpEngine takes
// its own, so the two kernels are regularized identically by construction.
struct SsnOptions {
    // Which iteration to run. kFull is the shipped default; kBare is the
    // positive control (see SsnSafeguards).
    SsnSafeguards safeguards = SsnSafeguards::kFull;

    // THE ESCALATION THRESHOLD, and it is a threshold on ATTEMPTS rather than
    // on accepted steps -- an attempt that ends in a rejected step is exactly
    // the kind of trouble the escalation exists for, so it must count.
    //
    // On reaching it a solve that is still running unregularized turns the
    // proximal term ON at detail::kSsnProxInit. That is the whole of the
    // "soft_budget warns" contract and the warning is OBSERVABLE, not printed:
    // SsnCounters::ssn_prox_updates leaves zero. There is no separate warn
    // counter, deliberately -- adding one would widen SqpCounters for a fact
    // ssn_prox_updates already carries.
    //
    // 12 is the brief's registered value, kept rather than re-derived: every
    // benign fixture in tests/test_ssn_engine.cpp converges in at most 9
    // attempts (BoxQpCountGrowsSlowlyInN's n = 400 cell is the largest), so the
    // threshold is provably unreachable on all of them and the escalation is
    // inert there by construction -- the strongest form of old-behaviour
    // preservation this project recognises (the identification-stall note's
    // "provably inert on every one of them" standard).
    Index soft_budget = 12;

    // THE HARD CAP, and it is a cap on ATTEMPTS rather than on accepted steps
    // (Fable review, M3: the field used to say "Newton-step cap" while the code
    // tested the attempt index -- SsnResult::factorizations states the real
    // invariant and this now agrees with it). A solve that reaches it stops
    // with QpStatus::kMaxIter and SsnEscape::kBudget. Must be >= 0; 0 means
    // "test the start point and take no step".
    //
    // **THE SECOND-ORDER VERIFICATION IS NOT AN ATTEMPT AND IS NOT CAPPED BY
    // THIS FIELD** (header section 7b). Under kFull a start point that is
    // already converged still pays the ONE verification factorization at
    // hard_budget = 0, because the alternative is certifying a point nothing
    // ever looked at. It takes no step, so the field's contract is intact.
    Index hard_budget = 25;

    // ||F||_inf convergence threshold. Default = SqpOptions::kkt_tol's own
    // default, per the derivation in header section 5; use
    // ssn_fb_tol_from_kkt_tol() to track a non-default kkt_tol. Must be > 0.
    double fb_tol = 1e-6;

    // THE PROXIMAL TERM'S STARTING VALUE, and **0.0 IS THE RULED DEFAULT**
    // rather than the leftover it was in Task 3.
    //
    // The ruling: the proximal term is a REPAIR, not a policy. It costs
    // iterations wherever it is on (it damps the Newton step toward the current
    // point), it repairs nothing on a solve that is converging, and every
    // fixture that needs it is a fixture the escalation ladder ARMS it on --
    // the line search exhausting its schedule, a wrong inertia, or crossing
    // soft_budget. Shipping it on by default would slow every healthy solve to
    // insure against cases the ladder already covers. A caller CONTINUING a
    // proximal sequence (Task 5's re-solve after an escape) can start it warm
    // by setting this field; must be >= 0.
    double prox_sigma_init = 0.0;

    // THE UNCERTAIN BAND's entering threshold, on |alpha - beta| -- see
    // detail::kSsnUncertainEnter for the geometry, the sweep and the
    // hysteresis ratio that derives the LEAVING threshold from it.
    //
    // 0.0 DISABLES THE UNCERTAIN SET without disabling anything else, which is
    // what makes "the uncertain set, ablated" a one-field experiment for Task 6
    // rather than a rebuild. Must be in [0, 1): at 1 a strictly active row
    // (|alpha - beta| = 1) would itself be uncertain and the partition would
    // carry no information at all.
    double uncertain_tol = detail::kSsnUncertainEnter;

    // =====================================================================
    // PHASE-7 TASK 6b PHASE B -- THE FOUR RESEARCH LEVERS (R5/R1/R2/R4)
    // =====================================================================
    //
    // Every field below is OPT-IN and ships at the value that reproduces the
    // shipped iteration BIT FOR BIT. They exist so Grant's ratification at
    // task close reads ADOPT/REJECT evidence rather than a proposal; none of
    // them is a default flip and none of them may become one in this task.
    // Their evidence is
    // `.superpowers/sdd/2026-08-05-scale-engine/task-6b-phaseB-report.md`
    // and their research provenance is
    // docs/notes/research/fable_fb_ssn_globalization_second_pass_claude.md.

    // ---- R5: DEFER THE CERTIFYING EXIT'S INERTIA EVIDENCE ---------------
    //
    // false (default) -- section 7b verbatim: the convergence test opens a
    // VERIFICATION ATTEMPT that factorizes K at the converged point and reads
    // its inertia before kOptimal is issued.
    //
    // true -- the verification attempt is built (rows classified at the
    // converged point, sigma dropped to the caller's own regularization, K's
    // diagonals refreshed) but NOT FACTORIZED. The solve returns kOptimal with
    // `SsnResult::certification_deferred` set, and the caller owes the engine
    // exactly one of `finish_deferred_certification()` (which pays that
    // factorization and reads the verdict) or `discard_deferred_certification()`
    // before the next solve.
    //
    // **WHY A CALLER WOULD WANT THAT** is Gould's lemma (Gould, Math. Prog. 32,
    // 1985): the KKT matrix of the face EQP has inertia (n_f, m_f, 0) IFF the
    // reduced Hessian on that face is positive definite, so a caller that is
    // about to re-solve the identified face EXACTLY -- which is what
    // QpEngine::refine_on_face does, gating on that very verdict -- already
    // buys the second-order evidence and does not need this file to buy it
    // twice. The certificate the two routes issue is NOT the same statement:
    // this file's verification tests (n, me+mi+mb) on the FULL augmented block
    // at the caller's regularization, the face route tests positive
    // definiteness of the reduced Hessian ON THE IDENTIFIED FACE. At a
    // certifying exit on an identified active set the face statement is the
    // semantically correct one (the full-block statement is stronger only
    // through the delta/sigma shift it carries, which is a statement about the
    // regularized model rather than about the QP); the two can therefore
    // disagree, and the measurement arm reports whether they ever do.
    //
    // **NOTHING IS WEAKENED WHEN THE FACE SOLVE IS REFUSED.** A caller whose
    // refinement is refused calls finish_deferred_certification() and gets
    // exactly the shipped verdict at exactly the shipped cost, because the
    // matrix it factorizes is the one this loop already built.
    bool defer_certification = false;

    // ---- R1: HOW sigma IS SIZED (sqp_types.h's SsnSigmaRule) ------------
    //
    // kLadder (default) is section 7's failure-reactive ladder verbatim. The
    // two residual rules replace the CLIMB with a size read off the residual:
    //
    //     sigma_k = max(ladder_k, clamp(c * min(r_k, r_k^2),
    //                                   kSsnProxInit, kSsnProxMax))
    //
    // with r_k = ||F_k||inf / max(1, ||F_0||inf) and c = detail::kSsnLmSigmaC.
    //
    // **THE LADDER IS RETAINED AS A MONOTONE FLOOR, NOT REPLACED**, which is
    // what the research pass asks for ("keep the ladder as the fallback when
    // the LM-sized shift still yields kSingular") and what keeps the escape
    // routes intact: every escalation trigger still climbs a rung, so a solve
    // that cannot be repaired still reaches the ceiling and still escapes with
    // the same reason. What changes is that between triggers sigma is sized
    // from the evidence rather than from the history -- and a residual that
    // FALLS lowers sigma back toward the floor, which the monotone ladder
    // structurally cannot do.
    //
    // kResidualArmed is INERT until the ladder arms (soft_budget crossing, a
    // wrong inertia, or an exhausted line search), so it is provably inert on
    // every fixture whose ladder never arms. kResidualAlways sizes sigma from
    // attempt 0, so it carries at least kSsnProxInit everywhere and is inert
    // NOWHERE -- that is the arm that answers whether LM sizing prevents the
    // wasted overshoot trial step rather than merely shortening the recovery.
    SsnSigmaRule sigma_rule = SsnSigmaRule::kLadder;

    // ---- R2: WHAT PROTECTS THE HINTED FIRST STEP ------------------------
    //
    // kIterationZeroFree (default) is section 7 step 9's exemption verbatim.
    // kWatchdog replaces it with the published rule the exemption is an
    // unsafeguarded special case of: up to `watchdog_q` relaxed (unsearched)
    // steps from the hinted start, a stored BEST point, and -- if the merit has
    // not achieved Armijo decrease against the watchdog's own reference by then
    // -- a RETURN TO THAT BEST POINT followed by a monotone step.
    //
    // The exemption and the watchdog agree exactly whenever the hint was right
    // (the relaxed step lands at the solution and the convergence test fires
    // before the watchdog can ever judge it) and differ exactly where the
    // exemption has no answer: a wrong hint whose relaxed step is accepted and
    // bad. `watchdog_q` must be >= 1; the research pass registers 1-2.
    SsnHintRule hint_rule = SsnHintRule::kIterationZeroFree;
    Index watchdog_q = detail::kSsnWatchdogQ;

    // ---- R4: WHAT TURNS A SUSPICION INTO AN EXIT ------------------------
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
    // budget/no-contraction routes. That is the whole trade the arm measures --
    // false positives against recall.
    SsnInfeasibilityRule infeasibility_rule = SsnInfeasibilityRule::kSymptoms;
};

// The tolerance derivation of header section 5, in code: an FB residual at
// `kkt_tol` buys stationarity and equality feasibility at exactly kkt_tol and
// per-row complementarity within detail::kSsnComplementarityFactor of it.
// Throws std::invalid_argument on a non-positive kkt_tol.
inline double ssn_fb_tol_from_kkt_tol(double kkt_tol) {
    if (!(kkt_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("ssn_fb_tol_from_kkt_tol: kkt_tol must be > 0, got {}", kkt_tol));
    }
    return kkt_tol;
}

struct SsnResult {
    QpStatus status = QpStatus::kOptimal;
    SsnEscape escape_reason = SsnEscape::kNone;

    // PHASE-7 TASK 6b PHASE B, R5. True iff this solve reached a certifying
    // exit under SsnOptions::defer_certification and therefore returned
    // kOptimal WITHOUT the second-order verification having been factorized.
    // The status is provisional until the caller calls
    // SsnEngine::finish_deferred_certification() (which may withdraw it) or
    // SsnEngine::discard_deferred_certification() (which drops the pending
    // evidence unread, legitimate only when the caller is discarding the exit
    // anyway). ALWAYS false when the option is off, on an escape, and under
    // kBare -- so a consumer that never sets the option never sees it set.
    bool certification_deferred = false;

    // PHASE-7 TASK 6b PHASE B -- THE TWO LEVER INSTRUMENTS.
    //
    // They live on SsnResult rather than on SsnCounters DELIBERATELY: they are
    // measurements of an opt-in arm, not of the product, and SqpCounters::ssn
    // is serialized into the corpus CSV whose schema is a pinned artifact.
    // Putting them here keeps the schema at 37 and keeps a reader of a shipped
    // artifact from finding a column that is structurally zero.
    //
    // `watchdog_returns` (R2) -- times the q-step watchdog exhausted its
    // relaxed window without Armijo decrease and RETURNED TO THE BEST STORED
    // POINT. Structurally 0 under SsnHintRule::kIterationZeroFree.
    //
    // `farkas_fired` / `farkas_refusals` (R4) -- infeasibility exits the
    // Farkas certificate CONFIRMED, and armed symptom pairs it REFUSED (i.e.
    // the shipped rule would have declared kInfeasible and this one did not).
    // Both structurally 0 under SsnInfeasibilityRule::kSymptoms.
    Index watchdog_returns = 0;
    Index farkas_fired = 0;
    Index farkas_refusals = 0;

    Vec x, lambda_e, lambda_i;
    // Signed bound multiplier, recombined from the two bound-row multipliers
    // (z_j = lambda^lo_j - lambda^up_j). Same convention as QpSolution::z.
    Vec z;

    // Newton steps ACCEPTED -- identical to counters.ssn_iters, which exists
    // separately only so that SqpCounters::ssn can aggregate the whole struct
    // in Task 5 without special-casing one field.
    Index iters = 0;
    // Numeric factorizations paid, one per ATTEMPT.
    //
    // **TASK 4 SEPARATED THESE TWO** and the gap is the safeguards' cost, read
    // directly. Task 3 could say "exactly one factorization per Newton step,
    // so this equals iters"; with a line search and an escalation ladder an
    // attempt can pay its factorization and then take NO step -- the schedule
    // was exhausted, or the inertia came back wrong -- so the invariant is now
    //
    //     iters <= factorizations <= min(attempts, hard_budget) + 1,
    //
    // where the +1 is the CERTIFYING EXIT's second-order verification (header
    // section 7b), which is the reason equality on the left is unreachable
    // under kFull: a certified guarded solve pays exactly one factorization
    // more than it takes steps, and an escaped one pays at least as many as it
    // takes. **UNDER SsnOptions::defer_certification THE +1 IS NOT PAID BY
    // THIS SOLVE** -- it moves to finish_deferred_certification(), which adds
    // it to THIS field when the caller runs it, or is never paid at all when
    // the caller's face solve supersedes it. So a deferring solve can read
    // iters == factorizations at a certifying exit, and that is the lever
    // working rather than the invariant breaking. Under kBare (no verification, no line search, no
    // ladder) equality is exact and Task 3's contract stands verbatim. A regression that
    // refactorized per branch change still shows up here and nowhere else.
    Index factorizations = 0;
    // Pardiso phase-11 symbolic analyses paid. 1 for the first solve of a new
    // structure on this engine, 0 for every later solve of the same structure
    // -- header section 3.
    Index symbolic_analyses = 0;
    // TRIPLET REBUILDS of K's sparsity pattern paid by this solve: 1 when the
    // structure differs from the one this engine currently holds, 0 when it
    // was reused and only VALUES were refreshed (review fix round 1, M5). The
    // sibling of symbolic_analyses one level down -- that field counts what
    // PARDISO re-derives, this one counts what THIS FILE re-derives, and the
    // "one symbolic analysis per structure" story was only ever telling half
    // of it.
    Index pattern_rebuilds = 0;

    // ||F(w)||_inf at the returned point, on the UNSCALED residual.
    double fb_residual = 0.0;

    // THE IMPLIED ACTIVE SET AT THE RETURNED POINT (review fix round 1, M4),
    // in the same two shapes QpSolution reports it (qp_problem.h): one flag per
    // row of Ai, one BoundState per variable. Derived from the FINAL iterate by
    // the engine's own partition rule -- row active iff lambda > s, equivalently
    // alpha > beta (header section 2) -- so it is the same partition the last
    // Jacobian would have selected, not a separately maintained working set.
    //
    // WRITE-ONLY, LIKE THE COUNTERS: nothing in this file reads either vector
    // back, and computing them cannot move a trajectory. They exist because
    // Task 5 needs an activity export for warm-start hand-off and for the
    // stable-face refinement, and widening the struct later is the cost this
    // avoids -- the same argument SsnStart::z is carried on.
    //
    // ALWAYS POPULATED, INCLUDING ON AN ESCAPE and including on a solve that
    // took zero steps: the derivation reads the iterate, not the loop's
    // history, so there is no state to be missing. On an escaped solve it
    // describes where the solve STOPPED and certifies nothing.
    //
    // A variable whose lower AND upper bound rows are both implied active
    // reports kFixed; one with neither reports kFree. A variable with no finite
    // bound at all always reports kFree, since it has no row to be active.
    std::vector<bool> ineq_active;       // mi
    std::vector<BoundState> bound_state; // n

    // TRUST-REGION ACTIVITY, size n, and qp_problem.h's QpSolution::tr_active
    // contract verbatim (Fable kernel review, I1). True at index i iff the
    // variable is held by a TR-tight effective bound rather than a real one.
    // Such a variable reports kFree in bound_state above -- which is a
    // REAL-BOUND-ONLY view -- and z(i) is 0, because TR duals are internal to
    // this solve and are never exposed.
    //
    // SAME STATIONARITY CAVEAT AS THE WALK'S: at a TR-pinned index the
    // reported quantities do NOT satisfy stationarity, since the multiplier
    // that actually balanced that row was dropped on the way out. A kFree entry
    // there is evidence that no REAL bound constrains the coordinate, NOT
    // evidence that the point is stationary in it. **Read tr_active, never z or
    // bound_state, to detect a binding radius.**
    //
    // All-false whenever the solve ran without a trust region (the default
    // SolveOverrides), and all-false when a finite radius never bound.
    std::vector<bool> tr_active; // n

    // **HOW FAR x LIES OUTSIDE THE TRUST REGION, AND IT CAN BE FAR** (review
    // fix round 1, I3). max_j max(0, x_j - up_eff_j, lo_eff_j - x_j) over the
    // variables whose effective bound came from the RADIUS -- 0.0 when no
    // radius was supplied, and 0.0 when the radius held.
    //
    // THE CONTRACT, stated here because Task 5's funnel is the consumer and its
    // ratio test presumes ||d||inf <= Delta:
    //
    //   * THE TRUST REGION IS A SOFT CONSTRAINT IN THIS KERNEL. It enters as FB
    //     bound ROWS, and an FB row is satisfied only at a root -- so no
    //     intermediate iterate is confined to the box, the line search does not
    //     confine it either (the TR rows are four more terms in the same merit,
    //     not a step-length cap), and a solve that stops early can stop
    //     anywhere. MEASURED on cycling_qp_3var from x0 = 0 at tr_radius =
    //     0.01: hard_budget 1..5 return ||x||inf between 1.33 and 1.60, i.e.
    //     133x to 160x the radius.
    //   * ON A CERTIFYING EXIT the violation is bounded by the tolerance and
    //     nothing else: |phi| <= fb_tol permits a slack negative by
    //     O(fb_tol) (header section 5's hypothesis caveat, applied to the TR
    //     rows in particular), so tr_violation <= kSsnComplementarityFactor *
    //     fb_tol ~ 1.71 fb_tol. That is the only exit at which this kernel's
    //     x may be used as a trust-region step.
    //   * ON ANY ESCAPE the value is UNBOUNDED and is reported rather than
    //     repaired. It is deliberately NOT clamped: clamping would break the
    //     one invariant the export has -- that x, fb_residual, ineq_active,
    //     bound_state, tr_active and the uncertain flags all describe ONE
    //     point -- and it would hand the funnel a point whose residual it had
    //     never been measured at. Task 5 routes every escape to the walk (the
    //     SsnEscape banner's standing instruction); this field is what lets it
    //     ASSERT that it did.
    double tr_violation = 0.0;

    // THE UNCERTAIN SET AT THE RETURNED POINT (Task 4), the third leg of the
    // CHR partition that ineq_active/bound_state cannot express -- those two
    // report the BINARY reading (lambda > s) and always will, because Task 5's
    // hint ingest and QpSolution's own contract are binary.
    //
    // ineq_uncertain[k] is true iff row k of Ai was in the uncertain set when
    // the LAST generalized Jacobian was assembled; bound_uncertain[j] is true
    // iff EITHER of variable j's bound rows was (a variable with one uncertain
    // side is an uncertain variable -- the pessimistic reading, since the point
    // of the flag is to warn a consumer off trusting the binary one).
    //
    // **THE LAST ASSEMBLY**, which under kFull is the RETURNED ITERATE on every
    // certifying exit (the second-order verification classifies there, header
    // section 7b) and is one iterate back on an escape. The distinction still
    // matters because the classification carries HYSTERESIS, so it is a
    // function of the whole trajectory and cannot be recomputed from the final
    // point. On a solve that assembled no Jacobian at all -- kBare converged at
    // the seed, or hard_budget = 0 under kBare -- both vectors are all-false,
    // which is the honest reading of "no classification was ever made" and not
    // a claim that every row was decided. Always sized (mi / n), like every
    // other export here.
    //
    // ALWAYS ALL-FALSE UNDER SsnSafeguards::kBare, which has no uncertain set.
    std::vector<bool> ineq_uncertain;  // mi
    std::vector<bool> bound_uncertain; // n

    // The proximal sigma this solve FINISHED at (0.0 whenever the ladder never
    // armed, which is every benign fixture). Write-only, like the counters, and
    // the one number a caller needs to restart a proximal sequence through
    // SsnOptions::prox_sigma_init.
    double prox_sigma = 0.0;

    // Why the solve stopped, in words, whenever there are words to add: the
    // linear solver's own message on kSingular, the stall/growth figures on
    // kInfeasibleSuspect, the exhausted schedule on kNoContraction. Carried
    // out rather than printed (tycho rule T6 -- a diagnostic this file does not
    // throw is one it must still hand to its caller). Empty on kNone and on
    // kBudget, which say everything they have to say in the enum.
    std::string escape_detail;

    SsnCounters counters;
};

// =============================================================================
// The engine
// =============================================================================
//
// Holds ONE KktFactor across solves, which is what makes the "symbolic
// analysis once per structure, reused across QPs of identical structure"
// property of header section 3 observable: construct one SsnEngine and solve a
// sequence of same-shaped QPs through it.
//
// NOT THREAD-SAFE and not copyable, for the same reason the sparse factor is not: it
// owns live Pardiso/Accelerate internal state.
class SsnEngine {
  public:
    explicit SsnEngine(const QpOptions &opts) : opts_(opts) {}

    SsnEngine(const SsnEngine &) = delete;
    SsnEngine &operator=(const SsnEngine &) = delete;

    const QpOptions &options() const { return opts_; }

    // Solve `qp` from `start`. `out` must be non-null and is fully overwritten.
    //
    // Throws std::invalid_argument for a null `out`, a malformed QpProblem
    // (qp.validate()'s own diagnostics), a wrongly sized start/hint vector, or
    // an out-of-range SsnOptions field -- i.e. for caller errors, which are
    // distinguished from SOLVER outcomes: a solve that cannot make progress
    // reports a status and an SsnEscape and does not throw.
    //
    // The brief for this task calls the first parameter's type `QpData`; this
    // repository's "QP subproblem view" is qp_problem.h's QpProblem and that
    // is what is used here -- there is no second type and no alias.
    //
    // This overload forwards a default-constructed SolveOverrides, which
    // resolves to every opts_ value unchanged, so it is BYTE-IDENTICAL to
    // solving with no override support at all -- the same guarantee
    // QpEngine's plain overloads carry (qp_types.h).
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
               SsnResult *out) {
        solve(qp, start, sopts, SolveOverrides{}, out);
    }

    // THE PER-SOLVE SEAM (Fable kernel review, I1). Takes the WALK'S OWN
    // SolveOverrides (qp_types.h) rather than a parallel type, because the funnel
    // driver already builds one per subproblem and per SOC re-solve, and a
    // second struct would only be a translation layer for Task 5 to maintain.
    // Every field's sentinel and resolution rule is qp_types.h's, unchanged:
    // tr_radius +inf means "no radius", primal_delta/dual_mu negative means
    // "use the engine's".
    //
    // TRUST REGION. lo_eff = max(lower, x0 - Delta), up_eff = min(upper,
    // x0 + Delta), about THIS solve's own start point, resolved once here and
    // used for every bound row thereafter -- see build_bound_rows for the
    // pattern-invariance property that makes a shrink-retry loop free, and
    // export_activity/recombine_bound_multipliers for the TR-pin/real-bound
    // separation that keeps radius artefacts out of z and bound_state.
    //
    // REGULARIZERS. primal_delta/dual_mu are honoured too, for a reason worth
    // stating rather than assuming: ignoring a field the caller deliberately
    // set is precisely the silent-drop antipattern the seeded-z check was
    // added to remove. **BUT THE DRIVER'S ADAPTIVE-mu SCHEDULE BUYS NOTHING
    // HERE**, and Task 5 should switch it off deliberately rather than
    // discover this: delta and mu perturb only the JACOBIAN (for_each_entry's
    // emissions and the FB diagonal) and never the residual, so this
    // iteration is modified Newton on the EXACT F and its fixed points are
    // exact, unregularized KKT points. They cost iterations, not accuracy --
    // the opposite trade from the walk, whose returned solution carries the
    // mu|lambda| footprint that motivated the schedule in the first place.
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
               const SolveOverrides &overrides, SsnResult *out) {
        if (out == nullptr) {
            throw std::invalid_argument("SsnEngine::solve: out must not be null");
        }
        // R5's contract, enforced rather than documented: a pending deferred
        // certification is EVIDENCE ABOUT K, and this solve is about to
        // overwrite K. Silently dropping it would let a caller believe a
        // certificate that nothing ever verified -- the same wrong-answer class
        // section 7b exists to close -- so the omission is a caller error and
        // is reported as one.
        if (deferred_pending_) {
            throw std::invalid_argument(
                "SsnEngine::solve: a deferred certification from the previous solve is still "
                "pending (SsnOptions::defer_certification was set and neither "
                "finish_deferred_certification() nor discard_deferred_certification() was "
                "called). The pending verdict is evidence about a matrix this solve is about "
                "to overwrite");
        }
        qp.validate();
        validate_options(sopts);
        validate_overrides(overrides);

        // Resolved once, exactly like QpEngine::run()'s own resolution. The
        // BASE pair is what the caller asked for; eff_* carries the proximal
        // increment on top of it and is what for_each_entry emits.
        const double tr_radius = overrides.tr_radius;
        base_delta_ = overrides.primal_delta < 0.0 ? opts_.primal_delta : overrides.primal_delta;
        base_mu_ = overrides.dual_mu < 0.0 ? opts_.dual_mu : overrides.dual_mu;
        const bool guarded = sopts.safeguards == SsnSafeguards::kFull;
        prox_sigma_ = guarded ? sopts.prox_sigma_init : 0.0;
        // R1: the ladder's OWN monotone state, which is `prox_sigma_` exactly
        // under the shipped rule and its FLOOR under the residual rules.
        ladder_sigma_ = prox_sigma_;
        lm_sigma_ = 0.0;
        eff_delta_ = base_delta_ + prox_sigma_;
        eff_mu_ = base_mu_ + prox_sigma_;

        const Index n = qp.n();
        const Index me = qp.me();
        const Index mi = qp.mi();

        validate_start(qp, start);

        *out = SsnResult{};
        out->x = seed_vector(start.x, n, "x");
        // The bound rows depend on the START POINT through the trust region, so
        // they are built after the seed is resolved and before anything reads
        // bound_rows_ (the multiplier split is the first such reader).
        build_bound_rows(qp, out->x, tr_radius);
        const Index mb = static_cast<Index>(bound_rows_.size());
        out->lambda_e = seed_vector(start.lambda_e, me, "lambda_e");
        out->lambda_i = seed_vector(start.lambda_i, mi, "lambda_i");
        Vec lambda_b = split_bound_multipliers(qp, seed_vector(start.z, n, "z"));

        out->pattern_rebuilds = sync_matrix(qp, n, me, mi, mb) ? 1 : 0;

        // Scratch, sized once.
        Vec resid_x(n), resid_e(me);
        Vec slack_i(mi), slack_b(mb);
        Vec phi_i(mi), phi_b(mb);
        Vec rhs(dim_);
        std::vector<double> alpha(static_cast<std::size_t>(mi + mb));
        std::vector<double> beta(static_cast<std::size_t>(mi + mb));
        std::vector<double> row_resid(static_cast<std::size_t>(mi + mb));
        // The three-set partition, and the PREVIOUS one -- which is the
        // hysteresis state, not just the flip counter's memory.
        std::vector<detail::SsnRowClass> klass(static_cast<std::size_t>(mi + mb),
                                               detail::SsnRowClass::kInactive);
        std::vector<detail::SsnRowClass> prev_klass;
        bool any_klass = false;

        // The LINE SEARCH's trial POINT. Unavoidable -- a trial step has to be
        // formed somewhere, and it cannot be formed in place because a rejected
        // one has to be discarded.
        //
        // **ITS RESIDUAL, HOWEVER, IS MEASURED IN THE WORKING SCRATCH ABOVE**,
        // which is worth stating because the obvious implementation gives the
        // line search a second set of residual blocks and that second set is
        // n + me + 2mi + 2mb doubles -- at a bounds-heavy QP at n = 1e6 (so
        // mb = 2e6) it is ~40 MB, against a phase whose cold ceiling figure is
        // 1285 MiB. The price of not paying it is ONE extra O(nnz) residual
        // evaluation per SOLVE (not per attempt): the loop re-evaluates at the
        // returned point just before the activity export, unconditionally, so
        // the export cannot read blocks a trial point left behind. Against the
        // factorizations a solve pays that is noise, and it removes a whole
        // class of "which point does this scratch describe" bug rather than
        // documenting one.
        Vec t_x(n), t_le(me), t_li(mi), t_lb(mb);

        // Divergence telemetry (SsnEscape::kInfeasibleSuspect). Every field is
        // WINDOWED and every window advances on ACCEPTED STEPS -- the three
        // properties detail::kSsnStallWindow derives, and the three the first
        // implementation did not have (fix round 1, I2 / F2).
        double window_ref = std::numeric_limits<double>::infinity();
        double dual_window = 1.0; // ||lambda||inf at the last accepted step that PROGRESSED
        double dual_start = 1.0;  // ||lambda||inf at the seed
        double dual_prev = 1.0;   // ||lambda||inf at the PREVIOUS accepted iterate
        double dual_last = 1.0;   // ||lambda||inf at the current accepted iterate
        Index window_len = 0;
        Index stall_len = 0;    // the window length the stall verdict was taken on
        Index window_iter = -1; // the out->iters this window last advanced on
        bool window_damped = false;
        bool stalled = false;

        // The second-order verification's saved sigma (section 7b): >= 0 only
        // while a verification is running with the ladder temporarily dropped
        // to the caller's own regularization.
        double verify_sigma = -1.0;

        // ---- R1 (Task 6b Phase B): the residual sizing's own state --------
        //
        // `f_scale` is fixed at the START residual and never moves, so the
        // normalized r_k is a statement about progress rather than about the
        // problem's absolute scaling. `lm_sigma_`/`ladder_sigma_` are members
        // because escalate_prox() and set_prox_sigma() both need them; they are
        // reset per solve at the top of this function.
        const bool lm_sigma_rule = guarded && sopts.sigma_rule != SsnSigmaRule::kLadder;
        double f_scale = 1.0;

        // ---- R2 (Task 6b Phase B): the watchdog's own state ---------------
        //
        // All of it is untouched under the shipped kIterationZeroFree rule, and
        // the four vectors are not even sized there.
        const bool watchdog_rule = guarded && sopts.hint_rule == SsnHintRule::kWatchdog;
        bool wd_active = false;
        Index wd_used = 0;
        double wd_ref_merit = 0.0;
        double wd_best_merit = std::numeric_limits<double>::infinity();
        Vec wd_best_x, wd_best_le, wd_best_li, wd_best_lb;

        // ---- R4 (Task 6b Phase B): the Farkas gate's own snapshots --------
        //
        // The DUAL VECTORS at the two reference points the two symptom routes
        // already measure their growth against -- the window's reference (the
        // STANDING route) and the previous accepted iterate (the EXHAUSTION
        // route). Sized and copied only under kFarkasGated.
        const bool farkas_rule =
            guarded && sopts.infeasibility_rule == SsnInfeasibilityRule::kFarkasGated;
        Vec wref_le, wref_li, wref_lb; // duals at the window's reference point
        Vec prev_le, prev_li, prev_lb; // duals at the previous accepted iterate
        Vec last_le, last_li, last_lb; // duals at the current accepted iterate
        if (farkas_rule) {
            wref_le = out->lambda_e;
            wref_li = out->lambda_i;
            wref_lb = lambda_b;
            prev_le = wref_le;
            prev_li = wref_li;
            prev_lb = wref_lb;
            last_le = wref_le;
            last_li = wref_li;
            last_lb = wref_lb;
        }

        for (Index it = 0;; ++it) {
            const detail::SsnNorms nrm =
                residual(qp, out->x, out->lambda_e, out->lambda_i, lambda_b, resid_x, resid_e,
                         slack_i, slack_b, phi_i, phi_b);
            out->fb_residual = nrm.inf_norm;
            if (it == 0) {
                // R1's normalization, fixed once (see detail::kSsnLmSigmaC).
                f_scale = std::max(1.0, nrm.inf_norm);
            }

            // --- convergence, and THE SECOND-ORDER VERIFICATION (C1) ----
            //
            // Under kBare this is Task 3's test, unchanged and free. Under
            // kFull it does NOT exit: it opens a verification attempt, which
            // falls through to the classification and the factorization below
            // and certifies only on the inertia verdict read THERE. Header
            // section 7b is the whole contract, including why an earlier
            // attempt's verdict cannot stand in for this one and why the
            // ladder is dropped to the caller's own regularization first.
            bool verifying = false;
            if (nrm.inf_norm <= sopts.fb_tol) {
                if (!guarded) {
                    out->status = QpStatus::kOptimal;
                    out->escape_reason = SsnEscape::kNone;
                    break;
                }
                verifying = true;
                if (prox_sigma_ > 0.0) {
                    verify_sigma = prox_sigma_;
                    set_prox_sigma(0.0, qp, n, me, mi, mb);
                }
            }

            // --- divergence telemetry, BEFORE the budget test -----------
            //
            // Order matters and is deliberate: an infeasible QP that also runs
            // out of budget must report the DIAGNOSIS, not the budget. kBudget
            // says "this budget was too small", which on an infeasible QP is
            // both true and useless, and it is the value a Task-5 caller would
            // respond to by re-solving with a bigger one.
            double dn = 0.0;
            if (guarded && !verifying) {
                dn = dual_norm(out->lambda_e, out->lambda_i, lambda_b);
                // The window advances once per ACCEPTED STEP. A ladder retry
                // re-enters this loop at the SAME iterate with the same
                // residual and must not be allowed to fill it.
                if (window_iter != out->iters) {
                    dual_prev = dual_last;
                    dual_last = std::max(dn, 1.0);
                    if (farkas_rule) {
                        // R4: the VECTORS behind dual_prev/dual_last, carried in
                        // lock-step with the scalars so the certificate's
                        // increment and the growth conjunct's ratio are measured
                        // between the same two points.
                        prev_le = last_le;
                        prev_li = last_li;
                        prev_lb = last_lb;
                        last_le = out->lambda_e;
                        last_li = out->lambda_i;
                        last_lb = lambda_b;
                    }
                    if (window_iter < 0) {
                        dual_start = dual_last;
                    }
                    window_iter = out->iters;
                    if (nrm.inf_norm <= window_ref * detail::kSsnStallImproveFactor) {
                        // Genuine progress: a fresh window, and the STANDING
                        // route's growth reference re-arms with it.
                        window_ref = nrm.inf_norm;
                        dual_window = dual_last;
                        if (farkas_rule) {
                            wref_le = out->lambda_e;
                            wref_li = out->lambda_i;
                            wref_lb = lambda_b;
                        }
                        window_len = 0;
                        stall_len = 0;
                        window_damped = false;
                        stalled = false;
                    } else if (++window_len >= detail::kSsnStallWindow) {
                        if (window_damped) {
                            // sigma moved inside this window, so its slow
                            // progress is the safeguard's doing rather than the
                            // problem's. Discard it and measure a clean one.
                            //
                            // A verdict ALREADY DECLARED on a clean window is
                            // NOT withdrawn: it was established on undamped
                            // steps, and only genuine progress -- the re-arm
                            // branch above -- retracts it. (Measured: withdrawing
                            // it as well loses slow_infeasible_qp's diagnosis
                            // entirely, which is the fixture that discriminates
                            // the two telemetry routes.)
                            window_len = 0;
                            window_damped = false;
                        } else if (!stalled) {
                            stalled = true;
                            stall_len = window_len;
                        }
                    }
                }
                // --- R4: THE SYMPTOMS ARM, THE CERTIFICATE FIRES -----------
                //
                // Under the shipped kSymptoms rule the two conjuncts below ARE
                // the exit test and `standing_fires` is exactly the shipped
                // condition. Under kFarkasGated they are only the ARMING
                // condition: the accumulated dual direction over this window,
                // projected onto the sign cone and normalized, must
                // additionally be an approximate Farkas direction. A stalled,
                // dual-growing but FEASIBLE QP has no such direction
                // (Hoffman-bound territory), which is exactly the
                // discrimination the symptom pair cannot make.
                bool standing_fires = stalled && dn >= dual_window * detail::kSsnDualGrowthFactor;
                double farkas_resid = 0.0;
                double farkas_gap = 0.0;
                if (standing_fires && farkas_rule &&
                    !farkas_certificate(qp, out->lambda_e - wref_le, out->lambda_i - wref_li,
                                        lambda_b - wref_lb, mb, &farkas_resid, &farkas_gap)) {
                    standing_fires = false;
                    ++out->farkas_refusals;
                }
                if (standing_fires) {
                    out->farkas_fired += farkas_rule ? 1 : 0;
                    out->status = QpStatus::kInfeasible;
                    out->escape_reason = SsnEscape::kInfeasibleSuspect;
                    out->escape_detail = fmt::format(
                        "SsnEngine: infeasibility SUSPECTED (not certified) -- ||F||inf is {} "
                        "after {} accepted steps that did not improve on {} by the demanded "
                        "factor {}, while the multiplier norm grew from {} to {} over the same "
                        "steps, a factor {} above the {}x threshold. That pairing is what an "
                        "infeasible QP produces: phi(s, lambda) -> s as lambda -> +inf on a row "
                        "whose slack cannot be made non-negative, so the FB rows can only shrink "
                        "by pushing lambda up forever",
                        nrm.inf_norm, stall_len, window_ref, detail::kSsnStallImproveFactor,
                        dual_window, dn, dn / dual_window, detail::kSsnDualGrowthFactor);
                    if (farkas_rule) {
                        // R4: the certificate's own two numbers, carried out
                        // rather than discarded (tycho rule T6). Under the
                        // shipped rule there is no certificate and nothing is
                        // appended, so the message is byte-identical there.
                        out->escape_detail += fmt::format(
                            ". THE FARKAS CERTIFICATE CONFIRMED IT: the normalized dual "
                            "increment over this window, projected onto the sign cone, has "
                            "relative residual ||A^T y||inf = {} (threshold {}) and relative "
                            "Farkas objective <b, y> = {} (threshold {})",
                            farkas_resid, detail::kSsnFarkasResidualTol, farkas_gap,
                            -detail::kSsnFarkasGapTol);
                    }
                    break;
                }
            }

            if (!verifying && it >= sopts.hard_budget) {
                out->status = QpStatus::kMaxIter;
                out->escape_reason = SsnEscape::kBudget;
                break;
            }

            // --- soft budget: arm the proximal term ---------------------
            //
            // R1 NOTE. The guard reads `ladder_sigma_`, which IS `prox_sigma_`
            // under the shipped kLadder rule (the two are kept equal there by
            // construction) and is the LADDER's own state under the residual
            // rules -- so a solve whose sigma is nonzero only because the
            // residual sizing put it there still arms the ladder at
            // soft_budget, and `ssn_prox_updates` keeps meaning "the ladder
            // armed" in every configuration.
            if (guarded && !verifying && it >= sopts.soft_budget && ladder_sigma_ <= 0.0) {
                ladder_sigma_ = detail::kSsnProxInit;
                apply_sigma(qp, n, me, mi, mb);
                ++out->counters.ssn_prox_updates;
                window_damped = true;
            }

            // --- R1: THE RESIDUAL-DRIVEN (LEVENBERG-MARQUARDT) SIZING ---
            //
            // Structurally skipped under the shipped kLadder rule. Under the
            // two residual rules sigma is re-sized from the CURRENT residual
            // every attempt, with the monotone ladder underneath it as a floor
            // -- so every escalation trigger still climbs a rung and every
            // escape route is exactly the shipped one, while between triggers
            // the shift tracks the evidence instead of the history.
            //
            // **window_damped IS SET ONLY ON AN INCREASE**, and that is not a
            // convenience: the flag means "slow progress here is the
            // safeguard's doing, so do not read it as a stall". A sigma that
            // FALLS damps less than the step before it, so a window that
            // spans a fall is not contaminated -- and marking every attempt
            // damped would disable the standing infeasibility route outright
            // under a rule that re-sizes every attempt.
            if (lm_sigma_rule && !verifying &&
                (sopts.sigma_rule == SsnSigmaRule::kResidualAlways || ladder_sigma_ > 0.0)) {
                const double r = nrm.inf_norm / f_scale;
                lm_sigma_ = std::min(
                    std::max(detail::kSsnLmSigmaC * std::min(r, r * r), detail::kSsnProxInit),
                    detail::kSsnProxMax);
                // apply_sigma() is the ONE site that combines the ladder floor
                // with the residual size. Recomputing the max here as well
                // would make the floor UNFALSIFIABLE -- a mutant that deleted
                // it from apply_sigma() SURVIVED on the strength of such a
                // copy, which is what the mutation sweep found and this
                // removed.
                //
                // **AND IT IS CALLED UNCONDITIONALLY, WHICH COSTS AN O(nnz)
                // VALUE REBUILD PER ARMED ATTEMPT** (Phase-B review, minor 8).
                // apply_sigma() -> set_prox_sigma() -> sync_matrix() re-emits
                // K's VALUES even when the combined sigma did not move: no
                // factorization, no symbolic analysis, no counter and no
                // trajectory effect (verified -- sync_matrix takes its REFRESH
                // path, which writes values only), but real work. Guarding the
                // call on a precomputed max would put the floor back at two
                // sites and undo the paragraph above, so THE TRADE IS
                // DELIBERATE and the cheap fix is a different one: have
                // set_prox_sigma() early-out when `sigma == prox_sigma_`, where
                // the floor is not involved at all. Carried as a Phase-8
                // micro-item; it is on the LEVER path only (`lm_sigma_rule`
                // is false at the shipped default), so nothing shipped pays it.
                const double before = prox_sigma_;
                apply_sigma(qp, n, me, mi, mb);
                if (prox_sigma_ != before) {
                    ++out->counters.ssn_prox_updates;
                    window_damped = window_damped || prox_sigma_ > before;
                }
            }

            // --- generalized Jacobian element, per FB row ---------------
            const bool use_hint = (it == 0) && !start.activity_hint.empty();

            // --- R2: THE WATCHDOG'S JUDGEMENT -------------------------------
            //
            // Structurally skipped under the shipped kIterationZeroFree rule.
            //
            // IT RUNS BEFORE THE FACTORIZATION, deliberately: the merit at the
            // current iterate is all the watchdog needs, so a return-to-best
            // costs NO factorization -- it re-enters the loop at the stored
            // point and lets the ordinary machinery build the step there. A
            // watchdog that judged after the solve would pay for a direction it
            // then threw away, which is not the published rule's cost either.
            //
            // Three outcomes, in the order the rule states them:
            //   * the relaxed window has PAID OFF (Armijo decrease against the
            //     watchdog's own reference) -- close the window, everything
            //     from here is monotone;
            //   * the window has budget left -- take another relaxed step;
            //   * the window is exhausted without decrease -- RETURN TO THE
            //     BEST STORED POINT and close the window, so the next pass is
            //     an ordinary monotone Armijo step from the best point seen.
            bool wd_relaxed = false;
            if (watchdog_rule && !verifying) {
                if (!wd_active && it == 0 && use_hint) {
                    wd_active = true;
                    wd_used = 0;
                    wd_ref_merit = nrm.merit;
                    wd_best_merit = nrm.merit;
                    wd_best_x = out->x;
                    wd_best_le = out->lambda_e;
                    wd_best_li = out->lambda_i;
                    wd_best_lb = lambda_b;
                }
                if (wd_active) {
                    if (nrm.merit < wd_best_merit) {
                        wd_best_merit = nrm.merit;
                        wd_best_x = out->x;
                        wd_best_le = out->lambda_e;
                        wd_best_li = out->lambda_i;
                        wd_best_lb = lambda_b;
                    }
                    if (wd_used > 0 &&
                        nrm.merit <= (1.0 - 2.0 * detail::kSsnArmijoSigma) * wd_ref_merit) {
                        wd_active = false;
                    } else if (wd_used >= sopts.watchdog_q) {
                        wd_active = false;
                        ++out->watchdog_returns;
                        out->x = wd_best_x;
                        out->lambda_e = wd_best_le;
                        out->lambda_i = wd_best_li;
                        lambda_b = wd_best_lb;
                        continue; // re-enter at the best point, monotone from here
                    } else {
                        wd_relaxed = true;
                        ++wd_used;
                    }
                }
            }
            for (Index k = 0; k < mi; ++k) {
                const bool hinted = use_hint && !start.activity_hint.ineq.empty();
                select_branch(slack_i(k), out->lambda_i(k), phi_i(k), hinted,
                              hinted && start.activity_hint.ineq[static_cast<std::size_t>(k)],
                              guarded, sopts.uncertain_tol, alpha, beta, row_resid, klass, k);
            }
            for (Index r = 0; r < mb; ++r) {
                const detail::SsnBoundRow &br = bound_rows_[static_cast<std::size_t>(r)];
                const bool hinted = use_hint && !start.activity_hint.bounds.empty();
                select_branch(slack_b(r), lambda_b(r), phi_b(r), hinted,
                              hinted && bound_hint_active(start.activity_hint.bounds, br), guarded,
                              sopts.uncertain_tol, alpha, beta, row_resid, klass, mi + r);
            }
            any_klass = true;

            // **THE VERIFICATION ATTEMPT MOVES NO COUNTER BUT factorizations**
            // (section 7b). It is not a step, so it cannot be a bulk flip, and
            // its classification is not a partition the iteration ever acted
            // on, so it cannot be an uncertain-set peak. It DOES leave `klass`
            // describing the returned point, which is what the uncertain export
            // then reports -- a strict improvement on the previous "the last
            // assembly, which was one iterate back".
            if (!verifying) {
                if (!prev_klass.empty() && klass != prev_klass) {
                    ++out->counters.ssn_bulk_flips;
                }
                prev_klass = klass;
                Index unc = 0;
                for (const detail::SsnRowClass c : klass) {
                    unc += (c == detail::SsnRowClass::kUncertain) ? 1 : 0;
                }
                out->counters.ssn_uncertain_peak = std::max(out->counters.ssn_uncertain_peak, unc);
            }

            // --- diagonal refresh: the ONLY thing that changes in K -----
            double *vals = k_.valuePtr();
            const SpMatRM::StorageIndex *outer = k_.outerIndexPtr();
            for (Index k = 0; k < mi + mb; ++k) {
                const std::size_t kk = static_cast<std::size_t>(k);
                const double alpha_f = std::max(alpha[kk], detail::kSsnAlphaFloor);
                vals[outer[n + me + k]] = -(beta[kk] / alpha_f + eff_mu_);
            }

            // --- R5: THE DEFERRED CERTIFICATION EXIT --------------------
            //
            // Everything the verification attempt needs has now been done to K
            // EXCEPT the factorization: the rows are classified at the
            // converged point, sigma has been dropped to the caller's own
            // regularization, and the diagonals hold the values the shipped
            // verification would have factorized. Leaving here rather than
            // falling through is what makes the deferred factorization
            // IDENTICAL to the shipped one rather than merely similar -- the
            // caller's `finish_deferred_certification()` factorizes THIS
            // matrix, unmodified.
            //
            // prox_sigma_ is restored to the ladder's own value for reporting
            // exactly as the two in-loop verification exits below restore it;
            // K stays at the dropped sigma, which is the matrix the pending
            // verdict is about.
            if (verifying && sopts.defer_certification) {
                deferred_pending_ = true;
                deferred_pos_ = n;
                deferred_neg_ = me + mi + mb;
                deferred_attempt_ = it;
                deferred_fb_residual_ = nrm.inf_norm;
                deferred_fb_tol_ = sopts.fb_tol;
                prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                out->status = QpStatus::kOptimal;
                out->escape_reason = SsnEscape::kNone;
                out->certification_deferred = true;
                break;
            }

            // --- right-hand side ---------------------------------------
            //
            // Skipped on a verification attempt: it takes no step, so the only
            // thing it wants out of K is the inertia.
            if (!verifying) {
                rhs.head(n) = -resid_x;
                rhs.segment(n, me) = -resid_e;
                for (Index k = 0; k < mi + mb; ++k) {
                    const std::size_t kk = static_cast<std::size_t>(k);
                    const double alpha_f = std::max(alpha[kk], detail::kSsnAlphaFloor);
                    rhs(n + me + k) = row_resid[kk] / alpha_f;
                }
            }

            // --- factor and step ---------------------------------------
            Vec dw;
            try {
                const detail::AnalysisDecision analysis = detail::analysis_decision(kkt_, k_);
                if (analysis.needed) {
                    ++out->symbolic_analyses;
                }
                detail::factorize_checked(kkt_, k_, analysis);
                ++out->factorizations;
                if (!verifying) {
                    dw = detail::solve_vec(kkt_, rhs);
                }
            } catch (const std::exception &e) {
                out->status = QpStatus::kNumericalError;
                out->escape_reason = SsnEscape::kSingular;
                // N4 (final branch review WAVE #8): a throw during a
                // VERIFICATION factorization (7b) must restore prox_sigma_ to
                // the ladder's own value before falling through to the
                // result assembly below, exactly as the two in-loop
                // verification exits do -- otherwise SsnResult::prox_sigma
                // reports 0.0 (the dropped verification value) rather than
                // what the ladder was actually carrying. escape_detail also
                // states that the point was already first-order KKT and the
                // failure was in CERTIFYING it, not in stepping, since the
                // raw backend message alone loses that distinction.
                if (verifying) {
                    prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                    out->escape_detail = fmt::format(
                        "SsnEngine: the point reached at attempt {} satisfies the FB residual "
                        "test (||F||inf = {} <= {}), so it IS a first-order KKT point -- but the "
                        "second-order VERIFICATION factorization (header section 7b) threw "
                        "rather than certifying it: {}",
                        it, nrm.inf_norm, sopts.fb_tol, e.what());
                } else {
                    out->escape_detail = e.what();
                }
                break;
            }

            // --- the inertia gate (Task 4) ------------------------------
            //
            // K = [H + (delta+sigma) I, A^T; A, -D] with D > 0 has
            //
            //     In(K) = (0, m) + In(H + (delta+sigma) I + A^T D^{-1} A),
            //
            // so its inertia is (n, me+mi+mb) IFF that AUGMENTED block -- the
            // Schur-reduced one, NOT H and not H + (delta+sigma) I alone -- is
            // positive definite (Haynsworth's additivity; reference audit item
            // 2, aligning this comment with what the note's section 6 already
            // states and this code already does). The distinction is the whole
            // reason the gate certifies a SECOND-ORDER condition ON THE ACTIVE
            // FACE rather than global convexity: at indefinite_qp's true
            // solution the active bound's own D -> mu supplies a D^{-1} penalty
            // of order 1e8 in the concave direction, which is why the verdict
            // is kOk there and kWrong at the interior saddle. So kOk is not a
            // hope on a convex subproblem, it is a theorem (H PSD makes the
            // augmented block PSD + PSD), and the gate is INERT on every convex
            // fixture rather than merely observed to be. kWrong therefore means
            // exactly one thing: the augmented block is not positive definite,
            // so the step is
            // toward a KKT point that may be a saddle or a maximizer of the QP
            // -- which is precisely what the walk's own inertia ladder exists
            // to prevent (qp_engine.h section 4b), and this responds the same
            // way, by regularizing until it is not.
            //
            // **kSuspect DOES NOT ACT**, matching qp_engine.h exactly ("a
            // kSuspect verdict is deliberately NOT a repair trigger"): a
            // factorization whose pivots were perturbed reports counts that
            // carry no information, so acting on them would be acting on
            // fabrication. It is recorded and surfaces only if the solve later
            // fails for another reason.
            if (guarded) {
                // Evidence captured ONCE and consumed by verdict and
                // diagnostics alike, so a message can never describe a
                // different factorization than the verdict judged
                // (docs/retarget-design-sqp.md SS4.2).
                const hven::linear::InertiaEvidence ie = kkt_.factor.inertia();
                const detail::SsnInertia verdict = detail::ssn_inertia_verdict(ie, n, me + mi + mb);
                if (verdict == detail::SsnInertia::kWrong) {
                    if (verifying) {
                        // **THE POINT IS FIRST-ORDER KKT AND IS NOT A
                        // MINIMIZER** (section 7b). No escalation: F is already
                        // inside fb_tol here, so every rung produces the same
                        // (zero) step and would only spend factorizations
                        // reaching the same verdict.
                        //
                        // **AND BOTH VERIFICATION OUTCOMES MUST BREAK**, which
                        // is what makes the sigma drop above safe rather than
                        // merely tidy: a variant that fell through to the
                        // escalate-and-retry path below would escalate FROM the
                        // sigma the verification just dropped to 0, re-enter the
                        // convergence test at the same point, and never
                        // terminate. Found by scoring mutation F1b in the fix
                        // round's sweep, which is killed by a test TIMEOUT.
                        prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                        out->status = QpStatus::kNumericalError;
                        out->escape_reason = SsnEscape::kIndefinite;
                        out->escape_detail = fmt::format(
                            "SsnEngine: the point reached at attempt {} satisfies the FB "
                            "residual test (||F||inf = {} <= {}), so it IS a first-order KKT "
                            "point -- but the KKT factorization THERE reports inertia ({}, {}) "
                            "against the ({}, {}) a positive-definite primal block requires. "
                            "The point is a saddle or a maximizer of this QP, which no "
                            "residual-based test can see, so it is NOT certified",
                            it, nrm.inf_norm, sopts.fb_tol, ie.n_pos, ie.n_neg, n, me + mi + mb);
                        break;
                    }
                    if (escalate_prox(qp, n, me, mi, mb)) {
                        ++out->counters.ssn_prox_updates;
                        window_damped = true;
                        continue; // same iterate, stiffer Jacobian
                    }
                    out->status = QpStatus::kNumericalError;
                    out->escape_reason = SsnEscape::kSingular;
                    out->escape_detail = fmt::format(
                        "SsnEngine: the KKT factorization at attempt {} reports inertia ({}, "
                        "{}) against the ({}, {}) a positive-definite primal block requires, "
                        "and the proximal ladder is at its ceiling (sigma = {}). The Newton "
                        "step is toward a KKT point that need not be a minimizer",
                        it, ie.n_pos, ie.n_neg, n, me + mi + mb, prox_sigma_);
                    break;
                }
            }

            // The verification's verdict was kOk or kSuspect -- the only two
            // that reach here -- so the certificate is issued (section 7b).
            if (verifying) {
                prox_sigma_ = verify_sigma >= 0.0 ? verify_sigma : prox_sigma_;
                out->status = QpStatus::kOptimal;
                out->escape_reason = SsnEscape::kNone;
                break;
            }

            if (!dw.allFinite()) {
                out->status = QpStatus::kNumericalError;
                out->escape_reason = SsnEscape::kSingular;
                out->escape_detail = fmt::format(
                    "SsnEngine: Newton step {} has a non-finite entry -- either the KKT "
                    "matrix is numerically singular at this iterate or the residual fed "
                    "into it was not finite (||F||inf = {})",
                    it, nrm.inf_norm);
                break;
            }

            // --- globalization: the Armijo line search ------------------
            //
            // **ITERATION 0 IS EXEMPT WHEN A HINT GOVERNED IT**, and this is
            // the single most load-bearing line in the safeguard set (Fable
            // kernel review, O5). A wrongly hinted first step can RAISE the
            // residual -- measured 1.0 -> 5.0 on this file's own two-row
            // fixture -- because the hint is a PDAS step, not an FB Newton
            // step, and nothing promises it descends the FB merit. A monotone
            // Armijo rule rejects it, backtracks to a floor, and the solve
            // escapes; with the exemption it converges. A CORRECT hint's step
            // lands at ||F|| ~ 0 and would pass the test anyway, so the
            // exemption gives up no protection on the path it is protecting --
            // tests/test_ssn_engine.cpp runs BOTH polarities through it.
            //
            // The exemption is as narrow as it can be: iteration 0 AND a hint
            // supplied. An unhinted first step is an ordinary FB Newton step
            // and is line-searched like every other one.
            //
            // R2 REPLACES THE CONDITION, NOT THE MECHANISM. Under the shipped
            // kIterationZeroFree rule `wd_relaxed` is structurally false and
            // `(it == 0 && use_hint)` is the exemption verbatim. Under
            // kWatchdog the exemption is switched OFF and `wd_relaxed` -- the
            // watchdog's own verdict, taken above -- is what grants a relaxed
            // step, for up to q steps and with a stored best point behind it.
            double step = 1.0;
            bool accepted = false;
            if (!guarded || (watchdog_rule ? wd_relaxed : (it == 0 && use_hint))) {
                accepted = true;
                trial_point(out->x, out->lambda_e, out->lambda_i, lambda_b, dw, 1.0, guarded, n, me,
                            mi, mb, t_x, t_le, t_li, t_lb);
            } else {
                const double m0 = nrm.merit;
                for (;;) {
                    trial_point(out->x, out->lambda_e, out->lambda_i, lambda_b, dw, step, guarded,
                                n, me, mi, mb, t_x, t_le, t_li, t_lb);
                    const detail::SsnNorms tn = residual(qp, t_x, t_le, t_li, t_lb, resid_x,
                                                         resid_e, slack_i, slack_b, phi_i, phi_b);
                    if (std::isfinite(tn.merit) &&
                        tn.merit <= (1.0 - 2.0 * detail::kSsnArmijoSigma * step) * m0) {
                        accepted = true;
                        break;
                    }
                    if (step * detail::kSsnBacktrackFactor < detail::kSsnMinStep) {
                        break;
                    }
                    step *= detail::kSsnBacktrackFactor;
                    ++out->counters.ssn_backtracks;
                }
            }

            if (!accepted) {
                // **THE DIAGNOSIS OUTRANKS THE REPAIR.** An exhausted Armijo
                // schedule is proof that the direction produces no descent at
                // all -- a strictly stronger "no progress" statement than the
                // stall window's five tepid steps -- so when the DIVERGENCE
                // half of the telemetry is already satisfied, this is the
                // infeasible picture and no amount of damping will change it.
                // Escalating first would still reach the same verdict (the
                // stall window fills while the iterate sits still), but only by
                // accident: the diagnosis would then depend on how many rungs
                // the ladder happened to have left, which means on soft_budget
                // and on whatever the inertia gate already spent. Measured on
                // the infeasible fixture: this reports at 8 attempts / 9
                // factorizations where the accidental route took 12 / 12.
                // **THE EXHAUSTION ROUTE'S SECOND CONJUNCT IS A PER-STEP ONE**
                // (fix round 1, I2 / F2). "The duals are 1e4x the start point"
                // alone is satisfied permanently by a feasible QP whose true
                // multipliers merely exceed 1e4, and this route fires BEFORE
                // the ladder gets its chance -- so it also demands that the
                // duals are still MOVING, by an order of magnitude across the
                // most recently accepted step. Converging multipliers cannot
                // do that; diverging ones do it every step.
                //
                // R4 GATES THIS ROUTE THE SAME WAY IT GATES THE STANDING ONE,
                // and on the increment this route's own growth conjunct is
                // measured over -- the MOST RECENT ACCEPTED STEP. Under the
                // shipped kSymptoms rule `exhaustion_fires` is exactly the
                // shipped conjunction.
                bool exhaustion_fires = guarded &&
                                        dn >= dual_start * detail::kSsnDualGrowthFactor &&
                                        dn >= dual_prev * detail::kSsnDualStepGrowth;
                double ex_resid = 0.0;
                double ex_gap = 0.0;
                if (exhaustion_fires && farkas_rule &&
                    !farkas_certificate(qp, out->lambda_e - prev_le, out->lambda_i - prev_li,
                                        lambda_b - prev_lb, mb, &ex_resid, &ex_gap)) {
                    exhaustion_fires = false;
                    ++out->farkas_refusals;
                }
                if (exhaustion_fires) {
                    out->farkas_fired += farkas_rule ? 1 : 0;
                    out->status = QpStatus::kInfeasible;
                    out->escape_reason = SsnEscape::kInfeasibleSuspect;
                    out->escape_detail = fmt::format(
                        "SsnEngine: infeasibility SUSPECTED (not certified) -- the Armijo "
                        "schedule on 1/2||F||^2 found no descent at all down to step {} at "
                        "attempt {} (merit {}), while the multiplier norm has grown from {} "
                        "to {} (a factor {} above the {}x threshold) and multiplied by {} on "
                        "the most recent accepted step alone (threshold {}x). A direction that "
                        "produces no descent while the duals are still diverging is the "
                        "signature of a QP with no KKT point to converge to",
                        step, it, nrm.merit, dual_start, dn, dn / dual_start,
                        detail::kSsnDualGrowthFactor, dn / dual_prev, detail::kSsnDualStepGrowth);
                    if (farkas_rule) {
                        out->escape_detail += fmt::format(
                            ". THE FARKAS CERTIFICATE CONFIRMED IT: the normalized dual "
                            "increment across the most recent accepted step, projected onto the "
                            "sign cone, has relative residual ||A^T y||inf = {} (threshold {}) "
                            "and relative Farkas objective <b, y> = {} (threshold {})",
                            ex_resid, detail::kSsnFarkasResidualTol, ex_gap,
                            -detail::kSsnFarkasGapTol);
                    }
                    break;
                }
                if (escalate_prox(qp, n, me, mi, mb)) {
                    ++out->counters.ssn_prox_updates;
                    window_damped = true;
                    continue; // same iterate, stiffer Jacobian, fresh schedule
                }
                out->status = QpStatus::kNumericalError;
                out->escape_reason = SsnEscape::kNoContraction;
                out->escape_detail = fmt::format(
                    "SsnEngine: the Armijo schedule on 1/2||F||^2 ran down to step {} (floor "
                    "{}) at attempt {} without sufficient decrease from merit {}, and the "
                    "proximal ladder is at its ceiling (sigma = {}). The Newton direction is "
                    "not a descent direction for the FB merit here and damping did not make "
                    "it one",
                    step, detail::kSsnMinStep, it, nrm.merit, prox_sigma_);
                break;
            }

            out->x = t_x;
            out->lambda_e = t_le;
            out->lambda_i = t_li;
            lambda_b = t_lb;
            ++out->iters;
        }

        out->counters.ssn_iters = out->iters;
        if (out->escape_reason != SsnEscape::kNone) {
            ++out->counters.ssn_escapes;
            // PHASE-7 TASK 6b -- THE ESCAPE-REASON CENSUS (docket D6). Written
            // HERE, beside the total it partitions, so the two can never drift:
            // one `switch` over the same value the line above tested, and
            // NO `default` -- so a new SsnEscape value raises -Wswitch here
            // (the same discipline every other exhaustive switch in this
            // repository uses) rather than being silently uncounted. See
            // SqpCounters::ssn (sqp_types.h) for what the six fields are for.
            switch (out->escape_reason) {
            case SsnEscape::kBudget:
                ++out->counters.ssn_escape_budget;
                break;
            case SsnEscape::kSingular:
                ++out->counters.ssn_escape_singular;
                break;
            case SsnEscape::kNoContraction:
                ++out->counters.ssn_escape_no_contraction;
                break;
            case SsnEscape::kInfeasibleSuspect:
                ++out->counters.ssn_escape_infeasible_suspect;
                break;
            case SsnEscape::kIndefinite:
                ++out->counters.ssn_escape_indefinite;
                break;
            case SsnEscape::kNone:
                break; // unreachable: guarded by the enclosing test
            }
        }
        out->prox_sigma = prox_sigma_;
        out->z = recombine_bound_multipliers(lambda_b, n);
        // The activity export reads slack_i/slack_b, which must describe the
        // returned point. An earlier form could rely on the loop for that (every
        // exit broke immediately after residual()); this one cannot, because the
        // line search evaluates trial points into these same blocks -- see the
        // scratch declaration for why it does and what that buys. One
        // unconditional re-evaluation here is the whole cost, and it makes the
        // export's precondition a local fact rather than an invariant spread
        // over six exit routes. `out->fb_residual` is deliberately not
        // overwritten: it is the norm the exit decision was taken on, and
        // recomputing it at the same point can only reproduce it anyway.
        residual(qp, out->x, out->lambda_e, out->lambda_i, lambda_b, resid_x, resid_e, slack_i,
                 slack_b, phi_i, phi_b);
        export_activity(out, slack_i, slack_b, lambda_b, n, mi);
        export_uncertain(out, any_klass ? &klass : nullptr, n, mi);
    }

    // =====================================================================
    // R5 -- THE DEFERRED CERTIFICATION'S TWO CLOSING MOVES
    // =====================================================================
    //
    // Both are no-ops on any engine that never ran with
    // SsnOptions::defer_certification, and both throw if called when nothing is
    // pending -- a caller that closes a deferral twice, or closes one it never
    // opened, has lost track of which point it is certifying, and that is the
    // failure this pair exists to make loud.
    //
    // finish_deferred_certification() PAYS THE FACTORIZATION THE SOLVE DID NOT.
    // It factorizes the matrix the solve left in place -- classified at the
    // converged point, at the caller's own regularization -- and reads its
    // inertia, so the verdict it returns is EXACTLY the one section 7b's
    // in-loop verification would have returned, at exactly the same cost. On
    // kWrong it rewrites `out` into the SsnEscape::kIndefinite escape the
    // in-loop route issues, census included; on a thrown factorization it
    // rewrites `out` into SsnEscape::kSingular the same way. `out` MUST be the
    // SsnResult the deferring solve wrote, and the fields it touches are the
    // ones that solve would have touched.
    //
    // Returns true iff the certificate stands.
    bool finish_deferred_certification(SsnResult *out) {
        if (out == nullptr) {
            throw std::invalid_argument(
                "SsnEngine::finish_deferred_certification: out must not be null");
        }
        if (!deferred_pending_) {
            throw std::invalid_argument(
                "SsnEngine::finish_deferred_certification: no deferred certification is pending "
                "-- the last solve either did not set SsnOptions::defer_certification, did not "
                "reach a certifying exit, or has already been closed");
        }
        deferred_pending_ = false;
        try {
            const detail::AnalysisDecision analysis = detail::analysis_decision(kkt_, k_);
            if (analysis.needed) {
                ++out->symbolic_analyses;
            }
            detail::factorize_checked(kkt_, k_, analysis);
            ++out->factorizations;
        } catch (const std::exception &e) {
            out->status = QpStatus::kNumericalError;
            out->escape_reason = SsnEscape::kSingular;
            out->escape_detail = e.what();
            ++out->counters.ssn_escapes;
            ++out->counters.ssn_escape_singular;
            return false;
        }
        const hven::linear::InertiaEvidence ie = kkt_.factor.inertia();
        if (detail::ssn_inertia_verdict(ie, deferred_pos_, deferred_neg_) ==
            detail::SsnInertia::kWrong) {
            out->status = QpStatus::kNumericalError;
            out->escape_reason = SsnEscape::kIndefinite;
            out->escape_detail = fmt::format(
                "SsnEngine: the point reached at attempt {} satisfies the FB residual test "
                "(||F||inf = {} <= {}), so it IS a first-order KKT point -- but the KKT "
                "factorization THERE reports inertia ({}, {}) against the ({}, {}) a "
                "positive-definite primal block requires. The point is a saddle or a maximizer "
                "of this QP, which no residual-based test can see, so it is NOT certified "
                "(deferred verification, SsnOptions::defer_certification)",
                deferred_attempt_, deferred_fb_residual_, deferred_fb_tol_, ie.n_pos, ie.n_neg,
                deferred_pos_, deferred_neg_);
            ++out->counters.ssn_escapes;
            ++out->counters.ssn_escape_indefinite;
            return false;
        }
        return true;
    }

    // DROPS THE PENDING EVIDENCE UNREAD, and is legitimate on exactly one
    // path: the caller is discarding the exit anyway (this engine's own
    // trust-region usability gate refused it, so no certificate is being
    // claimed and no step is being taken from that point). Calling it while a
    // certificate IS being claimed is the wrong-answer class section 7b
    // closes -- which is why it is a separate, named call rather than a
    // default.
    void discard_deferred_certification() {
        if (!deferred_pending_) {
            throw std::invalid_argument(
                "SsnEngine::discard_deferred_certification: no deferred certification is "
                "pending");
        }
        deferred_pending_ = false;
    }

    // Whether this engine owes a caller one of the two calls above.
    bool has_deferred_certification() const { return deferred_pending_; }

  private:
    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    // qp_types.h's SolveOverrides precondition, applied unchanged: tr_radius must
    // be the +inf sentinel or >= 0 -- never negative (a negative Delta would
    // silently cross lo_eff and up_eff) and never NaN; primal_delta/dual_mu
    // keep the negative-means-sentinel convention but reject NaN, which no
    // downstream arithmetic can absorb.
    static void validate_overrides(const SolveOverrides &o) {
        if (std::isnan(o.tr_radius) || o.tr_radius < 0.0) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: overrides.tr_radius must be >= 0 (or the +inf "
                            "sentinel), got {}",
                            o.tr_radius));
        }
        if (std::isnan(o.primal_delta)) {
            throw std::invalid_argument("SsnEngine::solve: overrides.primal_delta must not be NaN");
        }
        if (std::isnan(o.dual_mu)) {
            throw std::invalid_argument("SsnEngine::solve: overrides.dual_mu must not be NaN");
        }
    }

    static void validate_options(const SsnOptions &s) {
        if (s.hard_budget < 0) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: hard_budget must be >= 0, got {}", s.hard_budget));
        }
        if (s.soft_budget < 0) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: soft_budget must be >= 0, got {}", s.soft_budget));
        }
        if (!(s.fb_tol > 0.0)) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: fb_tol must be > 0, got {}", s.fb_tol));
        }
        if (!(s.prox_sigma_init >= 0.0)) {
            throw std::invalid_argument(fmt::format(
                "SsnEngine::solve: prox_sigma_init must be >= 0, got {}", s.prox_sigma_init));
        }
        // The upper bound is 1 EXCLUSIVE and it is a real boundary, not a
        // tidiness one: |alpha - beta| = 1 at a strictly active or strictly
        // inactive row, so a threshold of 1 would classify the two PURE states
        // as uncertain and the partition would carry no information at all.
        if (!(s.uncertain_tol >= 0.0) || !(s.uncertain_tol < 1.0)) {
            throw std::invalid_argument(fmt::format(
                "SsnEngine::solve: uncertain_tol must be in [0, 1), got {}", s.uncertain_tol));
        }
        // R2. q = 0 would be a watchdog that grants no relaxed step at all,
        // i.e. a plain monotone rule wearing the name of a nonmonotone one --
        // a configuration whose measurement would be attributed to the wrong
        // mechanism. The shipped rule is reached by hint_rule, not by q = 0.
        if (s.hint_rule == SsnHintRule::kWatchdog && s.watchdog_q < 1) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: watchdog_q must be >= 1 under "
                            "SsnHintRule::kWatchdog (a watchdog with no relaxed step is a "
                            "monotone rule -- select SsnHintRule::kIterationZeroFree for that), "
                            "got {}",
                            s.watchdog_q));
        }
    }

    void validate_start(const QpProblem &qp, const SsnStart &start) const {
        check_size(start.x, qp.n(), "x");
        check_size(start.lambda_e, qp.me(), "lambda_e");
        check_size(start.lambda_i, qp.mi(), "lambda_i");
        check_size(start.z, qp.n(), "z");
        check_size(start.slacks, qp.mi(), "slacks");
        check_size(start.prox_center_x, qp.n(), "prox_center_x");
        check_size(start.prox_center_lambda, qp.me() + qp.mi(), "prox_center_lambda");
        const SsnActivityHint &h = start.activity_hint;
        if (!h.ineq.empty() && static_cast<Index>(h.ineq.size()) != qp.mi()) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: activity_hint.ineq has size {}, expected {} "
                            "(= qp.mi()) or 0",
                            h.ineq.size(), qp.mi()));
        }
        if (!h.bounds.empty() && static_cast<Index>(h.bounds.size()) != qp.n()) {
            throw std::invalid_argument(
                fmt::format("SsnEngine::solve: activity_hint.bounds has size {}, expected {} "
                            "(= qp.n()) or 0",
                            h.bounds.size(), qp.n()));
        }
    }

    static void check_size(const Vec &v, Index want, const char *what) {
        if (v.size() != 0 && v.size() != want) {
            throw std::invalid_argument(fmt::format(
                "SsnEngine::solve: start.{} has size {}, expected {} or 0", what, v.size(), want));
        }
    }

    static Vec seed_vector(const Vec &v, Index want, const char *) {
        return v.size() == want ? v : Vec::Zero(want);
    }

    // -----------------------------------------------------------------------
    // Bound rows
    // -----------------------------------------------------------------------
    // Builds the bound-row list from the EFFECTIVE bounds
    //
    //     lo_eff = max(lower, x0 - Delta),   up_eff = min(upper, x0 + Delta)
    //
    // -- qp_types.h's SolveOverrides::tr_radius contract for the walk, applied
    // here unchanged, including "about the SOLVE'S OWN start point x0" and
    // "computed once, at the top of solve()". A row is marked from_tr iff the
    // trust region is STRICTLY tighter than the real bound on that side (a tie
    // reads as the real bound, since the reported activity is then genuine).
    //
    // WITH A FINITE RADIUS EVERY VARIABLE GETS BOTH ROWS, whatever the radius
    // is, so the row list -- and therefore K's pattern -- is INVARIANT across
    // radius changes. That is what makes a shrink-retry loop free: only bound
    // VALUES move, and values are not structure. Delta = +inf reproduces the
    // real-bound list exactly, bit for bit (max(lower, -inf) is lower).
    void build_bound_rows(const QpProblem &qp, const Vec &x0, double tr_radius) {
        bound_rows_.clear();
        const Index n = qp.n();
        // The QP's OWN bounds, kept for the export's structurally-fixed test
        // (l == u), which must not see the trust region.
        real_lower_.assign(static_cast<std::size_t>(n), 0.0);
        real_upper_.assign(static_cast<std::size_t>(n), 0.0);
        for (Index j = 0; j < n; ++j) {
            real_lower_[static_cast<std::size_t>(j)] = qp.lower(j);
            real_upper_[static_cast<std::size_t>(j)] = qp.upper(j);
        }
        const bool has_tr = std::isfinite(tr_radius);
        for (Index j = 0; j < n; ++j) {
            double lo = qp.lower(j);
            double up = qp.upper(j);
            bool lo_from_tr = false;
            bool up_from_tr = false;
            if (has_tr) {
                const double tr_lo = x0(j) - tr_radius;
                const double tr_up = x0(j) + tr_radius;
                if (tr_lo > lo) {
                    lo = tr_lo;
                    lo_from_tr = true;
                }
                if (tr_up < up) {
                    up = tr_up;
                    up_from_tr = true;
                }
            }
            if (lo > -detail::kSsnInfBound) {
                bound_rows_.push_back(detail::SsnBoundRow{j, -1.0, -lo, lo_from_tr});
            }
            if (up < detail::kSsnInfBound) {
                bound_rows_.push_back(detail::SsnBoundRow{j, 1.0, up, up_from_tr});
            }
        }
    }

    // A kFixed hint marks BOTH of a variable's rows active, which is right for a
    // genuinely fixed variable (l == u) and is a DOCUMENTED DEGRADED MODE when
    // l < u (Fable kernel review, M-3).
    //
    // HOW IT ARISES, and why it is not hypothetical: export_activity reports
    // kFixed whenever both of a variable's bound rows come out implied-active,
    // which a noisy or ESCAPED iterate can produce on an l < u variable. That
    // export is exactly what Task 5 re-ingests as a warm-start hint, so the
    // kernel can feed itself this hint. The first step then solves the
    // contradictory pair {x_j = l_j, x_j = u_j}, which the dual-mu block
    // regularizes into the midpoint rather than a singular factorization.
    //
    // **THE CHOICE IS DOCUMENTED RECOVERY, NOT REJECTION**, and deliberately.
    // Rejecting would make a warm-start hand-off THROW because the previous
    // solve happened to stop somewhere noisy -- turning a recoverable
    // hand-off into a caller error on the one path the warm-start subsystem
    // exists to serve, and pushing every Task-5 caller into sanitising a hint
    // the kernel itself produced. A hint is ADVICE about the first step, not a
    // constraint: every later step re-derives the partition from the FB
    // branch, so a contradictory hint costs iterations and nothing else. It
    // recovers to the same point as the walk (measured: 7 iterations, agreeing
    // to 1e-7 -- pinned by tests/test_ssn_engine.cpp's
    // ContradictoryFixedHintRecovers), which is the cost of a bad hint and not
    // a wrong answer.
    //
    // The alternative of masking kFixed at EXPORT was also rejected: it would
    // suppress a real signal (an iterate whose two bound rows genuinely both
    // read active is worth seeing) and it cannot help a caller who builds the
    // hint from somewhere other than this engine.
    static bool bound_hint_active(const std::vector<BoundState> &hint,
                                  const detail::SsnBoundRow &br) {
        const BoundState st = hint[static_cast<std::size_t>(br.var)];
        if (st == BoundState::kFixed) {
            return true;
        }
        return br.sign < 0.0 ? st == BoundState::kAtLower : st == BoundState::kAtUpper;
    }

    // Splits the signed z into the two non-negative bound-row multipliers.
    //
    // **A SEEDED z THAT PRICES A BOUND THIS QP DOES NOT HAVE IS A CALLER ERROR
    // AND THROWS** (review fix round 1, M6). z(j) > 0 prices variable j's LOWER
    // bound and z(j) < 0 its UPPER one; if that side is absent (+/-kSsnInfBound
    // or beyond, so there is no row for the multiplier to live on) the mass has
    // nowhere to go, and the previous behaviour -- dropping it silently --
    // made the one malformed input this engine accepted quietly, against a
    // surface where every other one is rejected with a message. It is also a
    // real hazard rather than a tidiness point: warm_start.h's own note records
    // exactly this shape (a z priced against a 1e20 bound) poisoning a
    // downstream estimate, and a Task-5 ingest that hands one model's z to a
    // differently bounded model would produce it.
    //
    // THE TEST IS EXACT ZERO, deliberately, and not a tolerance: an absent side
    // has no multiplier at all, so the only correct value there is 0, and any
    // nonzero -- however small -- means the caller believes in a bound this QP
    // does not have. (The same exact-zero reasoning the elastic ladder's
    // exhaustion conjunct is ruled on.)
    //
    // **THE ABSENT-SIDE TEST READS THE REAL BOUND, NOT THE EFFECTIVE ONE**, and
    // a TR row is seeded at zero. A caller's z prices the QP's OWN bounds; the
    // trust region is this solve's private construction, so a finite radius
    // must not turn "you priced a bound that does not exist" into silence
    // (every variable has finite effective bounds under a finite radius, which
    // would make the check vacuous). TR duals are internal, exactly as
    // qp_problem.h's tr_active contract requires on the way out.
    Vec split_bound_multipliers(const QpProblem &qp, const Vec &z) const {
        const Index n = qp.n();
        for (Index j = 0; j < n; ++j) {
            const double zj = z(j);
            if (zj > 0.0 && !(qp.lower(j) > -detail::kSsnInfBound)) {
                throw std::invalid_argument(fmt::format(
                    "SsnEngine::solve: start.z({}) = {} prices variable {}'s LOWER bound, but "
                    "that bound is absent (lower = {}, at or beyond the +/-{} infinity "
                    "sentinel) -- there is no row for that multiplier",
                    j, zj, j, qp.lower(j), detail::kSsnInfBound));
            }
            if (zj < 0.0 && !(qp.upper(j) < detail::kSsnInfBound)) {
                throw std::invalid_argument(fmt::format(
                    "SsnEngine::solve: start.z({}) = {} prices variable {}'s UPPER bound, but "
                    "that bound is absent (upper = {}, at or beyond the +/-{} infinity "
                    "sentinel) -- there is no row for that multiplier",
                    j, zj, j, qp.upper(j), detail::kSsnInfBound));
            }
        }
        Vec lb = Vec::Zero(static_cast<Index>(bound_rows_.size()));
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            const detail::SsnBoundRow &br = bound_rows_[r];
            if (br.from_tr) {
                continue; // TR duals are internal; a caller's z never prices one
            }
            const double zj = z(br.var);
            lb(static_cast<Index>(r)) = br.sign < 0.0 ? std::max(zj, 0.0) : std::max(-zj, 0.0);
        }
        return lb;
    }

    // **TR ROWS ARE EXCLUDED**, which is qp_problem.h's contract verbatim: "TR
    // duals are internal to the ratio test/drop rule and are never exposed",
    // and z(i) is forced to 0 at a TR-pinned index. A caller reading z back
    // therefore sees only multipliers of the QP's own bounds, whatever radius
    // the solve ran under -- the property that lets a warm-start export survive
    // a shrink-retry loop without accumulating radius-dependent junk.
    Vec recombine_bound_multipliers(const Vec &lb, Index n) const {
        Vec z = Vec::Zero(n);
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            const detail::SsnBoundRow &br = bound_rows_[r];
            if (br.from_tr) {
                continue;
            }
            z(br.var) += (br.sign < 0.0 ? 1.0 : -1.0) * lb(static_cast<Index>(r));
        }
        return z;
    }

    // -----------------------------------------------------------------------
    // The fixed pattern, and its reuse across solves
    // -----------------------------------------------------------------------
    //
    // THE FB DIAGONALS GET A NONZERO PLACEHOLDER (-1.0) rather than their
    // eventual value, so their slots exist regardless of what any later branch
    // wants there: beta/alpha_f + mu is legitimately EXACTLY 0 on a strictly
    // active row when the caller zeroes dual_mu, and the diagonal refresh in
    // solve() addresses those slots positionally (K.outerIndexPtr()[row])
    // rather than by search.
    //
    // **THAT PLACEHOLDER IS A DEFENSIVE GUARANTEE, NOT A LOAD-BEARING ONE ON
    // TODAY'S EIGEN**, and the mutation sweep says so: replacing -1.0 with 0.0
    // leaves the whole suite green, because the vendored Eigen's
    // setFromTriplets preserves explicit zeros. It is kept because that is a
    // behaviour of an implementation, not of a documented contract -- Eigen's
    // own prune()/assignment paths do drop exact zeros -- and the cost of not
    // depending on it is one literal. Recorded as a knowingly surviving mutant
    // in the Task-3 report rather than left to look like an untested line.
    //
    // ---- THE REUSE (review fix round 1, M5) -------------------------------
    //
    // The first version rebuilt K from a triplet list on EVERY solve() call,
    // and the review was right that this is a real tax rather than a tidiness
    // point: setFromTriplets is O(nnz log nnz) and Task 6 runs this per
    // subproblem at n = 1e5-1e6, where it is the same order as the
    // factorization it precedes -- while the header's own section 3 claims the
    // pattern is reused "across QPs of identical structure", which was only
    // true of PARDISO'S symbolic analysis, not of this file's own derivation.
    //
    // So sync_matrix() now decides between two paths and reports which:
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
    // is the single walk both drive -- so they cannot drift apart, which is the
    // failure mode a hand-written second walk would have.
    //
    // ACCUMULATION, NOT ASSIGNMENT, on the refresh path: setFromTriplets SUMS
    // duplicates, and this file emits duplicates deliberately (H's diagonal and
    // the separate primal_delta entry land on the same slot). Zeroing first and
    // then += reproduces that summation exactly; a straight = would silently
    // drop primal_delta wherever H has a stored diagonal.
    //
    // THE STRUCTURE KEY is a COMPOSITE of two conjuncts (phase-C H3), read
    // from exactly the inputs build_pattern reads and nothing else:
    //
    //   1. structure_hash -- hven's combined pattern key (one Fnv1a threaded
    //      across the dimensions and the three input matrices' patterns by
    //      feed_pattern, docs/pattern-hash.md), the same FNV-1a instrument,
    //      with the same collision exposure, that the KKT factor helper's
    //      pattern compare already uses to decide whether to skip the
    //      symbolic analysis (kkt_calls.h / hven::pattern_hash).
    //   2. the bound-row (var, sign) list, compared EXACTLY against the copy
    //      cached when the current pattern was built (bound_rows_match_cached
    //      / structure_bound_key_) -- not hashed, so no collision exposure at
    //      all on this half.
    //
    // **THIS FILE USED TO CLAIM THE KEY WAS "deliberately NOT the only guard",
    // AND THAT WAS FALSE** (Fable kernel review, M-1). On a collision the
    // REFRESH path writes the new QP's values through the STALE value_pos_ map
    // and returns -- all before the factor is involved at all -- and
    // needs_analysis() then hashes k_'s own cached pattern, which of
    // course matches, so it cannot see the corruption. Its hash guards
    // Pardiso's symbolic reuse against a CHANGED K, which is a different
    // failure. Worse, a colliding structure that emits MORE entries would run
    // value_pos_[t++] off the end of the vector -- undefined behaviour, not a
    // wrong answer.
    //
    // So the second guard is now REAL rather than asserted: the refresh path
    // bounds-checks every write and then requires that the emission consumed
    // the map EXACTLY (t == value_pos_.size()). That catches any emission-count
    // drift, including every collision whose structures differ in entry count,
    // and it converts the UB into a thrown std::runtime_error. A collision
    // between two structures with identical entry counts remains undetected --
    // a 64-bit FNV coincidence, and the honest residual exposure. Under the H3
    // composite that exposure lives entirely in conjunct 1's hashed half (the
    // dimensions and the three matrix patterns); the bound-row half cannot
    // collide, having stopped being a hash.
    //
    // **THE GUARD IS NOT TEST-REACHABLE, AND A MUTATION REMOVING IT SURVIVES**
    // -- reaching it requires forging an FNV-1a collision, which no fixture can
    // do. Recorded here as a knowingly-unkillable defensive line, alongside the
    // FB diagonal's placeholder, rather than left looking like coverage that
    // was forgotten. (The structure key's sign term used to be listed here too;
    // under the H3 composite it is an exact comparison, hence killable and
    // PINNED -- SignFlipAtConstantBoundLayoutForcesRebuild -- so it no longer
    // belongs on this list. See bound_rows_match_cached() below.)
    template <typename Emit>
    void for_each_entry(const QpProblem &qp, Index n, Index me, Index mi, Index mb,
                        Emit emit) const {
        const Index eq_off = n;
        const Index ineq_off = n + me;
        const Index bnd_off = n + me + mi;

        // H is already upper-triangular (qp.validate() enforces row <= col).
        for (Index i = 0; i < n; ++i) {
            for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
                emit(i, it.col(), it.value());
            }
        }
        for (Index i = 0; i < n; ++i) {
            emit(i, i, eff_delta_);
        }
        for (Index r = 0; r < me; ++r) {
            for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
                emit(it.col(), eq_off + r, it.value());
            }
            emit(eq_off + r, eq_off + r, -eff_mu_);
        }
        for (Index k = 0; k < mi; ++k) {
            for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
                emit(it.col(), ineq_off + k, it.value());
            }
            emit(ineq_off + k, ineq_off + k, -1.0); // placeholder
        }
        for (Index r = 0; r < mb; ++r) {
            const detail::SsnBoundRow &br = bound_rows_[static_cast<std::size_t>(r)];
            emit(br.var, bnd_off + r, br.sign);
            emit(bnd_off + r, bnd_off + r, -1.0); // placeholder
        }
    }

    // CONJUNCT 1 of the composite structure key: hven's combined pattern key
    // over the dimensions and the three input patterns. Fed **through
    // feed_pattern, NOT off the raw index arrays**, and that carries the
    // property the old InnerIterator walk here existed for: qp.H/Ae/Ai are
    // CALLER-SUPPLIED and QpProblem imposes no compression requirement, and
    // feed_pattern's contract is that either storage state produces the
    // compressed digest -- same O(nnz), no compressed copy, exact in both
    // states. (The emission still walks these matrices with InnerIterator, and
    // feed_pattern's stream is defined by the same iteration, so the key and
    // the emission keep agreeing about what "the pattern" is.) Every
    // ingredient goes through Fnv1a::feed_index -- 64-bit widened, LSB-first
    // -- so the digest does not depend on host byte order or on
    // SpMatRM::StorageIndex's width. The bound-row list is DELIBERATELY not
    // here: it is conjunct 2 (bound_rows_match_cached below), compared
    // exactly rather than hashed. The digest's VALUE changed with the H3
    // re-key (declared in that commit); it is only ever compared against
    // another computation of this same function on this same engine.
    std::uint64_t structure_hash(const QpProblem &qp, Index n, Index me, Index mi, Index mb) const {
        Fnv1a h;
        h.feed_index(n);
        h.feed_index(me);
        h.feed_index(mi);
        h.feed_index(mb);
        feed_pattern(h, qp.H);
        feed_pattern(h, qp.Ae);
        feed_pattern(h, qp.Ai);
        return h.value();
    }

    // CONJUNCT 2: the bound-row (var, sign) list, compared EXACTLY against
    // the copy cached when the current pattern was built.
    //
    // **br.var IS THE LOAD-BEARING HALF, AND mb ALONE DOES NOT COVER IT**
    // (fix round 1: a mutation that dropped the bound rows from the old
    // in-hash key entirely SURVIVED the first sweep, because almost every
    // bound change also changes mb, which structure_hash mixes). The case mb
    // misses is a DIFFERENT ASSIGNMENT of the same NUMBER of bound rows to
    // variables -- two rows both on variable 0 versus one row each on
    // variables 0 and 1 -- which puts the bound block's off-diagonal entries
    // in different columns of K. Reusing a pattern across that would write
    // B's values into A's slots. PatternKeySeparatesBoundLayoutsOfEqualSize
    // is the fixture; as an exact comparison the check is now deterministic,
    // where the old in-hash form left it a 2^-64 coincidence away from
    // exactly that wrong-slot write.
    //
    // sign participates too, CONSERVATIVELY rather than necessarily: a sign
    // flip (a variable trading its lower bound for its upper) moves no slot,
    // only a value, and the refresh path re-emits br.sign -- so dropping sign
    // from the conjunct would still be correct, just as `+ mb` alone would
    // not be. It is kept because an over-conservative key costs one avoidable
    // rebuild in that one case and buys never having to re-derive this
    // argument -- and because rebuild COUNTS are pinned currency: the old key
    // rebuilt on a sign flip, so the composite must too.
    // SignFlipAtConstantBoundLayoutForcesRebuild is the fixture (the old
    // in-hash sign term was recorded as knowingly unkillable; the exact
    // comparison is killable, so it is pinned rather than recorded).
    //
    // rhs and from_tr are EXCLUDED, exactly as they were from the old hash:
    // both are value attributes of a row whose slot they do not move (see
    // SsnBoundRow), and comparing either would rebuild the pattern on bound
    // VALUE moves -- from_tr on every radius change of a shrink-retry loop,
    // which is exactly the tax this cache exists to remove. The cache is a
    // (var, sign) pair list, not a SsnBoundRow copy, so neither CAN be
    // compared by accident.
    bool bound_rows_match_cached() const {
        if (bound_rows_.size() != structure_bound_key_.size()) {
            return false;
        }
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            const detail::SsnBoundRow &br = bound_rows_[r];
            if (br.var != structure_bound_key_[r].first ||
                br.sign != structure_bound_key_[r].second) {
                return false;
            }
        }
        return true;
    }

    // Brings k_ up to date for `qp`. Returns true iff the pattern was rebuilt.
    bool sync_matrix(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
        dim_ = n + me + mi + mb;
        const std::uint64_t key = structure_hash(qp, n, me, mi, mb);

        if (has_structure_ && key == structure_key_ && bound_rows_match_cached() &&
            k_.rows() == dim_) {
            double *vals = k_.valuePtr();
            std::fill(vals, vals + k_.nonZeros(), 0.0);
            std::size_t t = 0;
            const std::size_t expected = value_pos_.size();
            for_each_entry(qp, n, me, mi, mb, [&](Index, Index, double v) {
                if (t >= expected) {
                    throw std::runtime_error(fmt::format(
                        "SsnEngine: structure-key collision detected -- the reused pattern "
                        "expects {} entries but this QP emits more; refusing to write past "
                        "the cached position map",
                        expected));
                }
                vals[value_pos_[t++]] += v;
            });
            if (t != expected) {
                throw std::runtime_error(fmt::format(
                    "SsnEngine: structure-key collision detected -- the reused pattern expects "
                    "{} entries but this QP emitted {}",
                    expected, t));
            }
            return false;
        }

        // Structure changed (or this is the first solve): full rebuild.
        has_structure_ = false;
        std::vector<Eigen::Triplet<double>> trips;
        trips.reserve(static_cast<std::size_t>(qp.H.nonZeros() + n + qp.Ae.nonZeros() + me +
                                               qp.Ai.nonZeros() + mi + 2 * mb));
        for_each_entry(qp, n, me, mi, mb,
                       [&trips](Index r, Index c, double v) { trips.emplace_back(r, c, v); });

        k_ = SpMatRM(dim_, dim_);
        k_.setFromTriplets(trips.begin(), trips.end());
        k_.makeCompressed();

        // Record where each emitted entry landed. Every row's inner indices are
        // sorted after makeCompressed(), so this is a binary search per entry.
        value_pos_.assign(trips.size(), 0);
        const SpMatRM::StorageIndex *outer = k_.outerIndexPtr();
        const SpMatRM::StorageIndex *inner = k_.innerIndexPtr();
        for (std::size_t t = 0; t < trips.size(); ++t) {
            const Index row = trips[t].row();
            const Index col = trips[t].col();
            const SpMatRM::StorageIndex *begin = inner + outer[row];
            const SpMatRM::StorageIndex *end = inner + outer[row + 1];
            const SpMatRM::StorageIndex *hit =
                std::lower_bound(begin, end, static_cast<SpMatRM::StorageIndex>(col));
            if (hit == end || *hit != static_cast<SpMatRM::StorageIndex>(col)) {
                // Unreachable for a matrix just built from these very triplets;
                // checked rather than asserted because a Release build compiles
                // an assert out entirely and a wrong position would corrupt K
                // silently on every later reuse.
                throw std::runtime_error(fmt::format(
                    "SsnEngine: internal error -- entry ({}, {}) is missing from the matrix "
                    "just assembled from it",
                    row, col));
            }
            value_pos_[t] = static_cast<std::size_t>(hit - inner);
        }

        structure_key_ = key;
        structure_bound_key_.clear();
        structure_bound_key_.reserve(bound_rows_.size());
        for (const detail::SsnBoundRow &br : bound_rows_) {
            structure_bound_key_.emplace_back(br.var, br.sign);
        }
        has_structure_ = true;
        return true;
    }

    // -----------------------------------------------------------------------
    // The activity export
    // -----------------------------------------------------------------------
    //
    // The engine's own partition rule, applied to the returned iterate: a row
    // is active iff its multiplier exceeds its slack (equivalently alpha >
    // beta, header section 2). Written, never read.
    void export_activity(SsnResult *out, const Vec &slack_i, const Vec &slack_b,
                         const Vec &lambda_b, Index n, Index mi) const {
        out->ineq_active.assign(static_cast<std::size_t>(mi), false);
        for (Index k = 0; k < mi; ++k) {
            out->ineq_active[static_cast<std::size_t>(k)] = out->lambda_i(k) > slack_i(k);
        }
        out->bound_state.assign(static_cast<std::size_t>(n), BoundState::kFree);
        out->tr_active.assign(static_cast<std::size_t>(n), false);
        out->tr_violation = 0.0;
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            const detail::SsnBoundRow &br = bound_rows_[r];
            const Index rr = static_cast<Index>(r);
            // The TR EXIT CONTRACT (fix round 1, I3), measured on the same
            // slacks the partition is read from: a TR row's slack is negative
            // exactly when the returned point is outside the radius on that
            // side, and by exactly how much.
            if (br.from_tr && slack_b(rr) < 0.0) {
                out->tr_violation = std::max(out->tr_violation, -slack_b(rr));
            }
            if (!(lambda_b(rr) > slack_b(rr))) {
                continue;
            }
            // **THE TR/REAL SPLIT, qp_problem.h's contract verbatim**: a
            // variable held by a TR-tight effective bound is reported in
            // tr_active and reports kFree in bound_state -- which is a
            // REAL-BOUND-ONLY view, so a caller cannot mistake a radius for a
            // constraint. Its z is 0 for the same reason (see
            // recombine_bound_multipliers). A driver detects a binding radius
            // by reading tr_active, never z or bound_state.
            if (br.from_tr) {
                out->tr_active[static_cast<std::size_t>(br.var)] = true;
                continue;
            }
            BoundState &st = out->bound_state[static_cast<std::size_t>(br.var)];
            const BoundState here = br.sign < 0.0 ? BoundState::kAtLower : BoundState::kAtUpper;
            st = (st == BoundState::kFree) ? here : BoundState::kFixed;
        }
        // A STRUCTURALLY FIXED VARIABLE (l == u) READS kFixed, whichever of its
        // two rows won the partition. Found while pinning the kFixed hint: the
        // partition rule alone reports kAtUpper there, because the two rows'
        // multipliers split as (0, 1.5) rather than both being positive -- a
        // correct reading of the partition, but NOT the convention the rest of
        // the project uses. kkt_assembly.h treats kFixed as a structural fact
        // about the variable (it substitutes qp.lower(i) for kAtLower AND
        // kFixed alike), and the walk reports kFixed here. An export that
        // disagreed with the walk on a whole variable class would be a trap for
        // Task 5's re-ingest, so the structural fact wins over the partition.
        // TR rows are excluded from the test, since a radius does not make a
        // variable fixed.
        for (Index j = 0; j < n; ++j) {
            const std::size_t jj = static_cast<std::size_t>(j);
            if (out->bound_state[jj] != BoundState::kFree && real_lower_[jj] == real_upper_[jj]) {
                out->bound_state[jj] = BoundState::kFixed;
            }
        }
    }

    // The uncertain export (Task 4). `klass` is null when no Jacobian was ever
    // assembled, which is the honest "no classification was made" case and is
    // reported as all-false rather than as all-decided.
    void export_uncertain(SsnResult *out, const std::vector<detail::SsnRowClass> *klass, Index n,
                          Index mi) const {
        out->ineq_uncertain.assign(static_cast<std::size_t>(mi), false);
        out->bound_uncertain.assign(static_cast<std::size_t>(n), false);
        if (klass == nullptr) {
            return;
        }
        for (Index k = 0; k < mi; ++k) {
            out->ineq_uncertain[static_cast<std::size_t>(k)] =
                (*klass)[static_cast<std::size_t>(k)] == detail::SsnRowClass::kUncertain;
        }
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            if ((*klass)[static_cast<std::size_t>(mi) + r] == detail::SsnRowClass::kUncertain) {
                out->bound_uncertain[static_cast<std::size_t>(bound_rows_[r].var)] = true;
            }
        }
    }

    // -----------------------------------------------------------------------
    // The globalization helpers (Task 4)
    // -----------------------------------------------------------------------

    // ||(lambda_e, lambda_i, lambda_b)||_inf -- the divergence telemetry's
    // growth measure. The EQUALITY multipliers are included even though they
    // are sign-free: an inconsistent equality block diverges exactly the same
    // way an inconsistent inequality block does, and excluding them would make
    // the test blind to the commonest infeasibility an SQP linearization
    // produces.
    static double dual_norm(const Vec &le, const Vec &li, const Vec &lb) {
        double m = 0.0;
        if (le.size() > 0) {
            m = std::max(m, le.cwiseAbs().maxCoeff());
        }
        if (li.size() > 0) {
            m = std::max(m, li.cwiseAbs().maxCoeff());
        }
        if (lb.size() > 0) {
            m = std::max(m, lb.cwiseAbs().maxCoeff());
        }
        return m;
    }

    // w + step * dw, with THE DUAL PROJECTION applied to the two non-negative
    // multiplier blocks.
    //
    // **THE PROJECTION IS THE WRONG-HINT MITIGATION** the kernel review
    // identified as the only cheap one available (O5/section 5 there; no
    // derivative re-selection can help, because the landing configuration
    // (s, lambda) = (0, lambda < 0) is a DIFFERENTIABLE point of phi -- its
    // subdifferential is the singleton {(1, 2)} -- so there is no
    // generalized-Jacobian freedom to exploit). A wrongly hinted active row
    // lands with a NEGATIVE multiplier and is then confined to the line
    // s+ + 2 lambda+ = 2 s0 - (2 s0/|lambda| + mu) d lambda, on which a
    // non-degenerate row's root does not lie; clipping lambda to 0 moves the
    // pair onto the kink, where the symmetric subdifferential element applies
    // and the confinement dissolves.
    //
    // It is a PROJECTION ONTO A CONVEX SET THAT CONTAINS EVERY SOLUTION
    // (lambda >= 0 is a KKT condition), so it cannot move the iterate away from
    // the solution set, and it is inert at any point that already satisfies it
    // -- which every converged point does. lambda_e is untouched: equality
    // multipliers are free-sign and clipping them would be a wrong answer, not
    // a safeguard.
    static void trial_point(const Vec &x, const Vec &le, const Vec &li, const Vec &lb,
                            const Vec &dw, double step, bool project, Index n, Index me, Index mi,
                            Index mb, Vec &t_x, Vec &t_le, Vec &t_li, Vec &t_lb) {
        t_x = x + step * dw.head(n);
        t_le = le + step * dw.segment(n, me);
        t_li = li + step * dw.segment(n + me, mi);
        t_lb = lb + step * dw.segment(n + me + mi, mb);
        if (project) {
            if (mi > 0) {
                t_li = t_li.cwiseMax(0.0);
            }
            if (mb > 0) {
                t_lb = t_lb.cwiseMax(0.0);
            }
        }
    }

    // Sets sigma and re-emits K's VALUES for it. The proximal increment lands
    // on the same two slots primal_delta and dual_mu already occupy, so the
    // pattern is untouched and sync_matrix takes its REFRESH path -- O(nnz), no
    // sort, no allocation, and Pardiso's cached symbolic analysis survives. The
    // return value is ignored deliberately: a refresh is not a pattern rebuild
    // and must not be counted as one.
    void set_prox_sigma(double sigma, const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
        prox_sigma_ = sigma;
        eff_delta_ = base_delta_ + sigma;
        eff_mu_ = base_mu_ + sigma;
        (void)sync_matrix(qp, n, me, mi, mb);
    }

    // One rung up the ladder. Returns false at the ceiling, which is the
    // caller's signal to escape rather than to keep paying factorizations.
    //
    // **THE CEILING TEST CARRIES A RELATIVE SLACK, AND IT IS NOT COSMETIC**
    // (review fix round 1, I1). The rungs are computed by repeated
    // multiplication, which does not land on the cap exactly: rung 7 evaluates
    // to 999999.9999999998, strictly below 1e6, so an exact `>= kSsnProxMax`
    // guard granted an EIGHTH rung that raised sigma by 2.3e-10 relative and
    // bought a full numeric factorization plus a full backtracking schedule for
    // it. kSsnProxCapSlack's derivation is at the constant.
    // R1: sigma = max(the ladder's monotone state, the residual-driven size).
    // Under the shipped kLadder rule `lm_sigma_` is identically 0, so this is
    // `set_prox_sigma(ladder_sigma_, ...)` and `prox_sigma_ == ladder_sigma_`
    // at every point the shipped iteration can observe.
    void apply_sigma(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
        set_prox_sigma(std::max(ladder_sigma_, lm_sigma_), qp, n, me, mi, mb);
    }

    bool escalate_prox(const QpProblem &qp, Index n, Index me, Index mi, Index mb) {
        if (ladder_sigma_ >= detail::kSsnProxMax * (1.0 - detail::kSsnProxCapSlack)) {
            return false;
        }
        double next = ladder_sigma_ <= 0.0
                          ? detail::kSsnProxInit
                          : std::min(ladder_sigma_ * detail::kSsnProxGrowth, detail::kSsnProxMax);
        // The TOP rung is the cap EXACTLY, for the same reason the guard above
        // carries a slack: repeated multiplication reaches 999999.9999999998,
        // and a ceiling a caller can read back (SsnResult::prox_sigma) should
        // be the documented 1e6 rather than the accumulated rounding of it.
        if (next >= detail::kSsnProxMax * (1.0 - detail::kSsnProxCapSlack)) {
            next = detail::kSsnProxMax;
        }
        ladder_sigma_ = next;
        apply_sigma(qp, n, me, mi, mb);
        return true;
    }

    // -----------------------------------------------------------------------
    // R4 -- THE FARKAS RESIDUAL TEST (Task 6b Phase B), matvec-only
    // -----------------------------------------------------------------------
    //
    // The QP's constraint system is {Ae x = be} together with the
    // inequality-shaped rows a_k^T x <= b_k of section 1 -- the rows of Ai and
    // every finite bound row alike, which is exactly the row language this file
    // already speaks. Farkas' lemma: that system is INFEASIBLE if and only if
    // there is (y_e free, y >= 0) with
    //
    //     Ae^T y_e + sum_k y_k a_k = 0    and    <be, y_e> + sum_k y_k b_k < 0.
    //
    // `dle`/`dli`/`dlb` is the DUAL INCREMENT the caller wants tested. It is
    // projected onto the sign cone (the equality block is free and passes
    // through; the two non-negative blocks are clipped at 0), normalized by its
    // own inf-norm, and the two Farkas quantities are evaluated:
    //
    //   * the RESIDUAL ||Ae^T y_e + Ai^T y_i + B^T y_b||inf, reported RELATIVE
    //     to the same combination taken in absolute value -- the cancellation-
    //     free scale of the sum, so a badly scaled row cannot make a
    //     non-certificate look like one or vice versa;
    //   * the OBJECTIVE <b, y>, likewise relative to the sum of |b_k y_k|.
    //
    // One matvec over Ae/Ai plus O(mb) plus O(n): no factorization, no solve,
    // and no allocation beyond the two n-vectors below.
    //
    // Returns true iff both tolerances are met, i.e. iff the increment IS an
    // approximate Farkas direction. `resid_out`/`gap_out` carry the two
    // relative quantities for the caller's diagnostic message.
    bool farkas_certificate(const QpProblem &qp, const Vec &dle, const Vec &dli, const Vec &dlb,
                            Index mb, double *resid_out, double *gap_out) const {
        const Index n = qp.n();
        const Index me = qp.me();
        const Index mi = qp.mi();

        // --- project onto the sign cone, and normalize --------------------
        Vec ye = dle;
        Vec yi = mi > 0 ? Vec(dli.cwiseMax(0.0)) : Vec(0);
        Vec yb = mb > 0 ? Vec(dlb.cwiseMax(0.0)) : Vec(0);
        const double scale = dual_norm(ye, yi, yb);
        if (!(scale > 0.0) || !std::isfinite(scale)) {
            return false;
        }
        ye /= scale;
        if (mi > 0) {
            yi /= scale;
        }
        if (mb > 0) {
            yb /= scale;
        }

        // --- the two matvecs, accumulated in one pass ---------------------
        Vec r = Vec::Zero(n);
        Vec r_abs = Vec::Zero(n);
        double gap = 0.0;
        double gap_abs = 0.0;
        for (Index k = 0; k < me; ++k) {
            const double y = ye(k);
            for (SpMatRM::InnerIterator it(qp.Ae, k); it; ++it) {
                r(it.col()) += it.value() * y;
                r_abs(it.col()) += std::abs(it.value() * y);
            }
            gap += qp.be(k) * y;
            gap_abs += std::abs(qp.be(k) * y);
        }
        for (Index k = 0; k < mi; ++k) {
            const double y = yi(k);
            for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
                r(it.col()) += it.value() * y;
                r_abs(it.col()) += std::abs(it.value() * y);
            }
            gap += qp.bi(k) * y;
            gap_abs += std::abs(qp.bi(k) * y);
        }
        for (std::size_t s = 0; s < bound_rows_.size(); ++s) {
            const detail::SsnBoundRow &br = bound_rows_[s];
            const double y = yb(static_cast<Index>(s));
            r(br.var) += br.sign * y;
            r_abs(br.var) += std::abs(br.sign * y);
            gap += br.rhs * y;
            gap_abs += std::abs(br.rhs * y);
        }

        const double rel_resid =
            n > 0 ? r.cwiseAbs().maxCoeff() / std::max(1.0, r_abs.maxCoeff()) : 0.0;
        const double rel_gap = gap / std::max(1.0, gap_abs);
        *resid_out = rel_resid;
        *gap_out = rel_gap;
        if (!std::isfinite(rel_resid) || !std::isfinite(rel_gap)) {
            return false;
        }
        return rel_resid <= detail::kSsnFarkasResidualTol && rel_gap <= -detail::kSsnFarkasGapTol;
    }

    // -----------------------------------------------------------------------
    // Residual
    // -----------------------------------------------------------------------

    // Fills every scratch block and returns both norms of F: the inf-norm that
    // certifies (header section 5) and the merit 1/2||F||_2^2 the line search
    // decreases. Both come out of the same walk over the same blocks.
    detail::SsnNorms residual(const QpProblem &qp, const Vec &x, const Vec &le, const Vec &li,
                              const Vec &lb, Vec &resid_x, Vec &resid_e, Vec &slack_i, Vec &slack_b,
                              Vec &phi_i, Vec &phi_b) const {
        const Index n = qp.n();
        const Index me = qp.me();
        const Index mi = qp.mi();

        resid_x = qp.g;
        for (Index i = 0; i < n; ++i) { // H stores the upper triangle only
            for (SpMatRM::InnerIterator it(qp.H, i); it; ++it) {
                const Index j = it.col();
                resid_x(i) += it.value() * x(j);
                if (j != i) {
                    resid_x(j) += it.value() * x(i);
                }
            }
        }
        for (Index r = 0; r < me; ++r) {
            double ax = 0.0;
            for (SpMatRM::InnerIterator it(qp.Ae, r); it; ++it) {
                ax += it.value() * x(it.col());
                resid_x(it.col()) += it.value() * le(r);
            }
            resid_e(r) = ax - qp.be(r);
        }
        for (Index k = 0; k < mi; ++k) {
            double ax = 0.0;
            for (SpMatRM::InnerIterator it(qp.Ai, k); it; ++it) {
                ax += it.value() * x(it.col());
                resid_x(it.col()) += it.value() * li(k);
            }
            slack_i(k) = qp.bi(k) - ax;
            phi_i(k) = detail::ssn_fb(slack_i(k), li(k));
        }
        for (std::size_t r = 0; r < bound_rows_.size(); ++r) {
            const detail::SsnBoundRow &br = bound_rows_[r];
            const Index rr = static_cast<Index>(r);
            resid_x(br.var) += br.sign * lb(rr);
            slack_b(rr) = br.rhs - br.sign * x(br.var);
            phi_b(rr) = detail::ssn_fb(slack_b(rr), lb(rr));
        }

        detail::SsnNorms out;
        double sq = 0.0;
        auto acc = [&out, &sq](const Vec &v) {
            if (v.size() > 0) {
                out.inf_norm = std::max(out.inf_norm, v.cwiseAbs().maxCoeff());
                sq += v.squaredNorm();
            }
        };
        acc(resid_x);
        acc(resid_e);
        acc(phi_i);
        acc(phi_b);
        out.merit = 0.5 * sq;
        return out;
    }

    // -----------------------------------------------------------------------
    // Branch selection (header sections 2 and 4)
    // -----------------------------------------------------------------------
    //
    // Writes alpha/beta/row_resid/klass at slot `slot`. `hinted` says a hint
    // governs THIS row (first step, and the relevant half of the hint was
    // supplied); `hint_active` is that hint's verdict. `guarded` selects the
    // three-set classification and the uncertain damping; under kBare this
    // function is Task 3's, statement for statement.
    //
    // ---- THE THREE-SET PARTITION (CHR 2015's scaffold) --------------------
    //
    // The FB pair's own margin |alpha - beta| measures how far the row is from
    // the kink ray s = lambda, where its classification is undecidable
    // (detail::kSsnUncertainEnter derives the geometry). The classification is
    // read with HYSTERESIS -- enter the uncertain set at `tau`, leave it only
    // at kSsnUncertainLeaveRatio * tau -- so a row whose margin hovers inside
    // the band keeps whatever class it had and cannot chatter between two
    // partitions from step to step.
    //
    // **THE TIE POLICY IS DETERMINISTIC AND IS A CONSEQUENCE OF THE BAND, NOT A
    // SEPARATE RULE.** An exact tie (alpha == beta, the (0, 0) row among them)
    // has margin 0, which is inside every band with tau > 0, so a tie NEVER
    // decides anything: a previously decided row becomes uncertain and a
    // previously uncertain row stays uncertain. There is no coin to flip and no
    // dependence on which of two equal quantities the comparison happens to
    // favour. (Task 3's binary rule broke exact ties toward INACTIVE, by using
    // a strict >. That is still what kBare does, and it is still what the
    // ACTIVITY EXPORT does, because QpSolution's contract is binary.)
    //
    // ---- WHAT AN UNCERTAIN ROW GETS: BOTH BRANCHES DAMPED -----------------
    //
    // The FB pair is replaced wholesale by the SYMMETRIC element
    // (alpha, beta) = (1 - 1/sqrt(2), 1 - 1/sqrt(2)) -- the same
    // detail::kSsnDegenerateFbDeriv the kink itself uses. That is "both
    // branches damped" literally: the row's diagonal becomes -(1 + mu + sigma)
    // instead of racing toward -mu (a hard equality) or toward -2e12 (a
    // decoupled row), and its coupling to dx keeps a moderate 0.293 weight
    // instead of 1 or ~0.
    //
    // **AND IT IS STILL A GENERALIZED JACOBIAN ELEMENT**, which is what keeps
    // this a semismooth Newton method rather than a heuristic: with
    // (xi, eta) = (1 - alpha, 1 - beta), the C-subdifferential of phi at the
    // kink is exactly {(1 - xi, 1 - eta) : xi^2 + eta^2 <= 1}, the symmetric
    // element sits ON that unit circle (it is a B-subdifferential element, the
    // limit of gradients along s = lambda > 0), and the row it is applied to is
    // within O(tau) of the kink by construction. So a damped row is a Newton
    // step for an element of ∂_C phi at a point O(tau) away -- an inexact
    // generalized Jacobian with a bounded, stated error, not a branch.
    //
    // ---- WHY THE HINT OVERRIDES THE CLASSIFICATION ------------------------
    //
    // A hinted row takes its hint, uncertain or not, and that ordering is
    // load-bearing rather than incidental: the exact-solution seed of fixture
    // (a) has its active row at (s, lambda) = (0, 0), which is the kink itself,
    // so a classification that outranked the hint would damp precisely the row
    // the hint exists to snap onto its face -- and the one-iteration
    // correct-hint payoff, which is what gate G1 is scored on, would be gone.
    // The hint governs step 0 only; every step after it is classified.
    static void select_branch(double s, double lam, double phi, bool hinted, bool hint_active,
                              bool guarded, double tau, std::vector<double> &alpha,
                              std::vector<double> &beta, std::vector<double> &row_resid,
                              std::vector<detail::SsnRowClass> &klass, Index slot) {
        const std::size_t k = static_cast<std::size_t>(slot);
        if (hinted) {
            if (hint_active) {
                alpha[k] = 1.0;
                beta[k] = 0.0;
                row_resid[k] = s; // drive the slack to zero
            } else {
                alpha[k] = detail::kSsnAlphaFloor;
                beta[k] = 1.0;
                row_resid[k] = lam; // drive the multiplier to zero
            }
            klass[k] = hint_active ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
            return;
        }
        const double rho = std::sqrt(s * s + lam * lam);
        if (rho < detail::kSsnRhoFloor) {
            alpha[k] = detail::kSsnDegenerateFbDeriv;
            beta[k] = detail::kSsnDegenerateFbDeriv;
        } else {
            alpha[k] = 1.0 - s / rho;
            beta[k] = 1.0 - lam / rho;
        }
        row_resid[k] = phi;

        if (!guarded) {
            klass[k] =
                alpha[k] > beta[k] ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
            return;
        }

        const double margin = alpha[k] - beta[k];
        const double mag = std::abs(margin);
        const bool was_uncertain = klass[k] == detail::SsnRowClass::kUncertain;
        const double gate = was_uncertain ? tau * detail::kSsnUncertainLeaveRatio : tau;
        // tau == 0 disables the set OUTRIGHT, including for the exact tie that
        // "mag <= 0" would otherwise capture -- the field's documented meaning
        // is "no uncertain set", not "an uncertain set of measure zero".
        if (tau > 0.0 && mag <= gate) {
            klass[k] = detail::SsnRowClass::kUncertain;
        } else {
            klass[k] = margin > 0.0 ? detail::SsnRowClass::kActive : detail::SsnRowClass::kInactive;
        }
        if (klass[k] == detail::SsnRowClass::kUncertain) {
            alpha[k] = detail::kSsnDegenerateFbDeriv;
            beta[k] = detail::kSsnDegenerateFbDeriv;
        }
    }

    QpOptions opts_;
    detail::KktFactor kkt_;
    SpMatRM k_;
    Index dim_ = 0;
    std::vector<detail::SsnBoundRow> bound_rows_;
    std::vector<double> real_lower_, real_upper_;
    // The pattern cache (M5): the structure key k_ was built for -- both
    // conjuncts of it (the combined pattern digest, and the bound-row
    // (var, sign) list the digest deliberately omits) -- and the position in
    // k_.valuePtr() of each entry for_each_entry() emits, in emission order.
    std::vector<std::size_t> value_pos_;
    std::uint64_t structure_key_ = 0;
    std::vector<std::pair<Index, double>> structure_bound_key_;
    bool has_structure_ = false;
    // The regularizers THIS solve resolved to (SolveOverrides, or opts_ when
    // the override is at its sentinel). Members rather than parameters because
    // for_each_entry is driven from two places and both must see the same pair.
    //
    // base_* is what the CALLER asked for and never moves inside a solve;
    // eff_* is base_* + prox_sigma_ and is what for_each_entry emits. Keeping
    // both is what lets the proximal ladder step without losing the caller's
    // own regularization (a ladder that overwrote eff_* would silently reset
    // primal_delta to sigma).
    double base_delta_ = 0.0;
    double base_mu_ = 0.0;
    double eff_delta_ = 0.0;
    double eff_mu_ = 0.0;
    // The proximal-point regularizer (Task 4). Per-solve, monotone
    // non-decreasing, 0.0 whenever the escalation ladder never armed.
    double prox_sigma_ = 0.0;
    // R1 (Task 6b Phase B). `ladder_sigma_` is the LADDER's own monotone state
    // and `lm_sigma_` the residual-driven size; `prox_sigma_` is their max.
    // Under the shipped SsnSigmaRule::kLadder `lm_sigma_` never leaves 0, so
    // `ladder_sigma_ == prox_sigma_` identically and the pair is invisible.
    double ladder_sigma_ = 0.0;
    double lm_sigma_ = 0.0;

    // R5's pending-verdict state (Phase-7 Task 6b Phase B). `deferred_pending_`
    // is the whole of the contract: while it is true, k_ holds the matrix a
    // certifying exit declined to factorize and kkt_ must not be touched. The
    // other five fields are only what the withdrawal message needs, saved
    // because the loop that knew them has exited.
    bool deferred_pending_ = false;
    Index deferred_pos_ = 0;
    Index deferred_neg_ = 0;
    Index deferred_attempt_ = 0;
    double deferred_fb_residual_ = 0.0;
    double deferred_fb_tol_ = 0.0;
};

} // namespace hven::solvers
