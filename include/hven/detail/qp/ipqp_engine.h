// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The interior-point (IP-PMM) QP tier: declarations only.
///
/// A third QP kernel beside the working-set walk (qp_engine.h) and SsnEngine:
/// primal-dual barrier, Mehrotra predictor-corrector, a monotone (rho, delta)
/// schedule, and the Wachter-Biegler inertia ladder.
/// @see docs/notes/2026-09-header-prose-archive.md §ipqp_engine.h
//
// NOT a branch inside IpmSolver and NOT a user of detail/globalization/:
// fraction-to-boundary is the whole of globalization here. Declarations ONLY; the
// iteration, ladder and equilibration are in the .cpp.
//
// The status vocabulary for a downgraded certificate, without widening
// `QpStatus`: a downgraded certificate reports `QpStatus::kNumericalError`, and
// the currency is `certificate_downgraded`/`escape_reason`.

#include <limits>
#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/interior/kkt_factorization.h>
#include <hven/detail/qp/ipqp_evidence.h>
#include <hven/detail/qp/ipqp_kkt_layout.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

// Forward-declared, not included: `drivers/trace.h` is the PUBLIC schema and
// this header must not pull it in (the cycle it once broke is gone -- the
// evidence blocks moved to `ipqp_evidence.h`); a pointer/const-ref suffices.
class TraceSink;
struct IpqpTraceIterEvent;
struct IpqpTraceRegEvent;
struct IpqpTraceRestartEvent;
struct IpqpTraceCertifyEvent;
struct IpqpTraceEscapeEvent;

