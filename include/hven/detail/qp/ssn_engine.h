// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

/// @file
/// @brief The semismooth-Newton QP kernel: a Newton method on a
///        Fischer-Burmeister reformulation of the same QpProblem the walk
///        solves, in which every step changes the whole implied active set.
///
/// Two modes. SsnSafeguards::kBare is the bare local method -- undamped steps,
/// no line search, no proximal term, no uncertain set, no inertia gate --
/// locally superlinearly convergent under BD-regularity and globally nothing at
/// all; it is a POSITIVE CONTROL, not a product surface. SsnSafeguards::kFull is
/// the production iteration and the default, under which no kOptimal is issued
/// anywhere without an inertia verdict read at the certified point.
///
/// Convergence claims are made on the analytic fixtures only.
///
/// References are public mathematics, read clean-room: no third-party solver
/// source was read, and no paper's numerical example is reproduced here.
/// @see docs/notes/2026-09-header-prose-archive.md §ssn_engine.h

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <Eigen/SparseCore>
#include <fmt/format.h>

#include <hven/core/pattern_hash.h>
#include <hven/detail/kkt/kkt_calls.h>
#include <hven/detail/qp/qp_problem.h>
#include <hven/drivers/sqp_solver_types.h>
#include <hven/linear/symmetric_factor.h>
#include <hven/qp/qp_types.h>

