// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The SQP engine's options, as one value.
///
/// `SqpOptions` is an aggregate: write the fields you care about and hand the
/// whole value to the `SqpSolver` constructor or to `set_options()`, which
/// validate it.
///
///     SqpOptions o;
///     o.common.optimality_tol = 1e-8;
///     o.common.verbosity = Verbosity::kIterations;
///     SqpSolver solver(o);
///
/// Two tiers. The top-level fields, `common` and `linear` are the user tier:
/// documented and stable between break windows. `advanced` is the research tier:
/// its fields MAY CHANGE IN ANY MILESTONE, without a break window.

#include <functional>
#include <iosfwd>
#include <limits>
#include <memory>
#include <optional>
#include <string_view>

#include <hven/core/types.h>
#include <hven/drivers/common_options.h>
#include <hven/linear/dense_policy.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

/// The globalization strategy interface `SqpAdvancedOptions::make_strategy`
/// returns. Its definition is internal; a caller that implements one includes
/// the internal header that declares it.
class GlobalizationStrategy;

/// @brief Which kernel solves each SQP subproblem.
///
/// kWalk is the primal active-set walk (qp_engine.h): one working-set change per
/// minor iteration. kSsn is the semismooth-Newton kernel (ssn_engine.h), whose
/// step changes the whole implied active set at once. kIpm is the interior-point
/// tier (detail/qp/ipqp_engine.h), which acquires the active set from an interior
/// start and hands a converged iterate to the tier-3 exact face refinement.
///
/// The default is kWalk.
enum class QpMode {
    kWalk,
    kSsn,
    kIpm,
};

// Declared HERE rather than in ssn_engine.h for the same reason QpMode is:
// SqpOptions carries them and sqp_solver_types.h is the header ssn_engine.h includes,
// not the other way round. Their SEMANTICS live at their SsnOptions fields, which
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
/// `IpqpOptions::mu0_rule`.
enum class IpqpMu0Rule { kFixed, kComplementarity };

/// @brief The kIpm tier's own settings.
///
/// Declared HERE for the same reason QpMode and the three Ssn*Rule enums are:
/// SqpOptions carries this struct as `ipqp`, and sqp_solver_types.h is the header the
/// engine includes. Forwarded onto the tier verbatim, and each field's mechanism
/// is derived at the engine that consumes it.
///
/// `qp_mode` stays kWalk by default, so nothing here is reachable until a caller
/// opts in -- but the fields are VALIDATED UNCONDITIONALLY at every mode, because
/// a field is out of range whether or not this solve will read it.
struct IpqpOptions {
    /// Iteration budget for the tier's own Mehrotra loop. Empty (the default)
    /// derives it from the subproblem's size, as `QpOptions::max_iter` does. An
    /// engaged value must be >= 1.
    std::optional<Index> max_iterations;

    /// The hard cap of LAST RESORT (`IpqpEscape::kBudget`): it fires only when
    /// the early-stall detector should have fired first and did not. Unlike
    /// `max_iterations` it is never derived, so it must be a genuine, positive
    /// iteration count. Default 60. Must be > 0. PER ATTEMPT, not per
    /// subproblem: the warm-kill re-bases both caps at the cold restart, so an
    /// abandoned attempt may total up to twice this value.
    Index hard_iteration_cap = 60;

    /// Factorization budget for the tier, enforced BEFORE EVERY factorization --
    /// ladder rungs and the final inertia read included, not merely once per
    /// iteration. Empty (the default) is 3 x the tier's EFFECTIVE ITERATION
    /// BUDGET, `min(effective max_iterations, hard_iteration_cap)`, so 180 at the
    /// shipped defaults. An engaged value must be >= 1. PER ATTEMPT, for the
    /// reason `hard_iteration_cap` is, and every factorization is charged to the
    /// driver's probe budget either way.
    std::optional<Index> max_factorizations;

    /// The cold starting barrier parameter, and the ceiling of the warm-restart
    /// clamp `mu_0 = clamp(max(mu_meas, kappa_mu * mu_payload), min_mu,
    /// init_mu)` -- a setting in both readings, never overwritten by payload
    /// evidence, only clamped against it. Default 1e-2, Ruiz-scaled. Must be
    /// finite, > 0, and >= `min_mu`, since the clamp band is otherwise
    /// inverted.
    double init_mu = 1e-2;

    /// The FLOOR of the `mu_0` clamp above, and the tier's own barrier-decay
    /// floor thereafter. Default 1e-12, the interior-point engine's own
    /// `min_barrier`. Must be finite, > 0, and <= `init_mu`.
    double min_mu = 1e-12;

    /// The (rho, delta) proximal regularization schedule's INITIAL values:
    /// `rho_0 = rho_init`, `delta_0 = delta_init`. `rho`/`delta`
    /// themselves enter the regularized KKT matrix's diagonal blocks (`H + rho I`
    /// on the primal block, `-delta I` on each dual block); it is the PROXIMAL
    /// CENTERING terms that enter the right-hand side. Default 8.0 each.
    ///
    /// `rho_init` must be finite and lie in `[reg_floor,
    /// reg_max]`: the starting value of a quantity the ladder only ever
    /// moves within that band must itself start inside it.
    double rho_init = 8.0;
    /// The dual-block counterpart of `rho_init`, same schedule, on a
    /// RELAXED band: it need only be finite and in `(0, reg_max]` --
    /// positive, since a non-positive delta would not regularize the dual block
    /// at all, and no larger than the ceiling every regularized quantity in this
    /// schedule shares. Default 8.0.
    double delta_init = 8.0;

    /// The absolute floor the (rho, delta) ladder's GATED decrease may never
    /// cross (Cipolla-Gondzio eq. 19). Default 1e-10. Must be finite and > 0.
    double reg_floor = 1e-10;

    /// The ceiling the (rho, delta) ladder's inertia-demanded growth may
    /// never cross; the tier inherits SSN's relative cap-slack exhaustion guard
    /// with it. Default 1e6. Must be finite and >= `reg_floor`.
    double reg_max = 1e6;

