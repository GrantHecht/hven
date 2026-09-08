// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// sqp_types.h — the plain data types the SQP driver produces and consumes:
// status, options, counters, per-major history, solution. The driver itself
// (the loop, the subproblem construction and the KKT measure) lives in
// sqp_driver.h; nothing in this file does any work.
//
// This file is to sqp_driver.h what qp_types.h is to qp_engine.h.

#include <functional>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/drivers/common_options.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

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

    /// NOT A MODE. The ENUMERATOR COUNT, and the only thing that can catch a
    /// fourth kernel added without an arm in the driver's dispatch: a
    /// static_assert beside that switch pins this value, and appending a real
    /// mode ABOVE this line moves it and stops the build.
    ///
    /// It is a legal `QpMode` value that names no kernel, so
    /// `validate_sqp_options` refuses it like any other out-of-range setting, and
    /// the dispatch enumerates it in an arm that throws rather than omitting it.
    kQpModeCount,
};

// Declared HERE rather than in ssn_engine.h for the same reason QpMode is:
// SqpOptions carries them and sqp_types.h is the header ssn_engine.h includes,
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

/// @brief The kIpm tier's own settings.
///
/// Declared HERE for the same reason QpMode and the three Ssn*Rule enums are:
/// SqpOptions carries this struct as `ipqp`, and sqp_types.h is the header the
/// engine includes. Forwarded onto the tier verbatim, and each field's mechanism
/// is derived at the engine that consumes it.
///
/// `qp_mode` stays kWalk by default, so nothing here is reachable until a caller
/// opts in -- but the fields are VALIDATED UNCONDITIONALLY at every mode, because
/// a field is out of range whether or not this solve will read it.
struct IpqpOptions {
    /// Iteration budget for the tier's own Mehrotra loop. `<= 0` is a
    /// SENTINEL meaning "size-derived", mirroring `QpOptions::max_iter`'s own
    /// `detail::effective_qp_max_iter` discipline (`qp_engine.h:499-517`) --
    /// so every `Index` value is legal and nothing here is validated.
    Index ipqp_max_iter = 0;

    /// The hard cap of LAST RESORT (`IpqpEscape::kBudget`): it fires only when
    /// the early-stall detector should have fired first and did not. Unlike
    /// `ipqp_max_iter` it has no sentinel reading, so it must be a genuine,
    /// positive iteration count. Default 60. Must be > 0. PER ATTEMPT, not per
    /// subproblem: the warm-kill re-bases both caps at the cold restart, so an
    /// abandoned attempt may total up to twice this value.
    Index ipqp_hard_iter_cap = 60;

    /// Factorization budget for the tier, enforced BEFORE EVERY factorization --
    /// ladder rungs and the final inertia read included, not merely once per
    /// iteration. `<= 0` is a SENTINEL meaning "3 x the tier's EFFECTIVE
    /// ITERATION BUDGET", which is `min(effective ipqp_max_iter,
    /// ipqp_hard_iter_cap)` and therefore 180 at the shipped defaults. Every
    /// `Index` value is legal and nothing here is validated. PER ATTEMPT, for the
    /// reason `ipqp_hard_iter_cap` is, and every factorization is charged to the
    /// driver's probe budget either way.
    Index ipqp_max_factorizations = 0;

    /// The cold starting barrier parameter, and the ceiling of the warm-restart
    /// clamp `mu_0 = clamp(max(mu_meas, kappa_mu * mu_payload), ipqp_min_mu,
    /// ipqp_init_mu)` -- a setting in both readings, never overwritten by payload
    /// evidence, only clamped against it. Default 1e-2, Ruiz-scaled. Must be
    /// finite, > 0, and >= `ipqp_min_mu`, since the clamp band is otherwise
    /// inverted.
    double ipqp_init_mu = 1e-2;

    /// The FLOOR of the `mu_0` clamp above, and the tier's own barrier-decay
    /// floor thereafter. Matches `Settings::min_mu_`'s own default
    /// (`interior_point_solver.h:366`) so the two barrier engines agree on
    /// how low `mu` is ever allowed to go. Default 1e-12. Must be finite,
    /// > 0, and <= `ipqp_init_mu`.
    double ipqp_min_mu = 1e-12;

    /// The (rho, delta) proximal regularization schedule's INITIAL values:
    /// `rho_0 = ipqp_rho_init`, `delta_0 = ipqp_delta_init`. `rho`/`delta`
    /// themselves enter the regularized KKT matrix's diagonal blocks (`H + rho I`
    /// on the primal block, `-delta I` on each dual block); it is the PROXIMAL
    /// CENTERING terms that enter the right-hand side. Default 8.0 each.
    ///
    /// `ipqp_rho_init` must be finite and lie in `[ipqp_reg_floor,
    /// ipqp_reg_max]`: the starting value of a quantity the ladder only ever
    /// moves within that band must itself start inside it.
    double ipqp_rho_init = 8.0;
    /// The dual-block counterpart of `ipqp_rho_init`, same schedule, on a
    /// RELAXED band: it need only be finite and in `(0, ipqp_reg_max]` --
    /// positive, since a non-positive delta would not regularize the dual block
    /// at all, and no larger than the ceiling every regularized quantity in this
    /// schedule shares. Default 8.0.
    double ipqp_delta_init = 8.0;

    /// The absolute floor the (rho, delta) ladder's GATED decrease may never
    /// cross, matching `detail::kProxRegFloor`
    /// (`inertia_regularization.h:46`, Cipolla-Gondzio eq. 19). Default
    /// 1e-10. Must be finite and > 0.
    double ipqp_reg_floor = 1e-10;

    /// The ceiling the (rho, delta) ladder's inertia-demanded growth may
    /// never cross, matching `detail::kSsnProxMax` (`ssn_engine.h:576`) --
    /// the tier inherits SSN's relative cap-slack exhaustion guard
    /// (`detail::kSsnProxCapSlack`, `ssn_engine.h:577`) along with the
    /// constant, not just the number. Default 1e6. Must be finite and >=
    /// `ipqp_reg_floor`.
    double ipqp_reg_max = 1e6;

