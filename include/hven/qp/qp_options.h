// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The QP-kernel selector and the IPQP / SSN option types the SQP options carry.
///
/// These types are consumed by the QP engines (detail/qp/) and carried by `SqpOptions`
/// (sqp/sqp_options.h, which includes this header), so they live in the QP layer: the engines
/// below the solvers need no include of a solver header to name their own options.

#include <optional>

#include <hven/core/types.h>

namespace hven {

/// @brief Which kernel solves each SQP subproblem.
///
/// kWalk is the primal active-set walk (walk_engine.h): one working-set change per
/// minor iteration. kSsn is the semismooth-Newton kernel (ssn_engine.h), whose
/// step changes the whole implied active set at once. kIpqp is the interior-point
/// tier (detail/qp/ipqp/ipqp_engine.h), which acquires the active set from an interior
/// start and hands a converged iterate to the tier-3 exact face refinement.
///
/// The default is kWalk.
enum class QpMode {
    kWalk,
    kSsn,
    kIpqp,
};

// Declared here, in the QP layer, rather than in ssn_engine.h or sqp_options.h: the SQP
// options carry them and the QP engines consume them, and neither engine may include the
// other's header. Their SEMANTICS live at their SsnOptions fields, which
// is also where each mechanism is derived; this file declares the alphabet and
// the defaults, and every default below is the shipped iteration bit for bit.

/// @brief How the proximal/Levenberg-Marquardt shift sigma is sized.
///
/// - kLadder: the shipped failure-reactive ladder -- arm at kSsnProxInit, x100
///   per escalation trigger, capped at kSsnProxMax.
/// - kResidualArmed: once the ladder has ARMED, size sigma from the residual
///   instead of climbing, with the ladder retained underneath as a monotone
///   floor. Inert on any solve whose ladder never arms.
/// - kResidualAlways: as kResidualArmed, but sized from the first attempt --
///   the proactive form the Levenberg-Marquardt theory is stated for. Inert
///   nowhere: every solve carries at least the floor.
enum class SsnSigmaRule {
    kLadder = 0,
    kResidualArmed = 1,
    kResidualAlways = 2,
};

/// @brief What protects the hinted first step.
///
/// - kIterationZeroFree: the shipped rule -- iteration 0 is exempt from the
///   Armijo test when a hint governed it, with no safety net.
/// - kWatchdog: up to q relaxed steps, and if sufficient decrease has not
///   materialized by then, a return to the best stored point followed by a
///   monotone step. Reproduces the exemption exactly when the hinted step works.
enum class SsnHintRule {
    kIterationZeroFree = 0,
    kWatchdog = 1,
};

/// @brief What turns an infeasibility suspicion into an exit.
///
/// - kSymptoms: the shipped conjuncts -- a stalled residual window plus
///   diverging multipliers.
/// - kFarkasGated: the symptoms ARM the check and a CERTIFICATE fires it -- the
///   dual increment tested as an approximate Farkas direction. A symptom not
///   accompanied by a Farkas direction no longer exits.
enum class SsnInfeasibilityRule {
    kSymptoms = 0,
    kFarkasGated = 1,
};

/// @brief How the IPQP tier's solve-entry cold start sets `mu_0`; see
/// `IpqpOptions::barrier_initial_rule`.
enum class IpqpBarrierInitialRule { kFixed, kComplementarity };

/// @brief The kIpqp tier's own settings.
///
/// Declared here for the same reason QpMode and the three Ssn*Rule enums are:
/// SqpOptions carries this struct as `ipqp` and the engine consumes it. Forwarded onto the
/// tier verbatim, and each field's mechanism
/// is derived at the engine that consumes it.
///
/// `qp_mode` stays kWalk by default, so nothing here is reachable until a caller
/// opts in -- but the fields are VALIDATED UNCONDITIONALLY at every mode, because
/// a field is out of range whether or not this solve will read it.
struct IpqpOptions {
    /// Iteration budget for the tier's own Mehrotra loop. Empty (the default)
    /// derives it from the subproblem's size, as `QpOptions::max_iterations` does. An
    /// engaged value must be >= 1.
    std::optional<Index> max_iterations;

    /// The hard cap of LAST RESORT (`IpqpEscape::kBudget`): it fires only when
    /// the early-stall detector should have fired first and did not. Unlike
    /// `max_iterations` it is never derived, so it must be a genuine, positive
    /// iteration count. Default 60. Must be > 0. PER ATTEMPT, not per
    /// subproblem: the warm-kill re-bases both caps at the cold restart, so an
    /// abandoned attempt may total up to twice this value.
    Index hard_max_iterations = 60;

    /// Factorization budget for the tier, enforced BEFORE EVERY factorization --
    /// ladder rungs and the final inertia read included, not merely once per
    /// iteration. Empty (the default) is 3 x the tier's EFFECTIVE ITERATION
    /// BUDGET, `min(effective max_iterations, hard_max_iterations)`, so 180 at the
    /// shipped defaults. An engaged value must be >= 1. PER ATTEMPT, for the
    /// reason `hard_max_iterations` is, and every factorization is charged to the
    /// driver's probe budget either way.
    std::optional<Index> max_factorizations;

