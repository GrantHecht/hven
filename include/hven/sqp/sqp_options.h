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
#include <hven/linear/dense_policy.h>
#include <hven/qp/qp_options.h>
#include <hven/qp/qp_types.h>
#include <hven/solver/common_options.h>

namespace hven {

/// The globalization strategy interface `SqpAdvancedOptions::make_strategy`
/// returns. Its definition is internal; a caller that implements one includes
/// the internal header that declares it.
namespace detail {
class GlobalizationStrategy;
} // namespace detail

/// @brief Which QP kernel solves the elastic ladder's rungs.
///
/// `kWalk` is the shipped active-set ladder, hot-chained rung to rung; `kTier`
/// solves each rung with the interior-point kernel on a DEDICATED engine
/// instance.
enum class SqpElasticRungEngine { kWalk, kTier };

/// @brief The curvature source of the SQP subproblems.
///
/// `kExact` evaluates the model's Lagrangian Hessian at every subproblem build.
/// `kLbfgsDamped` never calls eval_hess: the subproblem curvature is a damped
/// compact L-BFGS approximation B (detail/sqp/quasi_newton/lbfgs_damped.h),
/// positive definite by construction, carried into every QP engine call through
/// the mixed lift (mixed_lift.h).
enum class SqpHessianMode { kExact, kLbfgsDamped };

/// @brief The VARIABLE (column) half of problem scaling; see
///        SqpOptions::column_scaling and detail/sqp/column_scaling.h.
///
/// `kOff` never scales a variable. `kAlways` installs the rule's column factors
/// whenever they are not all 1 (a decision carried by a warm start replaces the
/// rule, SqpWarmStart::column_factors); the row/objective layer stays as
/// `SqpOptions::row_scaling` says. `kAuto` installs them only on a BADLY scaled
/// problem -- factor spread >= `SqpOptions::column_scaling_threshold` -- and then
/// engages the row/objective layer for that solve too, whatever `row_scaling`
/// says (with the one-sided objective rule when `row_scaling` is off); below the
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
/// mu(x) = min(1, h(x)) (detail/sqp/globalization/restoration.h).
/// `kModelThenRegularized` runs the phase as kModel and restarts it from the point it
/// stands at under kRegularized at the first failed subproblem or exhausted elastic tier
/// inside the phase (the two triggers SqpOptions::restoration_curvature states); under
/// kLbfgsDamped (whose phase QPs are convex by construction) it is kModel.
enum class SqpRestorationCurvature {
    kModel,
    kRegularized,
    kModelThenRegularized,
};

/// @brief When an accepted step earns the trust-region radius a larger value; see
///        SqpAdvancedOptions::trust_region_growth.
///
/// Every arm grows only when the radius was active at the step and finite, and never after a
/// failed subproblem's own step.
enum class SqpTrustRegionGrowth {
    /// The accepted step's actual/predicted objective decrease ratio is at least 0.75.
    kRatio = 0,
    /// `kRatio`, or the accepted step was a feasibility (h-type) step.
    kRatioOrFeasibilityStep,
    /// Any accepted step, with no ratio test.
    kAnyAccepted
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
    bool second_order_correction = true;

    /// Adaptive dual regularization A/B lever: when on, schedules
    /// QpSolveOverrides::dual_regularization down with the KKT residual instead of leaving
    /// every subproblem at the engine's fixed QpOptions::dual_regularization, which closes
    /// the fixed-mu accuracy ceiling on a badly scaled active set. Default true;
    /// false leaves QpSolveOverrides::dual_regularization at its sentinel every major.
    /// primal_regularization is NOT scheduled by this lever or by anything else.
    bool adaptive_dual_regularization = true;

    /// Factory for the globalization strategy, called ONCE PER solve() call;
    /// must return a non-null, freshly resettable GlobalizationStrategy
    /// (funnel.h) -- returning nullptr is a caller error and solve()
    /// throws std::invalid_argument. Default (empty std::function) means
    /// FunnelStrategy, KLV's funnel and this project's default
    /// globalization. Two solves never share strategy state.
    std::function<std::unique_ptr<detail::GlobalizationStrategy>()> make_strategy;

    /// The full-step-first warm rule: when on, a solve whose warm-start level
    /// resolved to kWarm or above -- never a cold one, and never the 2-argument
    /// overload -- begins in funnel.h's full-step mode, with a WATCHDOG
    /// that restores the best iterate by max(stationarity, feasibility, complementarity) and hands
    /// the solve back to ordinary funnel globalization the moment the residual grows
    /// kWarmResidualGrowthMax majors in a row, or kWarmFullStepWindow majors pass
    /// without a new best. Default true.
    ///
    /// An A/B lever exactly like second_order_correction and adaptive_dual_regularization: set
    /// false and every warm solve behaves precisely as with the mode off.
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
    SqpElasticRungEngine elastic_rung_engine = SqpElasticRungEngine::kWalk;

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

