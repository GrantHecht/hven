// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// ipqp_engine.h -- the interior-point (IP-PMM) QP tier: declarations only.
//
// WHAT THIS TIER IS. A third QP kernel beside the working-set walk
// (detail/qp/qp_engine.h) and the semismooth Newton engine
// (detail/qp/ssn_engine.h): a primal-dual barrier method with Mehrotra
// predictor-corrector steps on the section 3.1 system, a monotone
// (rho, delta) proximal regularization schedule, and the Wachter-Biegler
// inertia-correction ladder on top of it. Specified in
// docs/notes/2026-08-m6-w1-ipqp-spec.md; the task decomposition is
// docs/notes/2026-08-m6-w1-implementation-plan.md.
//
// WHAT IT IS NOT. It is NOT a branch inside InteriorPointSolver: none of that
// engine's Settings/SolveResult/alg_impl, none of detail/globalization/'s
// acceptance strategies, funnels, watchdogs, recovery chains or restoration.
// Globalization here is fraction-to-boundary and nothing else (spec 3.1 item
// 3). It is a sibling of SsnEngine.
//
// REACHABILITY. As of M6 W1 task 5 this engine is reachable from TESTS ONLY.
// QpMode::kIpm is still refused at validate_sqp_options (task 1's temporary
// refusal) and the routing chain is task 6's -- see THE TASK-5 BOUNDARY below
// for exactly which parts of the specification this file implements today.
//
// TU PLACEMENT (CLAUDE.md section 5). This header carries declarations, enums
// and `inline constexpr` constants ONLY. The iteration, the ladder, the
// residual contract and the equilibration live in src/qp/ipqp_engine.cpp:
// they are orchestration, and orchestration lives in a .cpp regardless of how
// hot the surrounding loop is. The per-element hot loops the iteration runs
// are detail/qp/ipqp_math.h's kernels, which stay header-inline for exactly
// the opposite reason -- see that file's own TU-placement note.
//
// -------------------------------------------------------------------------
// THE TASK-5 BOUNDARY -- what this file does and does not do yet
// -------------------------------------------------------------------------
//
// IMPLEMENTED HERE (task 4): the clamp-centred box and its domain gate
// (IpqpBox / IpqpBounds), the cold start (spec 5.6), the Mehrotra
// predictor-corrector iteration (3.1), the gated (rho, delta) schedule (3.2),
// the inertia gate and the Algorithm IC ladder with its memory (2.2), Ruiz
// equilibration with unscale-on-export (4.3), the relative-KKT stopping rule
// (3.4), the required final unregularized inertia read (2.2 item 4), the
// budget/factorization caps, and the ratio face classification (2.3 item 2).
//
// ADDED HERE (task 5): the CERTIFICATION vocabulary built on top of task 4's
// final read (the status ruling below), section 2.2's evidence-failure policy
// (a step at a conservative `rho` floor plus a whole-solve downgrade when the
// inertia evidence cannot be read at all), the section 6.2 early-stall test
// and the section 6.3 infeasible-suspect test with their evidence blocks, the
// optional Farkas corroboration, the FIVE-WAY ESCAPE CENSUS, and the section
// 6.1 escape ladder (`IpqpEscapeLadder`: K consecutive escapes retire the
// tier for the remainder of an SQP solve).
//
// ADDED HERE (task 7): the subproblem-level warm restart of section 5, and
// the cross-major carry this instance holds (`warm_carry()`). NOT here: the
// routing chain and the tier-3 hand-off, which are task 6's.
// `.superpowers/w1-t7-report.md` FIX ROUND 3.
//
// -------------------------------------------------------------------------
// THE STATUS VOCABULARY FOR A DOWNGRADED CERTIFICATE -- task 5's ruling
// -------------------------------------------------------------------------
//
// Plan section 7 note (j) left this open: "today's QpStatus has no such
// value ... the status vocabulary for a downgraded-without-escape result is
// T5's ruling, as the owner of certification." RULED, AGAINST THE SPEC TEXT
// AND WITHOUT WIDENING `QpStatus`:
//
//   A DOWNGRADED CERTIFICATE REPORTS `QpStatus::kNumericalError`. The
//   certification currency is `IpqpResult::certificate_downgraded` and
//   `IpqpResult::escape_reason`, never `status`.
//
// THREE GROUNDS, each from the specification rather than from taste:
//
// 1. Section 2.2 item 4's own closing line: "CERTIFICATION VOCABULARY IS
//    SSN'S, so the driver's KKT gate reads one thing from all three kernels."
//    SSN maps every non-certifying, non-budget exit -- kIndefinite,
//    kNoContraction, kSingular -- onto `QpStatus::kNumericalError`
//    (ssn_engine.h's own status note), and the WALK maps its trusted
//    wrong-inertia exit there too (qp_engine.h's termination note: "the
//    answer is the regularization talking, not an optimum"). A converged
//    point whose second-order certificate could not be established is
//    already spelled `kNumericalError` by both existing kernels. Adding a
//    fourth spelling for this tier alone is exactly what that line forbids.
// 2. THE OTHER THREE VALUES ARE EACH A FALSE STATEMENT. `kOptimal` is
//    forbidden outright by section 2.2 item 4 ("the result is never
//    kOptimal"). `kInfeasible` is a CERTIFICATE this tier can never issue
//    (section 6.3). `kMaxIter` would claim the iteration cap stopped a solve
//    that converged.
// 3. WIDENING `QpStatus` WOULD PUT THE CERTIFICATE ON THE WRONG CHANNEL.
//    Section 7's trace schema already carries `downgraded` as its own field
//    on the `ipqp.certify` event, separate from `qp.mode`'s three outcomes
//    (`optimal|routed|escaped`); and `IpqpResult`'s own rule is BRANCH ON
//    `escape_reason`, NEVER ON `status`. A new enumerator would invite a
//    consumer to read the certificate off `status` -- the one thing every
//    kernel's result contract in this tree tells it not to do -- and would
//    ripple through a core, shared enum that the SQP driver, the ledger and
//    the corpus CSV all read.
//
// THE CONSEQUENCE, STATED SO IT IS NOT A SURPRISE: a caller who sets
// `ipqp_require_final_inertia = false` gets `kNumericalError` on a solve that
// converged cleanly. That is the option's own documented weakening ("FALSE
// means every certificate is downgraded unconditionally"), it is the SAFE
// direction for anything that gates on `status == kOptimal`, and
// `escape_reason == kNone` with `certificate_downgraded == true` is what
// separates it from a genuine failure.

#include <string>
#include <vector>

#include <Eigen/Core>

