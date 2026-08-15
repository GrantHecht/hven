#pragma once

// globalization.h — the acceptance decision that turns the full-step driver of
// sqp_driver.h into a globally convergent method: given the OLD and NEW values
// of the objective f and the infeasibility measure h, plus what the QP model
// PREDICTED, decide whether the trial iterate may be kept.
//
// TASK 5 SCOPE: the strategy in isolation. Nothing here evaluates a model,
// solves a QP, or touches the driver's loop -- judge() is a pure function of
// its StepContext plus one scalar of state (the funnel width). Task 6 wires it
// into SqpDriver::solve.
//
// =============================================================================
// SOURCES, AND WHICH ONE WINS
// =============================================================================
//
//   [KLV] D. Kiessling, S. Leyffer, C. Vanaret, "A Unified Funnel Restoration
//         SQP Algorithm", arXiv:2409.09208, Math. Program. (2025). Read in
//         full (arXiv PDF), not from memory. Every equation number below is
//         KLV's.
//   [TYC] tycho's validated funnel port,
//         ../tycho/include/tycho/detail/solvers/globalization/funnel_acceptance.h
//         (+ switching_acceptance.h, whose Wachter-Biegler skeleton it sits
//         on). That port transplanted the KLV funnel into the IPM engine's BARRIER
//         interior-point context and took its constants from the Uno solver's
//         shipped option defaults.
//
// THIS PORT RESTORES KLV's RESTORATION-SQP SEMANTICS. Where [TYC] and [KLV]
// disagree, [KLV] WINS and the resolution is recorded at the point of
// divergence below (six of them, each tagged "DIVERGENCE FROM [TYC]"). The
// barrier adaptations in [TYC] are precisely what must NOT be inherited: they
// exist because the IPM engine judges a BARRIER objective phi = f + auxiliary along a
// line search with a step length alpha, and neither of those things is true
// here. This file is a trust-region SQP judging f itself.
//
// =============================================================================
// THE ALGORITHM (KLV Algorithm 2, "Restoration funnel acceptance test",
// optimality-phase branch), transcribed
// =============================================================================
//
// A funnel is "a relaxation of the feasible set that allows a constraint
// violation up to a given upper bound tau > 0" (KLV Sec. 2.4.2). One scalar,
// monotonically tightened, replaces a filter's whole list of (h, f) pairs.
//
//   (init) Eq. (9):     tau_0 = max( tau_bar, kappa_bar * h_0 )
//                       with tau_bar > 0 and kappa_bar > 1, the latter
//                       "to ensure that the initial point is acceptable".
//
//   (8) FUNNEL CONDITION, a NECESSARY condition for acceptance, tested on
//       every trial before either type test:
//                       h(x_hat) <= tau.
//
//   (10) SWITCHING CONDITION, which selects the iteration TYPE:
//                       delta_m_f(d) >= delta * (h)^2,     delta in (0,1).
//        It "ensures that the algorithm does not take infinitely small steps
//        and thus avoids convergence toward infeasible points".
//
//        * If (10) HOLDS the trial is f-TYPE, accepted iff the Armijo-type
//          sufficient decrease condition
//   (11)                delta_f >= sigma * delta_m_f(d),   sigma in (0,1)
//          holds. The width is NOT updated (KLV Thm. 1, f-type branch (17):
//          "tau_(k+1) = tau_(k)").
//
//        * If (10) is VIOLATED the trial is h-TYPE, accepted iff the funnel
//          sufficient decrease condition
//   (12)                h(x_hat) <= beta * tau,            beta in (0,1)
//          holds, and then the width is decreased by
//   (13)                tau_+ = (1 - kappa) * h(x_hat) + kappa * tau,
//                                                          kappa in (0,1).
//
//   Otherwise the step is rejected "and either the trust-region radius or the
//   step size is reduced".
//
// ORDERING IS NOT A CHOICE. Algorithm 2 nests these as if/else-if: an f-type
// trial that fails Armijo is REJECTED, it does not fall through to the h-type
// test. Transcribed verbatim in judge().
//
// MONOTONICITY, and why it is unconditional here. Since beta < 1, (12) gives
// h(x_hat) <= beta*tau, so (13) yields
//       tau_+ <= (1-kappa)*beta*tau + kappa*tau = (1 - (1-beta)(1-kappa))*tau,
// i.e. tau_+ <= theta*tau with theta = 1 - (1-beta)(1-kappa) in (0,1) -- KLV
// Thm. 1, case 1. The contraction depends on NOTHING but the trial and the old
// width, so "the funnel ... whose width is monotonically non-increasing" holds
// for ANY accepted sequence, with no side condition. (Compare [TYC]'s note (3),
// which has to disclose a re-widening edge; see DIVERGENCE 1 for why that edge
// does not exist here.)
//
// =============================================================================
// WHAT judge() DELIBERATELY DOES NOT DECIDE
// =============================================================================
//
// * The KKT short-circuit. Algorithm 2 opens with "if ||d|| = 0 then
//   acceptable <- true // KKT point found". That reads the STEP, which is not
//   in StepContext, so it cannot live here.
//
//   WHERE IT DOES LIVE, corrected in Task 9: sqp_driver.h's kZeroStepScale,
//   as an explicit test on the step, NOT the convergence test at the top of
//   the major. The original text here said the convergence test already
//   covered it; that was wrong, and wrong in a way that cost a whole
//   mechanism. The convergence test measures stationarity with the ITERATE'S
//   CURRENT MULTIPLIERS, which are only refreshed when a step is ACCEPTED --
//   so at a point where the subproblem's answer is p = 0, the very
//   multipliers that make x stationary are the ones the funnel is about to
//   discard, and this file's judge() rejects the zero step on rounding noise
//   forever. See that constant for the measurement (64 consecutive
//   rejections where one major suffices).
// * The radius update. Algorithm 3's "if trust region is active at d then
//   increase radius" is the globalization MECHANISM, not the strategy.
//   StepContext::tr_active carries the fact to the driver (Task 9) and this
//   file never reads it; a unit test pins that.
// * The restoration phase itself -- the feasibility subproblem (KLV's l1
//   feasibility problem is Eq. (4); the smooth reformulation actually solved
//   is the separately-tagged FQP(x_k, Delta_TR), which is NOT equation (4) but
//   a distinct subproblem with elastic variables u, v) and the
//   Restoration-branch Armijo test on h. TASK 9 BUILT BOTH, in sqp_driver.h's
//   RESTORATION PHASE: the feasibility problem is a RestorationModel wrapper
//   solved by the same driver, so the Armijo test on h is that sub-solve's own
//   funnel judging its own objective, and no second acceptance test exists.
//   kRestore is still only the REQUEST; nothing in this file services it.
//   THE ONE PIECE OF THE SWITCH THAT DOES LIVE HERE is the re-basing of the
//   width on the way BACK to optimality -- resume_from_restoration() below,
//   Algorithm 2's tau_+ = (1-kappa)h + kappa*tau -- because it is a statement
//   about this class's own state and nothing else.
//
// =============================================================================
// DIVERGENCES FROM [TYC], EACH RESOLVED TOWARD [KLV]
// =============================================================================
//
// (1) WIDTH UPDATE RULE. [TYC] implements Uno's default update_strategy = 1,
//     tau_+ = max(beta*tau, kappa*h_cur + (1-kappa)*h_trial) -- a convex
//     combination of the CURRENT and TRIAL infeasibilities, floored at
//     beta*tau. That is KLV Eq. (14), the rule from the ORIGINAL funnel paper
//     (Gould & Toint [19]), which KLV explicitly declines: "We tried out
//     different update strategies inspired by [19], but all had similar
//     performance. Therefore, we opted for (13), which is the second term in
//     the max." (Sec. 2.4.2, "A note on the funnel reduction mechanism".)
//     RESOLVED: Eq. (13), tau_+ = (1-kappa)*h_trial + kappa*tau -- a convex
//     combination of the trial and the OLD WIDTH. [TYC] itself flags Eq. (13)
//     as the paper's rule and its own as Uno's default (its "Divergences"
//     note). Adopting (13) also removes [TYC]'s disclosed re-widening edge:
//     Eq. (14) reads h_cur, so an out-of-funnel current iterate can push the
//     width UP; Eq. (13) never reads h_cur and cannot.
//
// (2) SWITCHING CONDITION. [TYC] inherits Wachter-Biegler Eq. (19),
//     alpha*(-grad_phi^T d)^{s_phi} > delta*theta^{s_theta} with s_phi = 2.3,
//     s_theta = 1.1, delta = 1.0 (Ipopt's defaults) -- a LINE-SEARCH condition
//     on a BARRIER objective, gated additionally on theta <= theta_min.
//     RESOLVED: KLV Eq. (10), delta_m_f(d) >= delta*(h)^2. No step length, no
//     barrier objective, no exponents, no theta_min gate. This is the single
//     largest semantic difference between the two ports; KLV's convergence
//     proof (Thm. 1 case 2, which sums f_k - f_{k+1} >= sigma*delta*(h_k)^2)
//     depends on exactly this form.
//
// (3) ARMIJO TEST. [TYC] uses WB Eq. (20), phi(trial) <= phi(cur) - eta_phi*m_f
//     with eta_phi = 1e-8, on the barrier objective phi = f + auxiliary.
//     RESOLVED: KLV Eq. (11), delta_f >= sigma*delta_m_f, on f itself, with
//     sigma = 1e-4 (KLV Table 1). Structurally the same inequality; the
//     objective and the constant both change. 1e-8 is Ipopt's
//     deliberately-slack barrier value and would make Eq. (11) very nearly
//     vacuous in Thm. 1 case 2's summation.
//
// (4) THE theta_min / theta_max MACHINERY. [TYC] runs on
//     switching_acceptance.h, which imposes a hard ceiling theta_max =
//     1e4*max(1, theta_0) (WB Eq. 21) and only lets the switching condition
//     fire below theta_min = 1e-4*max(1, theta_0).
//     RESOLVED: NOT PORTED. KLV has no such thresholds -- the funnel condition
//     (8) IS the ceiling, and the switching condition (10) is tested
//     unconditionally. Carrying them over would add two constants with no
//     source in the paper and would suppress f-type steps that KLV's proof
//     relies on.
//
// (5) CONSTANT VALUES. [TYC] takes tau_bar = 1.0, kappa_bar = 1.5,
//     beta = 0.9999, kappa = 0.5 from Uno's shipped option defaults
//     ("funnel_ubd", "funnel_fact", "funnel_beta", "funnel_kappa").
//     RESOLVED: KLV Table 1's published values -- tau_bar = 100,
//     kappa_bar = 1.25, delta = 0.999, sigma = 1e-4, beta = 0.99, kappa = 0.5.
//     Only kappa agrees, and KLV singles it out: "More important is the choice
//     of the parameter kappa ... we picked kappa = 0.5, which worked well".
//     beta = 0.99 vs 0.9999 matters: with kappa = 0.5 the guaranteed
//     contraction factor theta = 1-(1-beta)(1-kappa) is 0.995 rather than
//     0.99995, i.e. the funnel actually closes.
//
// (6) MEMBERSHIP vs H-TYPE ORDER. [TYC]'s base runs membership (8) as step 2
//     of a five-step template method that also owns a ceiling, a speculative
//     re-evaluation of the "T1" test for Ipopt-compatible rejection
//     attribution, and a notify_trial_rejected hook.
//     RESOLVED: none of that scaffolding is ported. KLV Algorithm 2 is three
//     nested ifs and this file is those three nested ifs. The ONE thing kept
//     from [TYC] is the observation that (8) gates EVERY trial including
//     f-type -- which is also literally what Algorithm 2 says (the
//     "acceptable to funnel" test encloses both branches), so it is not
//     really a [TYC] inheritance at all.
//
// NOT a divergence, but a documented GENERALIZATION: h itself. KLV Sec. 2.4.1
// defines h(x) = ||c(x)||_1 for an NCO whose only inequalities are bounds,
// which "are always feasible throughout SQP iterations". This engine's NLP
// carries general inequalities cI(x) <= 0, so the l1 violation generalizes to
// ||cE||_1 + sum_j max(0, cI_j) and reduces to the paper's h exactly when
// mi() == 0. Bounds are excluded for the paper's own reason -- and here that
// assumption is a THEOREM rather than an assumption, because the subproblem's
// box is l - x .. u - x, so every iterate the driver produces satisfies the
// bounds by construction (sqp_driver.h). The measure lives with the model
// evaluation it is computed from: constraint_violation_l1 in sqp_driver.h.

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include <fmt/format.h>