namespace hven::solvers {

namespace detail {

// Bounds at or beyond this magnitude are ABSENT and contribute no FB row.
// Identical value and meaning to qp_engine.h's kEngineInfBound and to the
// +/-1e20 sentinel nlp_model.h/qp_problem.h document; restated rather than
// reached for so this header depends on the QP data types only, not on the
// walk's internals.
inline constexpr double kSsnInfBound = 1e20;

// Floor applied to alpha = d phi / d s before it is divided by. alpha_f appears
// in BOTH the FB diagonal and the right-hand side, so it CANCELS in the
// recovered multiplier step; what it does perturb is the dx-coupling of rows
// whose true coupling is numerically zero.
inline constexpr double kSsnAlphaFloor = 1e-12;

// Pure division guard for rho = sqrt(s^2 + lambda^2): below this the pair is
// the FB function's one non-differentiable point and the C-subdifferential
// selection below is used instead. Deliberately at the far bottom of the
// double range -- this is NOT a "treat as degenerate" tolerance (which would
// be a branch, and would need a justification of its own); it exists only so
// that 0/0 cannot be formed.
inline constexpr double kSsnRhoFloor = 1e-300;

// The C-subdifferential element selected at s = lambda = 0: xi = eta =
// 1/sqrt(2), giving alpha = beta = 1 - 1/sqrt(2). The symmetric choice, and
// the one that keeps alpha strictly above the floor so the degenerate row
// still couples to dx.
inline const double kSsnDegenerateFbDeriv = 1.0 - 1.0 / std::sqrt(2.0);

// 1 / (2 - sqrt(2)) ~ 1.7071: the factor by which a satisfied FB residual
// bounds the per-row complementarity min(s, lambda). See banner section 5.
inline const double kSsnComplementarityFactor = 1.0 / (2.0 - std::sqrt(2.0));

// phi(a, b) = a + b - sqrt(a^2 + b^2), evaluated in the CANCELLATION-FREE form
// 2ab / (a + b + rho) whenever a + b > 0 -- algebraically identical, but the
// naive form has an absolute error floor of ~ulp(rho)/2 that puts a slop on the
// exit certificate and on the merit. The stable form is used only when
// a + b > 0, the same-sign regime where the cancellation lives; when a + b <= 0
// the naive expression cannot cancel, and it also covers a = b = 0, where the
// stable form's denominator vanishes.
inline double ssn_fb(double a, double b) {
    const double rho = std::sqrt(a * a + b * b);
    const double sum = a + b;
    return sum > 0.0 ? 2.0 * a * b / (sum + rho) : sum - rho;
}

// THE SAFEGUARD CONSTANTS.
//
// Every value below is an IMPLEMENTATION CHOICE, not a paper constant, and is
// stated as such with its own argument -- the same standard sqp_solver_types.h's
// kWarmResidualGrowthMax / kWarmFullStepWindow are held to.

// ---- The uncertain band (the three-set partition's only tolerance) --------
//
// The margin is measured on the FB derivative pair itself, which makes it
// dimensionless and scale-free: with (s, lambda) = rho (cos theta, sin theta),
// alpha - beta = sqrt(2) * sin(theta - pi/4), exactly 0 on the kink ray
// s = lambda and 1 at either pure state. The partition rule alpha > beta is that
// quantity's SIGN, and the uncertain set is the band around its zero.
//
// kSsnUncertainEnter = 0.1: a row enters the uncertain set within 0.1 of the
// kink in this measure, i.e. within about 4 degrees of the kink ray. 0
// reproduces the binary partition exactly.
//
// kSsnUncertainLeaveRatio = 3: the HYSTERESIS. A row leaves the uncertain set
// only once its margin reaches 3x the entering threshold, so a row whose margin
// hovers in the band keeps whatever class it had and cannot chatter.
inline constexpr double kSsnUncertainEnter = 0.1;
inline constexpr double kSsnUncertainLeaveRatio = 3.0;

// ---- The line search -----------------------------------------------------
//
// Armijo on the FB merit psi(w) = 1/2 ||F(w)||_2^2, accepted when
//
//     psi(w + t d) <= (1 - 2 * kSsnArmijoSigma * t) * psi(w).
//
// 1e-4 is the textbook Armijo constant, loose enough that a full Newton step in
// the local regime always passes it -- which is what keeps the safeguarded
// trajectory equal to the bare one on a benign fixture. The merit is the 2-norm
// square and the certificate is the inf-norm, on purpose: fb_tol certifies
// per-row quantities, and a merit function has to be smooth where phi is.
inline constexpr double kSsnArmijoSigma = 1e-4;

// Halving. A coarser factor (0.1) throws away the intermediate step lengths a
// nearly-accepted Newton step needs; a finer one (0.8) pays more residual
// evaluations for the same reduction.
inline constexpr double kSsnBacktrackFactor = 0.5;

// The step FLOOR. Below this the backtracking schedule is declared exhausted
// and the escape ladder takes over (proximal escalation, then
// SsnEscape::kNoContraction). 1e-4 with a halving factor bounds the schedule
// at 14 trial points -- 14 O(nnz) residual evaluations against the ONE
// factorization they protect.
inline constexpr double kSsnMinStep = 1e-4;

// ---- THE LM-REGULARIZED LADDER ----
//
// NAMING: *proximal* properly means an outer loop with a LAGGING centre, which
// this ladder is not. sigma is a regularization escalation on a CURRENT-ITERATE
// anchor; the `ssn_prox_*` identifiers are shipped surface and are not renamed.
//
// The current-iterate anchor makes sigma an additive increment to the two
// regularizers the engine already applies, leaving the residual untouched: sigma
// costs ITERATIONS, never ACCURACY, and the iteration's fixed points stay exact
// unregularized KKT points.
//
// The ladder is PER-SOLVE and MONOTONE -- sigma never decreases inside one solve,
// except for the second-order verification's drop to the caller's own
// regularization, which is not a rung. Two decades per rung and a 1e6 ceiling
// give SEVEN rungs from 1e-6.
//
// The ceiling test needs A RELATIVE SLACK: repeated multiplication by 100 does
// not reproduce 1e6 in binary floating point, so an exact `>= kSsnProxMax` guard
// would grant an extra rung for a 2.3e-10 relative rise. kSsnProxCapSlack closes
// that: 1e-9 is far above the accumulated relative error and far below one rung,
// so it can neither miss the ceiling nor merge two genuine rungs.
inline constexpr double kSsnProxInit = 1e-6;
inline constexpr double kSsnProxGrowth = 100.0;
inline constexpr double kSsnProxMax = 1e6;
inline constexpr double kSsnProxCapSlack = 1e-9;
// The ladder's DOCUMENTED length, asserted by the test suite rather than read
// by the iteration -- the rungs come from Init/Growth/Max above. It exists so
// the documented count and the delivered count cannot drift apart silently.
inline constexpr Index kSsnProxRungs = 7;

// The infeasibility telemetry. An infeasible convex QP has no KKT point, so
// ||F||inf flattens onto a positive floor while the multipliers of the
// contradictory rows grow without bound; both halves are required before the
// telemetry fires.
//
// The window advances on accepted steps, never on attempts, and the improvement
// demand is over the whole window rather than per step. The growth conjunct
// differs on the two routes: the standing route re-arms its reference with the
// window, while the exhaustion route keeps the start-point reference and adds
// kSsnDualStepGrowth across the most recently accepted step.
//
// A window in which the proximal ladder escalated cannot declare a stall: it is
// discarded and a fresh one starts.
//
// kSsnStallWindow = 5 accepted steps over which ||F||inf must improve on the
// window's reference by kSsnStallImproveFactor. kSsnDualGrowthFactor = 1e4
// against the multiplier norm at the reference point the route selects, floored
// at 1. kSsnDualStepGrowth = 10 is the exhaustion route's second conjunct, on
// the growth across one accepted step.
inline constexpr Index kSsnStallWindow = 5;
inline constexpr double kSsnStallImproveFactor = 0.99;
inline constexpr double kSsnDualGrowthFactor = 1e4;
inline constexpr double kSsnDualStepGrowth = 10.0;

// THE RESEARCH LEVERS' OWN CONSTANTS.
//
// Every value here is reachable ONLY through a non-default SsnOptions field, so
// none of them can move a shipped trajectory. They are stated to the same
// standard as the constants above nonetheless, because a lever measured on an
// unjustified constant measures the constant rather than the lever.

// THE LEVENBERG-MARQUARDT SIZING (SsnOptions::sigma_rule).
//
// sigma_k = kSsnLmSigmaC * min(r_k, r_k^2), with r_k = ||F_k||inf / f_scale and
// f_scale = max(1, ||F_0||inf) this solve's own start residual. min(r, r^2)
// rather than r^2 because r^2 is the right size close to the solution and far
// too small away from it, where the same theory wants r; the two are joined
// where they agree. The start-residual normalization makes the lever's first
// sigma independent of the problem's absolute scaling, and c = 1 is neutral --
// with the floor and the cap, c only selects where between them sigma sits.
inline constexpr double kSsnLmSigmaC = 1.0;

// THE WATCHDOG (SsnOptions::hint_rule + watchdog_q).
//
// q = 1 relaxed step is the DEFAULT, and is the value at which the watchdog
// reproduces the shipped iteration-0 exemption exactly on a hint that works:
// step 0 is taken unsearched, and if the residual has not come down by step 1
// the iteration returns to its best stored point and takes a monotone step
// from there. Other values are reachable through SsnOptions::watchdog_q.
inline constexpr Index kSsnWatchdogQ = 1;

// The Farkas residual test (SsnOptions::infeasibility_rule).
//
// The system {Ae x = be, a_k^T x <= b_k} is infeasible iff there is (y_e free,
// y >= 0) with Ae^T y_e + sum_k y_k a_k = 0 and be^T y_e + sum_k y_k b_k < 0.
// This file tests that pair on the Normalized dual increment projected onto the
// sign cone -- one matvec plus O(m), no factorization.
//
// kSsnFarkasResidualTol = 1e-6 on the RELATIVE residual
// ||A^T y||inf / max(1, ||(|A|^T |y|)||inf), relative so the test survives bad
// scaling. kSsnFarkasGapTol = 1e-8 on the RELATIVE Farkas objective, which must
// be at most -kSsnFarkasGapTol. Both are relative only above 1: the max(1, .) in
// each denominator is an absolute floor, which is what stops a near-zero
// denominator from manufacturing a certificate out of rounding noise.
inline constexpr double kSsnFarkasResidualTol = 1e-6;
inline constexpr double kSsnFarkasGapTol = 1e-8;

// The three-set partition of CHR 2015's scaffold. kUncertain is unreachable
// with SsnSafeguards::kBare, which is what makes bare mode reproduce the bare
// binary partition exactly.
enum class SsnRowClass : std::uint8_t {
    kInactive = 0,
    kActive = 1,
    kUncertain = 2,
};

// One bound of one variable, recast as an inequality row
//
//     c = sign * x(var) - rhs <= 0,   slack s = rhs - sign * x(var)
//
// so sign = -1, rhs = -lower(var) is the lower bound and sign = +1,
// rhs = upper(var) the upper one. Held in a flat, ascending-by-variable list
// (both rows of a two-sided variable adjacent, lower first) which IS the
// bound block's row order in K.
struct SsnBoundRow {
    Index var = 0;
    double sign = 1.0;
    double rhs = 0.0;
    // True iff this row's bound is the TRUST REGION's, strictly tighter than
    // whatever real bound (if any) the QP declares on this side. An EXPORT
    // attribute, not a structural one: a TR row occupies the same slot in K as
    // the real row it displaced, so this field must NEVER enter the structure
    // key (structure_hash / bound_rows_match_cached) -- if it did, two radii
    // would look like two structures and the pattern cache would rebuild on
    // every major of a shrink-retry loop.
    bool from_tr = false;
};

// The two norms of F one iteration needs. Separate fields rather than two
// passes because they are accumulated in the same walk over the same blocks.
struct SsnNorms {
    double inf_norm = 0.0; // ||F||inf -- the CERTIFICATE (banner section 5)
    double merit = 0.0;    // 1/2 ||F||_2^2 -- the LINE SEARCH's function
};

// What one factorization's reported inertia says about the system factorized.
//
// This is qp_engine.h's detail::InertiaVerdict, RESTATED rather than included,
// for the same reason kSsnInfBound is: this header depends on the QP data types
// and the KKT factor helper, never on the walk's internals. The rules are that
// header's exactly -- non-kObserved evidence routes to kSuspect, then perturbed
// pivots (whose counts carry no information at all, including when they happen
// to match), then the short-sum rule.
enum class SsnInertia {
    kOk,      // trustworthy AND equal to the expectation
    kSuspect, // untrustworthy: perturbed pivots, or counts that do not sum to n
    kWrong,   // trustworthy AND different from the expectation
};

inline SsnInertia ssn_inertia_verdict(const hven::linear::InertiaEvidence &e, Index expected_pos,
                                      Index expected_neg) {
    if (e.state != hven::linear::InertiaEvidence::State::kObserved) {
        return SsnInertia::kSuspect;
    }
    if (e.perturbed_pivots.has_value() && *e.perturbed_pivots != 0) {
        return SsnInertia::kSuspect;
    }
    const Index pos = static_cast<Index>(e.n_pos);
    const Index neg = static_cast<Index>(e.n_neg);
    if (pos + neg != expected_pos + expected_neg) {
        return SsnInertia::kSuspect;
    }
    return (pos == expected_pos && neg == expected_neg) ? SsnInertia::kOk : SsnInertia::kWrong;
}

} // namespace detail

// Why a solve stopped somewhere other than a converged point. kNone is the
// converged case, and all five escapes are reachable:
//
//   kBudget            hard_budget attempts were spent. Says nothing about the
//                      problem -- only that this budget was too small.
//   kSingular          the factorization threw, the step came back non-finite,
//                      or the inertia was trustworthy and wrong at the top of
//                      the ladder. escape_detail names which.
//   kIndefinite        the second-order verification's verdict: the residual
//                      satisfies fb_tol, so the point IS first-order KKT, but
//                      the inertia of K there is trustworthy and not
//                      (n, me+mi+mb). The point is a saddle or a maximizer.
//   kNoContraction     the line search's verdict: the Armijo schedule ran down
//                      to detail::kSsnMinStep without acceptance, at the top of
//                      the ladder.
//   kInfeasibleSuspect the divergence telemetry's verdict: ||F||inf stalled
//                      while the multiplier norm grew. SUSPECT because it is a
//                      diagnosis from behaviour, never a Farkas certificate.
//
// None of them certifies anything: an escaped SsnResult reports where the solve
// STOPPED, and routing it back to the walk is the only correct consumption.
//
// BRANCH ON escape_reason, NEVER ON status: kInfeasibleSuspect reports
// QpStatus::kInfeasible, which the walk issues as a CERTIFICATE, and the other
// three report QpStatus::kNumericalError.
enum class SsnEscape {
    kNone = 0,
    kBudget = 1,
    kSingular = 2,
    kNoContraction = 3,
    kInfeasibleSuspect = 4,
    kIndefinite = 5,
};

// Which rows a caller believes are active, used for the FIRST Newton step only
// (banner section 4). The two halves are INDEPENDENT: supply either, both, or
// neither. An empty half means "no hint for these rows" and those rows take
// the ordinary FB branch on step one like every later step; a non-empty half
// must be exactly sized or solve() throws.
struct SsnActivityHint {
    std::vector<bool> ineq;         // size mi, or empty
    std::vector<BoundState> bounds; // size n, or empty