#include <hven/core/ledger.h>
#include <hven/core/solver_counters.h>
#include <hven/core/solver_status.h>
#include <hven/core/types.h>
#include <hven/detail/interior/kkt_factorization.h>
#include <hven/detail/qp/ipqp_kkt_layout.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_types.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// ===========================================================================
// THE INERTIA LADDER: WACHTER-BIEGLER 2006 ALGORITHM IC, WITH TWO DECLARED
// ADAPTATIONS
// ===========================================================================
//
// Source: A. Wachter and L. T. Biegler, "On the implementation of an
// interior-point filter line-search algorithm for large-scale nonlinear
// programming", Mathematical Programming 106(1):25-57, 2006 -- Algorithm IC
// ("Inertia Correction"), p. 10, and its four constants:
//
//     delta_w^0     = 1e-4     -> kIpqpLadderInit
//     bar kappa_w^+ = 100      -> kIpqpLadderUpFirst   (the FIRST climb only)
//     kappa_w^+     = 8        -> kIpqpLadderUp        (every later climb)
//     kappa_w^-     = 1/3      -> kIpqpLadderDown      (the next trial)
//
// The rule, in full, is implemented in `IpqpEngine::solve`'s
// `factorize_with_ladder` and its trial selector; this banner exists so the
// four numbers are read together with the paper they come from, and so the
// places hven DEPARTS from the paper are stated here rather than discovered.
//
// IT IS NOT "VERBATIM", AND SAYING SO WOULD BE FALSE ON TWO POINTS:
//
//  (i) BOUNDS. The paper's `delta_w^min = 1e-20` and `delta_w^max = 1e40` are
//      hven's `ipqp_reg_floor` (1e-10, Cipolla-Gondzio's `kProxRegFloor`) and
//      `ipqp_reg_max` (1e6, `detail::kSsnProxMax`) -- the spec's own section
//      3.2 bounds, which the tier shares with the SSN kernel's proximal cap
//      and its relative cap-slack rule. A ladder that ran to 1e40 would
//      contradict the exhaustion guard the spec writes for this tier.
//
// (ii) THE UNMODIFIED TRIAL. IC-1 tries the unmodified system EVERY iteration.
//      hven skips that trial only after `kIpqpLadderSkipAfter` CONSECUTIVE
//      iterations have needed a modification -- the deviation Ipopt's own
//      implementation documents (paper p. 10, "in our implementation") --
//      never from the first modification on. Skipping earlier would make the
//      "try zero first" property, which is what lets `rho_dem` fall to 0 the
//      moment the reduced curvature turns positive and one Newton step land
//      the residual at machine precision, unreachable on exactly the rows it
//      matters for.
//
// THE CONSTANTS ARE THE PAPER'S, NOT THE IN-TREE NLP DRIVER'S (1e-5, x8, /3
// plus a cycling guard; `interior_point_solver.h:419-425`). Three reasons, and
// none of them is "the driver is wrong": the shift is SCALE-RELATIVE and the
// two act on differently scaled matrices (the driver on the Lagrangian
// Hessian at its own scaling, this tier on the Ruiz-scaled QP matrix), so the
// same numbers would not be the same behaviour; the driver's cycling guard
// exists because an NLP's Hessian changes every iteration and a stale memory
// can mislead, whereas a QP's `H` is FIXED for the whole solve; and
// source parity between the two engines is about CONTRACTS, not about shared
// tuning constants. Registered for M7 (one line): measure the paper's
// constants against the driver's on the same instrumented fixtures rather
// than settling it by fiat. Ipopt's four numbers are twenty years of CUTEst
// -- do not tune them on three HS rows.

/// @brief IC's `delta_w^0`: the first rung of a climb that starts from no
/// memory at all. An ABSOLUTE magnitude, not a multiple of anything -- the
/// smallest shift this tier regards as a real inertia correction.
inline constexpr double kIpqpLadderInit = 1.0e-4;

/// @brief IC's `bar kappa_w^+`: the growth factor for the WHOLE of a solve's
/// FIRST climb, i.e. while the memory `rho_dem_last` is still zero. Two
/// decades per rung, because on the first climb there is no scale information
/// about the subproblem at all and a short ladder is what makes exhausting it
/// bounded work.
inline constexpr double kIpqpLadderUpFirst = 100.0;

/// @brief IC's `kappa_w^+`: the growth factor once a memory exists. Eight,
/// not a hundred -- with `rho_dem_last` in hand the ladder is refining a value
/// it already knows the order of, and overshooting it is the whole defect the
/// two-decade rung produced before T4b.
inline constexpr double kIpqpLadderUp = 8.0;

/// @brief IC's `1 / kappa_w^-`: the next iteration's trial is
/// `max(ipqp_reg_floor, rho_dem_last / kIpqpLadderDown)`. The ladder is
/// therefore NOT monotone within a solve, deliberately: a monotone floor makes
/// the first climb's overshoot permanent, and Algorithm IC -- which spec 2.2
/// cites by name -- has no such rule.
inline constexpr double kIpqpLadderDown = 3.0;

/// @brief How many CONSECUTIVE preceding iterations must have needed a
/// modification before the unmodified trial (IC-1) is skipped. Adaptation
/// (ii) in the banner above.
inline constexpr int kIpqpLadderSkipAfter = 3;

/// @brief How many CONSECUTIVE primal escalations may answer a perturbed-pivot
/// report before the ladder falls back to escalating the DUAL shift instead
/// (M6 W1 T4b fix round 1, settler ruling R2).
///
/// THE RE-ROUTE IS RIGHT FOR THE CASE THAT MOTIVATED IT AND HAS TO BE BOUNDED
/// FOR THE CASE IT CAN MIS-ROUTE. A uniform shift of the Ruiz-scaled system can
/// annihilate a scaled diagonal -- Ruiz normalizes a dominant diagonal to
/// almost exactly `-1` and `rho_dem = 1` is a rung of Algorithm IC's own first
/// climb -- and that singularity is PRIMAL, so no dual shift can clear it. But
/// a perturbed pivot whose cause is DUAL (near-dependent equality or inequality
/// rows) is not cleared by ANY primal rung, and an unbounded primal re-route
/// would ride the ladder to `ipqp_reg_max` and escape on exhaustion, spending
/// the whole ceiling's worth of factorizations to reach an answer the dual
/// escalation gives in one.
///
/// WHY A COUNT AND NOT PIVOT PROVENANCE. The honest instrument would be the
/// pivot BLOCK the backend perturbed -- primal rows say "climb `rho_dem`", dual
/// rows say "climb `delta`". `hven::linear::InertiaEvidence` does not carry
/// pivot locations: it carries COUNTS (positive/negative/zero, the perturbed
/// count and the evidence state) and nothing indexed. Two consecutive failed
/// primal escalations is the practical approximation of that provenance, and it
/// is written here as an approximation rather than as a rule with a reason. If
/// a future backend surface exposes pivot indices, this constant and its branch
/// are what that change replaces.
inline constexpr int kIpqpPivotReroutePrimalMax = 2;

/// @brief The SECTION 2.2 ITEM 4 READ'S WEAK-ACTIVITY SCALE, as a multiple of
/// `sqrt(mu_measured)` (settler ruling R1; the rule itself and its derivation
/// live on `detail::ipqp_accumulate_bound_sigma_critical_cone`).
///
/// TEN, and the value is chosen by the geometry rather than tuned. Because
/// complementarity ties `z * gap ~ mu`, requiring BOTH `z <= f sqrt(mu)` and
/// `gap <= f sqrt(mu)` confines a dropped bound side to a band of width `f^2`
/// around `sqrt(mu)`. At `f = 10` and a converged `mu = 1e-12` the multiplier
/// threshold is `1e-5`: every multiplier a real solution actually carries
/// survives it, and every multiplier that is barrier noise does not. A larger
/// factor would start dropping genuinely priced bounds -- which costs a false
/// DOWNGRADE, never a false certificate, so the failure direction is the safe
/// one -- and a factor at or below 1 would sit on the knife edge the rule
/// exists to move away from.
inline constexpr double kIpqpWeakActiveFactor = 10.0;

/// @brief Growth per rung for the DUAL shift `delta` on a perturbed-pivot
/// report -- `detail::kSsnProxGrowth`'s value, adopted for the reason that
/// ladder's own banner gives: at a 1e6 ceiling two decades per rung gives a
/// ladder short enough that exhausting it is bounded work, and one decade per
/// rung doubles the factorization bill to buy resolution no fixture has ever
/// needed.
///
/// THE PRIMAL LADDER NO LONGER USES THIS (T4b): `rho_dem` climbs on Algorithm
/// IC's own two factors above. `delta`'s escalation is a quasi-definiteness
/// repair with no inertia memory behind it and is unchanged.
inline constexpr double kIpqpDeltaGrowth = 100.0;