namespace detail {

// ===========================================================================
// The inertia ladder: Wachter-Biegler 2006 Algorithm IC, with two declared
// adaptations
// ===========================================================================
//
// Source: A. Wachter and L. T. Biegler, Math. Prog. 106(1):25-57, 2006 -- Algorithm IC
// ("Inertia Correction"), p. 10, and its four constants:
//     delta_w^0     = 1e-4     -> kIpqpLadderInit
//     bar kappa_w^+ = 100      -> kIpqpLadderUpFirst   (the FIRST climb only)
//     kappa_w^+     = 8        -> kIpqpLadderUp        (every later climb)
//     kappa_w^-     = 1/3      -> kIpqpLadderDown      (the next trial)
//
// hven DEPARTS on two declared points: the bounds are spec 3.2's, not the paper's
// 1e-20/1e40, and IC-1's unmodified trial is skipped only after `kIpqpLadderSkipAfter`
// modified iterations.

/// @brief IC's `delta_w^0`: the first rung of a climb that starts from no
/// memory at all. An ABSOLUTE magnitude, not a multiple of anything -- the
/// smallest shift this tier regards as a real inertia correction.
inline constexpr double kIpqpLadderInit = 1.0e-4;

/// @brief IC's `bar kappa_w^+`: the growth factor for the WHOLE of a solve's FIRST climb,
/// i.e. while the memory `rho_dem_last` is still zero. Two decades per rung, because with
/// no scale information a short ladder is what makes exhausting it bounded work.
inline constexpr double kIpqpLadderUpFirst = 100.0;

/// @brief IC's `kappa_w^+`: the growth factor once a memory exists. Eight, not a hundred --
/// with `rho_dem_last` in hand the ladder refines a value whose order it already knows,
/// and overshooting it is the defect a two-decade rung produces.
inline constexpr double kIpqpLadderUp = 8.0;

/// @brief IC's `1 / kappa_w^-`: the next iteration's trial is
/// `max(ipqp_reg_floor, rho_dem_last / kIpqpLadderDown)`. The ladder is therefore NOT
/// monotone within a solve, deliberately -- a monotone floor makes an overshoot permanent.
inline constexpr double kIpqpLadderDown = 3.0;

/// @brief How many CONSECUTIVE preceding iterations must have needed a
/// modification before the unmodified trial (IC-1) is skipped. Adaptation
/// (ii) in the banner above.
inline constexpr int kIpqpLadderSkipAfter = 3;

/// @brief How many CONSECUTIVE primal escalations may answer a perturbed-pivot
/// report before the ladder falls back to escalating the DUAL shift instead.
///
/// A uniform shift of the Ruiz-scaled system can annihilate a scaled diagonal, and
/// that singularity is PRIMAL; but a DUAL-caused perturbed pivot is cleared by no
/// primal rung, so the re-route is bounded. A count, not pivot provenance.
inline constexpr int kIpqpPivotReroutePrimalMax = 2;

/// @brief The SECTION 2.2 ITEM 4 READ'S WEAK-ACTIVITY SCALE, as a multiple of
/// `sqrt(mu_measured)`; the rule itself and its derivation live on
/// `detail::ipqp_accumulate_bound_sigma_critical_cone`.
///
/// TEN, chosen by the geometry rather than tuned: complementarity ties `z * gap ~ mu`, so
/// requiring BOTH below `f sqrt(mu)` confines a dropped side to a band of width `f^2`. A
/// larger factor costs a false DOWNGRADE, never a false certificate -- the safe direction.
inline constexpr double kIpqpWeakActiveFactor = 10.0;

/// @brief Disclosure-band ceiling: a KEPT side is band-counted iff its multiplier
/// sits in `(weak_scale, kIpqpTightBandFactor * weak_scale]` -- ambiguous, not
/// wrong.
inline constexpr double kIpqpTightBandFactor = 100.0;

/// @brief Growth per rung for the DUAL shift `delta` on a perturbed-pivot report --
/// `detail::kSsnProxGrowth`'s value, adopted for the reason that ladder's own banner gives.
/// The PRIMAL ladder does not use this: `rho_dem` climbs on IC's factors above.
inline constexpr double kIpqpDeltaGrowth = 100.0;

/// @brief The interior push's two Ipopt constants (`bound_push` /
/// `bound_frac`): the cold start's `x_0` is moved to
/// `max(x_0, l + min(kIpqpBoundPushAbs * max(1, |l|), kIpqpBoundPushRel * (u - l)))`
/// and symmetrically at the upper side.
///
/// Both sides move by at most `kIpqpBoundPushRel` of the box width, so the two pushes cannot
/// cross even on a narrow box. A ZERO-WIDTH box cannot reach here: the domain gate
/// (IpqpBounds) refuses it before the start point is built.
inline constexpr double kIpqpBoundPushAbs = 1.0e-2;
inline constexpr double kIpqpBoundPushRel = 1.0e-2;

/// @brief The cold start's slack floor (spec 5.6: `s_0 = max(bi - Ai x_0, 1)`).
inline constexpr double kIpqpSlackInit = 1.0;

/// @brief The warm restart's repair floors (spec 5.2 item 1): absolute epsilons
///        for a payload slack/price, and for `eps = kIpqpRepairEps * mu_0` once
///        `mu_0` is known.
///
/// The two are asymmetric: a payload slack and a payload price are floored on
/// different scales.
inline constexpr double kIpqpRepairEps = 1.0e-8;
inline constexpr double kIpqpRepairSlackEps = 1.0e-8;

/// @brief The SAY shift of 5.2 item 2 is applied ONLY to a seed that is not
/// already centred (`min pair product < this * mu_0`), so a good warm seed
/// pays nothing and `ipqp_restart_repairs` stays a signal.
inline constexpr double kIpqpSayCentralityFactor = 1.0e-1;

/// @brief The Skajaa-Andersen-Ye shift's target fraction: `delta_p = this *
/// mu_0 / z_avg`, `delta_d = this * mu_0 / d_avg` -- AVERAGES, never a
/// per-pair maximum.
inline constexpr double kIpqpSayTargetFraction = 0.5;

/// @brief The (rho, delta) schedule's decrease gate (spec 3.2's bounded-decrease
///        condition), as a contraction factor: the regularized problem's
///        relative residual must have fallen to this multiple of its value at
///        the last estimate advance before the schedule may fall again and
///        `(zeta, lambda_est)` may move.
///
/// A ratio, not an absolute threshold; the schedule requires only a gate.
inline constexpr double kIpqpRegGateContract = 0.5;

/// @brief The factorization budget's sentinel multiple (IpqpOptions'
/// `ipqp_max_factorizations <= 0`): one iteration costs one factorization
/// plus ladder rungs, and three per iteration is the ladder headroom the
/// budget grants before it calls a solve pathological.
inline constexpr Index kIpqpFactorizationsPerIter = 3;

/// @brief Ruiz equilibration (spec 4.3): sweep cap and stopping tolerance.
///
/// Ruiz's convergence is linear with a small constant, so the practical sweep count is single
/// digits (OSQP and Clarabel both ship 10); the tolerance stops early once every row/column
/// norm is within `kIpqpRuizTol` of 1.
inline constexpr Index kIpqpRuizSweeps = 10;
inline constexpr double kIpqpRuizTol = 1.0e-3;

/// @brief Section 2.2's conservative `rho` floor on an evidence failure: the
///        level a step is permitted at when a factorization succeeded but could
///        not report usable inertia evidence.
///
/// A minimum magnitude, absolute rather than a multiple of `rho`. Equal today to
/// `kIpqpLadderInit`, which is a ladder step size, but not the same contract, so
/// it carries its own name. A step permitted here is paired with a whole-solve
/// downgrade.
inline constexpr double kIpqpEvidenceFailureRhoFloor = kIpqpLadderInit;

/// @brief Section 6.2 conjunct (i): the factor by which `mu` must have been
/// reduced ACROSS the window for the window not to be a stall.
///
/// The barrier form of SSN's own "improvement is demanded over the whole window". Two is the
/// smallest factor that is unambiguously a reduction rather than noise, and deliberately
/// feeble: the test must fire on a trajectory that is not moving, never on a slow one.
inline constexpr double kIpqpStallMuFactor = 2.0;

/// @brief Section 6.2 conjunct (iii): the fraction-to-boundary step below
/// which a step counts as "dying", demanded on EVERY step of the window.
///
/// Spec 6.2 states the value (`1e-2`). A WHOLE-WINDOW property by construction -- "never
/// abort on one tiny-alpha iteration" -- so one healthy step in the window disarms it.
inline constexpr double kIpqpStallAlpha = 1.0e-2;

} // namespace detail