    bool empty() const { return ineq.empty() && bounds.empty(); }
};

// The point a solve starts from. Every vector may be left EMPTY, which means
// "zero of the right size" -- so a default-constructed SsnStart is the cold
// start from the origin with no multipliers and no hint.
struct SsnStart {
    Vec x;        // n
    Vec lambda_e; // me
    Vec lambda_i; // mi, >= 0 expected but not required (the FB residual is
                  // defined for any sign; a negative seed simply reads as a
                  // large complementarity residual)
    // SIGNED bound multiplier, qp_problem.h's QpSolution::z convention: >= 0
    // at an active lower bound, <= 0 at an active upper bound. Split into the
    // two non-negative bound-row multipliers on ingest (lambda^lo = max(z, 0)
    // against a finite lower bound, lambda^up = max(-z, 0) against a finite
    // upper one) and recombined into SsnResult::z on exit. Added beyond the
    // bare method's own field list: without it a warm start cannot carry
    // bound activity at all.
    Vec z; // n

    // **IGNORED, AND STRUCTURALLY SO.** For a QP the slack of a row is a
    // FUNCTION of x (s = bi - Ai x), so a separately seeded slack block can
    // only agree with x or contradict it; residual() always derives it. The
    // field exists because the proximally stabilized formulation carries an
    // independent slack/dual block a caller may want to hand back in.
    // Validated for size when non-empty; never read otherwise.
    Vec slacks; // mi, or empty

    // STILL IGNORED, deliberately: the proximal term anchors at the CURRENT
    // ITERATE instead of at a lagging centre, which
    // is what keeps it a perturbation of the Jacobian alone -- the iteration
    // stays modified Newton on the exact F and one residual per attempt serves
    // both the certificate and the merit. The fields stay, still size-checked
    // when non-empty, so the interface is the right shape if a lagging centre is
    // ever needed; a caller wanting a warm proximal sequence today uses
    // SsnOptions::prox_sigma_init.
    Vec prox_center_x;      // n, or empty
    Vec prox_center_lambda; // me + mi, or empty

    // The trust region'S CENTRE, AND IT IS HONOURED -- which is why it sits
    // beside two fields that are not.
    //
    // The trust region is `[c - Delta, c + Delta]` intersected with the QP's own
    // bounds. `c` is `start.x` when this is empty and the value here when it is
    // set. Nothing else reads IT: it is not a starting iterate, not a proximal
    // anchor, and not part of the structure key.
    //
    // It exists because a caller that has already solved this subproblem in ITS
    // OWN window and wants to continue from the point it reached must say "start
    // HERE, in THAT window", and those are two different vectors. Passing the
    // iterate alone would silently recentre the window on it.
    //
    // `std::optional` rather than this struct's empty-vector convention, and the
    // difference is load-bearing: elsewhere "empty" means "zero of the right
    // size", and for a CENTRE the origin is a legitimate value, so absent and
    // "at the origin" have to be distinguishable.
    //
    // Validated for size when engaged; a non-finite entry is refused.
    std::optional<Vec> box_center; // n, or nullopt

    SsnActivityHint activity_hint;
};

// WHICH ITERATION A SOLVE RUNS. Two modes, and the bare one is a POSITIVE
// CONTROL rather than a product surface.
//
// kBare is the bare local method, bit for bit: full undamped steps, the binary
// alpha > beta partition with no uncertain set and no hysteresis, no line
// search, no dual projection, no proximal term and no inertia gate. The two
// modes are the same function, so the reproduction claim is checked on every
// suite run rather than argued.
//
// A RUNTIME SWITCH, deliberately: it costs one predictable branch per solve on a
// path that has already paid a sparse factorization, and it keeps both modes in
// one binary so a single test can compare them directly.
enum class SsnSafeguards {
    kBare = 0, ///< The bare local method.
    kFull = 1, ///< The production iteration (default).
};

// Per-solve knobs. The regularizers delta/mu are NOT here: they come from the
// QpOptions the SsnEngine was constructed with, exactly as QpEngine takes
// its own, so the two kernels are regularized identically by construction.
struct SsnOptions {
    // Which iteration to run. kFull is the shipped default; kBare is the
    // positive control (see SsnSafeguards).
    SsnSafeguards safeguards = SsnSafeguards::kFull;