    /// The (rho, delta) ladder's GATED decrease factor: applied only after
    /// the regularized residuals have contracted by the required amount
    /// (spec 3.2), never unconditionally the way `prox_reg_decay`'s geometric
    /// decay is -- this is a fraction the schedule multiplies the current
    /// value by, so it must lie strictly inside (0, 1): 0 would collapse the
    /// regularization in one gated step and >= 1 would never decrease it.
    /// Default 0.1.
    double ipqp_reg_decrease = 0.1;

    /// The fraction-to-boundary parameter (`tau`/`bfrac`) both the affine
    /// and corrector steps are clipped by (spec 3.1). A standard IPM
    /// literature constant, meaningful only strictly inside (0, 1): 0 would
    /// permit no step at all and >= 1 would permit stepping onto or past a
    /// bound. Default 0.995.
    double ipqp_tau = 0.995;

    /// The RATIO threshold `kappa` of the face-classification rule: row `j` is
    /// active iff `s_j < kappa * z_j` and INACTIVE iff `z_j < kappa * s_j` --
    /// scale-free by construction, which is the point of a ratio rule over an
    /// absolute one. Default 1e-2. Must be finite and STRICTLY inside (0, 1): a
    /// non-positive kappa can never classify anything active, and at kappa >= 1
    /// the two tests stop being mutually exclusive, leaving no room for the
    /// routing chain's UNCERTAIN class.
    double ipqp_face_kappa = 1e-2;

    /// The barrier-phase convergence target's SLACK factor (spec 2.3 step 1
    /// / 3.4): the tier converges to `ipqp_converge_slack x` the QP layer's
    /// own relative tolerances, deliberately LOOSER than the 1e-10 tier-3
    /// exact refinement owns the last two decades of. Default 100. Must be
    /// finite and >= 1 -- a slack below 1 would ask the barrier phase for
    /// MORE accuracy than tier 3's own finish, inverting the division of
    /// labor the two-tier design rests on.
    double ipqp_converge_slack = 1e2;

    /// Ruiz equilibration (spec 4.3) inside the tier, unscaled on export.
    /// Default true. A plain A/B lever; every asserted pin is on unscaled
    /// quantities either way.
    bool ipqp_ruiz = true;

    /// The warm-seed repair (spec 5.2): strict-positivity clamps plus the
    /// two-scalar `(delta_p, delta_d)` shift, applied before a warm restart
    /// is trusted. Default true. Off disables the repair, never the validation:
    /// a seed the repair would have had to fix degrades cold instead,
    /// mode-local and visible in the grade.
    bool ipqp_warm_repair = true;

    /// The warm-kill rule's iteration budget: a warm-started solve overrunning
    /// this many iterations is restarted COLD exactly once. The effective budget
    /// IS CLAMPED at `min(ipqp_warm_iter_budget, effective ipqp_max_iter)`, so a
    /// caller-chosen value larger than the solve's own iteration budget can never
    /// become the binding limit. Default 15. Must be >= 0 (0 is legal: every warm
    /// restart is killed on its first iteration).
    Index ipqp_warm_iter_budget = 15;

    /// `kappa_mu` in the section 5.3 `mu_0` clamp rule: `mu_0 =
    /// clamp(max(mu_meas, ipqp_mu_adopt_factor * mu_payload), ipqp_min_mu,
    /// ipqp_init_mu)`. 0 DISABLES ADOPTION (the clamp then reads only
    /// `mu_meas`), which is a deliberate, documented reading of this field
    /// rather than a degenerate one, so it stays legal. Default 1.0. Must be
    /// finite and >= 0.
    double ipqp_mu_adopt_factor = 1.0;

    /// The early-stall detector's (spec 6.2) window width in ACCEPTED
    /// iterations, matching `detail::kSsnStallWindow` (`ssn_engine.h:641`)
    /// for the reason its own banner gives: an order of magnitude above the
    /// local regime, so no healthy trajectory reaches it. Default 5. Must be
    /// > 0 -- a zero-width window can never accumulate the whole-window
    /// evidence the test is built on.
    Index ipqp_stall_window = 5;

    /// The escape ladder's retirement threshold (spec 6.1): after this many
    /// CONSECUTIVE escapes within one SQP solve the tier is retired for the
    /// remainder of that solve; any success resets the count. Default 3.
    /// Must be > 0 -- retiring "after zero consecutive escapes" is not a
    /// count, it is disabling the tier outright, which this field does not
    /// exist to express.
    Index ipqp_retire_after = 3;

    /// Optional Farkas corroboration (spec 6.3) of an infeasible-suspect
    /// escape: one matvec plus O(m), no factorization. It may ARM the
    /// evidence block; it never certifies. Default true.
    bool ipqp_farkas_gate = true;

    /// Cross-major symbolic reuse (spec 4.1) kill switch: while true, the
    /// tier hoists its analysis/verify work across majors whose
    /// `AggregateEvalSeam::epoch()` is unchanged.
    /// Default true; false forces a fresh symbolic pass every major.
    bool ipqp_hoist_symbolic = true;

    /// The required final unregularized inertia read (spec 2.2 item 4): when
    /// true, a certifying exit pays one extra
    /// unregularized factorization to confirm the inertia the regularized
    /// solve reported; a WRONG read downgrades the certificate rather than
    /// reporting `kOptimal`. Default true. FALSE means every certificate is
    /// downgraded unconditionally -- an explicit, documented weakening for a
    /// caller who wants to skip the extra factorization, never a silent one.
    bool ipqp_require_final_inertia = true;
};

// The full-step-first warm rule's two constants. Both are watchdog thresholds:
// they bound how long the driver may keep taking undamped steps under
// globalization.h's full-step mode before restoring the best iterate it saw and
// handing the solve back to the funnel.
//
// kWarmResidualGrowthMax = 2 consecutive majors with a growing ||KKT||inf;
// kWarmFullStepWindow = 5 majors without a new best ||KKT||inf.
//
// Changing either is a behaviour change on every warm solve: the warm-start
// suite pins the majors of both a converging and a watchdog-restored run
// against these exact values.
// @see docs/notes/2026-09-header-prose-archive.md §sqp_types.h