/// @brief Why a tier solve stopped without a standing certificate.
///
/// SsnEscape's vocabulary and SsnEscape's warning, binding here identically:
/// none of these certifies anything, and the driver branches on
/// `escape_reason`, never on `status`. `kInfeasible` is a certificate word this
/// tier never issues (spec 6.3).
///
/// The indefinite/numerical boundary is a fixed rule, not a judgement at the
/// call site:
///   * `kIndefinite` -- an inertia reading was taken and disagreed with the
///     required signature: at the section 2.2 item 4 final certification
///     factorization, or with the ladder at `ipqp_reg_max`. Saddle-suspect;
///     2.3 item 4 routes it to the SSN warm grade.
///   * `kNumerical` -- everything else that is not budget, stall or
///     infeasible-suspect: a factorization failure, an unreadable inertia (no
///     evidence state observed at all), a non-finite iterate, residual or step.
enum class IpqpEscape {
    kNone = 0,
    kBudget = 1,
    kStall = 2,
    kIndefinite = 3,
    kNumerical = 4,
    kInfeasibleSuspect = 5,
};

/// @brief The section 2.3 ratio rule's three-way verdict on one inequality row
/// or one variable-bound side.
///
/// A RATIO rule, not an absolute threshold, and three-way rather than binary on purpose: a
/// pair whose members are BOTH tiny says nothing about which side of the face it is on.
/// `kUncertain` is counted and handed to the exact tier-3 refinement, never asserted.
enum class IpqpFace {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

/// @brief THE SECTION 5.4 PAYLOAD GRADE this solve started at, reported on
/// `IpqpResult::restart_grade`. `kBaseWarm` splits the currency's SIGNED bound price (lossy
/// at a two-sided bound); `kFullWarm` carries `zL`/`zU`/`mu`.
enum class IpqpRestartGrade {
    kCold = 0,
    kBaseWarm = 1,
    kFullWarm = 2,
};

/// @brief The immutable clamp-centred box.
///
/// Computed ONCE at solve entry from the effective trust-region radius and
/// never rebuilt inside a solve:
///
///     c(i)      = clamp(0, lower(i), upper(i))
///     lo_eff(i) = max(lower(i), c(i) - Delta)
///     up_eff(i) = min(upper(i), c(i) + Delta)
///
/// The centre rule is `refine_on_face`'s: that gate windows a refined point on
/// the clamped origin and takes no centre parameter.
///
/// `IpqpEngine::solve()` takes no centre parameter -- the clamp rule is the
/// contract. A warm iterate (`IpqpSeed::x`) lives inside this box and never
/// redefines the centre; a shrink-retry rebuilds the box at the new radius and
/// re-clamps the iterate.
struct IpqpBox {
    Vec centre;          ///< n. `clamp(0, lower, upper)`.
    Vec lo_eff;          ///< n. `max(lower, centre - Delta)`.
    Vec up_eff;          ///< n. `min(upper, centre + Delta)`.
    double radius = 0.0; ///< The EFFECTIVE Delta this box was built at.

