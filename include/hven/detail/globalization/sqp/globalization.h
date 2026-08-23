// Copyright 2026-present Grant R. Hecht. Licensed under the Apache License, Version 2.0
// (see LICENSE).

#pragma once

// globalization.h — the acceptance decision that turns the full-step driver of
// sqp_driver.h into a globally convergent method: given the OLD and NEW values
// of the objective f and the infeasibility measure h, plus what the QP model
// PREDICTED, decide whether the trial iterate may be kept.
//
// The strategy in isolation: nothing here evaluates a model, solves a QP, or
// touches the driver's loop -- judge() is a pure function of its StepContext
// plus one scalar of state (the funnel width).
//
// SOURCES. [KLV] D. Kiessling, S. Leyffer, C. Vanaret, "A Unified Funnel
// Restoration SQP Algorithm", arXiv:2409.09208, Math. Program. (2025) --
// every equation number below is KLV's. Where the interior-point port of the
// same paper (funnel_acceptance.h) and [KLV] disagree, [KLV] WINS; the two
// deliberate deviations from that port a reader might otherwise "fix" are
// recorded at the constants below. The port's barrier adaptations are
// precisely what must NOT be inherited: they exist because an IPM judges a
// BARRIER objective phi = f + auxiliary along a line search with a step length
// alpha, and neither is true here -- this is a trust-region SQP judging f
// itself.
//
// Deliberate deviations from that port, resolved toward [KLV]:
//  - NO theta_min/theta_max machinery: KLV has no such thresholds — the funnel
//    condition IS the ceiling and the switching condition is tested
//    unconditionally; carrying the WB ceiling/gate over would suppress f-type
//    steps KLV's proof relies on.
//  - CONSTANT VALUES are KLV Table 1's published values (tau_bar = 100,
//    kappa_bar = 1.25, delta = 0.999, sigma = 1e-4, beta = 0.99, kappa = 0.5),
//    NOT the port's Uno-shipped defaults. beta matters most: with kappa = 0.5
//    the guaranteed contraction factor theta = 1-(1-beta)(1-kappa) is 0.995
//    rather than 0.99995, i.e. the funnel actually closes.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/qp/qp_types.h>