    /// The kIpqp tier's own settings, forwarded verbatim onto the tier exactly
    /// as the SSN levers below are forwarded onto SsnOptions. Inert at
    /// `qp_mode != QpMode::kIpqp` and read on every subproblem at kIpqp -- see
    /// QpMode::kIpqp and IpqpOptions' own doc comments. The one field the
    /// driver narrows rather than forwards is `hoist_symbolic_analysis`; see
    /// `SqpSolver::ipqp_options`.
    IpqpOptions ipqp;

    /// Directory the walk's QP dump files are written to; empty is off, and so is
    /// `qp_dump_policy == kOff`. Each qualifying walk solve -- the major's QP, its SOC re-solve,
    /// its cap retry, the certified fallback's walks and each elastic-ladder rung, restoration's
    /// included -- writes one new file; format and file names in docs/qp-dump-format.md. kSsn and
    /// kIpqp solves are not written. The directory is created; a path that cannot be created or
    /// written, or a file name that already exists, throws std::runtime_error out of solve().
    std::string qp_dump_dir;

    /// Which walk solves are dumped; default `kOff`.
    QpDumpPolicy qp_dump_policy = QpDumpPolicy::kOff;

    /// Read the proximal carry off an ingested SqpWarmStart.
    ///
    /// It gates one thing: whether the first SSN subproblem of a solve starts its
    /// proximal ladder at `SqpWarmStart::prox_sigma` instead of at 0. The emission
    /// side is unconditional and this flag does not touch it, so a caller can
    /// read the carry without first turning it on. Inert in both directions under
    /// `qp_mode == QpMode::kWalk`. Default false.
    bool ssn_proximal_carry = false;

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
    /// routed failure as to a rejection: a shrink below `trust_region_min` raises a
    /// restoration request at any budget. A failed solve that returned an unusable
    /// iterate ends the solve at any budget. Not a curvature source
    /// (docs/counters.md). A caller who must bound the work per solve bounds it
    /// with `common.max_iterations` and the QP's own cap.
    Index max_consecutive_qp_failures = 3;

    /// Re-solve a subproblem that spent its whole minor budget, once, at twice the
    /// cap. Default false: the retry is never attempted and no second engine is
    /// built.
    ///
    /// `QpStatus::kIterationLimit` means the subproblem ran out of minors, not that its
    /// arithmetic failed. At true, a subproblem the walk answered with `kIterationLimit`
    /// and whose returned iterate is usable is re-solved before it counts as a
    /// failure: the same subproblem and radius, warm from the capped solve's
    /// working set (never its point), with an explicit `QpOptions::max_iterations` of
    /// twice the cap that solve resolved. If the retry converges the major
    /// proceeds; if it hits the doubled cap too, the ordinary failure routing
    /// runs. Scoped to the walk: a tier exit under `kSsn`/`kIpqp` is left alone,
    /// those tiers carry their own budgets. A retry that does not help costs one
    /// more walk of at most the doubled cap plus its engine's first
    /// factorization, folded into `qp_minor_iters` and `factorizations`;
    /// `SqpCounters::qp_cap_retries` and `qp_maxiter_exits` report it. Not a
    /// curvature source (docs/counters.md).
    bool qp_iteration_limit_retry = false;

    /// At true, a failed WALK subproblem's usable iterate p != 0 is judged by the
    /// unchanged strategy as a trial step when it predicts a decrease or halves a
    /// violation above `common.feasibility_tol`. Accepted: taken with the
    /// multipliers kept (the failed solve's are not trusted), no radius growth, no
    /// SOC. Rejected: routed, not charged to `max_consecutive_qp_failures`. Certifies
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
    /// it on the non-fixed columns (`detail/sqp/equality_basis.h`); every QP
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
    /// nothing (kNumericalError, or a kOptimal inside the no-claim margin).
    /// The funnel restoration exit (KLV Sec. 3) rather than feasibility; such an
    /// entry is counted in `SqpCounters::restoration_reduction_resumes`. `validate()`
    /// refuses a value outside [0, 1) and a NaN.
    double restoration_resume_reduction = 0.0;