    /// The (rho, delta) ladder's GATED decrease factor: applied only after
    /// the regularized residuals have contracted by the required amount
    /// never unconditionally the way `prox_reg_decay`'s geometric
    /// decay is -- this is a fraction the schedule multiplies the current
    /// value by, so it must lie strictly inside (0, 1): 0 would collapse the
    /// regularization in one gated step and >= 1 would never decrease it.
    /// Default 0.1.
    double reg_decrease = 0.1;

    /// The fraction-to-boundary parameter (`tau`/`bfrac`) both the affine
    /// and corrector steps are clipped by. A standard IPM
    /// literature constant, meaningful only strictly inside (0, 1): 0 would
    /// permit no step at all and >= 1 would permit stepping onto or past a
    /// bound. Default 0.995.
    double tau = 0.995;

    /// The RATIO threshold `kappa` of the face-classification rule: row `j` is
    /// active iff `s_j < kappa * z_j` and INACTIVE iff `z_j < kappa * s_j` --
    /// scale-free by construction, which is the point of a ratio rule over an
    /// absolute one. Default 1e-2. Must be finite and STRICTLY inside (0, 1): a
    /// non-positive kappa can never classify anything active, and at kappa >= 1
    /// the two tests stop being mutually exclusive, leaving no room for the
    /// routing chain's UNCERTAIN class.
    double face_kappa = 1e-2;

    /// The barrier-phase convergence target's SLACK factor: the tier converges to `converge_slack
    /// x` the QP layer's own relative tolerances, deliberately LOOSER than the 1e-10 tier-3 exact
    /// refinement owns the last two decades of. Default 100. Must be finite and >= 1 -- a slack
    /// below 1 would ask the barrier phase for MORE accuracy than tier 3's own finish, inverting
    /// the division of labor the two-tier design rests on.
    double converge_slack = 1e2;

    /// Ruiz equilibration inside the tier, unscaled on export.
    /// Default true. A plain A/B lever; every asserted pin is on unscaled
    /// quantities either way.
    bool ruiz = true;

    /// The warm-seed repair: strict-positivity clamps plus the
    /// two-scalar `(delta_p, delta_d)` shift, applied before a warm restart
    /// is trusted. Default true. Off disables the repair, never the validation:
    /// a seed the repair would have had to fix degrades cold instead,
    /// mode-local and visible in the grade.
    bool warm_repair = true;

    /// The warm-kill rule's iteration budget: a warm-started solve overrunning
    /// this many iterations is restarted COLD exactly once. The effective budget
    /// IS CLAMPED at `min(warm_iter_budget, effective max_iterations)`, so a
    /// caller-chosen value larger than the solve's own iteration budget can never
    /// become the binding limit. Default 15. Must be >= 0 (0 is legal: every warm
    /// restart is killed on its first iteration).
    Index warm_iter_budget = 15;

    /// `kappa_mu` in the `mu_0` clamp rule: `mu_0 =
    /// clamp(max(mu_meas, mu_adopt_factor * mu_payload), min_mu,
    /// init_mu)`. 0 DISABLES ADOPTION (the clamp then reads only
    /// `mu_meas`), which is a deliberate, documented reading of this field
    /// rather than a degenerate one, so it stays legal. Default 1.0. Must be
    /// finite and >= 0.
    double mu_adopt_factor = 1.0;

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
    Index retire_after = 3;

    /// Optional Farkas corroboration of an infeasible-suspect
    /// escape: one matvec plus O(m), no factorization. It may ARM the
    /// evidence block; it never certifies. Default true.
    bool farkas_gate = true;

    /// Cross-major symbolic reuse kill switch: while true, the
    /// tier hoists its analysis/verify work across majors whose
    /// `AssemblyEvalSeam::epoch()` is unchanged.
    /// Default true; false forces a fresh symbolic pass every major.
    bool hoist_symbolic = true;

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
    /// `init_mu`. kComplementarity: `mu_0` is derived from one affine-scaling solve; it
    /// fires only when the derived value lies in `[min_mu, init_mu]`, on a cold
    /// solve-entry only, otherwise kFixed. The estimate is charged to the factorization budget.
    /// Counters: `ipqp_mu0_derived`, `ipqp_mu0_fallbacks`.
    IpqpMu0Rule mu0_rule = IpqpMu0Rule::kFixed;

    /// The residual-proportional `mu` floor. Default false: the corrector
    /// target is `clamp(sigma * mu, min_mu, init_mu)`. True: `sigma * mu` is first
    /// raised to `mu_s * r / max(1, r_s)`, `r` the unregularized relative residual and
    /// `(mu_s, r_s)` the attempt's first iterate. Counter: `ipqp_mu_floor_binds`.
    bool mu_residual_floor = false;

    /// The inertia ladder's zero trial is skipped once this many CONSECUTIVE iterations have
    /// needed a modification (after Wachter-Biegler 2006). Once skipped it is not re-offered
    /// within the attempt. Default 3. 0 skips whenever the ladder has a memory; a value above
    /// the attempt's iteration count never skips. Must be >= 0.
    /// Counter: `ipqp_ladder_zero_trial_skips`. CURVATURE SOURCE: the subproblem Hessian
    /// -- the exact model Hessian under kExact; under SqpHessianMode::kLbfgsDamped the PD
    /// quasi-Newton B, so this reads B, not the NLP.
    Index ladder_skip_after = 3;
};

/// @brief Which QP kernel solves the elastic ladder's rungs.
///
/// `kWalk` is the shipped active-set ladder, hot-chained rung to rung; `kTier`
/// solves each rung with the interior-point kernel on a DEDICATED engine
/// instance.
enum class ElasticRungEngine { kWalk, kTier };

/// @brief The curvature source of the SQP subproblems.
///
/// `kExact` evaluates the model's Lagrangian Hessian at every subproblem build.
/// `kLbfgsDamped` never calls eval_hess: the subproblem curvature is a damped
/// compact L-BFGS approximation B (detail/globalization/sqp/lbfgs_damped.h),
/// positive definite by construction, carried into every QP engine call through
/// the mixed lift (mixed_lift.h).
enum class SqpHessianMode { kExact, kLbfgsDamped };