#include <hven/qp/qp_types.h>

namespace hven::solvers {

// =============================================================================
// KLV parameters. Every value is Table 1 ("Parameter values of the funnel
// method", KLV Sec. 5.1); every admissible range is the one stated with the
// equation that uses the constant. See DIVERGENCE 5 above for why these are
// the paper's values and not Uno's/[TYC]'s.
// =============================================================================

// tau_bar > 0: the absolute floor on the initial width (Eq. (9); Table 1 = 100).
inline constexpr double kFunnelTauBar = 100.0;
// kappa_bar > 1: the multiplier on h_0 (Eq. (9); Table 1 = 1.25). Strictly
// greater than 1 so the start point is strictly inside its own funnel.
inline constexpr double kFunnelKappaBar = 1.25;
// delta in (0,1): the switching-condition coefficient (Eq. (10); Table 1 = 0.999).
inline constexpr double kFunnelDelta = 0.999;
// sigma in (0,1): the Armijo fraction of the predicted decrease (Eq. (11);
// Table 1 = 1e-4).
inline constexpr double kFunnelSigma = 1.0e-4;
// beta and kappa live one level down, in detail::, unlike their five KLV
// siblings above: the IPM engine's funnel (detail/globalization/
// funnel_acceptance.h) already defines hven::solvers::kFunnelBeta and
// hven::solvers::kFunnelKappa as inline constexprs WITH A DIFFERENT BETA
// (0.9999, Uno's value, vs 0.99, KLV Table 1), so leaving these two at
// namespace scope would be an ODR violation the linker resolves silently.
// detail:: nesting is the sanctioned collision mechanism for the two
// engines' internal names (M3 plan §2 -- never new top-level namespaces);
// the five non-colliding siblings stay put so this deviation from the
// verbatim import is exactly as large as the collision that forces it.
namespace detail {
// beta in (0,1): the funnel sufficient-decrease margin (Eq. (12); Table 1 = 0.99).
inline constexpr double kFunnelBeta = 0.99;
// kappa in (0,1): the convex-combination coefficient in the width update
// (Eq. (13); Table 1 = 0.5).
inline constexpr double kFunnelKappa = 0.5;
} // namespace detail
// epsilon: the feasibility tolerance separating a feasible point from an
// INFEASIBLE STATIONARY one. KLV Sec. 5.1 states the two termination outcomes
// with the same epsilon = 1e-6 -- a KKT point needs ||c(x*)|| <= eps, an
// infeasible stationary point needs ||c(x*)|| > eps. Used only by the
// restoration signature below.
inline constexpr double kFunnelFeasEps = 1.0e-6;

// Minimum number of consecutive rejections at the SAME iterate before the
// restoration signature may fire. NOT A PAPER CONSTANT -- there is no value
// for it in KLV, and it is labelled as an implementation choice for exactly
// that reason. It exists to supply the hypothesis Lemma 5 case 1 actually
// carries: the QP goes infeasible only "for Delta_TR sufficiently small", and
// a radius is only demonstrably small AFTER the driver has shrunk it several
// times without the trial escaping the funnel. Without this gate the signature
// fires on trial #1 of a perfectly healthy solve (see conjunct (e) below).
// The paper's own analogue is equally implementation-chosen: Algorithm 4's
// alpha_min is a step-length floor with no value given in the text.
//
// RE-DERIVED IN TASK 9, AND THE VALUE MOVED (3 -> 4). The TODO this replaces
// asked for a derivation against sqp_driver.h's shrink factor rather than two
// independently chosen numbers. Here it is. Both arguments below are about the
// SAME quantity -- how much evidence "for Delta_TR sufficiently small" needs --
// and they land on the same count from opposite directions, which is why 4 is
// stated as derived rather than merely picked.
//
// (I) THE SHRINK-DEPTH ARGUMENT (the coupling the TODO named). With
//     kTrShrinkFactor = 1/2, r consecutive rejections at one iterate certify
//         Delta_r = 2^-r * Delta_0,
//     Delta_0 being the radius when the iterate was reached. Lemma 5 case 1's
//     threshold is Delta* = |c_i| / ||grad c_i||_1 -- a quantity the driver
//     cannot evaluate without the very subproblem it is trying to avoid
//     solving, so the gate cannot be a comparison against it. What it CAN be
//     is a statement about RESOLUTION: the smallest reduction that
//     distinguishes "Delta is below an unknown threshold" from "Delta is near
//     it" is one ORDER OF MAGNITUDE, and 2^-r <= 1/10 needs
//     r >= log2(10) = 3.32, i.e. r = 4. At r = 3 the radius has fallen by 8x,
//     which is BELOW the resolution the claim is made at; at r = 4 it has
//     fallen by 16x. This is what pins the pair: halve kTrShrinkFactor (to
//     1/4) and the same order-of-magnitude requirement gives r = 2.
//
// (II) THE EVIDENCE-MIX ARGUMENT (Task 6b's carry, which (I) alone does not
//     answer). rejections_at_iterate also counts ROUTED QP FAILURES -- trials
//     no strategy ever judged (sqp_driver.h's SUBPROBLEM FAILURE ROUTING). The
//     driver's retry is ONE-SHOT PER CHAIN (qp_failures_in_a_row), so a second
//     CONSECUTIVE routed failure ends the solve: in any surviving run of r
//     rejections at one iterate, routed rows are NON-ADJACENT and therefore
//     number at most ceil(r/2). At ODD r that is a strict MAJORITY -- at r = 3
//     it is the measured 2 of 3, pinned by
//     SqpDriverQpFailure.RoutedFailuresCountTowardTheRestorationGate -- while
//     at EVEN r it is exactly half and never a majority. So (II) selects the
//     EVEN counts and says nothing about their size.
//
// HOW THE TWO COMBINE, stated exactly (fix round 1 corrected an earlier
// sentence here that credited (II) alone with minimality, which is wrong:
// r = 2 is even too). (I) admits r >= 4; (II) admits r in {2, 4, 6, ...}. The
// INTERSECTION's smallest element is 4, and that is the whole selection
// argument -- neither constraint picks it alone.
//
// Routed rows still COUNT -- the counter means "the radius was shrunk here and
// nothing escaped the funnel", which a routed row satisfies exactly (see
// StepContext::rejections_at_iterate) -- and (II) is not an argument for
// excluding them. It is an argument for requiring enough of them that the
// judged ones cannot be outnumbered.
//
// STILL NOT A PAPER CONSTANT. KLV gives no value; its own analogue (Algorithm
// 4's alpha_min) is likewise unstated, as is the trust-region floor that
// replaces it here (sqp_driver.h's SqpOptions::tr_min). What has changed is
// that this one is now a consequence of kTrShrinkFactor rather than a second
// free choice: THE TWO REMAIN COUPLED AND MUST BE CHANGED TOGETHER, and
// argument (I) is the formula for doing so.
//
// COUPLED TEST: SqpDriverTrustRegion.RejectionCountReachesTheRestorationGate
// is written against this constant and its fixture's radius schedule was
// re-derived when the value moved (tr_init 4 -> 8, so that the trial at which
// the gate opens is still one at which the funnel condition fails).
inline constexpr Index kRestoreMinRejections = 4;

// =============================================================================
// StepContext — everything the acceptance test reads about one trial step.
// =============================================================================
//
// h is the KLV infeasibility measure (see the GENERALIZATION note above);
// sqp_driver.h's constraint_violation_l1 computes it.
//
// pred_df is delta_m_f(d) of KLV Eq. (6b): the predicted objective DECREASE
// from the QP model, POSITIVE for a model that promises progress.
//
// WHAT pred_df <= 0 DOES AND DOES NOT MEAN HERE. d = 0 is feasible for our
// subproblem only when the linearized constraints are already satisfied at
// p = 0 -- i.e. only at h = 0, since build_subproblem sets Je p = -cE (see
// sqp_driver.h). At an INFEASIBLE iterate the QP is forced away from the
// origin to satisfy that linearization, and the objective it must pay to get
// there routinely makes delta_m_f <= 0. So pred_df <= 0 is NOT evidence of
// model stationarity at an infeasible point -- it is the normal state of
// affairs there, and it is only at h = 0 that pred_df >= 0 is guaranteed.
// This is why the restoration signature below cannot rest on pred_df alone:
// it is a WEAK signal in exactly the regime conjunct (b) selects for.
struct StepContext {
    double f_old = 0.0;   // f(x_k)
    double f_new = 0.0;   // f(x_k + d)
    double h_old = 0.0;   // h(x_k)
    double h_new = 0.0;   // h(x_k + d)
    double pred_df = 0.0; // delta_m_f(d), Eq. (6b) -- positive for descent
    // Whether the trust region is active at d. Carried for the DRIVER's radius
    // update (KLV Algorithm 3); the acceptance test never reads it, by design.
    bool tr_active = false;
    // How many trials the driver has burned SHRINKING THE RADIUS at the
    // CURRENT iterate. Reset to 0 by the driver whenever the iterate moves.
    // Read only by the restoration signature; see conjunct (e).
    //
    // IT IS NOT ONLY THIS STRATEGY'S OWN kReject VERDICTS. The driver may also
    // count ROUTED QP FAILURES -- a subproblem that returned
    // kNumericalError/kMaxIter with a usable iterate, which sqp_driver.h's
    // SUBPROBLEM FAILURE ROUTING shrinks and re-solves without ever asking this
    // strategy to judge anything. Those trials satisfy the description above
    // exactly (the radius was shrunk here, nothing escaped the funnel) but no
    // judge() call produced them. CONSEQUENCE: a kRestore verdict may rest in
    // part on QP-FAILURE evidence -- measured at 2 of 3 on HS1 (tr_init = 12,
    // opts.qp.max_iter = 2). TASK 9 ACCOUNTED FOR THIS when it re-derived
    // kRestoreMinRejections (argument (II) at that constant): routed rows are
    // non-adjacent, so at the re-derived count of 4 they can be at most HALF
    // the evidence, never a majority. They still count, for the reason above.
    //
    // Defaults to 0, which the kRestore gate treats as "no evidence" -- a driver
    // that forgets to supply this field gets NO kRestore verdicts ever
    // (fail-safe toward kReject), never spurious restorations.
    Index rejections_at_iterate = 0;
};

// =============================================================================
// StepVerdict — the four outcomes of KLV Algorithm 2's optimality branch.
// =============================================================================
enum class StepVerdict {
    // f-type: Eq. (10) and Eq. (11) both hold. Width unchanged.
    //
    // DRIVER OBLIGATION: kAcceptF is an ACCEPTANCE verdict, not a convergence
    // verdict. Algorithm 2 opens with "if ||d|| = 0 then acceptable <- true //
    // KKT point found", i.e. the KKT test runs AHEAD of the acceptance test and
    // short-circuits it. The driver must keep testing KKT residuals at the top
    // of each major (sqp_driver.h's CONVERGENCE TEST) and must not infer
    // convergence from a run of kAcceptF verdicts -- Eq. (11) with sigma = 1e-4
    // is satisfied by arbitrarily small objective decreases.
    kAcceptF,
    kAcceptH, // h-type: Eq. (10) fails, Eq. (12) holds. Width shrinks, Eq. (13).
    kReject,  // "the step is rejected, and either the trust-region radius or
              // the step size is reduced" (KLV Sec. 2.4.2).
    // Switch to the feasibility restoration phase (see FunnelStrategy).
    //
    // DRIVER OBLIGATION: this verdict is ADDITIVE, NOT A REPLACEMENT. KLV's
    // authoritative restoration triggers are the DRIVER's to implement --
    // Algorithm 5's "if subproblem infeasible" and Algorithm 4's
    // "if alpha < alpha_min" (whose trust-region analogue is a radius floor).
    // The driver MUST NOT skip them on the grounds that the strategy also
    // reports kRestore: this signature is a heuristic early signal that can
    // miss, whereas an infeasible subproblem is a fact that leaves the driver
    // with no step at all.
    //
    // STATUS OF THE TWO, AS OF TASK 9: BOTH ARE IMPLEMENTED, and this verdict
    // is now one of three sources rather than the only one.
    //   Algorithm 5's ("subproblem infeasible") lives in sqp_driver.h's
    //     ELASTIC TIER: a kInfeasible subproblem is reformulated with
    //     penalized slacks and re-solved, and only an EXHAUSTED relaxation
    //     raises the request. It raises it INDEPENDENTLY of this verdict --
    //     the driver never consults the strategy on that path, because there
    //     is no trial point to judge -- which is exactly the additivity this
    //     note demands.
    //   Algorithm 4's alpha_min analogue is sqp_driver.h's RADIUS FLOOR
    //     (SqpOptions::tr_min): a shrink that would take Delta below the floor
    //     raises the request instead, again without consulting this strategy.
    // All three enter the SAME restoration phase (sqp_driver.h's RESTORATION
    // PHASE note). This one remains the heuristic that can fire EARLIEST and
    // can also miss; the other two are facts.
    kRestore
};

// =============================================================================
// GlobalizationStrategy — the interface the driver holds.
// =============================================================================
class GlobalizationStrategy {
  public:
    virtual ~GlobalizationStrategy() = default;