    /// The first index (ascending) whose effective width is NOT STRICTLY
    /// POSITIVE (`lo_eff(i) >= up_eff(i)`), or -1 when every width is
    /// positive. See IpqpBounds for what a non-negative value here means.
    ///
    /// `>=` rather than `==` deliberately, so a CROSSED declared box (`lower(i) > upper(i)`,
    /// which `QpProblem::validate` does not reject) lands here too and DECLINES rather than
    /// throwing: a crossed box is a legitimate infeasible QP, which the walk answers exactly.
    Index zero_width_index = -1;
};

/// @brief The tier's effective bounds and its domain gate.
///
/// Not `BoundSet`, which is the NLP engine's shape -- reduced-space indices,
/// relaxed values, fixed variables eliminated -- none of which a `QpProblem` has.
///
/// REPRESENTATION, SETTLED HERE: DENSE, not index lists. The ipqp_math.h kernels are written
/// against dense `(x, l, u, zl, zu)` with presence from `ipqp_has_lower`/`ipqp_has_upper`, so
/// index lists would be a SECOND answer to "which bounds are present", able to disagree.
///
/// THE ZERO-WIDTH RULE: a subproblem with ANY `lo_eff(i) == up_eff(i)` pair is OUT
/// OF THIS TIER'S DOMAIN -- the build reports it, the dispatch DECLINES pre-solve
/// and routes to the walk, and `ipqp_declined_pinned` records it. A DECLINE IS NOT
/// AN ESCAPE: it never ran. Declining covers every case, since under the clamp
/// centre a zero-width pair can arise only at a declared `lower(i) == upper(i)` or
/// at `Delta == 0`, both of which the walk solves best.
struct IpqpBounds {
    Vec lower; ///< n. `IpqpBox::lo_eff`. `<= -kIpqpInfBound` means ABSENT.
    Vec upper; ///< n. `IpqpBox::up_eff`. `>= kIpqpInfBound` means ABSENT.

    Index num_lower = 0; ///< Variables with a finite effective lower bound.
    Index num_upper = 0; ///< Variables with a finite effective upper bound.

    /// `IpqpBox::zero_width_index` carried through: the first index whose
    /// effective width is not strictly positive, or -1.
    Index zero_width_index = -1;

    /// True iff this subproblem is inside the tier's domain, i.e. no zero-width
    /// pair. `false` is a DECLINE, not a failure. Defined in the .cpp: a
    /// per-solve predicate, not a hot loop (CLAUDE.md section 5).
    bool in_domain() const;
};

/// @brief The tier's own iterate, carried across majors by the
/// subproblem-level warm restart (spec 5.1 flow (b)).
///
/// Deliberately NOT routed through `QpSolution` (no slack, no barrier block, published shape):
/// engine-internal currency, the way `QpEngine` carries `BorderState`/`HotState`.
///
/// EVERY BLOCK IS VALIDATED AT `solve()`'s BOUNDARY and a malformed one THROWS (CLAUDE.md
/// section 4). The section 5.4 COLD DEGRADE is the DRIVER's, taken before a seed is built:
/// by the time one reaches this engine it is a caller's assertion about the dimensions.
struct IpqpSeed {
    Vec x; ///< n. Repaired INTO the box (never the box's centre -- see IpqpBox).
    /// mi, > 0, or ALL-ZERO meaning ABSENT. Spec 5.2 item 1 recomputes it from
    /// `bi - Ai x`; with `ipqp_warm_repair` off a seed whose `s` is not
    /// strictly positive degrades COLD rather than being consumed.
    Vec s;
    Vec lambda_e; ///< me.
    Vec lambda_i; ///< mi, > 0.
    Vec zl;       ///< n, >= 0 (0 where the lower bound is absent).
    Vec zu;       ///< n, >= 0 (0 where the upper bound is absent).

    /// The seed's own barrier parameter (spec 5.3's clamp input). `<= 0` means
    /// ABSENT -- the base-warm grade has no payload `mu`, so its clamp reads
    /// only the repaired point's own measured complementarity.
    double mu = 0.0;

    Vec zeta;         ///< n, the proximal primal estimate.
    Vec lambda_est_e; ///< me, the proximal dual estimate.
    Vec lambda_est_i; ///< mi, the proximal dual estimate.

    /// Which section 5.4 grade this seed was built at, reported unchanged on
    /// `IpqpResult::restart_grade`. Never `kCold`: a cold solve passes no seed.
    IpqpRestartGrade grade = IpqpRestartGrade::kFullWarm;
};

/// @brief The relative KKT residual the tier converges on.
///
/// Owned by this tier, and scaled by the walk's own discipline: divide by a
/// `max(1, ...)` fold, so the test is never looser.
///
/// Full-KKT, WorkingSet-free, and taken on the unregularized problem: the
/// proximal terms are a solver device, so a point is judged against the caller's
/// QP.
struct IpqpResiduals {
    double stationarity = 0.0;    ///< ||Hx + g + Ae'ye + Ai'yi - zl + zu||inf / scale.
    double primal_eq = 0.0;       ///< ||Ae x - be||inf / scale.
    double primal_iq = 0.0;       ///< ||Ai x + s - bi||inf / scale.
    double complementarity = 0.0; ///< max pair |s.y|, |(x-l).zl|, |(u-x).zu| / scale.

