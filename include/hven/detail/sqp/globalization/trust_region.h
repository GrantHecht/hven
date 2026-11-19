// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

namespace hven::detail {

// Trust-region update constants of the SQP driver. Source: Conn, Gould &
// Toint, "Trust-Region Methods" (MPS-SIAM, 2000), Algorithm BTR and Table
// 6.1.1 -- eta_2, gamma_2, gamma_1 respectively. [KLV] states the RULE ("if
// the trust region is active at d then increase the radius", Algorithm 3) but
// no values, so these are ported rather than transcribed. See sqp_solver.h's
// RADIUS MANAGEMENT note for the full radius lifecycle.

/// rho = actual/predicted objective decrease at or above which an accepted step
/// counts as "very successful" and earns a larger radius.
inline constexpr double kTrGrowThreshold = 0.75;
/// The expansion factor, applied only when the radius was ACTIVE at the step
/// and capped at SqpOptions::trust_region_max.
inline constexpr double kTrGrowFactor = 2.0;
/// The contraction factor, applied on every kReject verdict. COUPLED to
/// funnel.h's kRestoreMinRejections -- see RADIUS MANAGEMENT.
inline constexpr double kTrShrinkFactor = 0.5;

/// Relative size below which an L-BFGS pair's step counts as zero, ||s||inf <= kZeroStepScale *
/// max(1, ||x||inf) (LbfgsDamped's zero_step_scale): a numerical-zero threshold, far tighter
/// than any convergence tolerance. The driver's own zero-step short-circuit (KLV Algorithm 2's
/// ||d|| = 0) is not this constant: it reads qp_no_motion_tol (qp_problem.h), the QP kernels'
/// no-motion tolerance (docs/migration-0.2.md, "Zero-step short-circuit").
inline constexpr double kZeroStepScale = 1e-12;

/// THE FAILED SUBPROBLEM'S STEP, its second gate (M7 W2 R2,
/// SqpOptions::qp_failure_step): a step whose model predicts no decrease is
/// still judged when its trial violation is at most this fraction of the
/// iterate's -- a real infeasibility reduction, not a funnel-width one.
inline constexpr double kFailureStepViolationFactor = 0.5;

} // namespace hven::detail
