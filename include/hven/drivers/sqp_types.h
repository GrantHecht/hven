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
#include <vector>

#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/detail/globalization/sqp/globalization.h>
#include <hven/detail/warmstart/warm_start.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

/// Which kernel solves each SQP subproblem. kWalk is the primal active-set
/// WALK (qp_engine.h): one working-set change per minor iteration, one
/// blocking constraint at a time. kSsn is the SEMISMOOTH-NEWTON kernel
/// (ssn_engine.h): a Newton method on a Fischer-Burmeister reformulation of
/// the QP's own KKT conditions, whose step changes the whole implied active
/// set AT ONCE (the PDAS "bulk flip") -- the property whose value shows up in
/// the NUMBER of minors, not in the cost of one.
///
/// THE DEFAULT IS kWalk AND MUST STAY SO absent an explicit ruling otherwise:
/// the walk is what every pin, battery and published figure in this
/// repository was measured on.
enum class QpMode {
    kWalk,
    kSsn,
};

// Declared HERE rather than in ssn_engine.h for the same reason QpMode is:
// SqpOptions carries them and sqp_types.h is the header ssn_engine.h
// includes, not the other way round. Their SEMANTICS live at their SsnOptions
// fields (ssn_engine.h), which is also where each one's mechanism is derived;
// this file only declares the alphabet and the defaults.
//
// **EVERY DEFAULT BELOW IS THE SHIPPED ITERATION, BIT FOR BIT**, and no
// default is flipped by the change that adds its lever.

/// HOW THE PROXIMAL/LEVENBERG-MARQUARDT SHIFT sigma IS SIZED.
///
/// - kLadder: the shipped failure-reactive ladder -- arm at 1e-6, x100 per
///   escalation trigger, cap 1e6 (ssn_engine.h's
///   detail::kSsnProxInit/Growth/Max).
/// - kResidualArmed: once the ladder has ARMED, size sigma from the residual
///   instead of climbing: sigma_k = c*min(r, r^2) with r = ||F_k||inf
///   scale-normalized, floored at the ladder's own first rung and capped at
///   its ceiling. The ladder is retained underneath as a monotone floor, so a
///   kSingular factorization still escalates and the escape route is
///   unchanged. INERT on any solve whose ladder never arms, which is every
///   benign fixture.
/// - kResidualAlways: as kResidualArmed, but sigma is sized from the residual
///   from the FIRST attempt rather than from the first arming -- the
///   proactive form the Levenberg-Marquardt theory is stated for
///   (Yamashita-Fukushima 2001, Fan-Yuan 2005). NOT inert anywhere: every
///   solve carries at least the floor.
enum class SsnSigmaRule {
    kLadder = 0,
    kResidualArmed = 1,
    kResidualAlways = 2,
};

/// WHAT PROTECTS THE HINTED FIRST STEP.
///
/// - kIterationZeroFree: the shipped rule -- iteration 0 is exempt from the
///   Armijo test when a hint governed it, with no safety net.
/// - kWatchdog: Chamberlain-Powell-Lemarechal-Pedersen (Math. Prog. Study 16,
///   1982) -- up to q relaxed steps, and if sufficient decrease has not
///   materialized by then, RETURN TO THE BEST STORED POINT and take a
///   monotone step from there. Reproduces the exemption exactly when the
///   hinted step works and closes the failure it cannot see when it does
///   not.
enum class SsnHintRule {
    kIterationZeroFree = 0,
    kWatchdog = 1,
};

/// WHAT TURNS AN INFEASIBILITY SUSPICION INTO AN EXIT.
///
/// - kSymptoms: the shipped conjuncts -- a stalled residual window plus
///   diverging multipliers (ssn_engine.h's kSsnStallWindow block).
/// - kFarkasGated: the symptoms ARM the check and a CERTIFICATE fires it: the
///   dual increment is projected onto the sign cone, normalized, and tested
///   as an approximate Farkas direction (one matvec plus O(m), no
///   factorization). A symptom not accompanied by a Farkas direction no
///   longer exits.
enum class SsnInfeasibilityRule {
    kSymptoms = 0,
    kFarkasGated = 1,
};

// THE FULL-STEP-FIRST WARM RULE'S TWO CONSTANTS. Both are WATCHDOG thresholds:
// they bound how long the driver may keep taking undamped steps under
// globalization.h's full-step mode before restoring the best iterate it saw
// and handing the solve back to the funnel. Neither is a paper constant --
// [KD] (globalization.h's THE FULL-STEP MODE note) argues for the rule and
// for a watchdog fallback but gives no schedule, exactly as [KLV] gives none
// for its own alpha_min -- so both are implementation choices, chosen against
// what the residual sequence of a healthy warm solve actually looks like on
// this project's own fixtures.
//
// kWarmResidualGrowthMax = 2 (CONSECUTIVE majors with a GROWING ||KKT||inf).
// One growth is not evidence: a warm SQP step routinely overshoots once and
// then contracts quadratically. MEASURED on the perturbed-HS7 family of
// tests/sqp/test_warm_start.cpp, warm from HS7's own solution: at eps = 1 the
// residual sequence 1.0 -> 2.1e-1 -> 3.8e-3 -> 1.3e-6 is monotone, while at
// eps = 10 the same family goes 1.0e1 -> 6.7 -> 7.1 (a rise) -> 2.1 -> ...
// and still converges in 7 majors. A single-growth threshold would abort that
// run for nothing. TWO IN A ROW is the smallest count no single overshoot can
// produce, and every count above it buys a full major spent moving AWAY from
// a solution.
//
// kWarmFullStepWindow = 5 (majors without a NEW BEST ||KKT||inf). The other
// failure shape: not divergence but a stall -- unit steps that neither
// improve on the best iterate nor grow monotonically (a cycle, or a
// repeatedly routed QP failure at an iterate that never moves). Five is one
// order of magnitude above the local regime this mode targets: a warm start
// inside a nearby solution's Newton domain converges in 1-2 majors and cannot
// spend five without a new best, so the window can only fire on a run the
// mode's own premise has already failed for.
//
// CHANGING EITHER IS A BEHAVIOUR CHANGE ON EVERY WARM SOLVE, not a tuning
// detail: tests/sqp/test_warm_start.cpp pins the majors of both a converging and
// a watchdog-restored run against these exact values.

/// Watchdog threshold: consecutive majors with growing ||KKT||inf tolerated
/// under the full-step mode before it restores the best iterate.
inline constexpr Index kWarmResidualGrowthMax = 2;

/// Watchdog threshold: majors without a new best ||KKT||inf tolerated under
/// the full-step mode before it restores the best iterate.
inline constexpr Index kWarmFullStepWindow = 5;