    // Renders the verdict for one trial step and applies whatever state update
    // that verdict implies. NOT idempotent (an accepted h-type step tightens
    // the funnel).
    //
    // MAY BE CALLED UP TO TWICE FOR THE SAME ITERATE (Task 7): when the
    // driver attempts a second-order correction (sqp_driver.h's SECOND-ORDER
    // CORRECTION note) on a kReject whose violation increased, it calls
    // judge() a second time on the CORRECTED point before deciding whether to
    // shrink the radius. THIS IS PART OF THE INTERFACE CONTRACT, not an
    // implementation detail of the shipped FunnelStrategy: make_strategy is
    // public API (sqp_types.h), and a caller-supplied, STATEFUL strategy must
    // tolerate a second judge() call on one iterate without corrupting its
    // own state. The shipped FunnelStrategy satisfies this because only an
    // ACCEPT verdict (kAcceptH specifically) mutates its width, and the FIRST
    // of the two calls is always a kReject (that is what triggers the second
    // call at all) -- so at most one mutating verdict is produced per
    // iterate, matching the single-mutation budget an implementation written
    // against the OLD "called once per trial" wording would have assumed.
    // A strategy that mutates state on ANY verdict (including kReject) would
    // need to account for this explicitly.
    virtual StepVerdict judge(const StepContext &ctx) = 0;