    /// Under kLbfgsDamped, also offer the pair of a REJECTED trial (one extra
    /// gradient/Jacobian evaluation per rejection). False (default) offers accepted
    /// pairs only.
    bool lbfgs_update_on_reject = false;

    /// Under kLbfgsDamped, let B act on the NONLINEAR variables only -- those the model's
    /// declared Lagrangian-Hessian pattern touches -- with zero curvature on the rest, as
    /// every Lagrangian Hessian has there (Gill, Murray & Saunders, SIAM Review 47 (2005),
    /// section 2). Falls back to every variable when the pattern touches all of them or
    /// none. Read in no other mode.
    bool lbfgs_nonlinear_only = false;

    /// Under kLbfgsDamped, a FULL-MEMORY damped BFGS B (dense, every pair kept) replaces
    /// the L-BFGS window whenever the quasi-Newton dimension -- the nonlinear variables
    /// under `lbfgs_nonlinear_only`, else every structural variable -- is at most this; it
    /// enters the QP as a dense block of H, with no lift, and is Oren-Luenberger
    /// self-scaled (qn_models.h). 0 = never. Default 400. Read in no other mode. `validate()`
    /// refuses a negative value.
    Index max_lbfgs_full_memory_dimension = 400;

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
    ///      the subproblem) is kIterationLimit or kNumericalError;
    ///   2. the elastic tier is exhausted and raises the nested restoration
    ///      request, which is never serviced inside a phase.
    /// The switch restarts the phase only when budget remains and the model is
    /// finite at that point; otherwise the stopped sub-solve's exit stands. A phase
    /// that meets neither trigger is identical to kModel. Under kLbfgsDamped it is
    /// kModel: that phase's quasi-Newton model is convex by construction.
    SqpRestorationCurvature restoration_curvature = SqpRestorationCurvature::kModelThenRegularized;

    /// Restoration entries allowed per SOLVE (the interior-point engine's
    /// `max_restoration_entries` counts per phase). Default 10; must be >= 0; 0
    /// refuses restoration: the first request ends the solve kNumericalError, as
    /// a refused later request does.
    ///
    /// 1 is the once-per-solve rule: a second restoration request is refused and
    /// the solve ends kNumericalError. Above 1 a later request is serviced, as in
    /// the filter and funnel methods (Fletcher & Leyffer, Math. Program. 91 (2002);
    /// [KLV], funnel.h), but only when the main loop has accepted at least one
    /// step since the previous resume. Every phase draws on the same
    /// `common.max_iterations` budget, and each resume re-bases the funnel (KLV
    /// Algorithm 2).
    Index max_restoration_entries = 10;

    /// The trust-region growth rule. Default kRatio.
    SqpTrustRegionGrowth trust_region_growth = SqpTrustRegionGrowth::kRatio;

    /// SSN Farkas certificate's relative-residual threshold (kFarkasGated only). In (0, 1).
    double ssn_farkas_residual_tol = 1e-6;

    /// SSN uncertain-set entering threshold on |alpha - beta|; 0 disables the set. In [0, 1).
    double ssn_uncertain_tol = 0.1;

    /// SSN Levenberg-Marquardt sizing coefficient c (residual sigma rules only). Finite, > 0.
    double ssn_lm_sigma_c = 1.0;

    /// SSN Levenberg-Marquardt sizing exponent p in c * min(r, r^p). In [1, 2].
    double ssn_lm_sigma_exponent = 2.0;

    /// Offer the elastic ladder's step as a restoration candidate at the ladder-unusable site.
    bool restoration_elastic_seed = true;

    /// Escalation headroom, in rungs of the penalty ladder, the evidence-placed first rung leaves
    /// below the ladder ceiling. Integer in [1, 6].
    Index elastic_rho0_headroom_rungs = 1;