/// Watchdog threshold: consecutive majors with growing ||KKT||inf tolerated
/// under the full-step mode before it restores the best iterate.
inline constexpr Index kWarmResidualGrowthMax = 2;

/// Watchdog threshold: majors without a new best ||KKT||inf tolerated under
/// the full-step mode before it restores the best iterate.
inline constexpr Index kWarmFullStepWindow = 5;

/// @brief Relative margin below which an active row's multiplier -- or an
///        inactive row's slack -- counts as weak/near activity in the per-major
///        census.
///
/// A CONSTANT, NOT A SETTING: nothing in this library reads or branches on either
/// count, so there is no behaviour for a caller to tune.
///
/// It measures strict complementarity BY MAGNITUDE: a row is weakly active when
/// it is in the working set at a price negligible beside the largest one, and
/// nearly active when it is out of the working set at a slack negligible beside
/// the largest one.
///
/// It is NOT the SSN's uncertain set: that set is the dimensionless kink band
/// with leave hysteresis (ssn_engine.h), which reads a PURE state at `s == 0` for
/// any lambda and so never flags a small multiplier on an active row.
inline constexpr double kWeakActivityMargin = 1e-6;

/// @brief Driver options for the whole SQP solve.
///
/// TOLERANCES. kkt_tol gates the STATIONARITY measure and feas_tol the
/// FEASIBILITY measure; both are defined at `SqpKkt` (sqp_driver.h). feas_tol
/// does double duty as the geometric BOUND-ACTIVITY tolerance of that measure, so
/// there is no separate activity-tolerance knob.
///
/// TRUST REGION. tr_init is the l-infinity radius the FIRST subproblem is given,
/// through SolveOverrides::tr_radius; from there the driver's radius management
/// takes over -- doubling on a strong accepted step up to tr_max, halving on a
/// rejected one, never below tr_min. A +inf tr_init resolves to `qp.tr_radius`,
/// i.e. no trust region, and its FIRST shrink resolves to tr_max, after which
/// ordinary halving applies.
///
/// tr_min is the radius FLOOR, and reaching it is an EVENT rather than a clamp: a
/// shrink that would go below it enters the RESTORATION PHASE instead, so the
/// radius is never left spinning at the floor. The default 1e-10 is an
/// implementation choice, four orders below the default tolerances and far enough
/// below tr_init that reaching it is evidence rather than noise.
///
/// max_iter bounds subproblems solved, not accepted steps, and the budget is
/// shared with the restoration phase: the bounded quantity is
/// major_iters + restoration_iters. history.size() still tracks major_iters
/// alone, since restoration produces no history rows.
///
/// GLOBALIZATION STRATEGY. make_strategy is called ONCE PER solve() call and must
/// return a non-null, freshly resettable GlobalizationStrategy; the driver owns
/// the returned object for that solve, so two solves never share funnel state.
/// Empty (the default) means FunnelStrategy. A factory returning nullptr is a
/// caller error and solve() throws std::invalid_argument.
///
/// `qp` is copied into the driver's single QpEngine at construction, so per-solve
/// variation goes through SolveOverrides, never through this struct.
/// @see docs/notes/2026-09-header-prose-archive.md §sqp_types.h
struct SqpOptions {
    /// Stationarity gate: CONVERGED requires stationarity <= kkt_tol (AND
    /// feasibility <= feas_tol). Default 1e-6. Must be > 0. See the
    /// CONVERGENCE TEST note in sqp_driver.h for exactly what "stationarity"
    /// measures.
    double kkt_tol = 1e-6;

    /// Feasibility gate, AND (deliberately, not a separate knob) the
    /// geometric bound-activity tolerance the stationarity measure itself
    /// uses -- see the TOLERANCES note above for why the two are the same
    /// number. Default 1e-6. Must be > 0.
    double feas_tol = 1e-6;

    /// Bounds SUBPROBLEMS SOLVED, not accepted steps -- a rejected trial
    /// costs one exactly like an accepted one. SHARED with the restoration
    /// phase: the bounded quantity is major_iters + restoration_iters, not
    /// major_iters alone. Default 100. Must be >= 0.
    Index max_iter = 100;

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
    /// default kkt_tol/feas_tol resolution and far below tr_init so reaching
    /// it is evidence rather than noise -- see the TRUST REGION note above
    /// for the derivation. Must be > 0 and <= both tr_init and tr_max.
    double tr_min = 1e-10;

    /// Second-order correction A/B lever: when ON, a kReject trial whose
    /// constraint violation INCREASED gets one hot-started re-solve rescue
    /// attempt before the radius shrinks (the Maratos-effect defense; this
    /// project's cheap edge over Uno, which omits SOC entirely). Default
    /// true. Set false to recover exact non-SOC behaviour. See the
    /// SECOND-ORDER CORRECTION note above (and sqp_driver.h's, for the
    /// mechanism itself).
    bool enable_soc = true;

    /// Adaptive dual regularization A/B lever: when on, schedules
    /// SolveOverrides::dual_mu down with the KKT residual instead of leaving
    /// every subproblem at the engine's fixed QpOptions::dual_mu, which closes
    /// the fixed-mu accuracy ceiling on a badly scaled active set. Default true;
    /// false leaves SolveOverrides::dual_mu at its sentinel every major.
    /// primal_delta is NOT scheduled by this lever or by anything else.
    bool adaptive_mu = true;

    /// Copied into the driver's single QpEngine at construction, so
    /// per-solve variation must go through SolveOverrides (sqp_driver.h),
    /// never through this struct -- see qp_types.h's PER-INSTANCE, CONST
    /// note on QpOptions::tr_radius for why. No default beyond QpOptions'
    /// own.
    QpOptions qp;