    /// The largest of the four -- the single number the stopping rule and the
    /// stall window read.
    double worst() const;
};

/// @brief One tier solve's outcome.
///
/// **BRANCH ON `escape_reason`, NEVER ON `status`** -- SsnResult's rule,
/// binding here for the same reason: `status` alone would let a driver promote a suspicion.
struct IpqpResult {
    QpStatus status = QpStatus::kOptimal;
    IpqpEscape escape_reason = IpqpEscape::kNone;
    IpqpCounters counters;

    /// The section 5.4 grade this solve STARTED at: `kCold` when no seed was passed, otherwise
    /// the seed's own grade. Unchanged by a warm-kill -- that is reported by
    /// `counters.ipqp_warm_restart_abandoned`, and the pair says a warm attempt was dropped.
    IpqpRestartGrade restart_grade = IpqpRestartGrade::kCold;

    /// True iff the domain gate declined this subproblem pre-solve
    /// (IpqpBounds' zero-width rule). `counters.ipqp_declined_pinned` is 1 and
    /// every other counter is at its default.
    ///
    /// EXACTLY ONE OTHER FIELD IS MEANINGFUL on that path, and naming it matters because the
    /// routing chain reads it: `box` IS populated -- the decline is DERIVED from it -- and it
    /// carries `zero_width_index`. No iterate, no face, no residual: the tier never ran.
    bool declined_pinned = false;

    /// True iff the certificate was DOWNGRADED (spec 2.2 item 4). A downgraded
    /// solve never reports `kOptimal`.
    ///
    /// FIVE WAYS TO GET HERE: four pair with an `ipqp_final_inertia_read` value
    /// (1 disagreed, 2 unusable evidence, 3 declined/budget-refused); the fifth is a
    /// MID-SOLVE evidence failure.
    bool certificate_downgraded = false;

    /// True iff section 2.2's evidence-failure policy was invoked ANYWHERE in this solve: some
    /// factorization SUCCEEDED but reported a state other than `kObserved`. Implies
    /// `certificate_downgraded`. Distinct from a FAILED factorization, which escapes at once.
    bool inertia_evidence_failed = false;

    /// True iff the item 4 read AGREED and, once every downgrade is applied,
    /// the disclosure still fires -- informative history via noise > 0 or an ambiguous
    /// per-side verdict, the band count otherwise.
    bool read_kept_tight = false;

    /// The raw exponent(s) behind `ipqp_read_barrier_noise_sides`, min/max over the
    /// band-counted sides, NaN when uninformative -- so a pin can state a numeric tolerance
    /// rather than only the discretized count.
    double read_barrier_noise_exponent_min = std::numeric_limits<double>::quiet_NaN();
    double read_barrier_noise_exponent_max = std::numeric_limits<double>::quiet_NaN();

    // --- the point ---------------------------------------------------------

    Vec x;        ///< n.
    Vec lambda_e; ///< me.
    Vec lambda_i; ///< mi, >= 0.

    /// SIGNED bound multiplier, `QpSolution::z`'s convention (>= 0 at an active lower bound,
    /// <= 0 at an active upper one), recombined as `zl - zu` and FORCED TO 0 at a TR-pinned
    /// index -- exactly QpSolution's contract, including its stationarity caveat there.
    Vec z;

    // --- the tier's own blocks (the warm seed reads these) ------------------

    Vec s;              ///< mi, > 0. `bi - Ai x` up to the primal residual.
    Vec zl;             ///< n, >= 0. RAW, not TR-swept.
    Vec zu;             ///< n, >= 0. RAW, not TR-swept.
    double mu = 0.0;    ///< The measured complementarity at the returned point.
    double rho = 0.0;   ///< The (rho, delta) schedule's final primal value.
    double delta = 0.0; ///< ... and its final dual value.

    /// The SECTION 2.2 INERTIA-DEMANDED MODIFICATION in force at the last step
    /// this solve took -- `0.0` when that step ran on the unmodified system,
    /// which is every step of every convex subproblem.
    ///
    /// A second quantity BESIDE `rho`, not a component of it: `rho` is the section 3.2
    /// PROXIMAL schedule and enters the right-hand side; this is Ipopt's `delta_w`, a
    /// diagonal-only modification additive on top of `rho` and absent from the RHS entirely.
    ///
    /// NOT the solve's high-water mark (`counters.ipqp_rho_demanded_max`) and NOT the ladder's
    /// memory (`counters.ipqp_rho_demanded_last`): it is the working value of the LAST step, so
    /// a solve that armed early and disarmed later reports `0.0` here and a nonzero max.
    double rho_mod = 0.0;