/// @brief The interior push's two Ipopt constants (`bound_push` /
/// `bound_frac`): the cold start's `x_0` is moved to
/// `max(x_0, l + min(kIpqpBoundPushAbs * max(1, |l|), kIpqpBoundPushRel * (u - l)))`
/// and symmetrically at the upper side.
///
/// Both sides move by at most `kIpqpBoundPushRel` of the box width, so the
/// two pushes cannot cross even on a very narrow box -- which is what makes
/// "the tier keeps a strict interior by construction" a statement about the
/// code and not a hope. A ZERO-WIDTH box cannot reach here at all: it is
/// refused by the domain gate (IpqpBounds) before the start point is built.
inline constexpr double kIpqpBoundPushAbs = 1.0e-2;
inline constexpr double kIpqpBoundPushRel = 1.0e-2;

/// @brief The cold start's slack floor (spec 5.6: `s_0 = max(bi - Ai x_0, 1)`).
inline constexpr double kIpqpSlackInit = 1.0;

/// @brief THE WARM RESTART'S REPAIR FLOORS (spec 5.2 item 1): absolute
/// epsilons for a payload slack/price, and for `eps = kIpqpRepairEps * mu_0`
/// once `mu_0` is known. Asymmetry (fix round 1, R4) argued in
/// `.superpowers/w1-t7-report.md` FIX ROUND 2, F4.
inline constexpr double kIpqpRepairEps = 1.0e-8;
inline constexpr double kIpqpRepairSlackEps = 1.0e-8;

/// @brief The SAY shift of 5.2 item 2 is applied ONLY to a seed that is not
/// already centred (`min pair product < this * mu_0`), so a good warm seed
/// pays nothing and `ipqp_restart_repairs` stays a signal.
inline constexpr double kIpqpSayCentralityFactor = 1.0e-1;

/// @brief The Skajaa-Andersen-Ye shift's target fraction: `delta_p = this *
/// mu_0 / z_avg`, `delta_d = this * mu_0 / d_avg` -- AVERAGES, never a
/// per-pair maximum. `.superpowers/w1-t7-report.md` FIX ROUND 2, section 4(b).
inline constexpr double kIpqpSayTargetFraction = 0.5;

/// @brief The (rho, delta) schedule's DECREASE GATE (spec 3.2's
/// bounded-decrease condition), as a CONTRACTION FACTOR: the regularized
/// problem's RELATIVE residual must have fallen to this multiple of its value
/// at the last estimate advance before the schedule may fall again and
/// `(zeta, lambda_est)` may move.
///
/// A RATIO, AND THAT IS LOAD-BEARING RATHER THAN STYLISTIC. The first version
/// of this gate compared the regularized residual against the current `mu`
/// -- two ABSOLUTE quantities -- and the comparison is then a statement about
/// the problem's units, not about its progress: on a QP whose gradient is of
/// order 1e6 the gate can never fire, `rho` stays at `ipqp_rho_init`, and the
/// tier converges to the PROXIMALLY BIASED point `argmin f(x) + rho/2 |x -
/// zeta|^2` while reporting healthy relative residuals in the well-scaled
/// coordinates. Measured, on a two-variable fixture with six decades between
/// its blocks: the tier returned x2 = 0.004988 where the answer is 2, which
/// is `4e-2 / (2e-2 + 8)` exactly -- the bias, solved for. The contraction
/// form has no units and cannot fail that way.
///
/// NOT A PAPER CONSTANT, and said so rather than dressed up as one.
/// Cipolla-Gondzio state the condition abstractly (the PMM subproblem is
/// solved to an accuracy tied to the current penalty); the FACTOR is an
/// implementation choice. 0.5 -- "the regularized residual has halved" -- is
/// slack enough that a healthy Newton solve, which contracts far faster than
/// that, advances the schedule on nearly every iteration, and tight enough
/// that a stalling one stops advancing it. What matters for CORRECTNESS is
/// only that the decrease is GATED at all: an ungated geometric decay
/// (`prox_reg_decay`'s) is what spec 3.2 rules out.
inline constexpr double kIpqpRegGateContract = 0.5;

/// @brief The factorization budget's sentinel multiple (IpqpOptions'
/// `ipqp_max_factorizations <= 0`): one iteration costs one factorization
/// plus ladder rungs, and three per iteration is the ladder headroom the
/// budget grants before it calls a solve pathological.
inline constexpr Index kIpqpFactorizationsPerIter = 3;

/// @brief Ruiz equilibration (spec 4.3): sweep cap and stopping tolerance.
///
/// Ruiz's own convergence result is linear with a small constant, so the
/// practical sweep count is single digits; OSQP and Clarabel both ship 10.
/// The tolerance stops early once every row/column norm is within
/// `kIpqpRuizTol` of 1, which on a well-scaled KKT matrix happens in two or
/// three sweeps.
inline constexpr Index kIpqpRuizSweeps = 10;
inline constexpr double kIpqpRuizTol = 1.0e-3;

/// @brief SECTION 2.2'S CONSERVATIVE `rho` FLOOR ON AN EVIDENCE FAILURE -- the
/// level a step is permitted at when a factorization SUCCEEDED but could not
/// report usable inertia evidence.
///
/// ITS OWN NAME, EQUAL TODAY TO `kIpqpLadderInit` BUT NOT THE SAME CONTRACT
/// (co-review I-3). The ladder constant is a STEP SIZE -- where the
/// Wachter-Biegler climb starts when an inertia reading says the system needs
/// shifting. This one is a MINIMUM MAGNITUDE under a solve that has no reading
/// at all. Sharing the symbol made a ladder retune silently move an
/// evidence-failure policy, with the seam pins tracking the change rather than
/// catching it; two names is how the two contracts stay separable.
///
/// AN ABSOLUTE MAGNITUDE, NOT A MULTIPLE OF THE WORKING `rho`, and it is a
/// MINIMUM rather than the level the factorization runs at (that is
/// `max(this iteration's IC trial, this floor)`). Section 2.2 names no sizing
/// at all, so the
/// choice is implementation latitude; what settles it is the branch the clause
/// exists for. On a backend reporting `kUnavailable` for EVERY factorization
/// -- the Accelerate case section 2.2 names -- a level-proportional floor
/// compounds without bound: measured on a two-row convex fixture, `rho * 100`
/// at the schedule's start left a permanent floor of 800 under the solve,
/// which then converged to the PROXIMALLY BIASED point (x = 0.0026 where the
/// answer is 0.75) and spent its entire 60-iteration budget doing it. Section
/// 2.2 says a step IS PERMITTED; a floor that makes the tier unusable on the
/// platform the clause names is not an implementation of it.
///
/// AND THE HONESTY IS CARRIED BY THE DOWNGRADE, NOT BY THE SHIFT. No finite
/// `rho` is provably sufficient without a reading -- that is precisely what
/// the missing evidence would have told us -- which is why section 2.2 pairs
/// "a step is permitted" with "the certificate is downgraded FOR THE WHOLE
/// SOLVE" rather than with a magnitude. This constant is ledgered under that
/// argument (settler ruling, plan section 7 note (n)) and NOT under the word
/// "conservative", whose plain Wachter-Biegler sense would point at a LARGER
/// shift than the ladder's smallest rung.
inline constexpr double kIpqpEvidenceFailureRhoFloor = kIpqpLadderInit;