    /// Factory for the globalization strategy, called ONCE PER solve() call;
    /// must return a non-null, freshly resettable GlobalizationStrategy
    /// (globalization.h) -- returning nullptr is a caller error and solve()
    /// throws std::invalid_argument. Default (empty std::function) means
    /// FunnelStrategy, KLV's funnel and this project's default
    /// globalization. See the GLOBALIZATION STRATEGY note above.
    std::function<std::unique_ptr<GlobalizationStrategy>()> make_strategy;

    /// A CEILING on the level the 3-argument solve() is allowed to resolve to,
    /// independent of what `warm` itself would otherwise justify.
    ///
    /// At StartLevel::kSeeded a driver ingests a hash-less object's values but
    /// never a factorization, never the funnel/TR state and never the
    /// Kungurtsev-Diehl window, EVEN when the object would have earned kWarm; it
    /// also short-circuits the structural-hash probe, whose answer could only
    /// raise the level above the ceiling.
    ///
    /// Default StartLevel::kWarm: kHot is reachable but opt-in, since it hands
    /// this instance's engine a factorization possibly built by a different
    /// engine instance. Set it to kCold to force every 3-argument solve to behave
    /// exactly as the 2-argument one does. Does NOT affect solve(model, x0),
    /// which is always cold by construction.
    StartLevel start_level = StartLevel::kWarm;

    /// The full-step-first warm rule: when on, a solve whose warm-start level
    /// resolved to kWarm or above -- never a cold one, and never the 2-argument
    /// overload -- begins in globalization.h's full-step mode, with a WATCHDOG
    /// that restores the best iterate by ||KKT||inf and hands the solve back to
    /// ordinary funnel globalization the moment the residual grows
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

    /// BUDGETED MODE: when true, a solve that exhausts max_iter in the MAIN
    /// optimality loop reports SqpStatus::kBudgetExhausted rather than kMaxIter,
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
    /// SCOPE: this lever governs ONLY the main loop's own max_iter exhaustion. It
    /// does not change what happens when the restoration phase runs out of the
    /// shared budget mid-restoration, which stays kMaxIter because it answers a
    /// different question.
    bool budget_mode = false;

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
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_types.h
    bool elastic_ladder_early_exit = false;

    /// The crash basis, OPT-IN: when on, a COLD solve seeds its FIRST QP
    /// subproblem's working set from the activity geometry at x0 -- every
    /// inequality row geometrically active there, and every variable sitting on a
    /// finite bound there -- instead of making the engine rediscover them one
    /// ratio test at a time. Default FALSE, and off is byte-identical to the
    /// driver's behaviour without the lever.
    ///
    /// The predicates are not new. Both are the tests this driver already applies
    /// at every convergence check, at the SAME tolerance (feas_tol), reused
    /// verbatim:
    ///
    ///     row j seeded    :=  cI_j(x0) >= -feas_tol
    ///     var i at lower  :=  x0(i) - l(i) <= feas_tol,  l(i) finite
    ///     var i at upper  :=  u(i) - x0(i) <= feas_tol,  u(i) finite
    ///
    /// So there is no new constant to calibrate and no second notion of "active"
    /// in the driver. A variable satisfying both bound tests without being fixed
    /// is seeded kAtLower -- an arbitrary choice, not a derivation.
    ///
    /// Cold only, by construction: the seed is built at the first subproblem of a
    /// solve whose resolved start level is kCold and only when no warm seed
    /// exists. No extra model evaluation: both predicates are read off the first
    /// subproblem itself, never from a fresh eval_ci call.
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_types.h
    bool crash_basis = false;

    /// Which QP kernel the driver's subproblems go through. kWalk is the shipped
    /// primal active-set walk (qp_engine.h) and is the default; kSsn selects the
    /// semismooth-Newton kernel (ssn_engine.h); kIpm selects the interior-point
    /// tier (detail/qp/ipqp_engine.h) -- see QpMode::kIpm's own doc comment above.
    QpMode qp_mode = QpMode::kWalk;

    /// The kIpm tier's own settings, forwarded verbatim onto the tier exactly
    /// as the SSN levers below are forwarded onto SsnOptions. Inert at
    /// `qp_mode != QpMode::kIpm` and read on every subproblem at kIpm -- see
    /// QpMode::kIpm and IpqpOptions' own doc comments. The one field the
    /// driver narrows rather than forwards is `ipqp_hoist_symbolic`; see
    /// `SqpDriver::ipqp_options`.
    IpqpOptions ipqp;

    /// Read the proximal carry off an ingested WarmStart.
    ///
    /// It gates one thing: whether the first SSN subproblem of a solve starts its
    /// proximal ladder at `WarmStart::prox_sigma` instead of at 0. The emission
    /// side is unconditional and this flag does not touch it, so a caller can
    /// read the carry without first turning it on. Inert in both directions under
    /// `qp_mode == QpMode::kWalk`. Default false.
    /// @see docs/notes/2026-09-header-prose-archive.md §sqp_types.h
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
    bool ssn_certify_from_face = false;

    /// The three SSN rule levers. Forwarded verbatim onto the SsnOptions every
    /// subproblem is solved with (sqp_driver.h's `ssn_options`), where each
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
    /// the SCALED residuals, while `SqpSolution`'s four terminal KKT fields, its
    /// `f`, its multiplier blocks and every history row report CALLER-scale
    /// values. Those two can differ, so a kOptimal solve may report a
    /// caller-scale `kkt_residual` above `kkt_tol`; `SqpScalingReport` carries
    /// both the factors and the scaled residual the gate actually read.
    bool enable_scaling = false;

    /// The inf-norm the scaled objective gradient and each scaled Jacobian row
    /// are aimed at. Default 100.0, following IPOPT's
    /// `nlp_scaling_max_gradient` in name and value. Must be finite and > 0.
    /// Read only when `enable_scaling` is true.
    double scaling_max_gradient = 100.0;