    /// Ceiling of the adaptive dual-regularization schedule (read when
    /// `adaptive_dual_regularization` is on). Finite, in [1e-12, 1].
    double adaptive_dual_regularization_max = 1e-8;
};

/// @brief The SQP engine's factorization settings.
struct SqpLinearOptions {
    /// When the dense factorization arm is used: `kAuto` (the default; M8 DN-3):
    /// the dense arm for each analyzed pattern of order n <= 105, the library's
    /// threshold for this engine, the sparse backend above it, so one solve can
    /// use both; `kNever`: the sparse backend always; or `kAlways`, which
    /// allocates 8*n^2 bytes per analyzed KKT pattern (see
    /// `SymmetricFactor::Options::dense_factorization`). The
    /// dense arm runs single-threaded on MKL (Accelerate: its default threading).
    /// `SqpCounters::dense_factorizations` counts what it took. It reaches the
    /// QP walk's, the SSN tier's and the IPQP subproblem's KKT factors and the
    /// rank repair's Gram factor; the
    /// warm-start predictor's factor and the equality-basis Gram and
    /// certificate factors stay on the platform session at every value.
    hven::linear::DensePolicy dense_factorization = hven::linear::DensePolicy::kAuto;
    /// RESERVED for a caller's `DensePolicy::kAuto` threshold: `validate()`
    /// accepts only the empty value, which uses the library's (105).
    std::optional<Index> max_dense_dimension;
};

/// @brief Driver options for the whole SQP solve.
///
/// TOLERANCES. `common.optimality_tol` gates the stationarity measure and the NLP
/// complementarity, `common.feasibility_tol` the feasibility measure; both
/// measures are defined at `SqpKkt` (sqp_solver.h). `common.feasibility_tol` is
/// also the bound-activity tolerance of the stationarity measure.
///
/// TRUST REGION. `trust_region_initial` is the l-infinity radius the first subproblem is
/// given; the radius then doubles on a strong accepted step up to `trust_region_max` and
/// halves on a rejected one. A +inf `trust_region_initial` means no trust region, and its
/// first shrink resolves to `trust_region_max`. `trust_region_min` is the radius floor, and
/// reaching it is an event rather than a clamp: a shrink that would go below it enters the
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
    /// region" (QpSolveOverrides' own sentinel), though an indefinite
    /// subproblem is then generally unbounded and the engine reports
    /// kNumericalError rather than a step. See the TRUST REGION note above.
    double trust_region_initial = 1.0;

    /// Ceiling the growth rule expands the radius toward, and the value a
    /// +inf trust_region_initial's FIRST shrink resolves to (inf/2 is inf, so the shrink
    /// rule needs a finite landing value the first time it fires). Default
    /// 1e10. Must be >= trust_region_initial, unless trust_region_initial is +inf (then exempt).
    double trust_region_max = 1e10;

    /// The radius FLOOR. A shrink that would take the radius below this
    /// value does not clamp and re-solve -- it raises a restoration request
    /// instead (KLV Algorithm 4's "alpha < alpha_min" trigger, in its
    /// trust-region form). Default 1e-10, chosen four orders below the
    /// default common.optimality_tol/common.feasibility_tol resolution and far below
    /// trust_region_initial so reaching it is evidence rather than noise -- see the TRUST REGION
    /// note above for the derivation. Must be > 0 and <= both trust_region_initial and
    /// trust_region_max.
    double trust_region_min = 1e-10;

    /// The caller-unit step limit r, default 0 = off; SQP-only (the interior-point solver has no
    /// trust region, column layer or walk QP). When > 0, every QP's variable box is narrowed to
    /// |dx_j| <= r * (1 + |x_j|), x_j the iterate in caller units, divided by the column factor
    /// d_j under column scaling. It binds kWalk, kSsn and kIpqp, the SOC re-solve, the elastic
    /// ladder and the restoration phase's x-block (its slacks are not limited). Where the limit
    /// and the declared box do not intersect, that variable keeps its declared box. A variable
    /// pinned at a limit side is reported free with z = 0 and counts as trust-region-binding. The
    /// trust region stays in engine units (|dx_j| <= d_j * radius). Must be finite and >= 0.
    double caller_step_limit = 0.0;

    /// Copied into the driver's single QpEngine at construction, so
    /// per-solve variation must go through QpSolveOverrides (sqp_solver.h),
    /// never through this struct -- see qp_types.h's PER-INSTANCE, CONST
    /// note on QpOptions::trust_region_radius for why. No default beyond QpOptions'
    /// own.
    QpOptions qp;

    /// Which QP kernel the driver's subproblems go through. kWalk is the shipped
    /// primal active-set walk (walk_engine.h) and is the default; kSsn selects the
    /// semismooth-Newton kernel (ssn_engine.h); kIpqp selects the interior-point
    /// tier (detail/qp/ipqp/ipqp_engine.h) -- see QpMode::kIpqp's own doc comment above.
    QpMode qp_mode = QpMode::kWalk;

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
    bool row_scaling = false;

    /// The inf-norm the scaled objective gradient and each scaled Jacobian row
    /// are aimed at. Default 100.0, following IPOPT's
    /// `nlp_scaling_max_gradient` in name and value. Must be finite and > 0.
    /// Read only when the row layer runs: `row_scaling`, or column_scaling kAuto's
    /// engagement of it.
    double scaling_max_gradient = 100.0;