/// @brief Section 6.2 conjunct (i): the factor by which `mu` must have been
/// reduced ACROSS the window for the window not to be a stall.
///
/// The barrier form of SSN's own "improvement is demanded over the whole
/// window, not per step". Two is the smallest factor that is unambiguously a
/// reduction rather than noise, and it is deliberately feeble for the same
/// reason `kSsnStallImproveFactor`'s 1% is: the test must not fire on a
/// trajectory that is merely slow, only on one that is not moving. A healthy
/// Mehrotra iteration reduces `mu` by an order of magnitude or more per step,
/// so five accepted steps that together cannot halve it are not a slow solve.
inline constexpr double kIpqpStallMuFactor = 2.0;

/// @brief Section 6.2 conjunct (iii): the fraction-to-boundary step below
/// which a step counts as "dying", demanded on EVERY step of the window.
///
/// Spec 6.2 states the value (`1e-2`). It is a WHOLE-WINDOW property by
/// construction -- "never abort on one tiny-alpha iteration" -- so a single
/// healthy step anywhere in the window disarms the conjunct.
inline constexpr double kIpqpStallAlpha = 1.0e-2;

} // namespace detail

/// @brief Why a tier solve STOPPED without a standing certificate.
///
/// SsnEscape's vocabulary and SsnEscape's warning, restated because they bind
/// this tier identically: **NONE OF THESE CERTIFIES ANYTHING**, and the
/// driver **branches on `escape_reason`, never on `status`** -- `kInfeasible`
/// is a certificate word the walk issues and this tier never does (spec 6.3:
/// IP-PMM has no homogeneous self-dual embedding and therefore no
/// infeasibility certificate).
///
/// The indefinite/numerical boundary is plan section 7 note (h), which is a
/// settler ruling and not a judgement call at the call site:
///   * `kIndefinite` -- an inertia reading WAS taken (an
///     `InertiaEvidence::State` was actually observed) and DISAGREED with the
///     required signature: at the section 2.2 item 4 final certification
///     factorization, or when the ladder reached `ipqp_reg_max` with
///     the reading still wrong. Saddle-suspect; section 2.3 item 4 routes it
///     to the SSN warm grade.
///   * `kNumerical` -- everything else that is not budget, stall or
///     infeasible-suspect: a factorization failure, an UNREADABLE inertia (no
///     evidence state observed at all), a non-finite iterate, residual or
///     step.
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
/// A RATIO rule, not an absolute threshold, and three-way rather than binary
/// on purpose: a pair whose BOTH members are tiny carries no information about
/// which side of the face it is on, and forcing it either way is exactly the
/// thresholding-boundary artifact the E1 study recorded (one row at relative
/// slack 1.33e-8 against a 1e-8 absolute rule, whose dual was unambiguous).
/// `kUncertain` is counted (`IpqpCounters::ipqp_face_uncertain`) and handed to
/// the exact tier-3 refinement rather than asserted.
enum class IpqpFace {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

/// @brief THE SECTION 5.4 PAYLOAD GRADE this solve started at, reported on
/// `IpqpResult::restart_grade`. `kBaseWarm` splits the currency's SIGNED
/// bound price (lossy at a two-sided bound); `kFullWarm` carries `zL`/`zU`/
/// `mu` unflattened. `.superpowers/w1-t7-report.md` FIX ROUND 2.
enum class IpqpRestartGrade {
    kCold = 0,
    kBaseWarm = 1,
    kFullWarm = 2,
};

/// @brief THE IMMUTABLE CLAMP-CENTRED BOX (T4.a; spec 2.1, plan ruling 2).
///
/// Computed ONCE at solve entry from the effective trust-region radius and
/// never rebuilt inside a solve:
///
///     c(i)      = clamp(0, lower(i), upper(i))
///     lo_eff(i) = max(lower(i), c(i) - Delta)
///     up_eff(i) = min(upper(i), c(i) + Delta)
///
/// THE CENTRE RULE IS `refine_on_face`'s OWN, and that is the reason it is
/// this rule rather than the plain origin. `QpEngine::refine_on_face`
/// (qp_engine.cpp's gate, qp_engine.h's public-API precondition) gates its
/// refined point against a window "centred on the clamped origin",
/// `c = min(max(0, lo), up)`, and takes no centre parameter. `QpProblem`
/// permits zero to lie OUTSIDE the box (qp_problem.h::validate checks sizes
/// and the upper-triangle convention, nothing else). A tier that centred its
/// own box on the plain origin would therefore hand tier 3 a point gated
/// against a DIFFERENT window than the one the tier itself solved in --
/// silently, and only on the problems where zero is outside the box, which is
/// the worst possible failure profile. Sharing the rule removes the
/// disagreement by construction.
///
/// THE CONTRACT, CHOSEN AND STATED: `IpqpEngine::solve()` takes NO centre
/// parameter. The clamp rule IS the documented contract. A warm iterate
/// (`IpqpSeed::x`, task 7) lives INSIDE this box; it never redefines the
/// centre and never becomes the refinement centre. A trust-region
/// shrink-retry rebuilds the box at the new radius under the same rule and
/// re-clamps the iterate into it.
struct IpqpBox {
    Vec centre;          ///< n. `clamp(0, lower, upper)`.
    Vec lo_eff;          ///< n. `max(lower, centre - Delta)`.
    Vec up_eff;          ///< n. `min(upper, centre + Delta)`.
    double radius = 0.0; ///< The EFFECTIVE Delta this box was built at.

    /// The first index (ascending) whose effective width is NOT STRICTLY
    /// POSITIVE (`lo_eff(i) >= up_eff(i)`), or -1 when every width is
    /// positive. See IpqpBounds for what a non-negative value here means.
    ///
    /// `>=` rather than `==` deliberately, so a CROSSED declared box
    /// (`lower(i) > upper(i)`, which `QpProblem::validate` does not reject --
    /// it checks sizes and the upper-triangle convention only) lands here too
    /// and DECLINES rather than throwing. A crossed box is a legitimate,
    /// infeasible QP, not a caller error, and the walk answers it exactly
    /// (`QpStatus::kInfeasible`) where a barrier method has no interior to
    /// start from at all.
    Index zero_width_index = -1;
};

/// @brief THE TIER'S EFFECTIVE BOUNDS AND ITS DOMAIN GATE (T4.b; plan rulings
/// 7 and r3.2). Replaces the WITHDRAWN verbatim reuse of `BoundSet`.
///
/// `BoundSet` (detail/interior/bound_set.h) is the NLP engine's:
/// reduced-space indices, RELAXED bound values, fixed variables already
/// eliminated, damping indicators materialized by the NLP's own classifier.
/// A `QpProblem` has none of that -- dense n-vectors with a +/-1e20 absent
/// sentinel, nothing eliminated -- so the reuse was withdrawn (plan section 7
/// note d) in favour of this.
///
/// REPRESENTATION, SETTLED HERE (task 3's carried item). DENSE, not index
/// lists. The ipqp_math.h kernels every iteration runs are written against
/// dense `(x, l, u, zl, zu)` with presence decided by `ipqp_has_lower` /
/// `ipqp_has_upper`, so dense IS the shape the hot path wants and no overload
/// beside those kernels is needed. Index lists would be a SECOND answer to
/// "which bounds are present", derivable from the first and able to disagree
/// with it after any edit; the counts below carry everything the orchestration
/// actually needs (the complementarity denominator and the inertia
/// bookkeeping) without that risk.
///
/// THE ZERO-WIDTH RULE, CHOSEN AND STATED. A subproblem containing ANY
/// `lo_eff(i) == up_eff(i)` pair is OUT OF THIS TIER'S DOMAIN. The build
/// reports it (`zero_width_index`), the dispatch DECLINES pre-solve and routes
/// to the walk, and `IpqpCounters::ipqp_declined_pinned` records it. **A
/// DECLINE IS NOT AN ESCAPE**: the tier never ran, so it never counts toward
/// `ipqp_escapes` or the K = 3 retirement threshold.
///
/// Why declining covers every case, rather than relaxing the pair by an
/// epsilon the way the v2 draft proposed: under the clamp centre a zero-width
/// pair can arise ONLY at a declaration-level `lower(i) == upper(i)` or at
/// `Delta == 0`. That is the tree's own statement, at qp_engine.h's section 6
/// note -- "lo_eff(i) == up_eff(i) can only happen at Delta == 0 for a
/// variable not already genuinely kFixed". Both cases are exactly what the
/// walk solves best (it pins the variable and eliminates it), an
/// epsilon-relaxed pin would no longer be an EXACT pin, and the tier keeps a
/// strict interior by construction instead of by an epsilon. The v2 rule and
/// its `ipqp_fixed_bounds_relaxed` counter are deleted entirely (plan section
/// 7 note e): no equal-bound pair survives to the barrier, so there is nothing
/// to relax.
struct IpqpBounds {
    Vec lower; ///< n. `IpqpBox::lo_eff`. `<= -kIpqpInfBound` means ABSENT.
    Vec upper; ///< n. `IpqpBox::up_eff`. `>= kIpqpInfBound` means ABSENT.