namespace hven::solvers {

// THE ALGORITHM (KLV Algorithm 2, optimality-phase branch, transcribed).
//
// A funnel is "a relaxation of the feasible set that allows a constraint
// violation up to a given upper bound tau > 0" (KLV Sec. 2.4.2): one scalar,
// monotonically tightened, replaces a filter's list of (h, f) pairs.
//
//   (init) Eq. (9):   tau_0 = max(tau_bar, kappa_bar*h_0)
//                     with tau_bar > 0 and kappa_bar > 1 ("to ensure that
//                     the initial point is acceptable").
//   (8)    FUNNEL CONDITION, NECESSARY for acceptance, tested on EVERY trial
//          before either type test:  h(x_hat) <= tau.
//   (10)   SWITCHING CONDITION, selects the iteration type:
//                     delta_m_f(d) >= delta*(h)^2,  delta in (0,1)
//          ("ensures that the algorithm does not take infinitely small steps
//          and thus avoids convergence toward infeasible points").
//          * HOLDS -> f-TYPE: accepted iff Armijo
//   (11)               delta_f >= sigma*delta_m_f(d),  sigma in (0,1)
//            holds. Width NOT updated (KLV Thm. 1, f-type branch:
//            "tau_(k+1) = tau_(k)").
//          * VIOLATED -> h-TYPE: accepted iff
//   (12)               h(x_hat) <= beta*tau,  beta in (0,1),
//            then the width decreases by
//   (13)               tau_+ = (1-kappa)*h(x_hat) + kappa*tau, kappa in (0,1).
//   Otherwise rejected ("and either the trust-region radius or the step size
//   is reduced").
//
// ORDERING IS NOT A CHOICE: Algorithm 2 nests these as if/else-if — an f-type
// trial that fails Armijo is REJECTED, it does not fall through to h-type.
//
// MONOTONICITY IS UNCONDITIONAL HERE: beta < 1 makes (13) give
// tau_+ <= theta*tau with theta = 1-(1-beta)(1-kappa) in (0,1) (KLV Thm. 1
// case 1). The contraction depends on nothing but the trial and old width, so
// the width is monotonically non-increasing for ANY accepted sequence, no side
// condition — which is also why the re-widening edge the barrier-port note
// discloses cannot exist under Eq. (13).

// KLV parameters: every value is Table 1 (Sec. 5.1); admissible ranges are
// those stated with the equation using each constant.

/// tau_bar > 0: absolute floor on the initial width (Eq. (9); Table 1 = 100).
inline constexpr double kFunnelTauBar = 100.0;
/// kappa_bar > 1: multiplier on h_0 (Eq. (9); Table 1 = 1.25) — strictly > 1
/// so the start point lies strictly inside its own funnel.
inline constexpr double kFunnelKappaBar = 1.25;
/// delta in (0,1): switching-condition coefficient (Eq. (10); Table 1 = 0.999).
inline constexpr double kFunnelDelta = 0.999;
/// sigma in (0,1): Armijo fraction of the predicted decrease (Eq. (11);
/// Table 1 = 1e-4).
inline constexpr double kFunnelSigma = 1.0e-4;

/// beta and kappa live one level down, in detail::, unlike their five KLV
/// siblings above: the IPM engine's funnel (detail/globalization/
/// funnel_acceptance.h) already defines hven::solvers::kFunnelBeta and
/// hven::solvers::kFunnelKappa WITH A DIFFERENT BETA (0.9999 vs KLV's 0.99),
/// so leaving these at namespace scope would be an ODR violation the linker
/// resolves silently. detail:: nesting is the sanctioned collision mechanism
/// for the two engines' internal names (never new top-level namespaces); the
/// five non-colliding siblings stay put so this deviation from the verbatim
/// import is exactly as large as the collision that forces it.
namespace detail {
/// beta in (0,1): funnel sufficient-decrease margin (Eq. (12); Table 1 = 0.99).
inline constexpr double kFunnelBeta = 0.99;
/// kappa in (0,1): convex-combination coefficient in the width update
/// (Eq. (13); Table 1 = 0.5 — KLV: "we picked kappa = 0.5, which worked well").
inline constexpr double kFunnelKappa = 0.5;
} // namespace detail

/// epsilon: feasibility tolerance separating a feasible point from an
/// INFEASIBLE STATIONARY one (KLV Sec. 5.1 uses the same epsilon for both
/// termination outcomes). Used only by the restoration signature.
inline constexpr double kFunnelFeasEps = 1.0e-6;

/// Minimum number of consecutive rejections at the SAME iterate before the
/// restoration signature may fire.
///
/// NOT A PAPER CONSTANT (KLV gives no value; its own analogue, Algorithm 4's
/// alpha_min, is likewise unstated). It supplies Lemma 5 case 1's hypothesis:
/// the QP goes infeasible only "for Delta_TR sufficiently small", and a radius
/// is only demonstrably small AFTER several shrinks without the trial escaping
/// the funnel — without this gate the signature fires on trial #1 of a healthy
/// solve.
///
/// DERIVED, AND COUPLED TO kTrShrinkFactor — change them together. Both
/// arguments bound the same quantity (how much evidence "sufficiently small"
/// needs) and land on the same count from opposite directions:
///  (I) SHRINK DEPTH. With shrink factor 1/2, r consecutive rejections certify
///      Delta_r = 2^-r * Delta_0. The smallest reduction distinguishing "below
///      an unknown threshold" from "near it" is one order of magnitude:
///      2^-r <= 1/10 needs r >= log2(10) = 3.32, i.e. r = 4 (at r = 3 the
///      radius has fallen only 8x, below the resolution the claim is made at).
///      Halve kTrShrinkFactor instead and the same requirement gives r = 2.
///  (II) EVIDENCE MIX. rejections_at_iterate also counts ROUTED QP FAILURES —
///      trials no strategy judged. The driver's QP retry is one-shot per chain,
///      so consecutive routed failures end the solve: in any surviving run of r
///      rejections, routed rows are NON-ADJACENT and number at most ceil(r/2).
///      At odd r that is a strict majority (the measured failure mode); at even
///      r exactly half. So (II) admits only EVEN counts.
/// Intersection of (I) {r >= 4} and (II) {even}: smallest element 4. Neither
/// constraint picks it alone. Routed rows still COUNT (the counter means "the
/// radius was shrunk here and nothing escaped the funnel"); (II) requires only
/// that judged rows cannot be outnumbered.
///
/// A unit test pins the gate against this constant; its fixture's radius
/// schedule was derived alongside the value.
inline constexpr Index kRestoreMinRejections = 4;

/// @brief Everything the acceptance test reads about one trial step.
///
/// h is the KLV infeasibility measure (see the generalization note at
/// GlobalizationStrategy); sqp_driver.h's constraint_violation_l1 computes it.
struct StepContext {
    double f_old = 0.0;   ///< f(x_k)
    double f_new = 0.0;   ///< f(x_k + d)
    double h_old = 0.0;   ///< h(x_k)
    double h_new = 0.0;   ///< h(x_k + d)
    /// delta_m_f(d) of KLV Eq. (6b): predicted objective DECREASE from the QP
    /// model, POSITIVE for a model promising progress.
    ///
    /// Semantics caveat: pred_df <= 0 does NOT mean model stationarity. d = 0
    /// satisfies our subproblem's linearized constraints only at h = 0
    /// (build_subproblem sets Je p = -cE); at an INFEASIBLE iterate the QP is
    /// forced away from the origin and routinely pays delta_m_f <= 0 for it.
    /// Only at h = 0 is pred_df >= 0 guaranteed — which is why the restoration
    /// signature cannot rest on pred_df alone: it is a WEAK signal in exactly
    /// the regime conjunct (b) selects for.
    double pred_df = 0.0; 
    /// Whether the trust region is active at d. Carried for the DRIVER's
    /// radius update (KLV Algorithm 3); the acceptance test never reads it,
    /// by design (a unit test pins that).
    bool tr_active = false;
    /// How many trials the driver burned SHRINKING THE RADIUS at the CURRENT
    /// iterate (reset by the driver whenever the iterate moves). Read only by
    /// the restoration signature; see conjunct (e).
    ///
    /// NOT ONLY THIS STRATEGY'S OWN kReject VERDICTS: the driver may count
    /// ROUTED QP FAILURES (kNumericalError/kMaxIter shrink-retries never shown
    /// to judge()). Those satisfy the description exactly. Consequence: a
    /// kRestore verdict may rest partly on QP-failure evidence (measured at
    /// 2 of 3 on HS1 before the re-derivation); kRestoreMinRejections'
    /// argument (II) bounds routed rows to at most half the evidence at the
    /// shipped count. Defaults to 0, which the gate treats as "no evidence":
    /// a driver that forgets the field gets NO kRestore verdicts ever
    /// (fail-safe toward kReject), never spurious restorations.
    Index rejections_at_iterate = 0;
};

/// @brief The four outcomes of KLV Algorithm 2's optimality branch.
enum class StepVerdict {
    /// f-type: Eq. (10) and (11) both hold. Width unchanged.
    ///
    /// DRIVER OBLIGATION: an ACCEPTANCE verdict, not a convergence verdict.
    /// Algorithm 2 opens with the ||d||=0 KKT short-circuit AHEAD of the
    /// acceptance test; the driver must keep testing KKT residuals at the top
    /// of every major and must not infer convergence from a run of kAcceptF —
    /// Eq. (11) with sigma = 1e-4 is satisfied by arbitrarily small decreases.
    kAcceptF,
    /// h-type: Eq. (10) fails, Eq. (12) holds. Width shrinks by Eq. (13).
    kAcceptH, 
    /// "The step is rejected, and either the trust-region radius or the step
    /// size is reduced" (KLV Sec. 2.4.2).
    kReject,  
    /// Switch to the feasibility restoration phase (see FunnelStrategy).
    ///
    /// DRIVER OBLIGATION: ADDITIVE, NOT A REPLACEMENT. KLV's authoritative
    /// restoration triggers are the driver's — Algorithm 5's "subproblem
    /// infeasible" and Algorithm 4's "alpha < alpha_min" (here: a radius
    /// floor). The driver must not skip them because the strategy also reports
    /// kRestore: this signature is a heuristic early signal that can miss;
    /// an infeasible subproblem is a fact leaving no step at all. Both
    /// authoritative triggers ship in sqp_driver.h (ELASTIC TIER; RADIUS
    /// FLOOR), firing independently of this verdict; all three enter the SAME
    /// restoration phase. This one can fire EARLIEST and can also miss; the
    /// other two are facts.
    kRestore
};

// Generalization note for h: KLV Sec. 2.4.1 defines h(x) = ||c(x)||_1 for an
// NCO whose only inequalities are bounds ("always feasible throughout SQP
// iterations"). This engine carries general inequalities cI(x) <= 0, so h
// generalizes to ||cE||_1 + sum_j max(0, cI_j), reducing to the paper's h
// exactly when mi() == 0. Bounds are excluded for the paper's own reason —
// and here that assumption is a THEOREM, since the subproblem's box is
// l - x .. u - x, so every driver-produced iterate satisfies the bounds by
// construction. The measure lives beside the evaluation it is computed from
// (sqp_driver.h's constraint_violation_l1).

/// @brief Interface the driver holds.
class GlobalizationStrategy {
  public:
    virtual ~GlobalizationStrategy() = default;