    // Begins a new solve from an iterate whose infeasibility is h0. Must be
    // called before the first judge(). h0 must be finite and >= 0.
    virtual void reset(double h0) = 0;

    // RE-ENTERS THE OPTIMALITY PHASE after a restoration phase has moved the
    // iterate to a point whose infeasibility is h_restored (KLV Algorithm 2's
    // switch back; sqp_driver.h's RESTORATION PHASE note is the driver side).
    // Called EXACTLY ONCE per restoration the driver resumes from, always
    // between two judge() calls, never before the first reset().
    //
    // WHY THIS IS NOT reset(h_restored), which is what an earlier draft of the
    // Task-9 brief specified. reset() is Eq. (9), tau_0 = max(tau_bar,
    // kappa_bar*h_0), and tau_bar = 100 is an ABSOLUTE FLOOR: re-initializing
    // a funnel that had tightened to, say, tau = 3 would push it back out to
    // 100 and un-do every h-type acceptance the solve had earned. The funnel's
    // monotonically non-increasing width is exactly what KLV Thm. 1 case 1
    // sums, so re-widening is not a cosmetic difference -- it forfeits the
    // convergence argument. Algorithm 2 instead RE-BASES on exit from
    // restoration, by the ordinary Eq. (13) update at the restored point, and
    // that is what FunnelStrategy does below.
    //
    // THE DEFAULT IS reset(h_restored) because it is the only thing that can
    // be written generically: a strategy whose state this header cannot see
    // has no re-basing rule to apply. A stateful caller-supplied strategy that
    // cares about the difference must override this; one that does not
    // override it gets the honest fallback and is no worse off than a
    // strategy told nothing at all.
    //
    // A DECORATOR MUST FORWARD THIS EXPLICITLY, and the trap is worth naming
    // because the shipped tests fell into it: a strategy that WRAPS another
    // (a recorder, a logger) and forwards only reset()/judge() will, on the
    // default path, call its OWN reset() and so re-initialize the wrapped
    // strategy -- turning a re-basing into exactly the Eq. (9) re-widening
    // this hook exists to avoid, and silently reporting a reset the driver
    // never asked for. Forward it like any other virtual.
    virtual void resume_from_restoration(double h_restored) { reset(h_restored); }
};

// =============================================================================
// FunnelStrategy — KLV Algorithm 2 (optimality phase), transcribed.
// =============================================================================
//
// State is one scalar: the funnel width tau. reset() sets it by Eq. (9);
// judge() reads it in Eq. (8)/(12) and tightens it by Eq. (13).
//
// THE kRestore TRIGGER, and exactly how far it is a transcription.
//
// KLV's two AUTHORITATIVE restoration entries are both outside this class's
// view: Algorithm 5 switches phase when "subproblem infeasible", and the
// line-search variant (Algorithm 4) switches when "alpha < alpha_min (step-size
// too small)". Neither a QP status nor a step length is in StepContext, and
// both stay the DRIVER's to implement -- this class cannot and does not usurp
// them. THE SIGNATURE BELOW IS ADDITIVE, NOT A REPLACEMENT: it is a heuristic
// early signal and can miss, so the driver must still implement both
// authoritative triggers and must not treat this verdict as standing in for
// either. (Algorithm 5's now lives in sqp_driver.h's ELASTIC TIER, and fires
// without ever calling judge(); Algorithm 4's is still outstanding. See
// StepVerdict::kRestore for the current state of both.)
//
// What it CAN see is the configuration those triggers are the endgame of.
// Lemma 5, case 1 is the paper's own account of it: at an iterate with
// h_k > 0, shrinking the trust region eventually makes the linearized
// constraints inconsistent -- "If either ||grad c_i|| = 0 or Delta_TR <
// |c_i|/||grad c_i||_1 ... Thus for Delta_TR SUFFICIENTLY SMALL,
// QP(x_k, Delta_TR) is infeasible, and the inner loop terminates finitely",
// which is precisely Algorithm 5's trigger.
//
// THE HYPOTHESIS IS "SUFFICIENTLY SMALL", AND IT IS LOAD-BEARING. Lemma 5
// says nothing whatever about a FIRST trial at a full radius; it describes the
// end of a shrink sequence. Conjunct (e) is what supplies that hypothesis --
// without it the signature fires on a healthy solve at trial #1. Worked
// counterexample, which is a regression test
// (FunnelRestore.HealthyOvershootIsRejectedNotRestored): min x s.t. x^2 - 1 = 0
// from x0 = 0.01. Then h_old = 0.9999, tau = 100, and the QP's linearization
// 0.02*p = 0.9999 forces p = 49.995, giving pred_df = -49.995 and
// h_new = 2499.5. Conjuncts (a)-(d) ALL hold, yet nothing is wrong: the paper's
// response is kReject and a radius shrink, and its own Lemma 3 guarantees that
// shrink succeeds (a step is funnel-acceptable once Delta^2 <= 2*beta*tau/(mnM),
// here Delta <= about 9.9). Declaring restoration there would abandon a problem
// that solves in a handful of majors.
//
// All FIVE conjuncts must hold:
//
//   (a) h_new > tau            -- the trial is funnel-incompatible: Eq. (8),
//                                 the necessary condition, fails.
//   (b) h_old > eps            -- the CURRENT iterate is infeasible. This is
//                                 Lemma 5 case 1's precondition h_k > 0, at
//                                 the tolerance KLV Sec. 5.1 uses to declare
//                                 an "Infeasible stationary point found"
//                                 (||c(x*)|| > eps).
//   (c) pred_df <= 0           -- the QP model offers no objective decrease.
//                                 A WEAK signal, deliberately kept as a
//                                 necessary-not-sufficient filter: at an
//                                 infeasible iterate (which (b) selects for)
//                                 delta_m_f <= 0 is NORMAL, because our
//                                 subproblem's Je p = -cE pushes the step away
//                                 from p = 0 and the objective pays for it
//                                 (see StepContext::pred_df). It excludes the
//                                 clear-cut case of a model still promising
//                                 progress; it does not on its own indicate
//                                 stationarity, and must not be read as doing
//                                 so.
//   (d) h_new >= h_old         -- and the step does not reduce infeasibility
//                                 either, so the h-type branch has nothing to
//                                 work with in this direction.
//   (e) rejections_at_iterate  -- the radius has ALREADY been shrunk at least
//         >= kRestoreMinRejections  this many times at this iterate without the
//                                 trial escaping the funnel. This is Lemma 5's
//                                 "for Delta_TR sufficiently small" hypothesis,
//                                 discharged with evidence rather than assumed.
//                                 It is the conjunct that separates a healthy
//                                 overshoot (rejected, radius shrunk, solved)
//                                 from a genuine dead end.
//                                 THE EVIDENCE IS NOT ALL THIS STRATEGY'S OWN:
//                                 the driver may count ROUTED QP FAILURES
//                                 (kNumericalError/kMaxIter shrink-retries --
//                                 see sqp_driver.h's SUBPROBLEM FAILURE
//                                 ROUTING) toward it. Task 9's re-derivation of
//                                 kRestoreMinRejections is what bounds their
//                                 share (argument (II) there: non-adjacent, so
//                                 at most half). See StepContext::
//                                 rejections_at_iterate for the measurement.
//
// (d) is implied by (a) whenever the current iterate is itself inside the
// funnel (h_old <= tau < h_new), which every strategy-accepted iterate is. It
// is tested anyway, because the driver may hand back an iterate this strategy
// never accepted (a restoration exit, a caller-supplied start point), and from
// OUTSIDE the funnel a step that genuinely reduces h must be a plain kReject
// -- shrink the radius and try again -- not a restoration request.
//
// Anything funnel-incompatible that misses the signature is kReject, i.e. the
// paper's default: reduce the radius and retry.
//
// =============================================================================
// THE FULL-STEP MODE (Phase-4 Task 5) -- a SECOND, OPT-IN STATE OF THIS CLASS
// =============================================================================
//
// [KD] V. Kungurtsev, M. Diehl, "Sequential quadratic programming methods for
// parametric nonlinear optimization", Comput. Optim. Appl. 59:475-509 (2014)
// -- the warm-start reference of docs/notes/2026-07-27-literature-survey.md's
// Axis 4, whose motivating observation is that STANDARD GLOBALIZATION ACTIVELY
// INTERFERES WITH WARM STARTS: at a point already inside the Newton domain of
// a NEARBY problem's solution, the undamped SQP step is the one that converges
// superlinearly, and an acceptance test calibrated for a cold start spends its
// majors refusing it.
//
// begin_full_step() puts this strategy into a mode where judge() returns
// kAcceptF for every FINITE trial WITHOUT applying any of Algorithm 2's tests
// -- KD's "full step first". The mode is:
//
//   * OPT-IN AND DRIVER-DRIVEN. Nothing here turns it on; sqp_driver.h engages
//     it only on a solve whose warm-start level resolved to kWarm or above and
//     only when SqpOptions::warm_full_step is set (both, so a cold solve and a
//     disabled flag are bit-identical to this class's pre-Task-5 behaviour).
//   * NON-CERTIFYING, WHICH IS THE SAFETY INVARIANT. A verdict is an
//     ACCEPTANCE, never a convergence claim (see StepVerdict::kAcceptF's own
//     DRIVER OBLIGATION note) -- the driver's KKT test at the top of every
//     major is untouched by this mode, so a full-step solve can only ever exit
//     kOptimal through the SAME evaluate_kkt every globalized solve exits
//     through.
//   * NOT A LICENCE TO STEP ONTO A NaN. A non-finite trial still returns
//     kReject, exactly as judge()'s own guard does below: the mode bypasses
//     the acceptance TESTS, not the measurement discipline, and without this
//     the driver would walk onto an unmeasurable point that its own
//     non-finite-iterate exit would then have to report as kNumericalError.
//   * WIDTH-PRESERVING. No verdict this mode returns mutates the width, so
//     tau is exactly what it was when the mode was entered -- which is what
//     lets the exit re-base rather than re-initialize (below).
//
// THE EXIT IS A WATCHDOG, and it is resume_from_restoration(): the driver
// tracks the best iterate seen under the mode by ||KKT||inf, and on either
// exit signal (sqp_types.h's kWarmResidualGrowthMax / kWarmFullStepWindow) it
// RESTORES that iterate and hands the solve back to ordinary funnel
// globalization AT THAT POINT. Re-entering the optimality phase at a point a
// non-funnel mechanism reached is precisely what resume_from_restoration
// already means, and Eq. (13)'s re-basing is exactly the right arithmetic for
// it, so the two share one function rather than growing a second one that
// would have to say the same thing. That is also why resume_from_restoration
// CLEARS the mode: a restoration phase that runs while the mode is on is
// itself evidence the mode's premise (we are in a nearby problem's Newton
// domain) is false, and the driver must not come back out of restoration
// still taking unit steps.
class FunnelStrategy final : public GlobalizationStrategy {
  public:
    // Eq. (9): tau_0 = max(tau_bar, kappa_bar * h_0).
    //
    // Throws std::invalid_argument on a negative or non-finite h0. A funnel
    // seeded from garbage would silently mis-scale every subsequent verdict,
    // and +inf specifically would make Eq. (8) accept everything forever.
    void reset(double h0) override {
        if (!(h0 >= 0.0) || !std::isfinite(h0)) { // the !(>=) form also catches NaN
            throw std::invalid_argument(
                fmt::format("FunnelStrategy::reset: h0 ({}) must be finite and >= 0 (it is the "
                            "constraint violation at the start point)",
                            h0));
        }
        width_ = std::max(kFunnelTauBar, kFunnelKappaBar * h0);
        initialized_ = true;
    }