    /// The cold starting barrier parameter, and the ceiling of the warm-restart
    /// clamp `mu_0 = clamp(max(mu_meas, kappa_mu * mu_payload), barrier_min,
    /// barrier_initial)` -- a setting in both readings, never overwritten by payload
    /// evidence, only clamped against it. Default 1e-2, Ruiz-scaled. Must be
    /// finite, > 0, and >= `barrier_min`, since the clamp band is otherwise
    /// inverted.
    double barrier_initial = 1e-2;

    /// The FLOOR of the `mu_0` clamp above, and the tier's own barrier-decay
    /// floor thereafter. Default 1e-12, the interior-point engine's own
    /// `barrier_min`. Must be finite, > 0, and <= `barrier_initial`.
    double barrier_min = 1e-12;

    /// The (rho, delta) proximal regularization schedule's INITIAL values:
    /// `rho_0 = primal_regularization_initial`, `delta_0 = dual_regularization_initial`.
    /// `rho`/`delta` themselves enter the regularized KKT matrix's diagonal blocks (`H + rho I` on
    /// the primal block, `-delta I` on each dual block); it is the PROXIMAL CENTERING terms that
    /// enter the right-hand side. Default 8.0 each.
    ///
    /// `primal_regularization_initial` must be finite and lie in `[regularization_min,
    /// regularization_max]`: the starting value of a quantity the ladder only ever
    /// moves within that band must itself start inside it.
    double primal_regularization_initial = 8.0;
    /// The dual-block counterpart of `primal_regularization_initial`, same schedule, on a
    /// RELAXED band: it need only be finite and in `(0, regularization_max]` --
    /// positive, since a non-positive delta would not regularize the dual block
    /// at all, and no larger than the ceiling every regularized quantity in this
    /// schedule shares. Default 8.0.
    double dual_regularization_initial = 8.0;

    /// The absolute floor the (rho, delta) ladder's GATED decrease may never
    /// cross (Cipolla-Gondzio eq. 19). Default 1e-10. Must be finite and > 0.
    double regularization_min = 1e-10;

    /// The ceiling the (rho, delta) ladder's inertia-demanded growth may
    /// never cross; the tier inherits SSN's relative cap-slack exhaustion guard
    /// with it. Default 1e6. Must be finite and >= `regularization_min`.
    double regularization_max = 1e6;

    /// The (rho, delta) ladder's GATED decrease factor: applied only after
    /// the regularized residuals have contracted by the required amount
    /// never unconditionally the way `prox_reg_decay`'s geometric
    /// decay is -- this is a fraction the schedule multiplies the current
    /// value by, so it must lie strictly inside (0, 1): 0 would collapse the
    /// regularization in one gated step and >= 1 would never decrease it.
    /// Default 0.1.
    double regularization_decrease = 0.1;

    /// The fraction-to-boundary parameter (`tau`/`bfrac`) both the affine
    /// and corrector steps are clipped by. A standard IPM
    /// literature constant, meaningful only strictly inside (0, 1): 0 would
    /// permit no step at all and >= 1 would permit stepping onto or past a
    /// bound. Default 0.995.
    double fraction_to_boundary = 0.995;

    /// The RATIO threshold `kappa` of the face-classification rule: row `j` is
    /// active iff `s_j < kappa * z_j` and INACTIVE iff `z_j < kappa * s_j` --
    /// scale-free by construction, which is the point of a ratio rule over an
    /// absolute one. Default 1e-2. Must be finite and STRICTLY inside (0, 1): a
    /// non-positive kappa can never classify anything active, and at kappa >= 1
    /// the two tests stop being mutually exclusive, leaving no room for the
    /// routing chain's UNCERTAIN class.
    double face_ratio_threshold = 1e-2;

    /// The barrier-phase convergence target's SLACK factor: the tier converges to
    /// `convergence_slack x` the QP layer's own relative tolerances, deliberately LOOSER than the
    /// 1e-10 tier-3 exact refinement owns the last two decades of. Default 100. Must be finite and
    /// >= 1 -- a slack below 1 would ask the barrier phase for MORE accuracy than tier 3's own
    /// finish, inverting the division of labor the two-tier design rests on.
    double convergence_slack = 1e2;

    /// Ruiz equilibration inside the tier, unscaled on export.
    /// Default true. A plain A/B lever; every asserted pin is on unscaled
    /// quantities either way.
    bool ruiz_equilibration = true;

    /// The warm-seed repair: strict-positivity clamps plus the
    /// two-scalar `(delta_p, delta_d)` shift, applied before a warm restart
    /// is trusted. Default true. Off disables the repair, never the validation:
    /// a seed the repair would have had to fix degrades cold instead,
    /// mode-local and visible in the grade.
    bool warm_repair = true;