/// @brief The VARIABLE (column) half of problem scaling; see
///        SqpOptions::column_scaling and detail/drivers/column_scaling.h.
///
/// `kOff` never scales a variable. `kAlways` installs the rule's column factors
/// whenever they are not all 1 (a decision carried by a warm start replaces the
/// rule, SqpWarmStart::column_factors); the row/objective layer stays as
/// `SqpOptions::enable_scaling` says. `kAuto` installs them only on a BADLY scaled
/// problem -- factor spread >= `SqpOptions::column_scaling_threshold` -- and then
/// engages the row/objective layer for that solve too, whatever `enable_scaling`
/// says (with the one-sided objective rule when `enable_scaling` is off); below the
/// threshold the solve is the `kOff` solve plus the rule's one Jacobian probe.
enum class SqpColumnScaling { kOff, kAlways, kAuto };

/// @brief The CURVATURE the restoration phase's QPs carry; see
///        SqpOptions::restoration_curvature.
///
/// `kModel` gives the restoration sub-solve the solve's own curvature source: the
/// exact Hessian of the feasibility problem's Lagrangian under kExact (the wrapped
/// model's sum lambda_i * hess c_i, with lambda the l1 subgradient selectors), the
/// quasi-Newton B under kLbfgsDamped. `kRegularized` gives it the Levenberg-Marquardt
/// model B = mu(x) D^2 on the original variables and no constraint curvature, with
/// mu(x) = min(1, h(x)) (detail/globalization/sqp/restoration.h).
/// `kModelThenRegularized` runs the phase as kModel and restarts it from the point it
/// stands at under kRegularized at the first failed subproblem or exhausted elastic tier
/// inside the phase (the two triggers SqpOptions::restoration_curvature states); under
/// kLbfgsDamped (whose phase QPs are convex by construction) it is kModel.
enum class SqpRestorationCurvature {
    kModel,
    kRegularized,
    kModelThenRegularized,
};

/// @brief The SQP engine's research tier.
///
/// THESE FIELDS MAY CHANGE IN ANY MILESTONE, outside the break windows that
/// govern the rest of the options: the QP-tier ladders (IPQP, SSN, elastic), the
/// restoration internals, the quasi-Newton internals and the globalization hook.
struct SqpAdvancedOptions {
    /// Second-order correction A/B lever: when ON, a kReject trial whose
    /// constraint violation INCREASED gets one hot-started re-solve rescue
    /// attempt before the radius shrinks (the Maratos-effect defense; this
    /// project's cheap edge over Uno, which omits SOC entirely). Default
    /// true. Set false to recover exact non-SOC behaviour. The mechanism is
    /// described at sqp_solver.h's SECOND-ORDER CORRECTION note.
    bool enable_soc = true;

    /// Adaptive dual regularization A/B lever: when on, schedules
    /// SolveOverrides::dual_mu down with the KKT residual instead of leaving
    /// every subproblem at the engine's fixed QpOptions::dual_mu, which closes
    /// the fixed-mu accuracy ceiling on a badly scaled active set. Default true;
    /// false leaves SolveOverrides::dual_mu at its sentinel every major.
    /// primal_delta is NOT scheduled by this lever or by anything else.
    bool adaptive_mu = true;

    /// Factory for the globalization strategy, called ONCE PER solve() call;
    /// must return a non-null, freshly resettable GlobalizationStrategy
    /// (globalization.h) -- returning nullptr is a caller error and solve()
    /// throws std::invalid_argument. Default (empty std::function) means
    /// FunnelStrategy, KLV's funnel and this project's default
    /// globalization. Two solves never share strategy state.
    std::function<std::unique_ptr<GlobalizationStrategy>()> make_strategy;

    /// The full-step-first warm rule: when on, a solve whose warm-start level
    /// resolved to kWarm or above -- never a cold one, and never the 2-argument
    /// overload -- begins in globalization.h's full-step mode, with a WATCHDOG
    /// that restores the best iterate by max(stationarity, feasibility, complementarity) and hands
    /// the solve back to ordinary funnel globalization the moment the residual grows
    /// kWarmResidualGrowthMax majors in a row, or kWarmFullStepWindow majors pass
    /// without a new best. Default true.
    ///
    /// An A/B lever exactly like enable_soc and adaptive_mu: set false and every
    /// warm solve behaves precisely as with the mode off.
    ///
    /// IT CANNOT AFFECT a cold solve, a solve whose `warm` failed to resolve, or
    /// a solve running a caller-supplied strategy that is not a FunnelStrategy --
    /// the mode is that class's own state. Nor does it ever CERTIFY anything: it
    /// changes which trials are accepted, never the KKT test that decides
    /// kOptimal.
    bool warm_full_step = true;

    /// The elastic ladder's stall early-exit, opt-in: when on, the rho escalation
    /// ladder stops the moment one escalation leaves the augmented solution within
    /// kElasticStallScale -- a numerical-zero tolerance, not a bit-for-bit test --
    /// of where the previous rung left it, instead of always spending every rung.
    /// Default false, and off reproduces the driver's behaviour without the lever
    /// exactly, everywhere.
    ///
    /// Safe to turn on for a problem whose elastic relaxation's relevant slacks
    /// are pinned at a real bound over the rho range in play. On a relaxation
    /// with a flat augmented objective the ladder ends at an arbitrary point of
    /// many equally optimal ones, so turning it on there is a deliberate
    /// trajectory choice rather than a free cost cut. No cheap runtime test
    /// separates the two cases.
    bool elastic_ladder_early_exit = false;