    // KLV Algorithm 2, optimality branch. See the class note for the ordering
    // (it is the paper's if/else-if, not a rearrangement) and for kRestore.
    StepVerdict judge(const StepContext &ctx) override {
        if (!initialized_) {
            throw std::logic_error("FunnelStrategy::judge: called before reset(h0); the funnel "
                                   "width is undefined until a solve has been started");
        }

        // NON-FINITE TRIALS ARE NOT SILENTLY CLEAN -- sqp_driver.h's discipline,
        // and here it is a wrong-ANSWER guard rather than a robustness nicety:
        // every comparison against NaN is false, so an unguarded NaN h_new
        // would pass Eq. (8) (`h_new > tau` false), fail Eq. (10) (`>=` false),
        // and then pass Eq. (12) (`<=` false -> ... ) only by accident of which
        // way each test is written. Rejecting up front is the only defensible
        // reading: nothing has been measured, so nothing can be accepted. The
        // driver then shrinks the radius, exactly as for any other rejection.
        if (!std::isfinite(ctx.f_old) || !std::isfinite(ctx.f_new) || !std::isfinite(ctx.h_old) ||
            !std::isfinite(ctx.h_new) || !std::isfinite(ctx.pred_df)) {
            return StepVerdict::kReject;
        }

        // THE FULL-STEP MODE (Task 5). Deliberately AFTER the non-finite guard
        // above and BEFORE every one of Algorithm 2's tests -- see this class's
        // THE FULL-STEP MODE note for both placements. The width is not
        // touched, so the mode is exactly "Algorithm 2 is not consulted", not
        // "Algorithm 2 is consulted with different constants".
        if (full_step_) {
            return StepVerdict::kAcceptF;
        }

        // Eq. (8) -- the funnel condition, necessary for ANY acceptance.
        if (!(ctx.h_new <= width_)) {
            // The restoration signature; see the class note for all five
            // conjuncts and for why (e) is not optional.
            const bool infeasible_stationary =
                ctx.h_old > kFunnelFeasEps &&                       // (b)
                ctx.pred_df <= 0.0 &&                               // (c) weak on its own
                ctx.h_new >= ctx.h_old &&                           // (d)
                ctx.rejections_at_iterate >= kRestoreMinRejections; // (e) Lemma 5's hypothesis
            return infeasible_stationary ? StepVerdict::kRestore : StepVerdict::kReject;
        }

        // Eq. (10) -- the switching condition selects the iteration type. The
        // argument is h_old (h^(k), the CURRENT iterate), not h_new: Eq. (10)
        // asks whether the model promises enough decrease to justify ignoring
        // the infeasibility WE ARE AT.
        //
        // h_old*h_old overflows to +inf for h_old > ~1.34e154. Benign, and
        // benign in the safe direction: pred_df is finite (guarded above), so
        // `pred_df >= inf` is false, the trial is classified h-type, and Eq.
        // (12) then judges it against the finite width -- which is the correct
        // treatment of an iterate that infeasible anyway.
        if (ctx.pred_df >= kFunnelDelta * ctx.h_old * ctx.h_old) {
            // f-TYPE. Eq. (11): the Armijo-type sufficient decrease on f.
            // Accepted or not, the width does not move (KLV Thm. 1, Eq. (17)).
            const double actual_df = ctx.f_old - ctx.f_new; // delta_f, Eq. (7a)
            return actual_df >= kFunnelSigma * ctx.pred_df ? StepVerdict::kAcceptF
                                                           : StepVerdict::kReject;
        }

        // h-TYPE. Eq. (12): the funnel sufficient decrease condition.
        if (ctx.h_new <= detail::kFunnelBeta * width_) {
            // Eq. (13). Written in the paper's own term order so the arithmetic
            // is the arithmetic the tests hand-derive.
            //
            // A TESTING BLIND SPOT, acknowledged rather than papered over: at
            // Table 1's kappa = 0.5 this expression is SYMMETRIC in its two
            // arguments, so (1-kappa)*width + kappa*h_new computes the same
            // number and no black-box test can distinguish the roles of h_new
            // and tau here. What pins the roles is (i) kappa's exact value,
            // asserted in FunnelConstants.MatchKlvTableOne, and (ii) the fact
            // that the two orderings coincide identically at 0.5, asserted in
            // FunnelHType.KappaRoleIsUnobservableAtOneHalf so that the blind
            // spot is recorded in the suite. Should kappa ever move off 0.5,
            // that test fails and the existing Eq. (13) arithmetic assertions
            // become role-sensitive automatically.
            width_ = (1.0 - detail::kFunnelKappa) * ctx.h_new + detail::kFunnelKappa * width_;
            return StepVerdict::kAcceptH;
        }

        return StepVerdict::kReject;
    }