    // --- the face blocks (spec 2.3 item 2; the routing chain's input) -------

    std::vector<IpqpFace> ineq_face;  ///< mi.
    std::vector<IpqpFace> lower_face; ///< n. kInactive at an ABSENT lower bound.
    std::vector<IpqpFace> upper_face; ///< n. kInactive at an ABSENT upper bound.

    /// mi / n / n, the two shapes `QpSolution` publishes an active set in. Derived from the
    /// three face vectors above: a row is active iff its verdict is `kActive` (an UNCERTAIN row
    /// reads inactive here and is counted in `ipqp_face_uncertain` -- never forced either way).
    std::vector<bool> ineq_active;
    std::vector<BoundState> bound_state;

    /// n, `QpSolution::tr_active`'s contract verbatim: true at i iff the
    /// variable is held by a TR-TIGHT effective bound rather than a real one.
    /// Such a variable reports `kFree` in `bound_state` and 0 in `z`.
    std::vector<bool> tr_active;

    // --- evidence ----------------------------------------------------------

    IpqpResiduals residuals;

    /// The section 6.2 stall escape's three conjunct values. `fired` is false
    /// on every other outcome; see IpqpStallEvidence for why this is an
    /// evidence block rather than a three-way counter census.
    IpqpStallEvidence stall_evidence;

    /// The section 6.3 infeasible-suspect escape's signals, window and
    /// least-infeasible point. `fired` is false on every other outcome.
    IpqpInfeasibilityEvidence infeasibility_evidence;

    /// The box this solve ran in -- the answer to "which window was this point
    /// gated against", which tier 3's hand-off needs and which is exactly the
    /// thing IpqpBox exists to keep from being re-derived differently.
    IpqpBox box;
};

/// @brief The IP-PMM interior-point QP tier.
///
/// STATEFUL ACROSS SOLVES, like `QpEngine`'s border cache and `SsnEngine`'s pattern cache: the
/// factorization, its symbolic analysis and the `IpqpKktLayout` plan persist, so subproblems
/// sharing a pattern pay ONE symbolic analysis (spec 4.1). `ipqp_hoist_symbolic` turns it off.
///
/// ONE LAYOUT SERVES ONE BUFFER for its whole life (IpqpKktLayout's own contract): the layout
/// on this instance is only ever handed `kkt_.matrix()`, and nothing else may be.
class IpqpEngine {
  public:
    /// @param opts    The QP options this tier solves under.
    /// @param threads The thread count in force -- SqpSolver passes
    ///                SqpOptions::common.threads (M6 W5 T8.8). It configures
    ///                the ONE `KktFactorization` this tier holds. 0 (the
    ///                default, and what every pre-T8.8 construction site
    ///                passed) leaves the backend's own default alone, so a
    ///                defaulted engine is bit-for-bit the pre-T8.8 one.
    /// @throws std::invalid_argument for a negative count (the linear
    ///         surface's own rule, reached through
    ///         KktFactorization::set_num_threads). SqpOptions validation
    ///         refuses a negative common.threads long before any engine is
    ///         built (tests/drivers/test_options.cpp), so this is a
    ///         defence-in-depth throw, not the primary gate.
    explicit IpqpEngine(const QpOptions &opts, int threads = 0);

    IpqpEngine(const IpqpEngine &) = delete;
    IpqpEngine &operator=(const IpqpEngine &) = delete;

    /// Defined in the .cpp for the same reason `IpqpBounds::in_domain()` is:
    /// a const accessor is not a per-element hot loop, and this header carries
    /// declarations and `inline constexpr` only.
    const QpOptions &options() const;

    /// The LIVE factor's own thread count, read through to the backend session
    /// (`KktFactorization::session_num_threads()`), not the stored option --
    /// a boundary observation of the factor this tier actually solves through.
    /// Defined in the .cpp, like `options()`.
    int num_threads() const;

    /// Attach a ledger for instrumentation. `QpEngine::attach_ledger`'s
    /// contract verbatim (qp_engine.h): a THROWING `solve()` emits no record.
    ///
    /// The emitted `SolveRecord` carries a QP-SHAPED PROJECTION of this tier's work
    /// (`minor_iters` <- `ipqp_iters`, and so on for factorizations and symbolic analyses) so
    /// one ledger holds all three kernels on one scale. The full counters ride `IpqpResult`.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    /// Attach a trace sink (spec section 7; nullptr = off, default off).
    /// Five of seven events fire here; the other two are the driver's own.
    /// Reattach clears last_trace_solve_id() back to 0 (R5).
    void attach_trace(TraceSink *sink);