/// Driver options for the whole SQP solve.
///
/// TOLERANCES. kkt_tol gates the STATIONARITY measure and feas_tol gates the
/// FEASIBILITY measure; both measures are defined precisely in sqp_driver.h's
/// CONVERGENCE TEST note, which is the contract. feas_tol does double duty as
/// the geometric BOUND-ACTIVITY tolerance of that measure (a variable within
/// feas_tol of a bound is treated as sitting on it), so there is no separate
/// activity-tolerance knob -- see that note for why the two are deliberately
/// the same number.
///
/// TRUST REGION. tr_init is the l-infinity radius the FIRST subproblem of a
/// solve is given, through SolveOverrides::tr_radius (qp_types.h); from there
/// the driver's radius-management loop takes over (sqp_driver.h's RADIUS
/// MANAGEMENT note) -- doubling it on a strong accepted step up to the tr_max
/// ceiling, halving it on a rejected one, and never below the tr_min FLOOR.
/// tr_max must be >= tr_init. Setting tr_init to +inf resolves, per
/// SolveOverrides' sentinel convention, to `qp.tr_radius` (itself +inf by
/// default), i.e. no trust region -- legitimate, but an indefinite
/// subproblem is then generally unbounded and the engine reports
/// kNumericalError rather than a step. THE FIRST SHRINK FROM +inf RESOLVES TO
/// tr_max (inf/2 is inf, so the shrink rule needs a finite landing value the
/// first time it fires), after which ordinary halving applies and the solve
/// behaves exactly like one started at tr_max (sqp_driver.h's RADIUS
/// MANAGEMENT note explains why tr_max is the landing value). A solve that
/// never rejects a trial still never materializes a radius at all, so the
/// "no trust region" reading of +inf is intact wherever it was meaningful.
///
/// tr_min IS THE RADIUS FLOOR, and reaching it is an EVENT rather than a
/// clamp: KLV Algorithm 4's restoration trigger is "alpha < alpha_min (step
/// size too small)", whose trust-region analogue is exactly this, so a shrink
/// that would go below tr_min instead enters the RESTORATION PHASE
/// (sqp_driver.h). The radius is therefore never left spinning at the floor.
///
/// THE DEFAULT tr_min (1e-10) IS AN IMPLEMENTATION CHOICE, like alpha_min
/// itself, which KLV also leaves without a value. It is chosen against two
/// requirements: far enough below the tolerances that it can never pre-empt a
/// legitimately small step (with kkt_tol/feas_tol at 1e-6, 1e-10 is four
/// orders below the resolution at which any residual is judged), and far
/// enough below tr_init that reaching it is evidence rather than noise (from
/// tr_init = 1 it takes 34 consecutive halvings, i.e. 34 rejections at ONE
/// iterate, which no healthy solve produces). A caller whose problem is
/// scaled so that meaningful steps are smaller than 1e-10 should lower it;
/// one who wants restoration entered sooner should raise it.
///
/// MAX_ITER BOUNDS SUBPROBLEMS SOLVED, not accepted steps -- a rejected trial
/// costs one, exactly like an accepted one, because it costs a QP solve. THE
/// BUDGET IS SHARED WITH THE RESTORATION PHASE: the quantity bounded is
/// major_iters + restoration_iters, not major_iters alone, because a
/// restoration major costs a QP solve on the feasibility problem exactly as
/// an optimality major costs one on the subproblem. A solve that spends 12
/// majors restoring has 12 fewer available to the main loop, and the total
/// work of a solve is bounded by max_iter subproblems however it is split.
/// history.size() still tracks major_iters ALONE (restoration produces no
/// history rows of its own -- see SqpIterate), so history.size() ==
/// major_iters + 1 on an iterate exit is unchanged.
///
/// GLOBALIZATION STRATEGY. make_strategy is called ONCE PER solve() call and
/// must return a non-null, freshly resettable GlobalizationStrategy
/// (globalization.h); the driver owns the returned object for that solve, so
/// two solves never share funnel state and solve() stays repeatable. Empty
/// (the default) means FunnelStrategy -- KLV's funnel, this project's default
/// globalization. A factory that returns nullptr is a caller error and
/// solve() throws std::invalid_argument.
///
/// qp is copied into the driver's single QpEngine instance at construction,
/// so per-solve variation goes through SolveOverrides, never through this
/// struct (see qp_types.h's PER-INSTANCE, CONST note on QpOptions::tr_radius).
///
/// SECOND-ORDER CORRECTION. enable_soc defaults ON: it is this project's
/// cheap edge over Uno, which omits SOC entirely. When a kReject trial's
/// constraint violation INCREASED (h_new > h_old -- the Maratos signature;
/// sqp_driver.h's SECOND-ORDER CORRECTION note), the driver spends one extra
/// hot-started QP re-solve before shrinking the radius, to try to rescue the
/// step from a pure curvature artifact rather than discard it.
///
/// "NEAR-FREE" IS SCOPED TO THE DESIGN REGIME -- a small residual near a
/// solution (the Maratos regime this feature targets), where the re-solve is
/// measured to cost 0 extra factorizations (SqpDriverSoc.SocDefeatsMaratos).
/// Far from a solution -- a large residual, the common case when this fires
/// incidentally rather than by design -- it is a REAL, full extra QP solve
/// that MOST OFTEN (measured: 79% on this file's own pre-existing suite)
/// returns kInfeasible outright: see sqp_driver.h's A FAILED SOC RE-SOLVE
/// note for the measured cost table and the reason (the rhs shift scales with
/// the very violation that triggered SOC, so a large h_new brings a large,
/// often box-busting correction target). Every attempt is still bounded and
/// cheap-to-FAIL (a handful of minor iterations, at most a few factorizations,
/// one attempt per rejected trial), which is why no magnitude gate is applied
/// before attempting.
///
/// ADAPTIVE DUAL REGULARIZATION. adaptive_mu defaults ON: it schedules
/// dual_mu down with the KKT residual, closing the accuracy ceiling a fixed
/// engine dual_mu leaves on a badly-scaled active set (iterative refinement
/// alone leaves measured relative error ~1e-4 -- see sqp_driver.h's ADAPTIVE
/// DUAL REGULARIZATION note and tests/sqp/test_sqp_driver.cpp's
/// AdaptiveMuRecoversTailAccuracy). primal_delta is NOT scheduled by this
/// lever or by anything else in the driver; see that same header note for why
/// the schedule is deliberately dual_mu-only.
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

    /// Adaptive dual regularization A/B lever: when ON, schedules
    /// SolveOverrides::dual_mu down with the KKT residual instead of leaving
    /// every subproblem at the engine's fixed QpOptions::dual_mu, which is
    /// what closes the fixed-mu accuracy ceiling on a badly-scaled active
    /// set (measured relative error ~1e-4). Default true. Set false to
    /// recover exact engine-default-mu behaviour -- SolveOverrides::dual_mu
    /// is then left at its sentinel every major. primal_delta is NOT
    /// scheduled by this lever or by anything else in the driver. See the
    /// ADAPTIVE DUAL REGULARIZATION note above (and sqp_driver.h's, for the
    /// schedule itself).
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

    /// A CEILING on the level solve(model, x0, warm) (the 3-arg overload,
    /// sqp_driver.h's WARM-START INGEST note) is allowed to resolve to,
    /// independent of what `warm` itself would otherwise justify -- the same
    /// kind of A/B lever enable_soc/adaptive_mu already are.
    ///
    /// At StartLevel::kSeeded a driver ingests a hash-less object's values
    /// (warm_start.h's StartLevel note) but never a factorization, never the
    /// funnel/TR state and never the Kungurtsev-Diehl window, EVEN when the
    /// object would have earned kWarm. It also short-circuits the
    /// structural-hash resolution probe entirely -- the probe's answer could
    /// only raise the level above the ceiling, so it is never paid.
    ///
    /// Default StartLevel::kWarm: kHot is reachable -- a `warm` carrying a
    /// non-null `hot` handle whose structure matches resolves there -- but
    /// is opt-in, not the default, since it hands this instance's engine a
    /// factorization possibly built by a DIFFERENT engine instance (see
    /// qp_engine.h's HotState OWNERSHIP note for the single-use,
    /// chained-hand-off invariant that comes with accepting one). Raise this
    /// to StartLevel::kHot to let the resolution reach it; set it to
    /// StartLevel::kCold to force EVERY solve through the 3-arg overload to
    /// behave exactly as the 2-arg one does, regardless of `warm` -- e.g.
    /// for an A/B comparison of warm vs. cold on the same problem sequence.
    /// Does NOT affect solve(model, x0): that overload is always cold by
    /// construction (there is no `warm` object to resolve).
    StartLevel start_level = StartLevel::kWarm;

    /// The KUNGURTSEV-DIEHL FULL-STEP-FIRST RULE: when ON, a solve whose
    /// warm-start level RESOLVED to kWarm or above (never a cold one, and
    /// never the 2-arg solve overload) begins in globalization.h's
    /// full-step mode -- unit steps, the funnel test bypassed -- with a
    /// WATCHDOG that restores the best iterate by ||KKT||inf and hands the
    /// solve back to ordinary funnel globalization the moment the residual
    /// grows kWarmResidualGrowthMax majors in a row, or kWarmFullStepWindow
    /// majors pass without a new best (both constants above). Default true:
    /// [KD]'s observation that standard globalization actively INTERFERES
    /// with warm starts is the reason the warm-start subsystem exists at
    /// all, and a warm solve that re-globalizes from scratch forfeits most
    /// of the advantage warm-starting buys.
    ///
    /// THE A/B LEVER, exactly like enable_soc/adaptive_mu: set false and
    /// every warm solve behaves precisely as with the mode off (the funnel
    /// judges the first trial like any other).
    ///
    /// IT CANNOT AFFECT a cold solve, a solve whose `warm` failed to
    /// resolve, or a solve running a caller-supplied make_strategy that is
    /// not a FunnelStrategy -- the mode is that class's own state
    /// (sqp_driver.h's FULL-STEP-FIRST WARM RULE note has all three
    /// engagement conditions). NOR DOES IT EVER CERTIFY ANYTHING: it changes
    /// which trials are ACCEPTED, never the KKT test that decides kOptimal.
    bool warm_full_step = true;

    /// BUDGETED MODE: when true, a solve that exhausts max_iter in the MAIN
    /// optimality loop (the "converged" test never fires and iter +
    /// restoration_iters reaches max_iter) reports
    /// SqpStatus::kBudgetExhausted rather than kMaxIter, and
    /// SqpSolution::x/lambda_e/lambda_i/z/f are the BEST ITERATE THIS SOLVE
    /// VISITED BY THE FUNNEL'S OWN ORDERING -- feasibility-first: min h(x)
    /// (violation_l1), tie-break min f -- rather than the last iterate
    /// reached. warm_start is populated from that same best iterate rather
    /// than the last one FOR x/lambda_e/lambda_i/z AND THE ACTIVITY VECTORS
    /// ONLY; its OWN globalization/regularization fields -- tr_radius,
    /// funnel_width, primal_delta, dual_mu -- describe the LAST pass this
    /// solve measured (the one at which max_iter was hit), NOT the best
    /// iterate's own row (sqp_driver.h's BUDGETED MODE note has the
    /// mechanics).
    ///
    /// WHY THIS ORDERING AND NOT MIN-||KKT||inf (the full-step watchdog's
    /// own ordering): the two exist for different consumers asking different
    /// questions. The watchdog asks "which iterate is closest to a KKT
    /// point", because that is exactly what the convergence test it protects
    /// gates on. Budgeted mode asks "what point should a CONTINUATION DRIVER
    /// pick up from" -- and the next solve's own globalization has to
    /// re-earn feasibility from scratch regardless of where it starts, so a
    /// point with a small stationarity residual but a large constraint
    /// violation is a WORSE hand-off than a clearly feasible point with a
    /// mediocre objective: it is exactly the feasible-ish starting position
    /// a warm start exists to provide. A min-KKT choice can therefore hand
    /// back a point the funnel itself would refuse to accept a step from --
    /// tests/sqp/test_warm_start.cpp's BudgetReturnsUsableIterate is built on a
    /// fixture where the two orderings disagree for exactly this reason.
    ///
    /// DEFAULT FALSE: off reproduces plain kMaxIter-at-the-last-iterate
    /// behaviour byte-identically. The same kind of A/B lever
    /// enable_soc/adaptive_mu/warm_full_step already are.
    ///
    /// SCOPE: this lever governs ONLY the main loop's own max_iter
    /// exhaustion (the "stopped AT an iterate" exit family, SqpCounters'
    /// note). It does NOT change what happens when the RESTORATION PHASE
    /// itself runs out of the shared budget mid-restoration
    /// (sqp_driver.h's RESTORATION PHASE note, the "no budget left to
    /// restore with" exit) -- that stays kMaxIter regardless of this flag,
    /// because it answers a different question ("restoration could not
    /// finish"), not "here is a usable point to continue from".
    bool budget_mode = false;

    /// THE ELASTIC LADDER'S STALL EARLY-EXIT, OPT-IN: when ON, the rho
    /// escalation ladder (sqp_driver.h's THE ELASTIC TIER note,
    /// kElasticRhoInit -> kElasticRhoMax) stops the moment one escalation
    /// leaves the augmented solution within kElasticStallScale (a
    /// NUMERICAL-ZERO tolerance, not a literal bit-for-bit test) of where
    /// the PREVIOUS rung left it, instead of always spending all six.
    /// Default FALSE -- OFF is the default and reproduces the driver's
    /// behaviour without the lever EXACTLY, everywhere (the
    /// crash_basis pattern, not the enable_soc one).
    ///
    /// WHY OFF BY DEFAULT, because the alternative was tried and measured
    /// unsafe as a default. The obvious argument for "one repeat is enough
    /// evidence to stop" is that ONLY g's slack block reads rho, so a rung
    /// whose solution repeats the previous one looks rho-invariant.
    /// MEASURED, this is closer to "the drift stays small" than to "the
    /// corner never moves": on SqpDriverElastic.InconsistentLinearizationRecovers
    /// the rungs are NOT bit-for-bit identical -- the solution drifts
    /// (|dx|inf 3.6e-13 at rho 1e2->1e3, growing roughly 10x per escalation
    /// to 3.6e-8 by rho 1e7->1e8) and the working set changes at rung 2 (one
    /// slack column flips from a real bound to free) -- but the drift the
    /// WHOLE ladder would accumulate stays four orders of magnitude below
    /// feas_tol/kkt_tol's default 1e-6, and the FIRST escalation's drift is
    /// already sub-threshold, which is why the exit fires there safely. That
    /// safety margin comes from most of the relaxed slacks being pinned at a
    /// REAL BOUND (the fixtures' hand-derivations say so explicitly: "p0
    /// runs to its bound for every rho >= 1"), which bounds how much of the
    /// reduced system CAN read rho at all -- not a proof that it reads none
    /// of it.
    ///
    /// It is UNSAFE in general, with a concrete counter-example in this
    /// project's own suite: tests/sqp/test_sqp_restoration.cpp's
    /// InfeasibleCircleLineModel, whose elastic relaxation's augmented
    /// objective is EXACTLY CONSTANT on the ENTIRE feasible set at every rho
    /// (Lagrangian Hessian identically zero, and the unrelaxed row forces
    /// the relaxed row's residual to be identically 1 on the feasible set --
    /// see sqp_driver.h's STALL EARLY-EXIT note for the derivation). No
    /// point on that set is ever more optimal than any other, at any rho, so
    /// a later rung is not finding progress a one-repeat test missed -- it
    /// is an O(1) tie-break getting lost against rho's growing scale in the
    /// engine's own arithmetic (the SAME mechanism
    /// ElasticSurvivesALargeConstraintScale's banner documents for the scale
    /// knob S). Traced by hand at one of its own majors: rungs 0-4 (rho =
    /// 1e2..1e6) agree to a few ULP and rung 5 (rho = 1e7) moves to a
    /// different point of the same flat set. A caller stopping after rung 1
    /// was not wrong that no further optimality progress existed -- none
    /// did, at any rho -- but they ended up at an ARBITRARY point among many
    /// equally optimal ones, and that choice is not stable under this lever.
    /// MEASURED on the corpus this fixture's class comes from
    /// (tests/sqp/support/hs_sweeps.h's HS15, cold arm): turning this lever
    /// on reshapes the TRAJECTORY -- majors 86 -> 63, elastic_activations
    /// 48 -> 27, accepted/rejected 70/16 -> 49/14 -- because an early
    /// tie-break returning a different point propagates into different
    /// subproblems for the rest of the solve. The corpus's OTHER
    /// elastic-active problem, HS10, is the opposite case: majors,
    /// accept/reject split and objective are IDENTICAL at every grid point
    /// with the lever on, for a free 6x escalation cut (336 -> 56) and 560
    /// fewer minor iterations -- so that corpus is one cost-only case and
    /// one reshaped case, not two reshaped cases. Every configuration
    /// measured, on every fixture class, still reaches a CORRECT final
    /// answer (no wrong kOptimal, no wrong certificate), and every measured
    /// trajectory change was in the IMPROVING direction -- so the case for
    /// off is "unpredictable which arbitrary optimum you get", not "measured
    /// harm" anywhere. That is nonetheless not the "cost only, ever"
    /// property needed to justify an on-by-default flip, and no cheap,
    /// general runtime test tells the two fixture classes apart:
    /// QpSolution::bound_state on the slack columns does NOT do it (both
    /// fixtures have a FREE slack column at the rung the decision is made
    /// on -- see sqp_driver.h's STALL EARLY-EXIT note for what a future
    /// change would need to establish before flipping the default, and for a
    /// Hessian-based lead offered instead).
    ///
    /// WHEN IT IS SAFE TO TURN ON: a problem whose elastic relaxation's
    /// relevant slacks are pinned at a REAL bound for the rho range in play
    /// -- hand-derivable the way InconsistentLinearizationModel's and
    /// InconsistentBoundedModel's are, where it is MEASURED to cut the
    /// escalation count 6 -> 1 and 12 -> 2 across the two activations of
    /// RhoEscalationIsBoundedAndSignals with certified outcomes
    /// byte-identical except an informational SqpIterate::violation_l1
    /// reading that can move by ~1e-13 (four to five orders below
    /// feas_tol/kkt_tol's default 1e-6). On a flat-objective (H === 0-like)
    /// relaxation, turning it on is a DELIBERATE trajectory choice, not a
    /// free cost cut -- leave it off unless the caller has checked which
    /// class their own problem falls in.
    bool elastic_ladder_early_exit = false;

    /// THE CRASH BASIS, OPT-IN: when ON, a COLD solve seeds its FIRST QP
    /// subproblem's working set from the activity geometry at x0 -- every
    /// inequality row that is geometrically active there, and every variable
    /// sitting on a finite bound there -- instead of handing the engine an
    /// empty working set and making it rediscover them one ratio test at a
    /// time. Default FALSE, and OFF is byte-identical to the driver's
    /// behaviour without the lever, everywhere (the
    /// elastic_ladder_early_exit pattern, not the enable_soc one).
    ///
    /// THE PREDICATES ARE NOT NEW, AND THAT IS THE WHOLE POINT OF THE
    /// TOLERANCE STORY. Both are the tests this driver ALREADY applies at
    /// every convergence check, at the SAME tolerance
    /// (SqpOptions::feas_tol), reused verbatim rather than re-derived:
    ///
    ///     row j seeded      :=  cI_j(x0) >= -feas_tol
    ///                           (evaluate_kkt's geometric-activity test,
    ///                            sqp_driver.h's INGESTED MULTIPLIERS ARE MADE
    ///                            COMPLEMENTARY note)
    ///     var i at lower    :=  x0(i) - l(i) <= feas_tol,  l(i) finite
    ///     var i at upper    :=  u(i) - x0(i) <= feas_tol,  u(i) finite
    ///                           (evaluate_kkt's at_lower/at_upper, the
    ///                            reduced-stationarity measure's own test)
    ///
    /// So there is NO new constant to calibrate and no second notion of
    /// "active" in the driver: a row the crash basis seeds is exactly a row
    /// the convergence test would price a multiplier on at that point, and a
    /// bound it pins is exactly a bound the stationarity measure would treat
    /// as active there. A variable that satisfies BOTH tests without being
    /// literally fixed is seeded kAtLower (the engine's own start_center
    /// already flips l == u to kFixed before this seed is ingested, so the
    /// only reachable case is a box narrower than 2*feas_tol; the choice is
    /// arbitrary and recorded so it is not mistaken for a derivation).
    ///
    /// A MORE GENEROUS THRESHOLD WAS REJECTED ON EVIDENCE, not on taste. The
    /// QP engine's Dantzig drop rule deliberately SKIPS shifted
    /// (homotopy-admitted) rows, but it does NOT skip a crash-seeded one --
    /// so a row seeded that should not have been can only leave through a
    /// drop, i.e. an OVER-GENEROUS crash basis costs strictly more than no
    /// crash basis at all, while a tight one is at worst free. The threshold
    /// therefore sits exactly at the driver's own definition of active and
    /// no wider.
    ///
    /// COLD ONLY, BY CONSTRUCTION. The seed is built at the first subproblem
    /// of a solve whose resolved start level is kCold, and only when no warm
    /// seed exists; a kWarm/kHot ingest already seeds that same working set
    /// from WarmStart::qp_working_set and this lever cannot touch it.
    /// tests/sqp/test_sqp_driver.cpp's CrashBasisIsInertOnAWarmIngest pins that
    /// a warm solve is bit-identical with the lever on and off.
    ///
    /// NO EXTRA MODEL EVALUATION. Both predicates are read off the FIRST
    /// SUBPROBLEM ITSELF -- build_subproblem sets qp.bi = -cI(x) and
    /// qp.lower/qp.upper = model bounds - x, so `cI_j(x0) >= -feas_tol` is
    /// `qp.bi(j) <= feas_tol` and the two bound tests are
    /// `qp.lower(i) >= -feas_tol` / `qp.upper(i) <= feas_tol` -- never from
    /// a fresh eval_ci call. See sqp_driver.h's crash_basis_seed().
    ///
    /// MEASURED OUTCOME, AND WHY THE DEFAULT IS OFF. On this project's two cold
    /// corpora the lever is close to inert, for a reason that is a MECHANISM
    /// rather than a tuning miss -- on F7 the first subproblem costs 2
    /// minors of 850 (all the identification happens on the SECOND major,
    /// where the engine's own homotopy already supplies a seed within 1.06x
    /// of |W*| at zero minor cost), and no F7 or Hock-Schittkowski start
    /// point has an inequality row within feas_tol of its boundary that the
    /// homotopy would not have admitted anyway. It moves counters only
    /// where the start point genuinely sits on constraints, which the note's
    /// own fixture demonstrates and 5 variable BOUNDS across 3
    /// Hock-Schittkowski problems (HS30, HS33, HS45) exercise. (HS25's
    /// published start point satisfies the bound predicate too, but it
    /// converges in ZERO majors, so no subproblem is ever built for a seed
    /// to be offered to and its counter reads 0;
    /// tests/sqp/test_hs_battery.cpp's corpus A/B asserts the 5, and exercises
    /// HS25 as the "never builds a subproblem" case.)
    bool crash_basis = false;

    /// WHICH QP KERNEL THE DRIVER'S SUBPROBLEMS GO THROUGH. kWalk is the
    /// shipped primal active-set walk (qp_engine.h) and is the DEFAULT;
    /// kSsn selects the semismooth-Newton kernel (ssn_engine.h).
    QpMode qp_mode = QpMode::kWalk;

    /// READ THE PROXIMAL CARRY OFF AN INGESTED WarmStart.
    ///
    /// WHAT IT GATES, AND ONLY IT: whether the FIRST SSN subproblem of a
    /// solve starts its proximal ladder at `WarmStart::prox_sigma` instead
    /// of at 0. The EMISSION side is unconditional and this flag does not
    /// touch it -- a solve always exports what its ladder found, so a caller
    /// can measure the carry without first turning it on. Under
    /// `qp_mode == QpMode::kWalk` the flag is inert in both directions (no
    /// SSN subproblem is solved, and `has_prox_center` is never set on the
    /// object such a solve emits).
    ///
    /// **THE DEFAULT IS OFF BECAUSE THE MEASUREMENT SAYS SO**, not because
    /// the mechanism is unfinished: a lever whose sweep is a null with a
    /// negative tail does not become a default. The sweep -- 7 cells of a
    /// parametric indefinite family (IndefiniteBoxModel at c = 0.25, 0.5,
    /// 0.75, 1.0, 1.5, 2.0, 3.0) plus every Hock-Schittkowski problem whose
    /// ladder arms at all, 23 rows in total, each run with the carry off and
    /// on -- reads: `ssn_prox_updates` DOWN on 2 rows (HS27 7 -> 0, HS11 6
    /// -> 5), UP on 1 (HS15 7 -> 13), unchanged on 20; `ssn_escapes` UP on
    /// 17 of 23 rows and DOWN ON NONE; total factorizations UP on 13 rows,
    /// down on 5, unchanged on 5. TWO of the 23 rows are committed as tests
    /// (tests/sqp/test_sqp_driver.cpp's TheProximalCarryLeverMovesTheLadderAndIsOff-
    /// ByDefault (HS27) and TheProximalCarryHasACommittedCounterExample
    /// (HS15)), deliberately the row that moves and the row that moves the
    /// wrong way; the remaining 21 rows are an un-committed probe.
    ///
    /// THE FACTORIZATION COLUMN'S SIGN WAS CORRECTED BY A COUNTER FIX, worth
    /// stating rather than quietly replacing: an original narrower read of
    /// "factorizations DOWN on 9 rows, up on 1" was measured against a
    /// counter that did NOT count an ESCAPED SSN subproblem's own
    /// factorizations at all (they reached no accumulation site -- see
    /// sqp_driver.h's hand-off note), so the arm that escapes more looked
    /// cheaper for the arithmetic reason that its escapes were free.
    /// Charged honestly, the carry is not cheaper: it escapes more on 17 of
    /// 23 rows and costs MORE factorizations on 13.
    ///
    /// THE MECHANISM is what the escape row always said: starting a
    /// subproblem at a large sigma damps its Newton step toward the current
    /// iterate, so the SSN tier stops contracting sooner and hands the
    /// subproblem to the WALK -- i.e. the carry's dominant measured effect
    /// is to DISABLE the kernel the SSN mode exists to move work onto.
    ///
    /// A second export policy was also measured and rejected: carrying only
    /// a sigma an SSN subproblem CERTIFIED at (rather than the max over the
    /// solve, which is dominated by exhausted ladders that escaped anyway).
    /// Strictly better motivated, strictly less useful -- it leaves 10 of
    /// the 13 factorization-negative rows with nothing to carry, and on the
    /// 3 that survive it costs 1-2 factorizations rather than saving any.
    bool ssn_prox_carry = false;

    /// GOULD'S LEMMA: READ THE CERTIFYING EXIT'S SECOND-ORDER EVIDENCE
    /// OFF THE FACE-EQP FACTORIZATION THE TIER-3 REFINEMENT ALREADY PAYS
    /// FOR.
    ///
    /// At false (the default) a certifying SSN subproblem pays TWO
    /// factorizations beyond its Newton steps: ssn_engine.h's own
    /// second-order verification (its section 7b) and
    /// QpEngine::refine_on_face's exact face solve. Those two factorize
    /// different matrices but answer the same question, and refine_on_face
    /// already REFUSES on a face whose KKT system fails the inertia gate --
    /// so on every ACCEPTED refinement the first one is redundant.
    ///
    /// At true the kernel runs with SsnOptions::defer_certification: the
    /// verification attempt is built but not factorized, the face solve
    /// runs, and an ACCEPTED refinement IS the certificate (the face KKT's
    /// inertia (n_f, m_f, 0) is positive definiteness of the reduced
    /// Hessian on the identified face -- Gould, Math. Prog. 32, 1985),
    /// saving exactly one factorization; a REFUSED refinement falls back to
    /// the deferred verification, at exactly the shipped cost and on exactly
    /// the shipped matrix, so the refusal residue is certified no more
    /// weakly than before.
    ///
    /// **THE SAVING IS PREDICTABLE TO THE UNIT AND THAT IS THE
    /// MEASUREMENT**: total factorizations drop by
    /// `SqpCounters::ssn.ssn_refinements` plus the driver's own trust-region
    /// gate refusals (structurally zero on the shipped corpora -- see
    /// sqp_driver.h's kSsnTrViolationFactor note), and nothing else moves.
    ///
    /// Inert in both directions at `qp_mode == QpMode::kWalk`: no SSN
    /// subproblem is solved, so no certificate is issued and nothing is
    /// deferred.
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
};