    // KLV Algorithm 2's switch back from restoration: the width is RE-BASED
    // at the restored point by the ordinary Eq. (13) update
    //
    //     tau_+ = (1 - kappa) * h_restored + kappa * tau,
    //
    // i.e. exactly what an accepted h-type step does, which is what the
    // restored point IS from the funnel's point of view -- a point reached by
    // a mechanism that reduced h. It is emphatically NOT Eq. (9): see the base
    // class's note for why re-initializing here would forfeit monotonicity.
    //
    // MONOTONICITY HOLDS UNDER THE CALLER'S OWN EXIT CONDITION, and only
    // under it: tau_+ <= tau iff h_restored <= tau. sqp_driver.h resumes only
    // when h_restored <= feas_tol, and any funnel that has not itself been
    // tightened below the feasibility tolerance has tau >= feas_tol, so the
    // inequality holds with room to spare (KLV's own exit condition is the
    // weaker h <= beta*tau, which ours implies there). The update is
    // transcribed UNCONDITIONALLY rather than clamped to min(tau, .) because
    // Eq. (13) is not a clamped rule, and a clamp would hide a caller that
    // resumed from a point outside its own funnel instead of reporting the
    // width that caller's behaviour actually implies.
    //
    // FROM TASK 5 IT ALSO CLEARS THE FULL-STEP MODE, and is the ONLY way out
    // of it: both callers -- the restoration resume and the full-step
    // watchdog -- are the same event as far as this class is concerned ("the
    // optimality phase is being re-entered at a point some other mechanism
    // chose"), and neither may leave the mode armed. See the class note's THE
    // EXIT IS A WATCHDOG paragraph.
    //
    // Throws on a negative or non-finite h_restored (reset()'s reasoning,
    // verbatim), and on a call before the first reset(), when there is no
    // width to re-base.
    void resume_from_restoration(double h_restored) override {
        if (!initialized_) {
            throw std::logic_error("FunnelStrategy::resume_from_restoration: called before "
                                   "reset(h0); there is no funnel width to re-base");
        }
        if (!(h_restored >= 0.0) || !std::isfinite(h_restored)) { // the !(>=) form catches NaN
            throw std::invalid_argument(
                fmt::format("FunnelStrategy::resume_from_restoration: h_restored ({}) must be "
                            "finite and >= 0 (it is the constraint violation at the restored "
                            "point)",
                            h_restored));
        }
        width_ = (1.0 - detail::kFunnelKappa) * h_restored + detail::kFunnelKappa * width_;
        full_step_ = false;
    }

    // TASK 5. Enter the full-step mode (see the class note). Callable only
    // after reset(), since the mode is a state of a started solve and its
    // eventual exit re-bases a width that must already exist; idempotent
    // while armed. Nothing but resume_from_restoration() leaves the mode.
    void begin_full_step() {
        if (!initialized_) {
            throw std::logic_error("FunnelStrategy::begin_full_step: called before reset(h0); the "
                                   "full-step mode is a state of a started solve, and its exit "
                                   "re-bases a funnel width that does not exist yet");
        }
        full_step_ = true;
    }

    // The current funnel width tau (diagnostics and tests). Undefined -- and
    // reported as the +inf sentinel -- until the first reset().
    double width() const { return width_; }

    // False until reset() has succeeded. A judge() call before then throws.
    bool initialized() const { return initialized_; }

    // TASK 5. True while the full-step mode is armed (diagnostics and tests).
    bool in_full_step() const { return full_step_; }

  private:
    double width_ = std::numeric_limits<double>::infinity();
    bool initialized_ = false;
    // TASK 5's mode flag. False is the pre-Task-5 class, exactly.
    bool full_step_ = false;
};

} // namespace hven::solvers