    /// Symmetric clamp on every factor the rule produces: each is confined to
    /// [1/scaling_factor_limit, scaling_factor_limit]. Default 1e12 -- twelve
    /// orders each way. Must be finite and >= 1. Read only when
    /// `enable_scaling` is true.
    ///
    /// It is what bounds the damage from the rule being TWO-SIDED
    /// (problem_scaling.h): a start point that is genuinely near-stationary has
    /// its objective gradient AMPLIFIED, and without a ceiling that amplification
    /// is unbounded.
    double scaling_factor_limit = 1e12;

    /// The options both engines share (drivers/common_options.h), at the SQP
    /// engine's own defaults -- `threads = 0` ("leave the backend alone", which
    /// is what this engine has always done; the process-wide MKL_NUM_THREADS pin
    /// is its reproducibility mechanism) and `print_level = 3` (silent, which is
    /// what this engine has always been).
    ///
    /// In T8.3 NEITHER of those two is read by this engine: the fields are here
    /// so both engines spell the same knob the same way, and so a hot handle's
    /// options fingerprint can cover the thread count from the start. T8.7 gives
    /// this engine a console table at `print_level`, and T8.8 makes a non-zero
    /// `threads` reach every factor path. `common.start_level` is likewise
    /// carried and unread: `SqpOptions::start_level` above is still the field
    /// the driver caps a warm start with, until T8.10 folds the two.
    ///
    /// LAST, not first, so that adding it moves no existing field's offset.
    CommonOptions common;
};

/// @brief Validates a whole SqpOptions value.
///
/// The body of validate_sqp_options() below, plus the two CommonOptions checks
/// (`common.threads` must be non-negative, `common.print_level` non-negative).
/// M6 W5 T8.3 named it `validate` so both engines spell whole-value validation
/// the same way; validate_sqp_options() stays as a one-line forwarder until
/// T8.10 sweeps its ~60 call sites.
///
/// @param opts The options object to validate.
/// @throws std::invalid_argument, on the same terms validate_sqp_options
///         documents, plus a negative `common.threads` or `common.print_level`.
void validate(const SqpOptions &opts);

/// @brief Returns a full SqpOptions value for a named preset.
///
/// "default" is the only name until M8's labeled configs, and it returns a
/// default-constructed value.
///
/// @param name A preset name.
/// @throws std::invalid_argument, listing every valid name, if `name` is not one.
SqpOptions sqp_preset(std::string_view name);

/// @brief The boundary validation SqpDriver's constructor runs over SqpOptions.
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
///         option the driver cannot honour: a non-positive or NaN
///         kkt_tol/feas_tol, a negative max_iter, a non-positive or NaN tr_init,
///         a tr_max below tr_init (a +inf tr_init is exempt), a tr_min that is
///         non-positive or above either end of the range it floors, or an
///         ill-formed IpqpOptions field -- validated unconditionally, like the
///         scaling fields, since `qp_mode` is a value a caller may change later.
///         `QpMode::kQpModeCount` names no kernel and is refused.
/// M6 W5 T8.3: a one-line forwarder to `validate(const SqpOptions &)`, which is
/// where the body lives now. T8.10 removes this name.
void validate_sqp_options(const SqpOptions &opts);

/// @brief One row of the per-major history -- the record of ONE ITERATE and of
///        the subproblem solved from it, if any.
///
/// This field set is the FORMAL CONTRACT and is additive-only: a field may be
/// appended, but none below may change meaning.
///
/// qp_solved == false marks an iterate the solve stopped AT without building a
/// subproblem from it. Its qp_* fields are meaningless and left at their
/// defaults. There is AT MOST ONE such row and it is always the last; a solve
/// that stopped ON a failing subproblem has none.
///
/// NaN IS A LEGAL VALUE for the four residual fields, on exactly the
/// non-finite-iterate row, so a consumer aggregating them must expect it.
///
/// ONE ROW IS ONE TRIAL, Not one iterate -- which is why the index field is named
/// `trial`. A rejected step leaves the iterate where it was and the driver
/// re-solves the SAME subproblem at a smaller radius, so consecutive rows can
/// describe the same point, differing only in tr_radius, the qp_* fields,
/// step_norm and verdict.
///
/// Recovering the iterate sequence. Row 0 is always an iterate the solve stood
/// on; after that, row k is a NEW point iff row k-1's step was accepted:
///
///     bool is_new_point = k == 0 ||
///                         history[k-1].verdict == StepVerdict::kAcceptF ||
///                         history[k-1].verdict == StepVerdict::kAcceptH;
///
/// The predicate reads the PREVIOUS row, not this one: filtering on a row's own
/// kReject verdict is wrong in both directions.
///
/// Every scale-carrying column IS ON THE CALLER'S SCALE, whether or not the solve
/// ran with `SqpOptions::enable_scaling` on. The number a scaled solve's
/// convergence test gated on is `SqpScalingReport::scaled_kkt_residual`.
struct SqpIterate {
    /// @brief 0-based index of THIS ROW (== subproblem index).
    Index trial = 0;

