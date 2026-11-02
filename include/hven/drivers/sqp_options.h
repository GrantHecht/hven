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

/// @brief A2 (M7-37): how the IPQP tier's solve-entry cold start sets `mu_0`. Semantics at
/// `IpqpOptions::mu0_rule`; design docs/notes/2026-10-m7-w2-a2-mu0-design.md section 3.
///
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
    /// floor thereafter. Matches `Settings::min_mu_`'s own default
    /// (`ipm_solver.h:366`) so the two barrier engines agree on
    /// how low `mu` is ever allowed to go. Default 1e-12. Must be finite,
    /// > 0, and <= `init_mu`.
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
    /// cross, matching `detail::kProxRegFloor`
    /// (`inertia_regularization.h:46`, Cipolla-Gondzio eq. 19). Default
    /// 1e-10. Must be finite and > 0.
    double reg_floor = 1e-10;

    /// The ceiling the (rho, delta) ladder's inertia-demanded growth may
    /// never cross, matching `detail::kSsnProxMax` (`ssn_engine.h:576`) --
    /// the tier inherits SSN's relative cap-slack exhaustion guard
    /// (`detail::kSsnProxCapSlack`, `ssn_engine.h:577`) along with the
    /// constant, not just the number. Default 1e6. Must be finite and >=
    /// `reg_floor`.
    double reg_max = 1e6;

    /// The (rho, delta) ladder's GATED decrease factor: applied only after
    /// the regularized residuals have contracted by the required amount
    /// (spec 3.2), never unconditionally the way `prox_reg_decay`'s geometric
    /// decay is -- this is a fraction the schedule multiplies the current
    /// value by, so it must lie strictly inside (0, 1): 0 would collapse the
    /// regularization in one gated step and >= 1 would never decrease it.
    /// Default 0.1.
    double reg_decrease = 0.1;

    /// The fraction-to-boundary parameter (`tau`/`bfrac`) both the affine
    /// and corrector steps are clipped by (spec 3.1). A standard IPM
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

    /// The barrier-phase convergence target's SLACK factor (spec 2.3 step 1
    /// / 3.4): the tier converges to `converge_slack x` the QP layer's
    /// own relative tolerances, deliberately LOOSER than the 1e-10 tier-3
    /// exact refinement owns the last two decades of. Default 100. Must be
    /// finite and >= 1 -- a slack below 1 would ask the barrier phase for
    /// MORE accuracy than tier 3's own finish, inverting the division of
    /// labor the two-tier design rests on.
    double converge_slack = 1e2;

    /// Ruiz equilibration (spec 4.3) inside the tier, unscaled on export.
    /// Default true. A plain A/B lever; every asserted pin is on unscaled
    /// quantities either way.
    bool ruiz = true;

    /// The warm-seed repair (spec 5.2): strict-positivity clamps plus the
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

    /// `kappa_mu` in the section 5.3 `mu_0` clamp rule: `mu_0 =
    /// clamp(max(mu_meas, mu_adopt_factor * mu_payload), min_mu,
    /// init_mu)`. 0 DISABLES ADOPTION (the clamp then reads only
    /// `mu_meas`), which is a deliberate, documented reading of this field
    /// rather than a degenerate one, so it stays legal. Default 1.0. Must be
    /// finite and >= 0.
    double mu_adopt_factor = 1.0;

    /// The early-stall detector's (spec 6.2) window width in ACCEPTED
    /// iterations, matching `detail::kSsnStallWindow` (`ssn_engine.h:641`)
    /// for the reason its own banner gives: an order of magnitude above the
    /// local regime, so no healthy trajectory reaches it. Default 5. Must be
    /// > 0 -- a zero-width window can never accumulate the whole-window
    /// evidence the test is built on.
    Index stall_window = 5;

    /// The escape ladder's retirement threshold (spec 6.1): after this many
    /// CONSECUTIVE escapes within one SQP solve the tier is retired for the
    /// remainder of that solve; any success resets the count. Default 3.
    /// Must be > 0 -- retiring "after zero consecutive escapes" is not a
    /// count, it is disabling the tier outright, which this field does not
    /// exist to express.
    Index retire_after = 3;

    /// Optional Farkas corroboration (spec 6.3) of an infeasible-suspect
    /// escape: one matvec plus O(m), no factorization. It may ARM the
    /// evidence block; it never certifies. Default true.
    bool farkas_gate = true;

    /// Cross-major symbolic reuse (spec 4.1) kill switch: while true, the
    /// tier hoists its analysis/verify work across majors whose
    /// `AssemblyEvalSeam::epoch()` is unchanged.
    /// Default true; false forces a fresh symbolic pass every major.
    bool hoist_symbolic = true;

    /// The required final unregularized inertia read (spec 2.2 item 4): when
    /// true, a certifying exit pays one extra
    /// unregularized factorization to confirm the inertia the regularized
    /// solve reported; a WRONG read downgrades the certificate rather than
    /// reporting `kOptimal`. Default true. FALSE means every certificate is
    /// downgraded unconditionally -- an explicit, documented weakening for a
    /// caller who wants to skip the extra factorization, never a silent one.
    /// CURVATURE SOURCE: the subproblem Hessian -- the exact model Hessian under kExact; under
    /// SqpHessianMode::kLbfgsDamped the PD quasi-Newton B, so this certifies B, not the NLP.
    bool require_final_inertia = true;

    /// How the solve-entry cold start sets `mu_0` (A2, M7-37). kFixed (default): every pair at
    /// `init_mu`. kComplementarity: `mu_0` is derived from one affine-scaling solve; it
    /// fires only when the derived value lies in `[min_mu, init_mu]`, on a cold
    /// solve-entry only, otherwise kFixed. The estimate is charged to the factorization budget.
    /// Counters: `ipqp_mu0_derived`, `ipqp_mu0_fallbacks`. Design:
    /// docs/notes/2026-10-m7-w2-a2-mu0-design.md.
    IpqpMu0Rule mu0_rule = IpqpMu0Rule::kFixed;

    /// The residual-proportional `mu` floor (A2, carrying A1-R1). Default false: the corrector
    /// target is `clamp(sigma * mu, min_mu, init_mu)`. True: `sigma * mu` is first
    /// raised to `mu_s * r / max(1, r_s)`, `r` the unregularized relative residual and
    /// `(mu_s, r_s)` the attempt's first iterate. Counter: `ipqp_mu_floor_binds`. A bool
    /// refuses no value, so `validate` has no predicate for it. Design: the A2 page section 4.
    bool mu_residual_floor = false;

    /// Algorithm IC's zero trial (IC-1) is skipped once this many CONSECUTIVE iterations have
    /// needed a modification (A3, M7-38; hven's adaptation (ii) of Wachter-Biegler 2006). Once
    /// skipped it is not re-offered within the attempt. Default 3. 0 skips whenever the ladder
    /// has a memory; a value above the attempt's iteration count never skips. Must be >= 0.
    /// Counter: `ipqp_ladder_zero_trial_skips`. Design:
    /// docs/notes/2026-10-m7-w2-a3-ipqp-levers-design.md. CURVATURE SOURCE: the subproblem Hessian
    /// -- the exact model Hessian under kExact; under SqpHessianMode::kLbfgsDamped the PD
    /// quasi-Newton B, so this reads B, not the NLP.
    Index ladder_skip_after = 3;
};