    // The escalation threshold, on ATTEMPTS rather than on accepted steps -- an
    // attempt that ends in a rejected step is exactly the kind of trouble the
    // escalation exists for. On reaching it, a solve still running unregularized
    // turns the proximal term on at detail::kSsnProxInit; that is the whole of
    // the "soft_budget warns" contract, and the warning is observable rather than
    // printed (SsnCounters::ssn_prox_updates leaves zero). 12 is above every
    // benign fixture's attempt count.
    Index soft_budget = 12;

    // The hard cap, on ATTEMPTS rather than on accepted steps. A solve that
    // reaches it stops with QpStatus::kMaxIter and SsnEscape::kBudget. Must be
    // >= 0; 0 means "test the start point and take no step".
    //
    // The second-order verification is not an attempt and is not capped by this
    // field: under kFull an already-converged start point still pays the one
    // verification factorization at hard_budget = 0. It takes no step, so the
    // field's contract is intact.
    Index hard_budget = 25;

    // ||F||_inf convergence threshold. Default = SqpOptions::kkt_tol's own
    // default, per the derivation in banner section 5; use
    // ssn_fb_tol_from_kkt_tol() to track a non-default kkt_tol. Must be > 0.
    double fb_tol = 1e-6;

    // The proximal term's starting value; 0.0 is the default:
    // the proximal term is a REPAIR, not a policy. It costs iterations
    // wherever it is on, it repairs nothing on a converging solve, and every
    // case that needs it is one the escalation ladder ARMS it on. A caller
    // CONTINUING a proximal sequence (the re-solve after an escape) can start
    // it warm here; must be >= 0.
    double prox_sigma_init = 0.0;

    // THE UNCERTAIN BAND's entering threshold, on |alpha - beta| -- see
    // detail::kSsnUncertainEnter for the geometry and the hysteresis ratio
    // that derives the LEAVING threshold from it.
    //
    // 0.0 DISABLES THE UNCERTAIN SET without disabling anything else, which is
    // what makes "the uncertain set, ablated" a one-field experiment rather
    // than a rebuild. Must be in [0, 1): at 1 a strictly active row
    // (|alpha - beta| = 1) would itself be uncertain and the partition would
    // carry no information at all.
    double uncertain_tol = detail::kSsnUncertainEnter;

    // THE FOUR RESEARCH LEVERS.
    //
    // Every field below is OPT-IN and ships at the value that reproduces the
    // shipped iteration BIT FOR BIT; none of them is a default.

    // Defer the certifying exit'S INERTIA EVIDENCE.
    //
    // false (the default) factorizes K at the converged point and reads its
    // inertia before kOptimal is issued. true builds the verification attempt --
    // rows classified at the converged point, sigma dropped to the caller's own
    // regularization, K's diagonals refreshed -- but does NOT factorize it: the
    // solve returns kOptimal with `SsnResult::certification_deferred` set, and
    // the caller owes the engine exactly one of finish_deferred_certification()
    // or discard_deferred_certification() before the next solve.
    //
    // A caller wants that when it is about to re-solve the identified face
    // exactly and so already buys the second-order evidence. THE TWO
    // Certificates are not the same statement -- this engine's verification tests
    // (n, me+mi+mb) on the full augmented block at the caller's regularization,
    // the face route tests positive definiteness of the reduced Hessian on the
    // identified face -- and they can disagree. Nothing is weakened when the face
    // solve is refused: finish_deferred_certification() then returns exactly the
    // shipped verdict at exactly the shipped cost.
    bool defer_certification = false;

    // HOW sigma IS SIZED. kLadder (the default) is the failure-reactive ladder
    // verbatim. The two residual rules replace the CLIMB with a size read off the
    // residual:
    //
    //     sigma_k = max(ladder_k, clamp(c * min(r_k, r_k^2),
    //                                   kSsnProxInit, kSsnProxMax))
    //
    // with r_k = ||F_k||inf / max(1, ||F_0||inf) and c = detail::kSsnLmSigmaC.
    // THE LADDER IS RETAINED AS A MONOTONE FLOOR, not replaced: every escalation
    // trigger still climbs a rung, so a solve that cannot be repaired still
    // reaches the ceiling and escapes with the same reason. kResidualArmed is
    // inert until the ladder arms; kResidualAlways sizes sigma from attempt 0 and
    // is inert nowhere.
    SsnSigmaRule sigma_rule = SsnSigmaRule::kLadder;

    // What protects the hinted first step. kIterationZeroFree (the default) is
    // the iteration-0 exemption. kWatchdog replaces it with the published rule
    // the exemption is an unsafeguarded special case of: up to `watchdog_q`
    // relaxed steps from the hinted start, a stored best point, and a return to
    // that point followed by a monotone step if Armijo decrease has not been
    // achieved by then. The two agree whenever the hint was right and differ
    // exactly where the exemption has no answer. `watchdog_q` must be >= 1.
    SsnHintRule hint_rule = SsnHintRule::kIterationZeroFree;
    Index watchdog_q = detail::kSsnWatchdogQ;

    // WHAT TURNS A SUSPICION INTO AN EXIT. kSymptoms (the default) makes the
    // stall/growth conjunct pair the exit test. kFarkasGated keeps every symptom
    // conjunct as the ARMING condition and adds a CERTIFICATE as the firing
    // condition -- the normalized dual increment, projected onto the sign cone,
    // tested as an approximate Farkas direction. The increment is measured
    // against each route's own growth reference. It can only make the engine MORE
    // reluctant to report kInfeasible: an armed check that finds no Farkas
    // direction falls through to the ordinary routes.
    SsnInfeasibilityRule infeasibility_rule = SsnInfeasibilityRule::kSymptoms;
};

// The tolerance derivation of banner section 5, in code: an FB residual at
// `kkt_tol` buys stationarity and equality feasibility at exactly kkt_tol and
// per-row complementarity within detail::kSsnComplementarityFactor of it.
// Throws std::invalid_argument on a non-positive kkt_tol.
inline double ssn_fb_tol_from_kkt_tol(double kkt_tol) {
    if (!(kkt_tol > 0.0)) {
        throw std::invalid_argument(
            fmt::format("ssn_fb_tol_from_kkt_tol: kkt_tol must be > 0, got {}", kkt_tol));
    }
    return kkt_tol;
}

struct SsnResult {
    QpStatus status = QpStatus::kOptimal;
    SsnEscape escape_reason = SsnEscape::kNone;

    // True iff this solve reached a certifying exit under
    // SsnOptions::defer_certification and therefore returned kOptimal
    // WITHOUT the second-order verification having been factorized. The
    // status is provisional until the caller calls
    // SsnEngine::finish_deferred_certification() (which may withdraw it) or
    // SsnEngine::discard_deferred_certification() (which drops the pending
    // evidence unread, legitimate only when the caller is discarding the
    // exit anyway). ALWAYS false when the option is off, on an escape, and
    // under kBare -- so a consumer that never sets the option never sees it
    // set.
    bool certification_deferred = false;

