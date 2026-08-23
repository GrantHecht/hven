// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::solvers {

// Trust-region update constants of the SQP driver. Source: Conn, Gould &
// Toint, "Trust-Region Methods" (MPS-SIAM, 2000), Algorithm BTR and Table
// 6.1.1 -- eta_2, gamma_2, gamma_1 respectively. [KLV] states the RULE ("if
// the trust region is active at d then increase the radius", Algorithm 3) but
// no values, so these are ported rather than transcribed. See sqp_driver.h's
// RADIUS MANAGEMENT note for the full radius lifecycle.

/// rho = actual/predicted objective decrease at or above which an accepted step
/// counts as "very successful" and earns a larger radius.
inline constexpr double kTrGrowThreshold = 0.75;
/// The expansion factor, applied only when the radius was ACTIVE at the step
/// and capped at SqpOptions::tr_max.
inline constexpr double kTrGrowFactor = 2.0;
/// The contraction factor, applied on every kReject verdict. COUPLED to
/// globalization.h's kRestoreMinRejections -- see RADIUS MANAGEMENT.
inline constexpr double kTrShrinkFactor = 0.5;

/// KLV Algorithm 2's opening line, in floating point: "if ||d|| = 0 then
/// acceptable <- true // KKT point found". A step counts as ZERO when its
/// inf-norm is below this fraction of the iterate's own scale,
/// ||p||inf <= kZeroStepScale * max(1, ||x||inf).
///
/// WHY THE SHORT-CIRCUIT EXISTS (and why it is not redundant with the
/// convergence test): the convergence test measures stationarity with THE
/// ITERATE'S CURRENT MULTIPLIERS, which are only refreshed on an ACCEPTED step
/// — a rejected trial's multipliers are deliberately discarded. At a point
/// where the subproblem's own answer is p = 0, the QP has just priced the
/// EXACT multipliers that make x stationary, yet the funnel rejects the zero
/// step on rounding alone: pred_df and actual decrease are both at noise level,
/// the Armijo comparison is two numbers that are noise, the iterate does not
/// move, the multipliers are thrown away, and the same thing repeats at half
/// the radius forever. MEASURED on the restoration phase's own feasibility
/// problem entered at an infeasible stationary point (the start point ALREADY
/// optimal — the normal case there): 64 consecutive rejections, stationarity
/// pinned by multipliers that stayed zero, ending at the radius floor instead
/// of certifying a point that was correct on trial 0.
///
/// The short-circuit therefore lives where the STEP is visible (here), and it
/// bypasses the strategy entirely (Algorithm 2 tests it BEFORE the funnel
/// condition; a zero step has nothing for an acceptance test to judge); the
/// resulting row is a kAcceptF with step_norm ~ 0.
///
/// THE SCALE FACTOR IS A NUMERICAL-ZERO THRESHOLD, not a convergence tolerance,
/// and is deliberately four to six orders TIGHTER than kkt_tol so the two can
/// never be confused: at 1e-12 relative, a qualifying step cannot move any
/// residual the driver measures by more than rounding, so accepting it unjudged
/// is safe even where the funnel would have rejected it. Testing p == 0 exactly
/// does not work: the engine's regularized solve returns ~1e-23, not 0.
inline constexpr double kZeroStepScale = 1e-12;

} // namespace hven::solvers