    /// The warm-kill rule's iteration budget: a warm-started solve overrunning
    /// this many iterations is restarted COLD exactly once. The effective budget
    /// IS CLAMPED at `min(max_warm_iterations, effective max_iterations)`, so a
    /// caller-chosen value larger than the solve's own iteration budget can never
    /// become the binding limit. Default 15. Must be >= 0 (0 is legal: every warm
    /// restart is killed on its first iteration).
    Index max_warm_iterations = 15;

    /// `kappa_mu` in the `mu_0` clamp rule: `mu_0 =
    /// clamp(max(mu_meas, barrier_adoption_factor * mu_payload), barrier_min,
    /// barrier_initial)`. 0 DISABLES ADOPTION (the clamp then reads only
    /// `mu_meas`), which is a deliberate, documented reading of this field
    /// rather than a degenerate one, so it stays legal. Default 1.0. Must be
    /// finite and >= 0.
    double barrier_adoption_factor = 1.0;

    /// The early-stall detector's window width in ACCEPTED
    /// iterations, an order of magnitude above the local regime so that no
    /// healthy trajectory reaches it. Default 5. Must be
    /// > 0 -- a zero-width window can never accumulate the whole-window
    /// evidence the test is built on.
    Index stall_window = 5;

    /// The escape ladder's retirement threshold: after this many
    /// CONSECUTIVE escapes within one SQP solve the tier is retired for the
    /// remainder of that solve; any success resets the count. Default 3.
    /// Must be > 0 -- retiring "after zero consecutive escapes" is not a
    /// count, it is disabling the tier outright, which this field does not
    /// exist to express.
    Index max_consecutive_escapes = 3;

    /// Optional Farkas corroboration of an infeasible-suspect
    /// escape: one matvec plus O(m), no factorization. It may ARM the
    /// evidence block; it never certifies. Default true.
    bool farkas_gate = true;

    /// Cross-major symbolic reuse kill switch: while true, the
    /// tier hoists its analysis/verify work across majors whose
    /// `AssemblyEvalSeam::epoch()` is unchanged.
    /// Default true; false forces a fresh symbolic pass every major.
    bool hoist_symbolic_analysis = true;

    /// The required final unregularized inertia read: when
    /// true, a certifying exit pays one extra
    /// unregularized factorization to confirm the inertia the regularized
    /// solve reported; a WRONG read downgrades the certificate rather than
    /// reporting `kOptimal`. Default true. FALSE means every certificate is
    /// downgraded unconditionally -- an explicit, documented weakening for a
    /// caller who wants to skip the extra factorization, never a silent one.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian under kExact; under
    /// SqpHessianMode::kLbfgsDamped the PD quasi-Newton B, so this certifies B, not the NLP.
    bool require_final_inertia = true;

    /// How the solve-entry cold start sets `mu_0`. kFixed (default): every pair at
    /// `barrier_initial`. kComplementarity: `mu_0` is derived from one affine-scaling solve; it
    /// fires only when the derived value lies in `[barrier_min, barrier_initial]`, on a cold
    /// solve-entry only, otherwise kFixed. The estimate is charged to the factorization budget.
    /// Counters: `ipqp_mu0_derived`, `ipqp_mu0_fallbacks`.
    IpqpBarrierInitialRule barrier_initial_rule = IpqpBarrierInitialRule::kFixed;

    /// The residual-proportional `mu` floor. Default false: the corrector
    /// target is `clamp(sigma * mu, barrier_min, barrier_initial)`. True: `sigma * mu` is first
    /// raised to `mu_s * r / max(1, r_s)`, `r` the unregularized relative residual and
    /// `(mu_s, r_s)` the attempt's first iterate. Counter: `ipqp_mu_floor_binds`.
    bool barrier_residual_floor = false;

    /// The inertia ladder's zero trial is skipped once this many CONSECUTIVE iterations have
    /// needed a modification (after Wachter-Biegler 2006). Once skipped it is not re-offered
    /// within the attempt. Default 3. 0 skips whenever the ladder has a memory; a value above
    /// the attempt's iteration count never skips. Must be >= 0.
    /// Counter: `ipqp_ladder_zero_trial_skips`. CURVATURE SOURCE: the subproblem Hessian
    /// -- the exact model Hessian under kExact; under SqpHessianMode::kLbfgsDamped the PD
    /// quasi-Newton B, so this reads B, not the NLP.
    Index ladder_skip_after = 3;
};

/// @brief Boundary validation of the kIpqp tier's own options.
///
/// Every field is validated unconditionally, whatever the mode a solve will run in. The IPQP
/// engine calls this on entry (it is reachable without a driver) and `validate(SqpOptions)`
/// calls it at the same position as before, so the thrown messages are the ones the SQP
/// options always produced.
///
/// @throws std::invalid_argument naming the field and its value.
void validate(const IpqpOptions &opts);

} // namespace hven