    /// @brief Renders the verdict for one trial step and applies whatever
    /// state update that verdict implies. NOT idempotent (an accepted h-type
    /// step tightens the funnel).
    /// @throws std::logic_error When called before the first reset().
    ///
    /// Interface contract: MAY BE CALLED UP TO TWICE FOR THE SAME ITERATE —
    /// when the driver attempts a second-order correction on a kReject whose
    /// violation increased, it calls judge() a second time on the CORRECTED
    /// point before deciding whether to shrink the radius. A caller-supplied,
    /// stateful strategy must tolerate that second call without corrupting its
    /// own state. The shipped FunnelStrategy qualifies because only an ACCEPT
    /// verdict mutates its width and the first of the two calls is always a
    /// kReject — at most one mutating verdict per iterate. A strategy mutating
    /// state on ANY verdict must account for this explicitly.
    virtual StepVerdict judge(const StepContext &ctx) = 0;

    /// Begins a new solve from an iterate whose infeasibility is h0. Must be
    /// called before the first judge(). h0 must be finite and >= 0.
    virtual void reset(double h0) = 0;

    /// @brief Re-enters the OPTIMALITY phase after a restoration phase moved
    /// the iterate to infeasibility h_restored. Called EXACTLY ONCE per
    /// restoration resumed from, always between two judge() calls, never
    /// before the first reset().
    ///
    /// Deliberately NOT reset(h_restored): reset() is Eq. (9), whose tau_bar =
    /// 100 is an ABSOLUTE FLOOR — re-initializing a funnel tightened to, say,
    /// tau = 3 would push it back out to 100 and undo every h-type acceptance
    /// earned; the monotone width is exactly what KLV Thm. 1 case 1 sums, so
    /// re-widening forfeits the convergence argument. Algorithm 2 instead
    /// RE-BASES on exit via the ordinary Eq. (13) update at the restored point.
    ///
    /// The default IS reset(h_restored) because it is the only generic body
    /// writable here: a strategy whose state this header cannot see has no
    /// re-basing rule. A stateful strategy that cares must override; one that
    /// does not gets the honest fallback.
    ///
    /// Callout: A DECORATOR MUST FORWARD THIS EXPLICITLY. A wrapper forwarding
    /// only reset()/judge() would, on the default path, call its OWN reset()
    /// and re-initialize the wrapped strategy — turning a re-basing into
    /// exactly the Eq. (9) re-widening this hook exists to avoid (the shipped
    /// tests fell into this trap).
    virtual void resume_from_restoration(double h_restored) { reset(h_restored); }
};

/// @brief KLV Algorithm 2 (optimality phase), transcribed.
///
/// State is one scalar: the funnel width tau. reset() sets it by Eq. (9);
/// judge() reads it in Eq. (8)/(12) and tightens it by Eq. (13).
///
/// THE kRestore TRIGGER, and exactly how far it is a transcription. KLV's two
/// AUTHORITATIVE restoration entries are both outside this class's view —
/// Algorithm 5 switches when "subproblem infeasible"; the line-search variant
/// (Algorithm 4) when "alpha < alpha_min". Neither a QP status nor a step
/// length is in StepContext; both stay the driver's (see
/// StepVerdict::kRestore). What the signature CAN see is the configuration
/// those triggers end: Lemma 5 case 1 — at h_k > 0, shrinking Delta_TR
/// eventually makes the linearized constraints inconsistent ("for Delta_TR
/// SUFFICIENTLY SMALL, QP(x_k, Delta_TR) is infeasible"), which is precisely
/// Algorithm 5's trigger.
///
/// THE HYPOTHESIS "SUFFICIENTLY SMALL" IS LOAD-BEARING: Lemma 5 says nothing
/// about a FIRST trial at full radius. Conjunct (e) supplies it — without it
/// the signature fires on a healthy solve at trial #1. Worked counterexample
/// (pinned by a regression test): min x s.t. x^2 - 1 = 0 from x0 = 0.01 gives
/// h_old = 0.9999, tau = 100, linearization 0.02*p = 0.9999 forcing p = 49.995,
/// pred_df = -49.995, h_new = 2499.5 — conjuncts (a)-(d) ALL hold, yet nothing
/// is wrong: the paper's response is kReject plus a radius shrink, and its
/// Lemma 3 guarantees that shrink succeeds. Declaring restoration there would
/// abandon a problem that solves in a handful of majors.
///
/// All FIVE conjuncts must hold:
///  (a) h_new > tau                 — funnel-incompatible trial: Eq. (8), the
///                                    necessary condition, fails.
///  (b) h_old > eps                 — current iterate infeasible: Lemma 5
///                                    case 1's precondition, at KLV Sec. 5.1's
///                                    tolerance.
///  (c) pred_df <= 0                — the QP offers no objective decrease. A
///                                    WEAK signal kept deliberately as a
///                                    necessary-not-sufficient filter: at an
///                                    infeasible iterate delta_m_f <= 0 is
///                                    NORMAL (see StepContext::pred_df). It
///                                    excludes a model still promising
///                                    progress; it must never be read as
///                                    stationarity.
///  (d) h_new >= h_old              — the step reduces no infeasibility either.
///                                    Implied by (a) whenever the current
///                                    iterate is itself inside the funnel, but
///                                    tested anyway: the driver may hand back
///                                    an iterate this strategy never accepted
///                                    (restoration exit, caller start point),
///                                    and from OUTSIDE the funnel a genuine
///                                    h-reducing step must be plain kReject.
///  (e) rejections_at_iterate       — the radius has ALREADY been shrunk at
///        >= kRestoreMinRejections    least this many times at this iterate
///                                    without the trial escaping the funnel:
///                                    Lemma 5's "sufficiently small",
///                                    discharged with evidence rather than
///                                    assumed — the conjunct separating a
///                                    healthy overshoot from a dead end.
///                                    (Evidence-mix caveat: see
///                                    StepContext::rejections_at_iterate.)
///
/// Anything funnel-incompatible that misses the signature is kReject — the
/// paper's default: reduce the radius and retry.
///
/// FULL-STEP MODE — a SECOND, OPT-IN STATE OF THIS CLASS, for warm starts.
/// Motivation (Kungurtsev & Diehl 2014, Comput. Optim. Appl. 59:475–509):
/// standard globalization actively interferes with warm starts — inside the
/// Newton domain of a NEARBY problem's solution the undamped SQP step is the
/// superlinearly convergent one, and a cold-start acceptance test spends its
/// majors refusing it. begin_full_step() puts the strategy into a mode where
/// judge() returns kAcceptF for every FINITE trial WITHOUT any of Algorithm 2's
/// tests:
///  - OPT-IN AND DRIVER-DRIVEN: engaged only on a warm-enough solve with the
///    option set (both conditions, so a cold solve and a disabled flag are
///    bit-identical to the class without the mode).
///  - NON-CERTIFYING, THE SAFETY INVARIANT: a verdict is an acceptance, never
///    a convergence claim — the driver's KKT test at the top of every major is
///    untouched, so a full-step solve exits kOptimal only through the same
///    evaluate_kkt every globalized solve exits through.
///  - NOT A LICENCE TO STEP ONTO A NaN: a non-finite trial still returns
///    kReject — the mode bypasses the acceptance TESTS, not the measurement
///    discipline.
///  - WIDTH-PRESERVING: no verdict in this mode mutates tau, which is what
///    lets exit re-base rather than re-initialize.
/// THE EXIT IS A WATCHDOG, and it is resume_from_restoration(): the driver
/// tracks the best iterate seen under the mode by ||KKT||inf and, on either
/// exit signal, RESTORES that iterate and hands the solve back to ordinary
/// funnel globalization AT THAT POINT — which is semantically exactly what
/// resume_from_restoration already means (re-entering the optimality phase at
/// a point some other mechanism chose), so the two share one function. That is
/// also why resume_from_restoration CLEARS the mode: a restoration phase
/// running while the mode is on is itself evidence the mode's premise (we are
/// in a nearby problem's Newton domain) is false, and the driver must not come
/// back out of restoration still taking unit steps.
///
/// Definitions: the three VIRTUAL overrides live out-of-line in
/// src/globalization/sqp/funnel.cpp; the four inline members after them stay
/// INLINE here deliberately — a clean Release build emits no symbol for any of
/// the four (fully inlined at every call site), and moving them would cost
/// exactly what moving the virtuals does not. The include set is unchanged by
/// that carve and deliberately unpruned (<algorithm>, <cmath>, <fmt/format.h>
/// are used only by the definitions that left): dropping them would perturb
/// the preprocessed input of most of the SQP tree, which the carve's
/// bit-identity proof held fixed; pruning owes its own proof.
class FunnelStrategy final : public GlobalizationStrategy {
  public:
    /// @brief Eq. (9): tau_0 = max(tau_bar, kappa_bar * h_0).
    /// @throws std::invalid_argument On a negative or non-finite h0: a funnel
    ///   seeded from garbage silently mis-scales every later verdict, and +inf
    ///   specifically would make Eq. (8) accept everything forever.
    void reset(double h0) override;