    /// The crash basis, OPT-IN: when on, a COLD solve seeds its FIRST QP
    /// subproblem's working set from the activity geometry at x0 -- every
    /// inequality row geometrically active there, and every variable sitting on a
    /// finite bound there -- instead of making the engine rediscover them one
    /// ratio test at a time. Default FALSE, and off is byte-identical to the
    /// driver's behaviour without the lever.
    ///
    /// The predicates are not new. Both are the tests this driver already applies
    /// at every convergence check, at the SAME tolerance (common.feasibility_tol), reused
    /// verbatim:
    ///
    ///     row j seeded    :=  cI_j(x0) >= -common.feasibility_tol
    ///     var i at lower  :=  x0(i) - l(i) <= common.feasibility_tol,  l(i) finite
    ///     var i at upper  :=  u(i) - x0(i) <= common.feasibility_tol,  u(i) finite
    ///
    /// So there is no new constant to calibrate and no second notion of "active"
    /// in the driver. A variable satisfying both bound tests without being fixed
    /// is seeded kAtLower -- an arbitrary choice, not a derivation.
    ///
    /// Cold only, by construction: the seed is built at the first subproblem of a
    /// solve whose resolved start level is kCold and only when no warm seed
    /// exists. No extra model evaluation: both predicates are read off the first
    /// subproblem itself, never from a fresh eval_ci call.
    bool crash_basis = false;

    /// Which engine solves the elastic ladder's rungs. Default `kWalk`: the
    /// active-set walk hot-chains rung to rung, which a tier-solved rung cannot
    /// (it costs a second symbolic analysis). `kTier` hands each rung to the
    /// interior-point kernel on a dedicated `IpqpEngine`, leaving the primary
    /// tier's one-analysis-per-solve hoist untouched. Independent of `qp_mode`:
    /// the ladder is reachable in every mode.
    ElasticRungEngine elastic_rung_engine = ElasticRungEngine::kWalk;

    /// On a disproval, the fallback's certified solution seeds the tier's next
    /// entry. The escape drops the tier's cross-major carry; when the elastic
    /// ladder then disproves the suspicion (the relaxation shut, so its answer is
    /// the unrelaxed subproblem's), true offers that solution's dual blocks
    /// (`lambda_e`, `lambda_i` and the bound price split into `zl`/`zu`) to the
    /// next entry, graded kBaseWarm; the primal block is not carried, because a
    /// new major is a new linearization. False restarts cold. Only a disproval
    /// seeds: a confirmed infeasibility and a relaxed answer keep the cold path.
    /// Default false.
    bool ipqp_reseed_after_disproval = false;

    /// The kIpm tier's own settings, forwarded verbatim onto the tier exactly
    /// as the SSN levers below are forwarded onto SsnOptions. Inert at
    /// `qp_mode != QpMode::kIpm` and read on every subproblem at kIpm -- see
    /// QpMode::kIpm and IpqpOptions' own doc comments. The one field the
    /// driver narrows rather than forwards is `hoist_symbolic`; see
    /// `SqpSolver::ipqp_options`.
    IpqpOptions ipqp;

    /// Read the proximal carry off an ingested SqpWarmStart.
    ///
    /// It gates one thing: whether the first SSN subproblem of a solve starts its
    /// proximal ladder at `SqpWarmStart::prox_sigma` instead of at 0. The emission
    /// side is unconditional and this flag does not touch it, so a caller can
    /// read the carry without first turning it on. Inert in both directions under
    /// `qp_mode == QpMode::kWalk`. Default false.
    bool ssn_prox_carry = false;

    /// Read the certifying exit'S Second-order evidence off the face-EQP
    /// Factorization the tier-3 Refinement already pays for.
    ///
    /// At false (the default) a certifying SSN subproblem pays TWO factorizations
    /// beyond its Newton steps: ssn_engine.h's own second-order verification and
    /// QpEngine::refine_on_face's exact face solve. Those two factorize different
    /// matrices but answer the same question, and refine_on_face already refuses
    /// on a face whose KKT system fails the inertia gate -- so on every ACCEPTED
    /// refinement the first is redundant.
    ///
    /// At true the kernel runs with SsnOptions::defer_certification: the
    /// verification attempt is built but not factorized, the face solve runs, and
    /// an accepted refinement IS the certificate, saving exactly one
    /// factorization; a REFUSED refinement falls back to the deferred
    /// verification at exactly the shipped cost and on exactly the shipped matrix.
    ///
    /// The saving is predictable to the unit: total factorizations drop by
    /// `SqpCounters::ssn.ssn_refinements` plus the driver's own trust-region gate
    /// refusals, and nothing else moves. Inert in both directions at kWalk.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian under kExact; under
    /// SqpHessianMode::kLbfgsDamped the PD quasi-Newton B, so this certifies B, not the NLP.
    bool ssn_certify_from_face = false;

    /// The three SSN rule levers. Forwarded verbatim onto the SsnOptions every
    /// subproblem is solved with (sqp_solver.h's `ssn_options`), where each
    /// one's mechanism is derived. All three are inert at
    /// `qp_mode == QpMode::kWalk`, and each defaults to the shipped
    /// iteration's own setting.
    SsnSigmaRule ssn_sigma_rule = SsnSigmaRule::kLadder;

    /// How the hinted first SSN step is protected; see SsnHintRule. Same
    /// forwarding and same kWalk inertness as ssn_sigma_rule above.
    SsnHintRule ssn_hint_rule = SsnHintRule::kIterationZeroFree;

    /// What turns an SSN infeasibility suspicion into an exit; see
    /// SsnInfeasibilityRule. Same forwarding and same kWalk inertness.
    SsnInfeasibilityRule ssn_infeasibility_rule = SsnInfeasibilityRule::kSymptoms;

    /// How many subproblem failures IN A ROW the driver may route at one iterate
    /// (shrink the radius, reseed from the failed solve's working set, continue)
    /// before the next one ends the solve. 1 routes the first failure and ends
    /// on the second. Default 3. Must be >= 1.
    ///
    /// The count is cleared by a `kOptimal` subproblem, so the budget is per run
    /// of consecutive failures, not per solve; the run reached is
    /// `SqpCounters::qp_failures_in_a_row_peak` and the total routed
    /// `SqpCounters::qp_failures_routed`. The radius floor still applies to a
    /// routed failure as to a rejection: a shrink below `tr_min` raises a
    /// restoration request at any budget. A failed solve that returned an unusable
    /// iterate ends the solve at any budget. Not a curvature source
    /// (docs/counters.md). A caller who must bound the work per solve bounds it
    /// with `common.max_iterations` and the QP's own cap.
    Index qp_failure_budget = 3;

