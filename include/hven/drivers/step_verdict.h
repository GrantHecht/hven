// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The SQP globalization's verdict on one trial step, as a history row
///        reports it (`SqpIterate::verdict`).

namespace hven {

/// @brief The four outcomes of KLV Algorithm 2's optimality branch.
enum class StepVerdict {
    /// f-type: Eq. (10) and (11) both hold. Width unchanged.
    ///
    /// DRIVER OBLIGATION: an ACCEPTANCE verdict, not a convergence verdict.
    /// Algorithm 2 opens with the ||d||=0 KKT short-circuit AHEAD of the
    /// acceptance test; the driver must keep testing KKT residuals at the top
    /// of every major and must not infer convergence from a run of kAcceptObjectiveStep --
    /// Eq. (11) with sigma = 1e-4 is satisfied by arbitrarily small decreases.
    kAcceptObjectiveStep,
    /// h-type: Eq. (10) fails, Eq. (12) holds. Width shrinks by Eq. (13).
    kAcceptFeasibilityStep,
    /// "The step is rejected, and either the trust-region radius or the step
    /// size is reduced" (KLV Sec. 2.4.2).
    kReject,
    /// Switch to the feasibility restoration phase.
    ///
    /// DRIVER OBLIGATION: ADDITIVE, NOT A REPLACEMENT. KLV's authoritative
    /// restoration triggers are the driver's -- Algorithm 5's "subproblem
    /// infeasible" and Algorithm 4's "alpha < alpha_min" (here: a radius
    /// floor). The driver must not skip them because the strategy also reports
    /// kRestore: this signature is a heuristic early signal that can miss;
    /// an infeasible subproblem is a fact leaving no step at all. All three
    /// enter the SAME restoration phase.
    kRestore
};

} // namespace hven