/// THE BOUNDARY VALIDATION SqpDriver's constructor runs over SqpOptions.
/// Returns normally on an options object the driver accepts.
///
/// Callers other than the driver may use it -- it is the cheapest way for a
/// front end to reject an options object before building a solver around it
/// -- but the driver validates unconditionally, so calling it first is an
/// optimization, never a prerequisite.
///
/// EVERY PREDICATE IN IT IS WRITTEN AS THE NEGATION OF THE ACCEPTANCE
/// CONDITION so that NaN is rejected rather than admitted, which rests on the
/// build's `-fno-finite-math-only`; that TU's banner carries the argument and
/// the battery pins it by disassembly.
///
/// @param opts The options object to validate.
/// @throws std::invalid_argument, with a message naming the option and the
/// value it had, on any option the driver cannot honour: a non-positive or
/// NaN kkt_tol/feas_tol, a negative max_iter, a non-positive or NaN tr_init,
/// a tr_max below tr_init (a +inf tr_init is exempt from that one check), or a
/// tr_min that is non-positive or above either end of the range it floors.
void validate_sqp_options(const SqpOptions &opts);

/// One row of the per-major history -- the record of ONE ITERATE and of the
/// subproblem solved from it (if any). Per-major record fields: KKT
/// residual, f, violation_l1 (h), tr_radius (Delta), verdict, and the QP
/// counters. Ledger integration is a SEPARATE, aggregate record --
/// ledger.h's SqpSolveRecord is one row per whole driver solve, not per
/// major. THIS FIELD SET IS THE FORMAL CONTRACT and is additive-only: a new
/// field may be appended, but none below may change meaning, since dozens of
/// existing tests read sol.history against exactly this shape.
///
/// qp_solved == false marks an iterate the solve stopped AT without building
/// a subproblem from it -- converged, out of major iterations, or found
/// non-finite. Its qp_* fields are meaningless and left at their defaults.
/// There is AT MOST ONE such row and it is always the last; a solve that
/// stopped ON a failing subproblem instead has NONE (see SqpCounters for the
/// two history shapes and why indexing by major_iters is unsafe).
///
/// NaN IS A LEGAL VALUE for the four residual fields, on exactly the
/// non-finite-iterate row: sqp_driver.h's evaluate_kkt deliberately reports
/// NaN rather than a swallowed 0.0 there, so a consumer aggregating these (a
/// plot, a ledger, a convergence table) must expect it.
///
/// ONE ROW IS ONE TRIAL, NOT ONE ITERATE -- which is why the index field is
/// named `trial` and not `major`. A rejected step leaves the iterate where
/// it was and the driver re-solves the SAME subproblem at a smaller radius,
/// so CONSECUTIVE ROWS CAN DESCRIBE THE SAME POINT: their f, stationarity,
/// feasibility, complementarity, kkt_residual and violation_l1 are
/// bit-identical and only tr_radius, the qp_* fields, step_norm and verdict
/// differ.
///
/// RECOVERING THE ITERATE SEQUENCE. Row 0 is always an iterate the solve
/// stood on; after that, row k is a NEW point iff row k-1's step was
/// accepted:
///
///     bool is_new_point = k == 0 ||
///                         history[k-1].verdict == StepVerdict::kAcceptF ||
///                         history[k-1].verdict == StepVerdict::kAcceptH;
///
/// Note the predicate reads the PREVIOUS row, not this one. "Filter out rows
/// whose own verdict is kReject" is WRONG in both directions: it would drop
/// the rejected row that still describes a real (repeated) iterate, and it
/// would also drop the final stopped-AT-iterate row, whose verdict field is
/// meaningless and sits at its kReject default.
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
    /// ||p||inf of the step taken FROM this iterate. ON A ROUTED
    /// QP-FAILURE ROW (qp_solved && qp_status != kOptimal) NO STEP WAS
    /// TAKEN: the field records the |p|inf of the iterate the FAILED solve
    /// returned, which the driver discarded. It is diagnostic there -- it
    /// says how far the subproblem got before giving up -- and must not be
    /// summed into a path length.
    ///
    /// ON A SOC-CORRECTED ROW (soc_applied == true) THIS IS STILL ||p||inf
    /// OF THE ORIGINAL, REJECTED QP STEP -- NOT the norm of the SOC
    /// re-solve's own step, which is the one the iterate actually moved by.
    /// See sqp_driver.h's SECOND-ORDER CORRECTION note for why: it keeps
    /// this field, and every existing reader of it (including the
    /// step_norm <= tr_radius invariant sweep), describing exactly one
    /// QpSolution's own box-respecting step, unconditionally -- the SOC
    /// re-solve is a SEPARATE solve at the SAME radius and so is ALSO <=
    /// tr_radius on its own, but that is not what this field reports on
    /// such a row.
    double step_norm = 0.0;
    /// @brief False on a stopped-AT-iterate row (see above).
    bool qp_solved = false;
    /// @brief Meaningful iff qp_solved.
    QpStatus qp_status = QpStatus::kOptimal;
    /// @brief Meaningful iff qp_solved.
    Index qp_minor_iters = 0;
    /// @brief Meaningful iff qp_solved.
    Index qp_factorizations = 0;
    /// @brief Any QpSolution::tr_active entry set (radius bit).
    bool tr_binding = false;
    /// The globalization strategy's verdict on this trial. It is the
    /// STRATEGY'S OWN verdict iff `qp_solved && qp_status ==
    /// QpStatus::kOptimal` -- on any other row no strategy was consulted,
    /// because there was no certified step to judge.
    ///
    /// The default below is deliberately kReject rather than a value a
    /// consumer could mistake for an acceptance, and on a FAILED-subproblem
    /// row it is more than a safe default: the row really was a rejection
    /// (the iterate did not move and the radius shrank), whether the driver
    /// retried it (sqp_driver.h's SUBPROBLEM FAILURE ROUTING) or propagated
    /// it. So a consumer reconstructing the ITERATE sequence from the
    /// history may read `verdict != kReject` on the PREVIOUS row as "the
    /// iterate moved" across every row shape.
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
    /// True iff this trial's subproblem was ELASTICALLY REFORMULATED, i.e.
    /// the plain QP at this iterate returned kInfeasible and the driver
    /// re-solved an augmented copy of it with penalized slacks on the
    /// violated linearized rows (sqp_driver.h's ELASTIC TIER note). Set on
    /// BOTH outcomes -- the row whose step came from the elastic solve, and
    /// the row that ended the solve because the elastic tier was exhausted
    /// (verdict kRestore) -- so it is an exact marker of "the linearization
    /// here was inconsistent", which is the diagnosis the restoration phase
    /// consumes.
    ///
    /// ON SUCH A ROW THE qp_* FIELDS DESCRIBE THE FINAL ELASTIC SOLVE, not
    /// the kInfeasible one that triggered it: qp_status is that solve's
    /// status (kOptimal on every row whose step was taken), and
    /// qp_minor_iters/qp_factorizations are its own counts, with the
    /// ESCALATION re-solves' costs folded into SqpCounters' aggregates only
    /// (the SOC convention, ported). The triggering kInfeasible is not lost
    /// -- it is exactly what this flag records, since the tier activates on
    /// nothing else.
    ///
    /// tr_binding IS STILL MEANINGFUL on such a row: the elastic solve gets
    /// its trust region as REAL bounds on the original variables rather
    /// than through SolveOverrides (so that the radius cannot also cap the
    /// slacks -- see the ELASTIC TIER note), and the driver re-derives the
    /// radius bit exactly as qp_engine.h's section 6 would have.
    bool elastic_applied = false;
    /// True iff the FULL-STEP WATCHDOG (SqpOptions::warm_full_step)
    /// restored an earlier best-||KKT||inf iterate ON THIS PASS, i.e. this
    /// row's f/stationarity/feasibility/kkt_residual/violation_l1 describe
    /// the RESTORED point, not the diverged-or-stalled one the previous row
    /// left off at -- sqp_driver.h's THE FULL STEP WATCHDOG block
    /// re-measures the row via `measure_iterate()` before this flag is set,
    /// so the two are never out of step. WITHOUT this flag the history's
    /// residual column can jump backward (a later row reporting a SMALLER
    /// kkt_residual than the row before it, the opposite of every other
    /// row-to-row transition) with nothing in the row itself explaining
    /// why; this is the cheapest honest label for that jump, cheaper than a
    /// full enum since the watchdog is the ONLY thing in this driver that
    /// can rebase the iterate backward mid-solve. False on every other row,
    /// including every row of a solve where the mode never engaged or ran
    /// to convergence without restoring
    /// (SqpCounters::watchdog_restores == 0 there). At most one row per
    /// solve is true today -- the mode restores at most once
    /// (SqpCounters::watchdog_restores is bounded by 1) -- but the flag is
    /// read per-row, not solve-wide, so a future relaxation of that bound
    /// needs no format change here.
    bool watchdog_restored = false;
};