    // The lever instruments live here rather than on SsnCounters deliberately:
    // they measure an opt-in arm, not the product, and SqpCounters::ssn is
    // serialized into a pinned CSV schema.
    //
    // `watchdog_returns` -- times the q-step watchdog exhausted its relaxed
    // window without Armijo decrease and returned to the best stored point.
    // `farkas_fired` / `farkas_refusals` -- infeasibility exits the Farkas
    // certificate confirmed, and armed symptom pairs it refused. All three are
    // structurally 0 at the shipped defaults.
    Index watchdog_returns = 0;
    Index farkas_fired = 0;
    Index farkas_refusals = 0;

    Vec x, lambda_e, lambda_i;
    // Signed bound multiplier, recombined from the two bound-row multipliers
    // (z_j = lambda^lo_j - lambda^up_j). Same convention as QpSolution::z.
    Vec z;

    // Newton steps ACCEPTED -- identical to counters.ssn_iters, which exists
    // separately only so that SqpCounters::ssn can aggregate the whole struct
    // without special-casing one field.
    Index iters = 0;
    // Numeric factorizations paid, one per ATTEMPT. An attempt can pay its
    // factorization and take NO step (an exhausted schedule, or a wrong inertia),
    // so
    //
    //     iters <= factorizations <= min(attempts, hard_budget) + 1,
    //
    // where the +1 is the certifying exit's second-order verification -- which is
    // why equality on the left is unreachable under kFull. Under
    // SsnOptions::defer_certification that +1 is NOT paid by this solve: it moves
    // to finish_deferred_certification(), or is never paid at all when the
    // caller's face solve supersedes it. Under kBare equality is exact.
    Index factorizations = 0;
    // Pardiso phase-11 symbolic analyses paid. 1 for the first solve of a new
    // structure on this engine, 0 for every later solve of the same structure
    // -- banner section 3.
    Index symbolic_analyses = 0;
    // TRIPLET REBUILDS of K's sparsity pattern paid by this solve: 1 when the
    // structure differs from the one this engine currently holds, 0 when it
    // was reused and only VALUES were refreshed. The sibling of
    // symbolic_analyses one level down -- that field counts what PARDISO
    // re-derives, this one what THIS FILE re-derives.
    Index pattern_rebuilds = 0;

    // ||F(w)||_inf at the returned point, on the UNSCALED residual.
    double fb_residual = 0.0;

    // The implied active set at the returned point, in the two shapes QpSolution
    // reports it. Derived from the FINAL iterate by the engine's own partition
    // rule -- row active iff lambda > s -- so it is the same partition the last
    // Jacobian would have selected, not a separately maintained working set.
    //
    // WRITE-ONLY, like the counters. ALWAYS POPULATED, including on an escape and
    // on a solve that took zero steps: the derivation reads the iterate, not the
    // loop's history, and on an escaped solve it describes where the solve
    // stopped and certifies nothing. A variable whose lower and upper bound rows
    // are both implied active reports kFixed; one with neither, and one with no
    // finite bound at all, reports kFree.
    std::vector<bool> ineq_active;       // mi
    std::vector<BoundState> bound_state; // n

    // TRUST-REGION ACTIVITY, size n, qp_problem.h's QpSolution::tr_active
    // contract verbatim. True at index i iff the variable is held by a TR-tight
    // effective bound rather than a real one. Such a variable reports kFree in
    // bound_state -- a real-bound-only view -- and z(i) is 0, because TR duals
    // are internal to this solve.
    //
    // Same stationarity caveat AS THE WALK'S: at a TR-pinned index the reported
    // quantities do not satisfy stationarity. Read tr_active, never z or
    // bound_state, to detect a binding radius. All-false when the solve ran
    // without a trust region, and when a finite radius never bound.
    std::vector<bool> tr_active; // n

    // How far x lies outside the trust region, and it can be far:
    // max_j max(0, x_j - up_eff_j, lo_eff_j - x_j) over the variables whose
    // effective bound came from the radius; 0.0 when no radius was supplied and
    // when the radius held.
    //
    //   * The trust region is a soft constraint in this kernel: it enters as FB
    //     bound rows, which are satisfied only at a root, so no intermediate
    //     iterate is confined to the box.
    //   * On a certifying exit the violation is bounded by the tolerance and
    //     nothing else: tr_violation <= kSsnComplementarityFactor * fb_tol. That
    //     is the only exit at which this kernel's x may be used as a
    //     trust-region step.
    //   * On any escape the value is unbounded, and is reported rather than
    //     repaired, so that x, fb_residual and the four activity vectors keep
    //     describing one point.
    double tr_violation = 0.0;

    // The uncertain set at the returned point, the third leg of the partition
    // that ineq_active/bound_state cannot express, those two reporting the binary
    // reading. ineq_uncertain[k] is true iff row k was uncertain when the LAST
    // generalized Jacobian was assembled; bound_uncertain[j] is true iff EITHER
    // of variable j's bound rows was -- the pessimistic reading, since the flag
    // exists to warn a consumer off the binary one.
    //
    // The last assembly is the returned iterate on every certifying exit under
    // kFull, and one iterate back on an escape. The distinction matters because
    // the classification carries hysteresis and cannot be recomputed from the
    // final point. On a solve that assembled no Jacobian both vectors are
    // all-false. Always all-false under SsnSafeguards::kBare.
    std::vector<bool> ineq_uncertain;  // mi
    std::vector<bool> bound_uncertain; // n

    // The proximal sigma this solve FINISHED at (0.0 whenever the ladder never
    // armed, which is every benign fixture). Write-only, like the counters, and
    // the one number a caller needs to restart a proximal sequence through
    // SsnOptions::prox_sigma_init.
    double prox_sigma = 0.0;

    // Why the solve stopped, in words, whenever there are words to add: the
    // linear solver's own message on kSingular, the stall/growth figures on
    // kInfeasibleSuspect, the exhausted schedule on kNoContraction. Carried
    // out rather than printed, per CLAUDE.md banner section 4's rule that a
    // diagnostic must fold into what the caller receives. Empty on kNone and
    // on kBudget, which say everything they have to say in the enum.
    std::string escape_detail;

    SsnCounters counters;
};

// The engine.
//
// Holds ONE KktFactor across solves, which is what makes banner section 3's
// "symbolic analysis once per structure, reused across QPs of identical
// structure" property observable: construct one SsnEngine and solve a
// sequence of same-shaped QPs through it.
//
// NOT THREAD-SAFE and not copyable, for the same reason the sparse factor is
// not: it owns live Pardiso/Accelerate internal state.
class SsnEngine {
  public:
    /// @param opts    The QP options this tier solves under.
    /// @param threads The thread count in force -- SqpSolver passes
    ///                SqpOptions::common.threads (M6 W5 T8.8). It configures
    ///                THE ONE persistent factor this tier holds, at
    ///                construction; 0 (the default, and what every pre-T8.8
    ///                construction site passed) leaves the backend's own
    ///                default alone, so a defaulted engine is bit-for-bit the
    ///                pre-T8.8 one. `QpOptions` deliberately carries no thread
    ///                field: the count is fingerprinted separately
    ///                (qp_types.h's options_fingerprint).
    explicit SsnEngine(const QpOptions &opts, int threads = 0) : opts_(opts), kkt_(threads) {}