    /// KLV Algorithm 2, optimality branch — the paper's if/else-if ordering,
    /// not a rearrangement. See the class note for kRestore.
    StepVerdict judge(const StepContext &ctx) override;

    /// @brief KLV Algorithm 2's switch back from restoration: the width is
    /// RE-BASED at the restored point by the ordinary Eq. (13) update
    ///     tau_+ = (1 - kappa)*h_restored + kappa*tau —
    /// exactly what an accepted h-type step does, which is what the restored
    /// point IS from the funnel's view (a point reached by a mechanism that
    /// reduced h). Emphatically NOT Eq. (9) — see the base class note.
    ///
    /// MONOTONICITY HOLDS UNDER THE CALLER'S EXIT CONDITION, and only under
    /// it: tau_+ <= tau iff h_restored <= tau. The driver resumes only when
    /// h_restored <= feas_tol, and any funnel not itself tightened below the
    /// feasibility tolerance has tau >= feas_tol, so the inequality holds with
    /// room to spare. Transcribed UNCONDITIONALLY rather than clamped to
    /// min(tau, .) because Eq. (13) is not a clamped rule — a clamp would hide
    /// a caller resuming from outside its own funnel instead of reporting the
    /// width that behaviour implies.
    ///
    /// Also the ONLY way out of the full-step mode: both callers (restoration
    /// resume, full-step watchdog) are the same event to this class, and
    /// neither may leave the mode armed.
    ///
    /// @throws std::invalid_argument On negative/non-finite h_restored;
    ///   std::logic_error on a call before the first reset() (no width to
    ///   re-base).
    void resume_from_restoration(double h_restored) override;

    /// Enters the full-step mode. Callable only after reset() (a started
    /// solve's state; its eventual exit re-bases a width that must exist);
    /// idempotent while armed. Nothing but resume_from_restoration() leaves it.
    /// @throws std::logic_error If called before reset(h0).
    void begin_full_step() {
        if (!initialized_) {
            throw std::logic_error("FunnelStrategy::begin_full_step: called before reset(h0); the "
                                   "full-step mode is a state of a started solve, and its exit "
                                   "re-bases a funnel width that does not exist yet");
        }
        full_step_ = true;
    }

    /// Current funnel width tau (diagnostics/tests). Undefined — reported as
    /// the +inf sentinel — until the first reset().
    double width() const { return width_; }

    /// False until reset() succeeds; a judge() call before then throws.
    bool initialized() const { return initialized_; }

    /// True while the full-step mode is armed (diagnostics/tests).
    bool in_full_step() const { return full_step_; }

  private:
    double width_ = std::numeric_limits<double>::infinity();
    bool initialized_ = false;
    /// Full-step-mode flag; false is the mode-less class, exactly.
    bool full_step_ = false;
};

} // namespace hven::solvers