    /// @brief Objective at the iterate.
    double f = 0.0;
    /// @brief Reduced/projected ||grad L||inf; see sqp_driver.h's CONVERGENCE TEST.
    double stationarity = 0.0;
    /// @brief max(||cE||inf, max(cI)+, bound violation).
    double feasibility = 0.0;
    /// max_j |lambda_i(j) * cI_j(x)|. RECORDED, NOT GATED -- see
    /// sqp_driver.h's CONVERGENCE TEST note for the argument, and its THE
    /// INGESTED MULTIPLIERS ARE MADE COMPLEMENTARY note for how the ingested
    /// multipliers were made complementary BY CONSTRUCTION rather than by
    /// adding a third conjunct to the test.
    double complementarity = 0.0;
    /// max(stationarity, feasibility) -- the scalar the contraction test
    /// reads.
    double kkt_residual = 0.0;
    /// h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)) at the iterate: the
    /// GLOBALIZATION measure (sqp_driver.h's constraint_violation_l1), a
    /// different quantity from `feasibility` above -- l1 vs inf-norm, and
    /// bounds excluded vs included. Recorded because it is what the funnel
    /// judges against, so the funnel's own guarantee (h stays inside a
    /// monotonically tightening width) is checkable from the history alone.
    double violation_l1 = 0.0;
    /// The l-infinity trust-region radius THIS subproblem was solved at,
    /// i.e. the SolveOverrides::tr_radius the driver passed. On a
    /// stopped-AT-iterate row it is the radius the next subproblem would
    /// have been given.
    double tr_radius = 0.0;
    /// The SolveOverrides::dual_mu THIS subproblem was solved at (the MAIN
    /// trial's QP -- a subsequent SOC/elastic re-solve on the same row uses
    /// its own separate override, per sqp_driver.h's ADAPTIVE DUAL
    /// REGULARIZATION note, so it is not what this field describes there).
    /// Meaningful iff qp_solved; 0.0 on a stopped-AT-iterate row, like the
    /// other qp_* fields. Always opts.qp.dual_mu (the engine default) when
    /// SqpOptions::adaptive_mu is false -- byte-identical across the whole
    /// history.
    double mu = 0.0;
    /// ||p||inf of the step taken FROM this iterate. ON A ROUTED QP-FAILURE ROW
    /// no step was taken: the field records the |p|inf of the iterate the failed
    /// solve returned, which the driver discarded -- diagnostic there, and it must
    /// not be summed into a path length.
    ///
    /// ON A SOC-corrected row this is still ||p||inf of the ORIGINAL, rejected QP
    /// step, not of the SOC re-solve's own step, which is the one the iterate
    /// actually moved by. That keeps this field, and every reader of it, describing
    /// exactly one QpSolution's own box-respecting step.
    double step_norm = 0.0;
    /// @brief False on a stopped-AT-iterate row (see above).
    bool qp_solved = false;

    // --- The interior-point tier's infeasibility evidence, where it arrives ---
    //
    // Two scalars off `IpqpInfeasibilityEvidence`, written by the escape branch's
    // hook (`certified_feasibility_fallback`) on the major it fires on, and
    // zero/false on every other row -- including every row of a kWalk or kSsn
    // solve, where no tier runs.
    //
    // They make the arrival OBSERVABLE: they are the evidence block's own headline
    // scalars, and a call site that substituted default evidence would report 0
    // and false here.
    //
    // NOT A CERTIFICATE: the escape signature is a SUSPICION,
    // `farkas_corroborated == false` means "not corroborated" rather than
    // "withdrawn", and the escape fires either way. Read them as telemetry about
    // why a subproblem was handed on.
    double ipqp_least_infeasible_primal = 0.0;
    bool ipqp_farkas_corroborated = false;
    /// @brief Meaningful iff qp_solved.
    QpStatus qp_status = QpStatus::kOptimal;
    /// @brief Meaningful iff qp_solved.
    Index qp_minor_iters = 0;
    /// @brief Meaningful iff qp_solved.
    Index qp_factorizations = 0;
    /// @brief Any QpSolution::tr_active entry set (radius bit).
    bool tr_binding = false;
    /// The globalization strategy's verdict on this trial. It is the strategy's
    /// OWN verdict iff `qp_solved && qp_status == QpStatus::kOptimal`; on any
    /// other row no strategy was consulted.
    ///
    /// The default is deliberately kReject, and on a failed-subproblem row it is
    /// more than a safe default -- the row really was a rejection. So a consumer
    /// reconstructing the ITERATE sequence may read `verdict != kReject` on the
    /// PREVIOUS row as "the iterate moved" across every row shape.
    StepVerdict verdict = StepVerdict::kReject;
    /// True iff this row's ACCEPTANCE (verdict is kAcceptF/kAcceptH) was
    /// won by a second-order correction, i.e. the strategy rejected the raw
    /// QP step p and a hot-started re-solve rescued it -- the iterate that
    /// follows this row is x + (the SOC re-solve's own step), NOT x + p.
    /// False on every other row, including one where SOC was ATTEMPTED but
    /// its own corrected point was also rejected (that attempt is counted
    /// in SqpCounters::soc_steps, not recorded per-row -- see
    /// sqp_driver.h's SECOND-ORDER CORRECTION note for why a failed attempt
    /// does not get a field of its own). Always false when
    /// SqpOptions::enable_soc is false.
    bool soc_applied = false;
    /// True iff an ELASTIC SOLVE supplied this major: the elastic tier's re-solve
    /// after a plain QP returned kInfeasible, or the certified fallback's rung A
    /// after a kIpm escape. Set on BOTH outcomes -- the row whose step came from
    /// the elastic solve, and the row that ended the solve because the ladder was
    /// exhausted. It is the branch input SOC is gated on.
    ///
    /// ON SUCH A ROW THE qp_* Fields describe the final elastic solve, not the
    /// kInfeasible one that triggered it, with the escalation re-solves' costs
    /// folded into SqpCounters' aggregates only. On a rung-A-owned major they are
    /// the ladder's own counts.
    ///
    /// tr_binding IS STILL MEANINGFUL: the elastic solve gets its trust region as
    /// REAL bounds on the original variables rather than through SolveOverrides,
    /// and the driver re-derives the radius bit exactly as the engine would have.
    bool elastic_applied = false;
    /// True iff THIS row's elastic ladder started at a CLAMPED placement -- the
    /// evidence priced the violation above the escalation headroom or above the
    /// dual_mu safety margin -- including a clamp whose declined ladder was then
    /// retried at the floor. False on every other row. Per-ENTRY, where
    /// `elastic_rho0_ceiling_hits` is per-ladder; they agree only because the
    /// retry's override placement can never clamp.
    bool elastic_rho0_ceiling_hit = false;
    /// True iff THIS row's restoration request started the phase at a CANDIDATE point rather
    /// than at the iterate -- the candidate's MEASURED violation was lower (sqp_driver.h's
    /// RESTORATION PHASE note). False on every other row, INCLUDING a degraded-to-x seed.
    bool restoration_seed_used = false;
    /// True iff the Full-step watchdog restored an earlier best-||KKT||inf
    /// iterate ON THIS PASS, i.e. this row's residual columns describe the
    /// RESTORED point rather than the diverged-or-stalled one the previous row
    /// left off at. Without this flag the history's residual column can jump
    /// backward with nothing in the row explaining why, and the watchdog is the
    /// only thing in this driver that can rebase the iterate backward mid-solve.
    /// False on every other row. At most one row per solve is true today, but the
    /// flag is read per-row, so a future relaxation of that bound needs no format
    /// change here.
    bool watchdog_restored = false;