    SsnEngine(const SsnEngine &) = delete;
    SsnEngine &operator=(const SsnEngine &) = delete;

    const QpOptions &options() const { return opts_; }

    /// The LIVE factor's own thread count, read through to the backend session
    /// (SymmetricFactor::num_threads()) rather than a stored copy -- a boundary
    /// observation of the factor this tier actually solves through.
    int num_threads() const noexcept { return kkt_.factor.num_threads(); }

    // Solve `qp` from `start`. `out` must be non-null and is fully overwritten.
    //
    // Throws std::invalid_argument for a null `out`, a malformed QpProblem
    // (qp.validate()'s own diagnostics), a wrongly sized start/hint vector, or
    // an out-of-range SsnOptions field -- i.e. for caller errors, which are
    // distinguished from SOLVER outcomes: a solve that cannot make progress
    // reports a status and an SsnEscape and does not throw.
    //
    // This overload forwards a default-constructed SolveOverrides, which
    // resolves to every opts_ value unchanged, so it is BYTE-IDENTICAL to
    // solving with no override support at all -- the same guarantee
    // QpEngine's plain overloads carry (qp_types.h).
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts, SsnResult *out);

    // The per-solve seam. Takes the WALK's own SolveOverrides rather than a
    // parallel type, and every field's sentinel and resolution rule is
    // qp_types.h's unchanged.
    //
    // TRUST REGION: lo_eff = max(lower, x0 - Delta), up_eff = min(upper,
    // x0 + Delta), about THIS solve's own start point, resolved once here and
    // used for every bound row thereafter.
    //
    // REGULARIZERS: primal_delta/dual_mu are honoured too -- but the driver's
    // adaptive-mu schedule buys nothing here, since delta and mu perturb only the
    // Jacobian and never the residual. They cost iterations, not accuracy, which
    // is the opposite trade from the walk.
    void solve(const QpProblem &qp, const SsnStart &start, const SsnOptions &sopts,
               const SolveOverrides &overrides, SsnResult *out);

    // THE DEFERRED CERTIFICATION'S TWO CLOSING MOVES. Both throw if called
    // when nothing is pending -- a caller that closes a deferral twice, or
    // closes one it never opened, has lost track of which point it is
    // certifying.
    //
    // finish_deferred_certification() PAYS THE FACTORIZATION THE SOLVE DID
    // NOT. It factorizes the matrix the solve left in place -- classified at
    // the converged point, at the caller's own regularization -- so its
    // verdict is EXACTLY the one banner section 7b's in-loop verification
    // would have returned, at the same cost. On kWrong it rewrites `out` into
    // the SsnEscape::kIndefinite escape the in-loop route issues, census
    // included; on a thrown factorization into SsnEscape::kSingular the same
    // way. `out` MUST be the SsnResult the deferring solve wrote.
    //
    // Returns true iff the certificate stands.
    bool finish_deferred_certification(SsnResult *out);

    // DROPS THE PENDING EVIDENCE UNREAD. Legitimate on exactly one path: the
    // caller is discarding the exit anyway, so no certificate is being claimed
    // and no step taken from that point. Calling it while a certificate IS
    // being claimed is the wrong-answer class banner section 7b closes --
    // which is why it is a separate, named call rather than a default.
    void discard_deferred_certification();

    // Whether this engine owes a caller one of the two calls above.
    bool has_deferred_certification() const { return deferred_pending_; }

  private:
    // -----------------------------------------------------------------------
    // Validation
    // -----------------------------------------------------------------------
    // qp_types.h's SolveOverrides precondition, applied unchanged: tr_radius
    // must be the +inf sentinel or >= 0 -- never negative (a negative Delta
    // would silently cross lo_eff and up_eff) and never NaN;
    // primal_delta/dual_mu keep the negative-means-sentinel convention but
    // reject NaN, which no downstream arithmetic can absorb.
    static void validate_overrides(const SolveOverrides &o);

    static void validate_options(const SsnOptions &s);

    void validate_start(const QpProblem &qp, const SsnStart &start) const;

    static void check_size(const Vec &v, Index want, const char *what);

    static Vec seed_vector(const Vec &v, Index want, const char *);

    // -----------------------------------------------------------------------
    // Bound rows
    // -----------------------------------------------------------------------
    // Builds the bound-row list from the EFFECTIVE bounds
    //
    //     lo_eff = max(lower, x0 - Delta),   up_eff = min(upper, x0 + Delta)
    //
    // -- qp_types.h's SolveOverrides::tr_radius contract for the walk, applied
    // here unchanged, including "about the SOLVE'S OWN start point x0" and
    // "computed once, at the top of solve()". A row is marked from_tr iff the
    // trust region is STRICTLY tighter than the real bound on that side (a tie
    // reads as the real bound, since the reported activity is then genuine).
    //
    // WITH A FINITE RADIUS EVERY VARIABLE GETS BOTH ROWS, whatever the radius
    // is, so the row list -- and therefore K's pattern -- is INVARIANT across
    // radius changes. That is what makes a shrink-retry loop free: only bound
    // VALUES move, and values are not structure. Delta = +inf reproduces the
    // real-bound list exactly, bit for bit (max(lower, -inf) is lower).
    // `centre` is the trust region's centre -- `SsnStart::box_center` when the
    // caller supplied one, the resolved start point otherwise. The two are the
    // same vector on every caller that does not set the field, which is what
    // makes the addition byte-identical for them.
    void build_bound_rows(const QpProblem &qp, const Vec &centre, double tr_radius);

    // A kFixed hint marks BOTH of a variable's rows active, which is right for a
    // genuinely fixed variable (l == u) and is a Documented degraded mode when
    // l < u. It arises because export_activity reports kFixed whenever both bound
    // rows come out implied-active, which a noisy or escaped iterate can produce;
    // the first step then solves the contradictory pair, which the dual-mu block
    // regularizes into the midpoint rather than a singular factorization.
    //
    // The choice is documented recovery, not rejection: a hint is ADVICE about
    // the first step, not a constraint, so a contradictory hint costs iterations
    // and nothing else, and rejecting would make a warm-start hand-off throw
    // because the previous solve stopped somewhere noisy.
    static bool bound_hint_active(const std::vector<BoundState> &hint,
                                  const detail::SsnBoundRow &br);

