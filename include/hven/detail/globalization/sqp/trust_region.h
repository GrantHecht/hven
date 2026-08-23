// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// trust_region.h -- the trust-region update constants of the SQP driver,
// carved verbatim out of drivers/sqp_driver.h (phase-C S3, restructure only).
// The comments below still speak from that header's point of view: "this
// header's RADIUS MANAGEMENT note" and "the kReject branch" name a note and
// code that remain in drivers/sqp_driver.h, which includes this file from its
// top include block.

namespace hven::solvers {

// =============================================================================
// Trust-region update constants. See this header's RADIUS MANAGEMENT note for
// the source: Conn, Gould & Toint, "Trust-Region Methods" (MPS-SIAM, 2000),
// Algorithm BTR and Table 6.1.1 -- eta_2, gamma_2, gamma_1 respectively. [KLV]
// states the RULE ("if the trust region is active at d then increase the
// radius", Algorithm 3) but no values, so these are ported rather than
// transcribed.
// =============================================================================

// rho = actual/predicted objective decrease at or above which an accepted step
// counts as "very successful" and earns a larger radius.
inline constexpr double kTrGrowThreshold = 0.75;
// The expansion factor, applied only when the radius was ACTIVE at the step
// and capped at SqpOptions::tr_max.
inline constexpr double kTrGrowFactor = 2.0;
// The contraction factor, applied on every kReject verdict. COUPLED to
// globalization.h's kRestoreMinRejections -- see RADIUS MANAGEMENT.
inline constexpr double kTrShrinkFactor = 0.5;

// KLV ALGORITHM 2'S OPENING LINE, in floating point: "if ||d|| = 0 then
// acceptable <- true // KKT point found". A step counts as ZERO when its
// inf-norm is below this fraction of the iterate's own scale,
// ||p||inf <= kZeroStepScale * max(1, ||x||inf).
//
// WHY A SHORT-CIRCUIT IS NEEDED AT ALL, and why it is not redundant with the
// convergence test at the top of the loop (which globalization.h's WHAT
// judge() DELIBERATELY DOES NOT DECIDE note assumed it was). The convergence
// test measures stationarity with THE ITERATE'S CURRENT MULTIPLIERS, and
// those are only refreshed when a step is ACCEPTED -- a rejected trial's
// multipliers are deliberately discarded (see the kReject branch). At a point
// where the subproblem's own answer is p = 0, the QP has just priced the
// EXACT multipliers that make x stationary, and the funnel then rejects the
// zero step on rounding alone: pred_df and the actual decrease are both at
// the 1e-23 level, the Armijo test Eq. (11) compares two numbers that are
// noise, and whichever way it falls the iterate does not move, the
// multipliers are thrown away, and the SAME thing happens at half the radius
// forever. MEASURED (Task 9, on the restoration phase's own feasibility
// problem entered at an infeasible stationary point -- the case where the
// start point is ALREADY optimal, which is the normal case there): 64
// consecutive rejections, stationarity pinned at 1.0 by multipliers that
// stayed zero, ending at the radius floor instead of certifying a point that
// was correct on trial 0.
//
// So the paper's short-circuit is load-bearing rather than decorative, and it
// has to live where the STEP is visible, which is here. It bypasses the
// strategy entirely (Algorithm 2 tests it BEFORE the funnel condition, and a
// zero step has nothing for an acceptance test to judge), and the resulting
// row is a kAcceptF with step_norm ~ 0.
//
// THE SCALE FACTOR IS A NUMERICAL-ZERO THRESHOLD, not a convergence
// tolerance, and is deliberately four to six orders TIGHTER than kkt_tol so
// the two can never be confused: at 1e-12 relative, a step that qualifies
// cannot move any residual the driver measures by more than rounding, so
// accepting it unjudged is safe even where the funnel would have rejected it.
// The alternative of testing p == 0 exactly does not work: the engine's
// regularized solve returns 1e-23, not 0.
inline constexpr double kZeroStepScale = 1e-12;

} // namespace hven::solvers