    Index num_lower = 0; ///< Variables with a finite effective lower bound.
    Index num_upper = 0; ///< Variables with a finite effective upper bound.

    /// `IpqpBox::zero_width_index` carried through: the first index whose
    /// effective width is not strictly positive, or -1.
    Index zero_width_index = -1;

    /// True iff this subproblem is inside the tier's domain, i.e. no zero-width
    /// pair. `false` is a DECLINE, not a failure -- see the zero-width rule
    /// above.
    ///
    /// Defined in the .cpp, not here: it is a predicate consulted ONCE per
    /// solve at the domain gate, not a per-element hot loop, so CLAUDE.md
    /// section 5's inlining criterion does not apply to it and this header's
    /// declarations-and-inline-constexpr-only budget does.
    bool in_domain() const;
};

/// @brief The tier's own iterate, carried across majors by task 7's
/// subproblem-level warm restart (spec 5.1 flow (b)).
///
/// Deliberately NOT routed through `QpSolution`, which has no slack and no
/// barrier block and whose shape is published: the tier carries its state the
/// way `QpEngine` carries `BorderState`/`HotState`, as engine-internal
/// currency.
///
/// EVERY BLOCK IS VALIDATED AT `solve()`'s BOUNDARY and a malformed one THROWS
/// (CLAUDE.md section 4). The section 5.4 COLD DEGRADE is the DRIVER's, taken
/// before a seed is built at all: by the time one reaches this engine it is a
/// caller's assertion about this problem's dimensions.
struct IpqpSeed {
    Vec x; ///< n. Repaired INTO the box (never the box's centre -- see IpqpBox).
    /// mi, > 0, or ALL-ZERO meaning ABSENT. Spec 5.2 item 1 recomputes it from
    /// `bi - Ai x`; with `ipqp_warm_repair` off a seed whose `s` is not
    /// strictly positive degrades COLD rather than being consumed (ruling R1).
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

/// @brief The relative KKT residual the tier converges on (T4.d, plan ruling 5).
///
/// A NEW implementation, owned by this tier. The reuse ledger's "QP
/// residual-scale helpers" row was WITHDRAWN (plan section 7 note d): the
/// walk's helpers are `QpEngine` PRIVATE MEMBERS bound to a `WorkingSet`
/// (qp_engine.cpp), and this tier has no working set and cannot reach them.
/// The DISCIPLINE is nonetheless the walk's, from
/// `detail::free_block_stationarity` (qp_engine.h): every residual is divided
/// by a `max(1, ...)` fold over the largest terms that went into it, so the
/// test is scale-free upward and never LOOSER than an absolute one.
///
/// Full-KKT and WorkingSet-free, and taken on the UNREGULARIZED problem: the
/// proximal terms `rho (x - zeta)` and `delta (y - lambda_est)` are a solver
/// device, so a point is judged against the caller's QP, not against the
/// regularized one the last factorization described.
///
/// Convergence-agreement pins against the walk are the cross-check that the
/// two implementations judge alike.
struct IpqpResiduals {
    double stationarity = 0.0;    ///< ||Hx + g + Ae'ye + Ai'yi - zl + zu||inf / scale.
    double primal_eq = 0.0;       ///< ||Ae x - be||inf / scale.
    double primal_iq = 0.0;       ///< ||Ai x + s - bi||inf / scale.
    double complementarity = 0.0; ///< max pair |s.y|, |(x-l).zl|, |(u-x).zu| / scale.

    /// The largest of the four -- the single number the stopping rule and the
    /// stall window read.
    double worst() const;
};

/// @brief THE SECTION 6.2 STALL ESCAPE'S EVIDENCE BLOCK.
///
/// Plan section 7 note (b), FINAL (r3): the spec v2 draft's three
/// `ipqp_stall_reason_*` counters are DROPPED as ill-posed -- all three
/// conjuncts hold at EVERY stall escape by construction, so a partition among
/// them is degenerate and a "which one fired" census would be an invented
/// answer to a question the test does not ask. The information travels here
/// instead: the three conjunct VALUES at the moment the window closed, so a
/// reader sees HOW stalled the trajectory was rather than a fabricated
/// attribution.
///
/// Populated ONLY on a `IpqpEscape::kStall` escape; `fired` is the flag that
/// says so, and every numeric field is 0 otherwise rather than carrying a
/// stale window's values.
struct IpqpStallEvidence {
    bool fired = false;

    /// Accepted steps in the closed window (`IpqpOptions::ipqp_stall_window`).
    Index window = 0;

    /// CONJUNCT (i): `mu(window start) / mu(window end)` -- the factor by
    /// which the barrier parameter was reduced ACROSS the window. The
    /// conjunct holds (i.e. contributes to a stall) iff this is
    /// `< detail::kIpqpStallMuFactor`. `+inf` if `mu` reached exactly 0,
    /// which cannot be a stall.
    ///
    /// THIS CONJUNCT ALSO CARRIES SECTION 6.2'S "RESET ON A MEHROTRA TARGET
    /// CHANGE THAT ACTUALLY DROPPED `mu`" (settler ruling, plan section 7
    /// note (o)): a reset on ANY `mu` drop would re-arm on every healthy step
    /// and make this conjunct vacuous, so the only drop that coherently counts
    /// as the target "taking" is the `kIpqpStallMuFactor` one the conjunct
    /// already names, and the reset IS this conjunct failing.
    ///
    /// ONE DEFERRAL, STATED RATHER THAN INFERRED: the ratio is evaluated only
    /// when the window CLOSES, so a genuine 10x `mu` drop at accepted step 2
    /// does not re-arm the window until step `ipqp_stall_window`. That is
    /// conservative in the safe direction and cannot produce a false stall --
    /// the ratio is measured across the WHOLE window, so a drop anywhere
    /// inside it still resets -- it only delays a re-arm by at most one
    /// window.
    double mu_ratio = 0.0;

    /// CONJUNCT (ii): `1 - res(end)/res(start)` on `max(primal_inf,
    /// dual_inf)` -- the RELATIVE improvement across the window, which the
    /// conjunct holds iff it is `< 1 - detail::kSsnStallImproveFactor`
    /// (1%). Negative when the residual grew.
    double residual_improvement = 0.0;

    /// The SMALLEST `min(alpha_p, alpha_d)` over the window -- plan section 7
    /// note (b)'s named "min alpha".
    double min_alpha = 0.0;