    // Splits the signed z into the two non-negative bound-row multipliers.
    //
    // A seeded z that prices a bound this QP does NOT have is a CALLER ERROR and
    // THROWS: z(j) > 0 prices variable j's lower bound and z(j) < 0 its upper one,
    // and if that side is absent the mass has nowhere to go. THE TEST IS EXACT
    // ZERO, not a tolerance -- an absent side has no multiplier at all.
    //
    // The absent-side test reads the real bound, Not the effective one, and a TR
    // row is seeded at zero: a caller's z prices the QP's own bounds, so a finite
    // radius must not make the check vacuous.
    Vec split_bound_multipliers(const QpProblem &qp, const Vec &z) const;

    // **TR ROWS ARE EXCLUDED**, which is qp_problem.h's contract verbatim: "TR
    // duals are internal to the ratio test/drop rule and are never exposed",
    // and z(i) is forced to 0 at a TR-pinned index. A caller reading z back
    // therefore sees only multipliers of the QP's own bounds, whatever radius
    // the solve ran under -- the property that lets a warm-start export survive
    // a shrink-retry loop without accumulating radius-dependent junk.
    Vec recombine_bound_multipliers(const Vec &lb, Index n) const;

    // The fixed pattern, and its reuse across solves.
    //
    // The FB diagonals get a nonzero PLACEHOLDER (-1.0) rather than their
    // eventual value, so their slots exist regardless of what any later branch
    // wants there, and the diagonal refresh addresses those slots positionally.
    //
    // sync_matrix() decides between two paths and reports which:
    //
    //   REBUILD (returns true) -- the structure differs from the cached one: emit
    //     every entry as a triplet, setFromTriplets, then record each entry's
    //     position in the compressed value array.
    //   REFRESH (returns false) -- same structure: zero the value array and
    //     re-emit, accumulating into the recorded positions. O(nnz), no sort, no
    //     allocation, and the cached symbolic analysis survives.
    //
    // The two paths share ONE emission order by construction -- for_each_entry()
    // is the single walk both drive -- and the refresh path ACCUMULATES rather
    // than assigns, because setFromTriplets sums duplicates and for_each_entry
    // emits them deliberately.
    //
    // The structure key is a COMPOSITE of two conjuncts: the combined pattern
    // hash, and the bound-row (var, sign) list compared EXACTLY rather than
    // hashed. A collision in the first is GUARDED, not merely asserted away: the
    // refresh path bounds-checks every write and requires the emission to consume
    // the map exactly, turning a structure-mismatched refresh into a thrown
    // std::runtime_error. A collision between two structures with the same entry
    // count remains the honest residual exposure.
    template <typename Emit>
    void for_each_entry(const QpProblem &qp, Index n, Index me, Index mi, Index mb,
                        Emit emit) const;

    // CONJUNCT 1 of the composite structure key: the combined pattern key over
    // the dimensions and the three input patterns. Fed through feed_pattern, NOT
    // off the raw index arrays, because qp.H/Ae/Ai are caller-supplied and
    // QpProblem imposes no compression requirement. Every ingredient goes through
    // Fnv1a::feed_index, so the digest depends on neither host byte order nor the
    // storage index width. The bound-row list is deliberately not here: it is
    // conjunct 2, compared exactly.
    std::uint64_t structure_hash(const QpProblem &qp, Index n, Index me, Index mi, Index mb) const;

    // CONJUNCT 2: the bound-row (var, sign) list, compared EXACTLY against the
    // copy cached when the current pattern was built.
    //
    // `br.var` is the load-bearing half, and mb alone does not cover it: a
    // DIFFERENT ASSIGNMENT of the same NUMBER of bound rows to variables puts the
    // bound block's off-diagonal entries in different columns of K, and reusing a
    // pattern across that would write one problem's values into another's slots.
    //
    // `sign` participates CONSERVATIVELY rather than necessarily -- a sign flip
    // moves no slot -- and is kept because an over-conservative key costs one
    // avoidable rebuild and rebuild counts are pinned currency. `rhs` and
    // `from_tr` are EXCLUDED: both are value attributes of a row whose slot they
    // do not move, and comparing `from_tr` would rebuild the pattern on every
    // radius change of a shrink-retry loop.
    bool bound_rows_match_cached() const;