    /// Re-solve a subproblem that spent its whole minor budget, once, at twice the
    /// cap. Default false: the retry is never attempted and no second engine is
    /// built.
    ///
    /// `QpStatus::kMaxIter` means the subproblem ran out of minors, not that its
    /// arithmetic failed. At true, a subproblem the walk answered with `kMaxIter`
    /// and whose returned iterate is usable is re-solved before it counts as a
    /// failure: the same subproblem and radius, warm from the capped solve's
    /// working set (never its point), with an explicit `QpOptions::max_iter` of
    /// twice the cap that solve resolved. If the retry converges the major
    /// proceeds; if it hits the doubled cap too, the ordinary failure routing
    /// runs. Scoped to the walk: a tier exit under `kSsn`/`kIpm` is left alone,
    /// those tiers carry their own budgets. A retry that does not help costs one
    /// more walk of at most the doubled cap plus its engine's first
    /// factorization, folded into `qp_minor_iters` and `factorizations`;
    /// `SqpCounters::qp_cap_retries` and `qp_maxiter_exits` report it. Not a
    /// curvature source (docs/counters.md).
    bool qp_cap_retry = false;

    /// At true, a failed WALK subproblem's usable iterate p != 0 is judged by the
    /// unchanged strategy as a trial step when it predicts a decrease or halves a
    /// violation above `common.feasibility_tol`. Accepted: taken with the
    /// multipliers kept (the failed solve's are not trusted), no radius growth, no
    /// SOC. Rejected: routed, not charged to `qp_failure_budget`. Certifies
    /// nothing: the KKT test runs on every major as usual. Not consulted in the
    /// full-step mode. False routes every failure without taking its step.
    /// Default true.
    /// @see docs/counters.md (the failed-subproblem step group)
    bool qp_failure_step = true;

    /// The restoration proximity weight `zeta`: the coefficient on the
    /// `1/2 zeta ||D (x - x_k)||^2` term in the restoration phase's feasibility
    /// wrapper objective, `D = diag(d_i)`, `d_i = min(1, 1/|x_{k,i}|)`, `x_k` the
    /// requesting iterate. It prices distance from the point that asked for the
    /// phase, which the violation-only objective does not.
    ///
    /// Default 0 = off. At 0 the term is branched out of `eval_f`, `eval_grad`
    /// and `eval_hess` (not added as zeros), so the objective, the gradient and
    /// the Hessian's sparsity pattern are the violation-only ones.
    ///
    /// At `zeta > 0` the phase can no longer certify infeasibility for free:
    /// `SqpResult::infeasibility_certified` is set only when the explicitly
    /// computed `||J^T lambda - z_sub||_inf <= common.optimality_tol`, so a phase
    /// that moved does not certify. EXPERIMENTAL and uncalibrated: no value is
    /// recommended.
    ///
    /// `validate()` refuses a negative, NaN or non-finite value. Not a curvature
    /// source (docs/counters.md): the `zeta D^2` block is the term's own exact
    /// second derivative.
    double restoration_proximity = 0.0;

    /// Split the equality rows into an independent set B and a set D dependent on
    /// it on the non-fixed columns (`detail/drivers/equality_basis.h`); every QP
    /// then carries only B with a projected rhs, and multipliers stay full-size.
    /// A split that drops nothing leaves every QP byte-identical to off.
    /// `SqpResult::equality_basis` reports what happened. Default true; the split's
    /// linear algebra is the hven::linear backend's.
    bool equality_basis = true;

    /// The elastic walk's feasible start. True starts each elastic walk
    /// ladder at the witness point (p_ref, |r| / sigma), feasible by construction
    /// (`place_elastic_seed_at_witness`, elastic.h), and keeps rung k's x as rung k+1's
    /// start (only g moves, so it stays feasible). False starts every rung at (0, 0),
    /// off its own relaxed rows. Walk rungs only; the tier rung keeps its own chain.
    /// @see docs/counters.md (elastic tier)
    bool elastic_witness_start = false;

    /// The last optimal rung. True: a ladder rung that fails above a
    /// solved rung reports the solved rung (an optimal l1-penalty step at the lower
    /// rho) instead of the failure; `SqpCounters::elastic_last_rung_fallbacks`.
    /// Under the fallback the report's qp_minor_iters / qp_factorizations are the solved
    /// rung's while the counters also fold the failing rung's cost; the fallback counter
    /// reconciles them.
    bool elastic_last_optimal_rung = false;

    /// The sufficient-reduction resume, a fraction in [0, 1); 0 = off (default).
    /// At k > 0 the restoration phase also resumes at a point with
    /// `h_r <= k * restoration_h_in` and its box held, when the sub-solve certified
    /// nothing (kNumericalError, or a kOptimal inside QC-1's no-claim margin).
    /// The funnel restoration exit (KLV Sec. 3) rather than feasibility; such an
    /// entry is counted in `SqpCounters::restoration_reduction_resumes`. `validate()`
    /// refuses a value outside [0, 1) and a NaN.
    double restoration_resume_reduction = 0.0;

    /// Under kLbfgsDamped, also offer the pair of a REJECTED trial (one extra
    /// gradient/Jacobian evaluation per rejection). False (default) offers accepted
    /// pairs only.
    bool qn_update_on_reject = false;

    /// Under kLbfgsDamped, let B act on the NONLINEAR variables only -- those the model's
    /// declared Lagrangian-Hessian pattern touches -- with zero curvature on the rest, as
    /// every Lagrangian Hessian has there (Gill, Murray & Saunders, SIAM Review 47 (2005),
    /// section 2). Falls back to every variable when the pattern touches all of them or
    /// none. Read in no other mode.
    bool qn_nonlinear_only = false;