/// Result of a whole SQP solve.
///
/// MULTIPLIERS. lambda_e/lambda_i are the LAST SUBPROBLEM'S multipliers,
/// carried out unchanged -- nlp_model.h's sign convention makes them the
/// NLP's multipliers at the returned point with no flip anywhere (that
/// header's MULTIPLIER SIGN CONVENTION note). z is NOT the subproblem's z;
/// it is the MODEL-IMPLIED bound multiplier at the returned point, computed
/// from grad L there. See sqp_driver.h's REPORTED BOUND MULTIPLIER note for
/// why (the QP's z is forced to 0 at a TR-pinned index and would not satisfy
/// NLP stationarity).
///
/// On a non-kOptimal exit x is the FINAL ITERATE reached, and the
/// multipliers are whatever the last successful subproblem priced -- NOT
/// cleared, unlike QpSolution's kInfeasible/kNumericalError convention,
/// because here they are the caller's evidence about where the driver
/// stopped.
///
/// ON A CERTIFIED kInfeasible EXIT THE MULTIPLIERS MEAN SOMETHING STRONGER
/// AND DIFFERENT, and a caller must not read them as prices of the NLP's own
/// constraints: they are the RESTORATION problem's multipliers, which are
/// precisely a SUBGRADIENT CERTIFICATE that the returned x is a stationary
/// point of the infeasibility measure
///     h(x) = ||cE(x)||_1 + sum_j max(0, cI_j(x)).
/// Concretely they satisfy
///     Je(x)^T lambda_e + Ji(x)^T lambda_i - z = 0,
///     lambda_e in [-1, 1]^me,  lambda_i in [0, 1]^mi,
///     lambda_e(i) = sign(cE_i(x)) wherever cE_i(x) != 0,
///     lambda_i(j) = 1 wherever cI_j(x) > 0 and 0 wherever cI_j(x) < 0,
/// i.e. the vanishing of a subgradient of h at x -- note there is NO grad f
/// term, which is what distinguishes this quadruple from an ordinary KKT
/// point and is why it certifies infeasibility rather than optimality. The
/// interval-valued entries are the free subgradient selectors on rows that
/// are exactly satisfied (|cE_i| = 0, cI_j = 0), which is where the
/// certificate gets the slack it needs.
///
/// z ON THIS EXIT IS THE RESTORATION PROBLEM'S BOUND PRICE, not the usual
/// model-implied one. The difference is exactly the objective: the ordinary
/// z is grad L = grad f + Je^T le + Ji^T li at an active bound, and grad f
/// is precisely the term a subgradient of h must not contain. Returning the
/// usual z would make the identity above fail by ||grad f|| at every
/// infeasible stationary point that sits ON a bound -- measured at a
/// residual of 1 on the box-blocked fixture, where the certificate calls for
/// z = -1 and the ordinary measure reports 0. See sqp_driver.h's
/// RESTORATION PHASE note. tests/sqp/test_sqp_restoration.cpp's
/// InfeasibleNlpCertifies re-derives this from the model at the returned
/// point rather than trusting the driver's own measurement.
///
/// THE ONE EXCEPTION is the non-finite-iterate kNumericalError exit, where
/// lambda_e/lambda_i/z ARE all cleared: at a NaN iterate nothing was
/// measured, so there is no evidence to preserve and a leaked price would be
/// noise wearing the shape of a multiplier. f is still reported as the model
/// returned it, which is to say possibly NaN.
///
/// history describes every iterate visited; whether the LAST row is an
/// iterate row or a failing-subproblem row depends on the exit -- see
/// SqpCounters.
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

    // THE TERMINAL KKT MEASUREMENT, taken at the RETURNED (x, lambda_e,
    // lambda_i) by the same evaluate_kkt call the convergence test read --
    // the four scalar columns of SqpIterate, at the point this solve
    // actually reports. They exist so a consumer can fill an outcome record
    // without reconstructing the history's exit shape: the last history row
    // is NOT reliably the returned point (a restoration exit returns the
    // RESTORED point, which has no row of its own), so scanning `history`
    // for these is wrong on exactly the exits where they matter most.
    //
    // kkt_residual is max(stationarity, feasibility) -- the scalar the
    // convergence test gates on; complementarity is RECORDED, NOT GATED
    // (SqpIterate::complementarity's own note has the argument).
    //
    // ALL FOUR ARE NaN ON THE NON-FINITE-ITERATE kNumericalError EXIT, for
    // the reason evaluate_kkt reports NaN there: nothing was measured at
    // that point, and a 0.0 would read as a converged residual.
    //
    // ON A CERTIFIED kInfeasible EXIT THEY MEASURE THE NLP'S OWN KKT
    // CONDITIONS at the returned point -- which is an INFEASIBLE point, so
    // `feasibility` is large by construction and `stationarity` is the
    // ordinary grad-L measure, NOT the subgradient certificate's residual
    // (that certificate is the multiplier quadruple, read under this
    // struct's own note above). `z` is the one field of this solution that
    // comes from the restoration problem instead; these four do not.
    /// @brief Reduced/projected ||grad L||inf at the returned point.
    double stationarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(||cE||inf, max(cI)+, bound violation) at the returned point.
    double feasibility = std::numeric_limits<double>::quiet_NaN();
    /// @brief max_j |lambda_i(j) * cI_j(x)| at the returned point.
    double complementarity = std::numeric_limits<double>::quiet_NaN();
    /// @brief max(stationarity, feasibility) -- the scalar the convergence
    ///        test gates on.
    double kkt_residual = std::numeric_limits<double>::quiet_NaN();

    /// Wall-clock seconds spent inside this solve, measured with
    /// std::chrono::steady_clock (the clock the interior-point engine's own
    /// timers wrap) around the driver's solve_impl ALONE -- never around
    /// model construction, the bridge/seam lay, the staged-value ingest or
    /// the ledger bookkeeping, all of which are setup. The same measurement
    /// SqpSolveRecord::wall_seconds carries (ledger.h), taken once and
    /// reported in both places.
    ///
    /// INFORMATIONAL, NEVER ASSERTED. This is a timing, and timings are not
    /// this project's currency of correctness -- counters are. No test in
    /// this repository asserts a VALUE here; the pins on it assert only that
    /// it is populated and non-negative. A consumer may report it and may
    /// compare it against another reading taken under the same measurement
    /// discipline; nothing may gate on it. This is exactly
    /// InteriorPointSolver::SolveResult::total_time_'s standing, stated here
    /// so the two engines' timing fields carry one contract.
    ///
    /// Defaults to 0.0; every public solve() that RETURNS writes a value
    /// >= 0.0 onto the solution it hands back.
    double wall_seconds = 0.0;

    /// TRUE ONLY ON THE CERTIFIED INFEASIBILITY EXIT: the restoration phase
    /// ran to its own KKT test, that test passed (residual <= kkt_tol) and h
    /// at the returned point is still above feas_tol -- so
    /// (x, lambda_e, lambda_i, z) is the subgradient certificate documented
    /// above and the returned point is a stationary point of h.
    ///
    /// FALSE ON EVERY OTHER EXIT, INCLUDING OTHER kInfeasible ONES, and
    /// that is what this flag is for. It is NOT reliably derivable from the
    /// status or the counters: a solve that restored once and then met a
    /// SECOND request (the once-per-solve cap), and a solve whose
    /// restoration was itself stuck, both report kInfeasible with
    /// restoration_iters > 0 -- exactly like the certified exit -- so the
    /// counters never tell the three apart. NOR IS THE LAST ROW'S VERDICT A
    /// RELIABLE DISCRIMINATOR: it is kRestore when the request came from
    /// the funnel's signature or the elastic tier's exhaustion, but kReject
    /// when it came from the radius floor (sqp_driver.h's RESTORATION PHASE
    /// note has the decision table and the floor's own tr_radius-at-the-
    /// floor marker), and any of the three kInfeasible outcomes above can
    /// pair with either shape. Trusting either the counters or the verdict
    /// produces the same caller-visible wrong answer: a FEASIBLE problem
    /// that stalls twice reports kInfeasible, and a caller following either
    /// rule would announce local infeasibility with no certificate behind
    /// it.
    ///
    /// READ IT AS A ONE-WAY GUARANTEE. true means the certificate holds.
    /// false means NO CLAIM IS MADE about the model -- the driver could not
    /// make progress, which is a statement about this solve, not about the
    /// problem.
    bool infeasibility_certified = false;

    /// The solve's exit state in warm_start.h's shape, for a LATER solve of
    /// a nearby problem to feed back in. Populated on EVERY exit of
    /// SqpDriver::solve() -- including a failed one -- from the best-known
    /// iterate; see warm_start.h's own note for the valid/cold contract and
    /// sqp_driver.h's POPULATION note for exactly what "best known"
    /// resolves to at each exit. POPULATED IS NOT THE SAME AS `valid`: one
    /// exit (a start point the model could not evaluate) fills these fields
    /// for inspection but reports valid == false, because feeding that
    /// point back would override the caller's own corrected x0 --
    /// warm_start.h's `valid` note states the exception and why it is the
    /// only one.
    WarmStart warm_start;
};

} // namespace hven::solvers