    /// The SQP major the NEXT solve() call belongs to (no other way to know
    /// it); 0 outside a driver.
    /// @throws std::invalid_argument if major < 0 (R5, CLAUDE.md section 4).
    void set_trace_major(Index major);

    /// The trace `solve` id of the most recently COMPLETED solve() call (0 if
    /// none has, R5), for correlating with a caller's own events for the
    /// same subproblem (the driver's route/qp.mode).
    Index last_trace_solve_id() const;

    /// @brief Solve `qp` by the interior-point tier.
    ///
    /// @param qp        the subproblem. Validated (`QpProblem::validate`).
    /// @param seed      the section 5 warm restart, or `nullptr` for a COLD
    ///                  solve. `nullptr` IS COLD unconditionally -- this call never consumes
    ///                  `warm_carry()` on the caller's behalf.
    /// @param iopts     the tier's own settings (validated by
    ///                  `validate`), re-checked here at the API boundary, because
    ///                  this engine is reachable without a driver.
    /// @param overrides the walk's own per-solve override type
    ///                  (`qp_types.h`), unchanged. `tr_radius` is the ONE field this tier
    ///                  reads; `primal_delta`/`dual_mu` are the walk's working-set device,
    ///                  VALIDATED here and then deliberately unused.
    ///
    /// @throws std::invalid_argument for CALLER errors -- a malformed
    ///         `QpProblem`, an out-of-range `IpqpOptions` or `SolveOverrides`
    ///         field, a seed whose blocks are wrongly sized or non-finite. A
    ///         solve that cannot make progress reports a status and an `IpqpEscape` and does
    ///         NOT throw; that separation is `SsnEngine::solve`'s and is kept exactly.
    IpqpResult solve(const QpProblem &qp, const IpqpSeed *seed, const IpqpOptions &iopts,
                     const SolveOverrides &overrides);

    /// @brief THE CROSS-MAJOR CARRY (spec 5.1 flow (b)): the state the last solve on this
    /// instance finished at, or `nullptr` when none is armed. Engine-internal currency the
    /// caller passes straight back. A solve that produced no usable state LEAVES IT STANDING.
    const IpqpSeed *warm_carry() const;

    /// Drop the carry. The driver calls this at SQP-solve entry and after a
    /// genuine escape, whose iterate spec 2.3 item 5 discards.
    void reset_warm_carry();

  private:
    struct Workspace;

    QpOptions opts_;

    KktFactorization kkt_;
    IpqpKktLayout layout_;

    /// True once this instance holds a symbolic analysis for the pattern the
    /// layout currently describes. Drives spec 4.1's compute()-vs-refactorize()
    /// decision.
    bool analyzed_ = false;

    /// The spec 5.1 flow (b) carry and its armed flag. Committed at the END of
    /// a solve that produced a finite iterate, so an unusable solve cannot
    /// overwrite a good carry.
    IpqpSeed carry_;
    bool carry_armed_ = false;

    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;

    // --- the trace hook -----------------------------------------------------
    TraceSink *trace_ = nullptr;
    Index trace_major_ = 0;
    Index trace_solve_counter_ = 0;
    Index last_trace_solve_id_ = 0;

    // Five named private emit sites, each a no-op when unattached.
    // `ipqp.route`/`qp.mode` are the driver's own -- see attach_trace's doc.
    void emit_trace_iter(const IpqpTraceIterEvent &event) const;
    void emit_trace_reg(const IpqpTraceRegEvent &event) const;
    void emit_trace_restart(const IpqpTraceRestartEvent &event) const;
    void emit_trace_certify(const IpqpTraceCertifyEvent &event) const;
    void emit_trace_escape(const IpqpTraceEscapeEvent &event) const;
};

/// @brief What one tier outcome is, in the section 6.1 ladder's own currency.
///
/// Three values, one per ladder rule. The `IpqpResult` -> outcome mapping is the
/// routing chain's: the convenience overload of `IpqpEscapeLadder::record` is
/// right everywhere but one case -- a `kBudget` refusal on a converged iterate,
/// which is an escape to the census and a success to the ladder.
enum class IpqpLadderOutcome {
    kSuccess = 0,  ///< Resets the consecutive-escape tally.
    kEscape = 1,   ///< Advances it, and may retire the tier.
    kDeclined = 2, ///< Neutral: neither advances nor resets.
};

/// @brief The section 6.1 escape ladder: K consecutive escapes retire the tier
///        for the remainder of one SQP solve.
///
/// Separate from `IpqpEngine` because section 6.1's decision is across
/// subproblems and no single subproblem's own solve can observe it. The routing
/// chain owns one per SQP solve, feeds it every tier outcome, and writes
/// `ipqp_tier_retired_after` from it.
///
/// The three rules: a fresh decision every major (the only state is the
/// consecutive-escape tally and the retirement flag); any success resets the
/// count, including a downgrade without an escape; and a decline is neutral.
///
/// Retirement fires at most once: once retired, `record()` keeps returning true
/// and `retired_after()` keeps naming the major it happened at, whatever arrives
/// afterwards.
class IpqpEscapeLadder {
  public:
    /// @param iopts the tier's settings; only `ipqp_retire_after` is read (validated `> 0` by
    ///              `validate`, re-checked here -- reachable without a driver).
    /// @throws std::invalid_argument if `ipqp_retire_after <= 0`.
    explicit IpqpEscapeLadder(const IpqpOptions &iopts);