    /// Under kLbfgsDamped, a FULL-MEMORY damped BFGS B (dense, every pair kept) replaces
    /// the L-BFGS window whenever the quasi-Newton dimension -- the nonlinear variables
    /// under `qn_nonlinear_only`, else every structural variable -- is at most this; it
    /// enters the QP as a dense block of H, with no lift, and is Oren-Luenberger
    /// self-scaled (qn_models.h). 0 = never. Default 400. Read in no other mode. `validate()`
    /// refuses a negative value.
    Index qn_full_memory_max = 400;

    /// The curvature the restoration phase's QPs carry; see SqpRestorationCurvature.
    ///
    /// The feasibility wrapper minimizes the l1 violation, so its multipliers are
    /// subgradient selectors that do not shrink with the violation and the
    /// curvature sum lambda_i hess c_i stays full-sized and indefinite: a
    /// restoration QP may be nonconvex at every major of the phase.
    ///
    /// kRegularized uses B = mu D^2 on the x-block, mu(x) = min(1, h(x)) (a
    /// Levenberg-Marquardt parameter proportional to the residual norm; Yamashita
    /// & Fukushima 2001; Fan & Yuan 2005), with D the proximity term's scaling
    /// when `restoration_proximity > 0`, else diag(min(1, 1/|x_i|)) at the
    /// wrapper's construction point. Every restoration QP is then convex. Only the
    /// model changes: the wrapper's objective, constraints, gradients and the
    /// infeasibility certificate are the same, and the sub-solve runs the
    /// exact-Hessian path whatever `hessian_mode` is. Alone it converges only
    /// linearly, and slowly from far away.
    ///
    /// kModelThenRegularized (the default) runs the phase on the exact model and
    /// switches to kRegularized, from the point it stands at, on the first of two
    /// triggers inside its sub-solve (`SqpCounters::restoration_curvature_switches`):
    ///   1. a walk subproblem's answer (the elastic tier's, where that tier took
    ///      the subproblem) is kMaxIter or kNumericalError;
    ///   2. the elastic tier is exhausted and raises the nested restoration
    ///      request, which is never serviced inside a phase.
    /// The switch restarts the phase only when budget remains and the model is
    /// finite at that point; otherwise the stopped sub-solve's exit stands. A phase
    /// that meets neither trigger is identical to kModel. Under kLbfgsDamped it is
    /// kModel: that phase's quasi-Newton model is convex by construction.
    SqpRestorationCurvature restoration_curvature = SqpRestorationCurvature::kModelThenRegularized;

    /// The number of restoration phases one solve may run. Default 10; must be
    /// >= 1.
    ///
    /// 1 is the once-per-solve rule: a second restoration request is refused and
    /// the solve ends kNumericalError. Above 1 a later request is serviced, as in
    /// the filter and funnel methods (Fletcher & Leyffer, Math. Program. 91 (2002);
    /// [KLV], globalization.h), but only when the main loop has accepted at least one
    /// step since the previous resume. Every phase draws on the same
    /// `common.max_iterations` budget, and each resume re-bases the funnel (KLV
    /// Algorithm 2).
    Index restoration_max_entries = 10;
};

/// @brief The SQP engine's factorization settings.
struct SqpLinearOptions {
    /// When the dense factorization arm is used. RESERVED: the arm is not wired
    /// yet, and `validate()` accepts only `kNever` (the default).
    hven::linear::DensePolicy dense_factorization = hven::linear::DensePolicy::kNever;
    /// The dimension at or below which `DensePolicy::kAuto` takes the dense arm;
    /// empty uses the library's rule. RESERVED: `validate()` accepts only the
    /// empty value until the arm is wired.
    std::optional<Index> dense_max_dimension;
};

/// @brief Driver options for the whole SQP solve.
///
/// TOLERANCES. `common.optimality_tol` gates the stationarity measure and the NLP
/// complementarity, `common.feasibility_tol` the feasibility measure; both
/// measures are defined at `SqpKkt` (sqp_solver.h). `common.feasibility_tol` is
/// also the bound-activity tolerance of the stationarity measure.
///
/// TRUST REGION. `tr_init` is the l-infinity radius the first subproblem is
/// given; the radius then doubles on a strong accepted step up to `tr_max` and
/// halves on a rejected one. A +inf `tr_init` means no trust region, and its
/// first shrink resolves to `tr_max`. `tr_min` is the radius floor, and reaching
/// it is an event rather than a clamp: a shrink that would go below it enters the
/// restoration phase instead.
///
/// ITERATIONS. `common.max_iterations` bounds subproblems solved, not accepted
/// steps, and is shared with the restoration phase: the bounded quantity is
/// major_iters + restoration_iters.
///
/// `qp` is copied into the driver's QP engine at construction, so per-solve
/// variation goes through the solve's own overrides, never through this struct.
struct SqpOptions {
    /// The shared options, at this engine's defaults: `max_iterations` 100,
    /// `threads` 0 (leave the backend alone).
    CommonOptions common;

    /// The l-infinity trust-region radius the FIRST subproblem of a solve is
    /// given. Default 1.0. Must be > 0; +inf is legal and means "no trust
    /// region" (SolveOverrides' own sentinel), though an indefinite
    /// subproblem is then generally unbounded and the engine reports
    /// kNumericalError rather than a step. See the TRUST REGION note above.
    double tr_init = 1.0;

    /// Ceiling the growth rule expands the radius toward, and the value a
    /// +inf tr_init's FIRST shrink resolves to (inf/2 is inf, so the shrink
    /// rule needs a finite landing value the first time it fires). Default
    /// 1e10. Must be >= tr_init, unless tr_init is +inf (then exempt).
    double tr_max = 1e10;