    /// Symmetric clamp on every factor the rule produces: each is confined to
    /// [1/scaling_factor_max, scaling_factor_max]. Default 1e12 -- twelve
    /// orders each way. Must be finite and >= 1. Read when the row layer runs
    /// (`row_scaling`, or column_scaling kAuto's engagement) and by kAuto's column rule;
    /// kAlways's column factors are clamped by `column_scaling_factor_max` instead.
    ///
    /// It is what bounds the damage from the rule being TWO-SIDED
    /// (problem_scaling.h): a start point that is genuinely near-stationary has
    /// its objective gradient AMPLIFIED, and without a ceiling that amplification
    /// is unbounded.
    double scaling_factor_max = 1e12;

    /// Round each row factor to the nearest power of two after the clamp. Read when the row
    /// layer runs.
    bool row_scaling_power_of_two = false;

    /// The column (variable) layer of problem scaling. When it fires the
    /// engine solves the problem in the variables x~ = x / d, d the power-of-two column
    /// half of a Ruiz equilibration of [Je; Ji] at the start point
    /// (detail/sqp/column_scaling.h); restoration, the trust region and the funnel
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
    /// `amplify_objective`) unless `row_scaling` is on.
    SqpColumnScaling column_scaling = SqpColumnScaling::kAuto;

    /// kAuto's gate: the column factors' spread (max d / min d over columns with
    /// Jacobian data) at or above which a problem counts as badly scaled. Default
    /// 2^26 = 1/sqrt(machine epsilon), the spread past which the smaller columns'
    /// contributions keep fewer than half of a double's digits. A mesh transcription's
    /// spread grows like its node count (control columns enter every defect row as
    /// O(h)), and 2^26 leaves that growth unscaled to about 4e7 nodes. Must be finite
    /// and >= 1. Read only under kAuto.
    double column_scaling_threshold = 67108864.0;

    /// Symmetric clamp on the column factors under `kAlways`: each is confined to
    /// [1/column_scaling_factor_max, column_scaling_factor_max]. Default 1024 (2^10), so the
    /// kAlways spread is at most 2^20, below kAuto's default gate 2^26. A column factor also
    /// scales the trust region in caller units (|dx_j| <= d_j * radius). Not read under kAuto,
    /// whose column factors `scaling_factor_max` clamps (up to 2^39 at its default). Must be
    /// finite and >= 1.
    double column_scaling_factor_max = 1024.0;

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
    Index lbfgs_memory = 10;

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
///         `common.threads`, an engaged `qp.max_iterations` below 1,
///         an out-of-range enum (every enum field is checked), a non-positive or
///         NaN `trust_region_initial`, a `trust_region_max` below `trust_region_initial` (a +inf
///         `trust_region_initial` is exempt), a `trust_region_min` that is non-positive or above
///         either end of the range it floors, an ill-formed scaling or IPQP field (validated at
///         every `qp_mode`), a negative, NaN or infinite `restoration_proximity`, a
///         `restoration_resume_reduction` outside [0, 1) or NaN, a `lbfgs_memory`
///         below 1, a negative `max_lbfgs_full_memory_dimension`, `kLbfgsDamped` with
///         `restoration_proximity > 0`, a `max_restoration_entries` below 0, a
///         `max_consecutive_qp_failures` below 1, a reserved dense-arm value, a `qp` regularization
///         or tolerance that is NaN, infinite or out of its range, a
///         `qp.max_schur_border_dimension` below 0, a `qp.trust_region_radius` that is negative or
///         NaN, or an out-of-range `advanced` lever (`trust_region_growth`,
///         `ssn_farkas_residual_tol`, `ssn_uncertain_tol`, `ssn_lm_sigma_c`,
///         `ssn_lm_sigma_exponent`, `elastic_rho0_headroom_rungs`,
///         `adaptive_dual_regularization_max`).
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
/// std::ostream &)` (ipm/ipm_options.h). Nested blocks print under their
/// member names (`common.`, `qp.`, `linear.`, `advanced.`, `advanced.ipqp.`).
/// `advanced.make_strategy` prints `set` or `unset`.
///
/// @param o The options to write.
/// @param os The stream, borrowed: this call neither flushes nor closes it.
void dump_options(const SqpOptions &o, std::ostream &os);

/// @brief Returns @p base prepared for a worker context: one backend thread and
///        silent output (`worker_options(IpmOptions)`'s counterpart). Every other
///        field is carried through untouched.
SqpOptions worker_options(SqpOptions base);

} // namespace hven
