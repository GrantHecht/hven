// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_evidence.h -- the IPQP tier's two ESCAPE EVIDENCE BLOCKS, split out of
// `ipqp_engine.h` at M6 W5 T4 so the PUBLIC trace schema (`drivers/trace.h`)
// can nest them in `IpqpTraceEscapeEvidence` without pulling the engine.
//
// `IpqpResult` still carries both, and `ipqp_engine.h` includes this header, so
// nothing that already had them lost them. THE DEFINITIONS ARE UNCHANGED by the
// split: same fields, same order, same defaults, same documentation.
//
// STORAGE IS `bool`/`Index`/`double`/`Vec` ONLY, which is why this header
// includes `core/types.h` and NOTHING else -- that is the property that makes
// the split acyclic, and the install smoke compiles this header standalone to
// keep it true.

#include <hven/core/types.h>

namespace hven::solvers {

/// @brief THE SECTION 6.2 STALL ESCAPE'S EVIDENCE BLOCK.
///
/// Plan section 7 note (b), FINAL: the v2 draft's three `ipqp_stall_reason_*` counters are
/// DROPPED as ill-posed -- all three conjuncts hold at EVERY stall escape, so a partition
/// among them is degenerate. The three conjunct VALUES at window close travel here instead.
///
/// Populated ONLY on a `IpqpEscape::kStall` escape; `fired` is the flag that
/// says so, and every numeric field is 0 otherwise rather than carrying a
/// stale window's values.
struct IpqpStallEvidence {
    bool fired = false;

    /// Accepted steps in the closed window (`IpqpOptions::ipqp_stall_window`).
    Index window = 0;

    /// CONJUNCT (i): `mu(window start) / mu(window end)`, the barrier's ACROSS-WINDOW
    /// reduction; holds (contributes to a stall) iff `< detail::kIpqpStallMuFactor`.
    /// `+inf` if `mu` reached exactly 0, which cannot be a stall.
    ///
    /// This conjunct ALSO carries section 6.2's reset (plan section 7 note (o)): a reset on any
    /// `mu` drop would re-arm on every healthy step, so the only coherent reset IS this
    /// conjunct failing. Read only at window CLOSE: that delays a re-arm, never fakes a stall.
    double mu_ratio = 0.0;

    /// CONJUNCT (ii): `1 - res(end)/res(start)` on `max(primal_inf, dual_inf)` -- the RELATIVE
    /// improvement across the window, which the conjunct holds iff it is
    /// `< 1 - detail::kSsnStallImproveFactor` (1%). Negative when the residual grew.
    double residual_improvement = 0.0;

    /// The SMALLEST `min(alpha_p, alpha_d)` over the window -- plan section 7
    /// note (b)'s named "min alpha".
    double min_alpha = 0.0;

    /// CONJUNCT (iii)'s ACTUAL TEST VALUE, a second field and not a replacement for `min_alpha`
    /// above: the conjunct is a WHOLE-WINDOW property, so what decides it is the LARGEST
    /// per-step `min(alpha_p, alpha_d)`. The plan names the first, the test uses this one.
    double max_step_alpha = 0.0;
};

/// @brief THE SECTION 6.3 INFEASIBLE-SUSPECT ESCAPE'S EVIDENCE BLOCK.
///
/// **A SIGNATURE, NEVER A PROOF** (section 6: IP-PMM has no homogeneous self-dual embedding
/// and so no infeasibility certificate). The tier emits `IpqpEscape::kInfeasibleSuspect`
/// carrying this block and NEVER `QpStatus::kInfeasible`.
///
/// Section 6.3 names three things this block must carry -- each signal with its value, the
/// window, and the least-infeasible point -- and they are the three groups below.
struct IpqpInfeasibilityEvidence {
    bool fired = false;

    /// True iff the EXHAUSTION route fired (budget ran out with the signature standing) rather
    /// than the standing windowed route. The two measure the growth conjunct differently (see
    /// `dual_growth` / `dual_step_growth`): on exhaustion a windowed reference would read 1.
    bool exhaustion_route = false;

    /// Accepted steps in the window the signals were measured over.
    Index window = 0;

    // --- signal (a): the primal residual, flat on a positive floor ---------

    double primal_start = 0.0; ///< `max(primal_eq, primal_iq)` at window start.
    double primal_end = 0.0;   ///< ... and at the window's end.
    /// `1 - primal_end/primal_start`; the conjunct holds iff this is below
    /// `1 - detail::kSsnStallImproveFactor` AND `primal_end` is above the solve's own
    /// feasibility target -- "flat ON A POSITIVE FLOOR", both halves.
    double primal_improvement = 0.0;

    // --- signal (b): multiplier norm growth --------------------------------

    double dual_norm_start = 0.0; ///< `||(y, z)||inf` at the reference point.
    double dual_norm_end = 0.0;   ///< ... and at the window's end.
    /// `dual_norm_end / max(1, dual_norm_start)`, floored at 1 so a zero-multiplier reference
    /// is measured absolutely (`detail::kSsnDualGrowthFactor`'s own convention). The conjunct
    /// holds iff this is `>= detail::kSsnDualGrowthFactor`.
    double dual_growth = 0.0;
    /// Growth across the MOST RECENTLY ACCEPTED STEP alone. Read only on the
    /// exhaustion route, where the conjunct additionally demands
    /// `>= detail::kSsnDualStepGrowth`; 0 on the standing route.
    double dual_step_growth = 0.0;

    // --- optional Farkas corroboration (IpqpOptions::ipqp_farkas_gate) -----

    /// True iff the Farkas test (one matvec plus O(m), no factorization) on the window's
    /// normalized dual INCREMENT corroborated the signature. **IT NEVER CERTIFIES** (6.3): a
    /// false is "not corroborated", not "withdrawn", and the escape fires either way.
    bool farkas_corroborated = false;
    double farkas_residual = 0.0; ///< Relative `||A^T y||inf`; 0 when the gate is off.
    double farkas_gap = 0.0;      ///< Relative `<b, y>`; 0 when the gate is off.

    // --- the least-infeasible point ---------------------------------------

    /// n. The iterate with the SMALLEST `max(primal_eq, primal_iq)` seen in this solve --
    /// section 6.3's own third requirement, and the point a feasibility-mode fallback (the W2
    /// hook) wants to start from. Empty when the block did not fire.
    Vec least_infeasible_x;
    double least_infeasible_primal = 0.0; ///< That point's own primal residual.

    /// THAT SAME POINT'S SLACKS AND DUALS -- `s` and `lambda_i` (mi), `zl`/`zu` (n). W2's
    /// working-set seed classifies the point with spec 2.3 item 2's RATIO rule, which needs both
    /// sides of every pair; empty exactly when `least_infeasible_x` is.
    Vec least_infeasible_s;
    Vec least_infeasible_lambda_i;
    Vec least_infeasible_zl;
    Vec least_infeasible_zu;
    /// The barrier level at that point: the scale the ratio rule's `z > mu` conjunct is stated
    /// against, so an absent one degrades the seed to GEOMETRIC activity alone.
    double least_infeasible_mu = 0.0;
};

} // namespace hven::solvers