    /// CONJUNCT (iii)'s ACTUAL TEST VALUE, and it is deliberately a second
    /// field rather than a replacement for `min_alpha` above. The conjunct is
    /// "`min(alpha_p, alpha_d) < 1e-2` on EVERY step in the window", so what
    /// decides it is the LARGEST per-step `min(alpha_p, alpha_d)`: the
    /// conjunct holds iff even the healthiest step in the window was below
    /// `detail::kIpqpStallAlpha`. `min_alpha` alone cannot witness a
    /// whole-window property -- one dying step would satisfy it while four
    /// full steps sat beside it -- which is exactly the "never abort on one
    /// tiny-alpha iteration" rule section 6.2 states. Both are carried
    /// because the plan names the first and the test uses the second.
    double max_step_alpha = 0.0;
};

/// @brief THE SECTION 6.3 INFEASIBLE-SUSPECT ESCAPE'S EVIDENCE BLOCK.
///
/// **A SIGNATURE, NEVER A PROOF** (section 6: IP-PMM has no homogeneous
/// self-dual embedding and therefore no infeasibility certificate). The tier
/// emits `IpqpEscape::kInfeasibleSuspect` carrying this block and NEVER
/// `QpStatus::kInfeasible`; promoting the suspicion to a certificate is the
/// driver-layer failure the SSN contract exists to prevent.
///
/// Section 6.3 names three things this block must carry -- "each signal with
/// its value, the window, and the least-infeasible point" -- and they are the
/// three groups below.
struct IpqpInfeasibilityEvidence {
    bool fired = false;

    /// True iff the EXHAUSTION route fired (the tier ran out of budget with
    /// the signature standing) rather than the standing windowed route. The
    /// two measure the growth conjunct differently -- see `dual_growth` and
    /// `dual_step_growth` -- for `ssn_engine.h`'s own reason: on the
    /// exhaustion route the divergence and the last progress are the SAME
    /// accepted step, so a windowed growth reference would read 1.
    bool exhaustion_route = false;

    /// Accepted steps in the window the signals were measured over.
    Index window = 0;

    // --- signal (a): the primal residual, flat on a positive floor ---------

    double primal_start = 0.0; ///< `max(primal_eq, primal_iq)` at window start.
    double primal_end = 0.0;   ///< ... and at the window's end.
    /// `1 - primal_end/primal_start`; the conjunct holds iff this is below
    /// `1 - detail::kSsnStallImproveFactor` AND `primal_end` is above the
    /// solve's own feasibility target -- "flat ON A POSITIVE FLOOR", both
    /// halves.
    double primal_improvement = 0.0;

    // --- signal (b): multiplier norm growth --------------------------------

    double dual_norm_start = 0.0; ///< `||(y, z)||inf` at the reference point.
    double dual_norm_end = 0.0;   ///< ... and at the window's end.
    /// `dual_norm_end / max(1, dual_norm_start)`, floored at 1 so a
    /// zero-multiplier reference is measured absolutely
    /// (`detail::kSsnDualGrowthFactor`'s own convention). The conjunct holds
    /// iff this is `>= detail::kSsnDualGrowthFactor`.
    double dual_growth = 0.0;
    /// Growth across the MOST RECENTLY ACCEPTED STEP alone. Read only on the
    /// exhaustion route, where the conjunct additionally demands
    /// `>= detail::kSsnDualStepGrowth`; 0 on the standing route.
    double dual_step_growth = 0.0;

    // --- optional Farkas corroboration (IpqpOptions::ipqp_farkas_gate) -----

    /// True iff the Farkas test (one matvec plus O(m), no factorization) on
    /// the window's normalized dual INCREMENT, projected onto the sign cone,
    /// corroborated the signature. **IT NEVER CERTIFIES** (section 6.3): a
    /// false here is a report that was not corroborated, not a report that
    /// was withdrawn, and the escape fires either way.
    bool farkas_corroborated = false;
    double farkas_residual = 0.0; ///< Relative `||A^T y||inf`; 0 when the gate is off.
    double farkas_gap = 0.0;      ///< Relative `<b, y>`; 0 when the gate is off.

    // --- the least-infeasible point ---------------------------------------

    /// n. The iterate with the SMALLEST `max(primal_eq, primal_iq)` seen in
    /// this solve -- section 6.3's own third requirement, and the point a
    /// feasibility-mode fallback (the W2 hook) wants to start from. Empty
    /// when the block did not fire.
    Vec least_infeasible_x;
    double least_infeasible_primal = 0.0; ///< That point's own primal residual.
};

/// @brief One tier solve's outcome.
///
/// **BRANCH ON `escape_reason`, NEVER ON `status`** -- SsnResult's rule,
/// binding here for the same reason: `status` alone would let a driver promote
/// a suspicion to a certificate.
struct IpqpResult {
    QpStatus status = QpStatus::kOptimal;
    IpqpEscape escape_reason = IpqpEscape::kNone;
    IpqpCounters counters;

    /// The section 5.4 grade this solve STARTED at: `kCold` when no seed was
    /// passed, otherwise the seed's own grade. Unchanged by a warm-kill --
    /// the kill is reported by `counters.ipqp_warm_restart_abandoned`, and the
    /// pair (grade, abandoned) is what says a warm attempt was made and
    /// dropped.
    IpqpRestartGrade restart_grade = IpqpRestartGrade::kCold;

    /// True iff the domain gate declined this subproblem pre-solve
    /// (IpqpBounds' zero-width rule). `counters.ipqp_declined_pinned` is 1 and
    /// every other counter is at its default.
    ///
    /// EXACTLY ONE OTHER FIELD IS MEANINGFUL ON THAT PATH, and naming it
    /// matters because the routing chain reads it: `box` IS populated -- the
    /// decline is DERIVED from it, so it has to be -- and it carries
    /// `zero_width_index`, which is the index that caused the decline. No
    /// iterate, no face, no residual: the tier never ran.
    bool declined_pinned = false;

    /// True iff the certificate was DOWNGRADED (spec 2.2 item 4). A downgraded
    /// solve never reports `kOptimal`.
    ///
    /// FIVE WAYS TO GET HERE, and they are not all failures -- `escape_reason`
    /// is what separates them, which is why the driver branches on THAT and
    /// never on `status`. The first four pair with one
    /// `ipqp_final_inertia_read` value each:
    ///   * the read was taken and DISAGREED (`read == 1`, `kIndefinite`);
    ///   * the read was taken and no usable evidence came back -- no observed
    ///     state, or a perturbed-pivot report (`read == 2`, `kNumerical`);
    ///   * the read was DECLINED because `ipqp_require_final_inertia` is false
    ///     (`read == 3`, escape `kNone` -- a downgrade with NO escape, no
    ///     census entry and no section 6.1 retirement charge);
    ///   * the read was REFUSED by the factorization budget before it could
    ///     run (`read == 3` likewise -- it never happened -- with escape
    ///     `kBudget`).
    /// The FIFTH is task 5's, and it is the one case where the final read's
    /// own value says nothing about the downgrade:
    ///   * A MID-SOLVE EVIDENCE FAILURE. Section 2.2's evidence-failure policy
    ///     states that an `InertiaEvidence::state != kObserved` permits a step
    ///     "only at a conservative `rho` floor AND the certificate is
    ///     downgraded FOR THE WHOLE SOLVE". The downgrade is therefore a
    ///     property of the solve's history, not of its last factorization: a
    ///     solve that read unusable evidence at iteration 3, took its
    ///     conservative-floor steps, converged, and then passed a perfectly
    ///     readable final inertia read reports `read == 0` AND
    ///     `certificate_downgraded == true`, with `escape_reason == kNone`.
    ///     "For the whole solve" is what makes that the correct pair.
    /// Always false on a solve that certified with no evidence failure, and on
    /// a declined-pinned one, which never reaches the read at all.
    bool certificate_downgraded = false;