    // --- The mode-selection telemetry ---
    //
    // Six counts off this major's own QpSolution, read by nothing in this
    // library; the shared reading rules are on `active_set_delta` below.

    /// The symmetric-difference count between THIS major's QP active set and the
    /// previous REPORTING major's: one for every inequality row that entered or
    /// left `QpSolution::ineq_active`, plus one for every variable whose
    /// `bound_state` changed at all (a per-variable state change counts once).
    ///
    /// The first reporting major counts against the empty set, so a solve's first
    /// row is a census of where the first subproblem landed rather than a 0. The
    /// restoration sub-solve is a separate sequence with its own empty start.
    ///
    /// All six fields are meaningful exactly where `tr_binding` IS: they read 0 on
    /// any row whose QpSolution never became this major's answer -- the
    /// stopped-AT-iterate and non-finite-iterate rows, and the exhausted-ladder
    /// row. Such a row also leaves the previous set unchanged, so the next
    /// reporting row's delta is measured against the last set really solved for.
    ///
    /// A TR-HELD VARIABLE IS NOT A Bound side here: every producer of a
    /// QpSolution reports `kFree` for a variable held by a TR-tight effective
    /// bound and flags it in `tr_active` instead. So these fields count REAL bound
    /// sides only, and a change of radius that moves a variable between a real
    /// bound and a TR pin DOES move this delta.
    ///
    /// ON A SOC-corrected row the set IS THE ORIGINAL QP's, on `tr_binding`'s own
    /// convention and for the same reason.
    Index active_set_delta = 0;
    /// Active inequality rows whose price is negligible beside the largest one:
    /// `|lambda_i(k)| <= kWeakActivityMargin * max(1, ||lambda_i||inf)`. See
    /// that constant for what the measure is and for what it is NOT.
    ///
    /// THE RELATIVE SCALING IS THE POINT AND ALSO THE CAVEAT: on an ill-scaled
    /// family a row priced at 10 reads weak beside one priced at 5e7. Accepted
    /// for telemetry, and stated so a reader does not take it for an absolute.
    Index weak_active_rows = 0;
    /// INACTIVE inequality rows whose slack is negligible beside the largest
    /// one: `s(k) <= kWeakActivityMargin * max(1, ||s||inf)`, where
    /// `s = bi - Ai p` is the QP's OWN nonnegative row slack (`qp_problem.h`'s
    /// `Ai x <= bi`) -- the units `ineq_active` is decided in, NOT the NLP row
    /// values `NlpEval::ci` carries.
    ///
    /// A VIOLATED ROW (`s(k) < 0`, reachable only on a non-optimal exit) is
    /// counted: it is at or past its own boundary, which is what this field
    /// reports about.
    Index near_active_rows = 0;
    /// Active inequality rows (`ineq_active` entries set).
    Index active_rows = 0;
    /// Variables at their real LOWER bound: `kAtLower`, plus `kFixed`, which is
    /// at both sides at once and so is counted in both census fields.
    Index active_lower_sides = 0;
    /// Variables at their real UPPER bound (`kAtUpper`, plus `kFixed`).
    Index active_upper_sides = 0;
};

/// @brief Result of a whole SQP solve.
///
/// MULTIPLIERS. lambda_e/lambda_i are the LAST SUBPROBLEM'S multipliers, carried
/// out unchanged -- nlp_model.h's sign convention makes them the NLP's
/// multipliers at the returned point with no flip. z is NOT the subproblem's z;
/// it is the MODEL-IMPLIED bound multiplier at the returned point, computed from
/// grad L there, because the QP's z is forced to 0 at a TR-pinned index.
///
/// On a non-kOptimal exit x is the FINAL ITERATE reached and the multipliers are
/// whatever the last successful subproblem priced -- NOT cleared, because here
/// they are the caller's evidence about where the driver stopped.
///
/// On a certified kInfeasible exit the multipliers mean something stronger and
/// DIFFERENT, and must not be read as prices of the NLP's own constraints: they
/// are the RESTORATION problem's multipliers, a SUBGRADIENT CERTIFICATE that the
/// returned x is a stationary point of h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)).
/// They satisfy
///     Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,
///     lambda_e in [-1, 1]^me,  lambda_i in [0, 1]^mi,
///     lambda_e(i) = sign(cE_i(x)) wherever cE_i(x) != 0,
///     lambda_i(j) = 1 wherever cI_j(x) > 0 and 0 wherever cI_j(x) < 0,
/// with no grad f term -- which is what makes it a certificate of infeasibility
/// rather than of optimality. z on this exit is the RESTORATION problem's bound
/// price for the same reason: grad f is precisely the term a subgradient of h
/// must not contain.
///
/// The one exception is the non-finite-iterate kNumericalError exit, where
/// lambda_e/lambda_i/z are all cleared: at a NaN iterate nothing was measured. f
/// is still reported as the model returned it, which is to say possibly NaN.
///
/// history describes every iterate visited; whether the last row is an iterate
/// row or a failing-subproblem row depends on the exit -- see SqpCounters.
/// @brief What the problem-scaling layer did to a solve, reported on its
///        solution.
///
/// A DIAGNOSTIC, NOT A COUNTER, and deliberately not a member of SqpCounters: a
/// unit factor neither sums nor peaks meaningfully across the restoration fold,
/// and the sub-solve runs unscaled by design. An INACTIVE report is the identity,
/// field for field, so a reader never has to branch on `active`.
struct SqpScalingReport {
    /// True when the solve ran scaled, i.e. `SqpOptions::enable_scaling` was on
    /// and factors were installed.
    bool active = false;
    /// The objective factor `sf`. Multiply a caller-scale objective by it to
    /// reach the engine's; divide to come back. 1.0 when inactive.
    double obj = 1.0;
    /// Largest and smallest constraint-row factor over the equality and
    /// inequality blocks together, or 1.0 when the problem has no rows (a
    /// bound-constrained problem has nothing to equilibrate). Their ratio is the
    /// row conditioning the layer removed.
    double row_max = 1.0;
    double row_min = 1.0;
    /// THE SCALED RESIDUAL THE CONVERGENCE TEST ACTUALLY READ, at the returned
    /// point -- the counterpart of SqpSolution::kkt_residual, which is on the
    /// CALLER's scale. On an inactive solve the two are the same number by
    /// construction. On an active one their difference is exactly the gap
    /// SqpOptions::enable_scaling discloses, and reporting both is what makes it
    /// computable. NaN whenever SqpSolution::kkt_residual is NaN, for the same
    /// reason: nothing was measured.
    double scaled_kkt_residual = std::numeric_limits<double>::quiet_NaN();
};