/// @brief Which QP kernel solves the elastic ladder's rungs (M7 W0 T3).
///
/// `kWalk` is the shipped active-set ladder, hot-chained rung to rung; `kTier`
/// solves each rung with the interior-point kernel on a DEDICATED engine
/// instance.
enum class ElasticRungEngine { kWalk, kTier };

/// @brief The curvature source of the SQP subproblems (M7 QN stage 3).
///
/// `kExact` evaluates the model's Lagrangian Hessian at every subproblem build.
/// `kLbfgsDamped` never calls eval_hess: the subproblem curvature is a damped
/// compact L-BFGS approximation B (detail/globalization/sqp/lbfgs_damped.h),
/// positive definite by construction, carried into every QP engine call through
/// the mixed lift (mixed_lift.h).
/// @see .superpowers/m7-qn-design.md sections 2-3
enum class SqpHessianMode { kExact, kLbfgsDamped };

/// @brief The VARIABLE (column) half of problem scaling (M7 W2 SCL); see
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

/// @brief The CURVATURE the restoration phase's QPs carry (M7 W2 RST); see
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
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_solver_types.h
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
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_solver_types.h
    bool crash_basis = false;

    /// WHICH ENGINE SOLVES THE ELASTIC RUNG (M7 W0 T3).
    ///
    /// Rung A -- the elastic ladder -- runs on the WALK by default, which is
    /// where M6 W2 left it and why (that plan's section 3: the walk hot-chains
    /// rung to rung, and a tier-solved rung costs a second symbolic analysis).
    /// `kTier` hands each rung to the interior-point kernel instead, on a
    /// DEDICATED `IpqpEngine` instance, so the primary tier's
    /// one-analysis-per-solve hoist (pin A9) is untouched.
    ///
    /// MODE-INDEPENDENT, deliberately: the ladder is reachable from the walk
    /// route's own kInfeasible consumer in EVERY mode, not only from the kIpm
    /// arm's certified fallback, so gating this on `qp_mode` would make one
    /// field mean different things in different modes.
    ///
    /// DEFAULT kWalk, and that default is a measurement result rather than a
    /// placeholder: M7 W0 T3 measured both states on a CONSTRUCTED family, and
    /// a win there is not the win on the suite's TRAIN split that the M7 exit
    /// criterion asks for. W2 re-reads the default on TRAIN.
    /// @see docs/notes/2026-09-m7-w0-t3-design.md
    ElasticRungEngine elastic_rung_engine = ElasticRungEngine::kWalk;

    /// ON A DISPROVAL, THE FALLBACK'S CERTIFIED SOLUTION SEEDS THE TIER'S NEXT
    /// ENTRY (M7 W0 T4).
    ///
    /// The escape drops the tier's cross-major carry unconditionally (section
    /// 6.1's rule: a point the tier could not certify does not seed the next
    /// major). When rung A then DISPROVES the suspicion -- the relaxation shut,
    /// so its answer IS the unrelaxed subproblem's -- the carry is already gone
    /// and the next entry restarts cold, although a certified solution with
    /// carried multipliers is sitting in the caller's hand. True offers that
    /// solution's DUAL blocks to the next entry through the existing staged-seed
    /// slot; false is today's cold restart.
    ///
    /// ONLY ON A DISPROVAL. A confirmed infeasibility (the exhaustion
    /// certificate) and a relaxed answer both keep the cold path: neither is a
    /// solution of the subproblem the tier was asked about.
    ///
    /// WHAT CARRIES IS THE DUAL BLOCKS ONLY -- `lambda_e`, `lambda_i` and the
    /// signed bound price split into `zl`/`zu` -- exactly as the engine's own
    /// cross-major carry does (spec 5.1 amendment D: across a major the duals
    /// carry, the primal block does not). `x`, `s` and `zeta` are zeroed because
    /// a new major is a new linearization in a new step space, and `IpqpBox`
    /// re-centres the window at every entry. The grade is kBaseWarm, the grade a
    /// signed bound price alone buys.
    ///
    /// DEFAULT FALSE: measured on a constructed cell only, which is not the
    /// TRAIN win the M7 exit criterion asks for. The measurement, the reason the
    /// cell had to be constructed, and the caveat that this MITIGATES a section
    /// 6.3 false positive rather than fixing one are all in the artifact.
    /// @see docs/notes/data/2026-09-m7-w0-t4-reseed/
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
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_solver_types.h
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

    /// THE CONSECUTIVE-SUBPROBLEM-FAILURE BUDGET (M7 W2 M1).
    ///
    /// How many failures IN A ROW the driver may route -- shrink the radius,
    /// reseed from the failed solve's own working set and continue -- at one
    /// iterate before the next one ends the solve. A subproblem that fails but
    /// hands back a usable iterate is a property of THAT SUBPROBLEM AT THAT
    /// RADIUS, and the same subproblem at half the radius is a different
    /// problem; this field says how many times the driver may make that
    /// argument before it stops.
    ///
    /// DEFAULT 3, AND IT IS MEASURED RATHER THAN PRESUMED. The gate was
    /// `qp_failures_in_a_row == 0` and is now
    /// `qp_failures_in_a_row < qp_failure_budget`, so **1 IS THIS ENGINE'S
    /// BEHAVIOUR BEFORE THE FIELD EXISTED** -- the first failure at an iterate
    /// routed, the second ending the solve -- and setting it to 1 restores that
    /// engine exactly, byte for byte. 3 is what the CUTEst TRAIN 23 reading
    /// chose over 1 by the M7 rule, on its own arm, run twice:
    /// `docs/notes/data/2026-09-m7-w2-m1-qp-cap/`. One more cell converges
    /// (`READING6`, to the objective the IPM and SNOPT lanes reach), no new cell
    /// errors, and the only cells whose counters move at all are the nine that
    /// fail a subproblem.
    ///
    /// WHAT THE THIRD FAILURE COSTS WHEN IT DOES NOT HELP: two more majors on a
    /// cell that was going to fail anyway, and their subproblems are the
    /// expensive ones. On that TRAIN split `BRITGAS` went from 9 265 to 93 749
    /// factorizations and, informationally, from 22 s to 275 s of a 300 s cell
    /// ceiling. A caller who must bound the work per solve bounds it with
    /// `common.max_iterations` and the QP's own cap, or sets this back to 1.
    ///
    /// Must be >= 1: a budget of 0 would refuse the first retry this engine has
    /// always taken, which is a behaviour change nobody asked for spelled as a
    /// validation value -- `validate()` refuses it.
    ///
    /// The counter is cleared by a `kOptimal` subproblem, so the budget is per
    /// RUN of consecutive failures, not per solve; the run this solve reached is
    /// `SqpCounters::qp_failures_in_a_row_peak` and the total routed is
    /// `SqpCounters::qp_failures_routed`.
    ///
    /// WHAT IT DOES NOT CHANGE. The radius floor still applies to a routed
    /// failure exactly as to a judged rejection: a shrink that would cross
    /// `tr_min` raises a restoration request instead, at any budget, so raising
    /// this does not let the radius walk below its floor. The retryability test
    /// is unchanged (a failed solve that returned an unusable iterate ends the
    /// solve at any budget), and `rejected_steps` still counts every routed
    /// failure.
    ///
    /// NOT a curvature source (docs/counters.md): it selects how many times a
    /// subproblem is re-posed, not whether or how a second-order reading is
    /// taken.
    /// @see docs/notes/2026-09-m7-w2-sqp-cutest-read.md section M1
    Index qp_failure_budget = 3;

    /// THE CAP RETRY (M7 W2 M1): re-solve a subproblem that spent its whole
    /// minor budget, ONCE, at twice the cap.
    ///
    /// `QpStatus::kMaxIter` means the subproblem ran out of MINORS, not that its
    /// arithmetic failed -- and the driver reports both as
    /// `SolveStatus::kNumericalError`. At true, a subproblem the WALK answered
    /// with `kMaxIter`, whose returned iterate is usable
    /// (`qp_failure_is_retryable`), is re-solved ONCE before it is treated as a
    /// failure at all: the same subproblem, the same trust-region radius, warm
    /// from the capped solve's own WORKING SET, with an explicit
    /// `QpOptions::max_iter` of twice the cap that solve resolved. If the retry
    /// converges the major proceeds as though the subproblem had simply taken
    /// longer; if it hits the doubled cap too, the ordinary failure routing runs
    /// on it, and a second capped subproblem is a failure exactly as today.
    ///
    /// DEFAULT FALSE, and off means the retry is never attempted and no second
    /// engine is constructed -- the solve is byte-identical to the same solve
    /// before this field existed.
    ///
    /// THE SEED IS THE WORKING SET, NOT THE POINT, and that is the engine's
    /// contract rather than a choice: `QpEngine` centres the trust-region window
    /// on the seed's primal, and every seeding site in this driver zeroes
    /// `seed.x` for that reason -- carrying the capped solve's step would
    /// re-centre the region on it and hand the retry a larger feasible set than
    /// the major was given. What the retry inherits is the working set the
    /// capped walk had to walk to, which is the information the cap denied it
    /// the minors to use.
    ///
    /// SCOPED TO THE WALK. It fires only on a subproblem the walk itself
    /// answered, so at the shipped `qp_mode == QpMode::kWalk` it covers every
    /// subproblem, and under `kSsn`/`kIpm` a tier exit is left alone: the tiers
    /// carry their own budgets (`IpqpOptions::max_iterations`,
    /// `hard_iteration_cap`), and re-solving a tier's subproblem on the walk
    /// would change the kernel that answered the major, not its budget.
    ///
    /// WHAT IT COSTS WHEN IT DOES NOT HELP: one more walk of at most the cap
    /// again, folded into `qp_minor_iters` and `factorizations` like any other
    /// subproblem's, plus the retry engine's own first factorization -- it has
    /// no border-stack history of its own. `SqpCounters::qp_cap_retries` counts
    /// the attempts and `qp_maxiter_exits` counts both exits, so the cost is
    /// readable from the result.
    ///
    /// It is a BOOL with no refusable value, so `validate()` carries no
    /// predicate for it: an exception that cannot be thrown is dead code
    /// (CLAUDE.md section 4). Both states are accepted, and the dump carries the
    /// line.
    ///
    /// NOT a curvature source (docs/counters.md): it selects how many minors a
    /// subproblem may spend, not whether or how a second-order reading is taken.
    /// @see docs/notes/2026-09-m7-w2-sqp-cutest-read.md section M1
    bool qp_cap_retry = false;

    /// THE FAILED SUBPROBLEM'S OWN STEP (M7 W2 R2). At true, a failed WALK
    /// subproblem's usable iterate p != 0 is judged by the unchanged strategy as a
    /// trial step when it predicts a decrease or halves a violation above
    /// `common.feasibility_tol`. Accepted: taken with the multipliers kept (the failed solve's
    /// are not trusted), no radius growth, no SOC. Rejected: routed, not charged
    /// to `qp_failure_budget`. Certifies nothing: a step taken here is never itself an
    /// optimality claim; the KKT test (complementarity conjunct included) runs on
    /// every major as usual. Not consulted in
    /// the full-step mode; off routes every failure as before R2.
    /// @see docs/counters.md (the failed-subproblem step group)
    bool qp_failure_step = true;

    /// THE RESTORATION PROXIMITY WEIGHT (M7 W2 M2b), `zeta` -- the coefficient
    /// on the `1/2 zeta ||D (x - x_k)||^2` term the restoration phase's
    /// feasibility wrapper carries in its OBJECTIVE, with `D = diag(d_i)`,
    /// `d_i = min(1, 1/|x_{k,i}|)` and `x_k` the REQUESTING ITERATE.
    ///
    /// WHAT IT IS FOR. The feasibility wrapper's objective is the violation
    /// alone, with NO x-block at all, so nothing in the sub-solve prices
    /// distance from the point that asked for the phase: measured, the
    /// restoration sub-solve walks `||x - x_k||_inf` from 0.13 to 9 999.9 while
    /// its own trust radius doubles fourteen times, and returns a point inside
    /// `common.feasibility_tol` at an objective of 1.8e14
    /// (`docs/notes/data/2026-09-m7-w2-m2-restoration/ladder/STEENBRC.txt`).
    /// This term is the price. It is transcribed from the interior-point lane's
    /// NESTED l1 restoration -- `detail/globalization/l1_restoration.h`,
    /// `rho*sum(n+p) + (eta(mu)/2)*|D_R (x - x_R)|^2` with
    /// `rho = kRestoPenaltyParameter = 1e3` and `eta = sqrt(mu)` -- which is the
    /// right analogue because the SQP wrapper IS an l1 elastic feasibility
    /// problem. The IPM lane's reading of the same term is a WEAK regulariser,
    /// three orders below the violation term.
    ///
    /// DEFAULT 0.0 = OFF, AND OFF IS A CODE SHAPE RATHER THAN AN ARITHMETIC
    /// IDENTITY. At 0 the wrapper BRANCHES the term out of `eval_f`,
    /// `eval_grad` and `eval_hess` -- it does not add zeros -- so the objective,
    /// the gradient AND the augmented Hessian's sparsity pattern are the ones
    /// that shipped before this field existed, which is what keeps the
    /// sub-solve's symbolic analysis where it was. `detail/globalization/sqp/
    /// restoration.h` states the same thing where the branch is.
    ///
    /// AT `zeta > 0` IT COSTS THE INFEASIBILITY CERTIFICATE, by design and not
    /// as a side effect. `SqpResult::infeasibility_certified` is then set only
    /// when the EXPLICITLY computed `||J^T lambda - z_sub||_inf <= common.optimality_tol` --
    /// the gap between "stationary for h + P" and "stationary for h" -- so a
    /// phase that MOVED does not certify. See that field's own note for why the
    /// cheaper `zeta ||D^2 (x_r - x_k)||_inf` test is NOT used.
    ///
    /// EXPERIMENTAL, AND UNCALIBRATED. No calibrated value exists: the
    /// pre-registered candidate set was `{h_k/(n*rho), h_k/rho, h_k/n, h_k}` and
    /// EVERY member failed the TRAIN bar -- the term freezes the restoration
    /// phase rather than steering it -- so the default stayed 0 and no default
    /// event followed. The rule, the 35 legs and the ruling are
    /// `docs/notes/data/2026-09-m7-w2-m2b-proximity/`; the design page is
    /// `docs/notes/2026-09-m7-w2-m2-restoration-design.md` (section 3 for the
    /// term, 3.4 for the certificate, 3.5 for the calibration, "Outcome (M2b)"
    /// for what was measured). Whether this field stays public is decided at
    /// M3's close, on M3's evidence. A caller that sets it is running an
    /// uncalibrated, experimental lever.
    ///
    /// `validate()` refuses a negative value and a NaN (the `!(x >= 0)` form,
    /// which catches both) and refuses a non-finite one: `+inf` would make the
    /// wrapper's objective `+inf` at every point but the centre.
    ///
    /// NOT a curvature source (docs/counters.md): it adds a term to an
    /// OBJECTIVE, and the `zeta D^2` block it puts on the wrapper's Hessian is
    /// that term's own exact second derivative -- it selects neither whether nor
    /// how any second-order reading is taken.
    /// @see docs/notes/2026-09-m7-w2-m2-restoration-design.md section 3
    double restoration_proximity = 0.0;

    /// Split the equality rows into an independent set B and a set D dependent on
    /// it on the non-fixed columns (`detail/drivers/equality_basis.h`); every QP
    /// then carries only B with a projected rhs, and multipliers stay full-size.
    /// A split that drops nothing leaves every QP byte-identical to off.
    /// `SqpResult::equality_basis` reports what happened. ON by default since M7 W2
    /// E1b (the split's linear algebra is the hven::linear backend's).
    /// @see docs/notes/2026-09-22-sqp-degeneracy-proposal.md
    bool equality_basis = true;

    /// THE ELASTIC WALK'S FEASIBLE START (M7 W2 P1E). True starts each elastic walk
    /// ladder at the witness point (p_ref, |r| / sigma), feasible by construction
    /// (`place_elastic_seed_at_witness`, elastic.h), and keeps rung k's x as rung k+1's
    /// start (only g moves, so it stays feasible). False starts every rung at (0, 0),
    /// off its own relaxed rows. Walk rungs only; the tier rung keeps its own chain.
    /// @see docs/counters.md (elastic tier)
    bool elastic_witness_start = false;

    /// THE LAST OPTIMAL RUNG (M7 W2 P1E). True: a ladder rung that fails above a
    /// solved rung reports the solved rung (an optimal l1-penalty step at the lower
    /// rho) instead of the failure; `SqpCounters::elastic_last_rung_fallbacks`.
    /// Under the fallback the report's qp_minor_iters / qp_factorizations are the solved
    /// rung's while the counters also fold the failing rung's cost; the fallback counter
    /// reconciles them.
    bool elastic_last_optimal_rung = false;

    /// THE SUFFICIENT-REDUCTION RESUME (M7 W2 P1E), a fraction in [0, 1); 0 = OFF.
    /// At k > 0 the restoration phase also resumes at a point with
    /// `h_r <= k * restoration_h_in` and its box held, when the sub-solve certified
    /// nothing (kNumericalError, or a kOptimal inside QC-1's no-claim margin).
    /// The funnel restoration exit (KLV Sec. 3) rather than feasibility; such an
    /// entry is counted in `SqpCounters::restoration_reduction_resumes`. `validate()`
    /// refuses a value outside [0, 1) and a NaN.
    double restoration_resume_reduction = 0.0;

    /// Under kLbfgsDamped, also offer the pair of a REJECTED trial (one extra
    /// gradient/Jacobian evaluation per rejection). False is the shipped
    /// accepted-only rule (ruling C30 AMEND-4); true is the ablation.
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
    /// self-scaled (qn_models.h). 0 = never. 400 (the M7 QN quality round's measured
    /// cut: the dense block won on TRAIN up to n = 344 and cost more than it gained at
    /// 450-540). Read in no other mode. `validate()` refuses a negative value.
    Index qn_full_memory_max = 400;

    /// THE RESTORATION PHASE'S CURVATURE (M7 W2 RST); see SqpRestorationCurvature.
    ///
    /// WHY NOT THE EXACT HESSIAN. The feasibility wrapper minimizes the l1 violation, so
    /// its multipliers are subgradient selectors of size sigma that do NOT shrink as the
    /// violation goes to zero, and the curvature sum lambda_i hess c_i stays full-sized
    /// and indefinite all the way to the restored point: a restoration QP may be
    /// nonconvex at every major of the phase.
    ///
    /// WHAT kRegularized DOES. B = mu D^2 on the x-block, mu(x) = min(1, h(x)) -- the
    /// Levenberg-Marquardt parameter proportional to the residual norm (Yamashita &
    /// Fukushima 2001; Fan & Yuan 2005). D is the proximity term's scaling when
    /// `restoration_proximity > 0` (read at its centre, the requesting iterate), else
    /// diag(min(1, 1/|x_i|)) at the wrapper's construction point: the phase's start, or
    /// the switch point on a phase restarted under kModelThenRegularized. Every
    /// restoration QP is then convex. Only the MODEL changes: the wrapper's objective,
    /// constraints, gradients and the infeasibility certificate (first-order quantities)
    /// are the same, and the phase's sub-solve runs the exact-Hessian path (eval_hess
    /// supplies B) whatever `hessian_mode` is. The final certification stays on the
    /// caller's problem. Alone it converges only linearly to a nonzero-residual
    /// stationary point of h (the certified infeasible case), and slowly from far away.
    ///
    /// WHAT kModelThenRegularized DOES (the default). The phase runs on the exact model
    /// and switches to kRegularized, from the point it stands at, on the FIRST of two
    /// triggers inside its sub-solve (`SqpCounters::restoration_curvature_switches`):
    ///   1. a walk subproblem's answer -- the elastic tier's, where that tier took the
    ///      subproblem -- is kMaxIter or kNumericalError (where the sub-solve would
    ///      otherwise route the failure itself, by shrinking its radius);
    ///   2. the elastic tier -- which keeps its place first for an infeasible subproblem
    ///      and for an E2-class or certificate-refusal exit at a violated linearization --
    ///      is exhausted and raises the nested restoration request, which is never
    ///      serviced inside a phase.
    /// The switch restarts the phase only when budget remains and the model is finite at
    /// that point; otherwise the stopped sub-solve's own exit stands. A phase that meets
    /// neither trigger is byte-identical to kModel. Under kLbfgsDamped it is kModel: that
    /// phase's QN model is convex by construction. Measurements:
    /// docs/notes/data/2026-10-m7-rst/.
    SqpRestorationCurvature restoration_curvature = SqpRestorationCurvature::kModelThenRegularized;

    /// THE NUMBER OF RESTORATION PHASES ONE SOLVE MAY RUN (M7 W2 RST); >= 1.
    ///
    /// 1 is the once-per-solve rule: a second restoration request is refused and the
    /// solve ends kNumericalError. Above 1 a later request is serviced, as the filter
    /// and funnel methods' restoration is (Fletcher & Leyffer, Math. Program. 91 (2002);
    /// [KLV], globalization.h), but only when the main loop has ACCEPTED at least one step
    /// since the previous resume -- a request from the point the last phase handed back,
    /// with nothing done there, is refused as before. Every phase draws on the same
    /// `common.max_iterations` budget, and each resume re-bases the funnel (KLV Algorithm 2). 10 is
    /// a safety cap, not a tuned value (docs/notes/data/2026-10-m7-rst/). `validate()` refuses a
    /// value below 1.
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
    /// optimality loop reports SolveStatus::kBudgetExhausted rather than kMaxIter,
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
    /// DEFAULT FALSE, and off reproduces plain kMaxIter-at-the-last-iterate
    /// behaviour byte-identically.
    ///
    /// SCOPE: this lever governs ONLY the main loop's own common.max_iterations exhaustion. It
    /// does not change what happens when the restoration phase runs out of the
    /// shared budget mid-restoration, which stays kMaxIter because it answers a
    /// different question.
    bool budget_mode = false;

    // --- Problem scaling ---

    /// The problem-scaling layer, OPT-IN. When on, the engine solves a diagonally
    /// rescaled problem -- the objective and each constraint row put into units
    /// taken from the derivatives at the start point -- and maps every exported
    /// quantity back to the caller's units at the export boundary. The declared
    /// problem is never modified.
    ///
    /// DEFAULT FALSE, and off means arithmetically untouched.
    ///
    /// What changes when IT IS ON, stated plainly because it is a contract
    /// difference and not only a performance one: the CONVERGENCE TEST gates on
    /// the SCALED residuals, while `SqpResult`'s four terminal KKT fields, its
    /// `f`, its multiplier blocks and every history row report CALLER-scale
    /// values. Those two can differ, so a kOptimal solve may report a
    /// caller-scale `kkt_residual` above `common.optimality_tol`; `SqpScalingReport` carries
    /// both the factors and the scaled residual the gate actually read. One
    /// exception, the caller-unit feasibility guard (M7 W2 SCL): a kOptimal also
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

    /// THE COLUMN (VARIABLE) LAYER of problem scaling (M7 W2 SCL). When it fires the
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
    /// SqpWarmStart carries it: the WarmStartData payload route and interior-point
    /// crossover warm starts re-read the rule.
    ///
    /// COST: whenever the rule runs, one Jacobian evaluation at the start point,
    /// counted in `evals_values` as the warm-start probe's is (none on a carried
    /// decision); a solve that fires also pays one full evaluation at the end (the
    /// caller-unit re-measurement, `evals_full`), two under kAuto, whose row layer
    /// re-measures too.
    ///
    /// DEFAULT kAuto (M7 W2 SCW, a declared event). kAuto's engagement of the row
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

    /// THE OUT-OF-BOX START POINT: project it, or take it verbatim (M7 W2 M4).
    ///
    /// DEFAULT TRUE since the M7 W2 ladder round (L2 = proposal E5; E2-D3 found
    /// YORKNET's and SAWPATH's CUTEst x0 outside their own box). The dump line
    /// moved with it -- a declared event.
    ///
    /// `solve()` checks `x0`'s SIZE and its FINITENESS and has never checked its
    /// BOX. FALSE -- the default before the ladder round -- therefore means the start
    /// point is TAKEN VERBATIM: an out-of-box `x0` is not refused, it is
    /// evaluated where it stands. On a model whose objective or constraints are
    /// undefined outside the box that first evaluation is non-finite, and the
    /// solve RETURNS a `kNumericalError` status in zero majors -- the NON-FINITE
    /// ITERATE exit, doing exactly what it was written to do, on a point no rule
    /// of the problem admits. It is a reported status, not a boundary refusal,
    /// which is why it reads to every consumer as "the solver failed".
    ///
    /// TRUE: after the size and finiteness refusals and BEFORE the first
    /// evaluation, `x0` is clamped onto the declared box -- `min(max(x, l), u)`,
    /// componentwise, which is the rule `NlpProblemModel::start_point()` already
    /// applies to the origin -- and the number of components that lay STRICTLY
    /// OUTSIDE the box is reported as `SqpCounters::start_point_clamps` (that
    /// counter's own comment says why it is defined on the POINT rather than on
    /// the repair; on this engine's clamp the two counts coincide). A fixed variable
    /// (`l == u`) lands exactly on its value; an infinite bound moves nothing;
    /// a point already inside its box is untouched, component for component, and
    /// the solve is byte-identical to the same solve with this field false.
    ///
    /// WHAT IT DOES NOT DO. It does not weaken the finiteness refusal -- an `x0`
    /// with a NaN or an infinity is still `std::invalid_argument`, because a NaN
    /// has no projection. It does not touch the NON-FINITE ITERATE exit, which
    /// keeps the case its own comment names: a point that is IN the box and still
    /// un-evaluable. And it never moves a point the caller placed inside its
    /// bounds, so it changes no solve that was well-posed to begin with.
    ///
    /// SCOPE -- IT PROJECTS THE `x0` ARGUMENT, and a WARM solve's start point is
    /// not that argument. `prepare_solve` starts a solve at `warm.x` whenever a
    /// warm object is ingested and at `x0` otherwise, so on the warm route the
    /// point projected here is not the point the loop begins from. That includes
    /// the MULTIPLIERS-ONLY seed, whose `warm.x` is a copy of `x0` taken one
    /// frame above this projection -- so a caller combining that payload form
    /// with an out-of-box `x0` gets the raw point even at true. Every cold solve,
    /// which is the case this option was measured on, is covered. Widening it to
    /// the resolved start point is registered, not done here: it would have to
    /// run above the payload ingest, which is a second decision about a second
    /// contract.
    ///
    /// NOT a curvature source (docs/counters.md): it selects where the solve
    /// starts, not whether or how any second-order reading is taken.
    /// @see docs/notes/2026-09-m7-w2-sqp-cutest-read.md section M4
    bool project_start_point = true;

    /// THE CURVATURE SOURCE (M7 QN stage 3); see SqpHessianMode. Under
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
///         negative `common.max_iterations`, an engaged `max_factorizations`
///         below 0, an engaged `time_limit_seconds` that is not finite and
///         positive, an `infinite_bound` <= 0 or NaN, a negative `common.threads`,
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
/// "default" is the only name until M8's labeled configs, and it returns a
/// default-constructed value.
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