    /// The radius FLOOR. A shrink that would take the radius below this
    /// value does not clamp and re-solve -- it raises a restoration request
    /// instead (KLV Algorithm 4's "alpha < alpha_min" trigger, in its
    /// trust-region form). Default 1e-10, chosen four orders below the
    /// default common.optimality_tol/common.feasibility_tol resolution and far below tr_init so
    /// reaching it is evidence rather than noise -- see the TRUST REGION note above for the
    /// derivation. Must be > 0 and <= both tr_init and tr_max.
    double tr_min = 1e-10;

    /// Copied into the driver's single QpEngine at construction, so
    /// per-solve variation must go through SolveOverrides (sqp_solver.h),
    /// never through this struct -- see qp_types.h's PER-INSTANCE, CONST
    /// note on QpOptions::tr_radius for why. No default beyond QpOptions'
    /// own.
    QpOptions qp;

    /// Which QP kernel the driver's subproblems go through. kWalk is the shipped
    /// primal active-set walk (qp_engine.h) and is the default; kSsn selects the
    /// semismooth-Newton kernel (ssn_engine.h); kIpm selects the interior-point
    /// tier (detail/qp/ipqp_engine.h) -- see QpMode::kIpm's own doc comment above.
    QpMode qp_mode = QpMode::kWalk;

    /// BUDGETED MODE: when true, a solve that exhausts common.max_iterations in the MAIN
    /// optimality loop reports SolveStatus::kMaxIter with returned_best_iterate set,
    /// and x/lambda_e/lambda_i/z/f are the Best iterate this solve visited by the
    /// funnel's own ordering -- feasibility first: min h(x), tie-break min f --
    /// rather than the last iterate reached. warm_start is populated from that
    /// same best iterate for x/lambda_e/lambda_i/z and the ACTIVITY VECTORS only;
    /// its own globalization and regularization fields describe the LAST pass the
    /// solve measured.
    ///
    /// The ordering is not min-||KKT||inf -- the full-step watchdog's ordering --
    /// because the two answer different questions: the watchdog asks which iterate
    /// is closest to a KKT point, while budgeted mode asks what point a
    /// continuation driver should pick up from, where a small stationarity
    /// residual at a large violation is a worse hand-off than a feasible point
    /// with a mediocre objective.
    ///
    /// Default false: a solve that exhausts the cap reports kMaxIter at the last
    /// iterate.
    ///
    /// SCOPE: this lever governs ONLY the main loop's own common.max_iterations exhaustion. It
    /// does not change what happens when the restoration phase runs out of the
    /// shared budget mid-restoration, which stays kMaxIter.
    bool budget_mode = false;

    // --- Problem scaling ---

    /// The problem-scaling layer, OPT-IN. When on, the engine solves a diagonally
    /// rescaled problem -- the objective and each constraint row put into units
    /// taken from the derivatives at the start point -- and maps every exported
    /// quantity back to the caller's units at the export boundary. The declared
    /// problem is never modified.
    ///
    /// Default false.
    ///
    /// When on, the contract differs, not only the performance: the convergence test gates on
    /// the SCALED residuals, while `SqpResult`'s four terminal KKT fields, its
    /// `f`, its multiplier blocks and every history row report CALLER-scale
    /// values. Those two can differ, so a kOptimal solve may report a
    /// caller-scale `kkt_residual` above `common.optimality_tol`; `SqpScalingReport` carries
    /// both the factors and the scaled residual the gate actually read. One
    /// exception, the caller-unit feasibility guard: a kOptimal also
    /// needs each row residual over its row factor to be <= common.feasibility_tol, so its
    /// caller-scale `sqp_feasibility` is within common.feasibility_tol
    /// (SqpCounters::scaled_convergence_refusals counts the refusals).
    bool enable_scaling = false;

    /// The inf-norm the scaled objective gradient and each scaled Jacobian row
    /// are aimed at. Default 100.0, following IPOPT's
    /// `nlp_scaling_max_gradient` in name and value. Must be finite and > 0.
    /// Read only when the row layer runs: `enable_scaling`, or column_scaling kAuto's
    /// engagement of it.
    double scaling_max_gradient = 100.0;

    /// Symmetric clamp on every factor the rule produces: each is confined to
    /// [1/scaling_factor_limit, scaling_factor_limit]. Default 1e12 -- twelve
    /// orders each way. Must be finite and >= 1. Read when the row layer runs
    /// (`enable_scaling`, or column_scaling kAuto's engagement) and by the column rule.
    ///
    /// It is what bounds the damage from the rule being TWO-SIDED
    /// (problem_scaling.h): a start point that is genuinely near-stationary has
    /// its objective gradient AMPLIFIED, and without a ceiling that amplification
    /// is unbounded.
    double scaling_factor_limit = 1e12;

    /// The column (variable) layer of problem scaling. When it fires the
    /// engine solves the problem in the variables x~ = x / d, d the power-of-two column
    /// half of a Ruiz equilibration of [Je; Ji] at the start point
    /// (detail/drivers/column_scaling.h); restoration, the trust region and the funnel
    /// all run in x~, and every exported quantity is mapped back.
    ///
    /// What changes when it fires: the trust region is an l-infinity box in x~, i.e.
    /// |dx_j| <= d_j * radius; the convergence test gates on the scaled stationarity
    /// and, beside it, on the CALLER-unit feasibility (rows and box) <= common.feasibility_tol, so
    /// a kOptimal point is feasible for the problem the caller posed; `SqpResult::x`, `z`, the
    /// multipliers and the terminal residuals are in caller units, re-measured on the caller's
    /// model. History rows, `sqp.major` trace rows and iteration events' scalar diagnostics stay in
    /// the engine's units for the x-dependent columns (stationarity, step_norm, tr_radius; and the
    /// feasibility columns when the row layer is off); an event's x and z are mapped, at depth 1 in
    /// their first n entries. A warm start crosses as x / d, z * d; its radius, regularization, hot
    /// factorization and prox centre are not carried (they are scaled-space state).
    ///
    /// A kOptimal on a column-scaled solve also needs the caller-unit stationarity
    /// <= 1 (IPOPT's unscaled `dual_inf_tol`): a coarse bound against a factor that
    /// hides an O(1) gradient, counted in SqpCounters::scaled_convergence_refusals.
    ///
    /// THE DECISION TRAVELS WITH THE WARM START (SqpWarmStart::column_factors): a
    /// warm start the ingest gate admits and that carries a decision is solved under
    /// it, without the rule; so is a decision of "unscaled". Only the native
    /// SqpWarmStart carries it: the WarmStart payload route and interior-point
    /// crossover warm starts re-read the rule.
    ///
    /// COST: whenever the rule runs, one Jacobian evaluation at the start point,
    /// counted in `evals_values` as the warm-start probe's is (none on a carried
    /// decision); a solve that fires also pays one full evaluation at the end (the
    /// caller-unit re-measurement, `evals_full`), two under kAuto, whose row layer
    /// re-measures too.
    ///
    /// Default kAuto. kAuto's engagement of the row
    /// layer uses the one-sided objective rule (problem_scaling.h,
    /// `amplify_objective`) unless `enable_scaling` is on.
    SqpColumnScaling column_scaling = SqpColumnScaling::kAuto;