struct SqpSolution {
    /// @brief How the solve ended.
    SqpStatus status = SqpStatus::kOptimal;
    /// The returned point and its prices: primal variables, equality
    /// multipliers, inequality multipliers, and bound multipliers -- read
    /// under the exit-dependent contract above.
    Vec x, lambda_e, lambda_i, z;
    /// Objective value at `x`, exactly as the model returned it (possibly NaN
    /// on a kNumericalError exit).
    double f = std::numeric_limits<double>::quiet_NaN();
    /// @brief Work spent by this solve, restoration folded in.
    SqpCounters counters;
    /// One row per iterate visited; whether the last row is an iterate row or a
    /// failing-subproblem row depends on the exit -- see SqpCounters.
    std::vector<SqpIterate> history;

    // The terminal KKT measurement, taken at the RETURNED (x, lambda_e, lambda_i)
    // by the same evaluate_kkt call the convergence test read. They exist so a
    // consumer can fill an outcome record without reconstructing the history's
    // exit shape: the last history row is NOT reliably the returned point.
    //
    // All four are NaN on the non-finite-iterate kNumericalError exit: nothing was
    // measured there, and a 0.0 would read as a converged residual.
    //
    // On a certified kInfeasible exit they measure the NLP's own KKT conditions at
    // the returned point -- an INFEASIBLE point, so `feasibility` is large by
    // construction and `stationarity` is the ordinary grad-L measure, NOT the
    // subgradient certificate's residual.
    //
    // One qualification on "at the returned multipliers": when
    // `counters.ssn.ssn_sign_swept > 0` the sign sweep clamped negative
    // inequality prices AFTER this measurement, so these four describe the
    // PRE-SWEEP multipliers while `lambda_i` holds the swept ones. `stationarity`
    // is then optimistic by at most `ssn_sign_sweep_max * ||Ji||inf` over the
    // swept rows, and `complementarity` can only be over-stated.
    /// @brief Reduced/projected ||grad L||inf at the returned point.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(||cE||inf, max(cI)+, bound violation) at the returned point.
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    /// @brief max_j |lambda_i(j) * cI_j(x)| at the returned point. Recorded,
    ///        not gated -- see SqpIterate::complementarity. At
    ///        `ssn_sign_swept > 0` this is taken at the pre-sweep multipliers
    ///        and can only be over-stated (see the note above).
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(stationarity, feasibility) -- the scalar the convergence
    ///        test gates on.
    double kkt_residual = std::numeric_limits<double>::quiet_NaN();

    /// Wall-clock seconds spent inside this solve, measured with
    /// std::chrono::steady_clock around the driver's solve_impl ALONE -- never
    /// around model construction, the bridge or seam lay, the staged-value ingest
    /// or the ledger bookkeeping.
    ///
    /// INFORMATIONAL, NEVER ASSERTED -- counters, not timings, are this project's
    /// currency of correctness. Nothing may gate on it. Defaults to 0.0; every
    /// public solve() that returns writes a value >= 0.0.
    double wall_seconds = 0.0;

    /// @brief What the problem-scaling layer did to this solve; the identity
    ///        when it was off, which is the shipped default.
    ///
    /// READ IT BEFORE COMPARING `f`, the multiplier blocks or the four terminal
    /// residuals against another solve's: all of those are reported on the
    /// CALLER's scale whatever this says, so they are directly comparable -- but
    /// `scaled_kkt_residual` is the number the convergence test gated on, and on
    /// an active solve it is the one that was required to be <= kkt_tol.
    SqpScalingReport scaling;

    /// True ONLY on the certified infeasibility exit: the restoration phase ran to
    /// its own KKT test, that test passed, and h at the returned point is still
    /// above feas_tol -- so (x, lambda_e, lambda_i, z) is the subgradient
    /// certificate documented above.
    ///
    /// False on EVERY OTHER exit, including other kInfeasible ones, and that is
    /// what this flag is for. It is NOT reliably derivable from the status or the
    /// counters: a solve that restored once and then met a second request, and a
    /// solve whose restoration was itself stuck, both report kInfeasible with
    /// restoration_iters > 0. Nor is the last row's verdict a reliable
    /// discriminator: it is kRestore from the funnel's signature or the elastic
    /// tier's exhaustion but kReject from the radius floor, and any of the three
    /// kInfeasible outcomes can pair with either shape.
    ///
    /// READ IT AS A One-way guarantee: true means the certificate holds; false
    /// means NO CLAIM IS MADE about the model.
    bool infeasibility_certified = false;

    /// The solve's exit state in warm_start.h's shape, for a LATER solve of a
    /// nearby problem to feed back in. Populated on EVERY exit of
    /// SqpDriver::solve(), including a failed one, from the best-known iterate.
    /// Populated is NOT the same as `valid`: one exit -- a start point the model
    /// could not evaluate -- fills these fields for inspection but reports
    /// valid == false, because feeding that point back would override the
    /// caller's own corrected x0.
    WarmStart warm_start;
};

} // namespace hven::solvers