    /// Record one tier outcome, observed at SQP major `major` (1-based, the
    /// index `ipqp_tier_retired_after` reports).
    /// @return true iff the tier is retired for the remainder of this solve.
    /// @throws std::invalid_argument if `major <= 0` -- a marker of 0 means "never retired".
    bool record(const IpqpResult &result, Index major);

    /// @brief Record one tier outcome whose ladder classification the caller has
    ///        made -- see `IpqpLadderOutcome` for the one case where it differs.
    ///
    /// Same contract as the `IpqpResult` overload otherwise.
    ///
    /// @param outcome the caller's own ladder classification.
    /// @param major   the SQP major this outcome was observed at (1-based).
    /// @return true iff the tier is retired for the remainder of this solve.
    /// @throws std::invalid_argument if `major <= 0`.
    bool record(IpqpLadderOutcome outcome, Index major);

    /// True once retirement has fired. The routing chain must stop entering
    /// the tier for this SQP solve.
    bool retired() const;

    /// The major retirement fired at, or 0 if it never did -- the value
    /// `SqpCounters::ipqp.ipqp_tier_retired_after` takes.
    Index retired_after() const;

    /// The current consecutive-escape tally. Exposed for the pins and for the
    /// trace; it is not part of any counter contract.
    Index consecutive_escapes() const;

  private:
    Index retire_after_ = 3;
    Index consecutive_ = 0;
    Index retired_after_ = 0;
    bool retired_ = false;
};

// ---------------------------------------------------------------------------
// Free functions -- the box and its domain gate, exposed so the routing layer can build the
// SAME box before entering the tier, and so they are testable.
// ---------------------------------------------------------------------------

/// @brief Build the immutable clamp-centred box (IpqpBox's own contract).
/// @param qp     the subproblem, already validated.
/// @param radius the EFFECTIVE trust-region radius: `SolveOverrides::tr_radius` resolved
///               against `QpOptions::tr_radius`; +inf disables the window exactly.
/// @throws std::invalid_argument if `radius` is negative or NaN, or if a
///         declared bound is NaN, checked here rather than left to Eigen's asserts (NDEBUG
///         compiles those out). A CROSSED declared box is NOT a throw -- see `zero_width_index`.
IpqpBox make_ipqp_box(const QpProblem &qp, double radius);

/// @brief Derive the tier's effective bounds and its domain verdict from a box.
/// @param box the box `make_ipqp_box` built.
/// @return the effective bounds, carrying the domain verdict.
IpqpBounds make_ipqp_bounds(const IpqpBox &box);

/// @brief Resolve one call's trust-region radius exactly as `solve()` does.
///
/// Exposed for the routing chain, for the same reason `make_ipqp_box` is: the driver
/// runs the domain gate before entering the tier and the box is a function of the
/// resolved radius, so two resolutions of the +inf sentinel could disagree.
///
/// @param opts      the engine's own options.
/// @param overrides this call's overrides.
/// @return the effective radius; +inf disables the window exactly.
double ipqp_effective_tr_radius(const QpOptions &opts, const SolveOverrides &overrides);

/// @brief The tier's stopping rule as a predicate on a residual block.
///
/// Exposed for the routing chain. Section 2.3 must tell a solve that converged and
/// then had its certificate refused by the factorization budget from one the
/// iteration cap stopped mid-descent: the first belongs at the tier-3 refinement,
/// the second at the walk.
///
/// `solve()`'s own stopping rule is this call, so the answer here is the same
/// answer the engine acted on.
///
/// @param residuals the residual block to test.
/// @param opts      the engine's own options, for the target tolerances.
/// @param iopts     the tier's settings, for `ipqp_converge_slack`.
/// @return true iff the block meets the tier's stopping target.
bool ipqp_residuals_meet_target(const IpqpResiduals &residuals, const QpOptions &opts,
                                const IpqpOptions &iopts);

} // namespace hven::solvers