    /// kAuto's gate: the column factors' spread (max d / min d over columns with
    /// Jacobian data) at or above which a problem counts as badly scaled. Default
    /// 2^26 = 1/sqrt(machine epsilon), the spread past which the smaller columns'
    /// contributions keep fewer than half of a double's digits. A mesh transcription's
    /// spread grows like its node count (control columns enter every defect row as
    /// O(h)), and 2^26 leaves that growth unscaled to about 4e7 nodes. Must be finite
    /// and >= 1. Read only under kAuto.
    double column_scaling_threshold = 67108864.0;

    /// Project the start point onto the declared box, or take it verbatim.
    /// Default true.
    ///
    /// `solve()` checks `x0`'s size and finiteness, never its box. At true, after
    /// those refusals and before the first evaluation, `x0` is clamped onto the
    /// box (`min(max(x, l), u)`, componentwise), and the number of components that
    /// lay strictly outside it is reported as `SqpCounters::start_point_clamps`. A
    /// point already inside its box is untouched. At false an out-of-box `x0` is
    /// evaluated where it stands, which on a model undefined outside the box
    /// returns `kNumericalError` in zero majors.
    ///
    /// It does not weaken the finiteness refusal (a NaN has no projection) or the
    /// non-finite-iterate exit, and it projects the `x0` argument only: a warm
    /// solve starts at `warm.x`, so on the warm route (including the
    /// multipliers-only seed) the raw point is used. Not a curvature source
    /// (docs/counters.md).
    bool project_start_point = true;

    /// The curvature source; see SqpHessianMode. Under
    /// kLbfgsDamped the model's eval_hess is never called
    /// (`SqpCounters::eval_hess_calls == 0`), `predicted_decrease` reads B, and every
    /// second-order reading an engine takes certifies the model B, not the NLP.
    /// `validate()` refuses kLbfgsDamped with
    /// `restoration_proximity > 0` (the exact zeta D^2 + QN split is not shipped).
    SqpHessianMode hessian_mode = SqpHessianMode::kExact;

    /// The L-BFGS window length m (pairs kept) under kLbfgsDamped; read in no other
    /// mode. `validate()` refuses a value below 1.
    Index qn_memory = 10;

    /// The dense-arm settings (reserved).
    SqpLinearOptions linear;

    /// The research tier. May change in any milestone.
    SqpAdvancedOptions advanced;
};

/// @brief The boundary validation SqpSolver's constructor runs over a whole
///        SqpOptions value.
///
/// Callers other than the driver may use it -- it is the cheapest way for a front
/// end to reject an options object before building a solver around it -- but the
/// driver validates unconditionally, so calling it first is an optimization,
/// never a prerequisite.
///
/// Every predicate is written as the NEGATION of the acceptance condition so that
/// NaN is rejected rather than admitted, which rests on the build's
/// `-fno-finite-math-only`.
///
/// @param opts The options object to validate.
/// @throws std::invalid_argument, naming the option and the value it had, on any
///         option the driver cannot honour: a non-positive or NaN tolerance, a
///         `common.max_iterations` outside [0, INT_MAX], an engaged
///         `max_factorizations` below 0, an engaged `time_limit_seconds` that is
///         not finite and positive, an `infinite_bound` <= 0 or NaN, a negative
///         `common.threads`, an engaged `qp.max_iter` below 1,
///         an out-of-range enum (every enum field is checked), a non-positive or
///         NaN `tr_init`, a `tr_max` below `tr_init` (a +inf `tr_init` is exempt),
///         a `tr_min` that is non-positive or above either end of the range it
///         floors, an ill-formed scaling or IPQP field (validated at every
///         `qp_mode`), a negative, NaN or infinite `restoration_proximity`, a
///         `restoration_resume_reduction` outside [0, 1) or NaN, a `qn_memory`
///         below 1, a negative `qn_full_memory_max`, `kLbfgsDamped` with
///         `restoration_proximity > 0`, a `restoration_max_entries` below 1, a
///         `qp_failure_budget` below 1, or a reserved dense-arm value.
void validate(const SqpOptions &opts);

/// @brief Returns a full SqpOptions value for a named preset.
///
/// "default" is the only name, and it returns a default-constructed value.
///
/// @param name A preset name.
/// @throws std::invalid_argument, listing every valid name, if `name` is not one.
SqpOptions sqp_preset(std::string_view name);

/// @brief Writes EVERY field of @p o to @p os, one `name = value` line each, in
///        DECLARATION order.
///
/// The format is the one stated at `dump_options(const IpmOptions &,
/// std::ostream &)` (drivers/ipm_options.h). Nested blocks print under their
/// member names (`common.`, `qp.`, `linear.`, `advanced.`, `advanced.ipqp.`).
/// `advanced.make_strategy` prints `set` or `unset`.
///
/// @param o The options to write.
/// @param os The stream, borrowed: this call neither flushes nor closes it.
void dump_options(const SqpOptions &o, std::ostream &os);

} // namespace hven::solvers