    /// True iff section 2.2's evidence-failure policy was invoked ANYWHERE in
    /// this solve: some factorization succeeded but reported an
    /// `InertiaEvidence::state` other than `kObserved`, so the tier raised its
    /// modification to a conservative floor, took its steps there, and
    /// downgraded the certificate for the whole solve. Implies
    /// `certificate_downgraded`. Distinct from a FAILED factorization, which
    /// is not an evidence failure at all and escapes `kNumerical` at once.
    bool inertia_evidence_failed = false;

    // --- the point ---------------------------------------------------------

    Vec x;        ///< n.
    Vec lambda_e; ///< me.
    Vec lambda_i; ///< mi, >= 0.

    /// SIGNED bound multiplier, `QpSolution::z`'s convention (>= 0 at an
    /// active lower bound, <= 0 at an active upper one), recombined as
    /// `zl - zu` and FORCED TO 0 at a trust-region-pinned index -- exactly
    /// QpSolution's contract, including its stationarity caveat: at such an
    /// index the reported quantities do NOT satisfy stationarity, because the
    /// multiplier that balanced the row was the TR dual and TR duals are
    /// internal.
    Vec z;

    // --- the tier's own blocks (task 7's seed reads these) ------------------

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
    /// A SECOND QUANTITY BESIDE `rho`, NOT A COMPONENT OF IT (T4b). `rho` is
    /// the section 3.2 PROXIMAL schedule: it defines the subproblem, enters
    /// the right-hand side, and is what the gate measures against. This is
    /// Ipopt's `delta_w`: a diagonal-only modification of the Newton matrix,
    /// additive on top of `rho`, applied uniformly in the Ruiz-scaled system,
    /// and absent from the right-hand side entirely. Reporting their sum in
    /// one field would lose exactly the distinction the tier was rebuilt to
    /// make, which is why task 8's `ipqp.reg` event carries both.
    ///
    /// NOT the solve's high-water mark (`counters.ipqp_rho_demanded_max`) and
    /// NOT the ladder's memory (`counters.ipqp_rho_demanded_last`): it is the
    /// working value of the last step, so a solve whose ladder armed early and
    /// disarmed later reports `0.0` here and a nonzero max, which is the
    /// honest pair. Structurally `0.0` on a solve that took no step at all.
    double rho_mod = 0.0;

    // --- the face blocks (spec 2.3 item 2; the routing chain's input) -------

    std::vector<IpqpFace> ineq_face;  ///< mi.
    std::vector<IpqpFace> lower_face; ///< n. kInactive at an ABSENT lower bound.
    std::vector<IpqpFace> upper_face; ///< n. kInactive at an ABSENT upper bound.

    /// mi / n / n, the two shapes `QpSolution` publishes an active set in.
    /// Derived from the three face vectors above: a row is active iff its
    /// verdict is `kActive` (an UNCERTAIN row reads inactive here and is
    /// counted in `ipqp_face_uncertain` -- never forced either way).
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
/// STATEFUL ACROSS SOLVES, like `QpEngine`'s border cache and `SsnEngine`'s
/// pattern cache: the KKT factorization, its symbolic analysis and the
/// `IpqpKktLayout` scatter plan persist on the instance, so a sequence of
/// subproblems sharing a sparsity pattern pays ONE symbolic analysis (spec
/// 4.1's hoisting rule). `IpqpOptions::ipqp_hoist_symbolic == false` turns
/// that off and forces a fresh symbolic pass every entry.
///
/// ONE LAYOUT SERVES ONE BUFFER for its whole life (IpqpKktLayout's own
/// contract): the layout on this instance is only ever handed
/// `kkt_.matrix()`, and nothing else may be.
class IpqpEngine {
  public:
    explicit IpqpEngine(const QpOptions &opts);

    IpqpEngine(const IpqpEngine &) = delete;
    IpqpEngine &operator=(const IpqpEngine &) = delete;

    /// Defined in the .cpp for the same reason `IpqpBounds::in_domain()` is:
    /// a const accessor is not a per-element hot loop, and this header carries
    /// declarations and `inline constexpr` only.
    const QpOptions &options() const;

    /// Attach a ledger for instrumentation (nullptr = off, default off).
    /// `QpEngine::attach_ledger`'s contract verbatim, including its
    /// failure-path rule: a `solve()` that THROWS emits no record and does not
    /// advance the per-solve counter, so the next successful solve gets the
    /// label it would have had if the throwing call had not been made.
    ///
    /// The emitted `SolveRecord` carries a QP-SHAPED PROJECTION of this tier's
    /// work (`minor_iters` <- `ipqp_iters`, `factorizations` <-
    /// `ipqp_factorizations`, `symbolic_analyses` <- `ipqp_symbolic_analyses`)
    /// so one ledger holds all three kernels' rows on one scale. The full
    /// `IpqpCounters` travel on `IpqpResult`, never through this projection.
    void attach_ledger(Ledger *ledger, std::string label_prefix);

    /// @brief Solve `qp` by the interior-point tier.
    ///
    /// @param qp        the subproblem. Validated (`QpProblem::validate`).
    /// @param seed      the section 5 warm restart, or `nullptr` for a COLD
    ///                  solve. `nullptr` IS COLD unconditionally -- this call
    ///                  never silently consumes `warm_carry()` on the caller's
    ///                  behalf, so which start a solve ran from is a property
    ///                  of the call and not of the instance's history.
    /// @param iopts     the tier's own settings (validated by
    ///                  `validate_sqp_options`; re-checked here at the API
    ///                  boundary, because this engine is reachable without a
    ///                  driver).
    /// @param overrides the walk's own per-solve override type
    ///                  (`qp_types.h`), unchanged: `tr_radius` disables at
    ///                  +inf, `primal_delta`/`dual_mu` are sentinel-negative.
    ///                  `tr_radius` is the ONE field this tier reads --
    ///                  `primal_delta`/`dual_mu` are the walk's regularized
    ///                  working-set device and have no meaning for a barrier
    ///                  method, which carries its own `(rho, delta)`; they are
    ///                  VALIDATED (so a malformed value is still refused) and
    ///                  then deliberately unused, which is stated here rather
    ///                  than left for a reader to infer from silence.
    ///
    /// @throws std::invalid_argument for CALLER errors -- a malformed
    ///         `QpProblem`, an out-of-range `IpqpOptions` or `SolveOverrides`
    ///         field, a seed whose blocks are wrongly sized or non-finite. A
    ///         solve that cannot make progress
    ///         reports a status and an `IpqpEscape` and does NOT throw; that
    ///         separation is `SsnEngine::solve`'s and is kept exactly.
    IpqpResult solve(const QpProblem &qp, const IpqpSeed *seed, const IpqpOptions &iopts,
                     const SolveOverrides &overrides);

    /// @brief THE CROSS-MAJOR CARRY (spec 5.1 flow (b)): the state the last
    /// solve on this instance finished at, or `nullptr` when none is armed.
    /// Engine-internal currency the caller passes straight back and never
    /// inspects. A solve that produced no usable state LEAVES IT STANDING,
    /// which is what makes 5.1's "a shrink-retry does not reset the seed" true.
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
    /// decision and, with it, the section 7 note (a) verify-once discipline.
    bool analyzed_ = false;

    /// The spec 5.1 flow (b) carry and its armed flag. Committed at the END of
    /// a solve that produced a finite iterate, so an unusable solve cannot
    /// overwrite a good carry.
    IpqpSeed carry_;
    bool carry_armed_ = false;