    // Brings k_ up to date for `qp`. Returns true iff the pattern was rebuilt.
    bool sync_matrix(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // -----------------------------------------------------------------------
    // The activity export
    // -----------------------------------------------------------------------
    //
    // The engine's own partition rule, applied to the returned iterate: a row
    // is active iff its multiplier exceeds its slack (equivalently
    // alpha > beta, banner section 2). Written, never read.
    void export_activity(SsnResult *out, const Vec &slack_i, const Vec &slack_b,
                         const Vec &lambda_b, Index n, Index mi) const;

    // The uncertain export. `klass` is null when no Jacobian was ever
    // assembled, which is the honest "no classification was made" case and is
    // reported as all-false rather than as all-decided.
    void export_uncertain(SsnResult *out, const std::vector<detail::SsnRowClass> *klass, Index n,
                          Index mi) const;

    // -----------------------------------------------------------------------
    // The globalization helpers
    // -----------------------------------------------------------------------

    // ||(lambda_e, lambda_i, lambda_b)||_inf -- the divergence telemetry's
    // growth measure. The EQUALITY multipliers are included even though they
    // are sign-free: an inconsistent equality block diverges exactly the same
    // way an inconsistent inequality block does, and excluding them would make
    // the test blind to the commonest infeasibility an SQP linearization
    // produces.
    static double dual_norm(const Vec &le, const Vec &li, const Vec &lb);

    // w + step * dw, with the dual projection applied to the two non-negative
    // multiplier blocks.
    //
    // The projection is the WRONG-HINT mitigation, and the only cheap one
    // available: the landing configuration (s, lambda) = (0, lambda < 0) is a
    // DIFFERENTIABLE point of phi, so no derivative re-selection can help. A
    // wrongly hinted active row lands with a negative multiplier and is confined
    // to a line on which a non-degenerate row's root does not lie; clipping
    // lambda to 0 moves the pair onto the kink, where the symmetric
    // subdifferential element applies and the confinement dissolves.
    //
    // It is a projection onto a convex set that Contains every solution, so it
    // cannot move the iterate away from the solution set, and it is inert at any
    // point that already satisfies it. lambda_e is untouched: equality
    // multipliers are free-sign.
    static void trial_point(const Vec &x, const Vec &le, const Vec &li, const Vec &lb,
                            const Vec &dw, double step, bool project, Index n, Index me, Index mi,
                            Index mb, Vec &t_x, Vec &t_le, Vec &t_li, Vec &t_lb);

    // Sets sigma and re-emits K's VALUES for it. The proximal increment lands
    // on the same two slots primal_delta and dual_mu already occupy, so the
    // pattern is untouched and sync_matrix takes its REFRESH path -- O(nnz), no
    // sort, no allocation, and Pardiso's cached symbolic analysis survives. The
    // return value is ignored deliberately: a refresh is not a pattern rebuild
    // and must not be counted as one.
    void set_prox_sigma(double sigma, const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // sigma = max(the ladder's monotone state, the residual-driven size).
    // Under the shipped kLadder rule `lm_sigma_` is identically 0, so this is
    // `set_prox_sigma(ladder_sigma_, ...)` and `prox_sigma_ == ladder_sigma_`
    // at every point the shipped iteration can observe.
    void apply_sigma(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // One rung up the ladder. Returns false at the ceiling, which is the
    // caller's signal to escape rather than to keep paying factorizations.
    //
    // **THE CEILING TEST CARRIES A RELATIVE SLACK**, because the rungs are
    // computed by repeated multiplication and do not land on the cap exactly.
    // detail::kSsnProxCapSlack carries the derivation.
    bool escalate_prox(const QpProblem &qp, Index n, Index me, Index mi, Index mb);

    // -----------------------------------------------------------------------
    // The Farkas residual test, matvec-only
    // -----------------------------------------------------------------------
    //
    // Farkas' lemma: the constraint system is INFEASIBLE iff there is (y_e free,
    // y >= 0) with Ae^T y_e + sum_k y_k a_k = 0 and <be, y_e> + sum_k y_k b_k < 0.
    //
    // `dle`/`dli`/`dlb` is the DUAL INCREMENT to be tested. It is projected onto
    // the sign cone, normalized by its own inf-norm, and the two Farkas
    // quantities are evaluated -- the RESIDUAL and the OBJECTIVE -- each reported
    // RELATIVE to the same combination taken in absolute value, so a badly scaled
    // row cannot make a non-certificate look like one or the reverse.
    //
    // One matvec over Ae/Ai plus O(mb) plus O(n): no factorization, no solve, and
    // no allocation beyond two n-vectors. Returns true iff both tolerances are
    // met; `resid_out`/`gap_out` carry the two relative quantities.
    bool farkas_certificate(const QpProblem &qp, const Vec &dle, const Vec &dli, const Vec &dlb,
                            Index mb, double *resid_out, double *gap_out) const;

    // -----------------------------------------------------------------------
    // Residual
    // -----------------------------------------------------------------------

    // Fills every scratch block and returns both norms of F: the inf-norm that
    // certifies (banner section 5) and the merit 1/2||F||_2^2 the line search
    // decreases. Both come out of the same walk over the same blocks.
    detail::SsnNorms residual(const QpProblem &qp, const Vec &x, const Vec &le, const Vec &li,
                              const Vec &lb, Vec &resid_x, Vec &resid_e, Vec &slack_i, Vec &slack_b,
                              Vec &phi_i, Vec &phi_b) const;

    // BRANCH SELECTION.
    //
    // Writes alpha/beta/row_resid/klass at slot `slot`. `hinted` says a hint
    // governs THIS row; `hint_active` is that hint's verdict; `guarded` selects
    // the three-set classification and the uncertain damping. Under kBare this
    // function IS the bare local method, statement for statement.
    //
    // The three-set partition. The FB pair's own margin |alpha - beta| measures
    // how far the row is from the kink ray s = lambda, where its classification
    // is undecidable. The classification is read with HYSTERESIS -- enter the
    // uncertain set at `tau`, leave it only at kSsnUncertainLeaveRatio * tau --
    // so a row whose margin hovers inside the band cannot chatter.
    //
    // The tie policy IS A CONSEQUENCE OF THE BAND, not a separate rule: an exact
    // tie has margin 0, inside every band with tau > 0, so a tie never decides
    // anything. kBare's binary rule and the activity export break an exact tie
    // toward INACTIVE via a strict >, since QpSolution's contract is binary.
    //
    // An uncertain row gets BOTH BRANCHES damped: the FB pair is replaced
    // wholesale by the SYMMETRIC element detail::kSsnDegenerateFbDeriv, so the
    // row's diagonal becomes -(1 + mu + sigma) instead of racing toward -mu or
    // toward a decoupled row, and its coupling to dx keeps a moderate weight.
    // That is still a generalized Jacobian element -- the symmetric element sits
    // on the C-subdifferential's unit circle and the row is within O(tau) of the
    // kink -- so this stays a semismooth Newton method rather than a heuristic.
    //
    // A Hinted row takes its hint, uncertain or not: an exact-solution seed can
    // put an active row exactly at the kink, so a classification that outranked
    // the hint would damp precisely the row the hint exists to snap onto its
    // face. The hint governs step 0 only.
    static void select_branch(double s, double lam, double phi, bool hinted, bool hint_active,
                              bool guarded, double tau, std::vector<double> &alpha,
                              std::vector<double> &beta, std::vector<double> &row_resid,
                              std::vector<detail::SsnRowClass> &klass, Index slot);

    QpOptions opts_;
    detail::KktFactor kkt_;
    SpMatRM k_;
    Index dim_ = 0;
    std::vector<detail::SsnBoundRow> bound_rows_;
    std::vector<double> real_lower_, real_upper_;
    // The pattern cache: the structure key k_ was built for -- both conjuncts
    // of it (the combined pattern digest, and the bound-row (var, sign) list
    // the digest deliberately omits) -- and the position in k_.valuePtr() of
    // each entry for_each_entry() emits, in emission order.
    std::vector<std::size_t> value_pos_;
    std::uint64_t structure_key_ = 0;
    std::vector<std::pair<Index, double>> structure_bound_key_;
    bool has_structure_ = false;
    // The regularizers THIS solve resolved to (SolveOverrides, or opts_ when
    // the override is at its sentinel). Members rather than parameters because
    // for_each_entry is driven from two places and both must see the same pair.
    //
    // base_* is what the CALLER asked for and never moves inside a solve;
    // eff_* is base_* + prox_sigma_ and is what for_each_entry emits. Keeping
    // both is what lets the proximal ladder step without losing the caller's
    // own regularization (a ladder that overwrote eff_* would silently reset
    // primal_delta to sigma).
    double base_delta_ = 0.0;
    double base_mu_ = 0.0;
    double eff_delta_ = 0.0;
    double eff_mu_ = 0.0;
    // The proximal-point regularizer. Per-solve, monotone non-decreasing,
    // 0.0 whenever the escalation ladder never armed.
    double prox_sigma_ = 0.0;
    // `ladder_sigma_` is the LADDER's own monotone state and `lm_sigma_` the
    // residual-driven size; `prox_sigma_` is their max.
    // Under the shipped SsnSigmaRule::kLadder `lm_sigma_` never leaves 0, so
    // `ladder_sigma_ == prox_sigma_` identically and the pair is invisible.
    double ladder_sigma_ = 0.0;
    double lm_sigma_ = 0.0;

    // The deferred-certification pending-verdict state. `deferred_pending_`
    // is the whole of the contract: while it is true, k_ holds the matrix a
    // certifying exit declined to factorize and kkt_ must not be touched. The
    // other five fields are only what the withdrawal message needs, saved
    // because the loop that knew them has exited.
    bool deferred_pending_ = false;
    Index deferred_pos_ = 0;
    Index deferred_neg_ = 0;
    Index deferred_attempt_ = 0;
    double deferred_fb_residual_ = 0.0;
    double deferred_fb_tol_ = 0.0;
};

} // namespace hven::solvers