    Ledger *ledger_ = nullptr;
    std::string label_prefix_;
    Index solve_counter_ = 0;
};

/// @brief THE SECTION 6.1 ESCAPE LADDER: K consecutive escapes retire the tier
/// for the remainder of ONE SQP solve.
///
/// SEPARATE FROM `IpqpEngine` DELIBERATELY, and not merely for testability.
/// Section 6.1's decision is ACROSS subproblems -- "after K = 3 consecutive
/// escapes WITHIN ONE SQP SOLVE the tier is retired for the REMAINDER of that
/// solve" -- and no single subproblem's own solve can observe it. The engine
/// is per-subproblem and stateful only in its factorization cache; putting a
/// cross-major tally on it would make two majors of the same SQP solve share
/// state through an object whose lifetime is not the solve's. The routing
/// chain (task 6) owns one of these per SQP solve, feeds it every tier
/// outcome, and writes `SqpCounters::ipqp.ipqp_tier_retired_after` from it --
/// which is exactly what that counter's "DRIVER-SCALE ONLY" doc comment
/// describes.
///
/// THE THREE RULES, EACH FROM SECTION 6.1'S OWN TEXT:
///
/// * **Fresh decision every major. No memory.** There is no bench, no backoff
///   and no penalty carried from one subproblem to the next; the ONLY state
///   is the consecutive-escape tally and the retirement flag.
/// * **Any success resets the count.** A solve that did not escape
///   (`escape_reason == kNone`) resets the tally to 0 -- INCLUDING a solve
///   whose certificate was downgraded without an escape. RULED, and stated
///   because the two readings differ: plan section 7 note (j) is explicit
///   that such a solve carries "no census entry, no section 6.1 K=3 charge",
///   so it cannot be an escape; and section 6.1's own word for the
///   alternative is "success", which a converged solve is. Treating it as
///   neutral instead would let three consecutive clean solves under
///   `ipqp_require_final_inertia = false` sit on top of an old tally.
/// * **A DECLINE IS NEUTRAL** -- neither an escape nor a success. RULED, on
///   `IpqpCounters::ipqp_declined_pinned`'s own settled text ("a decline is
///   not an escape: the tier never ran, so this never counts toward
///   `ipqp_escapes` or the K=3 retirement threshold"). It cannot advance the
///   tally; and it cannot RESET one either, because the tier produced no
///   evidence of suitability -- a zero-width box says nothing about whether
///   the previous two escapes were a pattern.
///
/// RETIREMENT FIRES AT MOST ONCE (`IpqpCounters::ipqp_tier_retired_after`'s
/// marker-not-a-count discipline): once retired, `record()` keeps returning
/// true and `retired_after()` keeps naming the major it happened at, whatever
/// arrives afterwards.
/// @brief What ONE tier outcome is, in the section 6.1 ladder's own currency.
///
/// THREE VALUES, BECAUSE §6.1 HAS THREE RULES, and the mapping from an
/// `IpqpResult` to one of them is the ROUTING CHAIN'S, not the ladder's. The
/// convenience overload `record(const IpqpResult &, Index)` applies the
/// obvious mapping (`declined_pinned` -> kDeclined, `escape_reason == kNone`
/// -> kSuccess, anything else -> kEscape), and that is the right mapping for
/// every outcome but one: a `kBudget` exit whose §2.2 item 4 certification
/// read the factorization budget refused on an OTHERWISE CONVERGED iterate is
/// an escape to the CENSUS and a SUCCESS to the ladder. The census answers
/// "what stopped the tier"; the ladder answers "is this tier suited to this
/// problem", and a tier that keeps producing usable steps is suited to it
/// whatever the bookkeeping refused. The driver names that case explicitly
/// through this enum rather than lying to the convenience overload.
enum class IpqpLadderOutcome {
    kSuccess = 0,  ///< Resets the consecutive-escape tally.
    kEscape = 1,   ///< Advances it, and may retire the tier.
    kDeclined = 2, ///< Neutral: neither advances nor resets.
};

class IpqpEscapeLadder {
  public:
    /// @param iopts the tier's settings; only `ipqp_retire_after` is read
    ///              (validated `> 0` by `validate_sqp_options`, re-checked
    ///              here because this type is reachable without a driver).
    /// @throws std::invalid_argument if `ipqp_retire_after <= 0`.
    explicit IpqpEscapeLadder(const IpqpOptions &iopts);

    /// Record one tier outcome, observed at SQP major `major` (1-based, the
    /// index `ipqp_tier_retired_after` reports).
    /// @return true iff the tier is retired for the remainder of this solve.
    /// @throws std::invalid_argument if `major <= 0` -- a retirement marker of
    ///         0 means "never retired", so major 0 is not a representable
    ///         place for retirement to have happened.
    bool record(const IpqpResult &result, Index major);

    /// Record one tier outcome whose ladder classification the CALLER has
    /// made -- see `IpqpLadderOutcome` for the one case where it differs from
    /// the convenience overload's mapping. Same contract otherwise.
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
// Free functions -- the box and its domain gate, exposed so they are testable
// and re-derivable at the routing layer (task 6 declines a subproblem BEFORE
// entering the tier, and must build the same box to do it).
// ---------------------------------------------------------------------------

/// @brief Build the immutable clamp-centred box (IpqpBox's own contract).
/// @param qp     the subproblem, already validated.
/// @param radius the EFFECTIVE trust-region radius: `SolveOverrides::tr_radius`
///               resolved against `QpOptions::tr_radius`. +inf disables the
///               window exactly (`lo_eff == lower`, `up_eff == upper`).
/// @throws std::invalid_argument if `radius` is negative or NaN, or if a
///         declared bound is NaN. Checked here rather than left to Eigen's
///         asserts, which NDEBUG compiles out. A CROSSED declared box is NOT
///         a throw -- see `IpqpBox::zero_width_index`.
IpqpBox make_ipqp_box(const QpProblem &qp, double radius);

/// @brief Derive the tier's effective bounds and its domain verdict from a box.
IpqpBounds make_ipqp_bounds(const IpqpBox &box);

/// @brief Resolve one call's trust-region radius exactly as `solve()` does.
///
/// EXPOSED FOR THE ROUTING CHAIN (task 6), for the same reason
/// `make_ipqp_box` is: the driver runs the domain gate BEFORE entering the
/// tier, so it must build the SAME box -- and the box is a function of the
/// RESOLVED radius, not of `SolveOverrides::tr_radius` as written. Two
/// resolutions of the sentinel convention (`qp_types.h`: +inf means "use the
/// engine's own `tr_radius`") could disagree on exactly the configurations
/// where the caller left the override at its default, which is most of them.
/// One implementation, used by `solve()` itself.
double ipqp_effective_tr_radius(const QpOptions &opts, const SolveOverrides &overrides);

/// @brief The tier's stopping rule as a predicate on a residual block.
///
/// EXPOSED FOR THE ROUTING CHAIN (task 6). Section 2.3's routing needs to tell
/// a solve that CONVERGED and then had its certificate refused by the
/// factorization budget (escape `kBudget`, `ipqp_final_inertia_read == 3`,
/// `certificate_downgraded`) from one the ITERATION CAP stopped mid-descent:
/// the first is a converged, uncertified iterate and belongs at the tier-3
/// refinement; the second is a genuine escape and belongs at the cold walk.
/// The distinguishing fact is "did the residuals meet the barrier phase's
/// target", which is this predicate -- and it is exported rather than
/// re-derived at the driver so the two readings cannot drift apart.
///
/// `solve()`'s own stopping rule IS this call, so the answer here is the same
/// answer the engine acted on.
bool ipqp_residuals_meet_target(const IpqpResiduals &residuals, const QpOptions &opts,
                                const IpqpOptions &iopts);

} // namespace hven::solvers
